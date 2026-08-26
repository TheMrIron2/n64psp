#include <n64psp/fog.h>

#include <stdint.h>
#include <stdio.h>

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                                   \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

int main(void) {
    n64psp_fog_coefficients fog = { 320, 128 };
    n64psp_fog_coefficients negative = { -320, 128 };

    CHECK(n64psp_fog_alpha(&fog, -2.0f, 5.0f) == 0);
    CHECK(n64psp_fog_alpha(&fog, 0.0f, 1.0f) == 128);
    CHECK(n64psp_fog_alpha(&fog, 127.0f, 320.0f) == 255);
    CHECK(n64psp_fog_alpha(&fog, 1.0f, 0.0f) == 0);
    CHECK(n64psp_fog_alpha(&fog, 1.0f, -1.0f) == 0);
    CHECK(n64psp_fog_alpha(&negative, -127.0f, 320.0f) == 255);
    CHECK(n64psp_fog_alpha(0, 1.0f, 1.0f) == 0);
    CHECK(n64psp_fog_alpha_lerp(0, 255, 0.5f) == 127);
    CHECK(n64psp_fog_alpha_lerp(64, 192, 0.25f) == 96);
    CHECK(n64psp_fog_alpha_lerp(255, 0, 0.5f) == 127);

    puts("n64psp fog tests: ok");
    return 0;
}
