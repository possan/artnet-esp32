#ifndef __WS2812_BITBANG_H__
#define __WS2812_BITBANG_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

void ws2812_bitbang_init();
void ws2812_bitbang_update(uint8_t gpio_num, uint8_t *rgbs, size_t numpixels);

#ifdef	__cplusplus
}
#endif

#endif