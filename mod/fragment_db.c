#include "nat64/mod/fragment_db.h"
#include "nat64/comm/constants.h"


/**
 * @file
 * RFC 815, adapted to the requirement of only correlating (never assembling) fragments.
 */


#define INFINITY 60000

struct hole_descriptor {
	u16 first;
	u16 last;

	/** The thing that connects this object in its hole descriptor list. */
	struct list_head hook;
};

struct reassembly_buffer_key {
	l3_protocol l3_proto;
	union {
		struct {
			struct in_addr src_addr;
			struct in_addr dst_addr;
			__be16 identification;
		} ipv4;
		struct {
			struct in6_addr src_addr;
			struct in6_addr dst_addr;
			__be32 identification;
		} ipv6;
	};
	__u8 l4_proto;
};

struct reassembly_buffer {
	/* The "hole descriptor list". */
	struct list_head holes;
	/* The "buffer". */
	struct packet *pkt;
	/* Jiffy at which the fragment timer will delete this buffer. */
	unsigned long dying_time;

	struct list_head hook;
};


#define HTABLE_NAME fragdb_table
#define KEY_TYPE struct reassembly_buffer_key
#define VALUE_TYPE struct reassembly_buffer
#include "hash_table.c"

static struct fragdb_table table;
static DEFINE_SPINLOCK(table_lock);

static struct fragmentation_config config;
static DEFINE_SPINLOCK(config_lock);

static struct timer_list expire_timer;
static LIST_HEAD(expire_list);


static unsigned long get_fragment_timeout(void)
{
	unsigned long result;

	spin_lock_bh(&config_lock);
	result = config.fragment_timeout;
	spin_unlock_bh(&config_lock);

	return result;
}

static bool equals_function(struct reassembly_buffer_key *key1, struct reassembly_buffer_key *key2)
{
	if (key1 == key2)
		return true;
	if (key1 == NULL || key2 == NULL)
		return false;

	if (key1->l3_proto != key2->l3_proto)
		return false;

	switch (key1->l3_proto) {
	case L3PROTO_IPV4:
		if (!ipv4_addr_equals(&key1->ipv4.src_addr, &key2->ipv4.src_addr))
			return false;
		if (!ipv4_addr_equals(&key1->ipv4.dst_addr, &key2->ipv4.dst_addr))
			return false;
		if (key1->ipv4.identification != key2->ipv4.identification)
			return false;
		break;
	case L3PROTO_IPV6:
		if (!ipv6_addr_equals(&key1->ipv6.src_addr, &key2->ipv6.src_addr))
			return false;
		if (!ipv6_addr_equals(&key1->ipv6.dst_addr, &key2->ipv6.dst_addr))
			return false;
		if (key1->ipv6.identification != key2->ipv6.identification)
			return false;
		break;
	}

	if (key1->l4_proto != key2->l4_proto)
		return false;

	return true;
}

static __u16 hash_function(struct reassembly_buffer_key *key)
{
	__u16 result = 0;

	switch (key->l3_proto) {
	case L3PROTO_IPV4:
		result = be16_to_cpu(key->ipv4.identification);
		break;
	case L3PROTO_IPV6:
		result = be32_to_cpu(key->ipv6.identification);
		break;
	}

	return result;
}

static bool is_fragmented(struct fragment *frag)
{
	struct iphdr *hdr4;
	struct frag_hdr *hdr_frag;
	__u16 fragment_offset = 0;
	bool mf = false;

	switch (frag->l3_hdr.proto) {
	case L3PROTO_IPV4:
		hdr4 = frag_get_ipv4_hdr(frag);
		fragment_offset = get_fragment_offset_ipv4(hdr4);
		mf = is_more_fragments_set_ipv4(hdr4);
		break;

	case L3PROTO_IPV6:
		hdr_frag = frag_get_fragment_hdr(frag);
		if (!hdr_frag)
			return false;
		fragment_offset = get_fragment_offset_ipv6(hdr_frag);
		mf = is_more_fragments_set_ipv6(hdr_frag);
		break;
	}

	return (fragment_offset != 0) || (mf);
}

static struct hole_descriptor *hole_alloc(u16 first, u16 last)
{
	struct hole_descriptor *hd = kmalloc(sizeof(*hd), GFP_ATOMIC);
	if (!hd)
		return NULL;

