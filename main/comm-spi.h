#ifndef __COMM_SPI_H__
#define __COMM_SPI_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

void comm_spi_init();
int comm_spi_push( uint8_t * buffer, uint16_t totalleds );

#ifdef	__cplusplus
}
#endif

#endif  // __WS2812_I2S_H__