#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "freertos/queue.h"
#include "driver/gpio.h"

#define POSITIVE 0x1000
#define BIT_A 1
#define BIT_B 2

#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;
int16_t clickcounter = 0;
int16_t turncounter = 0;
uint8_t turnstate = 0;
uint8_t lastturnstate = 0;
uint8_t clickstate = 0;
uint8_t lastclickstate = 0;
uint8_t knob_pin_a = 0;
uint8_t knob_pin_b = 0;
uint8_t knob_pin_c = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    uint8_t level = gpio_get_level(gpio_num);
    if (level) {
        gpio_num |= POSITIVE;
    }
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}


void knob_init(uint8_t pin_a, uint8_t pin_b, uint8_t pin_c) {
    knob_pin_a = pin_a;
    knob_pin_b = pin_b;
    knob_pin_c = pin_c;

    gpio_set_direction(pin_a, GPIO_MODE_INPUT);
    gpio_set_direction(pin_b, GPIO_MODE_INPUT);
    gpio_set_direction(pin_c, GPIO_MODE_INPUT);

    gpio_evt_queue = xQueueCreate(50, sizeof(uint32_t));

    gpio_set_intr_type(pin_a, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(pin_b, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(pin_c, GPIO_INTR_ANYEDGE);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_isr_handler_add(pin_a, gpio_isr_handler, (void*) pin_a);
    gpio_isr_handler_add(pin_b, gpio_isr_handler, (void*) pin_b);
    gpio_isr_handler_add(pin_c, gpio_isr_handler, (void*) pin_c);
}

uint8_t knob_read() {
    uint32_t io_num = 0;

    if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
        // printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        if (io_num >= POSITIVE) {

            if ((io_num & 255) == knob_pin_a) {
                turnstate |= BIT_A;
            }
            if ((io_num & 255) == knob_pin_b) {
                turnstate |= BIT_B;
            }
            if ((io_num & 255) == knob_pin_c) {
                clickstate = 1;
            }
        } else {

            if (io_num == knob_pin_a) {
                turnstate &= ~BIT_A;
            }
            if (io_num == knob_pin_b) {
                turnstate &= ~BIT_B;
            }
            if (io_num == knob_pin_c) {
                clickstate = 0;
            }
        }

        // printf("GPIO Interrupt, io_num=%d, turnstate=%d, clickstate=%d, clicks=%d, turn=%d\n", io_num, turnstate, clickstate, clickcounter, turncounter);

        if (turnstate != lastturnstate) {
            printf("Knob: Turned %d -> %d.\n", lastturnstate, turnstate);
            // 0->2, 2->3, 3->1, 1-> 0
            // if ()

            // 3 -> 1 = LEFT?
            // 3 -> 2 = RIGHT?

            if (turnstate == 1 && lastturnstate == 3) {
                turncounter --;
                printf("Knob: Turn left, turn counter = %d\n", turncounter);
            }
            if (turnstate == 2 && lastturnstate == 3) {
                turncounter ++;
                printf("Knob: Turn right, turn counter = %d\n", turncounter);
            }

            lastturnstate = turnstate;
        }

        if (clickstate != lastclickstate) {
            if (clickstate) {
                printf("Knob: Pressed, click counter = %d\n", clickcounter);
            } else {
                clickcounter ++;
                printf("Knob: Released, click counter = %d\n", clickcounter);
            }
            lastclickstate = clickstate;
        }
    }

    return 0;
}

