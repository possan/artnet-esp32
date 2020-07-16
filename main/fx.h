
#ifndef __FX_H__
#define __FX_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t opacity;
    float offset;
    uint32_t color[3];
    uint32_t radius;
    uint32_t feather_left;
    uint32_t feather_right;
    float speed;
    uint32_t repeat;
    float gamma;
} FxLayerSettings;

typedef struct {
    uint32_t time_offset;
    uint32_t num_leds;
    FxLayerSettings layer[4];
} FxSettings;

void fx_render_layer(FxLayerSettings *layer, float time, uint8_t *rgb, int len);
void fx_render(FxSettings *fx, float time, uint8_t *rgb, int len);

#ifdef	__cplusplus
}
#endif

#endif