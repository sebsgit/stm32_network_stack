# stm32_network_stack
Integration of ENC28J60 Ethernet module into STM32 board

# STM32 Nucleo peripheral configuration

|Pin header|Pin id|Pin name|Function|
|-|-|-|-|
|CN7|16|+3.3V|Input current to the Ethernet cap|
|CN7|20|GND|Ground connection|
|CN7|35|PC2|SPI2 MISO|
|CN7|37|PC3|SPI2 MOSI|
|CN19|1|PC9|GPIO Input: interrupt pin (falling edge)|
|CN19|16|PB12|SPI2 NSS|
|CN19|25|PB10|SPI2 Clock|

 - SPI2:
	* mode:	 0, 0
	* speed:  10Mbit/s
