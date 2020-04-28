// from https://github.com/SuperHouse/esp-open-rtos/tree/8840eb04112d6f32e4cf4e29784c6d9d2bfca52e/extras/ws2812_i2s

/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <string.h>
// #include "ws2812_i2s.h"
// #include "i2s_dma/i2s_dma.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifdef ESP_PLATFORM
#include "soc/gpio_struct.h"
#else
#include "esp8266/gpio_struct.h"
#endif

#include "driver/gpio.h"

#define WS2812_SHORT_DELAY() for (volatile uint32_t __j = 1; __j > 0; __j--)
#define WS2812_LONG_DELAY()  for (volatile uint32_t __j = 3; __j > 0; __j--)

static const char *TAG = "ws2812_bitbang";

#define F_CPU 160000000

// 1s =     F_CPU / 1
// 1ms =    F_CPU / 1000
// 1us =    F_CPU / 1000000
// 0.1us =  F_CPU / 10000000
// 300ns = F_CPU / (1000000 * 0.3)
// 900ns = F_CPU / (1000000 * 0.9)
#define CYCLES_800_T0H  (F_CPU / 2500000) // 0.4us
#define CYCLES_800_T1H  (F_CPU / 1250000) // 0.8us
#define CYCLES_800      (F_CPU /  800000) // 1.25us per bit


// 80mhz = 12.5ns period
// (1000mhz = 1ns period)

//
// 0: 0.3us + 0.9us
// 1: 0.9us + 0.3us
//
// 0.3us = 300ns = 
// 0.9us = 900ns = 
//

#define CYCLES2_800_T0L  (F_CPU * 3 / 10000000) // 0.3us
#define CYCLES2_800_T1L  (F_CPU * 9 / 10000000) // 0.9us

#define CYCLES2_800_T0H  (F_CPU * 9 / 10000000) // 0.9us
#define CYCLES2_800_T1H  (F_CPU * 3 / 10000000) // 0.3us



// #define CYCLES2_800      (F_CPU /  800000) // 1.25us per bit

// inline uint32_t _getCycleCount()
// {
//     uint32_t ccount;
//     __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
//     return ccount;
// }

volatile static uint16_t ws2812bitbang_gpiomask;


// // from https://github.com/Makuna/NeoPixelBus/blob/e13f9599c80bc5872f614d7cc184547b11aa225c/src/internal/NeoPixelEsp.c
// void bitbang_send_pixels_800(uint8_t* pixels, uint8_t* end, uint8_t pin)
// {
//     const uint32_t pinRegister = 1 << pin;
//     uint8_t mask;
//     uint8_t subpix;
//     uint32_t cyclesStart;

//     // trigger emediately
//     cyclesStart = _getCycleCount() - CYCLES_800;
//     do {
//         subpix = *pixels++;
//         for (mask = 0x80; mask != 0; mask >>= 1)
//         {
//             // do the checks here while we are waiting on time to pass
//             uint32_t cyclesBit = ((subpix & mask)) ? CYCLES_800_T1H : CYCLES_800_T0H;
//             uint32_t cyclesNext = cyclesStart;

//             // after we have done as much work as needed for this next bit
//             // now wait for the HIGH
//             do
//             {
//                 // cache and use this count so we don't incur another 
//                 // instruction before we turn the bit high
//                 cyclesStart = _getCycleCount();
//             } while ((cyclesStart - cyclesNext) < CYCLES_800);

//             GPIO.out_w1ts = pinRegister;

//             // wait for the LOW
//             do {
//                 cyclesNext = _getCycleCount();
//             } while ((cyclesNext - cyclesStart) < cyclesBit);

//             GPIO.out_w1tc = pinRegister;
//         }
//     } while (pixels < end);
// }

// WS2812 Timings for 80mhz
// 72 + 24 cycles

inline void bitbang_send_pixels_800_2_longdelay() {
    // 72 cycles / nops - overhead

    // 0
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 10
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 20
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 30
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 40
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 50
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 60
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // less overhead
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");

    // esp32 additionals:

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");


    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
}

inline void bitbang_send_pixels_800_2_shortdelay() {
    // 24 cycles / nops - overhead

    // less overhead
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");
    // __asm__ __volatile__("nop");

    // 0
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    // 10
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");




    // esp32 additionals:
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");


    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");

    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");















}

