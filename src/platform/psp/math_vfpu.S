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

    .set pop