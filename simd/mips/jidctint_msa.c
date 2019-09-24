/*
 * MIPS MSA optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * All rights reserved.
 * Authors: Vikram Dattu (vikram.dattu@imgtec.com)
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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"
#include "jmacros_msa.h"


#define DCTSIZE     8
#define CONST_BITS  13
#define PASS1_BITS  2

#define FIX_0_298631336  ((int32_t)2446)   /* FIX(0.298631336) */
#define FIX_0_390180644  ((int32_t)3196)   /* FIX(0.390180644) */
#define FIX_0_541196100  ((int32_t)4433)   /* FIX(0.541196100) */
#define FIX_0_765366865  ((int32_t)6270)   /* FIX(0.765366865) */
#define FIX_0_899976223  ((int32_t)7373)   /* FIX(0.899976223) */
#define FIX_1_175875602  ((int32_t)9633)   /* FIX(1.175875602) */
#define FIX_1_501321110  ((int32_t)12299)  /* FIX(1.501321110) */
#define FIX_1_847759065  ((int32_t)15137)  /* FIX(1.847759065) */
#define FIX_1_961570560  ((int32_t)16069)  /* FIX(1.961570560) */
#define FIX_2_053119869  ((int32_t)16819)  /* FIX(2.053119869) */
#define FIX_2_562915447  ((int32_t)20995)  /* FIX(2.562915447) */
#define FIX_3_072711026  ((int32_t)25172)  /* FIX(3.072711026) */

