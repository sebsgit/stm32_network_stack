/*
 * enc28j60.h
 *
 *  Created on: Aug 30, 2024
 *      Author: sebth
 */

#ifndef INC_ENC28J60_H_
#define INC_ENC28J60_H_

#include <stdint.h>
#include <stddef.h>

#define ENC28_SPI_ARG_BITS (5)
#define ENC28_SPI_OPCODE_MASK (uint8_t)(0xFF << (ENC28_SPI_ARG_BITS))
#define ENC28_SPI_ARG_MASK (0x1F)
#define ENC28_MAKE_RCR_CMD(reg_addr) (reg_addr & ENC28_SPI_ARG_MASK)

#define ENC28_OP_RCR	(0x0)	/* Read control register */
#define ENC28_OP_RBM 	(0x1)	/* Read buffer memory */
#define ENC28_OP_WCR	(0x2)	/* Write control register */
#define ENC28_OP_WBM	(0x3)	/* Write buffer memory */
#define ENC28_OP_BFS	(0x4)	/* Bit field set */
#define ENC28_OP_BFC	(0x5)	/* Bit field clear */
#define ENC28_OP_SRC	(0x7)	/* Soft reset */

#define ENC28_CR_ECON1		(0x1F)		/* Ethernet control register 1 */
#define ENC28_ECON1_BANK_SEL(n)		(n)		/* Bank select value */
#define ENC28_ECON1_BSEL			(0x3)	/* Bank select register bit mask */
#define ENC28_ECON1_RXEN			(1 << 2)	/* Receive enable bit */
#define ENC28_ECON1_TXRTS			(1 << 3)	/* Transmit request to send */
#define ENC28_ECON1_CSUM_EN			(1 << 4)	/* DMA checksum enable */
#define ENC28_ECON1_DMA_BUSY		(1 << 5)	/* DMA busy bit */
#define ENC28_ECON1_RX_RST			(1 << 6)	/* Receive logic reset bit */
#define ENC28_ECON1_TX_RST			(1 << 7)	/* Transmit logic reset bit */

#define ENC28_CR_MACON1		(0x0)	/* MAC control register 1 */
#define ENC28_MACON1_RXEN	(0)		/* MAC receive enable bit */
#define ENC28_MACON1_RXPAUS	(2)		/* Pause Control Frame receive bit */
#define ENC28_MACON1_TXPAUS	(3)		/* Pause Control Frame transmit bit */

#define ENC28_CR_MACON3			(0x2)	/* MAC control register 3 */
#define ENC28_MACON3_TXCRCEN	(0x4)	/* Transmit CRC Enable bit */
#define ENC28_MACON3_FULLDPX	(0x0)	/* Full-Duplex Enable bit */

#define ENC28_CR_MACON4		(0x3)	/* MAC control register 4 */
#define ENC28_MACON4_DEFER	(0x4)	/* Defer Transmission Enable bit */

#define ENC28_CR_MABBIPG	(0x4)	/* Back-to-Back Inter Packet Gap register */
#define ENC28_CR_MAIPGL		(0x6)	/* Non-Back-to-Back Inter Packet Gap register, low byte */

#define ENC28_CR_MAMXFLL	(0x0A)	/* Maximum Frame Length, low byte */
#define ENC28_CR_MAMXFLH	(0x0B)	/* Maximum Frame Length, high byte */

#define ENC28_CR_MIRDL		(0x18)	/* MII register value, low byte */
#define ENC28_CR_MIRDH		(0x19)	/* MII register value, high byte */

#define ENC28_CR_MICMD		(0x12)	/* MII command register */
#define ENC28_MICMD_MIIRD	(0)		/* MII address read bit */

#define ENC28_CR_MIREGADR	(0x14)	/* MII register address */

#define ENC28_CR_MISTAT		(0x0A)	/* MII status register */
#define ENC28_MISTAT_BUSY	(0)		/* MII busy bit */

#define ENC28_CR_MAC_ADD1	(0x04)		/* MAC address byte 0 */
#define ENC28_CR_MAC_ADD2	(0x05)		/* MAC address byte 1 */
#define ENC28_CR_MAC_ADD3	(0x02)		/* MAC address byte 2 */
#define ENC28_CR_MAC_ADD4	(0x03)		/* MAC address byte 3 */
#define ENC28_CR_MAC_ADD5	(0x00)		/* MAC address byte 4 */
#define ENC28_CR_MAC_ADD6	(0x01)		/* MAC address byte 5 */

#define ENC28_CR_ERXSTL		(0x08)		/* Receive buffer address start, low byte */
#define ENC28_CR_ERXSTH		(0x09)		/* Receive buffer address start, high byte */
#define ENC28_CR_ERXNDL		(0x0A)		/* Receive buffer address end, low byte */
#define ENC28_CR_ERXNDH		(0x0B)		/* Receive buffer address end, high byte */
#define ENC28_CR_ERDPTL		(0x0C)		/* Receive read pointer address, low byte */
#define ENC28_CR_ERDPTH		(0x0D)		/* Receive read pointer address, high byte */

