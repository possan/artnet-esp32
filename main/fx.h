#ifndef __FX_H__
#define __FX_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    int32_t _time; // local time
    int32_t _progress; // local progress (1024 * 1024)
    int32_t opacity; // percent
    int32_t offset; // offset in percent
    int32_t color[3]; // rgb value, 0-255
    int32_t size; // width in percent
    int32_t feather_left; // width in percent
    int32_t feather_right; // width in percent
    int32_t speed_multiplier; // base speed multiplier * 1000
    int32_t repeat; // repeats, absolute value, 1-10
    int32_t gamma; // in percent 0-200
} FxLayerSettings;

typedef struct {
    int32_t _time; // local time
    int32_t _progress; // local progress (1024 * 1024)
    int32_t time_offset; // time offset in milliseconds
    int32_t num_leds; // number of leds
    int32_t skip_leds; // number of leds to skip in the beginning
    int32_t opacity; // percent
    int32_t pixel_order; // indexed, 0-5
    int32_t base_speed; // rpm * 1000
    int32_t test_pattern; // indexed, 0-1
    int32_t artnet_universe; // indexed, 0-n
    int32_t dummy; // force reload
    FxLayerSettings layer[4];
} FxSettings;

void fx_render(FxSettings *fx, uint32_t time, uint8_t *rgb, int max_leds, uint8_t *temp, uint32_t delta_time);
bool fx_set_osc_property(FxSettings *fx, char *property, uint32_t value);
void fx_get_config_json(FxSettings *fx, char *destination, uint32_t maxsize, uint32_t uptime);

#ifdef	__cplusplus
}
#endif

#endif