#include "n64psp/display.h"

#include <stdio.h>

#define CHECK(value) do { if (!(value)) return 1; } while (0)
#define CHECK_CLOSE(actual, expected) CHECK((actual) > (expected) - 0.0001f && (actual) < (expected) + 0.0001f)

static int check_common_4_3(const n64psp_display_config *config, float scale_x, float scale_y) {
    CHECK_CLOSE(config->display_aspect, 4.0f / 3.0f);
    CHECK_CLOSE(config->pixel_aspect, 1.0f);
    CHECK_CLOSE(config->logical_width, 320.0f);
    CHECK_CLOSE(config->logical_height, 240.0f);
    CHECK_CLOSE(config->scale_x, scale_x);
    CHECK_CLOSE(config->scale_y, scale_y);
    CHECK_CLOSE(config->side_extension, 0.0f);
    CHECK(config->anamorphic == 0);
    return 0;
}

int main(void) {
    n64psp_display_config config;

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_PSP_320X240));
    CHECK(config.output == N64PSP_DISPLAY_OUTPUT_PSP_LCD);
    CHECK(config.framebuffer_width == 480 && config.framebuffer_height == 272);
    CHECK(config.viewport_x == 80 && config.viewport_y == 16);
    CHECK(config.viewport_width == 320 && config.viewport_height == 240);
    CHECK(check_common_4_3(&config, 1.0f, 1.0f) == 0);

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_PSP_362X272));
    CHECK(config.output == N64PSP_DISPLAY_OUTPUT_PSP_LCD);
    CHECK(config.framebuffer_width == 480 && config.framebuffer_height == 272);
    CHECK(config.viewport_x == 59 && config.viewport_y == 0);
    CHECK(config.viewport_width == 362 && config.viewport_height == 272);
    CHECK(check_common_4_3(&config, 1.13125f, 272.0f / 240.0f) == 0);

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_PSP_480X272));
    CHECK(config.output == N64PSP_DISPLAY_OUTPUT_PSP_LCD);
    CHECK(config.framebuffer_width == 480 && config.framebuffer_height == 272);
    CHECK(config.viewport_x == 0 && config.viewport_y == 0);
    CHECK(config.viewport_width == 480 && config.viewport_height == 272);
    CHECK(config.ui_viewport_x == 59 && config.ui_viewport_y == 0);
    CHECK(config.ui_viewport_width == 362 && config.ui_viewport_height == 272);
    CHECK_CLOSE(config.display_aspect, 480.0f / 272.0f);
    CHECK_CLOSE(config.pixel_aspect, 1.0f);
    CHECK_CLOSE(config.logical_width, 423.5294118f);
    CHECK_CLOSE(config.logical_height, 240.0f);
    CHECK_CLOSE(config.scale_x, 272.0f / 240.0f);
    CHECK_CLOSE(config.scale_y, 272.0f / 240.0f);
    CHECK_CLOSE(config.side_extension, 51.7647059f);
    CHECK(config.anamorphic == 0);

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_TV_720X480_4_3));
    CHECK(config.output == N64PSP_DISPLAY_OUTPUT_TV);
    CHECK(config.framebuffer_width == 720 && config.framebuffer_height == 480);
    CHECK(config.viewport_x == 40 && config.viewport_y == 0);
    CHECK(config.viewport_width == 640 && config.viewport_height == 480);
    CHECK(check_common_4_3(&config, 2.0f, 2.0f) == 0);

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_TV_720X480_16_9));
    CHECK(config.output == N64PSP_DISPLAY_OUTPUT_TV);
    CHECK(config.framebuffer_width == 720 && config.framebuffer_height == 480);
    CHECK(config.viewport_x == 0 && config.viewport_y == 0);
    CHECK(config.viewport_width == 720 && config.viewport_height == 480);
    CHECK(config.ui_viewport_x == 40 && config.ui_viewport_y == 0);
    CHECK(config.ui_viewport_width == 640 && config.ui_viewport_height == 480);
    CHECK_CLOSE(config.display_aspect, 16.0f / 9.0f);
    CHECK_CLOSE(config.pixel_aspect, 32.0f / 27.0f);
    CHECK_CLOSE(config.logical_width, 426.6666667f);
    CHECK_CLOSE(config.logical_height, 240.0f);
    CHECK_CLOSE(config.scale_x, 1.6875f);
    CHECK_CLOSE(config.scale_y, 2.0f);
    CHECK_CLOSE(config.side_extension, 53.3333333f);
    CHECK(config.anamorphic == 1);

    CHECK(!n64psp_display_configure(0, N64PSP_DISPLAY_PSP_320X240));
    CHECK(!n64psp_display_configure(&config, (n64psp_display_mode)N64PSP_DISPLAY_MODE_COUNT));
    puts("display tests passed");
    return 0;
}
