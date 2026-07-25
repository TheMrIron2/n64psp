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

    .align 2
    .global n64psp_tnl_transform_project_packed_batch_vfpu
    .type n64psp_tnl_transform_project_packed_batch_vfpu, @function

/*
 * a0 output streams
 * a1 modelview and projection matrices
 * a2 packed vertices
 * a3 has projection
 * t0 vertex count
 */
n64psp_tnl_transform_project_packed_batch_vfpu:
    beqz        $t0, 18f
    nop

    lv.q        R000, 0($a1)
    lv.q        R001, 16($a1)
    lv.q        R002, 32($a1)
    lv.q        R003, 48($a1)
    lv.q        R100, 64($a1)
    lv.q        R101, 80($a1)
    lv.q        R102, 96($a1)
    lv.q        R103, 112($a1)

    move        $v0, $a3
    move        $v1, $a0
    lw          $a0, 0($v1)
    lw          $a1, 4($v1)
    lw          $a3, 8($v1)
    lw          $t1, 16($v1)
    lw          $t2, 20($v1)
    lw          $t3, 24($v1)
    sll         $t0, $t0, 4
    addu        $t4, $a2, $t0

    lui         $t5, 0x3a83
    ori         $t5, $t5, 0x126f
    mtv         $t5, S702
    lui         $t5, 0x3b4c
    ori         $t5, $t5, 0xcccd
    mtv         $t5, S703
    lui         $t5, 0xbb88
    ori         $t5, $t5, 0x8889
    mtv         $t5, S713
    lui         $t5, 0x3980
    mtv         $t5, S723
    vzero.s     S700

9:
    lh          $t5, 0($a2)
    lh          $t6, 2($a2)
    lh          $t7, 4($a2)
    mtv         $t5, S200
    mtv         $t6, S210
    mtv         $t7, S220
    vnop
    vnop
    vi2f.t      R200, R200, 0
    vone.s      S230

    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R201
    sv.s        S201, 0($a0)
    sv.s        S211, 4($a0)
    sv.s        S221, 8($a0)
    sv.s        S231, 12($a0)

    beqz        $v0, 11f
    nop

    vabs.s      S603, S232
    vcmp.s      LT, S603, S702
    vnop
    bvt         0, 16f
    nop

    sv.s        S202, 0($a1)
    sv.s        S212, 4($a1)
    sv.s        S222, 8($a1)
    sv.s        S232, 12($a1)
    vrcp.s      S332, S232
    vscl.t      R302, R202, S332
    b           12f
    nop

11:
    vmul.t      R302, R201, R703
    vone.s      S332
    vmov.q      R202, R302
    sv.s        S302, 0($a1)
    sv.s        S312, 4($a1)
    sv.s        S322, 8($a1)
    sv.s        S332, 12($a1)

12:
    sv.s        S302, 0($a3)
    sv.s        S312, 4($a3)
    sv.s        S322, 8($a3)

    vone.t      R203
    vscl.t      R203, R203, S232
    vneg.t      R403, R203
    vcmp.t      LT, R202, R403
    vnop
    mfvc        $t5, $131
    andi        $t6, $t5, 1
    andi        $t7, $t5, 2
    sll         $t7, $t7, 1
    or          $t6, $t6, $t7
    andi        $t7, $t5, 4
    sll         $t7, $t7, 2
    or          $t6, $t6, $t7

    vcmp.t      GT, R202, R203
    vnop
    mfvc        $t5, $131
    andi        $t7, $t5, 1
    sll         $t7, $t7, 1
    or          $t6, $t6, $t7
    andi        $t7, $t5, 2
    sll         $t7, $t7, 2
    or          $t6, $t6, $t7
    andi        $t5, $t5, 4
    sll         $t5, $t5, 3
    or          $t6, $t6, $t5
    sw          $t6, 0($t1)
    li          $t5, 1
    b           17f
    sw          $t5, 0($t2)

16:
    sw          $zero, 0($t2)

17:
    addiu       $a2, $a2, 16
    addu        $a0, $a0, $t3
    addu        $a1, $a1, $t3
    addu        $a3, $a3, $t3
    addu        $t1, $t1, $t3
    bne         $a2, $t4, 9b
    addu        $t2, $t2, $t3

18:
    jr          $ra
    nop

    .size n64psp_tnl_transform_project_packed_batch_vfpu, \
        .-n64psp_tnl_transform_project_packed_batch_vfpu

    .align 2
    .global n64psp_tnl_transform_project_light_packed_batch_vfpu
    .type n64psp_tnl_transform_project_light_packed_batch_vfpu, @function

/*
 * a0 output streams
 * a1 modelview and projection matrices
 * a2 packed vertices
 * a3 lights
 * t0 ambient
 * t1 light count
 * t2 has projection
 * t3 vertex count
 */
n64psp_tnl_transform_project_light_packed_batch_vfpu:
    beqz        $t3, 28f
    nop

    lv.q        R000, 0($a1)
    lv.q        R001, 16($a1)
    lv.q        R002, 32($a1)
    lv.q        R003, 48($a1)
    lv.q        R100, 64($a1)
    lv.q        R101, 80($a1)
    lv.q        R102, 96($a1)
    lv.q        R103, 112($a1)
    lv.q        R600, 0($t0)

    move        $v0, $t2
    move        $v1, $a0
    sll         $t6, $t3, 4
    addu        $t6, $a2, $t6
    sll         $t7, $t1, 5
    addu        $t7, $a3, $t7
    li          $t8, 1
    beq         $t1, $t8, 31f
    nop
    li          $t8, 2
    bne         $t1, $t8, 30f
    nop
    lv.q        R400, 0($a3)
    lv.q        R401, 16($a3)
    lv.q        R402, 32($a3)
    lv.q        R502, 48($a3)
    move        $t7, $zero
    b           30f
    nop

