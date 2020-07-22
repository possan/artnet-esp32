/* Simple WiFi Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "rom/ets_sys.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_crc.h"
#include "esp_log.h"
#include <esp_system.h>
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_eth.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "display.h"
#include "knob.h"
#include "mdns.h"
#include <math.h>
#include "tinyosc.h"
#include "fx.h"
#include <esp_http_server.h>
#include <algorithm> 

#define FASTLED_RMT_BUILTIN_DRIVER false
#define FASTLED_RMT_MAX_CHANNELS 1
#include "FastLED.h"

#define MDNS_HOSTNAME "esp-lights"
#define EXAMPLE_MDNS_INSTANCE CONFIG_MDNS_INSTANCE
#define NUMSTRIPS 1
#define STRIPLENGTH 384
#define MAXPIXELS 512
#define FIRSTUNIVERSE 0
#define LASTUNIVERSE 3
#define LED_GPIO 5
#define EXAMPLE_ESP_WIFI_MODE_AP CONFIG_ESP_WIFI_MODE_AP
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN CONFIG_MAX_STA_CONN

static EventGroupHandle_t wifi_event_group;
static uint32_t dmxframesreceived = 0;
static uint32_t dmxfirstframe = 0;
static uint32_t dmxlastframe = 0;
static uint32_t dmxbytesreceived = 0;
static uint32_t renderframes = 0;
static int renderlength = 0;
static int rendertime = 0;
static int frametime = 0;
static int pixelsused = 0;
static int lastuniverse = 0;
const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG = "esp-artnet";
static uint16_t pixeldmxoffset[MAXPIXELS] = { 0, };
static uint8_t pixelbuffer[MAXPIXELS * 3] = { 0, };
static uint8_t pixelbuffer2[MAXPIXELS * 3] = { 0, };
static uint8_t tempbuffer[MAXPIXELS] = { 0, };
static uint8_t dirtypixels = true;
static bool run_demo = true;
static uint32_t reset_time = 0;
static FxSettings fx = { 0, };
static char device_id[10] = { 0, };

#define DATA_PIN1 18
#define DATA_PIN2 19
// #define DATA_PIN3 21
// #define DATA_PIN4 22
// #define DATA_PIN5 27
// #define DATA_PIN6 5
// #define DATA_PIN7 17
// #define DATA_PIN8 12
// #define DATA_PIN0  21
// #define DATA_PIN1  22
// #define DATA_PIN2  19
// #define DATA_PIN3   5
// #define DATA_PIN4  17
// #define DATA_PIN5   0
// #define DATA_PIN6   2
// #define DATA_PIN7  12

#define STORAGE_NAMESPACE "state"

// CRGB leds[4][STRIPLENGTH];
CRGB leds0[STRIPLENGTH];
// CRGB leds1[STRIPLENGTH];
// CRGB leds2[STRIPLENGTH];
// CRGB leds3[STRIPLENGTH];
// CRGB leds4[STRIPLENGTH];
// CRGB leds5[STRIPLENGTH];
// CRGB leds6[STRIPLENGTH];
// CRGB leds7[STRIPLENGTH];

float osc_fader1 = 0.0;
float osc_fader2 = 0.0;
float osc_fader3 = 0.0;
float osc_fader4 = 0.0;

uint32_t next_persist = 0;
uint8_t state_dirty = false;

void load_or_default_appstate() {
    nvs_handle_t my_handle;
    esp_err_t err;

    state_dirty = false;

    // default fx prameters

    memset(&fx, 0, sizeof(FxSettings));
    fx.num_leds = 100;
    fx.opacity = 100;

    fx.layer[0].offset = 0;
    fx.layer[0].opacity = 100;
    fx.layer[0].color[0] = 255;
    fx.layer[0].color[1] = 0;
    fx.layer[0].color[2] = 0;
    fx.layer[0].size = 3;
    fx.layer[0].repeat = 1;
    fx.layer[0].feather_left = 3;
    fx.layer[0].feather_right = 10;
    fx.layer[0].speed_multiplier = 1024;

    fx.layer[1].offset = 0;
    fx.layer[1].opacity = 100;
    fx.layer[1].color[0] = 0;
    fx.layer[1].color[1] = 0;
    fx.layer[1].color[2] = 255;
    fx.layer[1].size = 4;
    fx.layer[1].repeat = 1;
    fx.layer[1].feather_left = 10;
    fx.layer[1].feather_right = 10;
    fx.layer[1].speed_multiplier = 768;

    strcpy(device_id, "not-set");

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to open NVS");
        return;
    }

    // Read restart counter
    FxSettings tmp = { 0, };
    size_t required_size = 0;
    err = nvs_get_blob(my_handle, "state", NULL, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Stored blob is %d bytes, our structure is %d bytes", required_size, sizeof(FxSettings));
        required_size = sizeof(FxSettings);
        err = nvs_get_blob(my_handle, "state", &tmp, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded stored blob");
            memcpy(&fx, &tmp, sizeof(FxSettings));
        }
    }

    required_size = 0;
    err = nvs_get_blob(my_handle, "device_id", NULL, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Stored blob is %d bytes, our structure is %d bytes", required_size, 10);
        required_size = 10;
        err = nvs_get_blob(my_handle, "device_id", &device_id, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded stored blob");
        }
    }

    nvs_close(my_handle);
}


void save_appstate() {
    nvs_handle_t my_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Persisting app state NVS");

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to commit NVS");
        return;
    }

    ESP_LOGI(TAG, "Saving blob (%d bytes)", sizeof(FxSettings));
    err = nvs_set_blob(my_handle, "state", &fx, sizeof(FxSettings));
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to write blob to NVS");
        return;
    }

    ESP_LOGI(TAG, "Saving blob (%d bytes)", 10);
    err = nvs_set_blob(my_handle, "device_id", &device_id, 10);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to write blob to NVS");
        return;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to commit NVS");
        return;
    }

    nvs_close(my_handle);
}











static char* generate_hostname(void)
{
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", MDNS_HOSTNAME, mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
}







/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    char resp[200];
    const char *hostname = generate_hostname();
    sprintf(resp, "<h1>ESP LIGHT SERVER</h1><p>Hostname %s</p><p>ID %s</p>", hostname, device_id);
    httpd_resp_send(req, resp, strlen(resp));
    delete(hostname);
    return ESP_OK;
}

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_config_handler(httpd_req_t *req)
{
    /* Send a simple response */
    char resp[4000];
    memset(resp, 0, 4000);
    fx_get_config_json(&fx, (char *)&resp, 4000);
    httpd_resp_set_hdr(req, "Content-Type", "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = std::min(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }


    memset(device_id, 0, 10);
    memcpy(device_id, content, recv_size);

    printf("Set device id \"%s\"\n", device_id);

    state_dirty = true;
    next_persist = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 2000;

    /* Send a simple response */
    const char resp[] = "OK";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_prop_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = std::min(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    content[recv_size] = 0;

    char prop[100];
    // skip "/prop/" part of uri
    strcpy(prop, req->uri + 5);

    uint32_t value = atoi(content);

    printf("Set property \"%s\" to %d\n", prop, value);

    if (!fx_set_osc_property(&fx, prop, value)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    state_dirty = true;
    next_persist = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 2000;

    /* Send a simple response */
    const char resp[] = "OK";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_config = {
    .uri      = "/config",
    .method   = HTTP_GET,
    .handler  = get_config_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_post = {
    .uri      = "/id",
    .method   = HTTP_POST,
    .handler  = post_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_post_prop = {
    .uri      = "/prop/*?",
    .method   = HTTP_POST,
    .handler  = post_prop_handler,
    .user_ctx = NULL
};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_get_config);
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_uri_handler(server, &uri_post_prop);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}












uint32_t idiot_crc(const uint8_t* buf, uint32_t len)
{
    uint32_t i, crc = 0;

    for (i = 0; i < len; i++) {
        crc <<= 1;
        crc &= 0xFFFFFF;
        crc += buf[i] & 0xff;
    }

    return crc;
}


static void renderTask(void* pvParameters)
{
    int ledsperpage = 170; // 512 / 3
    int page = 0;
    int o = 0;
    for(int i=0; i<MAXPIXELS; i++) {
        int page = i / ledsperpage;
        int inpage = i % ledsperpage;
        pixeldmxoffset[i] = (512 * page) + (3 * inpage);
    }

    pixelsused = STRIPLENGTH * NUMSTRIPS;

    memset(pixelbuffer, 0, MAXPIXELS * 3);
    memset(pixelbuffer2, 0, MAXPIXELS * 3);

    for(int i=0; i<1; i++) {
        leds0[0 + i] = 0x0000FF;
        leds0[5 + i] = 0x00FF00;
        leds0[10 + i] = 0xFF0000;
        leds0[15 + i] = 0x0000FF;
        leds0[20 + i] = 0x00FF00;
        leds0[25 + i] = 0xFF0000;
    }

    FastLED.show();
    vTaskDelay(1000 / portTICK_RATE_MS);

    FastLED.show();
    vTaskDelay(1000 / portTICK_RATE_MS);

    FastLED.show();
    vTaskDelay(2000 / portTICK_RATE_MS);

    // ws2812_rmt_init(DATA_PIN2, DATA_PIN1, MAXPIXELS);

    // o = 0;
    // for(int i=0; i<10; i++) {
    //     pixelbuffer2[0 + o] = 255;
    //     pixelbuffer2[31 + o] = 255;
    //     pixelbuffer2[62 + o] = 255;
    //     o += 3;
    // }

    // ws2812_rmt_send(pixelbuffer2, pixelsused);


    dirtypixels = true;

    int debug = 1;

    int lastframecrc = 0;

    uint32_t nextupdate = 0;
    uint32_t demoframe = 0;
    while(true) {
        uint32_t T = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (T > nextupdate) {
            dirtypixels = true;
            nextupdate = T + 16;
        }

        if (dirtypixels) {
            uint32_t T2 = T - reset_time;

            if (run_demo) {
                fx_render(&fx, T2, (uint8_t*)&pixelbuffer2, pixelsused, (uint8_t*)&tempbuffer);
            } else {
                unsigned char *outptr = (unsigned char *)&pixelbuffer2;
                for(int i=0; i<pixelsused; i++) {
                    uint16_t o = pixeldmxoffset[i];
                    unsigned char *inptr = (unsigned char *)&pixelbuffer + o;
                    outptr[0] = inptr[0];
                    outptr[1] = inptr[1];
                    outptr[2] = inptr[2];
                    outptr += 3;
                }
            }

            int crcnow = idiot_crc(pixelbuffer2, pixelsused * 3);

            // if (run_demo) {
            //     // always render if demo.
                crcnow = 42;
                lastframecrc = 0;
            // }
            
            if (crcnow != lastframecrc) {
                lastframecrc = crcnow;

                // uint8_t *pb2ptr = pixelbuffer2;
                // pb2ptr[0] = rand() & 255;
                // pb2ptr[1] = 0;//rand() & 255;
                // pb2ptr[2] = 0;//rand() & 255;

                // pb2ptr[30] = 0;//rand() & 255;
                // pb2ptr[31] = rand() & 255;
                // pb2ptr[32] = 0;// rand() & 255;

                // pb2ptr[60] = 0;// rand() & 255;
                // pb2ptr[61] = 0;// rand() & 255;
                // pb2ptr[62] = rand() & 255;

                // ESP_LOGI(TAG, "Updating pixels from DMX data, frame %d", renderframes);

                // ESP_LOGI(TAG, "Pixelbuffer: %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X ",
                //     pixelbuffer2[0], pixelbuffer2[1], pixelbuffer2[2],
                //     pixelbuffer2[3], pixelbuffer2[4], pixelbuffer2[5],
                //     pixelbuffer2[6], pixelbuffer2[7], pixelbuffer2[8],
                //     pixelbuffer2[9], pixelbuffer2[10], pixelbuffer2[11],
                //     pixelbuffer2[12], pixelbuffer2[13], pixelbuffer2[14],
                //     pixelbuffer2[15], pixelbuffer2[16], pixelbuffer2[17],
                //     pixelbuffer2[18], pixelbuffer2[19], pixelbuffer2[20],
                //     pixelbuffer2[21], pixelbuffer2[22], pixelbuffer2[23]);

                // if (run_demo) {
                //     memset(pixelbuffer2, 0, pixelsused * 3);

                //     int t0 = (int)(osc_fader1 * 40.0f);
                //     int t1 = 255 - (int)(osc_fader2 * 255.0f);
                //     int t2 = 255 - (int)(osc_fader3 * 255.0f);
                //     int t3 = 255 - (int)(osc_fader4 * 255.0f);

                //     for(int j=0; j<NUMSTRIPS; j++) {
                //         for(int i=0; i<STRIPLENGTH; i++) {
                //             int x = (i + j + (demoframe + t0)) % 20;
                //             int o = (j * STRIPLENGTH + i) * 3;
                //             if (x == 0) pixelbuffer2[o + 0] = t1;
                //             if (x == 1) pixelbuffer2[o + 1] = t2;
                //             if (x == 2) pixelbuffer2[o + 2] = t3;
                //         }
                //     }

                //     demoframe ++;
                // }

                // ws2812_i2s_setpixels(pixelbuffer2, pixelrun_demosused);

                // ws2812_rmt_send(pixelbuffer2, pixelsused);

                for(int i=0; i<STRIPLENGTH; i++) {
                    int o = (0 * STRIPLENGTH + i) * 3;

                    uint8_t _r = pixelbuffer2[o+0];
                    uint8_t _g = pixelbuffer2[o+1];
                    uint8_t _b = pixelbuffer2[o+2];

                    if (fx.pixel_order == 1) {
                        // RBG
                        leds0[i] = _r + (_b << 8) + (_g << 16);

                    } else if (fx.pixel_order == 2) {
                        // BGR
                        leds0[i] = _b + (_g << 8) + (_r << 16);

                    } else if (fx.pixel_order == 3) {
                        // BRG
                        leds0[i] = _b + (_r << 8) + (_g << 16);

                    } else if (fx.pixel_order == 4) {
                        // GBR
                        leds0[i] = _g + (_b << 8) + (_r << 16);

                    } else if (fx.pixel_order == 5) {
                        // GRB
                        leds0[i] = _g + (_r << 8) + (_b << 16);

                    } else {
                        // RGB
                        leds0[i] = _r + (_g << 8) + (_b << 16);
                    }
                }

                FastLED.show();

                // comm_spi_push(pixelbuffer2, pixelsused);
                dirtypixels = false;

                renderframes ++;
            }
        }

        vTaskDelay(5 / portTICK_RATE_MS);
    }
}

struct ArtnetDmxHeader {
    char identifier[8];
    unsigned short opcode;
    unsigned short protver;
    unsigned char sequence;
    unsigned char physical;
    unsigned char universe;
    unsigned char universe2;
    unsigned short datalength;
};

static void parseArtnet(unsigned char *buffer, int len) {
    if (len < 20) {
        return;
    }

    struct ArtnetDmxHeader *h1 = (struct ArtnetDmxHeader *)buffer;

    if (memcmp(h1->identifier, "Art-Net", 7) != 0) {
        return;
    }

    h1->opcode = htons(h1->opcode);
    h1->protver = htons(h1->protver);
    h1->datalength = htons(h1->datalength);

    // printf("got artnet: identifier \"%s\", opcode=%x, protover=%x, universe=%x, datalength=%x\n",
    //     h1->identifier,
    //     h1->opcode,
    //     h1->protver,
    //     h1->universe,
    //     h1->datalength);

    if (h1->opcode != 0x50) {
        // not dmx
        return;
    }

    if (h1->universe < FIRSTUNIVERSE) {
        // wrong universe, pal!
        return;
    }

    if (h1->universe > LASTUNIVERSE) {
        // wrong universe, pal!
        return;
    }

    if (h1->universe > lastuniverse) {
        lastuniverse = h1->universe;
    }

    dmxframesreceived += 1;
    dmxbytesreceived += len;

    // check version?

    // grab pixels and map from RGB to GRB

    uint16_t pixeloffset = (h1->universe - FIRSTUNIVERSE) * 512;
    unsigned char *inptr = buffer + sizeof(struct ArtnetDmxHeader);
    unsigned char *outptr = (unsigned char *)&pixelbuffer + pixeloffset;
    memcpy(outptr, inptr, h1->datalength);

    // printf("x\n");

    run_demo = false;

    int lp = pixeloffset + h1->datalength;
    if (lp > renderlength) {
        renderlength = lp;
    }

    // mark as dirty when last frame received.
    if (h1->universe == lastuniverse) {
        dirtypixels = true;
    }

    pixelsused = renderlength / 3;
    if (pixelsused > (NUMSTRIPS * STRIPLENGTH)) {
        pixelsused = (NUMSTRIPS * STRIPLENGTH);
    }

    uint32_t T = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (dmxfirstframe == 0) {
        dmxfirstframe = T;
    }
    dmxlastframe = T;
}

static void handleOsc(tosc_message *osc) {
    // tosc_printMessage(osc);
    // tosc_reset(osc);

    // [20 bytes] /1/fader1 f 0.244624
    // sendosc 192.168.1.180 9000 /1/fader1 f 0.5

    const char *addr = tosc_getAddress(osc);
    tosc_getFormat(osc);

    if (strcmp(addr, "/sync") == 0) {
        reset_time = (xTaskGetTickCount() * portTICK_PERIOD_MS);
        return;
    }

    float v = tosc_getNextFloat(osc);
    if (!fx_set_osc_property(&fx, (char *)addr, v)) {
        printf("Unhandled OSC: ");
        tosc_printMessage(osc);
    } else {
        state_dirty = true;
        next_persist = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000;
    }

    // if (strcmp(addr, "/1/fader1") == 0) {
    //     tosc_getFormat(osc);
    //     osc_fader1 = tosc_getNextFloat(osc);
    //     printf("Fader 1: %f\n", osc_fader1);
    // }

    // else if (strcmp(addr, "/1/fader2") == 0) {
    //     tosc_getFormat(osc);
    //     osc_fader2 = tosc_getNextFloat(osc);
    //     printf("Fader 2: %f\n", osc_fader2);
    // }

    // else if (strcmp(addr, "/1/fader3") == 0) {
    //     tosc_getFormat(osc);
    //     osc_fader3 = tosc_getNextFloat(osc);
    //     printf("Fader 3: %f\n", osc_fader3);
    // }

    // else if (strcmp(addr, "/1/fader4") == 0) {
    //     tosc_getFormat(osc);
    //     osc_fader4 = tosc_getNextFloat(osc);
    //     printf("Fader 4: %f\n", osc_fader4);
    // }

    // else if (strcmp(addr, "/1/toggle1") == 0) {
    //     tosc_getFormat(osc);
    //     run_demo = tosc_getNextInt32(osc);
    //     printf("Toggle Demo: %d\n", run_demo);
    // }

    // else {
    //     printf("Unhandled OSC: ");
    //     tosc_printMessage(osc);
    // }
}

static void parseOsc(unsigned char *buffer, int len)
{
    if (tosc_isBundle((char *)buffer))
    {
        tosc_bundle bundle;
        tosc_parseBundle(&bundle, (char *)buffer, len);
        const uint64_t timetag = tosc_getTimetag(&bundle);
        tosc_message osc;
        while (tosc_getNextMessage(&bundle, &osc))
        {
            handleOsc(&osc);
        }
    } else {
        tosc_message osc;
        tosc_parseMessage(&osc, (char *)buffer, len);
        handleOsc(&osc);
    }
}

static unsigned char packetbuf[10000];

static void listeningTask(void* pvParameters)
{
    struct sockaddr_in si_me;
    struct sockaddr_in si_me2;
    struct sockaddr_in si_other;
    int s, s2, r;
    int port = 6454;
    int port2 = 9000;
    unsigned slen;

    start_webserver();

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ESP_LOGI(TAG, "Socket returned %d", s);

    s2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ESP_LOGI(TAG, "Socket returned %d", s2);

    // int broadcast = 1;
    // setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);
    // setsockopt(s2, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);

    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = INADDR_ANY;

    r = bind(s, (struct sockaddr *)&si_me, sizeof(struct sockaddr));
    ESP_LOGI(TAG, "Bind returned %d", r);

    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 1;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    memset(&si_me2, 0, sizeof(si_me2));
    si_me2.sin_family = AF_INET;
    si_me2.sin_port = htons(port2);
    si_me2.sin_addr.s_addr = INADDR_ANY;

    r = bind(s2, (struct sockaddr *)&si_me2, sizeof(struct sockaddr));
    ESP_LOGI(TAG, "Bind returned %d", r);

    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 2;
    setsockopt(s2, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    fcntl(s, F_SETFL, O_NONBLOCK);
    fcntl(s2, F_SETFL, O_NONBLOCK);

    while(true) {
        bool any = false;

        slen = sizeof(struct sockaddr);
        r = recvfrom(s2, packetbuf, sizeof(packetbuf)-1, MSG_DONTWAIT, (struct sockaddr *)&si_other, &slen);
        if (r > 0) {
            // printf("recvfrom got %d bytes of data %02X %02X %02X %02X %02X, OSC?\n", r, packetbuf[0], packetbuf[1], packetbuf[2], packetbuf[3], packetbuf[4]);
            parseOsc((unsigned char *)&packetbuf, r);
            any = true;
        }

        slen = sizeof(struct sockaddr);
        r = recvfrom(s, packetbuf, sizeof(packetbuf)-1, MSG_DONTWAIT, (struct sockaddr *)&si_other, &slen);
        if (r > 0) {
            // printf("recvfrom got %d bytes of data %02X %02X %02X %02X %02X, ArtNet?\n", r, packetbuf[0], packetbuf[1], packetbuf[2], packetbuf[3], packetbuf[4]);
            parseArtnet((unsigned char *)&packetbuf, r);
            any = true;
        }

        if (state_dirty) {
            uint32_t T = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (T > next_persist) {
                save_appstate();
                state_dirty = false;
            }
        }

        vTaskDelay(3); //  / portTICK_RATE_MS);
    }
}

static void startListening() {
    xTaskCreatePinnedToCore(&listeningTask, "listening", 2000, NULL, 3, NULL, 0);
}

static void stopListening() {
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip: %s", ip4addr_ntoa((const ip4_addr*)&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        startListening();
        break;
    // case SYSTEM_EVENT_AP_STACONNECTED:
    //     ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
    //              MAC2STR(event->event_info.sta_connected.mac),
    //              event->event_info.sta_connected.aid);
    //     break;
    // case SYSTEM_EVENT_AP_STADISCONNECTED:
    //     ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
    //              MAC2STR(event->event_info.sta_disconnected.mac),
    //              event->event_info.sta_disconnected.aid);
    //     break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        stopListening();
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta()
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)&wifi_config.sta.ssid, (const char *)EXAMPLE_ESP_WIFI_SSID);
    strcpy((char *)&wifi_config.sta.password, (const char *)EXAMPLE_ESP_WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

static char freertosstats[300] = { 0, };
static uint8_t displaydata[1024] = { 0, };

static void debugTask(void* pvParameters)
{
    while(true) {
        uint32_t dmxduration = dmxlastframe - dmxfirstframe;
        float dmxframerate = 0;
        if (dmxduration > 1000) {
            dmxframerate = (float)dmxframesreceived / ((float)dmxduration / 1000.0f);
        }

        // vTaskGetRunTimeStats((char *)&freertosstats);
        // ESP_LOGI(TAG, "FreeRTOS Stats:\n%s", freertosstats);
        ESP_LOGI(TAG, "Demo mode: %d - Faders: %.1f %.1f %.1f %.1f", run_demo, osc_fader1, osc_fader2, osc_fader3, osc_fader4);
        ESP_LOGI(TAG, "Artnet stats: %u dmx frames received (%u bytes) over %d ms (~%1.1f fps).", dmxframesreceived, dmxbytesreceived, dmxduration, dmxframerate);
        ESP_LOGI(TAG, "Rendered frames: %u, %d/%d pixels length received, blit time %d ms, frame time %d ms", renderframes, renderlength, MAXPIXELS, rendertime, frametime);
        // ESP_LOGI(TAG, "Pixelbuffer %02X%02X%02X %02X%02X%02X %02X%02X%02X",
        //     pixelbuffer[0], pixelbuffer[1], pixelbuffer[2],
        //     pixelbuffer[3], pixelbuffer[4], pixelbuffer[5],
        //     pixelbuffer[6], pixelbuffer[7], pixelbuffer[8]);
        // printf("\n\n\n\n");

        taskYIELD();
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

static void displayTask(void* pvParameters)
{
    display_init(4, 5);

    while(true) {
        // vTaskGetRunTimeStats((char *)&freertosstats);
        // ESP_LOGI(TAG, "FreeRTOS Stats:\n%s", freertosstats);
        // ESP_LOGI(TAG, "Artnet stats: %u dmx frames received (%u bytes) over %d ms (~%1.1f fps).", dmxframesreceived, dmxbytesreceived, dmxduration, dmxframerate);
        // ESP_LOGI(TAG, "Rendered frames: %u, %d/%d pixels length received, blit time %d ms, frame time %d ms", renderframes, renderlength, NUMPIXELS, rendertime, frametime);
        // ESP_LOGI(TAG, "Pixelbuffer %02X%02X%02X %02X%02X%02X %02X%02X%02X",
        //     pixelbuffer[0], pixelbuffer[1], pixelbuffer[2],
        //     pixelbuffer[3], pixelbuffer[4], pixelbuffer[5],
        //     pixelbuffer[6], pixelbuffer[7], pixelbuffer[8]);
        // printf("\n\n\n\n");

        for(int i=0; i<1024; i++) {
            displaydata[i] = 255;// 1 << (rand() & 7);
        }

        // for(int i=0; i<16; i++) {
        //     displaydata[32 + i] = clickcounter;
        //     displaydata[64 + i] = turncounter;
        // }

        display_update((uint8_t*)&displaydata);

        taskYIELD();
        vTaskDelay(30 / portTICK_RATE_MS);
    }
}

static void knobTask(void* pvParameters)
{
    knob_init(17, 18, 19);

    while(true) {
        // ESP_LOGI(TAG, "Pixelbuffer %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X ",
        //     pixelbuffer[0], pixelbuffer[1], pixelbuffer[2],
        //     pixelbuffer[3], pixelbuffer[4], pixelbuffer[5],
        //     pixelbuffer[6], pixelbuffer[7], pixelbuffer[8],
        //     pixelbuffer[9], pixelbuffer[10], pixelbuffer[11],
        //     pixelbuffer[12], pixelbuffer[13], pixelbuffer[14],
        //     pixelbuffer[15], pixelbuffer[16], pixelbuffer[17],
        //     pixelbuffer[18], pixelbuffer[19], pixelbuffer[20],
        //     pixelbuffer[21], pixelbuffer[22], pixelbuffer[23]);

        knob_read();

        taskYIELD();
        // vTaskDelay(5 / portTICK_RATE_MS);
    }
}










extern "C" {
    void app_main();
}


void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_or_default_appstate();

    FastLED.addLeds<WS2812B, DATA_PIN2, GRB>(leds0, STRIPLENGTH);
    // FastLED.addLeds<WS2812, DATA_PIN2, GRB>(leds1, STRIPLENGTH);

    xTaskCreatePinnedToCore(&renderTask, "render", 4000, NULL, 30, NULL, portNUM_PROCESSORS - 1);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();


    // initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    // set mDNS hostname (required if you want to advertise services)

    char* hostname = generate_hostname();
    ESP_LOGI(TAG, "mdns hostname: [%s]", hostname);
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_ERROR_CHECK( mdns_instance_name_set("ESP-Lights") );

    // //structure with TXT records
    mdns_txt_item_t serviceTxtData2[3] = {
        {"board", "esp32"},
    };

    ESP_ERROR_CHECK( mdns_service_add(hostname, "_osc", "_udp", 9000, serviceTxtData2, 1) );
    free(hostname);

    // xTaskCreatePinnedToCore(&i2sTask, "i2s", 3500, NULL, 2 | portPRIVILEGE_BIT, NULL, portNUM_PROCESSORS - 1);
    xTaskCreatePinnedToCore(&debugTask, "debug", 2500, NULL, 99, NULL, 0);
    // xTaskCreatePinnedToCore(&knobTask, "knob", 2500, NULL, 99, NULL, 0);
    // xTaskCreatePinnedToCore(&displayTask, "display", 2500, NULL, 99, NULL, 0);
}
