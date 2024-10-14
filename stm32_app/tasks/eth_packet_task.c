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
 * eth_packet_task.c
 *
 * Implementation of the ethernet packet handling task
 * */

#include "enc28j60.h"
#include "eth_packet_buff.h"
#include "stm32_network_app.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <string.h>
#include <stdio.h>

extern QueueHandle_t free_packet_buffer_queue;
extern QueueHandle_t ready_packet_buffer_queue;
extern QueueHandle_t transmit_packet_queue;

static struct eth_packet_buff_t eth_packets[MAX_ETH_PACKETS];

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

			rcv_stat = enc28_read_packet(ctx, pkt_buf, sizeof(pkt_buf), &status_vec);
			stack_high_watermark = uxTaskGetStackHighWaterMark(NULL);
			configASSERT(stack_high_watermark > 0); // stack exhausted !
		}


		{
			{
				struct eth_packet_buff_t *to_send = NULL;
				BaseType_t status = xQueueReceive(transmit_packet_queue, &to_send, 0);
				if (status == pdPASS)
				{
					ENC28_CommandStatus send_stat = enc28_write_packet(ctx, to_send->buf, to_send->used_bytes);
					configASSERT(send_stat == ENC28_OK);
					xQueueSend(free_packet_buffer_queue, &to_send, 0);
				}
			}

			stack_high_watermark = uxTaskGetStackHighWaterMark(NULL);
			configASSERT(stack_high_watermark > 0); // stack exhausted !
		}

		ulTaskNotifyTake(pdTRUE, 10);
	}
}
