#include "math_smoke.h"

#include "../../src/math/lighting_internal.h"
#include "../../src/math/math_internal.h"

#include "n64psp/lighting.h"
#include "n64psp/math.h"
#include "n64psp/runtime.h"

#include <pspdebug.h>
#include <stdint.h>
#include <string.h>

#ifndef N64PSP_USE_VFPU
#define N64PSP_USE_VFPU 0
#endif

#ifndef N64PSP_PSP_BENCHMARKS
#define N64PSP_PSP_BENCHMARKS 0
#endif

#ifndef N64PSP_VFPU_TRANSFORM_EXPERIMENT
#define N64PSP_VFPU_TRANSFORM_EXPERIMENT 0
#endif

#ifndef N64PSP_GIT_COMMIT
#define N64PSP_GIT_COMMIT "unknown"
#endif

#ifndef N64PSP_PSP_OPT_FLAGS
#define N64PSP_PSP_OPT_FLAGS "unknown"
#endif

static volatile float math_benchmark_checksum;

enum {
    MATH_BATCH_MAX_COUNT = 64,
    LIGHTING_CANARY_WORDS = 4
};

typedef struct N64PSP_ALIGN16 lighting_normal_guard {
    uint32_t before[LIGHTING_CANARY_WORDS];
    n64psp_snorm8x4 values[MATH_BATCH_MAX_COUNT];
    uint32_t after[LIGHTING_CANARY_WORDS];
} lighting_normal_guard;

typedef struct N64PSP_ALIGN16 lighting_output_guard {
    uint32_t before[LIGHTING_CANARY_WORDS];
    n64psp_vec4f values[MATH_BATCH_MAX_COUNT];
    uint32_t after[LIGHTING_CANARY_WORDS];
} lighting_output_guard;

static n64psp_vec4f math_batch_input[MATH_BATCH_MAX_COUNT];
static n64psp_vec4f_pair math_batch_scalar[MATH_BATCH_MAX_COUNT];
static n64psp_vec4f_pair math_batch_selected[MATH_BATCH_MAX_COUNT];
static lighting_normal_guard lighting_normals_guard;
static lighting_output_guard lighting_scalar_guard;
static lighting_output_guard lighting_selected_guard;
static n64psp_directional_lightf lighting_lights[7];

#define lighting_normals lighting_normals_guard.values
#define lighting_scalar lighting_scalar_guard.values
#define lighting_selected lighting_selected_guard.values
#if defined(__PSP__) && N64PSP_USE_VFPU
static n64psp_vec4f_pair math_batch_baseline[MATH_BATCH_MAX_COUNT];
static n64psp_vec4f_pair math_batch_candidate[MATH_BATCH_MAX_COUNT];
#endif
#if N64PSP_PSP_BENCHMARKS
static n64psp_vec4f_pair math_batch_independent[MATH_BATCH_MAX_COUNT];
#endif

