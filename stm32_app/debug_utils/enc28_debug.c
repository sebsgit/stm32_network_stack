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
 * enc28_debug.c
 *
 * Implementation of the debugging utilities.
 * */

#include "enc28_debug.h"
#include <string.h>

uint8_t enc28_debug_is_ping_request(const uint8_t *pkt_buf, uint16_t pkt_size)
{
	if (pkt_size < (sizeof(struct enc28_eth_header) + sizeof(struct enc28_ipv4_header) + sizeof(struct enc28_icmp_ping_header)))
	{
		return 0;
	}

	struct enc28_eth_header *hdr = (struct enc28_eth_header *)pkt_buf;

	if ((hdr->type_len == 0x08))
	{
		// ipv4 frame
		struct enc28_ipv4_header *ipv4 = (struct enc28_ipv4_header *)(pkt_buf + sizeof(*hdr));

		if (ipv4->protocol == 0x1)
		{
			// ICMP
			struct enc28_icmp_ping_header *icmp =
					(struct enc28_icmp_ping_header *)(pkt_buf + sizeof(*hdr) + sizeof(*ipv4));

			if (icmp->type == 0x8)
			{
				return 1;
			}
		}
	}

	return 0;
}

int32_t enc28_debug_handle_ping(const uint8_t *pkt_buf, uint16_t pkt_size, uint8_t *resp_buf, uint16_t resp_size)
{
	struct enc28_eth_header *src_eth_hdr;
	struct enc28_ipv4_header *src_ipv4;

	struct enc28_eth_header *dest_eth_hdr;
	struct enc28_ipv4_header *dest_ipv4;
	struct enc28_icmp_ping_header *dest_icmp;

	if (pkt_size != resp_size)
	{
		return -1;
	}

	if (!enc28_debug_is_ping_request(pkt_buf, pkt_size))
	{
		return -2;
	}

	memcpy(resp_buf, pkt_buf, resp_size);

	src_eth_hdr = (struct enc28_eth_header *)pkt_buf;
	dest_eth_hdr = (struct enc28_eth_header *)resp_buf;
	src_ipv4 = (struct enc28_ipv4_header *)(pkt_buf + sizeof(*src_eth_hdr));
	dest_ipv4 = (struct enc28_ipv4_header *)(resp_buf + sizeof(*dest_eth_hdr));
	dest_icmp = (struct enc28_icmp_ping_header *)(resp_buf + sizeof(*dest_eth_hdr) + sizeof(*dest_ipv4));

	memcpy(dest_eth_hdr->mac_dest, src_eth_hdr->mac_src, sizeof(dest_eth_hdr->mac_dest));
	memcpy(dest_eth_hdr->mac_src, src_eth_hdr->mac_dest, sizeof(dest_eth_hdr->mac_src));
	memcpy(dest_ipv4->addr_dest, src_ipv4->addr_src, sizeof(dest_ipv4->addr_dest));
	memcpy(dest_ipv4->addr_src, src_ipv4->addr_dest, sizeof(dest_ipv4->addr_src));

	dest_icmp->type = 0x0; // ping response
	dest_icmp->checksum += 0x8; // adjust checksum

	return 0;
}
