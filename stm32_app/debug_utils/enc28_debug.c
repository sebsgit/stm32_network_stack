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
	struct enc28_eth_header hdr;

	if (pkt_size != resp_size)
	{
		return -1;
	}

	if (pkt_size < (sizeof(struct enc28_eth_header) + sizeof(struct enc28_ipv4_header) + sizeof(struct enc28_icmp_ping_header)))
	{
		return -2;
	}

	memcpy(&hdr, pkt_buf, sizeof(hdr));
	memcpy(resp_buf, pkt_buf, resp_size);

	if ((hdr.type_len == 0x08))
	{
		// ipv4 frame
		struct enc28_ipv4_header ipv4;
		memcpy(&ipv4, pkt_buf + sizeof(hdr), sizeof(ipv4));

		if (ipv4.protocol == 0x1)
		{
			// ICMP
			struct enc28_icmp_ping_header icmp;
			memcpy(&icmp, pkt_buf + sizeof(hdr) + sizeof(ipv4), sizeof(icmp));

			if (icmp.type == 0x8)
			{
				// it's a ping request, send the ping reply
				uint8_t addr_buf[6];
				memcpy(addr_buf, hdr.mac_dest, sizeof(hdr.mac_dest));
				memcpy(hdr.mac_dest, hdr.mac_src, sizeof(hdr.mac_dest));
				memcpy(hdr.mac_src, addr_buf, sizeof(hdr.mac_src));

				memcpy(addr_buf, ipv4.addr_dest, sizeof(ipv4.addr_dest));
				memcpy(ipv4.addr_dest, ipv4.addr_src, sizeof(ipv4.addr_dest));
				memcpy(ipv4.addr_src, addr_buf, sizeof(ipv4.addr_src));

				icmp.type = 0x0; // ping response
				icmp.checksum += 0x8; // adjust checksum

				memcpy(resp_buf, &hdr, sizeof(hdr));
				memcpy(resp_buf + sizeof(hdr), &ipv4, sizeof(ipv4));
				memcpy(resp_buf + sizeof(hdr) + sizeof(ipv4), &icmp, sizeof(icmp));

				return 0;
			}
		}
	}

	return -3;
}
