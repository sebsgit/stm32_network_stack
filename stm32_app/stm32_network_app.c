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
 * stm32_network_app.c
 *
 * Implementation of the test application for the ENC28J60 driver.
 * */

#include "enc28j60.h"
#include "eth_packet_buff.h"
#include "stm32_network_app.h"
#include "debug_utils/enc28_debug.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/etharp.h>
#include <lwip/timeouts.h>

/*TODO:
 * - cleanup
 */

#define ASSERT_STATUS(s) if ((s) != ENC28_OK) { for(;;); }
#define PACKET_HANDLER_STACK_DEPTH_WORDS 600
#define IP_STACK_TASK_DEPTH_WORDS 1000
#define PACKET_HANDLER_TASK_PRIO 1
#define IP_STACK_TASK_PRIO 2

#define PACKET_PTR_SIZE (sizeof(void*))
#define STATIC_PACKET_QUEUE_SIZE (MAX_ETH_PACKETS * PACKET_PTR_SIZE)
#define USE_LWIP (1)

static volatile uint8_t exti_int_flag = 0;
static TaskHandle_t packet_task_handle;
static TaskHandle_t ip_task_handle;

static StaticQueue_t free_packet_buffer_queue_mem;
static StaticQueue_t ready_packet_buffer_queue_mem;
static StaticQueue_t transmit_packet_queue_mem;
static uint8_t free_packet_buff_storage[STATIC_PACKET_QUEUE_SIZE];
static uint8_t ready_packet_buff_storage[STATIC_PACKET_QUEUE_SIZE];
static uint8_t transmit_packet_storage[STATIC_PACKET_QUEUE_SIZE];
QueueHandle_t free_packet_buffer_queue;
QueueHandle_t ready_packet_buffer_queue;
QueueHandle_t transmit_packet_queue;

extern uint32_t HAL_GetTick(void);

void enc28_test_app_handle_packet_recv_interrupt(void)
{
	vTaskNotifyGiveFromISR(packet_task_handle, NULL);
}

static err_t enc28_netif_output(struct netif *netif, struct pbuf *p)
{
	struct eth_packet_buff_t *ip_resp = NULL;
	BaseType_t status = xQueueReceive(free_packet_buffer_queue, &ip_resp, 0);
	if (status == pdPASS)
	{
		ip_resp->used_bytes = p->len;
		memcpy(ip_resp->buf, p->payload, p->len);
		xQueueSend(transmit_packet_queue, &ip_resp, 0);
	}
	return ERR_OK;
}

u32_t sys_now(void)
{
	return HAL_GetTick();
}

static void enc28_pbuf_free(struct pbuf* p)
{
	xQueueSend(free_packet_buffer_queue, &p->enc28_eth_packet_ptr, portMAX_DELAY);
}

static err_t enc28_ip_init_callback(struct netif *n_if)
{
	n_if->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;;
	n_if->linkoutput = enc28_netif_output;
	n_if->output = etharp_output;
	n_if->mtu = 1518;

	n_if->hwaddr[0] = 0xde;
	n_if->hwaddr[1] = 0xad;
	n_if->hwaddr[2] = 0xbe;
	n_if->hwaddr[3] = 0xef;
	n_if->hwaddr[4] = 0xcc;
	n_if->hwaddr[5] = 0xaa;
	n_if->hwaddr_len = sizeof(n_if->hwaddr);
	return ERR_OK;
}

void ip_stack_task(void *arg)
{
	UBaseType_t stack_high_watermark = 0;
	struct netif net_ifc;
	struct ip4_addr my_addr;

	struct pbuf *input_buf;
	struct pbuf_custom pbuf_cst;

	pbuf_cst.custom_free_function = enc28_pbuf_free;

	my_addr.addr = (192 << 0) | (168 << 8) | (0 << 16) | (22 << 24);
	net_ifc.name[0] = 'e';
	net_ifc.name[1] = '1';

	lwip_init();

	{
		struct netif *res = netif_add(&net_ifc, &my_addr, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, enc28_ip_init_callback, netif_input);
		configASSERT(res != NULL);
		netif_set_default(&net_ifc);
		netif_set_up(&net_ifc);
	}

	while (1)
	{
		struct eth_packet_buff_t * ready_packet = NULL;
		BaseType_t status = xQueueReceive(ready_packet_buffer_queue, &ready_packet, portMAX_DELAY);

		configASSERT(status == pdPASS);
		configASSERT(ready_packet);

		{
#if USE_LWIP == 0
			if (enc28_debug_is_ping_request(ready_packet->buf, ready_packet->used_bytes))
			{
				struct eth_packet_buff_t *ping_resp = NULL;
				status = xQueueReceive(free_packet_buffer_queue, &ping_resp, 0);
				if (status == pdPASS)
				{
					ping_resp->used_bytes = ready_packet->used_bytes;
					int32_t resp_status = enc28_debug_handle_ping(ready_packet->buf,
							ready_packet->used_bytes,
							ping_resp->buf,
							ping_resp->used_bytes);
					if (resp_status == 0)
					{
						xQueueSend(transmit_packet_queue, &ping_resp, 0);
					}
					else
					{
						xQueueSend(free_packet_buffer_queue, &ping_resp, 0);
					}
				}
			}
			// put the packet back to the "free" queue
			xQueueSend(free_packet_buffer_queue, &ready_packet, 0);
#else
			// push the ETH packet to the lwIP stack
			input_buf = pbuf_alloced_custom(PBUF_RAW,
					ready_packet->used_bytes,
					PBUF_REF,
					&pbuf_cst,
					ready_packet->buf,
					ready_packet->used_bytes);
			configASSERT(input_buf);
			{
				// packet buffer is returned to "free" queue in enc28_pbuf_free function
				pbuf_cst.pbuf.enc28_eth_packet_ptr = ready_packet;
				err_t input_status = net_ifc.input(input_buf, &net_ifc);
				configASSERT(input_status == ERR_OK);
			}
#endif
		}

		sys_check_timeouts();

		stack_high_watermark = uxTaskGetStackHighWaterMark(NULL);
		configASSERT(stack_high_watermark > 0); // stack exhausted !
	}
}

extern void packet_handling_task(void * arg);

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

  transmit_packet_queue = xQueueCreateStatic(
		  MAX_ETH_PACKETS,
		  PACKET_PTR_SIZE,
		  transmit_packet_storage,
		  &transmit_packet_queue_mem);

  vQueueAddToRegistry(ready_packet_buffer_queue, "ready_packets");
  vQueueAddToRegistry(free_packet_buffer_queue, "free_packets");
  vQueueAddToRegistry(transmit_packet_queue, "transmit_queue");

  vTaskStartScheduler();
}

