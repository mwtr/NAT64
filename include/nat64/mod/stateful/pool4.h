#ifndef _JOOL_MOD_POOL4_H
#define _JOOL_MOD_POOL4_H

/**
 * @file
 * The pool of IPv4 addresses (and their ports and ICMP ids).
 *
 * @author Alberto Leiva
 */

#include <linux/types.h>
#include <linux/in.h>
#include "nat64/common/types.h"
#include "nat64/mod/stateful/poolnum.h"

/**
 * Readies the rest of this module for future use.
 *
 * @param addr_strs array of strings denoting the IP addresses the pool should start with.
 * @param addr_count size of the "addr_strs" array.
 * @return result status (< 0 on error).
 */
int pool4_init(char *addr_strs[], int addr_count);
/**
 * Frees resources allocated by the pool.
 */
void pool4_destroy(void);
/**
 * Removes all addresses (along with their ports and IDs) from the pool.
 */
int pool4_flush(void);
/**
 * Inserts the "addr" address (along with its 128k ports and 64k ICMP ids) to the pool.
 * These elements will then become borrowable through the pool_get* functions.
 */
int pool4_add(struct ipv4_prefix *prefix);
/**
 * Removes the "addr" address (along with its ports and IDs) from the pool.
 */
int pool4_remove(struct ipv4_prefix *prefix);

/**
 * Borrows "addr" from the pool. This function will only succeed if the exact combination of
 * address and port from "addr" can be found in the pool.
 *
 * Also, it won't return the address and port because you already have them in "addr";
 * it will simply return 0 if you can use the combination, and nonzero on failure.
 *
 * Warning: This function is pretty slow. Do not use it during packet processing.
 */
int pool4_get(l4_protocol l4_proto, struct ipv4_transport_addr *addr);
/**
 * Borrows an acceptable match for "addr" from the pool. That is, it'll borrow the same address as
 * "addr->address", and a similar ID as "addr->l4_id".
 *
 * If "proto" is UDP, then a 'similar ID' is one that has the same range (less than 1024 or higher
 * than 1023) and parity (even/odd).
 * If "proto" is TCP, then a 'similar ID' is one that has the same range.
 * If "proto" is ICMP, then a 'similar ID' is any ID.
 *
 * The function will not return the address because you already have it in "addr". The 'similar ID'
 * will be placed in the outgoing parameter, "result".
 */
int pool4_get_match(l4_protocol proto, struct ipv4_transport_addr *addr, __u16 *result);
/**
 * Borrows any port of the "addr" address from the pool.
 *
 * The borrowed port will be placed in the outgoing parameter, "result".
 */
int pool4_get_any_port(l4_protocol proto, const struct in_addr *addr, __u16 *result);
/**
 * Borrows any address that has a similar ID as "l4_id". This one is a little quirky in that it
 * falls back to returning any address with any ID if no similar one could be found.
 *
 * See pool4_get_match() for a definition of 'similar ID'.
 *
 * The resulting address-ID will be placed in the outgoing parameter, "result".
 */
int pool4_get_any_addr(l4_protocol proto, __u16 l4_id, struct ipv4_transport_addr *result);

/**
 * Returns the address-port combination from "addr" to the pool, so it can be borrowed again later.
 *
 * Don't sweat it too much if this function fails; the user might have removed the address from the
 * pool.
 */
int pool4_return(const l4_protocol l4_proto, const struct ipv4_transport_addr *addr);

/**
 * Returns whether the "addr" address is part of the pool.
 *
 * This function doesn't care if all of the ports and IDs from "addr" have been borrowed. All it
 * takes for an address to belong to the pool is to have been pool4_register()ed and not
 * pool4_remove()d.
 */
bool pool4_contains(__be32 addr);
/**
 * Executes the "func" function with the "arg" argument on every address in the pool.
 */
int pool4_for_each(int (*func)(struct ipv4_prefix *, void *), void * arg);
/**
 * Copies the current number of addresses in the pool to "result".
 */
int pool4_count(__u64 *result);
/**
 * Checks if the Pool is empty.
 * return false if is not empty, otherwise return true.
 */
bool pool4_is_empty(void);

#endif /* _JOOL_MOD_POOL4_H */
