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
 * lwipopts.h
 *
 * Configuration file for the lwIP library.
 * */

#ifndef INC_LWIPOPTS_H_
#define INC_LWIPOPTS_H_

/* Use only raw lwIP API */
#define NO_SYS 1

/* No socket API */
#define LWIP_SOCKET 0

/* No netconn support */
#define LWIP_NETCONN 0

/* Disable OS layer inter-task protection feature */
#define SYS_LIGHTWEIGHT_PROT 0

/* Enable timers support */
#define LWIP_TIMERS 1

/* User data in the pbuf structure */
#define LWIP_PBUF_CUSTOM_DATA void *enc28_eth_packet_ptr;

#endif /* INC_LWIPOPTS_H_ */
