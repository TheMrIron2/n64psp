#ifndef N64PSP_TRIG_H
#define N64PSP_TRIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Calculates sine and cosine for one finite radian angle.
 *
 * out_sine and out_cosine must be non-NULL. They may point to the same
 * float; in that case the cosine store is last and remains visible.
 *
 * Dispatch:
 *     Host builds use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=0 use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=1 use the VFPU implementation for
 *     angles in the hardware-validated range [-8*pi, +8*pi], with scalar
 *     fallback outside that range.
 *
 * Floating point:
 *     Results are expected to match the scalar reference within
 *     1.0e-5 absolute/relative tolerance for finite inputs, but are not
 *     required to be bit-identical.
 *
 * PSP callers selecting the VFPU path must execute on a thread with
 * PSP_THREAD_ATTR_VFPU.
 */
void n64psp_sincosf(
    float radians,
    float* out_sine,
    float* out_cosine
);

#ifdef __cplusplus
}
#endif

#endif