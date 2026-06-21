#include "n64psp/lighting.h"

#include "../src/math/lighting_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (!(expr)) {                                                     \
            fprintf(stderr, "check failed: %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expr);                            \
            return 1;                                                      \
        }                                                                  \
    } while (0)

enum {
    MAX_LIGHTS = 7,
    MAX_NORMALS = 64,
    CANARY_WORDS = 4
};

typedef struct N64PSP_ALIGN16 guarded_lighting_output {
    uint32_t before[CANARY_WORDS];
    n64psp_vec4f values[MAX_NORMALS];
    uint32_t after[CANARY_WORDS];
} guarded_lighting_output;

typedef struct N64PSP_ALIGN16 guarded_normals {
    uint32_t before[CANARY_WORDS];
    n64psp_snorm8x4 values[MAX_NORMALS];
    uint32_t after[CANARY_WORDS];
} guarded_normals;

static float absf_local(float value) {
    return value < 0.0f ? -value : value;
}

static int nearly_equal(float actual, float expected) {
    const float difference = absf_local(actual - expected);
    const float scale = absf_local(expected);

    return difference <= 1.0e-5f + 1.0e-5f * scale;
}

static n64psp_mat4f matrix_from_rows(const float rows[4][4]) {
    n64psp_mat4f result;
    size_t row;
    size_t column;

    for (row = 0u; row < 4u; ++row) {
        for (column = 0u; column < 4u; ++column) {
            result.m[column][row] = rows[row][column];
        }
    }

    return result;
}

static n64psp_mat4f identity_matrix(void) {
    static const float rows[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    return matrix_from_rows(rows);
}

static void reference_light(
    n64psp_vec4f* out,
    const n64psp_mat4f* normal_matrix,
    const n64psp_snorm8x4* normal,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count
) {
    float nx = (float)normal->x;
    float ny = (float)normal->y;
    float nz = (float)normal->z;
    float tx =
        (normal_matrix->m[0][0] * nx) +
        (normal_matrix->m[1][0] * ny) +
        (normal_matrix->m[2][0] * nz);
    float ty =
        (normal_matrix->m[0][1] * nx) +
        (normal_matrix->m[1][1] * ny) +
        (normal_matrix->m[2][1] * nz);
    float tz =
        (normal_matrix->m[0][2] * nx) +
        (normal_matrix->m[1][2] * ny) +
        (normal_matrix->m[2][2] * nz);
    float length_squared = (tx * tx) + (ty * ty) + (tz * tz);
    size_t light_index;

    out->x = ambient->x;
    out->y = ambient->y;
    out->z = ambient->z;
    out->w = 0.0f;

    if (length_squared > 0.000001f) {
        float inverse_length = 1.0f / sqrtf(length_squared);

        tx *= inverse_length;
        ty *= inverse_length;
        tz *= inverse_length;
    }

    for (light_index = 0u; light_index < light_count; ++light_index) {
        float dot =
            (tx * lights[light_index].direction.x) +
            (ty * lights[light_index].direction.y) +
            (tz * lights[light_index].direction.z);

        if (dot > 0.0f) {
            out->x += lights[light_index].color.x * dot;
            out->y += lights[light_index].color.y * dot;
            out->z += lights[light_index].color.z * dot;
        }
    }
}

static int vector_equal(
    const n64psp_vec4f* actual,
    const n64psp_vec4f* expected
) {
    if (!nearly_equal(actual->x, expected->x) ||
        !nearly_equal(actual->y, expected->y) ||
        !nearly_equal(actual->z, expected->z) ||
        !nearly_equal(actual->w, expected->w)) {
        fprintf(
            stderr,
            "lighting mismatch actual={%g,%g,%g,%g} expected={%g,%g,%g,%g}\n",
            (double)actual->x,
            (double)actual->y,
            (double)actual->z,
            (double)actual->w,
            (double)expected->x,
            (double)expected->y,
            (double)expected->z,
            (double)expected->w
        );
        return 0;
    }

    return 1;
}

static void fill_canary(uint32_t* words) {
    size_t index;

    for (index = 0u; index < CANARY_WORDS; ++index) {
        words[index] = UINT32_C(0xfeed0000) + (uint32_t)index;
    }
}

static int check_canary(const uint32_t* words) {
    size_t index;

    for (index = 0u; index < CANARY_WORDS; ++index) {
        CHECK(words[index] == UINT32_C(0xfeed0000) + (uint32_t)index);
    }

    return 0;
}

static void setup_lights(n64psp_directional_lightf* lights) {
    size_t index;

    for (index = 0u; index < MAX_LIGHTS; ++index) {
        float x = (index & 1u) ? -0.5f : 0.5f;
        float y = (index & 2u) ? -0.25f : 0.75f;
        float z = (index & 4u) ? -0.8f : 0.4f;
        float length = sqrtf((x * x) + (y * y) + (z * z));

        lights[index].direction.x = x / length;
        lights[index].direction.y = y / length;
        lights[index].direction.z = z / length;
        lights[index].direction.w = 0.0f;
        lights[index].color.x = 32.0f + (float)index * 29.0f;
        lights[index].color.y = 64.0f + (float)index * 17.0f;
        lights[index].color.z = 96.0f + (float)index * 11.0f;
        lights[index].color.w = 0.0f;
    }
}

static int run_case(
    const char* name,
    const n64psp_mat4f* normal_matrix,
    size_t light_count,
    size_t count
) {
    guarded_normals normals;
    guarded_lighting_output output;
    n64psp_directional_lightf lights[MAX_LIGHTS];
    n64psp_vec4f ambient = {12.0f, 24.0f, 36.0f, 0.0f};
    size_t index;

    fill_canary(normals.before);
    fill_canary(normals.after);
    fill_canary(output.before);
    fill_canary(output.after);
    setup_lights(lights);

    for (index = 0u; index < MAX_NORMALS; ++index) {
        normals.values[index].x = (int8_t)((int)(index * 17u) % 255 - 127);
        normals.values[index].y = (int8_t)((int)(index * 31u) % 255 - 127);
        normals.values[index].z = (int8_t)((int)(index * 47u) % 255 - 127);
        normals.values[index].w = (int8_t)0x5a;
        output.values[index].x = -9999.0f;
        output.values[index].y = -9999.0f;
        output.values[index].z = -9999.0f;
        output.values[index].w = -9999.0f;
    }

    if (count >= 4u) {
        normals.values[0].x = 0;
        normals.values[0].y = 0;
        normals.values[0].z = 0;
        normals.values[1].x = 127;
        normals.values[1].y = 0;
        normals.values[1].z = 0;
        normals.values[2].x = 0;
        normals.values[2].y = 127;
        normals.values[2].z = 0;
        normals.values[3].x = 73;
        normals.values[3].y = 73;
        normals.values[3].z = 73;
    }

    n64psp_directional_light_snorm8_batch(
        output.values,
        normal_matrix,
        normals.values,
        light_count > 0u ? lights : NULL,
        &ambient,
        light_count,
        count
    );

    for (index = 0u; index < count; ++index) {
        n64psp_vec4f expected;

        reference_light(
            &expected,
            normal_matrix,
            &normals.values[index],
            lights,
            &ambient,
            light_count
        );
        if (!vector_equal(&output.values[index], &expected)) {
            fprintf(stderr, "case=%s index=%lu lights=%lu count=%lu\n",
                    name,
                    (unsigned long)index,
                    (unsigned long)light_count,
                    (unsigned long)count);
            return 1;
        }
    }

    CHECK(check_canary(normals.before) == 0);
    CHECK(check_canary(normals.after) == 0);
    CHECK(check_canary(output.before) == 0);
    CHECK(check_canary(output.after) == 0);

    return 0;
}

static int test_layout_and_zero_count(void) {
    CHECK(sizeof(n64psp_snorm8x4) == 4u);
    CHECK(sizeof(n64psp_directional_lightf) == 32u);

#if defined(__GNUC__) || defined(__clang__)
    CHECK(__alignof__(n64psp_directional_lightf) >= 16u);
#endif

    n64psp_directional_light_snorm8_batch(NULL, NULL, NULL, NULL, NULL, 0u, 0u);
    n64psp_directional_light_snorm8_batch_scalar(NULL, NULL, NULL, NULL, NULL, 0u, 0u);

    return 0;
}

int main(void) {
    static const float rotation_rows[4][4] = {
        {0.0f, -1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    static const float scale_rows[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, 0.0f, 0.0f},
        {0.0f, 0.0f, -3.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    const n64psp_mat4f identity = identity_matrix();
    const n64psp_mat4f rotation = matrix_from_rows(rotation_rows);
    const n64psp_mat4f scale = matrix_from_rows(scale_rows);
    static const size_t counts[] = {1u, 2u, 3u, 4u, 6u, 8u, 10u, 16u, 24u, 32u, 64u};
    static const size_t light_counts[] = {0u, 1u, 2u, 4u, 7u};
    size_t count_index;
    size_t light_index;

    CHECK(test_layout_and_zero_count() == 0);

    for (count_index = 0u; count_index < sizeof(counts) / sizeof(counts[0]); ++count_index) {
        for (light_index = 0u; light_index < sizeof(light_counts) / sizeof(light_counts[0]); ++light_index) {
            CHECK(run_case("identity", &identity, light_counts[light_index], counts[count_index]) == 0);
            CHECK(run_case("rotation", &rotation, light_counts[light_index], counts[count_index]) == 0);
            CHECK(run_case("scale", &scale, light_counts[light_index], counts[count_index]) == 0);
        }
    }

    puts("n64psp lighting tests: PASS");
    return 0;
}
