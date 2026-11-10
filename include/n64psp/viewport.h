#ifndef N64PSP_VIEWPORT_H
#define N64PSP_VIEWPORT_H

// Apply the RSP quarter pixel viewport after projection
static inline void n64psp_viewport_project(float out[4][4], const float in[4][4],
                                          float scale_x, float scale_y, float trans_x, float trans_y,
                                          float width, float height) {
    float sx = scale_x / (width * 2.0f);
    float sy = scale_y / (height * 2.0f);
    float tx = trans_x / (width * 2.0f) - 1.0f;
    float ty = 1.0f - trans_y / (height * 2.0f);
    int i;

    for (i = 0; i < 4; i++) {
        out[i][0] = in[i][0] * sx + in[i][3] * tx;
        out[i][1] = in[i][1] * sy + in[i][3] * ty;
        out[i][2] = in[i][2];
        out[i][3] = in[i][3];
    }
}

static inline float n64psp_viewport_fit_x(float x, float anchor, float scale) {
    return anchor + (x - anchor) * scale;
}

// Fit projected geometry to the same canvas as screen rectangles
static inline void n64psp_viewport_fit_projection_x(float matrix[4][4], float anchor,
                                                   float width, float scale) {
    float offset = (anchor * 2.0f / width - 1.0f) * (1.0f - scale);
    int i;

    for (i = 0; i < 4; i++) matrix[i][0] = matrix[i][0] * scale + matrix[i][3] * offset;
}

#endif
