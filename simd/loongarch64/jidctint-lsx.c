/*
 * LOONGARCH LSX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2023 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Song Ding (songding@loongson.cn)
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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "jmacros_lsx.h"

#define DCTSIZE     8
#define CONST_BITS  13
#define PASS1_BITS  2
#define SHIFT_11 (CONST_BITS-PASS1_BITS)
#define SHIFT_18 (CONST_BITS+PASS1_BITS+3)

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

#define LSX_RANGE_PACK(_in1, _in2, _out)         \
{                                                \
  __m128i _pack_1_2;                             \
  _pack_1_2 = __lsx_vpickev_h(_in1, _in2);       \
  _pack_1_2 = __lsx_vadd_h(_pack_1_2, const128); \
  LSX_CLIP_H_0_255(_pack_1_2, _pack_1_2);        \
  _out = __lsx_vpickev_b(_in2, _pack_1_2);       \
}

#define LSX_MUL_W_H(_in0, _in1, _out0, _out1)   \
{                                               \
  __m128i _tmp0, _tmp1, _tmp2, _tmp3;           \
  LSX_UNPCKLH_W_H(_in0, _tmp1, _tmp0);          \
  LSX_UNPCKLH_W_H(_in1, _tmp3, _tmp2);          \
  _out0 = __lsx_vmul_w(_tmp0, _tmp2);           \
  _out1 = __lsx_vmul_w(_tmp1, _tmp3);           \
}

GLOBAL(void)
jsimd_idct_islow_lsx(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                     JCOEFPTR coef_block, JSAMPARRAY output_buf,
                     JDIMENSION output_col)
{
  JCOEFPTR quantptr = compptr->dct_table;
  __m128i coef0, coef1, coef2, coef3, coef4, coef5, coef6, coef7;
  __m128i tmp0, tmp1, tmp2;

  __m128i dcval0, dcval1, dcval2, dcval3;
  __m128i dcval4, dcval5, dcval6, dcval7;

  __m128i res0, res1, res2, res3, res4, res5, res6, res7;
  __m128i quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7;
  __m128i d0_h, d1_h, d2_h, d3_h, d4_h, d5_h, d6_h, d7_h;
  __m128i d0_l, d1_l, d2_l, d3_l, d4_l, d5_l, d6_l, d7_l;
  __m128i tmp_h, tmp_l, tmp2_h, tmp2_l, tmp3_h, tmp3_l, tmp0_h, tmp0_l, tmp1_h;
  __m128i tmp1_l, tmp10_h, tmp10_l, tmp11_h, tmp11_l, tmp12_h, tmp12_l;
  __m128i tmp13_h, tmp13_l;
  __m128i z1_h, z1_l, z2_h, z2_l, z3_h, z3_l, z4_h, z4_l, z5_h, z5_l;
  __m128i dst0_h, dst1_h, dst2_h, dst3_h, dst4_h, dst5_h, dst6_h, dst7_h;
  __m128i dst0_l, dst1_l, dst2_l, dst3_l, dst4_l, dst5_l, dst6_l, dst7_l;

  __m128i const100 = { FIX_0_541196100 };
  __m128i const065 = {-FIX_1_847759065 };
  __m128i const865 = { FIX_0_765366865 };
  __m128i const602 = { FIX_1_175875602 };
  __m128i const223 = {-FIX_0_899976223 };
  __m128i const447 = {-FIX_2_562915447 };
  __m128i const560 = {-FIX_1_961570560 };
  __m128i const644 = {-FIX_0_390180644 };
  __m128i const336 = { FIX_0_298631336 };
  __m128i const869 = { FIX_2_053119869 };
  __m128i const026 = { FIX_3_072711026 };
  __m128i const110 = { FIX_1_501321110 };

  __m128i const128 = {128};

  const100 = __lsx_vreplvei_w(const100, 0);
  const065 = __lsx_vreplvei_w(const065, 0);
  const865 = __lsx_vreplvei_w(const865, 0);
  const602 = __lsx_vreplvei_w(const602, 0);

  const223 = __lsx_vreplvei_w(const223, 0);
  const447 = __lsx_vreplvei_w(const447, 0);
  const560 = __lsx_vreplvei_w(const560, 0);
  const644 = __lsx_vreplvei_w(const644, 0);

  const336 = __lsx_vreplvei_w(const336, 0);
  const869 = __lsx_vreplvei_w(const869, 0);
  const026 = __lsx_vreplvei_w(const026, 0);
  const110 = __lsx_vreplvei_w(const110, 0);

  const128 = __lsx_vreplvei_h(const128, 0);

  LSX_LD_8(coef_block, 8, coef0, coef1, coef2, coef3,
           coef4, coef5, coef6, coef7 );

  tmp0 = __lsx_vor_v(coef1, coef2);
  tmp1 = __lsx_vor_v(coef3, coef4);
  tmp0 = __lsx_vor_v(tmp0, tmp1);
  tmp2 = __lsx_vor_v(coef5, coef6);
  tmp2 = __lsx_vor_v(tmp2, coef7);
  tmp0 = __lsx_vor_v(tmp0, tmp2);

  if (__lsx_bz_v(tmp0)) {
    /* AC terms all zero */
    quant0 = LSX_LD(quantptr);
    LSX_MUL_W_H(coef0, quant0, tmp1, tmp2);
    tmp1 = __lsx_vslli_w(tmp1, PASS1_BITS);
    tmp2 = __lsx_vslli_w(tmp2, PASS1_BITS);

    d0_l = __lsx_vreplvei_w(tmp1, 0);
    d1_l = __lsx_vreplvei_w(tmp1, 1);
    d2_l = __lsx_vreplvei_w(tmp1, 2);
    d3_l = __lsx_vreplvei_w(tmp1, 3);
    d4_l = __lsx_vreplvei_w(tmp2, 0);
    d5_l = __lsx_vreplvei_w(tmp2, 1);
    d6_l = __lsx_vreplvei_w(tmp2, 2);
    d7_l = __lsx_vreplvei_w(tmp2, 3);

    d0_h = d0_l;
    d1_h = d1_l;
    d2_h = d2_l;
    d3_h = d3_l;
    d4_h = d4_l;
    d5_h = d5_l;
    d6_h = d6_l;
    d7_h = d7_l;
  } else {
    LSX_LD_8(quantptr, 8, quant0, quant1, quant2, quant3,
             quant4, quant5, quant6, quant7);

    dcval0 = __lsx_vmul_h(coef0, quant0);
    dcval1 = __lsx_vmul_h(coef1, quant1);
    dcval2 = __lsx_vmul_h(coef2, quant2);
    dcval3 = __lsx_vmul_h(coef3, quant3);
    dcval4 = __lsx_vmul_h(coef4, quant4);
    dcval5 = __lsx_vmul_h(coef5, quant5);
    dcval6 = __lsx_vmul_h(coef6, quant6);
    dcval7 = __lsx_vmul_h(coef7, quant7);

    LSX_UNPCKLH_W_H(dcval0, d0_l, d0_h);
    LSX_UNPCKLH_W_H(dcval2, d2_l, d2_h);
    LSX_UNPCKLH_W_H(dcval4, d4_l, d4_h);
    LSX_UNPCKLH_W_H(dcval6, d6_l, d6_h);

    /* Even part */

    LSX_ADD_W_2(d2_h, d6_h, d2_l, d6_l, tmp_h, tmp_l);
    z1_h = __lsx_vmul_w(tmp_h, const100);
    z1_l = __lsx_vmul_w(tmp_l, const100);

    tmp2_h = __lsx_vmadd_w(z1_h, d6_h, const065);
    tmp2_l = __lsx_vmadd_w(z1_l, d6_l, const065);

    tmp3_h = __lsx_vmadd_w(z1_h, d2_h, const865);
    tmp3_l = __lsx_vmadd_w(z1_l, d2_l, const865);

    LSX_BUTTERFLY_4(v4i32, d0_h, d0_l, d4_l, d4_h, tmp0_h, tmp0_l, tmp1_l, tmp1_h);

    tmp0_h = __lsx_vslli_w(tmp0_h, CONST_BITS);
    tmp0_l = __lsx_vslli_w(tmp0_l, CONST_BITS);
    tmp1_h = __lsx_vslli_w(tmp1_h, CONST_BITS);
    tmp1_l = __lsx_vslli_w(tmp1_l, CONST_BITS);

    LSX_BUTTERFLY_8(v4i32, tmp0_h, tmp0_l, tmp1_h, tmp1_l, tmp2_l, tmp2_h, tmp3_l, tmp3_h,
                    tmp10_h, tmp10_l, tmp11_h, tmp11_l, tmp12_l, tmp12_h, tmp13_l, tmp13_h);

    /* Odd part */

    LSX_UNPCKLH_W_H(dcval1, d1_l, d1_h);
    LSX_UNPCKLH_W_H(dcval3, d3_l, d3_h);
    LSX_UNPCKLH_W_H(dcval5, d5_l, d5_h);
    LSX_UNPCKLH_W_H(dcval7, d7_l, d7_h);

    LSX_ADD_W_4(d7_h, d1_h, d7_l, d1_l, d5_h, d3_h, d5_l, d3_l,
		z1_h, z1_l, z2_h, z2_l);
    LSX_ADD_W_4(d7_h, d3_h, d7_l, d3_l, d5_h, d1_h, d5_l, d1_l,
		z3_h, z3_l, z4_h, z4_l);

    LSX_ADD_W_2(z3_h, z4_h, z3_l, z4_l, z5_h, z5_l);
    z5_h = __lsx_vmul_w(z5_h, const602);
    z5_l = __lsx_vmul_w(z5_l, const602);

    z1_h = __lsx_vmul_w(z1_h, const223);
    z1_l = __lsx_vmul_w(z1_l, const223);

    z2_h = __lsx_vmul_w(z2_h, const447);
    z2_l = __lsx_vmul_w(z2_l, const447);

    z3_h = __lsx_vmul_w(z3_h, const560);
    z3_l = __lsx_vmul_w(z3_l, const560);

    z4_h = __lsx_vmul_w(z4_h, const644);
    z4_l = __lsx_vmul_w(z4_l, const644);

    LSX_ADD_W_4(z3_h, z5_h, z3_l, z5_l, z4_h, z5_h, z4_l, z5_l,
		z3_h, z3_l, z4_h, z4_l);

    LSX_ADD_W_2(z1_h, z3_h, z1_l, z3_l, tmp0_h, tmp0_l);
    tmp0_h = __lsx_vmadd_w(tmp0_h, d7_h, const336);
    tmp0_l = __lsx_vmadd_w(tmp0_l, d7_l, const336);

    LSX_ADD_W_2(tmp13_h, tmp0_h, tmp13_l, tmp0_l, dst3_h, dst3_l);
    dst3_h = __lsx_vsrari_w(dst3_h, SHIFT_11);
    dst3_l = __lsx_vsrari_w(dst3_l, SHIFT_11);

    LSX_SUB_W_2(tmp13_h, tmp0_h, tmp13_l, tmp0_l, dst4_h, dst4_l);
    dst4_h = __lsx_vsrari_w(dst4_h, SHIFT_11);
    dst4_l = __lsx_vsrari_w(dst4_l, SHIFT_11);


    LSX_ADD_W_2(z2_h, z4_h, z2_l, z4_l, tmp1_h, tmp1_l);
    tmp1_h = __lsx_vmadd_w(tmp1_h, d5_h, const869);
    tmp1_l = __lsx_vmadd_w(tmp1_l, d5_l, const869);

    LSX_ADD_W_2(tmp12_h, tmp1_h, tmp12_l, tmp1_l, dst2_h, dst2_l);
    dst2_h = __lsx_vsrari_w(dst2_h, SHIFT_11);
    dst2_l = __lsx_vsrari_w(dst2_l, SHIFT_11);

    LSX_SUB_W_2(tmp12_h, tmp1_h, tmp12_l, tmp1_l, dst5_h, dst5_l);
    dst5_h = __lsx_vsrari_w(dst5_h, SHIFT_11);
    dst5_l = __lsx_vsrari_w(dst5_l, SHIFT_11);


    LSX_ADD_W_2(z2_h, z3_h, z2_l, z3_l, tmp2_h, tmp2_l);
    tmp2_h = __lsx_vmadd_w(tmp2_h, d3_h, const026);
    tmp2_l = __lsx_vmadd_w(tmp2_l, d3_l, const026);

    LSX_ADD_W_2(tmp11_h, tmp2_h, tmp11_l, tmp2_l, dst1_h, dst1_l);
    dst1_h = __lsx_vsrari_w(dst1_h, SHIFT_11);
    dst1_l = __lsx_vsrari_w(dst1_l, SHIFT_11);

    LSX_SUB_W_2(tmp11_h, tmp2_h, tmp11_l, tmp2_l, dst6_h, dst6_l);
    dst6_h = __lsx_vsrari_w(dst6_h, SHIFT_11);
    dst6_l = __lsx_vsrari_w(dst6_l, SHIFT_11);


    LSX_ADD_W_2(z1_h, z4_h, z1_l, z4_l, tmp3_h, tmp3_l);
    tmp3_h = __lsx_vmadd_w(tmp3_h, d1_h, const110);
    tmp3_l = __lsx_vmadd_w(tmp3_l, d1_l, const110);

    LSX_ADD_W_2(tmp10_h, tmp3_h, tmp10_l, tmp3_l, dst0_h, dst0_l);
    dst0_h = __lsx_vsrari_w(dst0_h, SHIFT_11);
    dst0_l = __lsx_vsrari_w(dst0_l, SHIFT_11);

    LSX_SUB_W_2(tmp10_h, tmp3_h, tmp10_l, tmp3_l, dst7_h, dst7_l);
    dst7_h = __lsx_vsrari_w(dst7_h, SHIFT_11);
    dst7_l = __lsx_vsrari_w(dst7_l, SHIFT_11);

    LSX_TRANSPOSE4x4_W(dst0_h, dst1_h, dst2_h, dst3_h, d0_h, d1_h, d2_h, d3_h);
    LSX_TRANSPOSE4x4_W(dst4_h, dst5_h, dst6_h, dst7_h, d0_l, d1_l, d2_l, d3_l);
    LSX_TRANSPOSE4x4_W(dst0_l, dst1_l, dst2_l, dst3_l, d4_h, d5_h, d6_h, d7_h);
    LSX_TRANSPOSE4x4_W(dst4_l, dst5_l, dst6_l, dst7_l, d4_l, d5_l, d6_l, d7_l);
  }

  LSX_ADD_W_2(d2_h, d6_h, d2_l, d6_l, tmp_h, tmp_l);
  z1_h = __lsx_vmul_w(tmp_h, const100);
  z1_l = __lsx_vmul_w(tmp_l, const100);

  tmp2_h = __lsx_vmul_w(d6_h, const065);
  tmp2_l = __lsx_vmul_w(d6_l, const065);
  LSX_ADD_W_2(tmp2_h, z1_h, tmp2_l, z1_l, tmp2_h, tmp2_l);

  tmp3_h = __lsx_vmul_w(d2_h, const865);
  tmp3_l = __lsx_vmul_w(d2_l, const865);
  LSX_ADD_W_2(tmp3_h, z1_h, tmp3_l, z1_l, tmp3_h, tmp3_l);

  LSX_BUTTERFLY_4(v4i32, d0_h, d0_l, d4_l, d4_h, tmp0_h, tmp0_l, tmp1_l, tmp1_h);
  tmp0_h = __lsx_vslli_w(tmp0_h, CONST_BITS);
  tmp0_l = __lsx_vslli_w(tmp0_l, CONST_BITS);
  tmp1_h = __lsx_vslli_w(tmp1_h, CONST_BITS);
  tmp1_l = __lsx_vslli_w(tmp1_l, CONST_BITS);

  LSX_BUTTERFLY_8(v4i32, tmp0_h, tmp0_l, tmp1_h, tmp1_l, tmp2_l, tmp2_h, tmp3_l, tmp3_h,
                  tmp10_h, tmp10_l, tmp11_h, tmp11_l, tmp12_l, tmp12_h, tmp13_l, tmp13_h);

  LSX_ADD_W_4(d7_h, d1_h, d7_l, d1_l, d5_h, d3_h, d5_l, d3_l,
              z1_h, z1_l, z2_h, z2_l);
  LSX_ADD_W_4(d7_h, d3_h, d7_l, d3_l, d5_h, d1_h, d5_l, d1_l,
              z3_h, z3_l, z4_h, z4_l);

  /* z5 = MULTIPLY(z3 + z4, FIX_1_175875602) */
  LSX_ADD_W_2(z3_h, z4_h, z3_l, z4_l, z5_h, z5_l);
  z5_h = __lsx_vmul_w(z5_h, const602);
  z5_l = __lsx_vmul_w(z5_l, const602);
  /* z1 = MULTIPLY(-z1, FIX_0_899976223) */
  z1_h = __lsx_vmul_w(z1_h, const223);
  z1_l = __lsx_vmul_w(z1_l, const223);
  /* z2 = MULTIPLY(-z2, FIX_2_562915447) */
  z2_h = __lsx_vmul_w(z2_h, const447);
  z2_l = __lsx_vmul_w(z2_l, const447);
  /* z3 = MULTIPLY(-z3, FIX_1_961570560) */
  z3_h = __lsx_vmul_w(z3_h, const560);
  z3_l = __lsx_vmul_w(z3_l, const560);
  /* z4 = MULTIPLY(-z4, FIX_0_390180644) */
  z4_h = __lsx_vmul_w(z4_h, const644);
  z4_l = __lsx_vmul_w(z4_l, const644);

  LSX_ADD_W_4(z3_h, z5_h, z3_l, z5_l, z4_h, z5_h, z4_l, z5_l, z3_h, z3_l, z4_h, z4_l);

  LSX_ADD_W_2(z1_h, z3_h, z1_l, z3_l, tmp0_h, tmp0_l);
  tmp0_h = __lsx_vmadd_w(tmp0_h, d7_h, const336);
  tmp0_l = __lsx_vmadd_w(tmp0_l, d7_l, const336);

  /* dataptr[3] = DESCALE(tmp13 + tmp0, CONST_BITS+PASS1_BITS+3) */
  LSX_ADD_W_2(tmp13_h, tmp0_h, tmp13_l, tmp0_l, dst3_h, dst3_l);
  dst3_h = __lsx_vsrari_w(dst3_h, SHIFT_18);
  dst3_l = __lsx_vsrari_w(dst3_l, SHIFT_18);
  LSX_RANGE_PACK(dst3_l, dst3_h, res3);

  /* dataptr[4] = DESCALE(tmp13 - tmp0, CONST_BITS+PASS1_BITS+3) */
  LSX_SUB_W_2(tmp13_h, tmp0_h, tmp13_l, tmp0_l, dst4_h, dst4_l);
  dst4_h = __lsx_vsrari_w(dst4_h, SHIFT_18);
  dst4_l = __lsx_vsrari_w(dst4_l, SHIFT_18);
  LSX_RANGE_PACK(dst4_l, dst4_h, res4);

  /* tmp1 = MULTIPLY(d5, FIX_2_053119869) */
  /* tmp1 += z2 + z4 */
  LSX_ADD_W_2(z2_h, z4_h, z2_l, z4_l, tmp1_h, tmp1_l);
  tmp1_h = __lsx_vmadd_w(tmp1_h, d5_h, const869);
  tmp1_l = __lsx_vmadd_w(tmp1_l, d5_l, const869);

  /* dataptr[2] = DESCALE(tmp12 + tmp1, CONST_BITS+PASS1_BITS+3) */
  LSX_ADD_W_2(tmp12_h, tmp1_h, tmp12_l, tmp1_l, dst2_h, dst2_l);
  dst2_h = __lsx_vsrari_w(dst2_h, SHIFT_18);
  dst2_l = __lsx_vsrari_w(dst2_l, SHIFT_18);
  LSX_RANGE_PACK(dst2_l, dst2_h, res2);

  /* dataptr[5] = DESCALE(tmp12 - tmp1, CONST_BITS+PASS1_BITS+3) */
  LSX_SUB_W_2(tmp12_h, tmp1_h, tmp12_l, tmp1_l, dst5_h, dst5_l);
  dst5_h = __lsx_vsrari_w(dst5_h, SHIFT_18);
  dst5_l = __lsx_vsrari_w(dst5_l, SHIFT_18);
  LSX_RANGE_PACK(dst5_l, dst5_h, res5);

  /* tmp2 = MULTIPLY(d3, FIX_3_072711026) */
  /* tmp2 += z2 + z3 */
  LSX_ADD_W_2(z2_h, z3_h, z2_l, z3_l, tmp2_h, tmp2_l);
  tmp2_h = __lsx_vmadd_w(tmp2_h, d3_h, const026);
  tmp2_l = __lsx_vmadd_w(tmp2_l, d3_l, const026);

  /* dataptr[1] = DESCALE(tmp11 + tmp2, CONST_BITS+PASS1_BITS+3) */
  LSX_ADD_W_2(tmp11_h, tmp2_h, tmp11_l, tmp2_l, dst1_h, dst1_l);
  dst1_h = __lsx_vsrari_w(dst1_h, SHIFT_18);
  dst1_l = __lsx_vsrari_w(dst1_l, SHIFT_18);
  LSX_RANGE_PACK(dst1_l, dst1_h, res1);

  /* dataptr[6] = DESCALE(tmp11 - tmp2, CONST_BITS+PASS1_BITS+3) */
  LSX_SUB_W_2(tmp11_h, tmp2_h, tmp11_l, tmp2_l, dst6_h, dst6_l);
  dst6_h = __lsx_vsrari_w(dst6_h, SHIFT_18);
  dst6_l = __lsx_vsrari_w(dst6_l, SHIFT_18);
  LSX_RANGE_PACK(dst6_l, dst6_h, res6);

  /* tmp3 = MULTIPLY(d1, FIX_1_501321110) */
  /* tmp3 += z1 + z4 */
  LSX_ADD_W_2(z1_h, z4_h, z1_l, z4_l, tmp3_h, tmp3_l);
  tmp3_h = __lsx_vmadd_w(tmp3_h, d1_h, const110);
  tmp3_l = __lsx_vmadd_w(tmp3_l, d1_l, const110);

  /* dataptr[0] = DESCALE(tmp10 + tmp3, CONST_BITS+PASS1_BITS+3) */
  LSX_ADD_W_2(tmp10_h, tmp3_h, tmp10_l, tmp3_l, dst0_h, dst0_l);
  dst0_h = __lsx_vsrari_w(dst0_h, SHIFT_18);
  dst0_l = __lsx_vsrari_w(dst0_l, SHIFT_18);
  LSX_RANGE_PACK(dst0_l, dst0_h, res0);

  /* dataptr[7] = DESCALE(tmp10 - tmp3, CONST_BITS+PASS1_BITS+3) */
  LSX_SUB_W_2(tmp10_h, tmp3_h, tmp10_l, tmp3_l, dst7_h, dst7_l);
  dst7_h = __lsx_vsrari_w(dst7_h, SHIFT_18);
  dst7_l = __lsx_vsrari_w(dst7_l, SHIFT_18);
  LSX_RANGE_PACK(dst7_l, dst7_h, res7);

  LSX_TRANSPOSE8x8_B(res0, res1, res2, res3, res4, res5, res6, res7,
                     res0, res1, res2, res3, res4, res5, res6, res7);

  __lsx_vstelm_d(res0, output_buf[0], 0, 0);
  __lsx_vstelm_d(res1, output_buf[1], 0, 0);
  __lsx_vstelm_d(res2, output_buf[2], 0, 0);
  __lsx_vstelm_d(res3, output_buf[3], 0, 0);
  __lsx_vstelm_d(res4, output_buf[4], 0, 0);
  __lsx_vstelm_d(res5, output_buf[5], 0, 0);
  __lsx_vstelm_d(res6, output_buf[6], 0, 0);
  __lsx_vstelm_d(res7, output_buf[7], 0, 0);
}