#define NOP1 \
    __asm__ __volatile__("nop");

#define NOP10 \
    NOP1; NOP1; NOP1; NOP1; NOP1; NOP1; NOP1; NOP1; NOP1; NOP1;

#define NOP20 \
    NOP10; NOP10;

// https://learn.adafruit.com/adafruit-neopixel-uberguide/advanced-coding

inline void bitbang_send_pixels_800_2_t0h_delay() {
    // 400us
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10;

    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;

}

inline void bitbang_send_pixels_800_2_t0l_delay() {
    // 850us
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10; NOP10;

    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
}

inline void bitbang_send_pixels_800_2_t1h_delay() {
    // 800us
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10;

    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
}

inline void bitbang_send_pixels_800_2_t1l_delay() {
    // 450us
    NOP10; NOP10; NOP10; NOP10; NOP10;
    NOP10; NOP10; NOP10; NOP10;

    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
}

void bitbang_send_pixels_800_2(uint8_t* pixels, uint8_t* end, uint8_t pin)
{
    const uint32_t pinRegister = 1 << pin;
    uint8_t mask;
    uint8_t channel;

    do {
        channel = *pixels++;
        for (mask = 0x80; mask != 0; mask >>= 1)
        {
            if (channel & mask) {
                GPIO.out_w1ts = pinRegister;
                bitbang_send_pixels_800_2_t1h_delay();
                GPIO.out_w1tc = pinRegister;
                bitbang_send_pixels_800_2_t1l_delay();
            } else {
                GPIO.out_w1ts = pinRegister;
                bitbang_send_pixels_800_2_t0h_delay();
                GPIO.out_w1tc = pinRegister;
                bitbang_send_pixels_800_2_t0l_delay();
            }
        }
    } while (pixels < end);
}

void ws2812_bitbang_init() {
    // do some measurements.

    // volatile uint32_t cyclecounter1;
    // volatile uint32_t cyclecounter2;
    // volatile uint32_t cyclecountercount;

    ESP_LOGI(TAG, "Compiletime WS2812 Timings");
    ESP_LOGI(TAG, "0: %u + %u cycles", CYCLES2_800_T0H, CYCLES2_800_T0L);
    ESP_LOGI(TAG, "1: %u + %u cycles", CYCLES2_800_T1H, CYCLES2_800_T1L);

    // taskENTER_CRITICAL();

    // cyclecounter1 = _getCycleCount();
    // for(volatile int i=100; i>=0; i--) {
    //     cyclecounter2 = _getCycleCount();
    // }

    // taskEXIT_CRITICAL();

    // ESP_LOGI(TAG, "WS2812 Bitbang init");
    // ESP_LOGI(TAG, "Cycle counter 1: %u", cyclecounter1);
    // ESP_LOGI(TAG, "Cycle counter 2: %u", cyclecounter2);

    // cyclecountercount = (cyclecounter2 - cyclecounter1) / 100;

    // ESP_LOGI(TAG, "Cycles needed for measuring cycle counter: %u", cyclecountercount);



    // taskENTER_CRITICAL();

    // cyclecounter1 = _getCycleCount();
    // for(volatile int i=10000; i>=0; i--) {
    //     __asm__ __volatile__("nop");
    // }
    // cyclecounter2 = _getCycleCount();

    // taskEXIT_CRITICAL();


    // ESP_LOGI(TAG, "Nop cycle counter 1: %u", cyclecounter1);
    // ESP_LOGI(TAG, "Nop cycle counter 2: %u", cyclecounter2);


}

static portMUX_TYPE bb_mux = portMUX_INITIALIZER_UNLOCKED;

void ws2812_bitbang_update(uint8_t gpio_num, uint8_t *rgbs, size_t numpixels)
{
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
    ws2812bitbang_gpiomask = (1 << gpio_num);
    taskENTER_CRITICAL(&bb_mux);
    portDISABLE_INTERRUPTS();

    bitbang_send_pixels_800_2(rgbs, rgbs + numpixels * 3, gpio_num);

    portENABLE_INTERRUPTS();
    taskEXIT_CRITICAL(&bb_mux);
}

