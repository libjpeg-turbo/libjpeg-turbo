/*
 * ARMv8 NEON optimizations for libjpeg-turbo
 *
 * Copyright (C) 2009-2011, Nokia Corporation and/or its subsidiary(-ies).
 *                          All Rights Reserved.
 * Author:  Siarhei Siamashka <siarhei.siamashka@nokia.com>
 * Copyright (C) 2013-2014, Linaro Limited.  All Rights Reserved.
 * Author:  Ragesh Radhakrishnan <ragesh.r@linaro.org>
 * Copyright (C) 2014-2016, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2015-2016, 2018, Matthieu Darbois.  All Rights Reserved.
 * Copyright (C) 2016, Siarhei Siamashka.  All Rights Reserved.
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

#if defined(__APPLE__)
.section __DATA, __const
#else
.section .rodata, "a", %progbits
#endif

/* Constants for jsimd_idct_4x4_neon() and jsimd_idct_2x2_neon() */

#define CONST_BITS  13

#define FIX_0_211164243  (1730)   /* FIX(0.211164243) */
#define FIX_0_509795579  (4176)   /* FIX(0.509795579) */
#define FIX_0_601344887  (4926)   /* FIX(0.601344887) */
#define FIX_0_720959822  (5906)   /* FIX(0.720959822) */
#define FIX_0_765366865  (6270)   /* FIX(0.765366865) */
#define FIX_0_850430095  (6967)   /* FIX(0.850430095) */
#define FIX_0_899976223  (7373)   /* FIX(0.899976223) */
#define FIX_1_061594337  (8697)   /* FIX(1.061594337) */
#define FIX_1_272758580  (10426)  /* FIX(1.272758580) */
#define FIX_1_451774981  (11893)  /* FIX(1.451774981) */
#define FIX_1_847759065  (15137)  /* FIX(1.847759065) */
#define FIX_2_172734803  (17799)  /* FIX(2.172734803) */
#define FIX_2_562915447  (20995)  /* FIX(2.562915447) */
#define FIX_3_624509785  (29692)  /* FIX(3.624509785) */

.balign 16
Ljsimd_idct_4x4_neon_consts:
  .short FIX_1_847759065        /* v0.h[0] */
  .short -FIX_0_765366865       /* v0.h[1] */
  .short -FIX_0_211164243       /* v0.h[2] */
  .short FIX_1_451774981        /* v0.h[3] */
  .short -FIX_2_172734803       /* d1[0] */
  .short FIX_1_061594337        /* d1[1] */
  .short -FIX_0_509795579       /* d1[2] */
  .short -FIX_0_601344887       /* d1[3] */
  .short FIX_0_899976223        /* v2.h[0] */
  .short FIX_2_562915447        /* v2.h[1] */
  .short 1 << (CONST_BITS + 1)  /* v2.h[2] */
  .short 0                      /* v2.h[3] */

.balign 8
Ljsimd_idct_2x2_neon_consts:
  .short -FIX_0_720959822  /* v14[0] */
  .short FIX_0_850430095   /* v14[1] */
  .short -FIX_1_272758580  /* v14[2] */
  .short FIX_3_624509785   /* v14[3] */

/* Constants for jsimd_encode_mcu_AC_refine_prepare_neon() */

.balign 16
Ljsimd_encode_mcu_AC_refine_prepare_neon_consts:
    .byte 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, \
          0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80

.text


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

/* Get symbol location */
.macro get_symbol_loc reg, symbol
#ifdef __APPLE__
    adrp            \reg, \symbol@PAGE
    add             \reg, \reg, \symbol@PAGEOFF
#else
    adrp            \reg, \symbol
    add             \reg, \reg, :lo12:\symbol
#endif
.endm

/* Transpose elements of single 128 bit registers */
.macro transpose_single x0, x1, xi, xilen, literal
    ins             \xi\xilen[0], \x0\xilen[0]
    ins             \x1\xilen[0], \x0\xilen[1]
    trn1            \x0\literal, \x0\literal, \x1\literal
    trn2            \x1\literal, \xi\literal, \x1\literal
.endm

/* Transpose elements of 2 different registers */
.macro transpose x0, x1, xi, xilen, literal
    mov             \xi\xilen, \x0\xilen
    trn1            \x0\literal, \x0\literal, \x1\literal
    trn2            \x1\literal, \xi\literal, \x1\literal
.endm

/* Transpose a block of 4x4 coefficients in four 64-bit registers */
.macro transpose_4x4_32 x0, x0len, x1, x1len, x2, x2len, x3, x3len, xi, xilen
    mov             \xi\xilen, \x0\xilen
    trn1            \x0\x0len, \x0\x0len, \x2\x2len
    trn2            \x2\x2len, \xi\x0len, \x2\x2len
    mov             \xi\xilen, \x1\xilen
    trn1            \x1\x1len, \x1\x1len, \x3\x3len
    trn2            \x3\x3len, \xi\x1len, \x3\x3len
