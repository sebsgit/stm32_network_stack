/*
 * MIT License
 *
 * Copyright (c) 2024 Sebastian Baginski
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * enc28_debug.h
 *
 * Debugging utilities for the ENC28J60 Ethernet module driver.
 * */

#ifndef ENC28_DEBUG_H_
#define ENC28_DEBUG_H_

#include <stdint.h>

/*
 * Ethernet frame header
 * */
struct enc28_eth_header
{
	uint8_t mac_dest[6];
	uint8_t mac_src[6];
	uint16_t type_len;
};

/*
 * IPv4 packet header
 * */
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

/*
 * ICMP ping message header
 * */
struct enc28_icmp_ping_header
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t seq_num;
};

/*
 * @brief Checks if the provided data buffer contains a valid ICMP ping message
 * */
extern uint8_t enc28_debug_is_ping_request(const uint8_t *pkt_buf, uint16_t pkt_size);

/*
 * @brief Creates a ping message reposnse
 * */
extern int32_t enc28_debug_handle_ping(const uint8_t *pkt_buf, uint16_t pkt_size, uint8_t *resp_buf, uint16_t resp_size);

#endif /* ENC28_DEBUG_H_ */
