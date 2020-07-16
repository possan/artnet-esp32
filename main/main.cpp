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
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
// #include "comm-spi.h"
#include "display.h"
#include "knob.h"
#include "mdns.h"
#include <math.h>
#include "tinyosc.h"
#include "fx.h"

#define FASTLED_RMT_BUILTIN_DRIVER false
#define FASTLED_RMT_MAX_CHANNELS 4
#include "FastLED.h"

#define MDNS_HOSTNAME "esp-lights"
#define EXAMPLE_MDNS_INSTANCE CONFIG_MDNS_INSTANCE
#define NUMSTRIPS 4
#define STRIPLENGTH 300
#define NUMPIXELS 4096
#define MAXPIXELS 4096
#define FIRSTUNIVERSE 0
#define LASTUNIVERSE 5
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
static uint8_t dirtypixels = true;
static bool run_demo = true;
static uint32_t reset_time = 0;
static FxSettings fx = { 0, };

#define DATA_PIN1 18
#define DATA_PIN2 19
#define DATA_PIN3 21
#define DATA_PIN4 22

#define DATA_PIN5 27
#define DATA_PIN6 5
#define DATA_PIN7 17
#define DATA_PIN8 12

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
CRGB leds1[STRIPLENGTH];
CRGB leds2[STRIPLENGTH];
CRGB leds3[STRIPLENGTH];
CRGB leds4[STRIPLENGTH];
CRGB leds5[STRIPLENGTH];
CRGB leds6[STRIPLENGTH];
CRGB leds7[STRIPLENGTH];

float osc_fader1 = 0.0;
float osc_fader2 = 0.0;
float osc_fader3 = 0.0;
float osc_fader4 = 0.0;

uint32_t next_persist = 0;
uint8_t state_dirty = false;

// static void i2sTask(void* pvParameters)
// {
//     uint32_t lastframe = xTaskGetTickCount();
//     // ws2812_i2s_init();
//     comm_spi_init()
//     while(true) {
//         // ws2812_i2s_update();
//         vTaskDelay(1 / portTICK_RATE_MS);
//         // taskYIELD();
//     }
// }

void load_or_default_appstate() {
    nvs_handle_t my_handle;
    esp_err_t err;

    state_dirty = false;

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

    nvs_close(my_handle);
}


void save_appstate() {
    nvs_handle_t my_handle;
    esp_err_t err;

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

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to commit NVS");
        return;
    }

    nvs_close(my_handle);
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

    // for(int i=0; i<pixelsused; i++) {
    //     pixelbuffer[i * 3 + 2] = i * 4;
    // }

    // comm_spi_init();

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


            // pixelbuffer2[0] = debug ? 255 : 0;
            // pixelbuffer2[1] = debug ? 255 : 0;
            // pixelbuffer2[2] = debug ? 255 : 0;
            // debug = 1 - debug;

            // o = 0;
            // for(int i=0; i<pixelsused; i++) {
            //     o ++;
            //     o ++;
            //     pixelbuffer[o ++] = round(128.0f + 127.0f * sin((float)(i + renderframes) / 20.0f));
            // }

            int crcnow = crc16_le(0, pixelbuffer, pixelsused * 3);

            if (run_demo) {
                // always render if demo.
                crcnow = 42;
                lastframecrc = 0;
            }

            if (crcnow != lastframecrc) {
                lastframecrc = crcnow;

                /*
                ESP_LOGI(TAG, "Updating pixels from DMX data, frame %d", renderframes);

                ESP_LOGI(TAG, "Pixelbuffer: %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X ",
                    pixelbuffer[0], pixelbuffer[1], pixelbuffer[2],
                    pixelbuffer[3], pixelbuffer[4], pixelbuffer[5],
                    pixelbuffer[6], pixelbuffer[7], pixelbuffer[8],
                    pixelbuffer[9], pixelbuffer[10], pixelbuffer[11],
                    pixelbuffer[12], pixelbuffer[13], pixelbuffer[14],
                    pixelbuffer[15], pixelbuffer[16], pixelbuffer[17],
                    pixelbuffer[18], pixelbuffer[19], pixelbuffer[20],
                    pixelbuffer[21], pixelbuffer[22], pixelbuffer[23]);
                */

                unsigned char *outptr = (unsigned char *)&pixelbuffer2;
                for(int i=0; i<pixelsused; i++) {
                    uint16_t o = pixeldmxoffset[i];
                    unsigned char *inptr = (unsigned char *)&pixelbuffer + o;
                    outptr[0] = inptr[0];
                    outptr[1] = inptr[1];
                    outptr[2] = inptr[2];
                    outptr += 3;
                }

                if (run_demo) {
                    memset(pixelbuffer2, 0, pixelsused * 3);

                    int t0 = (int)(osc_fader1 * 40.0f);
                    int t1 = 255 - (int)(osc_fader2 * 255.0f);
                    int t2 = 255 - (int)(osc_fader3 * 255.0f);
                    int t3 = 255 - (int)(osc_fader4 * 255.0f);

                    for(int j=0; j<NUMSTRIPS; j++) {
                        for(int i=0; i<STRIPLENGTH; i++) {
                            int x = (i + j + (demoframe + t0)) % 20;
                            int o = (j * STRIPLENGTH + i) * 3;
                            if (x == 0) pixelbuffer2[o + 0] = t1;
                            if (x == 1) pixelbuffer2[o + 1] = t2;
                            if (x == 2) pixelbuffer2[o + 2] = t3;
                        }
                    }

                    demoframe ++;
                }

                // ws2812_i2s_setpixels(pixelbuffer2, pixelsused);

                for(int i=0; i<STRIPLENGTH; i++) {
                    int o = (0 * STRIPLENGTH + i) * 3;
                    leds0[i] = pixelbuffer2[o+2] + (pixelbuffer2[o+1] << 8) + (pixelbuffer2[o+0] << 16);

                    o = (1 * STRIPLENGTH + i) * 3;
                    leds1[i] = pixelbuffer2[o+2] + (pixelbuffer2[o+1] << 8) + (pixelbuffer2[o+0] << 16);
                }

                for(int i=0; i<8; i++) {
                    leds0[i] = 0;
                }
                leds0[rand() & 7] = rand() & 255;
                FastLED.show();

                // comm_spi_push(pixelbuffer2, pixelsused);
                dirtypixels = false;

                renderframes ++;
            }


            // pixelbuffer2[0] = debug ? 255 : 0;
            // pixelbuffer2[1] = debug ? 255 : 0;
            // pixelbuffer2[2] = debug ? 255 : 0;
            // debug = 1 - debug;

        }

        vTaskDelay(8 / portTICK_RATE_MS);
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

    run_demo = false;

    dmxframesreceived += 1;
    dmxbytesreceived += len;

    // check version?

    // grab pixels and map from RGB to GRB

    uint16_t pixeloffset = (h1->universe - FIRSTUNIVERSE) * 512;
    unsigned char *inptr = buffer + sizeof(struct ArtnetDmxHeader);
    unsigned char *outptr = (unsigned char *)&pixelbuffer + pixeloffset;
    memcpy(outptr, inptr, h1->datalength);

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

    state_dirty = true;
    next_persist = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000;

    if (strcmp(addr, "/1/fader1") == 0) {
        tosc_getFormat(osc);
        osc_fader1 = tosc_getNextFloat(osc);
        printf("Fader 1: %f\n", osc_fader1);
    }

    else if (strcmp(addr, "/1/fader2") == 0) {
        tosc_getFormat(osc);
        osc_fader2 = tosc_getNextFloat(osc);
        printf("Fader 2: %f\n", osc_fader2);
    }

    else if (strcmp(addr, "/1/fader3") == 0) {
        tosc_getFormat(osc);
        osc_fader3 = tosc_getNextFloat(osc);
        printf("Fader 3: %f\n", osc_fader3);
    }

    else if (strcmp(addr, "/1/fader4") == 0) {
        tosc_getFormat(osc);
        osc_fader4 = tosc_getNextFloat(osc);
        printf("Fader 4: %f\n", osc_fader4);
    }

    else if (strcmp(addr, "/1/toggle1") == 0) {
        tosc_getFormat(osc);
        run_demo = tosc_getNextInt32(osc);
        printf("Toggle Demo: %d\n", run_demo);
    }

    else {
        printf("Unhandled OSC: ");
        tosc_printMessage(osc);
    }
}