.endm

.macro transpose_4x4_16 x0, x0len, x1, x1len, x2, x2len, x3, x3len, xi, xilen
    mov             \xi\xilen, \x0\xilen
    trn1            \x0\x0len, \x0\x0len, \x1\x1len
    trn2            \x1\x2len, \xi\x0len, \x1\x2len
    mov             \xi\xilen, \x2\xilen
    trn1            \x2\x2len, \x2\x2len, \x3\x3len
    trn2            \x3\x2len, \xi\x1len, \x3\x3len
.endm

.macro transpose_4x4 x0, x1, x2, x3, x5
    transpose_4x4_16 \x0, .4h, \x1, .4h, \x2, .4h, \x3, .4h, \x5, .16b
    transpose_4x4_32 \x0, .2s, \x1, .2s, \x2, .2s, \x3, .2s, \x5, .16b
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

.macro idct_helper x4, x6, x8, x10, x12, x14, x16, shift, y26, y27, y28, y29
    smull           v28.4s, \x4, v2.h[2]
    smlal           v28.4s, \x8, v0.h[0]
    smlal           v28.4s, \x14, v0.h[1]

    smull           v26.4s, \x16, v1.h[2]
    smlal           v26.4s, \x12, v1.h[3]
    smlal           v26.4s, \x10, v2.h[0]
    smlal           v26.4s, \x6, v2.h[1]

    smull           v30.4s, \x4, v2.h[2]
    smlsl           v30.4s, \x8, v0.h[0]
    smlsl           v30.4s, \x14, v0.h[1]

    smull           v24.4s, \x16, v0.h[2]
    smlal           v24.4s, \x12, v0.h[3]
    smlal           v24.4s, \x10, v1.h[0]
    smlal           v24.4s, \x6, v1.h[1]

    add             v20.4s, v28.4s, v26.4s
    sub             v28.4s, v28.4s, v26.4s

  .if \shift > 16
    srshr           v20.4s, v20.4s, #\shift
    srshr           v28.4s, v28.4s, #\shift
    xtn             \y26, v20.4s
    xtn             \y29, v28.4s
  .else
    rshrn           \y26, v20.4s, #\shift
    rshrn           \y29, v28.4s, #\shift
  .endif

    add             v20.4s, v30.4s, v24.4s
    sub             v30.4s, v30.4s, v24.4s

  .if \shift > 16
    srshr           v20.4s, v20.4s, #\shift
    srshr           v30.4s, v30.4s, #\shift
    xtn             \y27, v20.4s
    xtn             \y28, v30.4s
  .else
    rshrn           \y27, v20.4s, #\shift
    rshrn           \y28, v30.4s, #\shift
  .endif
.endm

