/* Count inclusive intersections across eight signed 16-bit boxes per block. */
    .section .text.radar_label_intersections_s3,"ax",@progbits
    .align 4
    .global radar_label_intersections_s3
    .type radar_label_intersections_s3,@function
radar_label_intersections_s3:
    entry a1, 16
    /* a2: 16-byte-aligned blocks, a3: x1,x2,y1,y2, a4: positive block count. */
    ee.vldbc.16.ip q0, a3, 2
    ee.vldbc.16.ip q1, a3, 2
    ee.vldbc.16.ip q2, a3, 2
    ee.vldbc.16.ip q3, a3, 2
    ee.zero.q q7
.Llabel_block:
    ee.vld.128.ip q4, a2, 16
    ee.vcmp.gt.s16 q5, q4, q1
    ee.vld.128.ip q4, a2, 16
    ee.vcmp.gt.s16 q6, q0, q4
    ee.orq q5, q5, q6
    ee.vld.128.ip q4, a2, 16
    ee.vcmp.gt.s16 q6, q4, q3
    ee.orq q5, q5, q6
    ee.vld.128.ip q4, a2, 16
    ee.vcmp.gt.s16 q6, q2, q4
    ee.orq q5, q5, q6
    ee.notq q5, q5
    ee.vsubs.s16 q7, q7, q5       /* A surviving lane contains -1. */
    addi.n a4, a4, -1
    bnez a4, .Llabel_block
    ee.movi.32.a q7, a2, 0
    ee.movi.32.a q7, a5, 1
    add.n a2, a2, a5
    ee.movi.32.a q7, a5, 2
    add.n a2, a2, a5
    ee.movi.32.a q7, a5, 3
    add.n a2, a2, a5
    srli a5, a2, 16
    extui a2, a2, 0, 16
    add.n a2, a2, a5
    retw.n
    .size radar_label_intersections_s3, .-radar_label_intersections_s3
