/*
 * ARMv7 NEON optimizations for libjpeg-turbo
 *
 * Copyright (C) 2009-2011, Nokia Corporation and/or its subsidiary(-ies).
 *                          All Rights Reserved.
 * Author:  Siarhei Siamashka <siarhei.siamashka@nokia.com>
 * Copyright (C) 2014, Siarhei Siamashka.  All Rights Reserved.
 * Copyright (C) 2014, Linaro Limited.  All Rights Reserved.
 * Copyright (C) 2015, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2015-2016, 2018, Matthieu Darbois.  All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack, "", %progbits  /* mark stack as non-executable */
#endif

.text
.fpu neon
.arch armv7a
.object_arch armv4
.arm
.syntax unified


#define RESPECT_STRICT_ALIGNMENT  1


/*****************************************************************************/

/* Supplementary macro for setting function attributes */
.macro asm_function fname
#ifdef __APPLE__
    .private_extern _\fname
    .globl _\fname
_\fname:
#else
    .global \fname
#ifdef __ELF__
    .hidden \fname
    .type \fname, %function
#endif
\fname:
#endif
.endm

/* Transpose a block of 4x4 coefficients in four 64-bit registers */
.macro transpose_4x4 x0, x1, x2, x3
    vtrn.16         \x0, \x1
    vtrn.16         \x2, \x3
    vtrn.32         \x0, \x2
    vtrn.32         \x1, \x3
.endm


#define CENTERJSAMPLE  128


/*****************************************************************************/

/*
 * jsimd_idct_4x4_neon
 *
 * This function contains inverse-DCT code for getting reduced-size
 * 4x4 pixels output from an 8x8 DCT block. It uses the same  calculations
 * and produces exactly the same output as IJG's original 'jpeg_idct_4x4'
 * function from jpeg-6b (jidctred.c).
 *
 * NOTE: jpeg-8 has an improved implementation of 4x4 inverse-DCT, which
 *       requires much less arithmetic operations and hence should be faster.
 *       The primary purpose of this particular NEON optimized function is
 *       bit exact compatibility with jpeg-6b.
 *
 * TODO: a bit better instructions scheduling can be achieved by expanding
 *       idct_helper/transpose_4x4 macros and reordering instructions,
 *       but readability will suffer somewhat.
 */

#define CONST_BITS  13

#define FIX_0_211164243  (1730)   /* FIX(0.211164243) */
#define FIX_0_509795579  (4176)   /* FIX(0.509795579) */
#define FIX_0_601344887  (4926)   /* FIX(0.601344887) */
#define FIX_0_765366865  (6270)   /* FIX(0.765366865) */
#define FIX_0_899976223  (7373)   /* FIX(0.899976223) */
#define FIX_1_061594337  (8697)   /* FIX(1.061594337) */
#define FIX_1_451774981  (11893)  /* FIX(1.451774981) */
#define FIX_1_847759065  (15137)  /* FIX(1.847759065) */
#define FIX_2_172734803  (17799)  /* FIX(2.172734803) */
#define FIX_2_562915447  (20995)  /* FIX(2.562915447) */

.balign 16
jsimd_idct_4x4_neon_consts:
  .short FIX_1_847759065        /* d0[0] */
  .short -FIX_0_765366865       /* d0[1] */
  .short -FIX_0_211164243       /* d0[2] */
  .short FIX_1_451774981        /* d0[3] */
  .short -FIX_2_172734803       /* d1[0] */
  .short FIX_1_061594337        /* d1[1] */
  .short -FIX_0_509795579       /* d1[2] */
  .short -FIX_0_601344887       /* d1[3] */
  .short FIX_0_899976223        /* d2[0] */
  .short FIX_2_562915447        /* d2[1] */
  .short 1 << (CONST_BITS + 1)  /* d2[2] */
  .short 0                      /* d2[3] */

.macro idct_helper x4, x6, x8, x10, x12, x14, x16, shift, y26, y27, y28, y29
    vmull.s16       q14, \x4, d2[2]
    vmlal.s16       q14, \x8, d0[0]
    vmlal.s16       q14, \x14, d0[1]

    vmull.s16       q13, \x16, d1[2]
    vmlal.s16       q13, \x12, d1[3]
    vmlal.s16       q13, \x10, d2[0]
    vmlal.s16       q13, \x6, d2[1]

    vmull.s16       q15, \x4, d2[2]
    vmlsl.s16       q15, \x8, d0[0]
    vmlsl.s16       q15, \x14, d0[1]

    vmull.s16       q12, \x16, d0[2]
    vmlal.s16       q12, \x12, d0[3]
    vmlal.s16       q12, \x10, d1[0]
    vmlal.s16       q12, \x6, d1[1]

    vadd.s32        q10, q14, q13
    vsub.s32        q14, q14, q13

  .if \shift > 16
    vrshr.s32       q10, q10, #\shift
    vrshr.s32       q14, q14, #\shift
    vmovn.s32       \y26, q10
    vmovn.s32       \y29, q14
  .else
    vrshrn.s32      \y26, q10, #\shift
    vrshrn.s32      \y29, q14, #\shift
  .endif

    vadd.s32        q10, q15, q12
    vsub.s32        q15, q15, q12

  .if \shift > 16
    vrshr.s32       q10, q10, #\shift
    vrshr.s32       q15, q15, #\shift
    vmovn.s32       \y27, q10
    vmovn.s32       \y28, q15
  .else
    vrshrn.s32      \y27, q10, #\shift
    vrshrn.s32      \y28, q15, #\shift
  .endif
