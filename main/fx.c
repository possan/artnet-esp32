#include "fx.h"
#include <stdio.h>
#include <string.h>

uint8_t brightness_at_position(FxLayerSettings *layer, FxSettings *fx, float time, int led, int num_leds, uint8_t *temp) {



    uint32_t ramp_i =
      1024*1024 +
      (led * 1024) +


      ((time * layer->speed_multiplier / 1000) * num_leds * fx->base_speed / 60000) + // base_speed is in rpm * 1000



      // ((time * layer->speed_multiplier) / 16) +

      ((layer->offset * num_leds * 1024 / layer->repeat) / 100000) // offset is in percent * 1000
      ;

    // uint32_t p = (ramp_i >> 10) % (num_leds / layer->repeat);

    ramp_i *= layer->repeat;
    ramp_i &= 0xFFFFFFF;


    uint16_t f1 = ramp_i & 1023;
    uint16_t f0 = 1023 - f1;

    uint16_t o0 = ramp_i >> 10;
    uint16_t v0 = temp[(o0) % num_leds];
    uint16_t v1 = temp[(o0 + 1) % num_leds];

    uint16_t v = ((v0 * f0) + (v1 * f1)) / 1024;

    return v;

    // return p < layer->size ? 255 : 0;
}

void fx_render_layer(FxLayerSettings *layer, FxSettings *fx, uint32_t time, uint8_t *rgb, int num_leds, uint8_t *temp, int opacity) {

    uint16_t finalopacity = (layer->opacity * opacity);

    if (finalopacity < 1) {
        return;
    }

    uint32_t rr = (layer->color[0] * finalopacity) / 10000;
    uint32_t gg = (layer->color[1] * finalopacity) / 10000;
    uint32_t bb = (layer->color[2] * finalopacity) / 10000;

    if (rr < 1 && gg < 1 && bb < 1) {
        return;
    }

    int o;

    memset(temp, 0, num_leds);

    // sizes are in percent * 1000
    int s1 = (layer->feather_left * num_leds) / 100;
    int s2 = (layer->size * num_leds) / 100;
    int s3 = (layer->feather_right * num_leds) / 100;

    o = 0;
    for(int j=0; j<num_leds; j++) {
      if (j < s1 && s1 > 0) {
        temp[o] = (j * 255) / s1;
      } else if(j < s1 + s2) {
        temp[o] = 255;
      } else if(j < s1 + s2 + s3 && s3 > 0) {
        temp[o] = 255 - ((j - (s1 + s2)) * 255) / s3;
      }
      o ++;
    }

    // printf("blending layer... %d leds, time %d, color %d %d %d: ", num_leds, time, rr, gg, bb);

    o = 0;
    for(int j=0; j<num_leds; j++) {

      uint16_t x, bri = brightness_at_position(layer, fx, time, j, num_leds, temp);

      // printf("%3d ", bri);

      x = rgb[o];
      x += (rr * bri) >> 8;
      if (x > 255) x = 255;
      rgb[o] = x;
      o ++;

      x = rgb[o];
      x += (gg * bri) >> 8;
      if (x > 255) x = 255;
      rgb[o] = x;
      o ++;

      x = rgb[o];
      x += (bb * bri) >> 8;
      if (x > 255) x = 255;
      rgb[o] = x;
      o ++;
    }
    // printf("\n");
}

void fx_render(FxSettings *fx, uint32_t time, uint8_t *rgb, int max_leds, uint8_t *temp) {
    memset(rgb, 0, max_leds * 3);

    uint32_t time2 = time + fx->time_offset;

    // printf("rendering... %d leds, time = %d\n", fx->num_leds, time2);

    if (fx->opacity < 1) {
        return;
    }

    fx_render_layer(&fx->layer[0], fx, time2, rgb, fx->num_leds, temp, fx->opacity);
    fx_render_layer(&fx->layer[1], fx, time2, rgb, fx->num_leds, temp, fx->opacity);
    fx_render_layer(&fx->layer[2], fx, time2, rgb, fx->num_leds, temp, fx->opacity);
    fx_render_layer(&fx->layer[3], fx, time2, rgb, fx->num_leds, temp, fx->opacity);
}

