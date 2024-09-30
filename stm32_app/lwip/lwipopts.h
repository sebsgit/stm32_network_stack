/*
 * lwipopts.h
 *
 *  Created on: Sep 30, 2024
 *      Author: sebth
 */

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

/* Disable timers support */
#define LWIP_TIMERS 0

#endif /* INC_LWIPOPTS_H_ */
