// from https://github.com/SuperHouse/esp-open-rtos/tree/8840eb04112d6f32e4cf4e29784c6d9d2bfca52e/extras/ws2812_i2s

#include "ws2812_i2s.h"
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2s.h"
#include <string.h>
#include <malloc.h>

#define SAMPLE_RATE 100000
// -250

#define I2S_NUM 0
#define BLOCKSIZE 128

static QueueHandle_t i2s_event_queue;
static SemaphoreHandle_t semaphore;
uint8_t *samples_data;
uint8_t *temp_data;
int samples_left;
int samples_offset;
static int ws2812_dummy_counter = 0;
bool ws2812_dirty = true;

const uint8_t bit8patterns[4] =
{
    0b10001000,
    0b10001110,
    0b11101000,
    0b11101110,
};

void ws2812_i2s_init()
{
    samples_data = malloc(8192);
    temp_data = malloc(1024);
    semaphore = xSemaphoreCreateMutex();

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .use_apll = true,
        .dma_buf_count = 16,
        .dma_buf_len = 64,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = -1,
        .data_out_num = 22,
        .data_in_num = -1
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 1, &i2s_event_queue);
    i2s_set_pin(I2S_NUM, &pin_config);
}

// int ws2812_frame = 0;

void ws2812_i2s_setpixels(uint8_t *pixels, int numpixels)
{
    if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdFALSE) {
        printf("Failed to get semaphore for setting pixels.\n");
        return;
    }

    memset(samples_data, 0, 8192);

    int i;
    int o = 0;

    // some stabilizing bits in the beginning
    for(i=0; i<4; i++) {
        samples_data[o++] = 0;
        samples_data[o++] = 0;
        samples_data[o++] = 0;
        samples_data[o++] = 0;
    }

    // samples_data[o++] = 0b11110000; // B
    // samples_data[o++] = 0b11100000; // A
    // samples_data[o++] = 0b11111100; // D
    // samples_data[o++] = 0b11111000; // C

    // every bit is 1.25uS, every byte is ~10uS
    for( i=0; i<numpixels; i++) {
        uint8_t r = pixels[i * 3 + 0];
        uint8_t g = pixels[i * 3 + 1];
        uint8_t b = pixels[i * 3 + 2];

        // G
        samples_data[o++] = bit8patterns[(g >> 2) & 3];
        samples_data[o++] = bit8patterns[(g >> 0) & 3];
        samples_data[o++] = bit8patterns[(g >> 6) & 3];
        samples_data[o++] = bit8patterns[(g >> 4) & 3];

        // R
        samples_data[o++] = bit8patterns[(r >> 2) & 3];
        samples_data[o++] = bit8patterns[(r >> 0) & 3];
        samples_data[o++] = bit8patterns[(r >> 6) & 3];
        samples_data[o++] = bit8patterns[(r >> 4) & 3];

        // B
        samples_data[o++] = bit8patterns[(b >> 2) & 3];
        samples_data[o++] = bit8patterns[(b >> 0) & 3];
        samples_data[o++] = bit8patterns[(b >> 6) & 3];
        samples_data[o++] = bit8patterns[(b >> 4) & 3];
    }

    // 50uS delay...
    // tmpbuf[o++] = bit8patterns[0];
    for(i=0; i<120; i++) {
        samples_data[o++] = 0;
        samples_data[o++] = 0;
        samples_data[o++] = 0;
        samples_data[o++] = 0;
    }

    samples_left = o;
    samples_offset = 0;

    // printf("Queued %d samples\n", o);

    xSemaphoreGive(semaphore);
}

#define BLOCKSIZE 1024

void ws2812_i2s_update()
{
    if (xSemaphoreTake(semaphore, 10 / portTICK_PERIOD_MS) == pdFALSE) {
        printf("Semapheore not available.\n");
        // deadlocked
        return;
    }

    memset(temp_data, 0, 1024);

    size_t left = samples_left;
    if (left > 0) {
        if (left > BLOCKSIZE) {
            left = BLOCKSIZE;
        }

        // printf("Copy from %d bytes from offset %d\n", left, samples_offset);
        memcpy(temp_data, samples_data + samples_offset, left);
    }

    int o = 0;

    size_t written = 0;
    // i2s_zero_dma_buffer(I2S_NUM);

    left = BLOCKSIZE;

    i2s_write(I2S_NUM, temp_data, left, &written, 10);

    // if (written != left) {
    // printf("Wrote %d bytes, %d bytes received (offset %d, samples left %d)\n", left, written, samples_offset, samples_left);
    // }

    samples_left -= written;

    if (samples_left <= 0 && left > 0) {
        // printf("End of buffer\n");
        samples_left = 0;
    }

    if (samples_left < 0) {
        samples_left = 0;
    }

    samples_offset += written;

    xSemaphoreGive(semaphore);
}
