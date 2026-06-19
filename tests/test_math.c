#include "n64psp/math.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (!(expr)) {                                                     \
            fprintf(stderr, "check failed: %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expr);                            \
            return 1;                                                      \
        }                                                                  \
    } while (0)

typedef char n64psp_vec4f_size_must_be_16[
    (sizeof(n64psp_vec4f) == 16) ? 1 : -1
];

typedef char n64psp_mat4f_size_must_be_64[
    (sizeof(n64psp_mat4f) == 64) ? 1 : -1
];

typedef char n64psp_vec4f_pair_size_must_be_32[
    (sizeof(n64psp_vec4f_pair) == 32) ? 1 : -1
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

enum {
    MAX_BATCH_COUNT = 64,
    CANARY_WORDS = 4,
    GUARDED_COUNT = 8
};

typedef struct N64PSP_ALIGN16 guarded_input {
    uint32_t before[CANARY_WORDS];
    n64psp_vec4f values[GUARDED_COUNT];
    uint32_t after[CANARY_WORDS];
} guarded_input;

typedef struct N64PSP_ALIGN16 guarded_output {
    uint32_t before[CANARY_WORDS];
    n64psp_vec4f_pair values[GUARDED_COUNT];
    uint32_t after[CANARY_WORDS];
} guarded_output;

static float absf_local(float value) {
    return value < 0.0f ? -value : value;
}

static int nearly_equal(float actual, float expected) {
    const float difference = absf_local(actual - expected);
    const float scale = absf_local(expected);

    return difference <= 1.0e-5f + 1.0e-5f * scale;
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

static int pair_equal(
    const n64psp_vec4f_pair* actual,
    const n64psp_vec4f_pair* expected
) {
    return
        vector_equal(&actual->first, &expected->first) &&
        vector_equal(&actual->second, &expected->second);
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
                    "matrix mismatch column=%lu row=%lu: actual=%g expected=%g\n",
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

static int run_chain_case(
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    n64psp_vec4f_pair actual[MAX_BATCH_COUNT];
    size_t index;

    CHECK(count <= MAX_BATCH_COUNT);

    n64psp_mat4f_transform_vec4_chain2_batch(
        actual,
        first_matrix,
        second_matrix,
        input,
        count
    );

    for (index = 0; index < count; ++index) {
        n64psp_vec4f_pair expected;

        n64psp_mat4f_transform_vec4(
            &expected.first,
            first_matrix,
            &input[index]
        );

        n64psp_mat4f_transform_vec4(
            &expected.second,
            second_matrix,
            &expected.first
        );

        CHECK(pair_equal(&actual[index], &expected));
    }

    return 0;
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

static int test_matrix_multiply_and_transform(void) {
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
    n64psp_mat4f actual;
    n64psp_mat4f alias;

    matrix_mul_reference(&expected, &a, &b);
    n64psp_mat4f_mul(&actual, &a, &b);
    CHECK(matrix_equal(&actual, &expected));

    alias = a;
    n64psp_mat4f_mul(&alias, &alias, &b);
    CHECK(matrix_equal(&alias, &expected));

    alias = b;
    n64psp_mat4f_mul(&alias, &a, &alias);
    CHECK(matrix_equal(&alias, &expected));

    return 0;
}


static uint32_t matrix_random_state = UINT32_C(0x13579bdf);

static float next_matrix_random_float(void) {
    matrix_random_state =
        matrix_random_state * UINT32_C(1664525) +
        UINT32_C(1013904223);

    return
        ((float)((matrix_random_state >> 8) & UINT32_C(0xffff)) / 8192.0f) -
        4.0f;
}

static int test_random_matrix_multiply(void) {
    int iteration;

    matrix_random_state = UINT32_C(0x13579bdf);

    for (iteration = 0; iteration < 1000; ++iteration) {
        n64psp_mat4f a;
        n64psp_mat4f b;
        n64psp_mat4f expected;
        n64psp_mat4f actual;
        size_t column;
        size_t row;

        for (column = 0; column < 4; ++column) {
            for (row = 0; row < 4; ++row) {
                a.m[column][row] = next_matrix_random_float();
                b.m[column][row] = next_matrix_random_float();
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

static int run_independent_case(
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    n64psp_vec4f_pair actual[MAX_BATCH_COUNT];
    size_t index;

    CHECK(count <= MAX_BATCH_COUNT);

    n64psp_mat4f_transform_vec4_2mat_batch(
        actual,
        first_matrix,
        second_matrix,
        input,
        count
    );

    for (index = 0; index < count; ++index) {
        n64psp_vec4f_pair expected;

        n64psp_mat4f_transform_vec4(
            &expected.first,
            first_matrix,
            &input[index]
        );

        n64psp_mat4f_transform_vec4(
            &expected.second,
            second_matrix,
            &input[index]
        );

        CHECK(pair_equal(&actual[index], &expected));
    }

    return 0;
}

static int test_independent_api_unchanged(void) {
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
    n64psp_vec4f_pair output;

    n64psp_mat4f_transform_vec4_2mat_batch(
        &output,
        &scale,
        &translation,
        &input,
        1
    );

    CHECK(vector_equal(
        &output.first,
        &(n64psp_vec4f){2.0f, 6.0f, 12.0f, 1.0f}
    ));

    CHECK(vector_equal(
        &output.second,
        &(n64psp_vec4f){11.0f, 22.0f, 33.0f, 1.0f}
    ));

    return 0;
}


static uint32_t random_state = UINT32_C(0x4e363450);

static float next_random_float(void) {
    random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);

    return
        ((float)((random_state >> 8) & UINT32_C(0xffff)) / 8192.0f) -
        4.0f;
}


static int test_independent_batch_regression(void) {
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

    const n64psp_mat4f first_matrix = matrix_from_rows(first_rows);
    const n64psp_mat4f second_matrix = matrix_from_rows(second_rows);
    n64psp_vec4f input[MAX_BATCH_COUNT];
    size_t index;

    random_state = UINT32_C(0x2468ace0);

    for (index = 0; index < MAX_BATCH_COUNT; ++index) {
        input[index].x = next_random_float();
        input[index].y = next_random_float();
        input[index].z = next_random_float();
        input[index].w = next_random_float();
    }

    CHECK(run_independent_case(
        &first_matrix,
        &second_matrix,
        input,
        MAX_BATCH_COUNT
    ) == 0);

    return 0;
}

static int test_chain_zero_count(void) {
    n64psp_mat4f_transform_vec4_chain2_batch(
        NULL,
        NULL,
        NULL,
        NULL,
        0
    );

    return 0;
}

static int test_chain_semantic_distinction(void) {
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

    CHECK(vector_equal(
        &chained.first,
        &(n64psp_vec4f){2.0f, 6.0f, 12.0f, 1.0f}
    ));

    CHECK(vector_equal(
        &chained.second,
        &(n64psp_vec4f){12.0f, 26.0f, 42.0f, 1.0f}
    ));

    CHECK(vector_equal(
        &independent.second,
        &(n64psp_vec4f){11.0f, 22.0f, 33.0f, 1.0f}
    ));

    CHECK(!nearly_equal(chained.second.x, independent.second.x));
    CHECK(!nearly_equal(chained.second.y, independent.second.y));
    CHECK(!nearly_equal(chained.second.z, independent.second.z));

    return 0;
}

static int test_chain_identity_and_independent_equivalence(void) {
    static const float arbitrary_rows[4][4] = {
        {1.25f, -2.5f, 3.75f, 4.5f},
        {5.5f, 6.25f, -7.75f, 8.0f},
        {-9.5f, 10.0f, 11.25f, -12.5f},
        {13.75f, -14.25f, 15.5f, 16.0f},
    };

    const n64psp_mat4f identity = identity_matrix();
    const n64psp_mat4f arbitrary = matrix_from_rows(arbitrary_rows);
    const n64psp_vec4f input[] = {
        {1.0f, 2.0f, 3.0f, 1.0f},
        {-4.0f, 5.5f, -6.25f, 2.0f},
        {0.0f, -1.0f, 0.5f, 0.25f},
    };

    n64psp_vec4f_pair chained[3];
    n64psp_vec4f_pair independent[3];
    size_t index;

    CHECK(run_chain_case(&identity, &identity, input, 3) == 0);
    CHECK(run_chain_case(&identity, &arbitrary, input, 3) == 0);

    n64psp_mat4f_transform_vec4_chain2_batch(
        chained,
        &identity,
        &arbitrary,
        input,
        3
    );

    n64psp_mat4f_transform_vec4_2mat_batch(
        independent,
        &identity,
        &arbitrary,
        input,
        3
    );

    for (index = 0; index < 3; ++index) {
        CHECK(pair_equal(&chained[index], &independent[index]));
    }

    return 0;
}

static int test_chain_multiple_matrix_shapes(void) {
    static const float scale_rows[4][4] = {
        {-2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, 0.0f, 0.0f},
        {0.0f, 0.0f, 3.25f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    static const float translation_rows[4][4] = {
        {1.0f, 0.0f, 0.0f, -7.5f},
        {0.0f, 1.0f, 0.0f, 2.25f},
        {0.0f, 0.0f, 1.0f, 11.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    static const float rotation_rows[4][4] = {
        {0.0f, -1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    static const float projection_rows[4][4] = {
        {1.5f, 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.25f, -0.5f},
        {0.0f, 0.0f, -1.0f, 0.0f},
    };

    static const float arbitrary_rows[4][4] = {
        {1.0f, 2.0f, 3.0f, 4.0f},
        {-5.0f, 6.5f, -7.0f, 8.0f},
        {9.0f, -10.0f, 11.25f, -12.0f},
        {13.0f, 14.0f, -15.0f, 16.5f},
    };

    const n64psp_mat4f scale = matrix_from_rows(scale_rows);
    const n64psp_mat4f translation = matrix_from_rows(translation_rows);
    const n64psp_mat4f rotation = matrix_from_rows(rotation_rows);
    const n64psp_mat4f projection = matrix_from_rows(projection_rows);
    const n64psp_mat4f arbitrary = matrix_from_rows(arbitrary_rows);

    const n64psp_vec4f input[] = {
        {1.0f, 2.0f, -3.0f, 1.0f},
        {-0.5f, 4.25f, 2.0f, 0.75f},
        {7.0f, -8.0f, 9.0f, 2.0f},
        {-2.75f, 0.125f, -0.5f, -1.25f},
    };

    CHECK(run_chain_case(&scale, &translation, input, 4) == 0);
    CHECK(run_chain_case(&translation, &projection, input, 4) == 0);
    CHECK(run_chain_case(&rotation, &projection, input, 4) == 0);
    CHECK(run_chain_case(&arbitrary, &rotation, input, 4) == 0);
    CHECK(run_chain_case(&arbitrary, &arbitrary, input, 4) == 0);

    return 0;
}

static int test_chain_projection_modelview_order(void) {
    static const float modelview_rows[4][4] = {
        {0.0f, -1.0f, 0.0f, 10.0f},
        {1.0f, 0.0f, 0.0f, 20.0f},
        {0.0f, 0.0f, 1.0f, 30.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    static const float projection_rows[4][4] = {
        {1.5f, 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.25f, -0.5f},
        {0.0f, 0.0f, -1.0f, 0.0f},
    };

    const n64psp_mat4f modelview = matrix_from_rows(modelview_rows);
    const n64psp_mat4f projection = matrix_from_rows(projection_rows);
    const n64psp_vec4f input[] = {
        {1.0f, 2.0f, 3.0f, 1.0f},
        {-2.0f, 1.5f, -6.0f, 1.0f},
    };

    n64psp_vec4f_pair chained[2];
    n64psp_mat4f projection_modelview;
    n64psp_vec4f_pair precomposed[2];
    size_t index;

    n64psp_mat4f_transform_vec4_chain2_batch(
        chained,
        &modelview,
        &projection,
        input,
        2
    );

    n64psp_mat4f_mul(
        &projection_modelview,
        &projection,
        &modelview
    );

    n64psp_mat4f_transform_vec4_2mat_batch(
        precomposed,
        &modelview,
        &projection_modelview,
        input,
        2
    );

    for (index = 0; index < 2; ++index) {
        CHECK(vector_equal(&chained[index].first, &precomposed[index].first));
        CHECK(vector_equal(&chained[index].second, &precomposed[index].second));
    }

    return 0;
}

static int test_chain_canaries(void) {
    const uint32_t input_canary = UINT32_C(0xa5a5a5a5);
    const uint32_t output_canary = UINT32_C(0x5a5a5a5a);
    const n64psp_mat4f identity = identity_matrix();
    guarded_input input;
    guarded_input input_before;
    guarded_output output;
    const n64psp_vec4f untouched = {99.0f, 99.0f, 99.0f, 99.0f};
    size_t word;
    size_t index;

    for (word = 0; word < CANARY_WORDS; ++word) {
        input.before[word] = input_canary;
        input.after[word] = input_canary;
        output.before[word] = output_canary;
        output.after[word] = output_canary;
    }

    for (index = 0; index < GUARDED_COUNT; ++index) {
        input.values[index] = (n64psp_vec4f){
            (float)index + 0.25f,
            (float)index - 1.5f,
            -(float)index * 0.5f,
            1.0f,
        };
        output.values[index].first = untouched;
        output.values[index].second = untouched;
    }

    input_before = input;

    n64psp_mat4f_transform_vec4_chain2_batch(
        output.values,
        &identity,
        &identity,
        input.values,
        4
    );

    for (word = 0; word < CANARY_WORDS; ++word) {
        CHECK(input.before[word] == input_canary);
        CHECK(input.after[word] == input_canary);
        CHECK(output.before[word] == output_canary);
        CHECK(output.after[word] == output_canary);
    }

    CHECK(memcmp(&input, &input_before, sizeof(input)) == 0);

    for (index = 0; index < 4; ++index) {
        CHECK(vector_equal(&output.values[index].first, &input.values[index]));
        CHECK(vector_equal(&output.values[index].second, &input.values[index]));
    }

    for (index = 4; index < GUARDED_COUNT; ++index) {
        CHECK(vector_equal(&output.values[index].first, &untouched));
        CHECK(vector_equal(&output.values[index].second, &untouched));
    }

    return 0;
}

static int test_chain_random_vectors(void) {
    n64psp_mat4f first_matrix;
    n64psp_mat4f second_matrix;
    n64psp_vec4f input[MAX_BATCH_COUNT];
    size_t column;
    size_t row;
    size_t index;

    random_state = UINT32_C(0x4e363450);

    for (column = 0; column < 4; ++column) {
        for (row = 0; row < 4; ++row) {
            first_matrix.m[column][row] = next_random_float();
            second_matrix.m[column][row] = next_random_float();
        }
    }

    for (index = 0; index < MAX_BATCH_COUNT; ++index) {
        input[index].x = next_random_float();
        input[index].y = next_random_float();
        input[index].z = next_random_float();
        input[index].w = next_random_float();
    }

    CHECK(run_chain_case(
        &first_matrix,
        &second_matrix,
        input,
        MAX_BATCH_COUNT
    ) == 0);

    return 0;
}

int main(void) {
    CHECK(test_layout_and_alignment() == 0);
    CHECK(test_matrix_multiply_and_transform() == 0);
    CHECK(test_random_matrix_multiply() == 0);
    CHECK(test_extreme_finite_values() == 0);
    CHECK(test_independent_api_unchanged() == 0);
    CHECK(test_independent_batch_regression() == 0);

    CHECK(test_chain_zero_count() == 0);
    CHECK(test_chain_semantic_distinction() == 0);
    CHECK(test_chain_identity_and_independent_equivalence() == 0);
    CHECK(test_chain_multiple_matrix_shapes() == 0);
    CHECK(test_chain_projection_modelview_order() == 0);
    CHECK(test_chain_canaries() == 0);
    CHECK(test_chain_random_vectors() == 0);

    puts("n64psp math tests passed");
    return 0;
}
