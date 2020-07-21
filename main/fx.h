#ifndef __FX_H__
#define __FX_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    int32_t opacity;
    int32_t offset;
    int32_t color[3];
    int32_t radius;
    int32_t feather_left;
    int32_t feather_right;
    int32_t speed;
    int32_t repeat;
    int32_t gamma;
} FxLayerSettings;

typedef struct {
    int32_t time_offset;
    int32_t num_leds;
    int32_t opacity;
    int32_t pixel_order;
    FxLayerSettings layer[4];
} FxSettings;

void fx_render_layer(FxLayerSettings *layer, uint32_t time, uint8_t *rgb, int num_leds, uint8_t *temp, int opacity);
void fx_render(FxSettings *fx, uint32_t time, uint8_t *rgb, int max_leds, uint8_t *temp);
bool fx_set_osc_property(FxSettings *fx, char *property, float value);
void fx_get_config_json(FxSettings *fx, char *destination, uint32_t maxsize);

#ifdef	__cplusplus
}
#endif

#endif