31:
    lv.q        R400, 0($a3)
    lv.q        R401, 16($a3)
    li          $t7, 1

30:
    lw          $a0, 0($v1)
    lw          $a1, 4($v1)
    lw          $t0, 8($v1)
    lw          $t1, 12($v1)
    lw          $t2, 16($v1)
    lw          $t3, 20($v1)
    lw          $t4, 24($v1)
    lw          $t5, 28($v1)

    vzero.q     R700
    lui         $t8, 0x3a83
    ori         $t8, $t8, 0x126f
    mtv         $t8, S702
    lui         $t8, 0x3b4c
    ori         $t8, $t8, 0xcccd
    mtv         $t8, S703
    lui         $t8, 0xbb88
    ori         $t8, $t8, 0x8889
    mtv         $t8, S713
    lui         $t8, 0x3980
    mtv         $t8, S723
    lui         $t8, 0x3586
    ori         $t8, $t8, 0x37bd
    mtv         $t8, S733

19:
    lh          $t8, 0($a2)
    lh          $t9, 2($a2)
    lh          $at, 4($a2)
    mtv         $t8, S200
    mtv         $t9, S210
    mtv         $at, S220
    vnop
    vnop
    vi2f.t      R200, R200, 0
    vone.s      S230

    vtfm4.q     R201, M000, R200
    vtfm4.q     R202, M100, R201
    sv.s        S201, 0($a0)
    sv.s        S211, 4($a0)
    sv.s        S221, 8($a0)
    sv.s        S231, 12($a0)

    beqz        $v0, 21f
    nop

    vabs.s      S603, S232
    vcmp.s      LT, S603, S702
    vnop
    bvt         0, 26f
    nop

    sv.s        S202, 0($a1)
    sv.s        S212, 4($a1)
    sv.s        S222, 8($a1)
    sv.s        S232, 12($a1)
    vrcp.s      S332, S232
    vscl.t      R302, R202, S332
    b           22f
    nop

21:
    vmul.t      R302, R201, R703
    vone.s      S332
    vmov.q      R202, R302
    sv.s        S302, 0($a1)
    sv.s        S312, 4($a1)
    sv.s        S322, 8($a1)
    sv.s        S332, 12($a1)

22:
    sv.s        S302, 0($t0)
    sv.s        S312, 4($t0)
    sv.s        S322, 8($t0)

    vone.t      R203
    vscl.t      R203, R203, S232
    vneg.t      R403, R203
    vcmp.t      LT, R202, R403
    vnop
    mfvc        $t8, $131
    andi        $t9, $t8, 1
    andi        $at, $t8, 2
    sll         $at, $at, 1
    or          $t9, $t9, $at
    andi        $at, $t8, 4
    sll         $at, $at, 2
    or          $t9, $t9, $at

    vcmp.t      GT, R202, R203
    vnop
    mfvc        $t8, $131
    andi        $at, $t8, 1
    sll         $at, $at, 1
    or          $t9, $t9, $at
    andi        $at, $t8, 2
    sll         $at, $at, 2
    or          $t9, $t9, $at
    andi        $t8, $t8, 4
    sll         $t8, $t8, 3
    or          $t9, $t9, $t8
    sw          $t9, 0($t2)
    li          $t8, 1
    b           27f
    sw          $t8, 0($t3)

26:
    sw          $zero, 0($t3)

27:
    lw          $t8, 12($a2)
    sll         $t9, $t8, 24
    sra         $t9, $t9, 24
    sll         $at, $t8, 16
    sra         $at, $at, 24
    sll         $v1, $t8, 8
    sra         $v1, $v1, 24
    mtv         $t9, S300
    mtv         $at, S310
    mtv         $v1, S320
    vnop
    vnop
    vi2f.t      R300, R300, 0

    vtfm3.t     R301, M000, R300
    vdot.t      S603, R301, R301
    vcmp.s      GT, S603, S733
    vnop
    bvf         0, 24f
    nop
    vrsq.s      S613, S603
    vscl.t      R301, R301, S613

24:
    vmov.q      R500, R600
    beqz        $t7, 29f
    li          $t8, 1
    beq         $t7, $t8, 29f
    move        $v1, $a3
    beq         $v1, $t7, 25f
    nop

23:
    lv.q        R400, 0($v1)
    lv.q        R401, 16($v1)
    vdot.t      S623, R301, R400
    vmax.s      S623, S623, S700
    vscl.t      R402, R401, S623
    vadd.t      R500, R500, R402
    addiu       $v1, $v1, 32
    bne         $v1, $t7, 23b
    nop
    b           25f
    nop

29:
    vdot.t      S623, R301, R400
    vmax.s      S623, S623, S700
    vscl.t      R501, R401, S623
    vadd.t      R500, R500, R501
    bnez        $t7, 25f
    nop
    vdot.t      S623, R301, R402
    vmax.s      S623, S623, S700
    vscl.t      R501, R502, S623
    vadd.t      R500, R500, R501

25:
    vzero.s     S530
    sv.q        R500, 0($t1)

    addiu       $a2, $a2, 16
    addu        $a0, $a0, $t4
    addu        $a1, $a1, $t4
    addu        $t0, $t0, $t4
    addu        $t1, $t1, $t5
    addu        $t2, $t2, $t4
    bne         $a2, $t6, 19b
    addu        $t3, $t3, $t4

28:
    jr          $ra
    nop

    .size n64psp_tnl_transform_project_light_packed_batch_vfpu, \
        .-n64psp_tnl_transform_project_light_packed_batch_vfpu

    .set pop