asm_function jsimd_idct_4x4_neon

    DCT_TABLE       .req x0
    COEF_BLOCK      .req x1
    OUTPUT_BUF      .req x2
    OUTPUT_COL      .req x3
    TMP1            .req x0
    TMP2            .req x1
    TMP3            .req x2
    TMP4            .req x15

    /* OUTPUT_COL is a JDIMENSION (unsigned int) argument, so the ABI doesn't
       guarantee that the upper (unused) 32 bits of x3 are valid.  This
       instruction ensures that those bits are set to zero. */
    uxtw x3, w3

    /* Save all used NEON registers */
    sub             sp, sp, 64
    mov             x9, sp
    /* Load constants (v3.4h is just used for padding) */
    get_symbol_loc  TMP4, Ljsimd_idct_4x4_neon_consts
    st1             {v8.8b, v9.8b, v10.8b, v11.8b}, [x9], 32
    st1             {v12.8b, v13.8b, v14.8b, v15.8b}, [x9], 32
    ld1             {v0.4h, v1.4h, v2.4h, v3.4h}, [TMP4]

    /* Load all COEF_BLOCK into NEON registers with the following allocation:
     *       0 1 2 3 | 4 5 6 7
     *      ---------+--------
     *   0 | v4.4h   | v5.4h
     *   1 | v6.4h   | v7.4h
     *   2 | v8.4h   | v9.4h
     *   3 | v10.4h  | v11.4h
     *   4 | -       | -
     *   5 | v12.4h  | v13.4h
     *   6 | v14.4h  | v15.4h
     *   7 | v16.4h  | v17.4h
     */
    ld1             {v4.4h, v5.4h, v6.4h, v7.4h}, [COEF_BLOCK], 32
    ld1             {v8.4h, v9.4h, v10.4h, v11.4h}, [COEF_BLOCK], 32
    add             COEF_BLOCK, COEF_BLOCK, #16
    ld1             {v12.4h, v13.4h, v14.4h, v15.4h}, [COEF_BLOCK], 32
    ld1             {v16.4h, v17.4h}, [COEF_BLOCK], 16
    /* dequantize */
    ld1             {v18.4h, v19.4h, v20.4h, v21.4h}, [DCT_TABLE], 32
    mul             v4.4h, v4.4h, v18.4h
    mul             v5.4h, v5.4h, v19.4h
    ins             v4.d[1], v5.d[0]              /* 128 bit q4 */
    ld1             {v22.4h, v23.4h, v24.4h, v25.4h}, [DCT_TABLE], 32
    mul             v6.4h, v6.4h, v20.4h
    mul             v7.4h, v7.4h, v21.4h
    ins             v6.d[1], v7.d[0]              /* 128 bit q6 */
    mul             v8.4h, v8.4h, v22.4h
    mul             v9.4h, v9.4h, v23.4h
    ins             v8.d[1], v9.d[0]              /* 128 bit q8 */
    add             DCT_TABLE, DCT_TABLE, #16
    ld1             {v26.4h, v27.4h, v28.4h, v29.4h}, [DCT_TABLE], 32
    mul             v10.4h, v10.4h, v24.4h
    mul             v11.4h, v11.4h, v25.4h
    ins             v10.d[1], v11.d[0]            /* 128 bit q10 */
    mul             v12.4h, v12.4h, v26.4h
    mul             v13.4h, v13.4h, v27.4h
    ins             v12.d[1], v13.d[0]            /* 128 bit q12 */
    ld1             {v30.4h, v31.4h}, [DCT_TABLE], 16
    mul             v14.4h, v14.4h, v28.4h
    mul             v15.4h, v15.4h, v29.4h
    ins             v14.d[1], v15.d[0]            /* 128 bit q14 */
    mul             v16.4h, v16.4h, v30.4h
    mul             v17.4h, v17.4h, v31.4h
    ins             v16.d[1], v17.d[0]            /* 128 bit q16 */

    /* Pass 1 */
    idct_helper     v4.4h, v6.4h, v8.4h, v10.4h, v12.4h, v14.4h, v16.4h, 12, \
                    v4.4h, v6.4h, v8.4h, v10.4h
    transpose_4x4   v4, v6, v8, v10, v3
    ins             v10.d[1], v11.d[0]
    idct_helper     v5.4h, v7.4h, v9.4h, v11.4h, v13.4h, v15.4h, v17.4h, 12, \
                    v5.4h, v7.4h, v9.4h, v11.4h
    transpose_4x4   v5, v7, v9, v11, v3
    ins             v10.d[1], v11.d[0]

    /* Pass 2 */
    idct_helper     v4.4h, v6.4h, v8.4h, v10.4h, v7.4h, v9.4h, v11.4h, 19, \
                    v26.4h, v27.4h, v28.4h, v29.4h
    transpose_4x4   v26, v27, v28, v29, v3

    /* Range limit */
    movi            v30.8h, #0x80
    ins             v26.d[1], v27.d[0]
    ins             v28.d[1], v29.d[0]
    add             v26.8h, v26.8h, v30.8h
    add             v28.8h, v28.8h, v30.8h
    sqxtun          v26.8b, v26.8h
    sqxtun          v27.8b, v28.8h

    /* Store results to the output buffer */
    ldp             TMP1, TMP2, [OUTPUT_BUF], 16
    ldp             TMP3, TMP4, [OUTPUT_BUF]
    add             TMP1, TMP1, OUTPUT_COL
    add             TMP2, TMP2, OUTPUT_COL
    add             TMP3, TMP3, OUTPUT_COL
    add             TMP4, TMP4, OUTPUT_COL

#if defined(__ARMEL__) && !RESPECT_STRICT_ALIGNMENT
    /* We can use much less instructions on little endian systems if the
     * OS kernel is not configured to trap unaligned memory accesses
     */
    st1             {v26.s}[0], [TMP1], 4
    st1             {v27.s}[0], [TMP3], 4
    st1             {v26.s}[1], [TMP2], 4
    st1             {v27.s}[1], [TMP4], 4
#else
    st1             {v26.b}[0], [TMP1], 1
    st1             {v27.b}[0], [TMP3], 1
    st1             {v26.b}[1], [TMP1], 1
    st1             {v27.b}[1], [TMP3], 1
    st1             {v26.b}[2], [TMP1], 1
    st1             {v27.b}[2], [TMP3], 1
    st1             {v26.b}[3], [TMP1], 1
    st1             {v27.b}[3], [TMP3], 1

    st1             {v26.b}[4], [TMP2], 1
    st1             {v27.b}[4], [TMP4], 1
    st1             {v26.b}[5], [TMP2], 1
    st1             {v27.b}[5], [TMP4], 1
    st1             {v26.b}[6], [TMP2], 1
    st1             {v27.b}[6], [TMP4], 1
    st1             {v26.b}[7], [TMP2], 1
    st1             {v27.b}[7], [TMP4], 1
