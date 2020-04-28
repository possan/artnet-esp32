#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

void display_init(uint8_t sda_pin, uint8_t scl_pin);
void display_update(uint8_t *pixels);


#ifdef	__cplusplus
}
#endif

#endif  // __WS2812_I2S_H__