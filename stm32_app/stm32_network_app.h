#ifndef STM32_NETWORK_APP_H_
#define STM32_NETWORK_APP_H_

#include "enc28j60.h"

extern void enc28_test_app_handle_packet_recv_interrupt(void);
extern void enc28_test_app(ENC28_SPI_Context *ctx);

#endif /* STM32_NETWORK_APP_H_ */