#endif

    /* vpop            {v8.4h - v15.4h}    ;not available */
    ld1             {v8.8b, v9.8b, v10.8b, v11.8b}, [sp], 32
    ld1             {v12.8b, v13.8b, v14.8b, v15.8b}, [sp], 32
    blr             x30

    .unreq          DCT_TABLE
    .unreq          COEF_BLOCK
    .unreq          OUTPUT_BUF
    .unreq          OUTPUT_COL
    .unreq          TMP1
    .unreq          TMP2
    .unreq          TMP3
    .unreq          TMP4

.purgem idct_helper


/*****************************************************************************/

/*
 * jsimd_idct_2x2_neon
 *
 * This function contains inverse-DCT code for getting reduced-size
 * 2x2 pixels output from an 8x8 DCT block. It uses the same  calculations
 * and produces exactly the same output as IJG's original 'jpeg_idct_2x2'
 * function from jpeg-6b (jidctred.c).
 *
 * NOTE: jpeg-8 has an improved implementation of 2x2 inverse-DCT, which
 *       requires much less arithmetic operations and hence should be faster.
 *       The primary purpose of this particular NEON optimized function is
 *       bit exact compatibility with jpeg-6b.
 */

.macro idct_helper x4, x6, x10, x12, x16, shift, y26, y27
    sshll           v15.4s, \x4, #15
    smull           v26.4s, \x6, v14.h[3]
    smlal           v26.4s, \x10, v14.h[2]
    smlal           v26.4s, \x12, v14.h[1]
    smlal           v26.4s, \x16, v14.h[0]

    add             v20.4s, v15.4s, v26.4s
    sub             v15.4s, v15.4s, v26.4s

  .if \shift > 16
    srshr           v20.4s, v20.4s, #\shift
    srshr           v15.4s, v15.4s, #\shift
    xtn             \y26, v20.4s
    xtn             \y27, v15.4s
  .else
    rshrn           \y26, v20.4s, #\shift
    rshrn           \y27, v15.4s, #\shift
  .endif
.endm

asm_function jsimd_idct_2x2_neon

    DCT_TABLE       .req x0
    COEF_BLOCK      .req x1
    OUTPUT_BUF      .req x2
    OUTPUT_COL      .req x3
    TMP1            .req x0
    TMP2            .req x15

    /* OUTPUT_COL is a JDIMENSION (unsigned int) argument, so the ABI doesn't
       guarantee that the upper (unused) 32 bits of x3 are valid.  This
       instruction ensures that those bits are set to zero. */
    uxtw x3, w3

    /* vpush           {v8.4h - v15.4h}            ; not available */
    sub             sp, sp, 64
    mov             x9, sp

    /* Load constants */
    get_symbol_loc  TMP2, Ljsimd_idct_2x2_neon_consts
    st1             {v8.8b, v9.8b, v10.8b, v11.8b}, [x9], 32
    st1             {v12.8b, v13.8b, v14.8b, v15.8b}, [x9], 32
    ld1             {v14.4h}, [TMP2]

    /* Load all COEF_BLOCK into NEON registers with the following allocation:
     *       0 1 2 3 | 4 5 6 7
     *      ---------+--------
     *   0 | v4.4h   | v5.4h
     *   1 | v6.4h   | v7.4h
     *   2 | -       | -
     *   3 | v10.4h  | v11.4h
     *   4 | -       | -
     *   5 | v12.4h  | v13.4h
     *   6 | -       | -
     *   7 | v16.4h  | v17.4h
     */
    ld1             {v4.4h, v5.4h, v6.4h, v7.4h}, [COEF_BLOCK], 32
    add             COEF_BLOCK, COEF_BLOCK, #16
    ld1             {v10.4h, v11.4h}, [COEF_BLOCK], 16
    add             COEF_BLOCK, COEF_BLOCK, #16
    ld1             {v12.4h, v13.4h}, [COEF_BLOCK], 16
    add             COEF_BLOCK, COEF_BLOCK, #16
    ld1             {v16.4h, v17.4h}, [COEF_BLOCK], 16
    /* Dequantize */
    ld1             {v18.4h, v19.4h, v20.4h, v21.4h}, [DCT_TABLE], 32
    mul             v4.4h, v4.4h, v18.4h
    mul             v5.4h, v5.4h, v19.4h
    ins             v4.d[1], v5.d[0]
    mul             v6.4h, v6.4h, v20.4h
    mul             v7.4h, v7.4h, v21.4h
    ins             v6.d[1], v7.d[0]
    add             DCT_TABLE, DCT_TABLE, #16
    ld1             {v24.4h, v25.4h}, [DCT_TABLE], 16
    mul             v10.4h, v10.4h, v24.4h
    mul             v11.4h, v11.4h, v25.4h
    ins             v10.d[1], v11.d[0]
    add             DCT_TABLE, DCT_TABLE, #16
    ld1             {v26.4h, v27.4h}, [DCT_TABLE], 16
    mul             v12.4h, v12.4h, v26.4h
    mul             v13.4h, v13.4h, v27.4h
    ins             v12.d[1], v13.d[0]
    add             DCT_TABLE, DCT_TABLE, #16
    ld1             {v30.4h, v31.4h}, [DCT_TABLE], 16
    mul             v16.4h, v16.4h, v30.4h
    mul             v17.4h, v17.4h, v31.4h
    ins             v16.d[1], v17.d[0]

    /* Pass 1 */
