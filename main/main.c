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
#include "ws2812_i2s.h"
#include "ws2812_bitbang.h"
#include "display.h"
#include "knob.h"

#define NUMPIXELS 2048
#define MAXPIXELS 2048
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

static void i2sTask(void* pvParameters)
{
    uint32_t lastframe = xTaskGetTickCount();
    ws2812_i2s_init();
    while(true) {
        ws2812_i2s_update();
        vTaskDelay(10);
    }
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

    pixelsused = 60;

    for(int i=0; i<pixelsused; i++) {
        pixelbuffer[i * 3 + 2] = i * 4;
    }

    dirtypixels = true;

    int debug = 1;

    int lastframecrc = 0;

    uint32_t nextupdate = 0;
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

            int crcnow = crc16_le(0, pixelbuffer, pixelsused * 3);
            if (crcnow != lastframecrc) {
                lastframecrc = crcnow;

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


                unsigned char *outptr = (unsigned char *)&pixelbuffer2;
                for(int i=0; i<pixelsused; i++) {
                    uint16_t o = pixeldmxoffset[i];
                    unsigned char *inptr = (unsigned char *)&pixelbuffer + o;
                    outptr[0] = inptr[0];
                    outptr[1] = inptr[1];
                    outptr[2] = inptr[2];
                    outptr += 3;
                }

                ws2812_i2s_setpixels(pixelbuffer2, pixelsused);
                dirtypixels = false;

                renderframes ++;
            }


            // pixelbuffer2[0] = debug ? 255 : 0;
            // pixelbuffer2[1] = debug ? 255 : 0;
            // pixelbuffer2[2] = debug ? 255 : 0;
            // debug = 1 - debug;

        }

        vTaskDelay(10);
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

    printf("got artnet: identifier \"%s\", opcode=%x, protover=%x, universe=%x, datalength=%x\n",
        h1->identifier,
        h1->opcode,
        h1->protver,
        h1->universe,
        h1->datalength);

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

    int lp = pixeloffset + h1->datalength;
    if (lp > renderlength) {
        renderlength = lp;
    }

    // mark as dirty when last frame received.
    if (h1->universe == lastuniverse) {
        // dirtypixels = true;
    }

    pixelsused = renderlength / 3;
    if (pixelsused > 60) {
        pixelsused = 60;
    }

    uint32_t T = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (dmxfirstframe == 0) {
        dmxfirstframe = T;
    }
    dmxlastframe = T;
}

static unsigned char packetbuf[10000];

static void listeningTask(void* pvParameters)
{
    struct sockaddr_in si_me;
    struct sockaddr_in si_other;
    int s, r;
    int port = 6454;
    int broadcast = 1;
    unsigned slen = sizeof(struct sockaddr);

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ESP_LOGI(TAG, "Socket returned %d", s);

    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);

    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = INADDR_ANY;

    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 10;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    r = bind(s, (struct sockaddr *)&si_me, sizeof(struct sockaddr));
    ESP_LOGI(TAG, "Bind returned %d", r);

    while(true) {
        r = recvfrom(s, packetbuf, sizeof(packetbuf)-1, 0, (struct sockaddr *)&si_other, &slen);
        if (r > 0) {
            printf("recvfrom got %d bytes of data %02X %02X %02X %02X %02X\n", r, packetbuf[0], packetbuf[1], packetbuf[2], packetbuf[3], packetbuf[4]);
            parseArtnet((unsigned char *)&packetbuf, r);
            // } else { 
            //     vTaskDelay(30);//  / portTICK_PERIOD_MS);
        }
        vTaskDelay(1);
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
        ESP_LOGI(TAG, "got ip: %s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
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
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

static char freertosstats[300] = { 0, };

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

// static void debugTask2(void* pvParameters)
// {
//     while(true) {
//         ESP_LOGI(TAG, "Pixelbuffer %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X ",
//             pixelbuffer[0], pixelbuffer[1], pixelbuffer[2],
//             pixelbuffer[3], pixelbuffer[4], pixelbuffer[5],
//             pixelbuffer[6], pixelbuffer[7], pixelbuffer[8],
//             pixelbuffer[9], pixelbuffer[10], pixelbuffer[11],
//             pixelbuffer[12], pixelbuffer[13], pixelbuffer[14],
//             pixelbuffer[15], pixelbuffer[16], pixelbuffer[17],
//             pixelbuffer[18], pixelbuffer[19], pixelbuffer[20],
//             pixelbuffer[21], pixelbuffer[22], pixelbuffer[23]);
//         vTaskDelay(32 / portTICK_RATE_MS);
//     }
// }

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    xTaskCreatePinnedToCore(&i2sTask, "i2s", 3500, NULL, 2 | portPRIVILEGE_BIT, NULL, portNUM_PROCESSORS - 1);
    xTaskCreatePinnedToCore(&renderTask, "render", 2500, NULL, 30, NULL, 0);
    xTaskCreatePinnedToCore(&debugTask, "debug", 2500, NULL, 99, NULL, 0);
    // xTaskCreatePinnedToCore(&debugTask2, "debug2", 2500, NULL, 99, NULL, 0);
}