GLOBAL(void)
jsimd_idct_islow_msa(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                     JCOEFPTR coef_block, JSAMPARRAY output_buf,
                     JDIMENSION output_col)
{
  v16u8 res0, res1, res2, res3, res4, res5, res6, res7;
  v8i16 val0, val1, val2, val3, val4, val5, val6, val7;
  v8i16 quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7;
  v4i32 d0_r, d1_r, d2_r, d3_r, d4_r, d5_r, d6_r, d7_r;
  v4i32 d0_l, d1_l, d2_l, d3_l, d4_l, d5_l, d6_l, d7_l;
  v4i32 tmp_r, tmp_l, tmp2_r, tmp2_l, tmp3_r, tmp3_l, tmp0_r, tmp0_l, tmp1_r;
  v4i32 tmp1_l, tmp10_r, tmp10_l, tmp11_r, tmp11_l, tmp12_r, tmp12_l;
  v4i32 tmp13_r, tmp13_l, tmp, tmp1;
  v4i32 z1_r, z1_l, z2_r, z2_l, z3_r, z3_l, z4_r, z4_l, z5_r, z5_l;
  v4i32 dst0_r, dst1_r, dst2_r, dst3_r, dst4_r, dst5_r, dst6_r, dst7_r;
  v4i32 dst0_l, dst1_l, dst2_l, dst3_l, dst4_l, dst5_l, dst6_l, dst7_l;
  v8i16 reg_128 = __msa_ldi_h(128);
  JCOEFPTR quantptr = compptr->dct_table;
  v4i32 const0 =
    { FIX_0_541196100, -FIX_1_847759065, FIX_0_765366865, FIX_1_175875602 };
  v4i32 const1 =
    { -FIX_0_899976223, -FIX_2_562915447, -FIX_1_961570560, -FIX_0_390180644 };
  v4i32 const2 =
    { FIX_0_298631336, FIX_2_053119869, FIX_3_072711026, FIX_1_501321110 };

  /* Dequantize */
  LD_SH8(coef_block, DCTSIZE, val0, val1, val2, val3, val4, val5, val6, val7);
  quant0 = LD_SH(quantptr);
  /* Handle 0 AC terms */
  tmp_r = (v4i32)(val1 | val2 | val3 | val4 | val5 | val6 | val7);
  if (__msa_test_bz_v((v16u8)tmp_r)) {
    UNPCK_SH_SW(val0, d0_r, d0_l);
    UNPCK_SH_SW(quant0, d2_r, d2_l);
    MUL2(d0_r, d2_r, d0_l, d2_l, dst0_r, dst0_l);
    dst0_r = MSA_SLLI_W(dst0_r, PASS1_BITS);
    dst0_l = MSA_SLLI_W(dst0_l, PASS1_BITS);

    SPLATI_W4_SW(dst0_r, d0_r, d1_r, d2_r, d3_r);
    SPLATI_W4_SW(dst0_l, d4_r, d5_r, d6_r, d7_r);

    d0_l = d0_r;
    d1_l = d1_r;
    d2_l = d2_r;
    d3_l = d3_r;
    d4_l = d4_r;
    d5_l = d5_r;
    d6_l = d6_r;
    d7_l = d7_r;
  } else { /* if (!__msa_test_bz_v((v16u8)tmp_r)) */
    /* Pass 1: process columns. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */

    LD_SH6(quantptr + DCTSIZE, DCTSIZE, quant1, quant2, quant3, quant4,
           quant5, quant6);
    quant7 = LD_SH(quantptr + 7 * DCTSIZE);
    MUL4(val0, quant0, val1, quant1, val2, quant2, val3, quant3,
         val0, val1, val2, val3);
    MUL4(val4, quant4, val5, quant5, val6, quant6, val7, quant7,
         val4, val5, val6, val7);

    UNPCK_SH_SW(val0, d0_r, d0_l);
    UNPCK_SH_SW(val2, d2_r, d2_l);
    UNPCK_SH_SW(val4, d4_r, d4_l);
    UNPCK_SH_SW(val6, d6_r, d6_l);

    /* z1 = MULTIPLY(d2 + d6, FIX_0_541196100); */
    tmp1 = __msa_splati_w(const0, 0);
    ADD2(d2_r, d6_r, d2_l, d6_l, tmp_r, tmp_l);
    MUL2(tmp_r, tmp1, tmp_l, tmp1, z1_r, z1_l);

    /* tmp2 = z1 + MULTIPLY(-d6, FIX_1_847759065) */
    tmp1 = __msa_splati_w(const0, 1);
    MUL2(d6_r, tmp1, d6_l, tmp1, tmp2_r, tmp2_l);
    ADD2(tmp2_r, z1_r, tmp2_l, z1_l, tmp2_r, tmp2_l);

    /* tmp3 = z1 + MULTIPLY(d2, FIX_0_765366865); */
    tmp1 = __msa_splati_w(const0, 2);
    MUL2(d2_r, tmp1, d2_l, tmp1, tmp3_r, tmp3_l);
    ADD2(tmp3_r, z1_r, tmp3_l, z1_l, tmp3_r, tmp3_l);
    BUTTERFLY_4(d0_r, d0_l, d4_l, d4_r, tmp0_r, tmp0_l, tmp1_l, tmp1_r);

    SLLI_W4_SW(tmp0_r, tmp0_l, tmp1_r, tmp1_l, CONST_BITS);

    BUTTERFLY_8(tmp0_r, tmp0_l, tmp1_r, tmp1_l, tmp2_l, tmp2_r, tmp3_l, tmp3_r,
                tmp10_r, tmp10_l, tmp11_r, tmp11_l, tmp12_l, tmp12_r, tmp13_l,
                tmp13_r);

    UNPCK_SH_SW(val1, d1_r, d1_l);
    UNPCK_SH_SW(val3, d3_r, d3_l);
    UNPCK_SH_SW(val5, d5_r, d5_l);
    UNPCK_SH_SW(val7, d7_r, d7_l);
    /* z1 = d7 + d1 */
    /* z2 = d5 + d3 */
    ADD4(d7_r, d1_r, d7_l, d1_l, d5_r, d3_r, d5_l, d3_l,
         z1_r, z1_l, z2_r, z2_l);
    /* z3 = d7 + d3 */
    /* z4 = d5 + d1 */
    ADD4(d7_r, d3_r, d7_l, d3_l, d5_r, d1_r, d5_l, d1_l,
         z3_r, z3_l, z4_r, z4_l);
    /* z5 = MULTIPLY(z3 + z4, FIX_1_175875602) */
    tmp1 = __msa_splati_w(const0, 3);
    ADD2(z3_r, z4_r, z3_l, z4_l, z5_r, z5_l);
    MUL2(z5_r, tmp1, z5_l, tmp1, z5_r, z5_l);

    /* z1 = MULTIPLY(-z1, FIX_0_899976223) */
    tmp1 = __msa_splati_w(const1, 0);
    MUL2(z1_r, tmp1, z1_l, tmp1, z1_r, z1_l);

    /* z2 = MULTIPLY(-z2, FIX_2_562915447) */
    tmp1 = __msa_splati_w(const1, 1);
    MUL2(z2_r, tmp1, z2_l, tmp1, z2_r, z2_l);

    /* z3 = MULTIPLY(-z3, FIX_1_961570560) */
    tmp1 = __msa_splati_w(const1, 2);
    MUL2(z3_r, tmp1, z3_l, tmp1, z3_r, z3_l);

    /* z4 = MULTIPLY(-z4, FIX_0_390180644) */
    tmp1 = __msa_splati_w(const1, 3);
    MUL2(z4_r, tmp1, z4_l, tmp1, z4_r, z4_l);

    /* z3 += z5 */
    /* z4 += z5 */
    ADD4(z3_r, z5_r, z3_l, z5_l, z4_r, z5_r, z4_l, z5_l,
         z3_r, z3_l, z4_r, z4_l);

    /* tmp0 = MULTIPLY(d7, FIX_0_298631336) */
    /* tmp0 += z1 + z3 */
    tmp1 = __msa_splati_w(const2, 0);
    ADD2(z1_r, z3_r, z1_l, z3_l, tmp0_r, tmp0_l);
    tmp0_r = __msa_maddv_w(tmp0_r, d7_r, tmp1);
    tmp0_l = __msa_maddv_w(tmp0_l, d7_l, tmp1);

    /* dataptr[3] = DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS) */
    ADD2(tmp13_r, tmp0_r, tmp13_l, tmp0_l, dst3_r, dst3_l);
    SRARI_W2_SW(dst3_r, dst3_l, CONST_BITS - PASS1_BITS);

    /* dataptr[4] = DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS) */
    SUB2(tmp13_r, tmp0_r, tmp13_l, tmp0_l, dst4_r, dst4_l);
    SRARI_W2_SW(dst4_r, dst4_l, CONST_BITS - PASS1_BITS);

    /* tmp1 = MULTIPLY(d5, FIX_2_053119869) */
    /* tmp1 += z2 + z4 */
    tmp1 = __msa_splati_w(const2, 1);
    ADD2(z2_r, z4_r, z2_l, z4_l, tmp1_r, tmp1_l);
    tmp1_r = __msa_maddv_w(tmp1_r, d5_r, tmp1);
    tmp1_l = __msa_maddv_w(tmp1_l, d5_l, tmp1);

    /* dataptr[2] = DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS) */
    ADD2(tmp12_r, tmp1_r, tmp12_l, tmp1_l, dst2_r, dst2_l);
    SRARI_W2_SW(dst2_r, dst2_l, CONST_BITS - PASS1_BITS);

    /* dataptr[5] = DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS) */
    SUB2(tmp12_r, tmp1_r, tmp12_l, tmp1_l, dst5_r, dst5_l);
    SRARI_W2_SW(dst5_r, dst5_l, CONST_BITS - PASS1_BITS);

    /* tmp2 = MULTIPLY(d3, FIX_3_072711026) */
    /* tmp2 += z2 + z3 */
    tmp1 = __msa_splati_w(const2, 2);
    ADD2(z2_r, z3_r, z2_l, z3_l, tmp2_r, tmp2_l);
    tmp2_r = __msa_maddv_w(tmp2_r, d3_r, tmp1);
    tmp2_l = __msa_maddv_w(tmp2_l, d3_l, tmp1);

    /* dataptr[1] = DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS) */
    ADD2(tmp11_r, tmp2_r, tmp11_l, tmp2_l, dst1_r, dst1_l)
    SRARI_W2_SW(dst1_r, dst1_l, CONST_BITS - PASS1_BITS);

    /* dataptr[6] = DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS) */
    SUB2(tmp11_r, tmp2_r, tmp11_l, tmp2_l, dst6_r, dst6_l)
    SRARI_W2_SW(dst6_r, dst6_l, CONST_BITS - PASS1_BITS);

    /* tmp3 = MULTIPLY(d1, FIX_1_501321110) */
    /* tmp3 += z1 + z4 */
    tmp1 = __msa_splati_w(const2, 3);
    ADD2(z1_r, z4_r, z1_l, z4_l, tmp3_r, tmp3_l);
    tmp3_r = __msa_maddv_w(tmp3_r, d1_r, tmp1);
    tmp3_l = __msa_maddv_w(tmp3_l, d1_l, tmp1);

    /* dataptr[0] = DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS) */
    ADD2(tmp10_r, tmp3_r, tmp10_l, tmp3_l, dst0_r, dst0_l);
    SRARI_W2_SW(dst0_r, dst0_l, CONST_BITS - PASS1_BITS);

    /* dataptr[7] = DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS) */
    SUB2(tmp10_r, tmp3_r, tmp10_l, tmp3_l, dst7_r, dst7_l);
    SRARI_W2_SW(dst7_r, dst7_l, CONST_BITS - PASS1_BITS);

    TRANSPOSE4x4_SW_SW(dst0_r, dst1_r, dst2_r, dst3_r, d0_r, d1_r, d2_r, d3_r);
    TRANSPOSE4x4_SW_SW(dst4_r, dst5_r, dst6_r, dst7_r, d0_l, d1_l, d2_l, d3_l);
    TRANSPOSE4x4_SW_SW(dst0_l, dst1_l, dst2_l, dst3_l, d4_r, d5_r, d6_r, d7_r);
    TRANSPOSE4x4_SW_SW(dst4_l, dst5_l, dst6_l, dst7_l, d4_l, d5_l, d6_l, d7_l);
  }

  /* z1 = MULTIPLY(d2 + d6, FIX_0_541196100); */
  tmp = __msa_shf_w(const0, 0);
  ADD2(d2_r, d6_r, d2_l, d6_l, tmp_r, tmp_l);
  MUL2(tmp_r, tmp, tmp_l, tmp, z1_r, z1_l);
  /* tmp2 = z1 + MULTIPLY(-d6, FIX_1_847759065) */
  tmp = __msa_shf_w(const0, 0x55);
  MUL2(d6_r, tmp, d6_l, tmp, tmp2_r, tmp2_l);
  ADD2(tmp2_r, z1_r, tmp2_l, z1_l, tmp2_r, tmp2_l);
  /* tmp3 = z1 + MULTIPLY(d2, FIX_0_765366865); */
  tmp = __msa_shf_w(const0, 0xAA);
  MUL2(d2_r, tmp, d2_l, tmp, tmp3_r, tmp3_l);
  ADD2(tmp3_r, z1_r, tmp3_l, z1_l, tmp3_r, tmp3_l);
  BUTTERFLY_4(d0_r, d0_l, d4_l, d4_r, tmp0_r, tmp0_l, tmp1_l, tmp1_r);
  SLLI_W4_SW(tmp0_r, tmp0_l, tmp1_r, tmp1_l, CONST_BITS);
  BUTTERFLY_8(tmp0_r, tmp0_l, tmp1_r, tmp1_l, tmp2_l, tmp2_r, tmp3_l, tmp3_r,
              tmp10_r, tmp10_l, tmp11_r, tmp11_l, tmp12_l, tmp12_r, tmp13_l,
              tmp13_r);
  /* z1 = d7 + d1 */
  /* z2 = d5 + d3 */
  ADD4(d7_r, d1_r, d7_l, d1_l, d5_r, d3_r, d5_l, d3_l, z1_r, z1_l, z2_r, z2_l);
  /* z3 = d7 + d3 */
  /* z4 = d5 + d1 */
  ADD4(d7_r, d3_r, d7_l, d3_l, d5_r, d1_r, d5_l, d1_l, z3_r, z3_l, z4_r, z4_l);
  /* z5 = MULTIPLY(z3 + z4, FIX_1_175875602) */
  tmp = __msa_shf_w(const0, 0xFF);
  ADD2(z3_r, z4_r, z3_l, z4_l, z5_r, z5_l);
  MUL2(z5_r, tmp, z5_l, tmp, z5_r, z5_l);
  /* z1 = MULTIPLY(-z1, FIX_0_899976223) */
  tmp = __msa_shf_w(const1, 0);
  MUL2(z1_r, tmp, z1_l, tmp, z1_r, z1_l);
  /* z2 = MULTIPLY(-z2, FIX_2_562915447) */
  tmp = __msa_shf_w(const1, 0x55);
  MUL2(z2_r, tmp, z2_l, tmp, z2_r, z2_l);
  /* z3 = MULTIPLY(-z3, FIX_1_961570560) */
  tmp = __msa_shf_w(const1, 0xAA);
  MUL2(z3_r, tmp, z3_l, tmp, z3_r, z3_l);
  /* z4 = MULTIPLY(-z4, FIX_0_390180644) */
  tmp = __msa_shf_w(const1, 0xFF);
  MUL2(z4_r, tmp, z4_l, tmp, z4_r, z4_l);
  /* z3 += z5 */
  /* z4 += z5 */
  ADD4(z3_r, z5_r, z3_l, z5_l, z4_r, z5_r, z4_l, z5_l, z3_r, z3_l, z4_r, z4_l);
  /* tmp0 = MULTIPLY(d7, FIX_0_298631336) */
  /* tmp0 += z1 + z3 */
  tmp = __msa_shf_w(const2, 0);
  ADD2(z1_r, z3_r, z1_l, z3_l, tmp0_r, tmp0_l);
  tmp0_r = __msa_maddv_w(tmp0_r, d7_r, tmp);
  tmp0_l = __msa_maddv_w(tmp0_l, d7_l, tmp);

  /* dataptr[3] = DESCALE(tmp13 + tmp0, CONST_BITS+PASS1_BITS+3) */
  ADD2(tmp13_r, tmp0_r, tmp13_l, tmp0_l, dst3_r, dst3_l);
  SRARI_W2_SW(dst3_r, dst3_l, CONST_BITS + PASS1_BITS + 3);
  dst3_r = (v4i32)__msa_pckev_h((v8i16)dst3_l, (v8i16)dst3_r);
  dst3_r = (v4i32)CLIP_SH_0_255(dst3_r + reg_128);
  res3 = (v16u8)__msa_pckev_b((v16i8)dst3_l, (v16i8)dst3_r);

  /* dataptr[4] = DESCALE(tmp13 - tmp0, CONST_BITS+PASS1_BITS+3) */
  SUB2(tmp13_r, tmp0_r, tmp13_l, tmp0_l, dst4_r, dst4_l);
  SRARI_W2_SW(dst4_r, dst4_l, CONST_BITS + PASS1_BITS + 3);
  dst4_r = (v4i32)__msa_pckev_h((v8i16)dst4_l, (v8i16)dst4_r);
  dst4_r = (v4i32)CLIP_SH_0_255(dst4_r + reg_128);
  res4 = (v16u8)__msa_pckev_b((v16i8)dst4_l, (v16i8)dst4_r);

  /* tmp1 = MULTIPLY(d5, FIX_2_053119869) */
  /* tmp1 += z2 + z4 */
  tmp = __msa_shf_w(const2, 0x55);
  ADD2(z2_r, z4_r, z2_l, z4_l, tmp1_r, tmp1_l);
  tmp1_r = __msa_maddv_w(tmp1_r, d5_r, tmp);
  tmp1_l = __msa_maddv_w(tmp1_l, d5_l, tmp);

  /* dataptr[2] = DESCALE(tmp12 + tmp1, CONST_BITS+PASS1_BITS+3) */
  ADD2(tmp12_r, tmp1_r, tmp12_l, tmp1_l, dst2_r, dst2_l);
  SRARI_W2_SW(dst2_r, dst2_l, CONST_BITS + PASS1_BITS + 3);
  dst2_r = (v4i32)__msa_pckev_h((v8i16)dst2_l, (v8i16)dst2_r);
  dst2_r = (v4i32)CLIP_SH_0_255(dst2_r + reg_128);
  res2 = (v16u8)__msa_pckev_b((v16i8)dst2_l, (v16i8)dst2_r);

  /* dataptr[5] = DESCALE(tmp12 - tmp1, CONST_BITS+PASS1_BITS+3) */
  SUB2(tmp12_r, tmp1_r, tmp12_l, tmp1_l, dst5_r, dst5_l);
  SRARI_W2_SW(dst5_r, dst5_l, CONST_BITS + PASS1_BITS + 3);
  dst5_r = (v4i32)__msa_pckev_h((v8i16)dst5_l, (v8i16)dst5_r);
  dst5_r = (v4i32)CLIP_SH_0_255(dst5_r + reg_128);
  res5 = (v16u8)__msa_pckev_b((v16i8)dst5_l, (v16i8)dst5_r);

  /* tmp2 = MULTIPLY(d3, FIX_3_072711026) */
  /* tmp2 += z2 + z3 */
  tmp = __msa_shf_w(const2, 0xAA);
  ADD2(z2_r, z3_r, z2_l, z3_l, tmp2_r, tmp2_l);
  tmp2_r = __msa_maddv_w(tmp2_r, d3_r, tmp);
  tmp2_l = __msa_maddv_w(tmp2_l, d3_l, tmp);

  /* dataptr[1] = DESCALE(tmp11 + tmp2, CONST_BITS+PASS1_BITS+3) */
  ADD2(tmp11_r, tmp2_r, tmp11_l, tmp2_l, dst1_r, dst1_l);
  SRARI_W2_SW(dst1_r, dst1_l, CONST_BITS + PASS1_BITS + 3);
  dst1_r = (v4i32)__msa_pckev_h((v8i16)dst1_l, (v8i16)dst1_r);
  dst1_r = (v4i32)CLIP_SH_0_255(dst1_r + reg_128);
  res1 = (v16u8)__msa_pckev_b((v16i8)dst1_l, (v16i8)dst1_r);

  /* dataptr[6] = DESCALE(tmp11 - tmp2, CONST_BITS+PASS1_BITS+3) */
  SUB2(tmp11_r, tmp2_r, tmp11_l, tmp2_l, dst6_r, dst6_l);
  SRARI_W2_SW(dst6_r, dst6_l, CONST_BITS + PASS1_BITS + 3);
  dst6_r = (v4i32)__msa_pckev_h((v8i16)dst6_l, (v8i16)dst6_r);
  dst6_r = (v4i32)CLIP_SH_0_255(dst6_r + reg_128);
  res6 = (v16u8)__msa_pckev_b((v16i8)dst6_l, (v16i8)dst6_r);

  /* tmp3 = MULTIPLY(d1, FIX_1_501321110) */
  /* tmp3 += z1 + z4 */
  tmp = __msa_shf_w(const2, 0xFF);
  ADD2(z1_r, z4_r, z1_l, z4_l, tmp3_r, tmp3_l);
  tmp3_r = __msa_maddv_w(tmp3_r, d1_r, tmp);
  tmp3_l = __msa_maddv_w(tmp3_l, d1_l, tmp);

  /* dataptr[0] = DESCALE(tmp10 + tmp3, CONST_BITS+PASS1_BITS+3) */
  ADD2(tmp10_r, tmp3_r, tmp10_l, tmp3_l, dst0_r, dst0_l);
  SRARI_W2_SW(dst0_r, dst0_l, CONST_BITS + PASS1_BITS + 3);
  dst0_r = (v4i32)__msa_pckev_h((v8i16)dst0_l, (v8i16)dst0_r);
  dst0_r = (v4i32)CLIP_SH_0_255(dst0_r + reg_128);
  res0 = (v16u8)__msa_pckev_b((v16i8)dst0_l, (v16i8)dst0_r);

  /* dataptr[7] = DESCALE(tmp10 - tmp3, CONST_BITS+PASS1_BITS+3) */
  SUB2(tmp10_r, tmp3_r, tmp10_l, tmp3_l, dst7_r, dst7_l);
  SRARI_W2_SW(dst7_r, dst7_l, CONST_BITS + PASS1_BITS + 3);
  dst7_r = (v4i32)__msa_pckev_h((v8i16)dst7_l, (v8i16)dst7_r);
  dst7_r = (v4i32)CLIP_SH_0_255(dst7_r + reg_128);
  res7 = (v16u8)__msa_pckev_b((v16i8)dst7_l, (v16i8)dst7_r);

  TRANSPOSE8x8_UB_UB(res0, res1, res2, res3, res4, res5, res6, res7,
                     res0, res1, res2, res3, res4, res5, res6, res7);

  ST8x1_UB(res0, output_buf[0]);
  ST8x1_UB(res1, output_buf[1]);
  ST8x1_UB(res2, output_buf[2]);
  ST8x1_UB(res3, output_buf[3]);
  ST8x1_UB(res4, output_buf[4]);
  ST8x1_UB(res5, output_buf[5]);
  ST8x1_UB(res6, output_buf[6]);
  ST8x1_UB(res7, output_buf[7]);
}
