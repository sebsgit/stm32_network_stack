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

  { // Setup packet receiving
	  status = enc28_select_register_bank(ctx, 0);
	  ASSERT_STATUS(status);

	  {
		  // update  ERDPT to point to the start of the ETH buffer
		  status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTL, ENC28_CONF_RX_ADDRESS_START & 0xFF);
		  ASSERT_STATUS(status);
		  status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTH, (ENC28_CONF_RX_ADDRESS_START >> 8) & 0x1F);
		  ASSERT_STATUS(status);
	  }

	  uint8_t mask = 0;
	  mask |= (1 << ENC28_EIE_INTIE);
	  mask |= (1 << ENC28_EIE_PKTIE);
	  mask |= (1 << ENC28_EIE_RXERIE);
	  status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_EIE, mask);
	  ASSERT_STATUS(status);

	  mask = (1 << ENC28_EIR_RXERIF);
	  status = enc28_do_clear_bits_ctl_reg(ctx, ENC28_CR_EIR, mask);
	  ASSERT_STATUS(status);

	  mask = (1 << ENC28_ECON2_AUTOINC);
	  status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_ECON2, mask);
	  ASSERT_STATUS(status);

	  mask = 0;
	  mask |= (1 << ENC28_ECON1_RXEN);
	  status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_ECON1, mask);
  }

	while (1)
	{
	  if (exti_int_flag)
	  {
		  exti_int_flag = 0;
		  uint8_t val = 0;
		  status = enc28_do_read_ctl_reg(ctx, ENC28_CR_EIR, &val);
		  ASSERT_STATUS(status);
		  if (val & (1 << ENC28_EIR_PKTIF))
		  {
			  printf("\n--- Interrupt: %x\n", val);
			  status = enc28_select_register_bank(ctx, 0);
			  ASSERT_STATUS(status);

			  uint16_t read_ptr = 0;
			  status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ERDPTL, &val);
			  read_ptr = val;
			  status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ERDPTH, &val);
			  read_ptr |= ((val & 0x1F) << 8);

			  printf("RDPTR: %d, ST: %d, ND: %d\n", read_ptr, ENC28_CONF_RX_ADDRESS_START, ENC28_CONF_RX_ADDRESS_END);

			  {
				  uint8_t command[7] = {0x3A, 0, 0, 0, 0, 0, 0};
				  uint8_t hdr[7] = {0, 0, 0, 0, 0, 0, 0};

				  ctx->nss_pin_op(0);
				  HAL_StatusTypeDef d = HAL_SPI_TransmitReceive(&hspi2, command, hdr, 7, HAL_MAX_DELAY);
				  if (d != HAL_OK)
				  {
					  for(;;);
				  }
				  ctx->nss_pin_op(1);
				  printf("HDR= %x %x | %x %x %x %x\n", hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6]);
				  uint16_t PP = (((hdr[2] & 0x1F) << 8) | hdr[1]);


				  ENC28_Receive_Status_Vector status_vec;
				  memcpy(&status_vec, hdr + 3, 4);

				  if (!status_vec.status_bits_lo.received_ok)
				  {
					  for(;;);
				  }

				  {
					  // update  ERDPT to skip the current packet next time
					  enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTL, hdr[1]);
					  enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTH, hdr[2]);
				  }

				  if ((PP -1 > ENC28_CONF_RX_ADDRESS_END) || (PP - 1 < ENC28_CONF_RX_ADDRESS_START) )
				  {
					  PP = ENC28_CONF_RX_ADDRESS_END;
				  }
				  else
				  {
					  PP -= 1;
				  }
				  printf("NEXT PP=%d\n", PP);

				  const uint16_t packet_len = (status_vec.packet_len_hi << 8) | status_vec.packet_len_lo;
				  printf("PACKET LEN= %d\n", packet_len);

				  status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTL, PP & 0xFF);
				  ASSERT_STATUS(status);
				  status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTH, (PP >> 8) & 0x1F);
				  ASSERT_STATUS(status);
			  }


			  {
				  status = enc28_select_register_bank(ctx, 1);
				  uint8_t mask;
				  ASSERT_STATUS(status);
				  status = enc28_do_read_ctl_reg(ctx, ENC28_CR_EPKTCNT, &mask);
				  ASSERT_STATUS(status);
				  printf("PKTCNT = %d\n", (int)mask);
			  }

			  uint8_t mask = (1 << ENC28_ECON2_PKTDEC);
			  status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_ECON2, mask);
			  ASSERT_STATUS(status);

		  }
		  else if (val & (1 << ENC28_EIR_RXERIF))
		  {
			  printf("PACKET RECV ERR\n");
			  status = enc28_select_register_bank(ctx, 0);
			  ASSERT_STATUS(status);

			  status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTL, ENC28_CONF_RX_ADDRESS_END & 0xFF);
			  ASSERT_STATUS(status);
			  status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTH, (ENC28_CONF_RX_ADDRESS_END >> 8) & 0x1F);
			  ASSERT_STATUS(status);

			  status = enc28_do_clear_bits_ctl_reg(ctx, ENC28_CR_EIR, (1 << ENC28_EIR_RXERIF));
			  ASSERT_STATUS(status);
		  }
	  }
	}
}

