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

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
            result.m[column][row] = rows[row][column];
        }
    }

    return result;
}

static int matrix_equal(
    const n64psp_mat4f* actual,
    const n64psp_mat4f* expected,
    float* out_max_difference
) {
    float max_difference = 0.0f;
    unsigned int column;
    unsigned int row;

    for (column = 0; column < 4; ++column) {
        for (row = 0; row < 4; ++row) {
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
                    " actual_bits=%08x expected_bits=%08x\n",
                    float_bits(actual_value),
                    float_bits(expected_value)
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

    if (!matrix_equal(
            &selected,
            &scalar,
            &max_difference
        )) {
        pspDebugScreenPrintf(
            "math normal-output comparison failed\n"
        );
        return 1;
    }

    selected = a;
    n64psp_mat4f_mul(&selected, &selected, &b);

    if (!matrix_equal(
            &selected,
            &scalar,
            &max_difference
        )) {
        pspDebugScreenPrintf(
            "math out==a comparison failed\n"
        );
        return 1;
    }

    selected = b;
    n64psp_mat4f_mul(&selected, &a, &selected);

    if (!matrix_equal(
            &selected,
            &scalar,
            &max_difference
        )) {
        pspDebugScreenPrintf(
            "math out==b comparison failed\n"
        );
        return 1;
    }

    pspDebugScreenPrintf(
        "math mat4 multiply: PASS\n"
    );

    pspDebugScreenPrintf(
        "math selected path: %s\n",
#if N64PSP_USE_VFPU
        "VFPU"
#else
        "scalar"
#endif
    );

    return 0;
}

#if N64PSP_PSP_BENCHMARKS

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
            : 0;

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

    uint64_t speedup_thousandths = 0;
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

#if N64PSP_PSP_BENCHMARKS
    if (run_math_benchmark() != 0) {
        return 1;
    }
#endif

    return 0;
}