#if 0
    idct_helper     v4.4h, v6.4h, v10.4h, v12.4h, v16.4h, 13, v4.4h, v6.4h
    transpose_4x4   v4.4h, v6.4h, v8.4h, v10.4h
    idct_helper     v5.4h, v7.4h, v11.4h, v13.4h, v17.4h, 13, v5.4h, v7.4h
    transpose_4x4   v5.4h, v7.4h, v9.4h, v11.4h
#else
    smull           v26.4s, v6.4h, v14.h[3]
    smlal           v26.4s, v10.4h, v14.h[2]
    smlal           v26.4s, v12.4h, v14.h[1]
    smlal           v26.4s, v16.4h, v14.h[0]
    smull           v24.4s, v7.4h, v14.h[3]
    smlal           v24.4s, v11.4h, v14.h[2]
    smlal           v24.4s, v13.4h, v14.h[1]
    smlal           v24.4s, v17.4h, v14.h[0]
    sshll           v15.4s, v4.4h, #15
    sshll           v30.4s, v5.4h, #15
    add             v20.4s, v15.4s, v26.4s
    sub             v15.4s, v15.4s, v26.4s
    rshrn           v4.4h, v20.4s, #13
    rshrn           v6.4h, v15.4s, #13
    add             v20.4s, v30.4s, v24.4s
    sub             v15.4s, v30.4s, v24.4s
    rshrn           v5.4h, v20.4s, #13
    rshrn           v7.4h, v15.4s, #13
    ins             v4.d[1], v5.d[0]
    ins             v6.d[1], v7.d[0]
    transpose       v4, v6, v3, .16b, .8h
    transpose       v6, v10, v3, .16b, .4s
    ins             v11.d[0], v10.d[1]
    ins             v7.d[0], v6.d[1]
#endif

    /* Pass 2 */
    idct_helper     v4.4h, v6.4h, v10.4h, v7.4h, v11.4h, 20, v26.4h, v27.4h

    /* Range limit */
    movi            v30.8h, #0x80
    ins             v26.d[1], v27.d[0]
    add             v26.8h, v26.8h, v30.8h
    sqxtun          v30.8b, v26.8h
    ins             v26.d[0], v30.d[0]
    sqxtun          v27.8b, v26.8h

    /* Store results to the output buffer */
    ldp             TMP1, TMP2, [OUTPUT_BUF]
    add             TMP1, TMP1, OUTPUT_COL
    add             TMP2, TMP2, OUTPUT_COL

    st1             {v26.b}[0], [TMP1], 1
    st1             {v27.b}[4], [TMP1], 1
    st1             {v26.b}[1], [TMP2], 1
    st1             {v27.b}[5], [TMP2], 1

    ld1             {v8.8b, v9.8b, v10.8b, v11.8b}, [sp], 32
    ld1             {v12.8b, v13.8b, v14.8b, v15.8b}, [sp], 32
    blr             x30

    .unreq          DCT_TABLE
    .unreq          COEF_BLOCK
    .unreq          OUTPUT_BUF
    .unreq          OUTPUT_COL
    .unreq          TMP1
    .unreq          TMP2

.purgem idct_helper


/*****************************************************************************/

/*
 * Macros to load data for jsimd_encode_mcu_AC_first_prepare_neon() and
 * jsimd_encode_mcu_AC_refine_prepare_neon()
 */

