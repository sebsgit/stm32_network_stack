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
#define ENC28_CR_MAC_ADD1	(0x04)		/* MAC address byte 0 */
#define ENC28_CR_MAC_ADD2	(0x05)		/* MAC address byte 1 */
#define ENC28_CR_MAC_ADD3	(0x02)		/* MAC address byte 2 */
#define ENC28_CR_MAC_ADD4	(0x03)		/* MAC address byte 3 */
#define ENC28_CR_MAC_ADD5	(0x00)		/* MAC address byte 4 */
#define ENC28_CR_MAC_ADD6	(0x01)		/* MAC address byte 5 */

#define ENC28_ECON1_BANK_SEL(n)		(n)		/* Bank select value */
#define ENC28_ECON1_BSEL			(0x3)	/* Bank select register bit mask */
#define ENC28_ECON1_RXEN			(1 << 2)	/* Receive enable bit */
#define ENC28_ECON1_TXRTS			(1 << 3)	/* Transmit request to send */
#define ENC28_ECON1_CSUM_EN			(1 << 4)	/* DMA checksum enable */
#define ENC28_ECON1_DMA_BUSY		(1 << 5)	/* DMA busy bit */
#define ENC28_ECON1_RX_RST			(1 << 6)	/* Receive logic reset bit */
#define ENC28_ECON1_TX_RST			(1 << 7)	/* Transmit logic reset bit */

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

/**
 * @brief Performs the initialisation sequence.
 * @return Status of the operation
 * */
extern ENC28_CommandStatus enc28_do_init(ENC28_SPI_Context *ctx);

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

extern ENC28_CommandStatus enc28_prepare_set_bits_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t mask);

extern ENC28_CommandStatus enc28_prepare_clear_bits_ctl_reg(uint16_t *out, uint8_t reg_id, uint8_t mask);

extern ENC28_CommandStatus enc28_do_set_bits_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t mask);

extern ENC28_CommandStatus enc28_do_clear_bits_ctl_reg(ENC28_SPI_Context *ctx, uint8_t reg_id, uint8_t mask);

#endif /* INC_ENC28J60_H_ */
