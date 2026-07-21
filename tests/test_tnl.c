#include "n64psp/tnl.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (!(expr)) {                                                     \
            fprintf(stderr, "check failed: %s:%d: %s\n",                \
                    __FILE__, __LINE__, #expr);                            \
            return 1;                                                      \
        }                                                                  \
    } while (0)

enum {
    TEST_VERTEX_COUNT = 4
};

static int nearly_equal(float actual, float expected) {
    float difference = fabsf(actual - expected);
    float scale = fabsf(expected);

    return difference <= 1.0e-5f + 1.0e-5f * scale;
}

static int vector_equal(const n64psp_vec4f* actual, const n64psp_vec4f* expected) {
    return nearly_equal(actual->x, expected->x) &&
           nearly_equal(actual->y, expected->y) &&
           nearly_equal(actual->z, expected->z) &&
           nearly_equal(actual->w, expected->w);
}

static n64psp_mat4f identity_matrix(void) {
    n64psp_mat4f matrix;
    size_t column;
    size_t row;

    memset(&matrix, 0, sizeof(matrix));
    for (column = 0u; column < 4u; ++column) {
        for (row = 0u; row < 4u; ++row) {
            matrix.m[column][row] = column == row ? 1.0f : 0.0f;
        }
    }
    return matrix;
}

static int test_layout(void) {
    CHECK(sizeof(n64psp_packed_vertex) == 16u);
    CHECK(offsetof(n64psp_packed_vertex, position) == 0u);
    CHECK(offsetof(n64psp_packed_vertex, texcoord) == 8u);
    CHECK(offsetof(n64psp_packed_vertex, attribute) == 12u);
    CHECK(offsetof(n64psp_packed_vertex, alpha) == 15u);
    CHECK(sizeof(n64psp_tnl_matrices) == 128u);
#if defined(__GNUC__) || defined(__clang__)
    CHECK(__alignof__(n64psp_tnl_matrices) >= 16u);
#endif
    n64psp_tnl_transform_packed_batch(NULL, NULL, NULL, 0u);
    n64psp_tnl_transform_light_packed_batch(
        NULL, NULL, NULL, NULL, NULL, NULL, 0u, 0u
    );
    return 0;
}

static int test_transform_and_lighting(void) {
    static const int16_t positions[TEST_VERTEX_COUNT][3] = {
        {1, 2, 3},
        {-4, 5, -6},
        {127, -128, 0},
        {300, 400, -500}
    };
    static const int8_t normals[TEST_VERTEX_COUNT][3] = {
        {127, 0, 0},
        {0, 127, 0},
        {0, 0, 127},
        {-64, 32, 16}
    };
    n64psp_packed_vertex vertices[TEST_VERTEX_COUNT];
    n64psp_vec4f_pair plain_output[TEST_VERTEX_COUNT];
    n64psp_vec4f_pair lit_output[TEST_VERTEX_COUNT];
    n64psp_vec4f lighting_output[TEST_VERTEX_COUNT];
    n64psp_tnl_matrices matrices;
    n64psp_directional_lightf light;
    n64psp_vec4f ambient;
    size_t index;

    memset(vertices, 0, sizeof(vertices));
    matrices.modelview = identity_matrix();
    matrices.projection = identity_matrix();
    matrices.modelview.m[3][0] = 10.0f;
    matrices.modelview.m[3][1] = -20.0f;
    matrices.projection.m[0][0] = 2.0f;
    matrices.projection.m[1][1] = 3.0f;

    light.direction.x = 1.0f;
    light.direction.y = 0.0f;
    light.direction.z = 0.0f;
    light.direction.w = 0.0f;
    light.color.x = 100.0f;
    light.color.y = 50.0f;
    light.color.z = 25.0f;
    light.color.w = 0.0f;
    ambient.x = 5.0f;
    ambient.y = 6.0f;
    ambient.z = 7.0f;
    ambient.w = 0.0f;

    for (index = 0u; index < TEST_VERTEX_COUNT; ++index) {
        vertices[index].position[0] = positions[index][0];
        vertices[index].position[1] = positions[index][1];
        vertices[index].position[2] = positions[index][2];
        vertices[index].attribute[0] = normals[index][0];
        vertices[index].attribute[1] = normals[index][1];
        vertices[index].attribute[2] = normals[index][2];
    }

    n64psp_tnl_transform_packed_batch(
        plain_output, &matrices, vertices, TEST_VERTEX_COUNT
    );
    n64psp_tnl_transform_light_packed_batch(
        lit_output,
        lighting_output,
        &matrices,
        vertices,
        &light,
        &ambient,
        1u,
        TEST_VERTEX_COUNT
    );

    for (index = 0u; index < TEST_VERTEX_COUNT; ++index) {
        n64psp_vec4f input;
        n64psp_vec4f expected_view;
        n64psp_vec4f expected_clip;

        input.x = (float)positions[index][0];
        input.y = (float)positions[index][1];
        input.z = (float)positions[index][2];
        input.w = 1.0f;
        n64psp_mat4f_transform_vec4(&expected_view, &matrices.modelview, &input);
        n64psp_mat4f_transform_vec4(&expected_clip, &matrices.projection, &expected_view);
        CHECK(vector_equal(&plain_output[index].first, &expected_view));
        CHECK(vector_equal(&plain_output[index].second, &expected_clip));
        CHECK(vector_equal(&lit_output[index].first, &expected_view));
        CHECK(vector_equal(&lit_output[index].second, &expected_clip));
    }

    CHECK(nearly_equal(lighting_output[0].x, 105.0f));
    CHECK(nearly_equal(lighting_output[0].y, 56.0f));
    CHECK(nearly_equal(lighting_output[0].z, 32.0f));
    CHECK(nearly_equal(lighting_output[1].x, 5.0f));
    CHECK(nearly_equal(lighting_output[1].y, 6.0f));
    CHECK(nearly_equal(lighting_output[1].z, 7.0f));
    CHECK(lighting_output[0].w == 0.0f);
    return 0;
}

int main(void) {
    CHECK(test_layout() == 0);
    CHECK(test_transform_and_lighting() == 0);
    puts("n64psp packed TnL tests passed");
    return 0;
}
