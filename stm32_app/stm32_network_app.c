#include "enc28j60.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#define ASSERT_STATUS(s) if ((s) != ENC28_OK) { for(;;); }

static volatile uint8_t exti_int_flag = 0;

extern SPI_HandleTypeDef hspi2;

void enc28_debug_do_reg_dump(ENC28_SPI_Context *ctx)
{
	assert(ctx);

	uint8_t reg_id;
	for (reg_id = 0x0; reg_id <= 0x1F; ++reg_id)
	{
		uint8_t reg_value = 0xFF;
		ENC28_CommandStatus status = enc28_do_read_ctl_reg(ctx, reg_id, &reg_value);
		if (status == ENC28_OK)
		{
			printf("reg read: %d = %d\n", (int)reg_id, (int)reg_value);
		}
		else
		{
			printf("error reading register %d: err code = %d\n", (int)reg_id, (int)status);
		}
	}
}

void enc28_test_app_handle_packet_recv_interrupt(void)
{
	exti_int_flag = 1;
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

  uint8_t pkt_buf[1600];

	while (1)
	{
	  if (exti_int_flag)
	  {
		  exti_int_flag = 0;

		  memset(pkt_buf, 0xFF, sizeof(pkt_buf));
		  ENC28_Receive_Status_Vector status_vec;
		  ENC28_CommandStatus rcv_stat = enc28_read_packet(ctx, pkt_buf, 1600, &status_vec);
		  while (rcv_stat == ENC28_OK)
		  {
			  const uint16_t packet_len = (status_vec.packet_len_hi << 8) | status_vec.packet_len_lo;
			  printf("GOT PACKET, LEN= %d\n", packet_len);

			  memset(pkt_buf, 0xFF, sizeof(pkt_buf));
			  rcv_stat = enc28_read_packet(ctx, pkt_buf, 1600, &status_vec);
		  }
	  }
	}
}

