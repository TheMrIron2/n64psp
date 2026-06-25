    .text
    .set push
    .set noreorder
    .set noat

    .align 2
    .global n64psp_sincosf_vfpu
    .type n64psp_sincosf_vfpu, @function

/*
 * void n64psp_sincosf_vfpu(
 *     float radians,       // f12
 *     float* out_sine,     // a0
 *     float* out_cosine    // a1
 * );
 *
 * PSP GCC's single-float ABI passes the scalar float argument in f12 and the
 * two pointer arguments in a0/a1. The angle is moved through a GPR into S000.
 *
 * VFPU trig instructions use a binary-angle scale where 1.0 is pi/2.
 * Convert ordinary radians with VFPU_2_PI:
 *
 *     vfpu_angle = radians * (2.0 / pi)
 *
 * vrot.p with immediate [s,c] stores:
 *
 *     C010[0] = sin(radians)
 *     C010[1] = cos(radians)
 *
 * The destination pair does not overlap the source scalar register. The stores
 * are ordered sine first, cosine second, matching the scalar alias behavior.
 *
 * Clobbers: GPR a2; VFPU scalars S000, S001, S010, S011.
 * Does not modify VFPU control state.
 */
n64psp_sincosf_vfpu:
    mfc1        $a2, $f12
    mtv         $a2, S000
    vcst.s      S001, VFPU_2_PI
    vmul.s      S000, S000, S001
    vrot.p      C010, S000, [s,c]
    sv.s        S010, 0($a0)
    sv.s        S011, 0($a1)

    jr          $ra
    nop

    .size n64psp_sincosf_vfpu, .-n64psp_sincosf_vfpu

    .align 2
    .global n64psp_mat4f_mul_vfpu
    .type n64psp_mat4f_mul_vfpu, @function

/*
 * void n64psp_mat4f_mul_vfpu(
 *     n64psp_mat4f* out,       // a0
 *     const n64psp_mat4f* a,   // a1
 *     const n64psp_mat4f* b    // a2
 * );
 *
 * All pointers must be aligned to 16 bytes.
 *
 * Matrix memory layout:
 *
 *     m[column][row]
 *
 * Each contiguous 16-byte block therefore contains one matrix column.
 *
 * Inputs are completely loaded before the result is stored, so these
 * supported public API cases remain safe:
 *
 *     out == a
 *     out == b
 *
 * VFPU matrices M000, M100 and M200 are clobbered.
 */
n64psp_mat4f_mul_vfpu:
    lv.q        C000, 0($a1)
    lv.q        C010, 16($a1)
    lv.q        C020, 32($a1)
    lv.q        C030, 48($a1)

    lv.q        C100, 0($a2)
    lv.q        C110, 16($a2)
    lv.q        C120, 32($a2)
    lv.q        C130, 48($a2)

    vmmul.q     M200, M000, M100

    sv.q        C200, 0($a0)
    sv.q        C210, 16($a0)
    sv.q        C220, 32($a0)
    sv.q        C230, 48($a0)

    jr          $ra
    nop

    .size n64psp_mat4f_mul_vfpu, .-n64psp_mat4f_mul_vfpu

    .align 2
    .global n64psp_mat4f_transform_vec4_2mat_batch_vfpu
    .type n64psp_mat4f_transform_vec4_2mat_batch_vfpu, @function

/*
 * void n64psp_mat4f_transform_vec4_2mat_batch_vfpu(
 *     n64psp_vec4f_pair* output,       // a0
 *     const n64psp_mat4f* first,       // a1
 *     const n64psp_mat4f* second,      // a2
 *     const n64psp_vec4f* input,       // a3
 *     size_t count                     // t0
 * );
 *
 * All pointers must be aligned to 16 bytes when count is non-zero.
 *
 * The matrices are loaded once and retained in M000 and M100.
 * Input advances by 16 bytes and output advances by 32 bytes.
 *
 * VFPU matrices M000 and M100 are clobbered.
 * VFPU vectors R200, R201 and R202 are clobbered.
 * General-purpose register t0 is consumed as the count/end pointer.
 */

n64psp_mat4f_transform_vec4_2mat_batch_vfpu:
    /*
     * PSP/MIPS argument registers:
     *
     *     a0 = output
     *     a1 = first_matrix
     *     a2 = second_matrix
     *     a3 = input
     *     t0 = count
     *
     * Return before accessing any pointer when count is zero.
     */
    beqz        $t0, 2f
    nop

    /*
     * n64psp matrices use the same transform convention as the
     * Daedalus PSP matrices:
     *
     *     result.x =
     *         x * m[0][0] +
     *         y * m[1][0] +
     *         z * m[2][0] +
     *         w * m[3][0]
     *
     * Load the four contiguous matrix vectors through R registers
     * for use by vtfm4.q.
     */
    lv.q        R000, 0($a1)
    lv.q        R001, 16($a1)
    lv.q        R002, 32($a1)
    lv.q        R003, 48($a1)

    lv.q        R100, 0($a2)
    lv.q        R101, 16($a2)
    lv.q        R102, 32($a2)
    lv.q        R103, 48($a2)

    /*
     * Convert count into an end pointer:
     *
     *     end = input + count * sizeof(n64psp_vec4f)
     */
    sll         $t0, $t0, 4
    addu        $t0, $a3, $t0

1:
    lv.q        R200, 0($a3)

    /*
     * vtfm4.q consumes the matrices through the R-row register layout.
     * This differs from the load aliases used by the vmmul.q routine above.
     */
    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R200

    sv.q        R201, 0($a0)
    sv.q        R202, 16($a0)

    addiu       $a3, $a3, 16

    bne         $a3, $t0, 1b
    addiu       $a0, $a0, 32

2:
    jr          $ra
    nop

    .size n64psp_mat4f_transform_vec4_2mat_batch_vfpu, \
        .-n64psp_mat4f_transform_vec4_2mat_batch_vfpu

    .align 2
    .global n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline
    .type n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline, @function

/*
 * void n64psp_mat4f_transform_vec4_chain2_batch_vfpu(
 *     n64psp_vec4f_pair* output,       // a0
 *     const n64psp_mat4f* first,       // a1
 *     const n64psp_mat4f* second,      // a2
 *     const n64psp_vec4f* input,       // a3
 *     size_t count                     // t0
 * );
 *
 * All pointers must be aligned to 16 bytes when count is non-zero.
 * Both matrices are loaded once before the loop and retained in M000/M100.
 * The second transform consumes the first transform result directly.
 * Input advances by 16 bytes and output advances by 32 bytes.
 *
 * VFPU matrices M000 and M100 are clobbered.
 * VFPU vectors R200, R201 and R202 are clobbered.
 * General-purpose register t0 is consumed as the count/end pointer.
 */
n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline:
    beqz        $t0, 4f
    nop

    lv.q        R000, 0($a1)
    lv.q        R001, 16($a1)
    lv.q        R002, 32($a1)
    lv.q        R003, 48($a1)

    lv.q        R100, 0($a2)
    lv.q        R101, 16($a2)
    lv.q        R102, 32($a2)
    lv.q        R103, 48($a2)

    sll         $t0, $t0, 4
    addu        $t0, $a3, $t0

3:
    lv.q        R200, 0($a3)

    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R201

    sv.q        R201, 0($a0)
    sv.q        R202, 16($a0)

    addiu       $a3, $a3, 16

    bne         $a3, $t0, 3b
    addiu       $a0, $a0, 32

4:
    jr          $ra
    nop

    .size n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline, \
        .-n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline

    .set pop
