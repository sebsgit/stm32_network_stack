#include "enc28j60.h"

#define EXIT_IF_ERR(s) if((s) != ENC28_OK) { return (s); }

/*
 * Datasheet for ENC28J60, page 12: " (...) The register at address 1Ah in
 * each bank is reserved; read and write operations
 * should not be performed on this register."
* */
#define CHECK_RESERVED_REG(reg_id) if ((reg_id) == 0x1A) { return ENC28_INVALID_REGISTER; }

static ENC28_CommandStatus priv_enc28_do_buffer_register_init(ENC28_SPI_Context *ctx)
{
	uint8_t addr_lo = ENC28_CONF_RX_ADDRESS_START & 0xFF;
	uint8_t addr_hi = (ENC28_CONF_RX_ADDRESS_START >> 8) & 0x1F;
	ENC28_CommandStatus status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXSTL, addr_lo);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXSTH, addr_hi);
	EXIT_IF_ERR(status);

	addr_lo = ENC28_CONF_RX_ADDRESS_END & 0xFF;
	addr_hi = (ENC28_CONF_RX_ADDRESS_END >> 8) & 0x1F;
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXNDL, addr_lo);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXNDH, addr_hi);
	EXIT_IF_ERR(status);

	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTL, addr_lo);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTH, addr_hi);
	EXIT_IF_ERR(status);

	return ENC28_OK;
}

static ENC28_CommandStatus priv_enc28_do_receive_filter_init(ENC28_SPI_Context *ctx)
{
	return enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXFCON, ENC28_CONF_PACKET_FILTER_MASK);
}

static ENC28_CommandStatus priv_enc28_do_poll_estat_clk(ENC28_SPI_Context *ctx)
{
	uint8_t reg_value = 0;
	ENC28_CommandStatus status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ESTAT, &reg_value);

	while (status == ENC28_OK)
	{
		if (reg_value & ENC28_ESTAT_CLKRDY)
		{
			break;
		}
		status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ESTAT, &reg_value);
	}

	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_init(ENC28_SPI_Context *ctx)
{
	// 1. program the ERXST and ERXND pointers
	// 2. program the ERXRDPT register
	ENC28_CommandStatus status = priv_enc28_do_buffer_register_init(ctx);
	EXIT_IF_ERR(status);

	// 3. program the "receive filters" in ERXFCON
	status = priv_enc28_do_receive_filter_init(ctx);
	EXIT_IF_ERR(status);

	// 4. poll ESTAT.CLKRDY before initialising MAC address
	status = priv_enc28_do_poll_estat_clk(ctx);
	EXIT_IF_ERR(status);

	// MAC initialization
	// PHY initialization

	return status;
}

ENC28_CommandStatus enc28_prepare_read_ctl_reg(uint8_t *out, uint8_t reg_id)
{
	CHECK_RESERVED_REG(reg_id);

	if (!out)
	{
		return ENC28_INVALID_PARAM;
	}

	*out = (ENC28_OP_RCR << ENC28_SPI_ARG_BITS) | (reg_id & ENC28_SPI_ARG_MASK);

	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_read_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t *reg_value)
{
	if (!ctx)
	{
		return ENC28_INVALID_PARAM;
	}

	if ((!ctx->nss_pin_op) || (!ctx->spi_in_op) || (!ctx->spi_out_op))
	{
		return ENC28_INVALID_PARAM;
	}

	if (!reg_value)
	{
		return ENC28_INVALID_PARAM;
	}

	{
		uint8_t cmd_buff;
		const ENC28_CommandStatus status = enc28_prepare_read_ctl_reg(&cmd_buff, reg_id);
		if (status != ENC28_OK)
		{
			return status;
		}

		ctx->nss_pin_op(0);
		ctx->spi_out_op(&cmd_buff, 1);
		ctx->spi_in_op(reg_value, 1);
		ctx->nss_pin_op(1);
	}

	return ENC28_OK;
}

ENC28_CommandStatus enc28_prepare_write_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t in)
{
	CHECK_RESERVED_REG(reg_id);

	if (!out)
	{
		return ENC28_INVALID_PARAM;
	}

	const uint8_t command_byte = (ENC28_OP_WCR << ENC28_SPI_ARG_BITS) | (reg_id & ENC28_SPI_ARG_MASK);
	*out = (command_byte << 8) | in;

	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_write_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t reg_value)
{
	if (!ctx)
	{
		return ENC28_INVALID_PARAM;
	}

	if ((!ctx->nss_pin_op) || (!ctx->spi_in_op) || (!ctx->spi_out_op))
	{
		return ENC28_INVALID_PARAM;
	}

	{
		uint16_t cmd_buff;
		const ENC28_CommandStatus status = enc28_prepare_write_ctl_reg(&cmd_buff, reg_id, reg_value);
		if (status != ENC28_OK)
		{
			return status;
		}

		ctx->nss_pin_op(0);
		ctx->spi_out_op((const uint8_t*)&cmd_buff, 2);
		ctx->nss_pin_op(1);
	}

	return ENC28_OK;
}
