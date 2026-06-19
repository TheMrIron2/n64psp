    .text
    .set push
    .set noreorder
    .set noat

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

    /*
     * M200 is distinct from both input register matrices. vmmul.q
     * does not permit its destination to overlap either source.
     */
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
 *     size_t count                     // 16(sp)
 * );
 *
 * All pointers must be aligned to 16 bytes when count is non-zero.
 *
 * Input advances by 16 bytes per vector.
 * Output advances by 32 bytes per vector.
 *
 * VFPU matrices M000 and M100 are used for the retained matrices.
 * VFPU vectors R200, R201 and R202 are used for input and results.
 * These VFPU registers are clobbered.
 */
 
n64psp_mat4f_transform_vec4_2mat_batch_vfpu:
    lw          $t0, 16($sp)

    beqz        $t0, 2f
    nop

    /*
     * Load both matrices once, before entering the vertex loop.
     */
    lv.q        C000, 0($a1)
    lv.q        C010, 16($a1)
    lv.q        C020, 32($a1)
    lv.q        C030, 48($a1)

    lv.q        C100, 0($a2)
    lv.q        C110, 16($a2)
    lv.q        C120, 32($a2)
    lv.q        C130, 48($a2)

1:
    lv.q        R200, 0($a3)

    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R200

    sv.q        R201, 0($a0)
    sv.q        R202, 16($a0)

    addiu       $a3, $a3, 16
    addiu       $a0, $a0, 32

    addiu       $t0, $t0, -1
    bnez        $t0, 1b
    nop

2:
    jr          $ra
    nop

    .size n64psp_mat4f_transform_vec4_2mat_batch_vfpu, \
        .-n64psp_mat4f_transform_vec4_2mat_batch_vfpu

    .set pop