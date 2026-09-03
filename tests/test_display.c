#include "n64psp/display.h"

#include <stdio.h>

#define CHECK(value) do { if (!(value)) return 1; } while (0)

int main(void) {
    n64psp_display_config config;

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_ORIGINAL, 320, 240, 480, 272));
    CHECK(config.viewport_x == 80 && config.viewport_y == 16);
    CHECK(config.viewport_width == 320 && config.viewport_height == 240);
    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_4_3, 320, 240, 480, 272));
    CHECK(config.viewport_x == 60 && config.viewport_y == 1);
    CHECK(config.viewport_width == 360 && config.viewport_height == 270);
    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_WIDESCREEN, 320, 240, 480, 272));
    CHECK(config.viewport_x == 0 && config.viewport_y == 0);
    CHECK(config.viewport_width == 480 && config.viewport_height == 272);
    CHECK(config.ui_viewport_x == 60 && config.ui_viewport_y == 1);
    CHECK(config.ui_viewport_width == 360 && config.ui_viewport_height == 270);
    CHECK(config.projection_aspect > 1.76f && config.projection_aspect < 1.77f);
    CHECK(!n64psp_display_configure(&config, N64PSP_DISPLAY_ORIGINAL, 320, 240, 300, 200));
    puts("display tests passed");
    return 0;
}
