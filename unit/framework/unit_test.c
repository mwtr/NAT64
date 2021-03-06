#include "nat64/unit/unit_test.h"
#include <linux/kernel.h>
#include "nat64/common/str_utils.h"


#define UNIT_WARNING(test_name, expected, actual, specifier) \
		log_err("Test '%s' failed. Expected: " specifier ". Actual: " specifier ".", \
				test_name, \
				expected, \
				actual);
#define UNIT_WARNING_NOT(test_name, expected, actual, specifier) \
		log_err("Test '%s' failed. Expected: not " specifier ". Actual: " specifier ".", \
				test_name, \
				expected, \
				actual);


bool assert_true(bool condition, char *test_name)
{
	if (!condition) {
		log_err("Test '%s' failed.", test_name);
		return false;
	}
	return true;
}

bool assert_equals_int(int expected, int actual, char *test_name)
{
	if (expected != actual) {
		UNIT_WARNING(test_name, expected, actual, "%d");
		return false;
	}
	return true;
}

bool assert_equals_ulong(unsigned long expected, unsigned long actual, char *test_name)
{
	if (expected != actual) {
		UNIT_WARNING(test_name, expected, actual, "%lu");
		return false;
	}
	return true;
}

bool assert_equals_u8(__u8 expected, __u8 actual, char *test_name)
{
	return assert_equals_u64(expected, actual, test_name);
}

bool assert_equals_u16(__u16 expected, __u16 actual, char *test_name)
{
	return assert_equals_u64(expected, actual, test_name);
}

bool assert_equals_be16(__be16 expected, __be16 actual, char *test_name)
{
	return assert_equals_u16(ntohs(expected), ntohs(actual), test_name);
}

bool assert_equals_u32(__u32 expected, __u32 actual, char *test_name)
{
	return assert_equals_u64(expected, actual, test_name);
}

bool assert_equals_be32(__be32 expected, __be32 actual, char *test_name)
{
	return assert_equals_u32(ntohl(expected), ntohl(actual), test_name);
}

bool assert_equals_u64(__u64 expected, __u64 actual, char *test_name)
{
	if (expected != actual) {
		UNIT_WARNING(test_name, expected, actual, "%llu");
		return false;
	}
	return true;
}

bool assert_equals_ptr(void *expected, void *actual, char *test_name)
{
	if (expected != actual) {
		UNIT_WARNING(test_name, expected, actual, "%p");
		return false;
	}
	return true;
}

bool assert_equals_ipv4(struct in_addr *expected, const struct in_addr *actual, char *test_name)
{
	if (!ipv4_addr_equals(expected, actual)) {
		UNIT_WARNING(test_name, expected, actual, "%pI4");
		return false;
	}
	return true;
}

bool assert_equals_ipv4_str(unsigned char *expected_str, const struct in_addr *actual,
		char *test_name)
{
	struct in_addr expected;

	if (str_to_addr4(expected_str, &expected) != 0) {
		log_err("Could not parse '%s' as a valid IPv4 address.", expected_str);
		return false;
	}

	return assert_equals_ipv4(&expected, actual, test_name);
}

bool assert_equals_ipv6(struct in6_addr *expected, const struct in6_addr *actual, char *test_name)
{
	if (!ipv6_addr_equals(expected, actual)) {
		UNIT_WARNING(test_name, expected, actual, "%pI6c");
		return false;
	}
	return true;
}

bool assert_equals_ipv6_str(unsigned char *expected_str, const struct in6_addr *actual,
		char *test_name)
{
	struct in6_addr expected;

	if (str_to_addr6(expected_str, &expected) != 0) {
		log_err("Could not parse '%s' as a valid IPv6 address.", expected_str);
		return false;
	}

	return assert_equals_ipv6(&expected, actual, test_name);
}

bool assert_equals_csum(__sum16 expected, __sum16 actual, char *test_name)
{
	if (expected != actual) {
		UNIT_WARNING(test_name, expected, actual, "%x");
		return false;
	}
	return true;
}

bool assert_range(unsigned int expected_min, unsigned int expected_max, unsigned int actual,
		char *test_name)
{
	if (actual < expected_min || expected_max < actual) {
		UNIT_WARNING(test_name, (expected_min + expected_max) / 2, actual, "%d");
		return false;
	}

	return true;
}

bool assert_null(void *actual, char *test_name)
{
	return assert_equals_ptr(NULL, actual, test_name);
}

bool assert_false(bool condition, char *test_name)
{
	if (condition) {
		log_err("Test '%s' failed.", test_name);
		return false;
	}
	return true;
}

bool assert_not_equals_int(int expected, int actual, char *test_name)
{
	if (expected == actual) {
		UNIT_WARNING_NOT(test_name, expected, actual, "%d");
		return false;
	}
	return true;
}

bool assert_not_equals_u16(__u16 expected, __u16 actual, char *test_name)
{
	if (expected == actual) {
		UNIT_WARNING_NOT(test_name, expected, actual, "%u");
		return false;
	}
	return true;
}

bool assert_not_equals_be16(__be16 expected, __be16 actual, char *test_name)
{
	return assert_not_equals_u16(ntohs(expected), ntohs(actual), test_name);
}

bool assert_not_equals_ptr(void *expected, void *actual, char *test_name)
{
	if (expected == actual) {
		UNIT_WARNING_NOT(test_name, expected, actual, "%p");
		return false;
	}
	return true;
}

bool assert_not_null(void *actual, char *test_name)
{
	return assert_not_equals_ptr(NULL, actual, test_name);
}

bool assert_equals_tuple(struct tuple *expected, struct tuple *actual, char *test_name)
{
	bool success = true;

	log_debug("%s", test_name);
	switch (expected->l3_proto) {
	case L3PROTO_IPV4:
		success &= assert_equals_ipv4(&expected->src.addr4.l3, &actual->src.addr4.l3, "Src addr");
		success &= assert_equals_u16(expected->src.addr4.l4, actual->src.addr4.l4, "src l4_id");
		success &= assert_equals_ipv4(&expected->dst.addr4.l3, &actual->dst.addr4.l3, "Dst addr");
		success &= assert_equals_u16(expected->dst.addr4.l4, actual->dst.addr4.l4, "dst l4_id");
		break;
	case L3PROTO_IPV6:
		success &= assert_equals_ipv6(&expected->src.addr6.l3, &actual->src.addr6.l3, "Src addr");
		success &= assert_equals_u16(expected->src.addr6.l4, actual->src.addr6.l4, "src l4_id");
		success &= assert_equals_ipv6(&expected->dst.addr6.l3, &actual->dst.addr6.l3, "Dst addr");
		success &= assert_equals_u16(expected->dst.addr6.l4, actual->dst.addr6.l4, "dst l4_id");
		break;
	}

	success &= assert_equals_u8(expected->l3_proto, actual->l3_proto, "l3_proto");
	success &= assert_equals_u8(expected->l4_proto, actual->l4_proto, "l4_proto");

	return success;
}

bool assert_list_count(int expected, struct list_head *head, char *test_name)
{
	struct list_head *node;
	int count = 0;

	list_for_each(node, head) {
		count++;
	}

	return assert_equals_int(expected, count, test_name);
}
