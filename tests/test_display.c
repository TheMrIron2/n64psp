#include "n64psp/display.h"
#include "n64psp/viewport.h"
#include "n64psp/hud.h"

#include <stdio.h>

#define CHECK(value) do { if (!(value)) return 1; } while (0)
#define CHECK_CLOSE(actual, expected) CHECK((actual) > (expected) - 0.0001f && (actual) < (expected) + 0.0001f)

static int check_rsp_viewports(void) {
    const float projection[4][4] = {
        { 1.5f, 0, 0, 0 }, { 0, 2.0f, 0, 0 },
        { 0, 0, -1.1f, -1 }, { 0, 0, -20.0f, 0 }
    };
    float out[4][4];
    int view, row, col;

    n64psp_viewport_project(out, projection, 640, 480, 640, 480, 320, 240);
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) CHECK_CLOSE(out[row][col], projection[row][col]);
    }
    for (view = 0; view < 4; view++) {
        float tx = (view & 1) ? 0.5f : -0.5f;
        float ty = (view & 2) ? -0.5f : 0.5f;
        int corner;

        n64psp_viewport_project(out, projection, 320, 240,
            (view & 1) ? 960 : 320, (view & 2) ? 720 : 240, 320, 240);
        for (corner = 0; corner < 4; corner++) {
            float x = (corner & 1) ? 2.0f : -2.0f;
            float y = (corner & 2) ? 1.5f : -1.5f;
            float z = -3.0f;
            float clip_x = x * projection[0][0];
            float clip_y = y * projection[1][1];
            float w = -z;

            CHECK_CLOSE((x * out[0][0] + z * out[2][0]) / w, clip_x / w * 0.5f + tx);
            CHECK_CLOSE((y * out[1][1] + z * out[2][1]) / w, clip_y / w * 0.5f + ty);
        }
        for (row = 0; row < 4; row++) {
            CHECK_CLOSE(out[row][2], projection[row][2]);
            CHECK_CLOSE(out[row][3], projection[row][3]);
        }
    }
    n64psp_viewport_project(out, projection, 0, 0, 320, 240, 320, 240);
    CHECK_CLOSE(out[0][0], 0);
    CHECK_CLOSE(out[1][1], 0);
    n64psp_viewport_project(out, projection, -320, -240, 960, 720, 320, 240);
    CHECK_CLOSE(out[0][0], -0.75f);
    CHECK_CLOSE(out[1][1], -1.0f);
    return 0;
}

static int check_quadrant_hud(void) {
    const float projection[4][4] = {
        { 1.5f, 0, 0, 0 }, { 0, 2.0f, 0, 0 },
        { 0, 0, -1.1f, -1 }, { 0, 0, -20.0f, 0 }
    };
    n64psp_display_config config;
    int mode, view, corner;

    for (mode = 0; mode < N64PSP_DISPLAY_PSP_MODE_COUNT; mode++) {
        float scale;
        CHECK(n64psp_display_configure(&config, (n64psp_display_mode) mode));
        scale = mode == N64PSP_DISPLAY_PSP_480X272 ? (4.0f / 3.0f) / config.display_aspect : 1.0f;
        for (view = 0; view < 4; view++) {
            float matrix[4][4];
            float anchor = 80.0f + (float) (view & 1) * 160.0f;
            float center_y = 60.0f + (float) (view >> 1) * 120.0f;
            float left = 20.0f + (float) (view & 1) * 160.0f;
            float top = 72.0f + (float) (view >> 1) * 120.0f;
            float fitted_left = n64psp_viewport_fit_x(left, anchor, scale);
            float fitted_right = n64psp_viewport_fit_x(left + 28.0f, anchor, scale);
            float width = (fitted_right - fitted_left) * (float) config.viewport_width / 320.0f;
            float height = 28.0f * (float) config.viewport_height / 240.0f;

            CHECK_CLOSE(n64psp_viewport_fit_x(anchor, anchor, scale), anchor);
            if (mode != N64PSP_DISPLAY_PSP_480X272) CHECK_CLOSE(fitted_left, left);
            if (mode == N64PSP_DISPLAY_PSP_480X272) CHECK_CLOSE(width, height);
            n64psp_viewport_project(matrix, projection, 320, 240,
                (view & 1) ? 960 : 320, (view & 2) ? 720 : 240, 320, 240);
            n64psp_viewport_fit_projection_x(matrix, anchor, 320, scale);
            for (corner = 0; corner < 5; corner++) {
                float x = left + (corner == 4 ? 14.0f : (float) (corner & 1) * 28.0f);
                float y = top + (corner == 4 ? 14.0f : (float) (corner >> 1) * 28.0f);
                float object_x = (x - anchor) / 80.0f * 3.0f / 1.5f;
                float object_y = (center_y - y) / 60.0f * 3.0f / 2.0f;
                float ndc_x = (object_x * matrix[0][0] - 3.0f * matrix[2][0]) / 3.0f;
                float ndc_y = (object_y * matrix[1][1] - 3.0f * matrix[2][1]) / 3.0f;

                CHECK_CLOSE((ndc_x + 1.0f) * 160.0f, n64psp_viewport_fit_x(x, anchor, scale));
                CHECK_CLOSE((1.0f - ndc_y) * 120.0f, y);
                CHECK_CLOSE(matrix[2][2], projection[2][2]);
                CHECK_CLOSE(matrix[2][3], projection[2][3]);
            }
        }
    }
    return 0;
}

