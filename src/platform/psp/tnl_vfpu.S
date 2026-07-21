    .text
    .set push
    .set noreorder
    .set noat

    .align 2
    .global n64psp_tnl_transform_packed_batch_vfpu
    .type n64psp_tnl_transform_packed_batch_vfpu, @function

/*
 * a0 transform output
 * a1 modelview and projection matrices
 * a2 packed vertices
 * a3 vertex count
 */
n64psp_tnl_transform_packed_batch_vfpu:
    beqz        $a3, 2f
    nop

    lv.q        R000, 0($a1)
    lv.q        R001, 16($a1)
    lv.q        R002, 32($a1)
    lv.q        R003, 48($a1)

    lv.q        R100, 64($a1)
    lv.q        R101, 80($a1)
    lv.q        R102, 96($a1)
    lv.q        R103, 112($a1)

    sll         $a3, $a3, 4
    addu        $a3, $a2, $a3

1:
    lh          $t0, 0($a2)
    lh          $t1, 2($a2)
    lh          $t2, 4($a2)
    mtv         $t0, S200
    mtv         $t1, S210
    mtv         $t2, S220
    vnop
    vnop
    vi2f.t      R200, R200, 0
    vone.s      S230

    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R201

    sv.q        R201, 0($a0)
    sv.q        R202, 16($a0)

    addiu       $a2, $a2, 16
    bne         $a2, $a3, 1b
    addiu       $a0, $a0, 32

2:
    jr          $ra
    nop

    .size n64psp_tnl_transform_packed_batch_vfpu, \
        .-n64psp_tnl_transform_packed_batch_vfpu

    .align 2
    .global n64psp_tnl_transform_light_packed_batch_vfpu
    .type n64psp_tnl_transform_light_packed_batch_vfpu, @function

/*
 * a0 transform output
 * a1 lighting output
 * a2 modelview and projection matrices
 * a3 packed vertices
 * t0 lights
 * t1 ambient
 * t2 light count
 * t3 vertex count
 */
n64psp_tnl_transform_light_packed_batch_vfpu:
    beqz        $t3, 8f
    nop

    lv.q        R000, 0($a2)
    lv.q        R001, 16($a2)
    lv.q        R002, 32($a2)
    lv.q        R003, 48($a2)

    lv.q        R100, 64($a2)
    lv.q        R101, 80($a2)
    lv.q        R102, 96($a2)
    lv.q        R103, 112($a2)

    lv.q        R600, 0($t1)
    vzero.q     R700
    lui         $t4, 0x3586
    ori         $t4, $t4, 0x37bd
    mtv         $t4, S702

    sll         $t8, $t3, 4
    addu        $t8, $a3, $t8
    sll         $t9, $t2, 5
    addu        $t9, $t0, $t9

3:
    lh          $t4, 0($a3)
    lh          $t5, 2($a3)
    lh          $t6, 4($a3)
    mtv         $t4, S200
    mtv         $t5, S210
    mtv         $t6, S220
    vnop
    vnop
    vi2f.t      R200, R200, 0
    vone.s      S230

    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R201
    sv.q        R201, 0($a0)
    sv.q        R202, 16($a0)

    lw          $t4, 12($a3)
    sll         $t5, $t4, 24
    sra         $t5, $t5, 24
    sll         $t6, $t4, 16
    sra         $t6, $t6, 24
    sll         $t7, $t4, 8
    sra         $t7, $t7, 24
    mtv         $t5, S300
    mtv         $t6, S310
    mtv         $t7, S320
    vnop
    vnop
    vi2f.t      R300, R300, 0

    vtfm3.t     R301, M000, R300
    vdot.t      S603, R301, R301
    vcmp.s      GT, S603, S702
    vnop
    bvf         0, 4f
    nop
    vrsq.s      S613, S603
    vscl.t      R301, R301, S613

4:
    vmov.q      R500, R600
    move        $t3, $t0
    beq         $t3, $t9, 6f
    nop

5:
    lv.q        R400, 0($t3)
    lv.q        R401, 16($t3)
    vdot.t      S623, R301, R400
    vmax.s      S623, S623, S700
    vscl.t      R402, R401, S623
    vadd.t      R500, R500, R402
    addiu       $t3, $t3, 32
    bne         $t3, $t9, 5b
    nop

6:
    vzero.s     S530
    sv.q        R500, 0($a1)

    addiu       $a3, $a3, 16
    addiu       $a0, $a0, 32
    bne         $a3, $t8, 3b
    addiu       $a1, $a1, 16

8:
    jr          $ra
    nop

    .size n64psp_tnl_transform_light_packed_batch_vfpu, \
        .-n64psp_tnl_transform_light_packed_batch_vfpu

    .set pop
