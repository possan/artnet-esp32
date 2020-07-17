// from https://github.com/SuperHouse/esp-open-rtos/tree/8840eb04112d6f32e4cf4e29784c6d9d2bfca52e/extras/ws2812_i2s

#include "ws2812_i2s.h"
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/rmt.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <malloc.h>

// See https://gist.github.com/nevercast/9c48505cc6c5687af59bcb4a22062795
#define T_0H (35 / 5)
#define T_1H (70 / 5)
#define T_0L (80 / 5)
#define T_1L (60 / 5)
#define T_RST (5310 / 5)
#define LED_RMT_TX_CHANNEL RMT_CHANNEL_0
#define BITS_PER_LED_CMD 24

rmt_item32_t *rmt_data = NULL;

void ws2812_rmt_init(int pin1, int pin2, int maxpixels)
{
    printf("ws2812_rmt_init pin1=%d, pin2=%d, maxpixels=%d\n", pin1, pin2, maxpixels);

    rmt_data = (rmt_item32_t*)malloc(maxpixels * BITS_PER_LED_CMD * sizeof(rmt_item32_t)); // ), MALLOC_CAP_DMA);
    memset(rmt_data, 0, maxpixels * BITS_PER_LED_CMD * sizeof(rmt_item32_t));

    rmt_config_t config;
    config.rmt_mode = RMT_MODE_TX;
    config.channel = LED_RMT_TX_CHANNEL;
    config.gpio_num = pin1;
    config.mem_block_num = 3;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = 0;
    config.clk_div = 4;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
}

// int ws2812_frame = 0;

void ws2812_rmt_send(uint8_t *pixels, int numpixels)
{
    if (rmt_data == NULL) {
        printf("rmt_data is null?!\n");
        return;
    }

    uint32_t outbit = 0;
    for (uint32_t led = 0; led < numpixels * 3; led++) {
        uint32_t bits_to_send = pixels[led];
        uint32_t mask = 1 << (8 - 1);
        for (uint32_t bit = 0; bit < 8; bit++) {
            uint32_t bit_is_set = bits_to_send & mask;
            rmt_data[outbit] = bit_is_set ?
            (rmt_item32_t){{{T_1H, 1, T_1L, 0}}} :
            (rmt_item32_t){{{T_0H, 1, T_0L, 0}}};
            outbit ++;
            mask >>= 1;
        }
    }

    rmt_data[outbit] = (rmt_item32_t){{{T_RST, 0, T_RST, 0}}};
    outbit ++;

    // printf("Writing %d RMT bits.\n", outbit);
    ESP_ERROR_CHECK(rmt_write_items(LED_RMT_TX_CHANNEL, rmt_data, outbit, false));
    ESP_ERROR_CHECK(rmt_wait_tx_done(LED_RMT_TX_CHANNEL, portMAX_DELAY));
}
