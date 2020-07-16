#include <stdint.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "freertos/event_groups.h"
#include "rom/ets_sys.h"
// #include "esp_wifi.h"
// #include "esp_event_loop.h"
// #include "esp_err.h"
#include "esp_log.h"
// #include "driver/soc.h"
// #include "driver/i2s.h"
// #include "driver/i2c.h"
// #include "driver/gpio.h"
// #include "driver/spi.h"
#include "driver/spi_master.h"


static const char *TAG = "comm-spi";

#define TEENSY_HOST HSPI_HOST
// #define PIN_NUM_MISO 25
// #define PIN_NUM_MOSI 23
// #define PIN_NUM_CLK  19
// #define PIN_NUM_CS   22
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15
#define DMA_CHAN      2

spi_device_handle_t spi;

void comm_spi_init()
{
    ESP_LOGI(TAG, "init SPI");
    int ret;

    // spi_config_t spi_config;
    // spi_config.interface.val = SPI_DEFAULT_INTERFACE;
    // spi_config.mode = SPI_MASTER_MODE;
    // spi_config.clk_div = SPI_4MHz_DIV;
    // spi_config.event_cb = NULL;
    // spi_init(HSPI_HOST, &spi_config);

    spi_bus_config_t buscfg={
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz = 4*1000*1000,           //Clock out at 10 MHz
        .mode = 0,                                //SPI mode 0
        .spics_io_num = PIN_NUM_CS,               //CS pin
        .queue_size = 7,                          //We want to be able to queue 7 transactions at a time
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
    };
    //Initialize the SPI bus
    ret = spi_bus_initialize(TEENSY_HOST, &buscfg, DMA_CHAN);
    assert( ret == ESP_OK );

    ret = spi_bus_add_device(TEENSY_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

#define LEDS_PER_PACKET 10

// static unsigned char packetbuffer[100] = { 0, };

int comm_spi_push( uint8_t *buffer, uint16_t totalleds )
{
        uint8_t *packetbuffer = malloc(100);

    // ESP_LOGI(TAG, "send total %d leds (%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X)", totalleds,
    //     buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5],
    //     buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11]);

    // spi_transaction_t trans;

    int ret;
    int left = totalleds;
    int packetlength;
    int offset = 0;

    while(left > 0) {
        int chunksize = LEDS_PER_PACKET;

        memset(packetbuffer, 0, 100);
        packetbuffer[0] = 'R';
        packetbuffer[1] = 'G';
        packetbuffer[2] = 'B';
        packetbuffer[3] = LEDS_PER_PACKET;
        packetbuffer[4] = 0;
        packetbuffer[5] = (offset >> 8);
        packetbuffer[6] = (offset & 255);
        memcpy(packetbuffer + 7, buffer + (offset * 3), (chunksize * 3));

        packetlength = 8 * ((chunksize * 3) + 7);

        ret = spi_device_acquire_bus(spi, portMAX_DELAY);
        ESP_ERROR_CHECK(ret);

        spi_transaction_t t = {
            .tx_buffer = packetbuffer,
            .length = packetlength,
        };
        spi_device_transmit(spi, &t);
        spi_device_release_bus(spi);

        // vTaskDelay(1);
        portYIELD();


        offset += chunksize;
        left -= chunksize;
    }

    memset(packetbuffer, 0, 10);
    packetbuffer[0] = 'S';
    packetbuffer[1] = 'Y';
    packetbuffer[2] = 'N';

    ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    ESP_ERROR_CHECK(ret);
    spi_transaction_t t = {
        .tx_buffer = packetbuffer,
        .length = 10,
    };
    spi_device_transmit(spi, &t);
    spi_device_release_bus(spi);

    vTaskDelay(1 / portTICK_RATE_MS);

    free(packetbuffer);

    return 0;
}
