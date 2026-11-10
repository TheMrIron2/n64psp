#ifndef N64PSP_HUD_TYPES_H
#define N64PSP_HUD_TYPES_H

enum { N64PSP_HUD_LEFT = 0, N64PSP_HUD_RIGHT = 1, N64PSP_HUD_CENTER = 2, N64PSP_HUD_BOTTOM = 4 };

typedef struct n64psp_hud_transform {
    float scale_x, scale_y;
    float offset_x, offset_y;
} n64psp_hud_transform;

#endif
