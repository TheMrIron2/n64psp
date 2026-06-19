#include "math_smoke.h"

#include "../../src/math/math_internal.h"

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

static volatile float math_benchmark_checksum;

enum {
    MATH_BATCH_MAX_COUNT = 64
};

static n64psp_vec4f math_batch_input[MATH_BATCH_MAX_COUNT];
static n64psp_vec4f_pair math_batch_scalar[MATH_BATCH_MAX_COUNT];
static n64psp_vec4f_pair math_batch_selected[MATH_BATCH_MAX_COUNT];
static n64psp_vec4f_pair math_batch_independent[MATH_BATCH_MAX_COUNT];

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
    return "VFPU";
#else
    return "scalar";
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
        4u,
        8u,
        16u,
        32u,
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
        " %-18s total=%llu us batch=%llu ns vertex=%llu ns\n",
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
    consume_batch(math_batch_independent, MATH_BATCH_MAX_COUNT);
}

static int run_batch_benchmark(void) {
    static const unsigned int counts[] = {
        1u,
        4u,
        8u,
        16u,
        32u,
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
        "chain2 benchmark selected=%s\n",
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
            "chain scalar",
            scalar_end - scalar_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "chain selected",
            chain_end - chain_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "independent selected",
            independent_end - independent_start,
            repetitions,
            count
        );

        print_batch_measurement(
            "precompose+indep",
            compound_end - compound_start,
            repetitions,
            count
        );

        print_ratio(
            "scalar/chain",
            scalar_end - scalar_start,
            chain_end - chain_start
        );

        print_ratio(
            "indep/chain",
            independent_end - independent_start,
            chain_end - chain_start
        );

        print_ratio(
            "compound/chain",
            compound_end - compound_start,
            chain_end - chain_start
        );
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

#if N64PSP_PSP_BENCHMARKS
    if (run_math_benchmark() != 0) {
        return 1;
    }

    if (run_batch_benchmark() != 0) {
        return 1;
    }
#endif

    return 0;
}
