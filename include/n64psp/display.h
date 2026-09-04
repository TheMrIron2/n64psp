#ifndef N64PSP_DISPLAY_H
#define N64PSP_DISPLAY_H

#include <stdint.h>

typedef enum n64psp_display_output {
    N64PSP_DISPLAY_OUTPUT_PSP_LCD = 0,
    N64PSP_DISPLAY_OUTPUT_TV = 1
} n64psp_display_output;

typedef enum n64psp_display_mode {
    N64PSP_DISPLAY_PSP_320X240 = 0,
    N64PSP_DISPLAY_PSP_362X272 = 1,
    N64PSP_DISPLAY_PSP_480X272 = 2,
    N64PSP_DISPLAY_PSP_MODE_COUNT = 3,
    N64PSP_DISPLAY_TV_720X480_4_3 = 3,
    N64PSP_DISPLAY_TV_720X480_16_9 = 4,
    N64PSP_DISPLAY_MODE_COUNT = 5,

    N64PSP_DISPLAY_ORIGINAL = N64PSP_DISPLAY_PSP_320X240,
    N64PSP_DISPLAY_4_3 = N64PSP_DISPLAY_PSP_362X272,
    N64PSP_DISPLAY_WIDESCREEN = N64PSP_DISPLAY_PSP_480X272
} n64psp_display_mode;

typedef struct n64psp_display_config {
    n64psp_display_mode mode;
    n64psp_display_output output;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t viewport_x;
    uint16_t viewport_y;
    uint16_t viewport_width;
    uint16_t viewport_height;
    uint16_t ui_viewport_x;
    uint16_t ui_viewport_y;
    uint16_t ui_viewport_width;
    uint16_t ui_viewport_height;
    float display_aspect;
    float pixel_aspect;
    float logical_width;
    float logical_height;
    float scale_x;
    float scale_y;
    float side_extension;
    uint8_t anamorphic;
} n64psp_display_config;

int n64psp_display_configure(n64psp_display_config *config, n64psp_display_mode mode);

/* Semantic layout remains logical until framebuffer mapping */
float n64psp_ui_from_left(const n64psp_display_config *config, float x);
float n64psp_ui_from_right(const n64psp_display_config *config, float x);
float n64psp_ui_centered(float x);
float n64psp_ui_to_framebuffer_x(const n64psp_display_config *config, float x);
float n64psp_ui_to_framebuffer_y(const n64psp_display_config *config, float y);
int n64psp_ui_pixel_x(const n64psp_display_config *config, float x);
int n64psp_ui_pixel_y(const n64psp_display_config *config, float y);
int n64psp_ui_left_edge(const n64psp_display_config *config, float x);
int n64psp_ui_right_edge(const n64psp_display_config *config, float x);
int n64psp_ui_top_edge(const n64psp_display_config *config, float y);
int n64psp_ui_bottom_edge(const n64psp_display_config *config, float y);

#endif
