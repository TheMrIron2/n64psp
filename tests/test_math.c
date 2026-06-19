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

typedef char n64psp_vec4f_pair_alignment_must_be_16[
    (__alignof__(n64psp_vec4f_pair) >= 16) ? 1 : -1
];
#endif

typedef char n64psp_vec4f_pair_size_must_be_32[
    (sizeof(n64psp_vec4f_pair) == 32) ? 1 : -1
];

static float absf_local(float value) {
    return value < 0.0f ? -value : value;
}

enum {
    BATCH_CANARY_WORDS = 4,
    BATCH_GUARDED_COUNT = 8
};

typedef struct N64PSP_ALIGN16 guarded_batch_input {
    uint32_t before[BATCH_CANARY_WORDS];
    n64psp_vec4f values[BATCH_GUARDED_COUNT];
    uint32_t after[BATCH_CANARY_WORDS];
} guarded_batch_input;

typedef struct N64PSP_ALIGN16 guarded_batch_output {
    uint32_t before[BATCH_CANARY_WORDS];
    n64psp_vec4f_pair values[BATCH_GUARDED_COUNT];
    uint32_t after[BATCH_CANARY_WORDS];
} guarded_batch_output;


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

static void vector_transform_reference(
    n64psp_vec4f* out,
    const n64psp_mat4f* matrix,
    const n64psp_vec4f* input
) {
    const double x = (double)input->x;
    const double y = (double)input->y;
    const double z = (double)input->z;
    const double w = (double)input->w;

    n64psp_vec4f result;

    result.x = (float)(
        (double)matrix->m[0][0] * x +
        (double)matrix->m[1][0] * y +
        (double)matrix->m[2][0] * z +
        (double)matrix->m[3][0] * w
    );

    result.y = (float)(
        (double)matrix->m[0][1] * x +
        (double)matrix->m[1][1] * y +
        (double)matrix->m[2][1] * z +
        (double)matrix->m[3][1] * w
    );

    result.z = (float)(
        (double)matrix->m[0][2] * x +
        (double)matrix->m[1][2] * y +
        (double)matrix->m[2][2] * z +
        (double)matrix->m[3][2] * w
    );

    result.w = (float)(
        (double)matrix->m[0][3] * x +
        (double)matrix->m[1][3] * y +
        (double)matrix->m[2][3] * z +
        (double)matrix->m[3][3] * w
    );

    *out = result;
}

static int pair_equal(
    const n64psp_vec4f_pair* actual,
    const n64psp_vec4f_pair* expected
) {
    return
        vector_equal(&actual->first, &expected->first) &&
        vector_equal(&actual->second, &expected->second);
}

static int test_layout_and_alignment(void) {
    n64psp_vec4f vector;
    n64psp_mat4f matrix;
    n64psp_vec4f_pair pair;

    CHECK(((uintptr_t)&vector & 15u) == 0u);
    CHECK(((uintptr_t)&matrix & 15u) == 0u);
    CHECK(((uintptr_t)&pair & 15u) == 0u);

    CHECK(sizeof(vector) == 16u);
    CHECK(sizeof(matrix) == 64u);
    CHECK(sizeof(pair) == 32u);

    return 0;
}