static uint32_t float_bits(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float absf_local(float value) {
    return value < 0.0f ? -value : value;
}

static int nearly_equal(float actual, float expected) {
    const float difference = absf_local(actual - expected);
    const float scale = absf_local(expected);

    return difference <= 1.0e-4f + 1.0e-4f * scale;
}

static n64psp_mat4f matrix_from_rows(const float rows[4][4]) {
    n64psp_mat4f result;
    unsigned int row;
    unsigned int column;

    for (row = 0; row < 4u; ++row) {
        for (column = 0; column < 4u; ++column) {
            result.m[column][row] = rows[row][column];
        }
    }

    return result;
}

static float vector_component(
    const n64psp_vec4f* vector,
    unsigned int component
) {
    switch (component) {
        case 0u:
            return vector->x;

        case 1u:
            return vector->y;

        case 2u:
            return vector->z;

        default:
            return vector->w;
    }
}

static const char* component_name(unsigned int component) {
    static const char* names[4] = {
        "x",
        "y",
        "z",
        "w",
    };

    return names[component];
}

static const char* selected_path_name(void) {
#if N64PSP_USE_VFPU
    return "chain2 VFPU";
#else
    return "chain2 scalar";
#endif
}

static const char* selected_lighting_path_name(void) {
#if N64PSP_USE_VFPU
    return "lighting PSP";
#else
    return "lighting scalar";
#endif
}

static int compare_batch_output(
    const n64psp_vec4f_pair* scalar,
    const n64psp_vec4f_pair* selected,
    unsigned int count,
    const char* operation
) {
    unsigned int index;
    unsigned int output_index;
    unsigned int component;

    for (index = 0; index < count; ++index) {
        for (output_index = 0; output_index < 2u; ++output_index) {
            const n64psp_vec4f* scalar_vector =
                output_index == 0u
                    ? &scalar[index].first
                    : &scalar[index].second;

            const n64psp_vec4f* selected_vector =
                output_index == 0u
                    ? &selected[index].first
                    : &selected[index].second;

            for (component = 0; component < 4u; ++component) {
                const float scalar_value =
                    vector_component(scalar_vector, component);

                const float selected_value =
                    vector_component(selected_vector, component);

                const float difference =
                    absf_local(scalar_value - selected_value);

                if (!nearly_equal(selected_value, scalar_value)) {
                    pspDebugScreenPrintf(
                        "%s mismatch vector=%u output=%s component=%s\n",
                        operation,
                        index,
                        output_index == 0u ? "first" : "second",
                        component_name(component)
                    );

                    pspDebugScreenPrintf(
                        " scalar=%g %s=%g absdiff=%g\n",
                        (double)scalar_value,
                        selected_path_name(),
                        (double)selected_value,
                        (double)difference
                    );

                    return 0;
                }
            }
        }
    }

    return 1;
}

static int compare_batch_against_single_transform(
    const n64psp_vec4f_pair* batch,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    unsigned int count,
    int chained
) {
    unsigned int index;
    unsigned int output_index;
    unsigned int component;

    for (index = 0; index < count; ++index) {
        n64psp_vec4f expected_first;
        n64psp_vec4f expected_second;

        n64psp_mat4f_transform_vec4_scalar(
            &expected_first,
            first_matrix,
            &input[index]
        );

        n64psp_mat4f_transform_vec4_scalar(
            &expected_second,
            second_matrix,
            chained ? &expected_first : &input[index]
        );

        for (output_index = 0; output_index < 2u; ++output_index) {
            const n64psp_vec4f* expected =
                output_index == 0u
                    ? &expected_first
                    : &expected_second;

            const n64psp_vec4f* actual =
                output_index == 0u
                    ? &batch[index].first
                    : &batch[index].second;

            for (component = 0; component < 4u; ++component) {
                const float expected_value =
                    vector_component(expected, component);

                const float actual_value =
                    vector_component(actual, component);

                const float difference =
                    absf_local(expected_value - actual_value);

                if (!nearly_equal(actual_value, expected_value)) {
                    pspDebugScreenPrintf(
                        "%s single mismatch vector=%u output=%s component=%s\n",
                        chained ? "chain2" : "independent",
                        index,
                        output_index == 0u ? "first" : "second",
                        component_name(component)
                    );

                    pspDebugScreenPrintf(
                        " scalar=%g batch=%g absdiff=%g\n",
                        (double)expected_value,
                        (double)actual_value,
                        (double)difference
                    );

                    return 0;
                }
            }
        }
    }

    return 1;
}

static void initialize_batch_input(void) {
    unsigned int index;

    for (index = 0; index < MATH_BATCH_MAX_COUNT; ++index) {
        math_batch_input[index].x =
            (float)index * 0.25f - 7.5f;

        math_batch_input[index].y =
            (float)index * -0.5f + 3.25f;

        math_batch_input[index].z =
            (float)(index % 9u) - 4.5f;

        math_batch_input[index].w =
            0.5f + (float)(index % 5u) * 0.25f;
    }
}

static int run_batch_correctness_case(
    const char* name,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix
) {
    static const unsigned int counts[] = {
        0u,
        1u,
        2u,
        3u,
        4u,
        8u,
        16u,
        31u,
        32u,
        63u,
        64u,
    };

    unsigned int count_index;

    for (
        count_index = 0;
        count_index < sizeof(counts) / sizeof(counts[0]);
        ++count_index
    ) {
        const unsigned int count = counts[count_index];

        if (count == 0u) {
            n64psp_mat4f_transform_vec4_2mat_batch_scalar(
                NULL,
                NULL,
                NULL,
                NULL,
                0
            );

            n64psp_mat4f_transform_vec4_2mat_batch(
                NULL,
                NULL,
                NULL,
                NULL,
                0
            );

            n64psp_mat4f_transform_vec4_chain2_batch_scalar(
                NULL,
                NULL,
                NULL,
                NULL,
                0
            );

            n64psp_mat4f_transform_vec4_chain2_batch(
                NULL,
                NULL,
                NULL,
                NULL,
                0
            );

            continue;
        }

        n64psp_mat4f_transform_vec4_2mat_batch_scalar(
            math_batch_scalar,
            first_matrix,
            second_matrix,
            math_batch_input,
            count
        );

        n64psp_mat4f_transform_vec4_2mat_batch(
            math_batch_selected,
            first_matrix,
            second_matrix,
            math_batch_input,
            count
        );

        if (!compare_batch_output(
                math_batch_scalar,
                math_batch_selected,
                count,
                "independent"
            )) {
            pspDebugScreenPrintf(
                "independent case failed: %s count=%u\n",
                name,
                count
            );
            return 1;
        }

        if (!compare_batch_against_single_transform(
                math_batch_selected,
                first_matrix,
                second_matrix,
                math_batch_input,
                count,
                0
            )) {
            pspDebugScreenPrintf(
                "independent reference failed: %s count=%u\n",
                name,
                count
            );
            return 1;
        }

        n64psp_mat4f_transform_vec4_chain2_batch_scalar(
            math_batch_scalar,
            first_matrix,
            second_matrix,
            math_batch_input,
            count
        );

        n64psp_mat4f_transform_vec4_chain2_batch(
            math_batch_selected,
            first_matrix,
            second_matrix,
            math_batch_input,
            count
        );

        if (!compare_batch_output(
                math_batch_scalar,
                math_batch_selected,
                count,
                "chain2"
            )) {
            pspDebugScreenPrintf(
                "chain2 case failed: %s count=%u\n",
                name,
                count
            );
            return 1;
        }

        if (!compare_batch_against_single_transform(
                math_batch_selected,
                first_matrix,
                second_matrix,
                math_batch_input,
                count,
                1
            )) {
            pspDebugScreenPrintf(
                "chain2 reference failed: %s count=%u\n",
                name,
                count
            );
            return 1;
        }

#if defined(__PSP__) && N64PSP_USE_VFPU
        n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline(
            math_batch_baseline,
            first_matrix,
            second_matrix,
            math_batch_input,
            count
        );

        n64psp_mat4f_transform_vec4_precompose_2mat_batch_vfpu(
            math_batch_candidate,
            first_matrix,
            second_matrix,
            math_batch_input,
            count
        );

        if (!compare_batch_output(
                math_batch_scalar,
                math_batch_baseline,
                count,
                "chain2 baseline"
            )) {
            pspDebugScreenPrintf(
                "chain2 baseline failed: %s count=%u\n",
                name,
                count
            );
            return 1;
        }

        if (!compare_batch_output(
                math_batch_scalar,
                math_batch_candidate,
                count,
                "precompose + independent VFPU"
            )) {
            pspDebugScreenPrintf(
                "precompose + independent VFPU failed: %s count=%u\n",
                name,
                count
            );
            return 1;
        }
#endif
    }

    pspDebugScreenPrintf("math batches %s: PASS\n", name);
    return 0;
}

static int run_chain_semantic_distinction(void) {
    static const float scale_rows[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 3.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 4.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    static const float translation_rows[4][4] = {
        {1.0f, 0.0f, 0.0f, 10.0f},
        {0.0f, 1.0f, 0.0f, 20.0f},
        {0.0f, 0.0f, 1.0f, 30.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    const n64psp_mat4f scale = matrix_from_rows(scale_rows);
    const n64psp_mat4f translation = matrix_from_rows(translation_rows);
    const n64psp_vec4f input = {1.0f, 2.0f, 3.0f, 1.0f};
    n64psp_vec4f_pair chained;
    n64psp_vec4f_pair independent;

    n64psp_mat4f_transform_vec4_chain2_batch(
        &chained,
        &scale,
        &translation,
        &input,
        1
    );

    n64psp_mat4f_transform_vec4_2mat_batch(
        &independent,
        &scale,
        &translation,
        &input,
        1
    );

    if (!nearly_equal(chained.first.x, 2.0f) ||
        !nearly_equal(chained.first.y, 6.0f) ||
        !nearly_equal(chained.first.z, 12.0f) ||
        !nearly_equal(chained.first.w, 1.0f) ||
        !nearly_equal(chained.second.x, 12.0f) ||
        !nearly_equal(chained.second.y, 26.0f) ||
        !nearly_equal(chained.second.z, 42.0f) ||
        !nearly_equal(chained.second.w, 1.0f) ||
        !nearly_equal(independent.second.x, 11.0f) ||
        !nearly_equal(independent.second.y, 22.0f) ||
        !nearly_equal(independent.second.z, 33.0f) ||
        !nearly_equal(independent.second.w, 1.0f)) {
        pspDebugScreenPrintf("chain2 semantic distinction: FAIL\n");
        return 1;
    }

    pspDebugScreenPrintf("chain2 semantic distinction: PASS\n");
    return 0;
}

static int run_batch_correctness(void) {
    static const float first_rows[4][4] = {
        {1.25f, -2.5f, 3.75f, 4.5f},
        {5.5f, 6.25f, -7.75f, 8.0f},
        {-9.5f, 10.0f, 11.25f, -12.5f},
        {13.75f, -14.25f, 15.5f, 16.0f},
    };

    static const float second_rows[4][4] = {
        {-3.5f, 2.25f, 1.5f, 0.75f},
        {4.25f, -5.5f, 6.75f, 7.25f},
        {8.5f, 9.25f, -10.75f, 11.5f},
        {12.25f, 13.5f, 14.75f, -15.25f},
    };

    const n64psp_mat4f first_matrix =
        matrix_from_rows(first_rows);

    const n64psp_mat4f second_matrix =
        matrix_from_rows(second_rows);

    initialize_batch_input();

    if (run_batch_correctness_case(
            "different matrices",
            &first_matrix,
            &second_matrix
        ) != 0) {
        return 1;
    }

    if (run_batch_correctness_case(
            "equal matrices",
            &first_matrix,
            &first_matrix
        ) != 0) {
        return 1;
    }

    if (run_chain_semantic_distinction() != 0) {
        return 1;
    }

    pspDebugScreenPrintf(
        "math batch selected path: %s\n",
        selected_path_name()
    );
    pspDebugScreenPrintf(
        "math git=%s vfpu=%d transform_experiment=%d opt=%s\n",
        N64PSP_GIT_COMMIT,
        N64PSP_USE_VFPU,
        N64PSP_VFPU_TRANSFORM_EXPERIMENT,
        N64PSP_PSP_OPT_FLAGS
    );

    return 0;
}

static void initialize_lighting_inputs(void) {
    unsigned int index;

    for (index = 0u; index < LIGHTING_CANARY_WORDS; ++index) {
        lighting_normals_guard.before[index] = 0x6c6e0000u + index;
        lighting_normals_guard.after[index] = 0x6c6e1000u + index;
        lighting_scalar_guard.before[index] = 0x6c730000u + index;
        lighting_scalar_guard.after[index] = 0x6c731000u + index;
        lighting_selected_guard.before[index] = 0x6c760000u + index;
        lighting_selected_guard.after[index] = 0x6c761000u + index;
    }

    for (index = 0u; index < MATH_BATCH_MAX_COUNT; ++index) {
        lighting_normals[index].x =
            (int8_t)((int)(index * 17u) % 255 - 127);
        lighting_normals[index].y =
            (int8_t)((int)(index * 31u) % 255 - 127);
        lighting_normals[index].z =
            (int8_t)((int)(index * 47u) % 255 - 127);
        lighting_normals[index].w = 0;
        lighting_scalar[index].x = -9999.0f;
        lighting_scalar[index].y = -9999.0f;
        lighting_scalar[index].z = -9999.0f;
        lighting_scalar[index].w = -9999.0f;
        lighting_selected[index] = lighting_scalar[index];
    }

    lighting_normals[0].x = 0;
    lighting_normals[0].y = 0;
    lighting_normals[0].z = 0;
    lighting_normals[1].x = 1;
    lighting_normals[1].y = 2;
    lighting_normals[1].z = 4;
    lighting_normals[2].x = 127;
    lighting_normals[2].y = 0;
    lighting_normals[2].z = 0;
    lighting_normals[3].x = 0;
    lighting_normals[3].y = 127;
    lighting_normals[3].z = 0;
    lighting_normals[4].x = 73;
    lighting_normals[4].y = 73;
    lighting_normals[4].z = 73;

    for (index = 0u; index < 7u; ++index) {
        static const float directions[7][3] = {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.5f, 0.75f, 0.4f},
            {-0.5f, -0.25f, -0.8f},
            {0.25f, -0.9f, 0.35f},
        };
        float x = directions[index][0];
        float y = directions[index][1];
        float z = directions[index][2];
        float length = x * x + y * y + z * z;
        float inverse_length = 1.0f;

        if (length > 0.000001f) {
            inverse_length = 1.0f / __builtin_sqrtf(length);
        }

        lighting_lights[index].direction.x = x * inverse_length;
        lighting_lights[index].direction.y = y * inverse_length;
        lighting_lights[index].direction.z = z * inverse_length;
        lighting_lights[index].direction.w = 0.0f;
        lighting_lights[index].color.x = 48.0f + (float)index * 37.0f;
        lighting_lights[index].color.y = 12.0f + (float)index * 53.0f;
        lighting_lights[index].color.z = 96.0f + (float)index * 71.0f;
        lighting_lights[index].color.w = 0.0f;
    }
}

static int check_lighting_canaries(void) {
    unsigned int index;

    for (index = 0u; index < LIGHTING_CANARY_WORDS; ++index) {
        if ((lighting_normals_guard.before[index] != 0x6c6e0000u + index) ||
            (lighting_normals_guard.after[index] != 0x6c6e1000u + index) ||
            (lighting_scalar_guard.before[index] != 0x6c730000u + index) ||
            (lighting_scalar_guard.after[index] != 0x6c731000u + index) ||
            (lighting_selected_guard.before[index] != 0x6c760000u + index) ||
            (lighting_selected_guard.after[index] != 0x6c761000u + index)) {
            pspDebugScreenPrintf("lighting canary mismatch index=%u\n", index);
            return 0;
        }
    }

    return 1;
}

static int compare_lighting_output(
    const n64psp_vec4f* scalar,
    const n64psp_vec4f* selected,
    unsigned int count,
    unsigned int light_count
) {
    unsigned int index;
    unsigned int component;

    for (index = 0u; index < count; ++index) {
        for (component = 0u; component < 4u; ++component) {
            const float scalar_value =
                vector_component(&scalar[index], component);
            const float selected_value =
                vector_component(&selected[index], component);

            if (!nearly_equal(selected_value, scalar_value)) {
                pspDebugScreenPrintf(
                    "lighting mismatch case=batched vertex=%u lights=%u component=%s\n",
                    index,
                    light_count,
                    component_name(component)
                );
                pspDebugScreenPrintf(
                    " normal=%d,%d,%d scalar=%g vfpu=%g absdiff=%g\n",
                    (int)lighting_normals[index].x,
                    (int)lighting_normals[index].y,
                    (int)lighting_normals[index].z,
                    (double)scalar_value,
                    (double)selected_value,
                    (double)absf_local(scalar_value - selected_value)
                );
                return 0;
            }
        }
    }

    return 1;
}

static int run_lighting_correctness(void) {
    static const unsigned int counts[] = {
        0u, 1u, 2u, 3u, 4u, 6u, 8u, 10u, 16u, 24u, 32u, 64u,
    };
    static const unsigned int light_counts[] = {
        0u, 1u, 2u, 4u, 7u,
    };
    static const float identity_rows[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
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
    static const float tiny_rows[4][4] = {
        {0.00000001f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.00000001f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.00000001f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    const n64psp_mat4f matrices[] = {
        matrix_from_rows(identity_rows),
        matrix_from_rows(rotation_rows),
        matrix_from_rows(scale_rows),
        matrix_from_rows(tiny_rows),
    };
    const n64psp_vec4f ambient = {12.0f, 24.0f, 36.0f, 0.0f};
    unsigned int matrix_index;
    unsigned int count_index;
    unsigned int light_count_index;

    initialize_lighting_inputs();

    n64psp_directional_light_snorm8_batch_scalar(
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0
    );
    n64psp_directional_light_snorm8_batch(
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0
    );

    for (
        matrix_index = 0u;
        matrix_index < sizeof(matrices) / sizeof(matrices[0]);
        ++matrix_index
    ) {
        for (
            count_index = 0u;
            count_index < sizeof(counts) / sizeof(counts[0]);
            ++count_index
        ) {
            for (
                light_count_index = 0u;
                light_count_index < sizeof(light_counts) / sizeof(light_counts[0]);
                ++light_count_index
            ) {
                const unsigned int count = counts[count_index];
                const unsigned int light_count = light_counts[light_count_index];

                if (count == 0u) {
                    continue;
                }

                n64psp_directional_light_snorm8_batch_scalar(
                    lighting_scalar,
                    &matrices[matrix_index],
                    lighting_normals,
                    light_count != 0u ? lighting_lights : NULL,
                    &ambient,
                    light_count,
                    count
                );
#if defined(__PSP__) && N64PSP_USE_VFPU
                n64psp_directional_light_snorm8_batch_vfpu(
                    lighting_selected,
                    &matrices[matrix_index],
                    lighting_normals,
                    light_count != 0u ? lighting_lights : NULL,
                    &ambient,
                    light_count,
                    count
                );
#else
                n64psp_directional_light_snorm8_batch(
                    lighting_selected,
                    &matrices[matrix_index],
                    lighting_normals,
                    light_count != 0u ? lighting_lights : NULL,
                    &ambient,
                    light_count,
                    count
                );
#endif

                if (!compare_lighting_output(
                        lighting_scalar,
                        lighting_selected,
                        count,
                        light_count
                    ) ||
                    !check_lighting_canaries()) {
                    pspDebugScreenPrintf(
                        "lighting case matrix=%u count=%u lights=%u\n",
                        matrix_index,
                        count,
                        light_count
                    );
                    return 1;
                }
            }
        }
    }

    pspDebugScreenPrintf(
        "lighting batches selected path: %s\n",
        selected_lighting_path_name()
    );
    return 0;
}

static int matrix_equal(
    const n64psp_mat4f* actual,
    const n64psp_mat4f* expected,
    float* out_max_difference
) {
    float max_difference = 0.0f;
    unsigned int column;
    unsigned int row;

    for (column = 0; column < 4u; ++column) {
        for (row = 0; row < 4u; ++row) {
            const float actual_value = actual->m[column][row];
            const float expected_value = expected->m[column][row];
            const float difference =
                absf_local(actual_value - expected_value);

            if (difference > max_difference) {
                max_difference = difference;
            }

            if (!nearly_equal(actual_value, expected_value)) {
                pspDebugScreenPrintf(
                    "math mismatch c=%u r=%u\n",
                    column,
                    row
                );
                pspDebugScreenPrintf(
                    " actual_bits=%08lx expected_bits=%08lx\n",
                    (unsigned long)float_bits(actual_value),
                    (unsigned long)float_bits(expected_value)
                );

                if (out_max_difference != NULL) {
                    *out_max_difference = max_difference;
                }

                return 0;
            }
        }
    }

    if (out_max_difference != NULL) {
        *out_max_difference = max_difference;
    }

    return 1;
}

static int run_math_correctness(void) {
    static const float a_rows[4][4] = {
        {1.0f, -2.0f, 3.5f, 4.0f},
        {5.25f, 6.0f, -7.0f, 8.0f},
        {-9.0f, 10.5f, 11.0f, -12.0f},
        {13.0f, -14.0f, 15.0f, 16.5f},
    };

    static const float b_rows[4][4] = {
        {-3.0f, 2.0f, 1.0f, 0.5f},
        {4.0f, -5.0f, 6.0f, 7.0f},
        {8.0f, 9.0f, -10.0f, 11.0f},
        {12.0f, 13.0f, 14.0f, -15.0f},
    };

    const n64psp_mat4f a = matrix_from_rows(a_rows);
    const n64psp_mat4f b = matrix_from_rows(b_rows);

    n64psp_mat4f scalar;
    n64psp_mat4f selected;
    float max_difference = 0.0f;

    n64psp_mat4f_mul_scalar(&scalar, &a, &b);
    n64psp_mat4f_mul(&selected, &a, &b);

    if (!matrix_equal(&selected, &scalar, &max_difference)) {
        pspDebugScreenPrintf("math normal-output comparison failed\n");
        return 1;
    }

    selected = a;
    n64psp_mat4f_mul(&selected, &selected, &b);

    if (!matrix_equal(&selected, &scalar, &max_difference)) {
        pspDebugScreenPrintf("math out==a comparison failed\n");
        return 1;
    }

    selected = b;
    n64psp_mat4f_mul(&selected, &a, &selected);

    if (!matrix_equal(&selected, &scalar, &max_difference)) {
        pspDebugScreenPrintf("math out==b comparison failed\n");
        return 1;
    }

    pspDebugScreenPrintf("math mat4 multiply: PASS\n");
    pspDebugScreenPrintf(
        "math selected path: %s\n",
        selected_path_name()
    );

    return 0;
}

#if N64PSP_PSP_BENCHMARKS

static uint32_t batch_benchmark_repetitions(unsigned int count) {
    const uint32_t target_vertices = 1048576u;
    uint32_t repetitions = target_vertices / (uint32_t)count;

    if (repetitions < 4096u) {
        repetitions = 4096u;
    }

    return repetitions;
}

static void consume_batch(
    const n64psp_vec4f_pair* output,
    unsigned int count
) {
    math_benchmark_checksum +=
        output[count - 1u].first.x +
        output[count - 1u].second.w;
}

static uint64_t nanoseconds_per_batch(
    n64psp_time_us elapsed,
    uint32_t repetitions
) {
    return
        ((uint64_t)elapsed * 1000ULL) /
        (uint64_t)repetitions;
}

static uint64_t nanoseconds_per_vertex(
    n64psp_time_us elapsed,
    uint32_t repetitions,
    unsigned int count
) {
    return
        ((uint64_t)elapsed * 1000ULL) /
        ((uint64_t)repetitions * (uint64_t)count);
}

static void print_batch_measurement(
    const char* name,
    n64psp_time_us elapsed,
    uint32_t repetitions,
    unsigned int count
) {
    pspDebugScreenPrintf(
        " %-18s total=%llu us ns/batch=%llu ns/vertex=%llu\n",
        name,
        (unsigned long long)elapsed,
        (unsigned long long)nanoseconds_per_batch(elapsed, repetitions),
        (unsigned long long)nanoseconds_per_vertex(
            elapsed,
            repetitions,
            count
        )
    );
}

static void print_ratio(
    const char* name,
    n64psp_time_us numerator,
    n64psp_time_us denominator
) {
    uint64_t thousandths = 0ULL;

    if (denominator != 0) {
        thousandths =
            ((uint64_t)numerator * 1000ULL) /
            (uint64_t)denominator;
    }

    pspDebugScreenPrintf(
        " %-18s %llu.%03llux\n",
        name,
        (unsigned long long)(thousandths / 1000ULL),
        (unsigned long long)(thousandths % 1000ULL)
    );
}

static void batch_benchmark_warmup(
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix
) {
    n64psp_mat4f combined;
    unsigned int iteration;

    for (iteration = 0; iteration < 64u; ++iteration) {
        n64psp_mat4f_transform_vec4_chain2_batch_scalar(
            math_batch_scalar,
            first_matrix,
            second_matrix,
            math_batch_input,
            MATH_BATCH_MAX_COUNT
        );

        n64psp_mat4f_transform_vec4_chain2_batch(
            math_batch_selected,
            first_matrix,
            second_matrix,
            math_batch_input,
            MATH_BATCH_MAX_COUNT
        );

#if defined(__PSP__) && N64PSP_USE_VFPU
        n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline(
            math_batch_baseline,
            first_matrix,
            second_matrix,
            math_batch_input,
            MATH_BATCH_MAX_COUNT
        );

        n64psp_mat4f_transform_vec4_precompose_2mat_batch_vfpu(
            math_batch_candidate,
            first_matrix,
            second_matrix,
            math_batch_input,
            MATH_BATCH_MAX_COUNT
        );
#endif

        n64psp_mat4f_transform_vec4_2mat_batch(
            math_batch_independent,
            first_matrix,
            second_matrix,
            math_batch_input,
            MATH_BATCH_MAX_COUNT
        );

        n64psp_mat4f_mul(&combined, second_matrix, first_matrix);

        n64psp_mat4f_transform_vec4_2mat_batch(
            math_batch_independent,
            first_matrix,
            &combined,
            math_batch_input,
            MATH_BATCH_MAX_COUNT
        );
    }

    consume_batch(math_batch_scalar, MATH_BATCH_MAX_COUNT);
    consume_batch(math_batch_selected, MATH_BATCH_MAX_COUNT);
#if defined(__PSP__) && N64PSP_USE_VFPU
    consume_batch(math_batch_baseline, MATH_BATCH_MAX_COUNT);
    consume_batch(math_batch_candidate, MATH_BATCH_MAX_COUNT);
#endif
    consume_batch(math_batch_independent, MATH_BATCH_MAX_COUNT);
}

static int run_batch_benchmark(void) {
    static const unsigned int counts[] = {
        1u,
        2u,
        3u,
        4u,
        8u,
        16u,
        31u,
        32u,
        63u,
        64u,
    };

    static const float first_rows[4][4] = {
        {1.25f, -2.5f, 3.75f, 4.5f},
        {5.5f, 6.25f, -7.75f, 8.0f},
        {-9.5f, 10.0f, 11.25f, -12.5f},
        {13.75f, -14.25f, 15.5f, 16.0f},
    };

    static const float second_rows[4][4] = {
        {-3.5f, 2.25f, 1.5f, 0.75f},
        {4.25f, -5.5f, 6.75f, 7.25f},
        {8.5f, 9.25f, -10.75f, 11.5f},
        {12.25f, 13.5f, 14.75f, -15.25f},
    };

    const n64psp_mat4f first_matrix =
        matrix_from_rows(first_rows);

    const n64psp_mat4f second_matrix =
        matrix_from_rows(second_rows);

    unsigned int count_index;

    initialize_batch_input();
    batch_benchmark_warmup(&first_matrix, &second_matrix);

    pspDebugScreenPrintf(
        "transform benchmark git=%s vfpu=%d experiment=%d opt=%s selected=%s\n",
        N64PSP_GIT_COMMIT,
        N64PSP_USE_VFPU,
        N64PSP_VFPU_TRANSFORM_EXPERIMENT,
        N64PSP_PSP_OPT_FLAGS,
        selected_path_name()
    );

    for (
        count_index = 0;
        count_index < sizeof(counts) / sizeof(counts[0]);
        ++count_index
    ) {
        const unsigned int count = counts[count_index];
        const uint32_t repetitions =
            batch_benchmark_repetitions(count);

        n64psp_time_us scalar_start;
        n64psp_time_us scalar_end;
        n64psp_time_us chain_start;
        n64psp_time_us chain_end;
        n64psp_time_us baseline_start;
        n64psp_time_us baseline_end;
        n64psp_time_us candidate_start;
        n64psp_time_us candidate_end;
        n64psp_time_us independent_start;
        n64psp_time_us independent_end;
        n64psp_time_us compound_start;
        n64psp_time_us compound_end;
        uint32_t iteration;

        scalar_start = n64psp_time_monotonic_us();

        for (iteration = 0; iteration < repetitions; ++iteration) {
            n64psp_mat4f_transform_vec4_chain2_batch_scalar(
                math_batch_scalar,
                &first_matrix,
                &second_matrix,
                math_batch_input,
                count
            );
        }

        scalar_end = n64psp_time_monotonic_us();
        consume_batch(math_batch_scalar, count);

        chain_start = n64psp_time_monotonic_us();

        for (iteration = 0; iteration < repetitions; ++iteration) {
            n64psp_mat4f_transform_vec4_chain2_batch(
                math_batch_selected,
                &first_matrix,
                &second_matrix,
                math_batch_input,
                count
            );
        }

        chain_end = n64psp_time_monotonic_us();
        consume_batch(math_batch_selected, count);

#if defined(__PSP__) && N64PSP_USE_VFPU
        baseline_start = n64psp_time_monotonic_us();

        for (iteration = 0; iteration < repetitions; ++iteration) {
            n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline(
                math_batch_baseline,
                &first_matrix,
                &second_matrix,
                math_batch_input,
                count
            );
        }

        baseline_end = n64psp_time_monotonic_us();
        consume_batch(math_batch_baseline, count);

        candidate_start = n64psp_time_monotonic_us();

        for (iteration = 0; iteration < repetitions; ++iteration) {
            n64psp_mat4f_transform_vec4_precompose_2mat_batch_vfpu(
                math_batch_candidate,
                &first_matrix,
                &second_matrix,
                math_batch_input,
                count
            );
        }

        candidate_end = n64psp_time_monotonic_us();
        consume_batch(math_batch_candidate, count);
#else
        baseline_start = chain_start;
        baseline_end = chain_end;
        candidate_start = chain_start;
        candidate_end = chain_end;
#endif

        independent_start = n64psp_time_monotonic_us();

        for (iteration = 0; iteration < repetitions; ++iteration) {
            n64psp_mat4f_transform_vec4_2mat_batch(
                math_batch_independent,
                &first_matrix,
                &second_matrix,
                math_batch_input,
                count
            );
        }

        independent_end = n64psp_time_monotonic_us();
        consume_batch(math_batch_independent, count);

        compound_start = n64psp_time_monotonic_us();

        for (iteration = 0; iteration < repetitions; ++iteration) {
            n64psp_mat4f combined;

            n64psp_mat4f_mul(
                &combined,
                &second_matrix,
                &first_matrix
            );

            n64psp_mat4f_transform_vec4_2mat_batch(
                math_batch_independent,
                &first_matrix,
                &combined,
                math_batch_input,
                count
            );
        }

        compound_end = n64psp_time_monotonic_us();
        consume_batch(math_batch_independent, count);

        pspDebugScreenPrintf(
            "batch count=%u reps=%lu\n",
            count,
            (unsigned long)repetitions
        );

        print_batch_measurement(
            "chain2 scalar",
            scalar_end - scalar_start,
            repetitions,
            count
        );

        print_batch_measurement(
            selected_path_name(),
            chain_end - chain_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "chain2 VFPU",
            baseline_end - baseline_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "precompose + independent VFPU",
            candidate_end - candidate_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "independent VFPU",
            independent_end - independent_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "precompose+independent",
            compound_end - compound_start,
            repetitions,
            count
        );

        print_ratio(
            "chain2 scalar / chain2 VFPU",
            scalar_end - scalar_start,
            chain_end - chain_start
        );

        print_ratio(
            "independent / chain2 VFPU",
            independent_end - independent_start,
            chain_end - chain_start
        );

        print_ratio(
            "precompose+independent / chain2 VFPU",
            candidate_end - candidate_start,
            chain_end - chain_start
        );
    }

    return 0;
}

static void consume_lighting_output(
    const n64psp_vec4f* output,
    unsigned int count
) {
    math_benchmark_checksum +=
        output[count - 1u].x +
        output[count - 1u].y +
        output[count - 1u].z +
        output[count - 1u].w;
}

static void lighting_benchmark_warmup(
    const n64psp_mat4f* normal_matrix,
    const n64psp_vec4f* ambient
) {
    unsigned int iteration;

    for (iteration = 0u; iteration < 64u; ++iteration) {
        n64psp_directional_light_snorm8_batch_scalar(
            lighting_scalar,
            normal_matrix,
            lighting_normals,
            lighting_lights,
            ambient,
            7u,
            MATH_BATCH_MAX_COUNT
        );

#if defined(__PSP__) && N64PSP_USE_VFPU
        n64psp_directional_light_snorm8_batch_vfpu(
            lighting_selected,
            normal_matrix,
            lighting_normals,
            lighting_lights,
            ambient,
            7u,
            MATH_BATCH_MAX_COUNT
        );
#else
        n64psp_directional_light_snorm8_batch(
            lighting_selected,
            normal_matrix,
            lighting_normals,
            lighting_lights,
            ambient,
            7u,
            MATH_BATCH_MAX_COUNT
        );
#endif
    }

    consume_lighting_output(lighting_scalar, MATH_BATCH_MAX_COUNT);
    consume_lighting_output(lighting_selected, MATH_BATCH_MAX_COUNT);
}

static int run_lighting_benchmark(void) {
    static const unsigned int counts[] = {
        1u, 2u, 3u, 4u, 6u, 8u, 10u, 16u, 24u, 32u,
    };
    static const unsigned int light_counts[] = {
        0u, 1u, 2u, 4u, 7u,
    };
    static const float normal_rows[4][4] = {
        {0.25f, -1.0f, 0.5f, 0.0f},
        {1.5f, 0.5f, -0.25f, 0.0f},
        {-0.75f, 0.25f, 1.25f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    const n64psp_mat4f normal_matrix = matrix_from_rows(normal_rows);
    const n64psp_vec4f ambient = {12.0f, 24.0f, 36.0f, 0.0f};
    unsigned int count_index;
    unsigned int light_count_index;

    initialize_lighting_inputs();
    lighting_benchmark_warmup(&normal_matrix, &ambient);

    pspDebugScreenPrintf(
        "lighting benchmark git=%s vfpu=%d opt=%s selected=%s\n",
        N64PSP_GIT_COMMIT,
        N64PSP_USE_VFPU,
        N64PSP_PSP_OPT_FLAGS,
        selected_lighting_path_name()
    );

    for (
        count_index = 0u;
        count_index < sizeof(counts) / sizeof(counts[0]);
        ++count_index
    ) {
        const unsigned int count = counts[count_index];
        const uint32_t repetitions =
            batch_benchmark_repetitions(count);

        for (
            light_count_index = 0u;
            light_count_index < sizeof(light_counts) / sizeof(light_counts[0]);
            ++light_count_index
        ) {
            const unsigned int light_count = light_counts[light_count_index];
            const n64psp_directional_lightf* light_ptr =
                light_count != 0u ? lighting_lights : NULL;
            n64psp_time_us scalar_start;
            n64psp_time_us scalar_end;
            n64psp_time_us vfpu_start;
            n64psp_time_us vfpu_end;
            uint32_t iteration;

            scalar_start = n64psp_time_monotonic_us();

            for (iteration = 0u; iteration < repetitions; ++iteration) {
                n64psp_directional_light_snorm8_batch_scalar(
                    lighting_scalar,
                    &normal_matrix,
                    lighting_normals,
                    light_ptr,
                    &ambient,
                    light_count,
                    count
                );
            }

            scalar_end = n64psp_time_monotonic_us();
            consume_lighting_output(lighting_scalar, count);

            vfpu_start = n64psp_time_monotonic_us();

            for (iteration = 0u; iteration < repetitions; ++iteration) {
#if defined(__PSP__) && N64PSP_USE_VFPU
                n64psp_directional_light_snorm8_batch_vfpu(
                    lighting_selected,
                    &normal_matrix,
                    lighting_normals,
                    light_ptr,
                    &ambient,
                    light_count,
                    count
                );
#else
                n64psp_directional_light_snorm8_batch(
                    lighting_selected,
                    &normal_matrix,
                    lighting_normals,
                    light_ptr,
                    &ambient,
                    light_count,
                    count
                );
#endif
            }

            vfpu_end = n64psp_time_monotonic_us();
            consume_lighting_output(lighting_selected, count);

            pspDebugScreenPrintf(
                "lighting count=%u lights=%u reps=%lu\n",
                count,
                light_count,
                (unsigned long)repetitions
            );

            print_batch_measurement(
                "lighting scalar",
                scalar_end - scalar_start,
                repetitions,
                count
            );

            print_batch_measurement(
                selected_lighting_path_name(),
                vfpu_end - vfpu_start,
                repetitions,
                count
            );

            print_ratio(
                "lighting scalar / VFPU",
                scalar_end - scalar_start,
                vfpu_end - vfpu_start
            );
        }
    }

    return 0;
}

static void benchmark_warmup(
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
) {
    n64psp_mat4f result;
    int iteration;

    for (iteration = 0; iteration < 64; ++iteration) {
        n64psp_mat4f_mul_scalar(&result, a, b);
        n64psp_mat4f_mul(&result, a, b);
    }

    math_benchmark_checksum +=
        result.m[0][0] +
        result.m[1][1] +
        result.m[2][2] +
        result.m[3][3];
}

static void print_benchmark_result(
    const char* name,
    uint64_t iterations,
    n64psp_time_us elapsed
) {
    const uint64_t nanoseconds_per_operation =
        elapsed != 0
            ? ((uint64_t)elapsed * 1000ULL) / iterations
            : 0ULL;

    pspDebugScreenPrintf(
        "%s: %llu us, %llu ns/op\n",
        name,
        (unsigned long long)elapsed,
        (unsigned long long)nanoseconds_per_operation
    );
}

static int run_math_benchmark(void) {
    enum {
        ITERATIONS = 200000
    };

    static const float a_rows[4][4] = {
        {1.25f, -2.5f, 3.75f, 4.5f},
        {5.5f, 6.25f, -7.75f, 8.0f},
        {-9.5f, 10.0f, 11.25f, -12.5f},
        {13.75f, -14.25f, 15.5f, 16.0f},
    };

    static const float b_rows[4][4] = {
        {-3.5f, 2.25f, 1.5f, 0.75f},
        {4.25f, -5.5f, 6.75f, 7.25f},
        {8.5f, 9.25f, -10.75f, 11.5f},
        {12.25f, 13.5f, 14.75f, -15.25f},
    };

    const n64psp_mat4f a = matrix_from_rows(a_rows);
    const n64psp_mat4f b = matrix_from_rows(b_rows);
    n64psp_mat4f result;
    n64psp_time_us scalar_start;
    n64psp_time_us scalar_end;
    n64psp_time_us selected_start;
    n64psp_time_us selected_end;
    uint64_t speedup_thousandths = 0ULL;
    int iteration;

    benchmark_warmup(&a, &b);

    scalar_start = n64psp_time_monotonic_us();

    for (iteration = 0; iteration < ITERATIONS; ++iteration) {
        n64psp_mat4f_mul_scalar(&result, &a, &b);
    }

    scalar_end = n64psp_time_monotonic_us();

    math_benchmark_checksum +=
        result.m[0][0] +
        result.m[1][1] +
        result.m[2][2] +
        result.m[3][3];

    selected_start = n64psp_time_monotonic_us();

    for (iteration = 0; iteration < ITERATIONS; ++iteration) {
        n64psp_mat4f_mul(&result, &a, &b);
    }

    selected_end = n64psp_time_monotonic_us();

    math_benchmark_checksum +=
        result.m[0][0] +
        result.m[1][1] +
        result.m[2][2] +
        result.m[3][3];

    print_benchmark_result(
        "math scalar",
        ITERATIONS,
        scalar_end - scalar_start
    );

    print_benchmark_result(
        "math selected",
        ITERATIONS,
        selected_end - selected_start
    );

    if (selected_end > selected_start) {
        speedup_thousandths =
            ((uint64_t)(scalar_end - scalar_start) * 1000ULL) /
            (uint64_t)(selected_end - selected_start);
    }

    pspDebugScreenPrintf(
        "math relative speed: %llu.%03llux\n",
        (unsigned long long)(speedup_thousandths / 1000ULL),
        (unsigned long long)(speedup_thousandths % 1000ULL)
    );

    return 0;
}

#endif

int n64psp_psp_math_smoke(void) {
    if (run_math_correctness() != 0) {
        return 1;
    }

    if (run_batch_correctness() != 0) {
        return 1;
    }

    if (run_lighting_correctness() != 0) {
        return 1;
    }

#if N64PSP_PSP_BENCHMARKS
    if (run_math_benchmark() != 0) {
        return 1;
    }

    if (run_batch_benchmark() != 0) {
        return 1;
    }

    if (run_lighting_benchmark() != 0) {
        return 1;
    }
#endif

    return 0;
}
