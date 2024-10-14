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
 * stm32_network_app.h
 *
 * */

#ifndef STM32_NETWORK_APP_H_
#define STM32_NETWORK_APP_H_

#include "enc28j60.h"

/* Flag to control the use of lwIP library for the IP stack */
#define USE_LWIP (1)

/* Maximum number of ethernet packets in use */
#define MAX_ETH_PACKETS 8

/* MAC address for the ENC28J60 interface, byte 0 */
#define MAC_ADDR_BYTE_0 0xDE
/* MAC address for the ENC28J60 interface, byte 1 */
#define MAC_ADDR_BYTE_1 0xAD
/* MAC address for the ENC28J60 interface, byte 2 */
#define MAC_ADDR_BYTE_2 0xBE
/* MAC address for the ENC28J60 interface, byte 3 */
#define MAC_ADDR_BYTE_3 0xEF
/* MAC address for the ENC28J60 interface, byte 4 */
#define MAC_ADDR_BYTE_4 0xCC
/* MAC address for the ENC28J60 interface, byte 5 */
#define MAC_ADDR_BYTE_5 0xAA

/* Static IP address for the ENC28J60 interface [129.168.0.22] */
#define ENC28_IP_ADDR ((192 << 0) | (168 << 8) | (0 << 16) | (22 << 24))

/*
 * @brief Handles the interrupt from ENC28J60 module. Should be called in the GPIO interrupt handler.
 * */
extern void enc28_test_app_handle_packet_recv_interrupt(void);

/*
 * @brief Entry point for the ENC28J60 driver test application. Does not return.
 * */
extern void enc28_test_app(ENC28_SPI_Context *ctx) __attribute__ ((noreturn));

#endif /* STM32_NETWORK_APP_H_ */