.endm

asm_function jsimd_idct_4x4_neon

    DCT_TABLE       .req r0
    COEF_BLOCK      .req r1
    OUTPUT_BUF      .req r2
    OUTPUT_COL      .req r3
    TMP1            .req r0
    TMP2            .req r1
    TMP3            .req r2
    TMP4            .req ip

    vpush           {d8-d15}

    /* Load constants (d3 is just used for padding) */
    adr             TMP4, jsimd_idct_4x4_neon_consts
    vld1.16         {d0, d1, d2, d3}, [TMP4, :128]

    /* Load all COEF_BLOCK into NEON registers with the following allocation:
     *       0 1 2 3 | 4 5 6 7
     *      ---------+--------
     *   0 | d4      | d5
     *   1 | d6      | d7
     *   2 | d8      | d9
     *   3 | d10     | d11
     *   4 | -       | -
     *   5 | d12     | d13
     *   6 | d14     | d15
     *   7 | d16     | d17
     */
    vld1.16         {d4, d5, d6, d7}, [COEF_BLOCK, :128]!
    vld1.16         {d8, d9, d10, d11}, [COEF_BLOCK, :128]!
    add COEF_BLOCK, COEF_BLOCK, #16
    vld1.16         {d12, d13, d14, d15}, [COEF_BLOCK, :128]!
    vld1.16         {d16, d17}, [COEF_BLOCK, :128]!
    /* dequantize */
    vld1.16         {d18, d19, d20, d21}, [DCT_TABLE, :128]!
    vmul.s16        q2, q2, q9
    vld1.16         {d22, d23, d24, d25}, [DCT_TABLE, :128]!
    vmul.s16        q3, q3, q10
    vmul.s16        q4, q4, q11
    add             DCT_TABLE, DCT_TABLE, #16
    vld1.16         {d26, d27, d28, d29}, [DCT_TABLE, :128]!
    vmul.s16        q5, q5, q12
    vmul.s16        q6, q6, q13
    vld1.16         {d30, d31}, [DCT_TABLE, :128]!
    vmul.s16        q7, q7, q14
    vmul.s16        q8, q8, q15

    /* Pass 1 */
    idct_helper     d4, d6, d8, d10, d12, d14, d16, 12, d4, d6, d8, d10
    transpose_4x4   d4, d6, d8, d10
    idct_helper     d5, d7, d9, d11, d13, d15, d17, 12, d5, d7, d9, d11
    transpose_4x4   d5, d7, d9, d11

    /* Pass 2 */
    idct_helper     d4, d6, d8, d10, d7, d9, d11, 19, d26, d27, d28, d29
    transpose_4x4   d26, d27, d28, d29

    /* Range limit */
    vmov.u16        q15, #0x80
    vadd.s16        q13, q13, q15
    vadd.s16        q14, q14, q15
    vqmovun.s16     d26, q13
    vqmovun.s16     d27, q14

    /* Store results to the output buffer */
    ldmia           OUTPUT_BUF, {TMP1, TMP2, TMP3, TMP4}
    add             TMP1, TMP1, OUTPUT_COL
    add             TMP2, TMP2, OUTPUT_COL
    add             TMP3, TMP3, OUTPUT_COL
    add             TMP4, TMP4, OUTPUT_COL

#if defined(__ARMEL__) && !RESPECT_STRICT_ALIGNMENT
    /* We can use much less instructions on little endian systems if the
     * OS kernel is not configured to trap unaligned memory accesses
     */
    vst1.32         {d26[0]}, [TMP1]!
    vst1.32         {d27[0]}, [TMP3]!
    vst1.32         {d26[1]}, [TMP2]!
    vst1.32         {d27[1]}, [TMP4]!
#else
    vst1.8          {d26[0]}, [TMP1]!
    vst1.8          {d27[0]}, [TMP3]!
    vst1.8          {d26[1]}, [TMP1]!
    vst1.8          {d27[1]}, [TMP3]!
    vst1.8          {d26[2]}, [TMP1]!
    vst1.8          {d27[2]}, [TMP3]!
    vst1.8          {d26[3]}, [TMP1]!
    vst1.8          {d27[3]}, [TMP3]!

    vst1.8          {d26[4]}, [TMP2]!
    vst1.8          {d27[4]}, [TMP4]!
    vst1.8          {d26[5]}, [TMP2]!
    vst1.8          {d27[5]}, [TMP4]!
    vst1.8          {d26[6]}, [TMP2]!
    vst1.8          {d27[6]}, [TMP4]!
    vst1.8          {d26[7]}, [TMP2]!
    vst1.8          {d27[7]}, [TMP4]!
#endif

    vpop            {d8-d15}
    bx              lr

    .unreq          DCT_TABLE
    .unreq          COEF_BLOCK
    .unreq          OUTPUT_BUF
    .unreq          OUTPUT_COL
    .unreq          TMP1
    .unreq          TMP2
    .unreq          TMP3
    .unreq          TMP4

.purgem idct_helper
