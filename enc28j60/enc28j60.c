#include "enc28j60.h"

#define EXIT_IF_ERR(s) if((s) != ENC28_OK) { return (s); }

/*
 * Datasheet for ENC28J60, page 12: " (...) The register at address 1Ah in
 * each bank is reserved; read and write operations
 * should not be performed on this register."
* */
#define CHECK_RESERVED_REG(reg_id) if ((reg_id) == 0x1A) { return ENC28_INVALID_REGISTER; }

//TODO refactor : keep bank ID together with register name
static uint8_t priv_enc28_curr_bank = 0;

static uint8_t priv_enc28_is_phy_reg(uint8_t reg_id)
{
	return (reg_id <= 0x03) || (reg_id >= 0x10 && reg_id <= 0x14);
}

static uint8_t priv_enc28_is_mac_or_mii_reg(uint8_t reg_id)
{
	if (priv_enc28_curr_bank == 2)
	{
		return (reg_id == ENC28_CR_MACON1) ||
				(reg_id == ENC28_CR_MACON3) ||
				(reg_id == ENC28_CR_MACON4) ||
				(reg_id == ENC28_CR_MABBIPG) ||
				(reg_id == ENC28_CR_MAIPGL) ||
				(reg_id == ENC28_CR_MAIPGH) ||
				(reg_id == ENC28_CR_MACLCON1) ||
				(reg_id == ENC28_CR_MACLCON2) ||
				(reg_id == ENC28_CR_MAMXFLH) ||
				(reg_id == ENC28_CR_MAMXFLL) ||
				(reg_id == ENC28_CR_MICMD) ||
				(reg_id == ENC28_CR_MIREGADR) ||
				(reg_id == ENC28_CR_MIWRL) ||
				(reg_id == ENC28_CR_MIWRH) ||
				(reg_id == ENC28_CR_MIRDL) ||
				(reg_id == ENC28_CR_MIRDH);
	}
	else if (priv_enc28_curr_bank == 3)
	{
		return (reg_id == ENC28_CR_MISTAT) ||
				(reg_id == ENC28_CR_MAC_ADD1) ||
				(reg_id == ENC28_CR_MAC_ADD2) ||
				(reg_id == ENC28_CR_MAC_ADD3) ||
				(reg_id == ENC28_CR_MAC_ADD4) ||
				(reg_id == ENC28_CR_MAC_ADD5) ||
				(reg_id == ENC28_CR_MAC_ADD6);
	}
	else
	{
		return 0;
	}
}

static ENC28_CommandStatus priv_enc28_do_buffer_register_init(ENC28_SPI_Context *ctx)
{
	uint8_t addr_lo = ENC28_CONF_RX_ADDRESS_START & 0xFF;
	uint8_t addr_hi = (ENC28_CONF_RX_ADDRESS_START >> 8) & 0x1F;

	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 0);

	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXSTL, addr_lo);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXSTH, addr_hi);
	EXIT_IF_ERR(status);

	addr_lo = ENC28_CONF_RX_ADDRESS_END & 0xFF;
	addr_hi = (ENC28_CONF_RX_ADDRESS_END >> 8) & 0x1F;
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXNDL, addr_lo);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXNDH, addr_hi);
	EXIT_IF_ERR(status);

	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTL, addr_lo);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTH, addr_hi);
	EXIT_IF_ERR(status);

	return ENC28_OK;
}

static ENC28_CommandStatus priv_enc28_do_receive_filter_init(ENC28_SPI_Context *ctx)
{
	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 1);
	EXIT_IF_ERR(status);

	return enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXFCON, ENC28_CONF_PACKET_FILTER_MASK);
}

static ENC28_CommandStatus priv_enc28_do_poll_estat_clk(ENC28_SPI_Context *ctx)
{
	uint8_t reg_value = 0;
	ENC28_CommandStatus status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ESTAT, &reg_value);

	while (status == ENC28_OK)
	{
		if (reg_value & (1 << ENC28_ESTAT_CLKRDY))
		{
			break;
		}
		status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ESTAT, &reg_value);
	}

	return ENC28_OK;
}

