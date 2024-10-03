#include "enc28j60.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <lwip/init.h>

#include <debug/enc28_debug.h>

#define ASSERT_STATUS(s) if ((s) != ENC28_OK) { for(;;); }
#define PACKET_HANDLER_STACK_DEPTH_WORDS 600
#define IP_STACK_TASK_DEPTH_WORDS 1000
#define PACKET_HANDLER_TASK_PRIO 1
#define IP_STACK_TASK_PRIO 2

#define MAX_ETH_PACKETS 4
#define PACKET_PTR_SIZE (sizeof(void*))
#define STATIC_PACKET_QUEUE_SIZE (MAX_ETH_PACKETS * PACKET_PTR_SIZE)

static volatile uint8_t exti_int_flag = 0;
static TaskHandle_t packet_task_handle;
static TaskHandle_t ip_task_handle;

static StaticQueue_t free_packet_buffer_queue_mem;
static StaticQueue_t ready_packet_buffer_queue_mem;
static QueueHandle_t free_packet_buffer_queue;
static QueueHandle_t ready_packet_buffer_queue;
static uint8_t free_packet_buff_storage[STATIC_PACKET_QUEUE_SIZE];
static uint8_t ready_packet_buff_storage[STATIC_PACKET_QUEUE_SIZE];

#define MAX_ETH_PACKET_SIZE 1600

struct eth_packet_buff_t
{
	uint8_t buf[MAX_ETH_PACKET_SIZE];
	uint16_t used_bytes;
};

static struct eth_packet_buff_t eth_packets[MAX_ETH_PACKETS];

void enc28_test_app_handle_packet_recv_interrupt(void)
{
	vTaskNotifyGiveFromISR(packet_task_handle, NULL);
}

void ip_stack_task(void *arg)
{
	UBaseType_t stack_high_watermark = 0;

	lwip_init();

	while (1)
	{
		struct eth_packet_buff_t * ready_packet = NULL;
		BaseType_t status = xQueueReceive(ready_packet_buffer_queue, &ready_packet, portMAX_DELAY);

		configASSERT(status == pdPASS);
		configASSERT(ready_packet);

		{
			//TODO: push the ETH packet to the lwIP stack

			// put the packet back to the "free" queue
			xQueueSend(free_packet_buffer_queue, &ready_packet, 0);
		}

		stack_high_watermark = uxTaskGetStackHighWaterMark(NULL);
		configASSERT(stack_high_watermark > 0); // stack exhausted !
	}
}

void packet_handling_task(void * arg)
{
	ENC28_SPI_Context *ctx = (ENC28_SPI_Context*)arg;
	uint8_t pkt_buf[MAX_ETH_PACKET_SIZE];
	UBaseType_t stack_high_watermark = 0;
	ENC28_Receive_Status_Vector status_vec;
	ENC28_CommandStatus rcv_stat;

	for (size_t i = 0; i < sizeof(eth_packets) / sizeof(eth_packets[0]); ++i)
	{
		struct eth_packet_buff_t *item = &eth_packets[i];
		BaseType_t status = xQueueSend(free_packet_buffer_queue, &item, 0);
		configASSERT(status == pdPASS);
		memset(&eth_packets[i], 0xFF, sizeof(eth_packets[i]));
	}

	while (1)
	{
		rcv_stat = enc28_read_packet(ctx, pkt_buf, sizeof(pkt_buf), &status_vec);

		while (rcv_stat == ENC28_OK)
		{
			const uint16_t packet_len = (status_vec.packet_len_hi << 8) | status_vec.packet_len_lo;
			printf("GOT PACKET, LEN= %d\n", packet_len);

			{
				struct eth_packet_buff_t *free_buf = NULL;
				BaseType_t status = xQueueReceive(free_packet_buffer_queue, &free_buf, portMAX_DELAY);

				configASSERT(status == pdPASS);
				configASSERT(free_buf);

				free_buf->used_bytes = packet_len;
				memcpy(free_buf->buf, pkt_buf, packet_len);

				status = xQueueSend(ready_packet_buffer_queue, &free_buf, portMAX_DELAY);
				configASSERT(status == pdPASS);
			}

			//TODO debug: handle ping requests
			{
				struct enc28_eth_header hdr;
				memcpy(&hdr, pkt_buf, sizeof(hdr));

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

							memcpy(pkt_buf, &hdr, sizeof(hdr));
							memcpy(pkt_buf + sizeof(hdr), &ipv4, sizeof(ipv4));
							memcpy(pkt_buf + sizeof(hdr) + sizeof(ipv4), &icmp, sizeof(icmp));

							ENC28_CommandStatus send_stat = enc28_write_packet(ctx, pkt_buf, packet_len);
							configASSERT(send_stat == ENC28_OK);

						}
					}
				}
			}

			rcv_stat = enc28_read_packet(ctx, pkt_buf, sizeof(pkt_buf), &status_vec);
			stack_high_watermark = uxTaskGetStackHighWaterMark(NULL);
			configASSERT(stack_high_watermark > 0); // stack exhausted !
		}

		{
			ENC28_CommandStatus send_stat = enc28_check_outgoing_packet_status(ctx);
			if (send_stat == ENC28_OK)
			{
			//	printf("Packet sent\n");
			}
			stack_high_watermark = uxTaskGetStackHighWaterMark(NULL);
			configASSERT(stack_high_watermark > 0); // stack exhausted !
		}

		ulTaskNotifyTake(pdTRUE, 10);
	}
}

