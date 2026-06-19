#include "n64psp/math.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                                  \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

typedef char n64psp_vec4f_size_must_be_16[
    (sizeof(n64psp_vec4f) == 16) ? 1 : -1
];

typedef char n64psp_mat4f_size_must_be_64[
    (sizeof(n64psp_mat4f) == 64) ? 1 : -1
];

#if defined(__GNUC__) || defined(__clang__)
typedef char n64psp_vec4f_alignment_must_be_16[
    (__alignof__(n64psp_vec4f) >= 16) ? 1 : -1
];

typedef char n64psp_mat4f_alignment_must_be_16[
    (__alignof__(n64psp_mat4f) >= 16) ? 1 : -1
];
#endif

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

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
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

static int matrix_equal(
    const n64psp_mat4f* actual,
    const n64psp_mat4f* expected
) {
    size_t column;
    size_t row;

    for (column = 0; column < 4; ++column) {
        for (row = 0; row < 4; ++row) {
            if (!nearly_equal(
                    actual->m[column][row],
                    expected->m[column][row]
                )) {
                fprintf(
                    stderr,
                    "matrix mismatch at column=%lu row=%lu: "
                    "actual=%g expected=%g\n",
                    (unsigned long)column,
                    (unsigned long)row,
                    (double)actual->m[column][row],
                    (double)expected->m[column][row]
                );
                return 0;
            }
        }
    }

    return 1;
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
            "vector mismatch: actual={%g,%g,%g,%g} "
            "expected={%g,%g,%g,%g}\n",
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

/*
 * Independent reference using double intermediates.
 *
 * This deliberately does not call the production scalar function.
 */
static void matrix_mul_reference(
    n64psp_mat4f* out,
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
) {
    n64psp_mat4f result;
    size_t column;
    size_t row;
    size_t k;

    for (column = 0; column < 4; ++column) {
        for (row = 0; row < 4; ++row) {
            double sum = 0.0;

            for (k = 0; k < 4; ++k) {
                sum +=
                    (double)a->m[k][row] *
                    (double)b->m[column][k];
            }

            result.m[column][row] = (float)sum;
        }
    }

    *out = result;
}

static int test_layout_and_alignment(void) {
    n64psp_vec4f vector;
    n64psp_mat4f matrix;

    CHECK(((uintptr_t)&vector & 15u) == 0u);
    CHECK(((uintptr_t)&matrix & 15u) == 0u);

    return 0;
}

static int test_identity(void) {
    static const float rows[4][4] = {
        {1.0f, 2.0f, 3.0f, 4.0f},
        {5.0f, 6.0f, 7.0f, 8.0f},
        {9.0f, 10.0f, 11.0f, 12.0f},
        {13.0f, 14.0f, 15.0f, 16.0f},
    };

    const n64psp_mat4f identity = identity_matrix();
    const n64psp_mat4f matrix = matrix_from_rows(rows);
    n64psp_mat4f result;

    n64psp_mat4f_mul(&result, &identity, &matrix);
    CHECK(matrix_equal(&result, &matrix));

    n64psp_mat4f_mul(&result, &matrix, &identity);
    CHECK(matrix_equal(&result, &matrix));

    return 0;
}

static int test_transform_and_composition(void) {
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
    const n64psp_mat4f translation =
        matrix_from_rows(translation_rows);

    const n64psp_vec4f input = {
        1.0f,
        2.0f,
        3.0f,
        1.0f,
    };

    const n64psp_vec4f expected = {
        12.0f,
        26.0f,
        42.0f,
        1.0f,
    };

    n64psp_mat4f combined;
    n64psp_vec4f output;

    // Translation * scale applies scale first, then translation
    n64psp_mat4f_mul(&combined, &translation, &scale);
    n64psp_mat4f_transform_vec4(&output, &combined, &input);

    CHECK(vector_equal(&output, &expected));

    output = input;
    n64psp_mat4f_transform_vec4(&output, &combined, &output);

    CHECK(vector_equal(&output, &expected));

    return 0;
}

static int test_arbitrary_and_aliasing(void) {
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

    n64psp_mat4f expected;
    n64psp_mat4f result;
    n64psp_mat4f alias;

    matrix_mul_reference(&expected, &a, &b);

    n64psp_mat4f_mul(&result, &a, &b);
    CHECK(matrix_equal(&result, &expected));

    alias = a;
    n64psp_mat4f_mul(&alias, &alias, &b);
    CHECK(matrix_equal(&alias, &expected));

    alias = b;
    n64psp_mat4f_mul(&alias, &a, &alias);
    CHECK(matrix_equal(&alias, &expected));

    return 0;
}

static uint32_t random_state = 0x4e363450u;

static float next_random_float(void) {
    random_state =
        random_state * 1664525u +
        1013904223u;

    return
        ((float)((random_state >> 8) & 0xffffu) / 8192.0f) -
        4.0f;
}

static int test_random_matrices(void) {
    int iteration;

    for (iteration = 0; iteration < 1000; ++iteration) {
        n64psp_mat4f a;
        n64psp_mat4f b;
        n64psp_mat4f expected;
        n64psp_mat4f actual;

        size_t column;
        size_t row;

        for (column = 0; column < 4; ++column) {
            for (row = 0; row < 4; ++row) {
                a.m[column][row] = next_random_float();
                b.m[column][row] = next_random_float();
            }
        }

        matrix_mul_reference(&expected, &a, &b);
        n64psp_mat4f_mul(&actual, &a, &b);

        CHECK(matrix_equal(&actual, &expected));
    }

    return 0;
}

static int test_extreme_finite_values(void) {
    const n64psp_mat4f identity = identity_matrix();

    n64psp_mat4f matrix = identity_matrix();
    n64psp_mat4f result;

    matrix.m[0][0] = 1.0e20f;
    matrix.m[1][1] = -1.0e-20f;
    matrix.m[2][2] = 123456.0f;
    matrix.m[3][0] = -98765.0f;

    n64psp_mat4f_mul(&result, &identity, &matrix);

    CHECK(matrix_equal(&result, &matrix));

    return 0;
}

int main(void) {
    CHECK(test_layout_and_alignment() == 0);
    CHECK(test_identity() == 0);
    CHECK(test_transform_and_composition() == 0);
    CHECK(test_arbitrary_and_aliasing() == 0);
    CHECK(test_random_matrices() == 0);
    CHECK(test_extreme_finite_values() == 0);

    puts("n64psp math tests passed");
    return 0;
}