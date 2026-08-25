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

typedef struct TestDirectOutput {
    uint32_t guard_before;
    n64psp_vec4f view;
    n64psp_vec4f clip;
    float projected[3];
    uint32_t clip_code;
    uint32_t valid;
    n64psp_vec4f lighting;
    uint32_t guard_after;
} TestDirectOutput;

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
    n64psp_tnl_transform_project_packed_batch(
        NULL, NULL, NULL, 0, 0u
    );
    n64psp_tnl_transform_project_light_packed_batch(
        NULL, NULL, NULL, NULL, NULL, 0u, 0, 0u
    );
    n64psp_texgen_snorm8_batch(
        NULL, NULL, NULL, N64PSP_TEXGEN_SPHERICAL, 0u
    );
    return 0;
}

static int test_texture_generation(void) {
    n64psp_packed_vertex vertices[4];
    n64psp_texcoord_s10_5 output[4];
    n64psp_mat4f modelview = identity_matrix();

    memset(vertices, 0, sizeof(vertices));
    vertices[0].attribute[0] = 127;
    vertices[1].attribute[0] = -127;
    vertices[2].attribute[1] = 127;
    vertices[3].attribute[0] = 90;
    vertices[3].attribute[1] = 90;

    n64psp_texgen_snorm8_batch(
        output,
        &modelview,
        vertices,
        N64PSP_TEXGEN_SPHERICAL,
        4u
    );
    CHECK(output[0].s == 16384);
    CHECK(output[0].t == 32767);
    CHECK(output[1].s == 16384);
    CHECK(output[1].t == 0);
    CHECK(output[2].s == 32767);
    CHECK(output[2].t == 16384);
    CHECK(output[3].s > 27800 && output[3].s < 28100);
    CHECK(output[3].t == output[3].s);

    n64psp_texgen_snorm8_batch(
        output,
        &modelview,
        vertices,
        N64PSP_TEXGEN_LINEAR,
        3u
    );
    CHECK(output[0].s == 16384);
    CHECK(output[0].t == 32767);
    CHECK(output[1].s == 16384);
    CHECK(output[1].t == 0);
    CHECK(output[2].s == 32767);
    CHECK(output[2].t == 16384);

    modelview.m[0][0] = 0.0f;
    modelview.m[0][1] = 1.0f;
    modelview.m[1][0] = -1.0f;
    modelview.m[1][1] = 0.0f;
    n64psp_texgen_snorm8_batch(
        output,
        &modelview,
        vertices,
        N64PSP_TEXGEN_SPHERICAL,
        1u
    );
    CHECK(output[0].s == 0);
    CHECK(output[0].t == 16384);
    return 0;
}

static void init_direct_streams(
    n64psp_tnl_output_streams* streams,
    TestDirectOutput* output
) {
    streams->view = &output[0].view;
    streams->clip = &output[0].clip;
    streams->projected = &output[0].projected;
    streams->lighting = &output[0].lighting;
    streams->clip_code = &output[0].clip_code;
    streams->valid = &output[0].valid;
    streams->vertex_stride = sizeof(output[0]);
    streams->lighting_stride = sizeof(output[0]);
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

static int test_direct_output(void) {
    n64psp_packed_vertex vertices[3];
    TestDirectOutput output[3];
    n64psp_tnl_output_streams streams;
    n64psp_tnl_matrices matrices;
    n64psp_directional_lightf light;
    n64psp_vec4f ambient;
    size_t index;

    memset(vertices, 0, sizeof(vertices));
    memset(output, 0x5a, sizeof(output));
    matrices.modelview = identity_matrix();
    matrices.projection = identity_matrix();
    vertices[0].position[0] = 1;
    vertices[0].position[1] = -2;
    vertices[0].position[2] = 3;
    vertices[1].position[0] = 2;
    vertices[1].position[1] = 0;
    vertices[1].position[2] = 0;
    vertices[2].position[0] = -4;
    matrices.projection.m[3][3] = 0.0f;
    init_direct_streams(&streams, output);

    n64psp_tnl_transform_project_packed_batch(
        &streams,
        &matrices,
        vertices,
        1,
        3u
    );

    for (index = 0u; index < 3u; ++index) {
        CHECK(output[index].guard_before == 0x5a5a5a5au);
        CHECK(output[index].guard_after == 0x5a5a5a5au);
        CHECK(output[index].valid == 0u);
    }
    CHECK(output[0].view.x == 1.0f);
    CHECK(output[0].view.y == -2.0f);
    CHECK(output[0].view.z == 3.0f);

    matrices.projection = identity_matrix();
    memset(output, 0x5a, sizeof(output));
    ambient.x = 11.0f;
    ambient.y = 22.0f;
    ambient.z = 33.0f;
    ambient.w = 0.0f;
    vertices[0].attribute[0] = 1;
    light.direction.x = 1.0f;
    light.direction.y = 0.0f;
    light.direction.z = 0.0f;
    light.direction.w = 0.0f;
    light.color.x = 4.0f;
    light.color.y = 5.0f;
    light.color.z = 6.0f;
    light.color.w = 0.0f;
    matrices.modelview.m[0][0] = 0.03125f;
    n64psp_tnl_transform_project_light_packed_batch(
        &streams,
        &matrices,
        vertices,
        &light,
        &ambient,
        1u,
        1,
        3u
    );
    CHECK(output[0].valid == 1u);
    CHECK(nearly_equal(output[0].projected[0], 0.03125f));
    CHECK(output[0].projected[1] == -2.0f);
    CHECK(output[0].projected[2] == 3.0f);
    CHECK(output[0].clip_code == ((1u << 2) | (1u << 5)));
    CHECK(output[0].lighting.x == ambient.x + light.color.x);
    CHECK(output[0].lighting.y == ambient.y + light.color.y);
    CHECK(output[0].lighting.z == ambient.z + light.color.z);
    CHECK(output[0].lighting.w == 0.0f);

    matrices.modelview = identity_matrix();
    memset(output, 0x5a, sizeof(output));
    n64psp_tnl_transform_project_packed_batch(
        &streams,
        &matrices,
        vertices,
        0,
        3u
    );
    CHECK(output[0].valid == 1u);
    CHECK(nearly_equal(output[0].projected[0], 1.0f / 320.0f));
    CHECK(nearly_equal(output[0].projected[1], 2.0f / 240.0f));
    CHECK(nearly_equal(output[0].projected[2], 3.0f / 4096.0f));
    CHECK(output[0].clip.x == output[0].projected[0]);
    CHECK(output[0].clip.y == output[0].projected[1]);
    CHECK(output[0].clip.z == output[0].projected[2]);
    CHECK(output[0].clip.w == 1.0f);
    CHECK(output[0].clip_code == 0u);
    return 0;
}

int main(void) {
    CHECK(test_layout() == 0);
    CHECK(test_transform_and_lighting() == 0);
    CHECK(test_direct_output() == 0);
    CHECK(test_texture_generation() == 0);
    puts("n64psp packed TnL tests passed");
    return 0;
}
