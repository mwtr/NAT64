#include <linux/module.h>
#include <linux/slab.h>

#include "nat64/mod/common/ipv6_hdr_iterator.h"
#include "nat64/unit/unit_test.h"
#include "nat64/unit/skb_generator.h"
#include "nat64/unit/validator.h"
#include "nat64/unit/types.h"

#define GENERATE_FOR_EACH true
#include "fragment_db.c"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roberto Aceves");
MODULE_AUTHOR("Alberto Leiva");
MODULE_DESCRIPTION("Fragment database test");


static struct frag_hdr *get_frag_hdr(struct sk_buff *skb)
{
	return hdr_iterator_find(ipv6_hdr(skb), NEXTHDR_FRAGMENT);
}

static __u16 get_offset(struct sk_buff *skb)
{
	struct frag_hdr *hdr = get_frag_hdr(skb);
	return hdr ? get_fragment_offset_ipv6(hdr) : 0;
}

static bool assert_fragdb_handle(struct sk_buff *skb, verdict expected)
{
	struct packet pkt;
	int error = -EINVAL;

	switch (ntohs(skb->protocol)) {
	case ETH_P_IPV6:
		error = pkt_init_ipv6(&pkt, skb);
		break;
	case ETH_P_IP:
		error = pkt_init_ipv4(&pkt, skb);
		break;
	}

	if (error) {
		log_debug("the pkt init function returned errcode %d.", error);
		return false;
	}

	return assert_equals_int(expected, fragdb_handle(&pkt), "verdict result");
}

static bool validate_packet(struct sk_buff *skb, int expected_frag_count)
{
	int actual_frag_count = 1;
	__u16 current_offset, last_offset;
	bool success = true;

	if (!skb) {
		success &= assert_equals_int(0, expected_frag_count, "There are no packets.");
		return success;
	}

	success &= assert_equals_u16(0, get_offset(skb), "1st frag has offset 0");
	last_offset = 0;

	skb = skb_shinfo(skb)->frag_list;
	while (skb) {
		actual_frag_count++;
		current_offset = get_offset(skb);
		success &= assert_true(last_offset < current_offset, "frags have increasing offset");

		last_offset = current_offset;
		skb = skb->next;
	}

	success &= assert_equals_int(expected_frag_count, actual_frag_count, "Fragment count");
	return success;
}

static bool validate_fragment(struct sk_buff *skb, bool has_frag_hdr, int l3_payload_len)
{
	bool success = true;

	success &= assert_equals_int(has_frag_hdr, !!get_frag_hdr(skb), "Presence of frag header");
	success &= assert_equals_int(l3_payload_len, skb_tail_pointer(skb) - skb_transport_header(skb),
			"L3 payload length");

	return success;
}

static int fragdb_counter(struct reassembly_buffer *buffer, void *arg)
{
	unsigned int *int_arg = arg;
	(*int_arg)++;
	return 0;
}

static bool validate_database(int expected_count)
{
	struct list_head *node;
	int p = 0;
	bool success = true;

	/* list */
	list_for_each(node, &expire_list) {
		p++;
	}
	success &= assert_equals_int(expected_count, p, "Packets in the list");

	/* table */
	p = 0;
	fragdb_table_for_each(&table, fragdb_counter, &p);
	success &= assert_equals_int(expected_count, p, "Packets in the hash table");

	return success;
}

/**
 * Asserts the packet doesn't stay in the database if it is not a fragment.
 * IPv6-to-IPv4 direction.
 */
static bool test_no_frags(void)
{
	struct sk_buff *skb;
	struct tuple tuple6;
	int error;
	bool success = true;

	/* Prepare */
	error = init_ipv6_tuple(&tuple6, "1::2", 1212, "3::4", 3434, L4PROTO_UDP);
	if (error)
		return false;
	error = create_skb6_udp(&tuple6, &skb, 10, 32);
	if (error)
		return false;

	success &= assert_fragdb_handle(skb, VERDICT_CONTINUE);
	success &= validate_packet(skb, 1);
	success &= validate_fragment(skb, false, sizeof(struct udphdr) + 10);
	success &= validate_database(0);

	kfree_skb(skb);
	return success;
}

static bool test_happy_path(void)
{
	struct sk_buff *skb, *first;
	struct tuple tuple6;
	int error;
	bool success = true;

	error = init_ipv6_tuple(&tuple6, "1::2", 1212, "3::4", 3434, L4PROTO_UDP);
	if (error)
		return false;

	/* First fragment arrives. */
	error = create_skb6_udp_frag(&tuple6, &skb, 64 - sizeof(struct udphdr), 384, true, true, 0, 32);
	if (error)
		return false;
	success &= assert_fragdb_handle(skb, VERDICT_STOLEN);
	success &= validate_database(1);

	first = skb;

	/* Second fragment arrives. */
	error = create_skb6_udp_frag(&tuple6, &skb, 128, 384, true, true, 64, 32);
	if (error)
		return false;
	success &= assert_fragdb_handle(skb, VERDICT_STOLEN);
	success &= validate_database(1);

	/* Third and final fragment arrives. */

	error = create_skb6_udp_frag(&tuple6, &skb, 192, 384, true, false, 192, 32);
	if (error)
		return false;
	success &= assert_fragdb_handle(skb, VERDICT_CONTINUE);
	success &= validate_database(0);

	/* Validate the packet. */
	success &= validate_packet(first, 3);
	if (!success)
		return false;

	/* Validate the fragments. */
	log_debug("Validating the first fragment...");
	success &= validate_fragment(first, true, 64);

	log_debug("Validating the second fragment...");
	success &= validate_fragment(skb_shinfo(first)->frag_list, true, 128);

	log_debug("Validating the third fragment...");
	success &= validate_fragment(skb_shinfo(first)->frag_list->next, true, 192);

	kfree_skb(first);
	return success;
}