	hd->first = first;
	hd->last = last;
	/* hook can keep its trash. */

	return hd;
}

static struct reassembly_buffer *buffer_alloc(struct fragment *frag)
{
	struct reassembly_buffer *buffer;
	struct packet *pkt;

	buffer = kmalloc(sizeof(*buffer), GFP_ATOMIC);
	if (!buffer)
		return NULL;
	pkt = pkt_create(frag);
	if (!pkt) {
		kfree(buffer);
		return NULL;
	}

	INIT_LIST_HEAD(&buffer->holes);
	buffer->pkt = pkt;
	buffer->dying_time = jiffies + get_fragment_timeout();

	return buffer;
}

static void frag_to_key(struct fragment *frag, struct reassembly_buffer_key *key)
{
	struct iphdr *hdr4;
	struct ipv6hdr *hdr6;
	struct hdr_iterator iterator;

	switch (frag->l3_hdr.proto) {
	case L3PROTO_IPV4:
		hdr4 = frag_get_ipv4_hdr(frag);
		key->l3_proto = L3PROTO_IPV4;
		key->ipv4.src_addr.s_addr = hdr4->saddr;
		key->ipv4.dst_addr.s_addr = hdr4->daddr;
		key->ipv4.identification = hdr4->id;
		key->l4_proto = hdr4->protocol;
		break;

	case L3PROTO_IPV6:
		hdr6 = frag_get_ipv6_hdr(frag);
		key->l3_proto = L3PROTO_IPV6;
		key->ipv6.src_addr = hdr6->saddr;
		key->ipv6.dst_addr = hdr6->daddr;

		hdr_iterator_init(&iterator, hdr6);
		while (iterator.hdr_type != NEXTHDR_FRAGMENT) {
			if (hdr_iterator_next(&iterator) != HDR_ITERATOR_SUCCESS)
				log_debug("Error"); /* TODO */
		}
		key->ipv6.identification = ((struct frag_hdr *) iterator.data)->identification;

		hdr_iterator_last(&iterator);
		key->l4_proto = iterator.hdr_type;
		break;
	}

}

static struct reassembly_buffer *buffer_get(struct fragment *frag)
{
	struct reassembly_buffer_key key;
	frag_to_key(frag, &key);
	return fragdb_table_get(&table, &key);
}

static int buffer_put(struct reassembly_buffer *buffer)
{
	struct reassembly_buffer_key key;
	int error;

	frag_to_key(pkt_get_first_frag(buffer->pkt), &key);
	error = fragdb_table_put(&table, &key, buffer);
	if (error)
		return error;

	list_add(&buffer->hook, expire_list.prev);
	if (!timer_pending(&expire_timer) || time_before(buffer->dying_time, expire_timer.expires)) {
		mod_timer(&expire_timer, buffer->dying_time);
		log_debug("The buffer cleaning timer will awake in %u msecs.",
				jiffies_to_msecs(expire_timer.expires - jiffies));
	}

	return 0;
}

static void buffer_destroy(struct reassembly_buffer *buffer, bool free_pkt)
{
	struct reassembly_buffer_key key;

	/* Remove it from the DB. */
	frag_to_key(pkt_get_first_frag(buffer->pkt), &key);
	if (!fragdb_table_remove(&table, &key, false)) {
		log_crit(ERR_UNKNOWN_ERROR, "Something is attempting to delete a buffer that wasn't stored"
				"in the database.");
		return;
	}

	list_del(&buffer->hook);

	/* Deallocate it. */
	while (!list_empty(&buffer->holes)) {
		struct hole_descriptor *hole = list_entry(buffer->holes.next, struct hole_descriptor, hook);
		kfree(hole);
	}
	if (free_pkt)
		pkt_kfree(buffer->pkt, true);
	kfree(buffer);
}

static u16 compute_fragment_first(struct fragment *frag)
{
	u16 offset = 0; /* In bytes. */

	switch (frag->l3_hdr.proto) {
	case L3PROTO_IPV4:
		offset = get_fragment_offset_ipv4(frag_get_ipv4_hdr(frag));
		break;
	case L3PROTO_IPV6:
		offset = get_fragment_offset_ipv6(frag_get_fragment_hdr(frag));
		break;
	}

	return offset >> 3;
}

