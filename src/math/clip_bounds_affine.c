#include <n64psp/math.h>
#include <math.h>

int n64psp_aabb_inside_clip_affine(const float bounds[2][3], const n64psp_mat4f* modelview,
                                   const n64psp_mat4f* projection, float inflation,
                                   float minimum_w, float relative_margin) {
    unsigned corner;
    if (modelview->m[0][3] != 0 || modelview->m[1][3] != 0 ||
        modelview->m[2][3] != 0 || modelview->m[3][3] != 1) return 0;
    for (corner = 0; corner < 8; corner++) {
        unsigned xHigh = corner & 1;
        unsigned yHigh = (corner >> 1) & 1;
        unsigned zHigh = (corner >> 2) & 1;
        float x = bounds[xHigh][0] + (xHigh ? inflation : -inflation);
        float y = bounds[yHigh][1] + (yHigh ? inflation : -inflation);
        float z = bounds[zHigh][2] + (zHigh ? inflation : -inflation);
        float viewX, viewY, viewZ;
        float clipX, clipY, clipZ, clipW, margin;
        viewX = modelview->m[0][0] * x + modelview->m[1][0] * y +
                modelview->m[2][0] * z + modelview->m[3][0];
        viewY = modelview->m[0][1] * x + modelview->m[1][1] * y +
                modelview->m[2][1] * z + modelview->m[3][1];
        viewZ = modelview->m[0][2] * x + modelview->m[1][2] * y +
                modelview->m[2][2] * z + modelview->m[3][2];
        clipX = projection->m[0][0] * viewX + projection->m[1][0] * viewY +
                projection->m[2][0] * viewZ + projection->m[3][0];
        clipY = projection->m[0][1] * viewX + projection->m[1][1] * viewY +
                projection->m[2][1] * viewZ + projection->m[3][1];
        clipZ = projection->m[0][2] * viewX + projection->m[1][2] * viewY +
                projection->m[2][2] * viewZ + projection->m[3][2];
        clipW = projection->m[0][3] * viewX + projection->m[1][3] * viewY +
                projection->m[2][3] * viewZ + projection->m[3][3];
        if (!(clipW > minimum_w) || !isfinite(clipW)) return 0;
        margin = clipW * relative_margin;
        if (!(clipX > -clipW + margin && clipX < clipW - margin &&
              clipY > -clipW + margin && clipY < clipW - margin &&
              clipZ > -clipW + margin && clipZ < clipW - margin)) return 0;
    }
    return 1;
}