void enc28_test_app(ENC28_SPI_Context *ctx)
{
  ctx->nss_pin_op(1);

  printf("Soft reset\n");

  ENC28_CommandStatus status = enc28_do_soft_reset(ctx);
  ASSERT_STATUS(status);

  printf("Initializing...\n");

  ENC28_MAC_Address mac;
  mac.addr[0] = 0xDE;
  mac.addr[1] = 0xAD;
  mac.addr[2] = 0xBE;
  mac.addr[3] = 0xEF;
  mac.addr[4] = 0xCC;
  mac.addr[5] = 0xAA;
  status = enc28_do_init(mac, ctx);
  ASSERT_STATUS(status);

  {
	  ENC28_HW_Rev hw_rev;
	  status = enc28_do_read_hw_rev(ctx, &hw_rev);
	  ASSERT_STATUS(status);

	  printf("PHID1: 0x%x\n", hw_rev.phid1);
	  printf("PHID2: 0x%x\n", hw_rev.phid2);
	  printf("REV ID: %d\n", (int)hw_rev.ethrev);
  }

  status = enc28_begin_packet_transfer(ctx);
  ASSERT_STATUS(status);

  BaseType_t task_status = xTaskCreate(
		  packet_handling_task,
		  "packet_handler",
		  PACKET_HANDLER_STACK_DEPTH_WORDS,
		  ctx,
		  PACKET_HANDLER_TASK_PRIO,
		  &packet_task_handle);
  configASSERT(task_status == pdPASS);

  task_status = xTaskCreate(ip_stack_task,
		  "ip_stack",
		  IP_STACK_TASK_DEPTH_WORDS,
		  NULL,
		  IP_STACK_TASK_PRIO,
		  &ip_task_handle);
  configASSERT(task_status == pdPASS);

  free_packet_buffer_queue = xQueueCreateStatic(
		  MAX_ETH_PACKETS,
		  PACKET_PTR_SIZE,
		  free_packet_buff_storage,
		  &free_packet_buffer_queue_mem);
  configASSERT(free_packet_buffer_queue);

  ready_packet_buffer_queue = xQueueCreateStatic(
		  MAX_ETH_PACKETS,
		  PACKET_PTR_SIZE,
		  ready_packet_buff_storage,
		  &ready_packet_buffer_queue_mem);
  configASSERT(ready_packet_buffer_queue);

  vQueueAddToRegistry(ready_packet_buffer_queue, "ready_packet");
  vQueueAddToRegistry(free_packet_buffer_queue, "free_packet");

  vTaskStartScheduler();
}