static ENC28_CommandStatus priv_enc28_do_mac_init(const ENC28_MAC_Address mac_add, ENC28_SPI_Context *ctx)
{
	uint8_t is_full_duplex = 0;
	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 2); //TODO do not hardcode 2
	EXIT_IF_ERR(status);

	{
		const uint8_t macon1_mask = (1 << ENC28_MACON1_RXEN)
								| (1 << ENC28_MACON1_RXPAUS)
								| (1 << ENC28_MACON1_TXPAUS);

		status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_MACON1, macon1_mask);
		EXIT_IF_ERR(status);
	}

	{
		uint16_t phcon1_value = 0;
		status = enc28_do_read_phy_register(ctx, ENC28_PHYR_PHCON1, &phcon1_value);
		EXIT_IF_ERR(status);
		is_full_duplex = (phcon1_value & (1 << ENC28_PHCON1_PDPXMD)) != 0;
	}

	{
		uint8_t macon3_mask = ENC28_CONF_MACON3_FRAME_PAD_MASK |
					(1 << ENC28_MACON3_TXCRCEN);
		if (is_full_duplex)
		{
			macon3_mask |= (1 << ENC28_MACON3_FULLDPX);
		}
		status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_MACON3, macon3_mask);
		EXIT_IF_ERR(status);
	}

	{
		const uint8_t macon4_mask = (1 << ENC28_MACON4_DEFER);
		status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_MACON4, macon4_mask);
		EXIT_IF_ERR(status);
	}

	{
		const uint8_t max_fr_len_lo = ENC28_CONF_MAX_FRAME_LEN & 0xFF;
		const uint8_t max_fr_len_hi = (ENC28_CONF_MAX_FRAME_LEN >> 8) & 0xFF;
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAMXFLL, max_fr_len_lo);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAMXFLH, max_fr_len_hi);
		EXIT_IF_ERR(status);
	}

	{
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MABBIPG, ENC28_CONF_MABBIPG_BITS);
		EXIT_IF_ERR(status);
	}

	{
		status = enc28_do_write_ctl_reg(ctx,
				ENC28_CR_MAIPGL,
				is_full_duplex ? ENC28_CONF_MAIPGL_BITS_FULLDUP : ENC28_CONF_MAIPGL_BITS_HALFDUP);
		EXIT_IF_ERR(status);

		if (!is_full_duplex)
		{
			status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAIPGH, ENC28_CONF_MAIPGH_BITS);
			EXIT_IF_ERR(status);
		}
	}

	{
		status = enc28_select_register_bank(ctx, 3);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAC_ADD1, mac_add.addr[0]);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAC_ADD2, mac_add.addr[1]);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAC_ADD3, mac_add.addr[2]);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAC_ADD4, mac_add.addr[3]);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAC_ADD5, mac_add.addr[4]);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MAC_ADD6, mac_add.addr[5]);
		EXIT_IF_ERR(status);
	}

	{
		status = enc28_select_register_bank(ctx, 0);
		EXIT_IF_ERR(status);
	}

	return ENC28_OK;
}

