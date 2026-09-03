#ifndef N64PSP_DISPLAY_H
#define N64PSP_DISPLAY_H

#include <stdint.h>

typedef enum n64psp_display_mode {
    N64PSP_DISPLAY_ORIGINAL = 0,
    N64PSP_DISPLAY_4_3 = 1,
    N64PSP_DISPLAY_WIDESCREEN = 2,
    N64PSP_DISPLAY_MODE_COUNT = 3
} n64psp_display_mode;

typedef struct n64psp_display_config {
    n64psp_display_mode mode;
    uint16_t logical_width;
    uint16_t logical_height;
    uint16_t surface_width;
    uint16_t surface_height;
    uint16_t viewport_x;
    uint16_t viewport_y;
    uint16_t viewport_width;
    uint16_t viewport_height;
    uint16_t ui_viewport_x;
    uint16_t ui_viewport_y;
    uint16_t ui_viewport_width;
    uint16_t ui_viewport_height;
    float projection_aspect;
} n64psp_display_config;

int n64psp_display_configure(n64psp_display_config *config, n64psp_display_mode mode,
                             uint16_t logical_width, uint16_t logical_height,
                             uint16_t surface_width, uint16_t surface_height);

#endif