#define ENC28_CR_ESTAT		(0x1D)		/* Status register */
#define ENC28_ESTAT_CLKRDY	(1)			/* ESTAT clock ready bit */

#define ENC28_CR_ERXFCON	(0x18)		/* Packet filter register */
#define ENC28_ERXFCON_UNI	(1 << 7)	/* Unicast packet filter bit */
#define ENC28_ERXFCON_ANDOR	(1 << 6)	/* AND/OR filter selection bit */
#define ENC28_ERXFCON_CRC	(1 << 5)	/* Post-filter CRC check bit */
#define ENC28_ERXFCON_MULTI	(1 << 1)	/* Multicast packet filter bit */
#define ENC28_ERXFCON_BCAST	(1 << 0)	/* Broadcast packet filter bit */

/* Customization constants */
#ifndef ENC28_CONF_RX_ADDRESS_START
#define ENC28_CONF_RX_ADDRESS_START	(0x00)	/* Default start address of the packet receive buffer */
#endif

#ifndef ENC28_CONF_RX_ADDRESS_END
#define ENC28_CONF_RX_ADDRESS_END	(0x11ff)	/* Default end address of the packet receive buffer */
#endif

#ifndef ENC28_CONF_PACKET_FILTER_MASK
#define ENC28_CONF_PACKET_FILTER_MASK (ENC28_ERXFCON_UNI | ENC28_ERXFCON_MULTI | ENC28_ERXFCON_BCAST)
#endif

#ifndef ENC28_CONF_MACON3_FRAME_PAD_MASK
#define ENC28_CONF_MACON3_FRAME_PAD_MASK	(0xE0)
#endif

#ifndef ENC28_CONF_MAX_FRAME_LEN
#define ENC28_CONF_MAX_FRAME_LEN (1536)
#endif

#ifndef ENC28_CONF_MABBIPG_BITS
#define ENC28_CONF_MABBIPG_BITS (0x15)
#endif

#ifndef ENC28_CONF_MAIPGL_BITS
#define ENC28_CONF_MAIPGL_BITS (0x12)
#endif

typedef void (*ENC28_Wait_Micro)(uint32_t);

typedef enum
{
	ENC28_OK,
	ENC28_INVALID_REGISTER,
	ENC28_INVALID_PARAM
} ENC28_CommandStatus;

typedef struct
{
	void (*nss_pin_op)(uint8_t);
	void (*spi_out_op)(const uint8_t *buff, size_t len);
	void (*spi_in_op)(uint8_t *buff, size_t len);
} ENC28_SPI_Context;

typedef struct
{
	uint8_t addr[6];
} ENC28_MAC_Address;

/**
 * @brief Performs the initialisation sequence.
 * @param mac_add The MAC address to initialize the interface with
 * @param ctx The SPI communication context
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_do_init(const ENC28_MAC_Address mac_add, ENC28_SPI_Context *ctx);

/**
 *  @brief Prepares the "register read" command for the specified control register.
 *  @param out Output buffer for the command data
 *  @param reg_id Register ID to write
 *  @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_prepare_read_ctl_reg(uint8_t *out, uint8_t reg_id);

/**
 * @brief Reads the value of control register
 * @param ctx The SPI communication context
 * @param reg_id The ID of the register to read
 * @param reg_value The value of the register
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_do_read_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t *reg_value);

/**
 * @brief Prepares the "register write" command for the specified control register
 * @param out Buffer for the output command
 * @param reg_id The register ID
 * @param in The input data
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_prepare_write_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t in);

/**
 * @brief Writes the value of the control register
 * @param ctx The SPI communication context
 * @param reg_id The register ID
 * @param reg_value The value to write
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_do_write_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t reg_value);

/**
 * @brief Prepares the "Set Bits" command for the specified register
 * @param out The output buffer
 * @param reg_id The register ID
 * @param mask The bits to set in @p reg_id
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_prepare_set_bits_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t mask);

/**
 * @brief Prepares the "Clear Bits" command for the specified register
 * @param out The output buffer
 * @param reg_id The register ID
 * @param mask The bits to clear in @p reg_id
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_prepare_clear_bits_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t mask);

/**
 * @brief Sets the bits of the specified register
 * @param ctx The SPI communication context
 * @param reg_id The register ID
 * @param mask The bits to set
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_do_set_bits_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t mask);

/**
 * @brief Clears the bits of the specified register
 * @param ctx The SPI communication context
 * @param reg_id The register ID
 * @param mask The bits to clear
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_do_clear_bits_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t mask);

/**
 * @brief Selects the specified register bank
 * @param ctx The SPI communication context
 * @param bank_id The register bank ID to activate [0:3]
 * */
extern ENC28_CommandStatus enc28_select_register_bank(ENC28_SPI_Context *ctx, const uint8_t bank_id);

extern ENC28_CommandStatus enc28_do_read_phy_register(ENC28_SPI_Context *ctx, ENC28_Wait_Micro wait_func, uint8_t reg_id, uint16_t *reg_value);

#endif /* INC_ENC28J60_H_ */
