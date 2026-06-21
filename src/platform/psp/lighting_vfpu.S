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
 *     const n64psp_vec4f* ambient,              // t0
 *     size_t light_count,                       // t1
 *     size_t count                              // t2
 * );
 *
 *
 * Input/output layout:
 *     normals advance by 4 bytes per packed n64psp_snorm8x4.
 *     lights advance by 32 bytes per n64psp_directional_lightf.
 *     output_rgb advances by 16 bytes per n64psp_vec4f.
 *
 * Alignment:
 *     normal_matrix, ambient, lights when light_count > 0, and output_rgb
 *     must be 16-byte aligned. normals is 16-byte aligned at the base but is
 *     consumed as packed four-byte records through GPR loads.
 *
 * count == 0:
 *     Returns before dereferencing any supplied pointer.
 *
 * light_count == 0:
 *     lights may be NULL and is not dereferenced.
 *
 * Overlap:
 *     Input and output ranges must not overlap.
 *
 * This is a leaf function and does not create a stack frame.
 *
 * VFPU clobbers:
 *     M000 rows R000/R001/R002 for the 3x3 matrix.
 *     R200/R201/R203, R300/R301/R302, R600 and R700.
 *     Scalars S500/S501/S503 and S702.
 *
 * GPR clobbers:
 *     t0-t9. Argument registers a0/a2 are advanced.
 *
 * Floating point:
 *     Uses VFPU vdot.t, vrsq.s and vscl.t. Results are expected to be
 *     semantically equivalent to scalar within the public tolerance, but not
 *     bit-identical.
 */
n64psp_directional_light_snorm8_batch_vfpu:
    /*
     * PSP EABI:
     *     a0 = output_rgb
     *     a1 = normal_matrix
     *     a2 = normals
     *     a3 = lights
     *     t0 = ambient
     *     t1 = light_count
     *     t2 = count
     */
    beqz        $t2, 1f
    nop

    move        $t4, $t0            /* ambient */

    /*
     * Load matrix columns once. vtfm3.t consumes R000/R001/R002 from M000
     * under n64psp's m[column][row] convention.
     */
    lv.q        R000, 0($a1)
    lv.q        R001, 16($a1)
    lv.q        R002, 32($a1)

    lv.q        R600, 0($t4)        /* ambient, reused for each vertex */
    vzero.q     R700                /* zero constant */

    lui         $t5, 0x3586
    ori         $t5, $t5, 0x37bd    /* 0.000001f */
    mtv         $t5, S702

    sll         $t9, $t1, 5
    addu        $t9, $a3, $t9      /* light_end */

    sll         $t1, $t2, 2
    addu        $t1, $a2, $t1      /* normal_end */

2:
    lw          $t5, 0($a2)

    /*
     * Decode little-endian packed signed bytes x/y/z. normal.w is ignored.
     */
    sll         $t6, $t5, 24
    sra         $t6, $t6, 24
    sll         $t7, $t5, 16
    sra         $t7, $t7, 24
    sll         $t8, $t5, 8
    sra         $t8, $t8, 24

    /*
     * R200 lanes are S200, S210 and S220.
     */
    mtv         $t6, S200
    mtv         $t7, S210
    mtv         $t8, S220
    vnop
    vnop
    vi2f.t      R200, R200, 0

    vtfm3.t     R201, M000, R200

    /*
     * Keep normalization temporaries away from R200 and R201.
     * R201 lanes are S201, S211 and S221.
     */
    vdot.t      S500, R201, R201

    vcmp.s      GT, S500, S702
    vnop
    bvf         0, 3f
    nop

    vrsq.s      S501, S500
    vscl.t      R201, R201, S501
3:
    vmov.t      R203, R600
    move        $t3, $a3

    beq         $t3, $t9, 5f
    nop

4:
    lv.q        R300, 0($t3)        /* light direction */
    lv.q        R301, 16($t3)       /* light color */

    /*
     * Keep the dot scalar separate from destination R302.
     */
    vdot.t      S503, R201, R300
    vmax.s      S503, S503, S700
    vscl.t      R302, R301, S503
    vadd.t      R203, R203, R302

    addiu       $t3, $t3, 32
    bne         $t3, $t9, 4b
    nop

5:
    vzero.s     S233
    sv.q        R203, 0($a0)

    addiu       $a2, $a2, 4
    bne         $a2, $t1, 2b
    addiu       $a0, $a0, 16

1:
    jr          $ra
    nop

    .size n64psp_directional_light_snorm8_batch_vfpu, \
        .-n64psp_directional_light_snorm8_batch_vfpu

    .set pop
