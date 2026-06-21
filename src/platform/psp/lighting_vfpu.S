    .text
    .set push
    .set noreorder
    .set noat

    .align 2
    .global n64psp_directional_light_snorm8_batch_vfpu
    .type n64psp_directional_light_snorm8_batch_vfpu, @function

/*
 * void n64psp_directional_light_snorm8_batch_vfpu(
 *     n64psp_vec4f* output_rgb,                 // a0
 *     const n64psp_mat4f* normal_matrix,        // a1
 *     const n64psp_snorm8x4* normals,           // a2
 *     const n64psp_directional_lightf* lights,  // a3
 *     const n64psp_vec4f* ambient,              // stack + 16
 *     size_t light_count,                       // stack + 20
 *     size_t count                              // stack + 24
 * );
 *
 * Contract mirrors the public C API: return before dereferencing pointers
 * when count is zero; aligned matrix/light/output storage; packed four-byte
 * normals; no input/output overlap.
 *
 * This PSP entry point currently delegates to the scalar semantic reference.
 * It preserves the public dispatch surface while the handwritten VFPU loop is
 * developed and validated independently.
 */
n64psp_directional_light_snorm8_batch_vfpu:
    lw          $t0, 24($sp)
    beqz        $t0, 1f
    nop

    j           n64psp_directional_light_snorm8_batch_scalar
    nop

1:
    jr          $ra
    nop

    .size n64psp_directional_light_snorm8_batch_vfpu, \
        .-n64psp_directional_light_snorm8_batch_vfpu

    .set pop
