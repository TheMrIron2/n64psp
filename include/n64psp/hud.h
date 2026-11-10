#ifndef N64PSP_HUD_H
#define N64PSP_HUD_H

#include "n64psp/display.h"
#include "n64psp/hud_types.h"

// Place a HUD widget within a logical screen region
static inline void n64psp_hud_region_fit(n64psp_hud_transform *out, const n64psp_display_config *display,
                                        float left, float top, float width, float height,
                                        float anchor_x, float anchor_y, unsigned alignment, int scaled) {
    float sx = (float) display->viewport_width / 320.0f;
    float sy = (float) display->viewport_height / 240.0f;
    float ux = (float) display->ui_viewport_width / 320.0f;
    float uy = (float) display->ui_viewport_height / 240.0f;
    float pixel_x, pixel_y;
    float draw_x = scaled ? ux : 1.0f;
    float draw_y = scaled ? uy : 1.0f;

    if (alignment & N64PSP_HUD_RIGHT) {
        pixel_x = (left + width) * sx - (left + width - anchor_x) * ux;
    } else if (alignment & N64PSP_HUD_CENTER) {
        pixel_x = (left + width * 0.5f) * sx + (anchor_x - left - width * 0.5f) * ux;
    } else {
        pixel_x = left * sx + (anchor_x - left) * ux;
    }
    pixel_y = alignment & N64PSP_HUD_BOTTOM ?
        (top + height) * sy - (top + height - anchor_y) * uy : top * sy + (anchor_y - top) * uy;
    out->scale_x = draw_x / sx;
    out->scale_y = draw_y / sy;
    out->offset_x = (__builtin_roundf(pixel_x) - anchor_x * draw_x) / sx;
    out->offset_y = (__builtin_roundf(pixel_y) - anchor_y * draw_y) / sy;
}

#endif