.macro LOAD16
    ldr             T0d, [LUT, #(0*4)]
    ldr             T1d, [LUT, #(8*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[0], [T0]
    ld1             {Y1.h}[0], [T1]

    ldr             T0d, [LUT, #(1*4)]
    ldr             T1d, [LUT, #(9*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[1], [T0]
    ld1             {Y1.h}[1], [T1]

    ldr             T0d, [LUT, #(2*4)]
    ldr             T1d, [LUT, #(10*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[2], [T0]
    ld1             {Y1.h}[2], [T1]

    ldr             T0d, [LUT, #(3*4)]
    ldr             T1d, [LUT, #(11*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[3], [T0]
    ld1             {Y1.h}[3], [T1]

    ldr             T0d, [LUT, #(4*4)]
    ldr             T1d, [LUT, #(12*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[4], [T0]
    ld1             {Y1.h}[4], [T1]

    ldr             T0d, [LUT, #(5*4)]
    ldr             T1d, [LUT, #(13*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[5], [T0]
    ld1             {Y1.h}[5], [T1]

    ldr             T0d, [LUT, #(6*4)]
    ldr             T1d, [LUT, #(14*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[6], [T0]
    ld1             {Y1.h}[6], [T1]

    ldr             T0d, [LUT, #(7*4)]
    ldr             T1d, [LUT, #(15*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[7], [T0]
    ld1             {Y1.h}[7], [T1]

    add             LUT, LUT, #(16*4)
.endm

.macro LOAD15
    eor             Y1.16b, Y1.16b, Y1.16b

    ldr             T0d, [LUT, #(0*4)]
    ldr             T1d, [LUT, #(8*4)]
    add             T0, BLOCK, T0, lsl #1
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[0], [T0]
    ld1             {Y1.h}[0], [T1]

    ldr             T0d, [LUT, #(1*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[1], [T0]

    ldr             T0d, [LUT, #(2*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[2], [T0]

    ldr             T0d, [LUT, #(3*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[3], [T0]

    ldr             T0d, [LUT, #(4*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[4], [T0]

    ldr             T0d, [LUT, #(5*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[5], [T0]

    ldr             T0d, [LUT, #(6*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[6], [T0]

    ldr             T0d, [LUT, #(7*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[7], [T0]

    cmp             LENEND, #2
    b.lt            1515f
    ldr             T1d, [LUT, #(9*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y1.h}[1], [T1]

    cmp             LENEND, #3
    b.lt            1515f
    ldr             T1d, [LUT, #(10*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y1.h}[2], [T1]

    cmp             LENEND, #4
    b.lt            1515f
    ldr             T1d, [LUT, #(11*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y1.h}[3], [T1]

    cmp             LENEND, #5
    b.lt            1515f
    ldr             T1d, [LUT, #(12*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y1.h}[4], [T1]

    cmp             LENEND, #6
    b.lt            1515f
    ldr             T1d, [LUT, #(13*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y1.h}[5], [T1]

    cmp             LENEND, #7
    b.lt            1515f
    ldr             T1d, [LUT, #(14*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y1.h}[6], [T1]

1515:
.endm

.macro LOAD8
    ldr             T0d, [LUT, #(0*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[0], [T0]

    ldr             T0d, [LUT, #(1*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[1], [T0]

    ldr             T0d, [LUT, #(2*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[2], [T0]

    ldr             T0d, [LUT, #(3*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[3], [T0]

    ldr             T0d, [LUT, #(4*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[4], [T0]

    ldr             T0d, [LUT, #(5*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[5], [T0]

    ldr             T0d, [LUT, #(6*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[6], [T0]

    ldr             T0d, [LUT, #(7*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[7], [T0]
.endm

.macro LOAD7
    eor             Y0.16b, Y0.16b, Y0.16b

    ldr             T0d, [LUT, #(0*4)]
    add             T0, BLOCK, T0, lsl #1
    ld1             {Y0.h}[0], [T0]

    cmp             LENEND, #2
    b.lt            77f
    ldr             T1d, [LUT, #(1*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[1], [T1]

    cmp             LENEND, #3
    b.lt            77f
    ldr             T1d, [LUT, #(2*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[2], [T1]

    cmp             LENEND, #4
    b.lt            77f
    ldr             T1d, [LUT, #(3*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[3], [T1]

    cmp             LENEND, #5
    b.lt            77f
    ldr             T1d, [LUT, #(4*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[4], [T1]

    cmp             LENEND, #6
    b.lt            77f
    ldr             T1d, [LUT, #(5*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[5], [T1]

    cmp             LENEND, #7
    b.lt            77f
    ldr             T1d, [LUT, #(6*4)]
    add             T1, BLOCK, T1, lsl #1
    ld1             {Y0.h}[6], [T1]

77:
.endm

.macro REDUCE0
    ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [VALUES], #64
    ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [VALUES], #64

    cmeq            v0.8h, v0.8h, #0
    cmeq            v1.8h, v1.8h, #0
    cmeq            v2.8h, v2.8h, #0
    cmeq            v3.8h, v3.8h, #0
    cmeq            v4.8h, v4.8h, #0
    cmeq            v5.8h, v5.8h, #0
    cmeq            v6.8h, v6.8h, #0
    cmeq            v7.8h, v7.8h, #0

    xtn             v0.8b, v0.8h
    xtn             v2.8b, v2.8h
    xtn             v4.8b, v4.8h
    xtn             v6.8b, v6.8h
    xtn2            v0.16b, v1.8h
    xtn2            v2.16b, v3.8h
    xtn2            v4.16b, v5.8h
    xtn2            v6.16b, v7.8h

    and             v0.16b, v0.16b, ANDMASK.16b
    and             v2.16b, v2.16b, ANDMASK.16b
    and             v4.16b, v4.16b, ANDMASK.16b
    and             v6.16b, v6.16b, ANDMASK.16b
    addp            v0.16b, v0.16b, v2.16b
    addp            v4.16b, v4.16b, v6.16b
    addp            v0.16b, v0.16b, v4.16b
    addp            v0.16b, v0.16b, v0.16b
    umov            T0, v0.D[0]
    mvn             T0, T0
    str             T0, [BITS]
.endm

/*
 * Prepare data for jsimd_encode_mcu_AC_refine.
 *
 * GLOBAL(int)
 * jsimd_encode_mcu_AC_refine_prepare_neon(const JCOEF *block,
 *                                         const int *jpeg_natural_order_start,
 *                                         int Sl, int Al, JCOEF *absvalues,
 *                                         size_t *bits)
 *
 * x0 = const JCOEF *block
 * x1 = const int *jpeg_natural_order_start
 * w2 = int Sl
 * w3 = int Al
 * x4 = JCOEF *absvalues
 * x5 = size_t *bits
 *
 */

    ZERO            .req v0
    ONE             .req v1
    Y0              .req v2
    Y1              .req v3
    N0              .req v4
    N1              .req v5
    AL              .req v6
    ANDMASK         .req v20
    K               .req w12
    KK              .req w13
    EOB             .req w14
    SIGN            .req x15
    LUT             .req x1
    T0              .req x10
    T0d             .req w10
    T1              .req x11
    T1d             .req w11
    BLOCK           .req x0
    VALUES          .req x4
    LEN             .req w2
    LENEND          .req w9
    BITS            .req x5

asm_function jsimd_encode_mcu_AC_refine_prepare_neon
    get_symbol_loc  T0, Ljsimd_encode_mcu_AC_refine_prepare_neon_consts
    neg             w3, w3                        /* Al = -Al */
    movi            ONE.8h, #1
    eor             SIGN, SIGN, SIGN
    eor             ZERO.16b, ZERO.16b, ZERO.16b
    eor             EOB, EOB, EOB
    ld1             {ANDMASK.16b}, [T0]
    eor             KK, KK, KK
    dup             AL.8h, w3
    and             LENEND, LEN, 7
    lsr             K, LEN, 4
    cbz             K, 3f
1:
    LOAD16
    cmlt            N0.8h, Y0.8h, #0
    cmlt            N1.8h, Y1.8h, #0
    abs             Y0.8h, Y0.8h
    abs             Y1.8h, Y1.8h
    ushl            Y0.8h, Y0.8h, AL.8h
    ushl            Y1.8h, Y1.8h, AL.8h
    st1             {Y0.8h, Y1.8h}, [VALUES], #32
    xtn             N0.8b, N0.8h
    xtn             N1.8b, N1.8h
    cmeq            Y0.8h, Y0.8h, ONE.8h
    cmeq            Y1.8h, Y1.8h, ONE.8h
    xtn             Y0.8b, Y0.8h
    xtn             Y1.8b, Y1.8h
    and             N0.8b, N0.8b, ANDMASK.8b
    and             N1.8b, N1.8b, ANDMASK.8b
    and             Y0.8b, Y0.8b, ANDMASK.8b
    and             Y1.8b, Y1.8b, ANDMASK.8b
    addv            B28, N0.8b
    addv            B29, N1.8b
    addv            B30, Y0.8b
    addv            B31, Y1.8b
    ins             v28.b[1], v29.b[0]
    ins             v30.b[1], v31.b[0]
    umov            T0d, v28.h[0]    /* lsignbits.val16u[k>>4] = _mm_movemask_epi8(neg); */
    umov            T1d, v30.h[0]    /* idx = _mm_movemask_epi8(x1); */
    lsr             SIGN, SIGN, #16  /* make room for sizebits */
    orr             SIGN, SIGN, T0, lsl #48
    cbz             T1d, 2f
    rbit            T1d, T1d
    clz             T1d, T1d
    add             EOB, KK, T1d     /* EOB = k + idx; */
2:
    add             KK, KK, #16
    subs            K, K, #1
    b.ne            1b
3:
    tst             LEN, #8
    b.eq            3f
    tst             LEN, #7
    b.eq            2f

    LOAD15
    cmlt            N0.8h, Y0.8h, #0
    cmlt            N1.8h, Y1.8h, #0
    abs             Y0.8h, Y0.8h
    abs             Y1.8h, Y1.8h
    ushl            Y0.8h, Y0.8h, AL.8h
    ushl            Y1.8h, Y1.8h, AL.8h
    st1             {Y0.8h, Y1.8h}, [VALUES], #32
    xtn             N0.8b, N0.8h
    xtn             N1.8b, N1.8h
    cmeq            Y0.8h, Y0.8h, ONE.8h
    cmeq            Y1.8h, Y1.8h, ONE.8h
    xtn             Y0.8b, Y0.8h
    xtn             Y1.8b, Y1.8h
    and             N0.8b, N0.8b, ANDMASK.8b
    and             N1.8b, N1.8b, ANDMASK.8b
    and             Y0.8b, Y0.8b, ANDMASK.8b
    and             Y1.8b, Y1.8b, ANDMASK.8b
    addv            B28, N0.8b
    addv            B29, N1.8b
    addv            B30, Y0.8b
    addv            B31, Y1.8b
    ins             v28.b[1], v29.b[0]
    ins             v30.b[1], v31.b[0]
    umov            T0d, v28.h[0]    /* lsignbits.val16u[k>>4] = _mm_movemask_epi8(neg); */
    umov            T1d, v30.h[0]    /* idx = _mm_movemask_epi8(x1); */
    lsr             SIGN, SIGN, #16  /* make room for sizebits */
    orr             SIGN, SIGN, T0, lsl #48
    cbz             T1d, 4f
    rbit            T1d, T1d
    clz             T1d, T1d
    add             EOB, KK, T1d     /* EOB = k + idx; */
    b               4f
2:
    LOAD8
    cmlt            N0.8h, Y0.8h, #0
    abs             Y0.8h, Y0.8h
    ushl            Y0.8h, Y0.8h, AL.8h
    st1             {Y0.8h}, [VALUES], #16
    xtn             N0.8b, N0.8h
    cmeq            Y0.8h, Y0.8h, ONE.8h
    xtn             Y0.8b, Y0.8h
    and             N0.8b, N0.8b, ANDMASK.8b
    and             Y0.8b, Y0.8b, ANDMASK.8b
    addv            B28, N0.8b
    addv            B30, Y0.8b
    umov            T0d, v28.b[0]    /* lsignbits.val16u[k>>4] = _mm_movemask_epi8(neg); */
    umov            T1d, v30.b[0]    /* idx = _mm_movemask_epi8(x1); */
    lsr             SIGN, SIGN, #8   /* make room for sizebits */
    orr             SIGN, SIGN, T0, lsl #56
    cbz             T1d, 4f
    rbit            T1d, T1d
    clz             T1d, T1d
    add             EOB, KK, T1d     /* EOB = k + idx; */
    b               4f
3:
    cbz             LENEND, 4f
    LOAD7
    cmlt            N0.8h, Y0.8h, #0
    abs             Y0.8h, Y0.8h
    ushl            Y0.8h, Y0.8h, AL.8h
    st1             {Y0.8h}, [VALUES], #16
    xtn             N0.8b, N0.8h
    cmeq            Y0.8h, Y0.8h, ONE.8h
    xtn             Y0.8b, Y0.8h
    and             N0.8b, N0.8b, ANDMASK.8b
    and             Y0.8b, Y0.8b, ANDMASK.8b
    addv            B28, N0.8b
    addv            B30, Y0.8b
    umov            T0d, v28.b[0]    /* lsignbits.val16u[k>>4] = _mm_movemask_epi8(neg); */
    umov            T1d, v30.b[0]    /* idx = _mm_movemask_epi8(x1); */
    lsr             SIGN, SIGN, #8   /* make room for sizebits */
    orr             SIGN, SIGN, T0, lsl #56
    cbz             T1d, 4f
    rbit            T1d, T1d
    clz             T1d, T1d
    add             EOB, KK, T1d     /* EOB = k + idx; */
    /* b               4f */
    /* fallthrough */
4:
    add             K, LEN, #7
    lsr             K, K, #3
    subs            K, K, #(/*DCTSIZE2*/ 64 / 8)
    b.eq            5f
1:
    st1             {ZERO.8h}, [VALUES], #16
    lsr             SIGN, SIGN, #8
    adds            K, K, #1
    b.ne            1b
5:
    mvn             SIGN, SIGN
    sub             VALUES, VALUES, #(/*DCTSIZE2*/ 64 * 2)
    str             SIGN, [BITS, #8]

    REDUCE0

    mov             w0, EOB
    br              x30

    .unreq          ZERO
    .unreq          ONE
    .unreq          Y0
    .unreq          Y1
    .unreq          N0
    .unreq          N1
    .unreq          AL
    .unreq          ANDMASK
    .unreq          K
    .unreq          KK
    .unreq          EOB
    .unreq          SIGN
    .unreq          LUT
    .unreq          T0
    .unreq          T0d
    .unreq          T1
    .unreq          T1d
    .unreq          BLOCK
    .unreq          VALUES
    .unreq          LEN
    .unreq          LENEND
    .unreq          BITS

.purgem LOAD16
.purgem LOAD15
.purgem LOAD8
.purgem LOAD7
.purgem REDUCE0