static ENC28_CommandStatus priv_enc28_do_phy_init(ENC28_SPI_Context *ctx)
{
	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_init(const ENC28_MAC_Address mac_add, ENC28_SPI_Context *ctx)
{
	// program the ERXST and ERXND pointers
	// program the ERXRDPT register
	ENC28_CommandStatus status = priv_enc28_do_buffer_register_init(ctx);
	EXIT_IF_ERR(status);

	// program the "receive filters" in ERXFCON
	status = priv_enc28_do_receive_filter_init(ctx);
	EXIT_IF_ERR(status);

	// poll ESTAT.CLKRDY before initialising MAC address
	status = priv_enc28_do_poll_estat_clk(ctx);
	EXIT_IF_ERR(status);

	// MAC initialization
	status = priv_enc28_do_mac_init(mac_add, ctx);
	EXIT_IF_ERR(status);

	// PHY initialization
	status = priv_enc28_do_phy_init(ctx);

	return status;
}

ENC28_CommandStatus enc28_do_soft_reset(ENC28_SPI_Context *ctx)
{
	if (!ctx)
	{
		return ENC28_INVALID_PARAM;
	}

	if ((!ctx->nss_pin_op) || (!ctx->spi_in_op) || (!ctx->spi_out_op))
	{
		return ENC28_INVALID_PARAM;
	}

	uint8_t cmd_buff = 0xFF;

	ctx->nss_pin_op(0);
	ctx->spi_out_op(&cmd_buff, 1);
	ctx->nss_pin_op(1);

	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_read_hw_rev(ENC28_SPI_Context *ctx, ENC28_HW_Rev *hw_rev)
{
	if (!hw_rev)
	{
		return ENC28_INVALID_PARAM;
	}

	ENC28_CommandStatus status = enc28_do_read_phy_register(
			ctx,
			ENC28_PHYR_PHID1,
			&hw_rev->phid1);
	EXIT_IF_ERR(status);

	status = enc28_do_read_phy_register(
			ctx,
			ENC28_PHYR_PHID2,
			&hw_rev->phid2);
	EXIT_IF_ERR(status);

	status = enc28_select_register_bank(ctx, 3);
	EXIT_IF_ERR(status);

	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_EREVID, &hw_rev->ethrev);

	return status;
}

ENC28_CommandStatus enc28_do_read_mac(ENC28_SPI_Context *ctx, ENC28_MAC_Address *mac)
{
	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 3);
	EXIT_IF_ERR(status);
	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MAC_ADD1, &mac->addr[0]);
	EXIT_IF_ERR(status);
	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MAC_ADD2, &mac->addr[1]);
	EXIT_IF_ERR(status);
	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MAC_ADD3, &mac->addr[2]);
	EXIT_IF_ERR(status);
	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MAC_ADD4, &mac->addr[3]);
	EXIT_IF_ERR(status);
	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MAC_ADD5, &mac->addr[4]);
	EXIT_IF_ERR(status);
	status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MAC_ADD6, &mac->addr[5]);

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

		const uint8_t skip_dummy_byte = priv_enc28_is_mac_or_mii_reg(reg_id);

		ctx->nss_pin_op(0);
		ctx->spi_out_op(&cmd_buff, 1);
		if (skip_dummy_byte)
		{
			uint8_t buff[2] = {0, 0};
			ctx->spi_in_op(buff, 2);
			*reg_value = buff[1];
		}
		else
		{
			ctx->spi_in_op(reg_value, 1);
		}
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

	if ((!ctx->nss_pin_op) || (!ctx->spi_in_op) || (!ctx->spi_out_op) || (!ctx->wait_nano))
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
		ctx->wait_nano(50); // CS setup time
		uint8_t send_buff[2] = {(cmd_buff >> 8), cmd_buff & 0xFF};
		ctx->spi_out_op(send_buff, 2);
		ctx->wait_nano(210); // CS hold time (TODO: MAC and MII registers = 210, ETH registers = 10)
		ctx->nss_pin_op(1);
	}

	return ENC28_OK;
}

ENC28_CommandStatus enc28_prepare_set_bits_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t mask)
{
	CHECK_RESERVED_REG(reg_id);

	if (!out)
	{
		return ENC28_INVALID_PARAM;
	}

	const uint8_t command_byte = (ENC28_OP_BFS << ENC28_SPI_ARG_BITS) | (reg_id & ENC28_SPI_ARG_MASK);
	*out = (command_byte << 8) | mask;

	return ENC28_OK;
}

ENC28_CommandStatus enc28_prepare_clear_bits_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t mask)
{
	CHECK_RESERVED_REG(reg_id);

	if (!out)
	{
		return ENC28_INVALID_PARAM;
	}

	const uint8_t command_byte = (ENC28_OP_BFC << ENC28_SPI_ARG_BITS) | (reg_id & ENC28_SPI_ARG_MASK);
	*out = (command_byte << 8) | mask;

	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_set_bits_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t mask)
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
		const ENC28_CommandStatus status = enc28_prepare_set_bits_ctl_reg(&cmd_buff, reg_id, mask);
		if (status != ENC28_OK)
		{
			return status;
		}

		ctx->nss_pin_op(0);
		uint8_t send_buff[2] = {(cmd_buff >> 8), cmd_buff & 0xFF};
		ctx->spi_out_op(send_buff, 2);
		ctx->nss_pin_op(1);
	}

	return ENC28_OK;
}

ENC28_CommandStatus enc28_do_clear_bits_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t mask)
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
		const ENC28_CommandStatus status = enc28_prepare_clear_bits_ctl_reg(&cmd_buff, reg_id, mask);
		if (status != ENC28_OK)
		{
			return status;
		}

		ctx->nss_pin_op(0);
		uint8_t send_buff[2] = {(cmd_buff >> 8), cmd_buff & 0xFF};
		ctx->spi_out_op(send_buff, 2);
		ctx->nss_pin_op(1);
	}

	return ENC28_OK;
}

