#include "n64psp/display.h"

static uint32_t n64psp_display_gcd(uint32_t a, uint32_t b) {
    while (b != 0U) {
        const uint32_t remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

int n64psp_display_configure(n64psp_display_config *config, n64psp_display_mode mode,
                             uint16_t logical_width, uint16_t logical_height,
                             uint16_t surface_width, uint16_t surface_height) {
    uint32_t width;
    uint32_t height;

    if (config == 0 || logical_width == 0 || logical_height == 0 || surface_width == 0 || surface_height == 0 ||
        mode < N64PSP_DISPLAY_ORIGINAL || mode >= N64PSP_DISPLAY_MODE_COUNT) {
        return 0;
    }

    width = surface_width;
    height = surface_height;
    if (mode == N64PSP_DISPLAY_ORIGINAL) {
        width = logical_width;
        height = logical_height;
    } else if (mode == N64PSP_DISPLAY_4_3) {
        const uint32_t divisor = n64psp_display_gcd(logical_width, logical_height);
        const uint32_t aspect_width = logical_width / divisor;
        const uint32_t aspect_height = logical_height / divisor;

        height = surface_height - ((uint32_t)surface_height % aspect_height);
        width = (height / aspect_height) * aspect_width;
        if (width > surface_width) {
            width = surface_width - ((uint32_t)surface_width % aspect_width);
            height = (width / aspect_width) * aspect_height;
        }
    }
    if (width > surface_width || height > surface_height) {
        return 0;
    }

    config->mode = mode;
    config->logical_width = logical_width;
    config->logical_height = logical_height;
    config->surface_width = surface_width;
    config->surface_height = surface_height;
    config->viewport_width = (uint16_t)width;
    config->viewport_height = (uint16_t)height;
    config->viewport_x = (uint16_t)((surface_width - width) / 2U);
    config->viewport_y = (uint16_t)((surface_height - height) / 2U);
    config->ui_viewport_x = config->viewport_x;
    config->ui_viewport_y = config->viewport_y;
    config->ui_viewport_width = config->viewport_width;
    config->ui_viewport_height = config->viewport_height;
    if (mode == N64PSP_DISPLAY_WIDESCREEN) {
        const uint32_t divisor = n64psp_display_gcd(logical_width, logical_height);
        const uint32_t aspect_width = logical_width / divisor;
        const uint32_t aspect_height = logical_height / divisor;
        const uint32_t ui_height = surface_height - ((uint32_t)surface_height % aspect_height);
        const uint32_t ui_width = (ui_height / aspect_height) * aspect_width;

        config->ui_viewport_width = (uint16_t)ui_width;
        config->ui_viewport_height = (uint16_t)ui_height;
        config->ui_viewport_x = (uint16_t)((surface_width - ui_width) / 2U);
        config->ui_viewport_y = (uint16_t)((surface_height - ui_height) / 2U);
    }
    config->projection_aspect = mode == N64PSP_DISPLAY_WIDESCREEN
                                    ? (float)surface_width / (float)surface_height
                                    : (float)logical_width / (float)logical_height;
    return 1;
}
