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

#define ASSERT_STATUS(s) if ((s) != ENC28_OK) { for(;;); }
#define PACKET_HANDLER_STACK_DEPTH_WORDS 600
#define IP_STACK_TASK_DEPTH_WORDS 1000
#define PACKET_HANDLER_TASK_PRIO 1
#define IP_STACK_TASK_PRIO 2

#define PACKET_PTR_SIZE (sizeof(void*))
#define STATIC_PACKET_QUEUE_SIZE (MAX_ETH_PACKETS * PACKET_PTR_SIZE)

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

void enc28_test_app_handle_packet_recv_interrupt(void)
{
	vTaskNotifyGiveFromISR(packet_task_handle, NULL);
}

extern void ip_stack_task(void *arg);
extern void packet_handling_task(void * arg);

void enc28_test_app(ENC28_SPI_Context *ctx)
{
  ctx->nss_pin_op(1);

  printf("Soft reset\n");

  ENC28_CommandStatus status = enc28_do_soft_reset(ctx);
  ASSERT_STATUS(status);

  printf("Initializing...\n");

  ENC28_MAC_Address mac;
  mac.addr[0] = MAC_ADDR_BYTE_0;
  mac.addr[1] = MAC_ADDR_BYTE_1;
  mac.addr[2] = MAC_ADDR_BYTE_2;
  mac.addr[3] = MAC_ADDR_BYTE_3;
  mac.addr[4] = MAC_ADDR_BYTE_4;
  mac.addr[5] = MAC_ADDR_BYTE_5;
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