static bool is_mf_set(struct fragment *frag)
{
	bool mf = false;

	switch (frag->l3_hdr.proto) {
	case L3PROTO_IPV4:
		mf = is_more_fragments_set_ipv4(frag_get_ipv4_hdr(frag));
		break;
	case L3PROTO_IPV6:
		mf = is_more_fragments_set_ipv6(frag_get_fragment_hdr(frag));
		break;
	}

	return mf;
}

static void clean_expired_buffers(void)
{
	struct list_head *current_node, *next_node;
	unsigned int f = 0;
	struct reassembly_buffer *buffer;

	log_debug("Deleting expired reassembly buffers...");

	spin_lock_bh(&table_lock);

	list_for_each_safe(current_node, next_node, &expire_list) {
		buffer = list_entry(current_node, struct reassembly_buffer, hook);

		if (time_after(buffer->dying_time, jiffies)) {
			spin_unlock_bh(&table_lock);
			log_debug("Deleted %u reassembly buffers.", f);
			return;
		}

		buffer_destroy(buffer, true);
		f++;
	}

	spin_unlock_bh(&table_lock);
	log_debug("Deleted %u reassembly buffers. The database is now empty.", f);
}

static void cleaner_timer(unsigned long param)
{
	struct reassembly_buffer *buffer;
	unsigned long next_expire;

	clean_expired_buffers();

	spin_lock_bh(&table_lock);
	if (list_empty(&expire_list)) {
		spin_unlock_bh(&table_lock);
		/* No need to re-schedule the timer. */
		return;
	}

	/* Restart the timer. */
	buffer = list_entry(expire_list.next, struct reassembly_buffer, hook);
	next_expire = buffer->dying_time;
	spin_unlock_bh(&table_lock);
	mod_timer(&expire_timer, next_expire);
}

static verdict l4_post(struct packet *pkt) {
	verdict result = VER_CONTINUE;
	struct fragment *frag;
	struct iphdr *ip4_hdr;
	struct udphdr *hdr_udp;
	struct icmphdr *hdr_icmp4;
	struct icmp6hdr *hdr_icmp6;
	__sum16 tmp_csum;
	unsigned long len;

	switch (pkt_get_l4proto(pkt)) {
	case L4PROTO_TCP:
		break;
	case L4PROTO_UDP:
		if (pkt_get_l3proto(pkt) == L3PROTO_IPV6)
			break; /* The checksum is mandatory in IPv6, so it's already set. */

		hdr_udp = frag_get_udp_hdr(pkt->first_fragment);
		if (hdr_udp->check != 0)
			break; /* The client went through the trouble to compute the csum. */

		/* We'll have to compute the checksum ourselves. */
		tmp_csum = csum_partial(hdr_udp, sizeof(*hdr_udp), 0);
		len = sizeof(*hdr_udp);
		list_for_each_entry(frag, &pkt->fragments, next) {
			tmp_csum = csum_partial(frag->payload.ptr, frag->payload.len, tmp_csum);
			len += frag->payload.len;
		}

		ip4_hdr = frag_get_ipv4_hdr(pkt->first_fragment);
		hdr_udp->check = csum_tcpudp_magic(ip4_hdr->saddr, ip4_hdr->daddr, len, IPPROTO_UDP, tmp_csum);
		break;

	case L4PROTO_ICMP:
		switch (pkt_get_l3proto(pkt)) {
		case L3PROTO_IPV4:
			hdr_icmp4 = frag_get_icmp4_hdr(pkt->first_fragment);
			if (is_icmp4_info(hdr_icmp4->type)) {
				/*
				 * The ICMP payload is not another packet.
				 * Hence, it will not be translated (it will be copied as-is).
				 * Hence, we will not have to recompute the checksum from scratch
				 * (we'll just update the old checksum with the new header's data).
				 * Hence, we don't have to validate the incoming checksum.
				 * (Because that will be the IPv6 node's responsibility.)
				 */
				break;
			}

			/* BTW: we're not iterating over all the fragments because ICMP errors are never fragmented. */
			result = validate_csum_icmp4(pkt->first_fragment->skb,
					pkt->first_fragment->l4_hdr.len + pkt->first_fragment->payload.len);
			if (result != VER_CONTINUE)
				log_debug("The packet's checksum is incorrect. Dropping...");
			break;
		case L3PROTO_IPV6:
			hdr_icmp6 = frag_get_icmp6_hdr(pkt->first_fragment);
			if (is_icmp6_info(hdr_icmp6->icmp6_type))
				break; /* See the comment above. */

			result = validate_csum_icmp6(pkt->first_fragment->skb,
					pkt->first_fragment->l4_hdr.len + pkt->first_fragment->payload.len);
			if (result != VER_CONTINUE)
				log_debug("The packet's checksum is incorrect. Dropping...");
			break;
		}
		break;

	case L4PROTO_NONE:
		log_warning("The transport protocol of the first fragment is NONE.");
		result = VER_DROP;
		break;
	}

	return result;
}

