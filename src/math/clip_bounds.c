#include <n64psp/math.h>
#include <math.h>

int n64psp_aabb_inside_clip(const float bounds[2][3], const n64psp_mat4f* modelview,
                          const n64psp_mat4f* projection, float inflation,
                          float minimum_w, float relative_margin) {
    unsigned corner;
    if (modelview->m[0][3] != 0 || modelview->m[1][3] != 0 ||
        modelview->m[2][3] != 0 || modelview->m[3][3] != 1) return 0;
    for (corner = 0; corner < 8; corner++) {
        n64psp_vec4f point, view, clip;
        unsigned axis;
        float margin;
        float* coords = &point.x;
        for (axis = 0; axis < 3; axis++) {
            unsigned high = (corner >> axis) & 1;
            coords[axis] = bounds[high][axis] + (high ? inflation : -inflation);
        }
        point.w = 1;
        n64psp_mat4f_transform_vec4(&view, modelview, &point);
        n64psp_mat4f_transform_vec4(&clip, projection, &view);
        if (!(clip.w > minimum_w) || !isfinite(clip.w)) return 0;
        margin = clip.w * relative_margin;
        if (!(clip.x > -clip.w + margin && clip.x < clip.w - margin &&
              clip.y > -clip.w + margin && clip.y < clip.w - margin &&
              clip.z > -clip.w + margin && clip.z < clip.w - margin)) return 0;
    }
    return 1;
}
