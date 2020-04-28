#ifndef __KNOB_H__
#define __KNOB_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

void knob_init(uint8_t pin_a, uint8_t pin_b, uint8_t pin_c);
uint8_t knob_read();

#ifdef	__cplusplus
}
#endif

#endif  // __WS2812_I2S_H__