struct frag_summary {
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
	enum l4_protocol l4_proto;
};

static bool validate_list(struct frag_summary *expected, int expected_count)
{
	struct reassembly_buffer *buffer;
	struct packet *pkt;
	struct ipv6hdr *hdr6;
	bool success = true;
	int c = 0;

	list_for_each_entry(buffer, &expire_list, list_hook) {
		if (!assert_true(c < expected_count, "List count"))
			return false;

		pkt = &buffer->pkt;

		success &= assert_equals_u8(L4PROTO_UDP, pkt_l4_proto(pkt), "proto");

		hdr6 = pkt_ip6_hdr(pkt);
		success &= assert_equals_ipv6(&expected[c].ipv6.src_addr, &hdr6->saddr, "src addr6");
		success &= assert_equals_ipv6(&expected[c].ipv6.dst_addr, &hdr6->daddr, "dst addr6");
		success &= assert_equals_be32(expected[c].ipv6.identification,
				get_frag_hdr(pkt->skb)->identification, "frag id 6");

		c++;
	}

	return success;
}

/**
 * Two things are being validated here:
 * - The timer deletes the correct stuff whenever it has to.
 * - multiple packets in the DB at once.
 * Both IPv4 and IPv6.
 */
static bool test_timer(void)
{
	struct sk_buff *skb;
	struct tuple tuple1, tuple2;
	struct frag_summary expected_keys[2];
	struct reassembly_buffer *dummy_buffer;
	bool success = true;
	int error;

	error = init_ipv6_tuple(&tuple1, "1::2", 1212, "3::4", 3434, L4PROTO_UDP);
	if (error)
		return false;
	error = init_ipv6_tuple(&tuple2, "8::7", 8787, "6::5", 6565, L4PROTO_UDP);
	if (error)
		return false;

	expected_keys[0].ipv6.src_addr = tuple1.src.addr6.l3;
	expected_keys[0].ipv6.dst_addr = tuple1.dst.addr6.l3;
	expected_keys[0].ipv6.identification = cpu_to_be32(4321);
	expected_keys[0].l4_proto = NEXTHDR_UDP;

	expected_keys[1].ipv6.src_addr = tuple2.src.addr6.l3;
	expected_keys[1].ipv6.dst_addr = tuple2.dst.addr6.l3;
	expected_keys[1].ipv6.identification = cpu_to_be32(4321);
	expected_keys[1].l4_proto = NEXTHDR_UDP;

	/* Fragment 1.1 arrives. */
	error = create_skb6_udp_frag(&tuple1, &skb, 100, 1000, true, true, 0, 32);
	if (error)
		return false;
	success &= assert_fragdb_handle(skb, VERDICT_STOLEN);

	success &= validate_database(1);
	success &= validate_list(&expected_keys[0], 1);
	clean_expired_buffers();
	success &= validate_database(1);
	success &= validate_list(&expected_keys[0], 1);

	/* Fragment 2.1 arrives. */
	error = create_skb6_udp_frag(&tuple2, &skb, 100, 1000, true, true, 0, 32);
	if (error)
		return false;
	success &= assert_fragdb_handle(skb, VERDICT_STOLEN);

	success &= validate_database(2);
	success &= validate_list(&expected_keys[0], 2);
	clean_expired_buffers();
	success &= validate_database(2);
	success &= validate_list(&expected_keys[0], 2);

	/* Fragment 1.2 arrives. */
	error = create_skb6_udp_frag(&tuple1, &skb, 100, 1000, true, true, 108, 32);
	if (error)
		return false;
	success &= assert_fragdb_handle(skb, VERDICT_STOLEN);

	success &= validate_database(2);
	success &= validate_list(&expected_keys[0], 2);
	clean_expired_buffers();
	success &= validate_database(2);
	success &= validate_list(&expected_keys[0], 2);

	/* After 2 seconds, packet 1 should die. */
	dummy_buffer = container_of(expire_list.next, struct reassembly_buffer, list_hook);
	dummy_buffer->dying_time = jiffies - 1;
	dummy_buffer = container_of(dummy_buffer->list_hook.next, struct reassembly_buffer, list_hook);
	dummy_buffer->dying_time = jiffies + msecs_to_jiffies(4000);

	clean_expired_buffers();
	success &= validate_database(1);
	success &= validate_list(&expected_keys[1], 1);

	/* After a while, packet 2 should die. */
	dummy_buffer->dying_time = jiffies - 1;

	clean_expired_buffers();
	success &= validate_database(0);

	return success;
}

int init_module(void)
{
	START_TESTS("Fragment database");

	if (is_error(config_init(false)))
		return -EINVAL;
	if (is_error(fragdb_init())) {
		config_destroy();
		return -EINVAL;
	}

	CALL_TEST(test_no_frags(), "Unfragmented IPv6 packet arrives");
	CALL_TEST(test_happy_path(), "Happy defragmentation.");
	CALL_TEST(test_timer(), "Timer test.");

	fragdb_destroy();
	config_destroy();

	END_TESTS;
}

void cleanup_module(void)
{
	/* No code. */
}
