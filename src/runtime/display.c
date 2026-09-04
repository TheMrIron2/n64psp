#include "n64psp/display.h"

#include <math.h>

#define N64PSP_DISPLAY_LOGICAL_WIDTH 320.0f
#define N64PSP_DISPLAY_LOGICAL_HEIGHT 240.0f

typedef struct n64psp_display_mode_description {
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
    uint8_t anamorphic;
} n64psp_display_mode_description;

static const n64psp_display_mode_description n64psp_display_modes[N64PSP_DISPLAY_MODE_COUNT] = {
    {
        N64PSP_DISPLAY_OUTPUT_PSP_LCD,
        480, 272,
        80, 16, 320, 240,
        80, 16, 320, 240,
        4.0f / 3.0f,
        0
    },
    {
        N64PSP_DISPLAY_OUTPUT_PSP_LCD,
        480, 272,
        59, 0, 362, 272,
        59, 0, 362, 272,
        4.0f / 3.0f,
        0
    },
    {
        N64PSP_DISPLAY_OUTPUT_PSP_LCD,
        480, 272,
        0, 0, 480, 272,
        59, 0, 362, 272,
        480.0f / 272.0f,
        0
    },
    {
        N64PSP_DISPLAY_OUTPUT_TV,
        720, 480,
        40, 0, 640, 480,
        40, 0, 640, 480,
        4.0f / 3.0f,
        0
    },
    {
        N64PSP_DISPLAY_OUTPUT_TV,
        720, 480,
        0, 0, 720, 480,
        40, 0, 640, 480,
        16.0f / 9.0f,
        1
    }
};

int n64psp_display_configure(n64psp_display_config *config, n64psp_display_mode mode) {
    const n64psp_display_mode_description *description;

    if (config == 0 || mode < N64PSP_DISPLAY_PSP_320X240 || mode >= N64PSP_DISPLAY_MODE_COUNT) {
        return 0;
    }

    description = &n64psp_display_modes[mode];
    config->mode = mode;
    config->output = description->output;
    config->framebuffer_width = description->framebuffer_width;
    config->framebuffer_height = description->framebuffer_height;
    config->viewport_x = description->viewport_x;
    config->viewport_y = description->viewport_y;
    config->viewport_width = description->viewport_width;
    config->viewport_height = description->viewport_height;
    config->ui_viewport_x = description->ui_viewport_x;
    config->ui_viewport_y = description->ui_viewport_y;
    config->ui_viewport_width = description->ui_viewport_width;
    config->ui_viewport_height = description->ui_viewport_height;
    /* Anamorphic display aspect is independent of framebuffer aspect */
    config->display_aspect = description->display_aspect;
    config->logical_height = N64PSP_DISPLAY_LOGICAL_HEIGHT;
    config->logical_width = config->logical_height * config->display_aspect;
    config->scale_x = (float)config->viewport_width / config->logical_width;
    config->scale_y = (float)config->viewport_height / config->logical_height;
    config->side_extension = (config->logical_width - N64PSP_DISPLAY_LOGICAL_WIDTH) * 0.5f;
    config->pixel_aspect = description->anamorphic
                               ? config->display_aspect /
                                     ((float)config->framebuffer_width / (float)config->framebuffer_height)
                               : 1.0f;
    config->anamorphic = description->anamorphic;
    return 1;
}

float n64psp_ui_from_left(const n64psp_display_config *config, float x) {
    return x - config->side_extension;
}

float n64psp_ui_from_right(const n64psp_display_config *config, float x) {
    return x + config->side_extension;
}

float n64psp_ui_centered(float x) {
    return x;
}

float n64psp_ui_to_framebuffer_x(const n64psp_display_config *config, float x) {
    return (float)config->viewport_x + (x + config->side_extension) * config->scale_x;
}

float n64psp_ui_to_framebuffer_y(const n64psp_display_config *config, float y) {
    return (float)config->viewport_y + y * config->scale_y;
}

int n64psp_ui_pixel_x(const n64psp_display_config *config, float x) {
    return (int)lroundf(n64psp_ui_to_framebuffer_x(config, x));
}

int n64psp_ui_pixel_y(const n64psp_display_config *config, float y) {
    return (int)lroundf(n64psp_ui_to_framebuffer_y(config, y));
}

int n64psp_ui_left_edge(const n64psp_display_config *config, float x) {
    return (int)floorf(n64psp_ui_to_framebuffer_x(config, x));
}

int n64psp_ui_right_edge(const n64psp_display_config *config, float x) {
    return (int)ceilf(n64psp_ui_to_framebuffer_x(config, x));
}

int n64psp_ui_top_edge(const n64psp_display_config *config, float y) {
    return (int)floorf(n64psp_ui_to_framebuffer_y(config, y));
}

int n64psp_ui_bottom_edge(const n64psp_display_config *config, float y) {
    return (int)ceilf(n64psp_ui_to_framebuffer_y(config, y));
}