int fragdb_init(void)
{
	config.fragment_timeout = msecs_to_jiffies(FRAGMENT_MIN);

	fragdb_table_init(&table, equals_function, hash_function);

	init_timer(&expire_timer);
	expire_timer.function = cleaner_timer;
	expire_timer.expires = 0;
	expire_timer.data = 0;

	return 0;
}

/**
 * RFC 815, section 3.
 */
verdict fragment_arrives(struct sk_buff *skb, struct packet **result)
{
	struct reassembly_buffer *buffer;
	struct hole_descriptor *hole;
	struct hole_descriptor *hole_aux;
	struct fragment *frag;
	u16 fragment_first = 0; /* In octets. */
	u16 fragment_last = 0; /* In octets. */

	switch (ntohs(skb->protocol)) {
	case ETH_P_IP:
		frag = frag_create_ipv4(skb);
		break;
	case ETH_P_IPV6:
		frag = frag_create_ipv6(skb);
		break;
	default:
		log_err(ERR_L3PROTO, "Unsupported network protocol: %u", ntohs(skb->protocol));
		return VER_DROP;
	}

	/*
	 * This short circuit is not part of the RFC.
	 * I added it because I really don't want to spinlock nor allocate if I don't have to.
	 */
	if (!is_fragmented(frag)) {
		/* No need to interact with the database. Encapsulate the packet and let it fly. */
		*result = pkt_create(frag);
		return l4_post(*result);
	}

	spin_lock_bh(&table_lock);

	buffer = buffer_get(frag);
	if (buffer) {
		pkt_add_frag(buffer->pkt, frag);

	} else {
		buffer = buffer_alloc(frag);
		if (!buffer)
			goto fail;

		hole = hole_alloc(0, INFINITY);
		if (!hole) {
			kfree(buffer);
			goto fail;
		}

		list_add(&hole->hook, &buffer->holes);

		if (buffer_put(buffer) != 0)
			goto fail;
	}

	fragment_first = compute_fragment_first(frag);
	fragment_last = fragment_first + ((frag->l4_hdr.len + frag->payload.len) >> 3);

	/* Step 1 */
	list_for_each_entry_safe(hole, hole_aux, &buffer->holes, hook) {
		/* Step 2 */
		if (fragment_first > hole->last)
			continue;

		/* Step 3 */
		if (fragment_last < hole->first)
			continue;

		/* Step 5 */
		if (fragment_first > hole->first) {
			struct hole_descriptor *new_hole;
			new_hole = hole_alloc(hole->first, fragment_first - 1);
			if (!new_hole)
				goto fail;
			list_add(&new_hole->hook, hole->hook.prev);
		}

		/* Step 6 */
		if (fragment_last < hole->last && is_mf_set(frag)) {
			struct hole_descriptor *new_hole;
			new_hole = hole_alloc(fragment_last + 1, hole->last);
			if (!new_hole)
				goto fail;
			list_add(&new_hole->hook, hole->hook.next);
		}

		/*
		 * Step 4
		 * (I had to move this because it seems to be the simplest way to append the new_holes to
		 * the list in steps 5 and 6.)
		 */
		list_del(&hole->hook);
	} /* Step 7 */

	/* Step 8 */
	if (list_empty(&buffer->holes)) {
		*result = buffer->pkt;
		buffer_destroy(buffer, false);
		spin_unlock_bh(&table_lock);
		return l4_post(*result);
	}

	spin_unlock_bh(&table_lock);
	return VER_STOLEN;

fail:
	spin_unlock_bh(&table_lock);
	return VER_DROP;
}

void fragdb_destroy(void)
{
	fragdb_table_empty(&table, true);
	del_timer_sync(&expire_timer);
}
