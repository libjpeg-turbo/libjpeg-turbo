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
#elif defined(_WIN32)
.section .rdata
#else
.section .rodata, "a", %progbits
#endif

/* Constants for jsimd_idct_4x4_neon() */

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
