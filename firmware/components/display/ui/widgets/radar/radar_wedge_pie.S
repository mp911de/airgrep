/* Constant partial opacity across eight verified full-coverage pixels. */
    .section .text.radar_wedge_mix_blocks,"ax",@progbits
    .align 4
    .global radar_wedge_mix_blocks
    .type radar_wedge_mix_blocks,@function
radar_wedge_mix_blocks:
    entry a1, 16
    /* a2 destination, a3 block count, a4 constants, a5 reference background pair. */
    ee.vldbc.16.ip q5, a4, 2
    ee.vldbc.16.ip q6, a4, 2
    ee.vldbc.16.ip q1, a4, 2
    addi a6, a4, 2
    addi a7, a4, 4
    addi a8, a4, 6
.Lblock:
    l32i.n a9, a2, 0
    bne a9, a5, .Lmixed
    l32i.n a9, a2, 4
    bne a9, a5, .Lmixed
    l32i.n a9, a2, 8
    bne a9, a5, .Lmixed
    l32i.n a9, a2, 12
    bne a9, a5, .Lmixed
    ee.vldbc.16 q4, a8
    j .Lstore
.Lmixed:
    ee.vld.128.ip q0, a2, 0
    /* Each channel is bg + ((fg - bg) * weight >> 5). */
    ssai 5
    ee.andq q2, q0, q5
    ee.vldbc.16 q7, a4
    ee.vsubs.s16 q3, q7, q2
    ee.vmul.s16 q3, q3, q1
    ee.vadds.s16 q4, q2, q3
    ee.vsr.32 q2, q0
    ee.andq q2, q2, q6
    ee.vldbc.16 q7, a6
    ee.vsubs.s16 q3, q7, q2
    ee.vmul.s16 q3, q3, q1
    ee.vadds.s16 q3, q2, q3
    ee.vsl.32 q3, q3
    ee.orq q4, q4, q3
    ssai 11
    ee.vsr.32 q2, q0
    ee.andq q2, q2, q5
    ee.vldbc.16 q7, a7
    ee.vsubs.s16 q3, q7, q2
    ssai 5
    ee.vmul.s16 q3, q3, q1
    ee.vadds.s16 q3, q2, q3
    ssai 11
    ee.vsl.32 q3, q3
    ee.orq q4, q4, q3
.Lstore:
    ee.vst.128.ip q4, a2, 16
    addi.n a3, a3, -1
    bnez a3, .Lblock
    retw.n
    .size radar_wedge_mix_blocks, .-radar_wedge_mix_blocks