static void parseOsc(unsigned char *buffer, int len) {
    if (tosc_isBundle((char *)buffer)) {
        tosc_bundle bundle;
        tosc_parseBundle(&bundle, (char *)buffer, len);
        const uint64_t timetag = tosc_getTimetag(&bundle);
        tosc_message osc;
        while (tosc_getNextMessage(&bundle, &osc)) {
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
    read_timeout.tv_usec = 10;
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

        slen  = sizeof(struct sockaddr);
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
        ESP_LOGI(TAG, "Rendered frames: %u, %d/%d pixels length received, blit time %d ms, frame time %d ms", renderframes, renderlength, NUMPIXELS, rendertime, frametime);
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

    FastLED.addLeds<WS2812, DATA_PIN1, GRB>(leds0, STRIPLENGTH);
    FastLED.addLeds<WS2812, DATA_PIN2, GRB>(leds1, STRIPLENGTH);

    xTaskCreatePinnedToCore(&renderTask, "render", 4000, NULL, 30, NULL, 0);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();


    // initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    // set mDNS hostname (required if you want to advertise services)

    char* hostname = generate_hostname();
    ESP_LOGI(TAG, "mdns hostname: [%s]", hostname);
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    // //set default mDNS instance name
    // ESP_ERROR_CHECK( mdns_instance_name_set("ESP-Lights") );

    // //structure with TXT records
    mdns_txt_item_t serviceTxtData2[3] = {
        // {"board", "esp32"},
    };

    ESP_ERROR_CHECK( mdns_service_add(hostname, "_osc", "_udp", 9000, serviceTxtData2, 0) );
    free(hostname);

    // mdns_txt_item_t serviceTxtData[3] = {
    //     {"board", "esp32"},
    // };
    // // // //initialize service
    // ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 1) );
    // //add another TXT item
    // ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "path", "/foobar") );
    // //change TXT item value
    // ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "u", "admin") );

    // xTaskCreatePinnedToCore(&i2sTask, "i2s", 3500, NULL, 2 | portPRIVILEGE_BIT, NULL, portNUM_PROCESSORS - 1);
    xTaskCreatePinnedToCore(&debugTask, "debug", 2500, NULL, 99, NULL, 0);
    xTaskCreatePinnedToCore(&knobTask, "knob", 2500, NULL, 99, NULL, 0);
    xTaskCreatePinnedToCore(&displayTask, "display", 2500, NULL, 99, NULL, 0);
}