static int check_widget_regions(void) {
    n64psp_display_config config;
    n64psp_hud_transform fit;
    int view, scaled;

    CHECK(n64psp_display_configure(&config, N64PSP_DISPLAY_PSP_480X272));
    n64psp_hud_region_fit(&fit, &config, 0, 0, 320, 240, 294, 28, N64PSP_HUD_RIGHT, 0);
    CHECK_CLOSE((294 * fit.scale_x + fit.offset_x) * 1.5f, 451);
    CHECK_CLOSE((28 * fit.scale_y + fit.offset_y) * 272.0f / 240.0f, 32);
    for (view = 0; view < 4; view++) {
        float left = (float) (view & 1) * 160;
        float top = (float) (view >> 1) * 120;
        for (scaled = 0; scaled < 2; scaled++) {
            float width = scaled ? (float) config.ui_viewport_width / 320.0f : 1.0f;
            float height = scaled ? (float) config.ui_viewport_height / 240.0f : 1.0f;
            n64psp_hud_region_fit(&fit, &config, left, top, 160, 120,
                left + 142, top + 16, N64PSP_HUD_RIGHT, scaled);
            CHECK_CLOSE(fit.scale_x * 1.5f, width);
            CHECK_CLOSE(fit.scale_y * 272.0f / 240.0f, height);
            CHECK_CLOSE(((left + 142) * fit.scale_x + fit.offset_x) * 1.5f,
                (float) (view & 1) * 240 + 220);
            CHECK_CLOSE(((top + 16) * fit.scale_y + fit.offset_y) * 272.0f / 240.0f,
                (float) (view >> 1) * 136 + 18);
            n64psp_hud_region_fit(&fit, &config, left, top, 160, 120,
                left + 140, top + 104, N64PSP_HUD_RIGHT | N64PSP_HUD_BOTTOM, scaled);
            CHECK_CLOSE(((left + 140) * fit.scale_x + fit.offset_x) * 1.5f,
                (float) (view & 1) * 240 + 217);
            CHECK_CLOSE(((top + 104) * fit.scale_y + fit.offset_y) * 272.0f / 240.0f,
                (float) (view >> 1) * 136 + 118);
        }
    }
    return 0;
}

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

static int check_ui_helpers(n64psp_display_config *config) {
    float x;

    CHECK(n64psp_display_configure(config, N64PSP_DISPLAY_PSP_320X240));
    CHECK_CLOSE(n64psp_ui_from_left(config, 79.0f), 79.0f);
    CHECK_CLOSE(n64psp_ui_from_right(config, 285.0f), 285.0f);
    CHECK_CLOSE(n64psp_ui_centered(160.0f), 160.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, 0.0f), 80.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_y(config, 0.0f), 16.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, 320.0f), 400.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_y(config, 240.0f), 256.0f);

    CHECK(n64psp_display_configure(config, N64PSP_DISPLAY_PSP_362X272));
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, 0.0f), 59.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_y(config, 0.0f), 0.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, 320.0f), 421.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_y(config, 240.0f), 272.0f);

    CHECK(n64psp_display_configure(config, N64PSP_DISPLAY_PSP_480X272));
    x = n64psp_ui_from_left(config, 79.0f);
    CHECK_CLOSE(x, 27.2352941f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, x), 89.5333333f);
    x = n64psp_ui_from_right(config, 285.0f);
    CHECK_CLOSE(x, 336.7647059f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, x), 440.3333333f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, n64psp_ui_from_left(config, 0.0f)), 0.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, n64psp_ui_from_right(config, 320.0f)), 480.0f);
    CHECK(n64psp_ui_left_edge(config, n64psp_ui_from_left(config, 0.0f)) == 0);
    CHECK(n64psp_ui_right_edge(config, n64psp_ui_from_right(config, 320.0f)) == 480);
    CHECK(n64psp_ui_pixel_x(config, 0.0f) == 59);
    CHECK(n64psp_ui_left_edge(config, 0.0f) == 58);
    CHECK(n64psp_ui_right_edge(config, 0.0f) == 59);
    CHECK(n64psp_ui_pixel_y(config, 1.0f) == 1);
    CHECK(n64psp_ui_top_edge(config, 1.0f) == 1);
    CHECK(n64psp_ui_bottom_edge(config, 1.0f) == 2);

    CHECK(n64psp_display_configure(config, N64PSP_DISPLAY_TV_720X480_16_9));
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, n64psp_ui_from_left(config, 0.0f)), 0.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, n64psp_ui_from_right(config, 320.0f)), 720.0f);
    CHECK(n64psp_ui_left_edge(config, n64psp_ui_from_left(config, 0.0f)) == 0);
    CHECK(n64psp_ui_right_edge(config, n64psp_ui_from_right(config, 320.0f)) == 720);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, n64psp_ui_centered(0.0f)), 90.0f);
    CHECK_CLOSE(n64psp_ui_to_framebuffer_x(config, n64psp_ui_centered(320.0f)), 630.0f);
    CHECK(n64psp_ui_pixel_x(config, 1.0f) == 92);
    CHECK(n64psp_ui_left_edge(config, 1.0f) == 91);
    CHECK(n64psp_ui_right_edge(config, 1.0f) == 92);
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
    CHECK(check_ui_helpers(&config) == 0);
    CHECK(check_rsp_viewports() == 0);
    CHECK(check_quadrant_hud() == 0);
    CHECK(check_widget_regions() == 0);
    puts("display tests passed");
    return 0;
}