ENC28_CommandStatus enc28_select_register_bank(ENC28_SPI_Context *ctx, const uint8_t bank_id)
{
	if (bank_id >= 4)
	{
		return ENC28_INVALID_PARAM;
	}

	uint8_t econ1_reg = 0;
	ENC28_CommandStatus status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ECON1, &econ1_reg);
	EXIT_IF_ERR(status);

	econ1_reg &= ~(ENC28_ECON1_BSEL);
	econ1_reg |= ENC28_ECON1_BANK_SEL(bank_id);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ECON1, econ1_reg);

	if (status == ENC28_OK)
	{
		priv_enc28_curr_bank = bank_id;
	}

	return status;
}

ENC28_CommandStatus enc28_do_read_phy_register(ENC28_SPI_Context *ctx, uint8_t reg_id, uint16_t *reg_value)
{
	if (!priv_enc28_is_phy_reg(reg_id))
	{
		return ENC28_INVALID_PARAM;
	}

	if (!ctx)
	{
		return ENC28_INVALID_PARAM;
	}

	if ((!ctx->nss_pin_op) || (!ctx->spi_in_op) || (!ctx->spi_out_op) || (!ctx->wait_nano))
	{
		return ENC28_INVALID_PARAM;
	}

	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 2);
	EXIT_IF_ERR(status);

	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MIREGADR, reg_id);
	EXIT_IF_ERR(status);

	{
		const uint8_t miird_mask = (1 << ENC28_MICMD_MIIRD);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MICMD, miird_mask);
		EXIT_IF_ERR(status);
	}

	ctx->wait_nano(11 * 1000);

	{
		status = enc28_select_register_bank(ctx, 3);
		EXIT_IF_ERR(status);

		while (status == ENC28_OK)
		{
			uint8_t mistat_value = 0xff;
			status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MISTAT, &mistat_value);
			EXIT_IF_ERR(status);
			if ((mistat_value & (1 << ENC28_MISTAT_BUSY)) == 0)
			{
				break;
			}
			ctx->wait_nano(1);
		}
		EXIT_IF_ERR(status);
	}

	{
		status = enc28_select_register_bank(ctx, 2);
		EXIT_IF_ERR(status);

		const uint8_t miird_mask = 0;
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_MICMD, miird_mask);
		EXIT_IF_ERR(status);
	}

	{
		uint8_t reg_val_lo = 0xff;
		uint8_t reg_val_hi = 0xff;
		status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MIRDL, &reg_val_lo);
		EXIT_IF_ERR(status);
		status = enc28_do_read_ctl_reg(ctx, ENC28_CR_MIRDH, &reg_val_hi);
		EXIT_IF_ERR(status);
		*reg_value = (reg_val_hi << 8) | reg_val_lo;
	}

	return ENC28_OK;
}

ENC28_CommandStatus enc28_begin_packet_transfer(ENC28_SPI_Context *ctx)
{
	uint8_t mask;
	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 0);
	EXIT_IF_ERR(status);

	// update  ERDPT to point to the start of the ETH buffer
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTL, ENC28_CONF_RX_ADDRESS_START & 0xFF);
	EXIT_IF_ERR(status);
	status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTH, (ENC28_CONF_RX_ADDRESS_START >> 8) & 0x1F);
	EXIT_IF_ERR(status);

	mask = 0;
	mask |= (1 << ENC28_EIE_INTIE);
	mask |= (1 << ENC28_EIE_PKTIE);
	mask |= (1 << ENC28_EIE_RXERIE);
	status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_EIE, mask);
	EXIT_IF_ERR(status);

	mask = (1 << ENC28_EIR_RXERIF);
	status = enc28_do_clear_bits_ctl_reg(ctx, ENC28_CR_EIR, mask);
	EXIT_IF_ERR(status);

	mask = (1 << ENC28_ECON2_AUTOINC);
	status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_ECON2, mask);
	EXIT_IF_ERR(status);

	mask = 0;
	mask |= (1 << ENC28_ECON1_RXEN);
	status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_ECON1, mask);
	return status;
}