static int run_batch_case(
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    n64psp_vec4f_pair actual[64];
    n64psp_vec4f_pair expected[64];
    size_t index;

    CHECK(count <= 64u);

    for (index = 0; index < count; ++index) {
        vector_transform_reference(
            &expected[index].first,
            first_matrix,
            &input[index]
        );

        vector_transform_reference(
            &expected[index].second,
            second_matrix,
            &input[index]
        );
    }

    n64psp_mat4f_transform_vec4_2mat_batch(
        actual,
        first_matrix,
        second_matrix,
        input,
        count
    );

    for (index = 0; index < count; ++index) {
        n64psp_vec4f single_first;
        n64psp_vec4f single_second;

        CHECK(pair_equal(&actual[index], &expected[index]));

        n64psp_mat4f_transform_vec4(
            &single_first,
            first_matrix,
            &input[index]
        );

        n64psp_mat4f_transform_vec4(
            &single_second,
            second_matrix,
            &input[index]
        );

        CHECK(vector_equal(&actual[index].first, &single_first));
        CHECK(vector_equal(&actual[index].second, &single_second));
    }

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

static int test_batch_zero_count(void) {
    n64psp_mat4f_transform_vec4_2mat_batch(
        NULL,
        NULL,
        NULL,
        NULL,
        0
    );

    return 0;
}

static int test_batch_known_values(void) {
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

    const n64psp_vec4f input[] = {
        {1.0f, 2.0f, 3.0f, 1.0f},
        {-2.0f, 0.5f, 4.0f, 1.0f},
        {3.25f, -1.5f, 0.75f, 2.0f},
        {-0.25f, -2.5f, 8.0f, 0.5f},
    };

    n64psp_vec4f_pair output[4];

    n64psp_mat4f_transform_vec4_2mat_batch(
        output,
        &scale,
        &translation,
        input,
        4
    );

    CHECK(vector_equal(
        &output[0].first,
        &(n64psp_vec4f){2.0f, 6.0f, 12.0f, 1.0f}
    ));

    CHECK(vector_equal(
        &output[0].second,
        &(n64psp_vec4f){11.0f, 22.0f, 33.0f, 1.0f}
    ));

    CHECK(run_batch_case(
        &scale,
        &translation,
        input,
        4
    ) == 0);

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

static int test_batch_identity_and_equal_matrices(void) {
    const n64psp_mat4f identity = identity_matrix();

    const n64psp_vec4f input[] = {
        {1.0f, 2.0f, 3.0f, 1.0f},
        {-4.0f, 5.5f, -6.25f, 2.0f},
        {0.0f, -1.0f, 0.5f, 0.25f},
    };

    n64psp_vec4f_pair output[3];
    size_t index;

    n64psp_mat4f_transform_vec4_2mat_batch(
        output,
        &identity,
        &identity,
        input,
        3
    );

    for (index = 0; index < 3; ++index) {
        CHECK(vector_equal(&output[index].first, &input[index]));
        CHECK(vector_equal(&output[index].second, &input[index]));
    }

    CHECK(run_batch_case(
        &identity,
        &identity,
        input,
        3
    ) == 0);

    return 0;
}

static int test_batch_canaries(void) {
    const uint32_t input_canary = UINT32_C(0xa5a5a5a5);
    const uint32_t output_canary = UINT32_C(0x5a5a5a5a);

    const n64psp_mat4f identity = identity_matrix();

    guarded_batch_input input;
    guarded_batch_input input_before;
    guarded_batch_output output;

    size_t index;
    size_t word;

    for (word = 0; word < BATCH_CANARY_WORDS; ++word) {
        input.before[word] = input_canary;
        input.after[word] = input_canary;

        output.before[word] = output_canary;
        output.after[word] = output_canary;
    }

    for (index = 0; index < BATCH_GUARDED_COUNT; ++index) {
        input.values[index].x = (float)index + 0.25f;
        input.values[index].y = (float)index - 1.5f;
        input.values[index].z = -(float)index * 0.5f;
        input.values[index].w = 1.0f;

        output.values[index].first =
            (n64psp_vec4f){99.0f, 99.0f, 99.0f, 99.0f};

        output.values[index].second =
            (n64psp_vec4f){99.0f, 99.0f, 99.0f, 99.0f};
    }

    input_before = input;

    n64psp_mat4f_transform_vec4_2mat_batch(
        output.values,
        &identity,
        &identity,
        input.values,
        4
    );

    for (word = 0; word < BATCH_CANARY_WORDS; ++word) {
        CHECK(input.before[word] == input_canary);
        CHECK(input.after[word] == input_canary);
        CHECK(output.before[word] == output_canary);
        CHECK(output.after[word] == output_canary);
    }

    for (index = 0; index < BATCH_GUARDED_COUNT; ++index) {
        CHECK(vector_equal(
            &input.values[index],
            &input_before.values[index]
        ));
    }

    for (index = 0; index < 4; ++index) {
        CHECK(vector_equal(
            &output.values[index].first,
            &input.values[index]
        ));

        CHECK(vector_equal(
            &output.values[index].second,
            &input.values[index]
        ));
    }

    for (index = 4; index < BATCH_GUARDED_COUNT; ++index) {
        const n64psp_vec4f untouched = {
            99.0f,
            99.0f,
            99.0f,
            99.0f,
        };

        CHECK(vector_equal(
            &output.values[index].first,
            &untouched
        ));

        CHECK(vector_equal(
            &output.values[index].second,
            &untouched
        ));
    }

    return 0;
}

static int test_batch_arbitrary_matrices(void) {
    static const float rotation_rows[4][4] = {
        {0.0f, -1.0f, 0.0f, 0.0f},
        {1.0f,  0.0f, 0.0f, 0.0f},
        {0.0f,  0.0f, 1.0f, 0.0f},
        {0.0f,  0.0f, 0.0f, 1.0f},
    };

    static const float projection_rows[4][4] = {
        {1.25f, 0.0f,  0.0f,  0.0f},
        {0.0f,  1.75f, 0.0f,  0.0f},
        {0.0f,  0.0f, -1.1f, -0.2f},
        {0.0f,  0.0f, -1.0f,  0.0f},
    };

    static const float asymmetric_rows[4][4] = {
        {1.0f,  2.0f,  3.0f,  4.0f},
        {-5.0f, 6.5f, -7.0f,  8.0f},
        {9.0f, -10.0f, 11.25f, -12.0f},
        {13.0f, 14.0f, -15.0f, 16.5f},
    };

    const n64psp_mat4f rotation =
        matrix_from_rows(rotation_rows);

    const n64psp_mat4f projection =
        matrix_from_rows(projection_rows);

    const n64psp_mat4f asymmetric =
        matrix_from_rows(asymmetric_rows);

    const n64psp_vec4f input[] = {
        {1.0f, 2.0f, -3.0f, 1.0f},
        {-0.5f, 4.25f, 2.0f, 0.75f},
        {7.0f, -8.0f, 9.0f, 2.0f},
    };

    CHECK(run_batch_case(
        &rotation,
        &projection,
        input,
        3
    ) == 0);

    CHECK(run_batch_case(
        &asymmetric,
        &rotation,
        input,
        3
    ) == 0);

    return 0;
}

static int test_batch_projection_modelview(void) {
    static const float modelview_rows[4][4] = {
        {0.0f, -1.0f, 0.0f, 10.0f},
        {1.0f,  0.0f, 0.0f, 20.0f},
        {0.0f,  0.0f, 1.0f, 30.0f},
        {0.0f,  0.0f, 0.0f, 1.0f},
    };

    static const float projection_rows[4][4] = {
        {1.5f, 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.25f, -0.5f},
        {0.0f, 0.0f, -1.0f, 0.0f},
    };

    const n64psp_mat4f modelview =
        matrix_from_rows(modelview_rows);

    const n64psp_mat4f projection =
        matrix_from_rows(projection_rows);

    const n64psp_vec4f input = {
        1.0f,
        2.0f,
        3.0f,
        1.0f,
    };

    n64psp_mat4f projection_modelview;
    n64psp_vec4f_pair pair;
    n64psp_vec4f expected_first;
    n64psp_vec4f expected_second;

    n64psp_mat4f_mul(
        &projection_modelview,
        &projection,
        &modelview
    );

    n64psp_mat4f_transform_vec4_2mat_batch(
        &pair,
        &modelview,
        &projection_modelview,
        &input,
        1
    );

    vector_transform_reference(
        &expected_first,
        &modelview,
        &input
    );

    vector_transform_reference(
        &expected_second,
        &projection,
        &expected_first
    );

    CHECK(vector_equal(&pair.first, &expected_first));
    CHECK(vector_equal(&pair.second, &expected_second));

    return 0;
}

static int test_batch_random_vectors(void) {
    n64psp_mat4f first_matrix;
    n64psp_mat4f second_matrix;
    n64psp_vec4f input[64];

    size_t column;
    size_t row;
    size_t index;

    random_state = 0x4e363450u;

    for (column = 0; column < 4; ++column) {
        for (row = 0; row < 4; ++row) {
            first_matrix.m[column][row] = next_random_float();
            second_matrix.m[column][row] = next_random_float();
        }
    }

    for (index = 0; index < 64; ++index) {
        input[index].x = next_random_float();
        input[index].y = next_random_float();
        input[index].z = next_random_float();
        input[index].w = next_random_float();
    }

    CHECK(run_batch_case(
        &first_matrix,
        &second_matrix,
        input,
        64
    ) == 0);

    return 0;
}

int main(void) {
    CHECK(test_layout_and_alignment() == 0);

    CHECK(test_identity() == 0);
    CHECK(test_transform_and_composition() == 0);
    CHECK(test_arbitrary_and_aliasing() == 0);
    CHECK(test_random_matrices() == 0);
    CHECK(test_extreme_finite_values() == 0);

    CHECK(test_batch_zero_count() == 0);
    CHECK(test_batch_known_values() == 0);
    CHECK(test_batch_identity_and_equal_matrices() == 0);
    CHECK(test_batch_arbitrary_matrices() == 0);
    CHECK(test_batch_projection_modelview() == 0);
    CHECK(test_batch_random_vectors() == 0);
    CHECK(test_batch_canaries() == 0);

    puts("n64psp math tests passed");
    return 0;
}