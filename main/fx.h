#ifndef __FX_H__
#define __FX_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    int32_t opacity; // percent
    // int32_t opacity_envamt; // opacity env amount 0-100 % 
    // int32_t opacity_envtype; // opacity env type, 0=none, 1=lfo, 2=noise,
    // int32_t opacity_envspeed; // opacity env speed

    int32_t offset; // offset in percent
    // int32_t offset_envamt; // offset env amount 0-100 % 
    // int32_t offset_envtype; // offset env type, 0=none, 1=lfo, 2=noise,
    // int32_t offset_envspeed; // offset env speed

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
 
    // int32_t opacity_c;
    // int32_t opacity_d;

    // int32_t offset_c;
    // int32_t offset_d;

    // int32_t red_c;
    // int32_t red_d;

    // int32_t green_c;
    // int32_t green_d;

    // int32_t blue_c;
    // int32_t blue_d;

    // int32_t size_c;
    // int32_t size_d;

    // int32_t feather_left_c;
    // int32_t feather_left_d;

    // int32_t feather_right_c;
    // int32_t feather_right_d;

    // int32_t speed_multiplier_c;
    // int32_t speed_multiplier_d;

    // int32_t repeat_c;
    // int32_t repeat_d;
} FxLayerState;

typedef struct {
    int32_t num_leds; // number of leds
    int32_t skip_leds; // number of leds to skip in the beginning
    int32_t pixel_order; // indexed, 0-5
    int32_t test_pattern; // indexed, 0-1
    int32_t artnet_universe; // indexed, 0-n
    int32_t reversed; // on/off
    int32_t interpolation_time; // time in milliseconds
    int32_t dummy; // force reload

    int32_t base_speed; // rpm * 1000
    int32_t opacity; // percent
    int32_t time_offset; // time offset in milliseconds

    FxLayerSettings layer[4];
} FxVisSettings;

typedef struct {
    int32_t _time; // local time
    int32_t _progress; // local progress (1024 * 1024)

    int32_t base_speed_c;
    int32_t base_speed_d; // maximum delta per ms

    // int32_t opacity_c;
    // int32_t opacity_d;
    // int32_t time_offset_c;
    // int32_t time_offset_d;

    FxLayerState layer[4];
} FxVisState;

typedef struct {
    FxVisSettings settings;
    FxVisState state;
} Fx;

void fx_render(Fx *fx, uint32_t time, uint8_t *rgb, int max_leds, uint8_t *temp, uint32_t delta_time);
bool fx_set_osc_property(Fx *fx, char *property, uint32_t value);
void fx_get_config_json(FxVisSettings *fxSettings, char *destination, uint32_t maxsize, uint32_t uptime);

#ifdef	__cplusplus
}
#endif

#endif