ENC28_CommandStatus enc28_read_packet(ENC28_SPI_Context *ctx, uint8_t *packet_buf, uint16_t buf_size, ENC28_Receive_Status_Vector *opt_status_vec)
{
	uint8_t val = 0;
	ENC28_CommandStatus status = enc28_do_read_ctl_reg(ctx, ENC28_CR_EIR, &val);
	EXIT_IF_ERR(status);

	if (val & (1 << ENC28_EIR_PKTIF))
	{
		uint16_t read_ptr = 0;

		status = enc28_select_register_bank(ctx, 0);
		EXIT_IF_ERR(status);
		status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ERDPTL, &val);
		read_ptr = val;
		status = enc28_do_read_ctl_reg(ctx, ENC28_CR_ERDPTH, &val);
		read_ptr |= ((val & 0x1F) << 8);

		if ((read_ptr < ENC28_CONF_RX_ADDRESS_START) || (read_ptr > ENC28_CONF_RX_ADDRESS_END))
		{
			return ENC28_READ_PTR_OUT_OF_RANGE;
		}

		uint8_t command[7] = {0x3A, 0, 0, 0, 0, 0, 0};
		uint8_t hdr[7] = {0, 0, 0, 0, 0, 0, 0};

		ctx->nss_pin_op(0);
		ctx->spi_in_out_op(command, hdr, 7);
		ctx->nss_pin_op(1);

		ENC28_Receive_Status_Vector status_vec;
		status_vec.packet_len_lo = hdr[3];
		status_vec.packet_len_hi = hdr[4];
		*((uint8_t*)&status_vec.status_bits_lo) = hdr[5];
		*((uint8_t*)&status_vec.status_bits_hi) = hdr[6];

		if (opt_status_vec)
		{
			*opt_status_vec = status_vec;
		}

		if (!status_vec.status_bits_lo.received_ok)
		{
			return ENC28_PACKET_RCV_ERR;
		}

		{
			const uint16_t packet_len = (status_vec.packet_len_hi << 8) | status_vec.packet_len_lo;
			if (packet_len > buf_size)
			{
				return ENC28_BUFFER_TOO_SMALL;
			}

			ctx->nss_pin_op(0);
			ctx->spi_out_op(command, 1);
			ctx->spi_in_op(packet_buf, packet_len);
			ctx->nss_pin_op(1);
		}

		{ // Update ERXDPT according to the errata
			uint16_t PP = (((hdr[2] & 0x1F) << 8) | hdr[1]);
			{
				// update  ERDPT to skip the current packet next time
				status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTL, hdr[1]);
				EXIT_IF_ERR(status);

				status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERDPTH, hdr[2]);
				EXIT_IF_ERR(status);
			}

			if ((PP -1 > ENC28_CONF_RX_ADDRESS_END) || (PP - 1 < ENC28_CONF_RX_ADDRESS_START) )
			{
				PP = ENC28_CONF_RX_ADDRESS_END;
			}
			else
			{
				PP -= 1;
			}

			status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTL, PP & 0xFF);
			EXIT_IF_ERR(status);
			status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTH, (PP >> 8) & 0x1F);
			EXIT_IF_ERR(status);
		}

		status = enc28_do_set_bits_ctl_reg(ctx, ENC28_CR_ECON2, (1 << ENC28_ECON2_PKTDEC));

		return status;
	}
	else if (val & (1 << ENC28_EIR_RXERIF))
	{
		status = enc28_select_register_bank(ctx, 0);
		EXIT_IF_ERR(status);

		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTL, ENC28_CONF_RX_ADDRESS_END & 0xFF);
		EXIT_IF_ERR(status);
		status = enc28_do_write_ctl_reg(ctx, ENC28_CR_ERXRDPTH, (ENC28_CONF_RX_ADDRESS_END >> 8) & 0x1F);
		EXIT_IF_ERR(status);

		status = enc28_do_clear_bits_ctl_reg(ctx, ENC28_CR_EIR, (1 << ENC28_EIR_RXERIF));
		EXIT_IF_ERR(status);

		return ENC28_PACKET_RCV_ERR;
	}
	else
	{
		return ENC28_NO_DATA;
	}
}

ENC28_CommandStatus enc28_end_packet_transfer(ENC28_SPI_Context *ctx)
{
	const uint8_t mask = (1 << ENC28_ECON1_RXEN);
	ENC28_CommandStatus status = enc28_select_register_bank(ctx, 0);
	EXIT_IF_ERR(status);
	status = enc28_do_clear_bits_ctl_reg(ctx, ENC28_CR_ECON1, mask);
	return status;
}