bool fx_set_osc_property(FxSettings *fx, char *addr, uint32_t value) {
  bool debug = true;

  if (strcmp(addr, "/length") == 0) {
    fx->num_leds = value;
    if (debug) {
      printf("Master num leds: %d\n", fx->num_leds);
    }
    return true;
  }

  if (strcmp(addr, "/nudge") == 0) {
    fx->time_offset = value;
    if (debug) {
      printf("Master time offset: %d\n", fx->time_offset);
    }
    return true;
  }

  if (strcmp(addr, "/opacity") == 0) {
    fx->opacity = value;
    if (debug) {
      printf("Master opacity: %d\n", fx->opacity);
    }
    return true;
  }

  if (strcmp(addr, "/pixelorder") == 0) {
    fx->pixel_order = value;
    if (debug) {
      printf("Pixel order: %d\n", fx->pixel_order);
    }
    return true;
  }

  if (strcmp(addr, "/basespeed") == 0) {
    fx->base_speed = value;
    if (debug) {
      printf("Base speed: %d\n", fx->base_speed);
    }
    return true;
  }


  if (strcmp(addr, "/layer1/opacity") == 0) {
    fx->layer[0].opacity = value;
    if (debug) {
      printf("Layer 1 opacity: %d\n", fx->layer[0].opacity);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/offset") == 0) {
    fx->layer[0].offset = value;
    if (debug) {
      printf("Layer 1 offset: %d\n", fx->layer[0].offset);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/red") == 0) {
    fx->layer[0].color[0] = value;
    if (debug) {
      printf("Layer 1 red: %d\n", fx->layer[0].color[0]);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/green") == 0) {
    fx->layer[0].color[1] = value;
    if (debug) {
      printf("Layer 1 green: %d\n", fx->layer[0].color[1]);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/blue") == 0) {
    fx->layer[0].color[2] = value;
    if (debug) {
      printf("Layer 1 blue: %d\n", fx->layer[0].color[2]);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/size") == 0) {
    fx->layer[0].size = value;
    if (debug) {
      printf("Layer 1 size: %d\n", fx->layer[0].size);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/repeat") == 0) {
    fx->layer[0].repeat = value;
    if (debug) {
      printf("Layer 1 repeat: %d\n", fx->layer[0].repeat);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/gamma") == 0) {
    fx->layer[0].gamma = value;
    if (debug) {
      printf("Layer 1 gamma: %d\n", fx->layer[0].gamma);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/speed") == 0) {
    fx->layer[0].speed_multiplier = value;
    if (debug) {
      printf("Layer 1 speed multiplier: %d\n", fx->layer[0].speed_multiplier);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/feather1") == 0) {
    fx->layer[0].feather_left = value;
    if (debug) {
      printf("Layer 1 feather_left: %d\n", fx->layer[0].feather_left);
    }
    return true;
  }

  if (strcmp(addr, "/layer1/feather2") == 0) {
    fx->layer[0].feather_right = value;
    if (debug) {
      printf("Layer 1 feather_right: %d\n", fx->layer[0].feather_right);
    }
    return true;
  }


  if (strcmp(addr, "/layer2/opacity") == 0) {
    fx->layer[1].opacity = value;
    if (debug) {
      printf("Layer 2 opacity: %d\n", fx->layer[1].opacity);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/offset") == 0) {
    fx->layer[1].offset = value;
    if (debug) {
      printf("Layer 2 offset: %d\n", fx->layer[1].offset);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/red") == 0) {
    fx->layer[1].color[0] = value;
    if (debug) {
      printf("Layer 2 red: %d\n", fx->layer[1].color[0]);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/green") == 0) {
    fx->layer[1].color[1] = value;
    if (debug) {
      printf("Layer 2 green: %d\n", fx->layer[1].color[1]);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/blue") == 0) {
    fx->layer[1].color[2] = value;
    if (debug) {
      printf("Layer 2 blue: %d\n", fx->layer[1].color[2]);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/size") == 0) {
    fx->layer[1].size = value;
    if (debug) {
      printf("Layer 2 size: %d\n", fx->layer[1].size);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/repeat") == 0) {
    fx->layer[1].repeat = value;
    if (debug) {
      printf("Layer 2 repeat: %d\n", fx->layer[1].repeat);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/gamma") == 0) {
    fx->layer[1].gamma = value;
    if (debug) {
      printf("Layer 2 gamma: %d\n", fx->layer[1].gamma);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/speed") == 0) {
    fx->layer[1].speed_multiplier = value;
    if (debug) {
      printf("Layer 2 speed multiplier: %d\n", fx->layer[1].speed_multiplier);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/feather1") == 0) {
    fx->layer[1].feather_left = value;
    if (debug) {
      printf("Layer 2 feather_left: %d\n", fx->layer[1].feather_left);
    }
    return true;
  }

  if (strcmp(addr, "/layer2/feather2") == 0) {
    fx->layer[1].feather_right = value;
    if (debug) {
      printf("Layer 2 feather_right: %d\n", fx->layer[1].feather_right);
    }
    return true;
  }


  if (strcmp(addr, "/layer3/opacity") == 0) {
    fx->layer[2].opacity = value;
    if (debug) {
      printf("Layer 3 opacity: %d\n", fx->layer[2].opacity);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/offset") == 0) {
    fx->layer[2].offset = value;
    if (debug) {
      printf("Layer 3 offset: %d\n", fx->layer[2].offset);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/red") == 0) {
    fx->layer[2].color[0] = value;
    if (debug) {
      printf("Layer 3 red: %d\n", fx->layer[2].color[0]);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/green") == 0) {
    fx->layer[2].color[1] = value;
    if (debug) {
      printf("Layer 3 green: %d\n", fx->layer[2].color[1]);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/blue") == 0) {
    fx->layer[2].color[2] = value;
    if (debug) {
      printf("Layer 3 blue: %d\n", fx->layer[2].color[2]);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/size") == 0) {
    fx->layer[2].size = value;
    if (debug) {
      printf("Layer 3 size: %d\n", fx->layer[2].size);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/repeat") == 0) {
    fx->layer[2].repeat = value;
    if (debug) {
      printf("Layer 3 repeat: %d\n", fx->layer[2].repeat);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/gamma") == 0) {
    fx->layer[2].gamma = value;
    if (debug) {
      printf("Layer 3 gamma: %d\n", fx->layer[2].gamma);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/speed") == 0) {
    fx->layer[2].speed_multiplier = value;
    if (debug) {
      printf("Layer 3 speed multiplier: %d\n", fx->layer[2].speed_multiplier);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/feather1") == 0) {
    fx->layer[2].feather_left = value;
    if (debug) {
      printf("Layer 3 feather_left: %d\n", fx->layer[2].feather_left);
    }
    return true;
  }

  if (strcmp(addr, "/layer3/feather2") == 0) {
    fx->layer[2].feather_right = value;
    if (debug) {
      printf("Layer 3 feather_right: %d\n", fx->layer[2].feather_right);
    }
    return true;
  }


  if (strcmp(addr, "/layer4/opacity") == 0) {
    fx->layer[3].opacity = value;
    if (debug) {
      printf("Layer 4 opacity: %d\n", fx->layer[3].opacity);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/offset") == 0) {
    fx->layer[3].offset = value;
    if (debug) {
      printf("Layer 4 offset: %d\n", fx->layer[3].offset);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/red") == 0) {
    fx->layer[3].color[0] = value;
    if (debug) {
      printf("Layer 4 red: %d\n", fx->layer[3].color[0]);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/green") == 0) {
    fx->layer[3].color[1] = value;
    if (debug) {
      printf("Layer 4 green: %d\n", fx->layer[3].color[1]);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/blue") == 0) {
    fx->layer[3].color[2] = value;
    if (debug) {
      printf("Layer 4 blue: %d\n", fx->layer[3].color[2]);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/size") == 0) {
    fx->layer[3].size = value;
    if (debug) {
      printf("Layer 4 size: %d\n", fx->layer[3].size);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/repeat") == 0) {
    fx->layer[3].repeat = value;
    if (debug) {
      printf("Layer 4 repeat: %d\n", fx->layer[3].repeat);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/gamma") == 0) {
    fx->layer[3].gamma = value;
    if (debug) {
      printf("Layer 4 gamma: %d\n", fx->layer[3].gamma);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/speed") == 0) {
    fx->layer[3].speed_multiplier = value;
    if (debug) {
      printf("Layer 4 speed multiplier: %d\n", fx->layer[3].speed_multiplier);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/feather1") == 0) {
    fx->layer[3].feather_left = value;
    if (debug) {
      printf("Layer 4 feather_left: %d\n", fx->layer[3].feather_left);
    }
    return true;
  }

  if (strcmp(addr, "/layer4/feather2") == 0) {
    fx->layer[3].feather_right = value;
    if (debug) {
      printf("Layer 4 feather_right: %d\n", fx->layer[3].feather_right);
    }
    return true;
  }

  return false;
}

void fx_get_layer_config_json(FxLayerSettings *layer, char *destination, uint32_t maxsize) {
  sprintf(destination, "{"
    "\"op\":%d,"
    "\"of\":%d,"
    "\"r\":%d,"
    "\"g\":%d,"
    "\"b\":%d,"
    "\"siz\":%d,"
    "\"rep\":%d,"
    "\"gam\":%d,"
    "\"spd\":%d,"
    "\"fe1\":%d,"
    "\"fe2\":%d"
    "}",
    layer->opacity,
    layer->offset,
    layer->color[0],
    layer->color[1],
    layer->color[2],
    layer->size,
    layer->repeat,
    layer->gamma,
    layer->speed_multiplier,
    layer->feather_left,
    layer->feather_right);
}

void fx_get_config_json(FxSettings *fx, char *destination, uint32_t maxsize) {
  char l1buf[500];
  char l2buf[500];
  char l3buf[500];
  char l4buf[500];

  fx_get_layer_config_json(&fx->layer[0], &l1buf, 200);
  fx_get_layer_config_json(&fx->layer[1], &l2buf, 200);
  fx_get_layer_config_json(&fx->layer[2], &l3buf, 200);
  fx_get_layer_config_json(&fx->layer[3], &l4buf, 200);

  // strcmp(addr, "/length") == 0
  // strcmp(addr, "/nudge") == 0
  // strcmp(addr, "/opacity") == 0
  // strcmp(addr, "/pixelorder") == 0

  sprintf(destination, "{\n"
    "\"length\":%d,\"pixelorder\":%d,\"opacity\":%d,\"nudge\":%d,\n,\"basespeed\":%d,\n"
    "\"layers\":[\n%s,\n%s,\n%s,\n%s\n"
    "]}",
    fx->num_leds, fx->pixel_order, fx->opacity, fx->time_offset, fx->base_speed,
    l1buf, l2buf, l3buf, l4buf);
}
