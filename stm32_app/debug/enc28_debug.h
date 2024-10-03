#ifndef ENC28_DEBUG_H_
#define ENC28_DEBUG_H_

#include <stdint.h>

struct enc28_eth_header
{
	uint8_t mac_dest[6];
	uint8_t mac_src[6];
	uint16_t type_len;
};

struct enc28_ipv4_header
{
	uint8_t version: 4;
	uint8_t ihl: 4;
	uint8_t dscp: 6;
	uint8_t ecn: 2;
	uint16_t total_len;
	uint16_t identification;
	uint16_t flags: 3;
	uint16_t offset: 13;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	uint8_t addr_src[4];
	uint8_t addr_dest[4];
};

_Static_assert(sizeof(struct enc28_ipv4_header) == 20);

struct enc28_icmp_ping_header
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t seq_num;
};

#endif /* ENC28_DEBUG_H_ */
