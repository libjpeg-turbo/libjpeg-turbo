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

#define DCTSIZE 8
#define PASS1_BITS 2
#define CONST_BITS 13

#define FIX_0_765366865  ((int32_t)6270)   /* FIX(0.765366865) */
#define FIX_0_899976223  ((int32_t)7373)   /* FIX(0.899976223) */
#define FIX_1_847759065  ((int32_t)15137)  /* FIX(1.847759065) */
#define FIX_2_562915447  ((int32_t)20995)  /* FIX(2.562915447) */
#define FIX_0_211164243  ((int32_t)1730)   /* FIX(0.211164243) */
#define FIX_0_509795579  ((int32_t)4176)   /* FIX(0.509795579) */
#define FIX_0_601344887  ((int32_t)4926)   /* FIX(0.601344887) */
#define FIX_0_720959822  ((int32_t)5906)   /* FIX(0.720959822) */
#define FIX_0_850430095  ((int32_t)6967)   /* FIX(0.850430095) */
#define FIX_1_061594337  ((int32_t)8697)   /* FIX(1.061594337) */
#define FIX_1_272758580  ((int32_t)10426)  /* FIX(1.272758580) */
#define FIX_1_451774981  ((int32_t)11893)  /* FIX(1.451774981) */
#define FIX_2_172734803  ((int32_t)17799)  /* FIX(2.172734803) */
#define FIX_3_624509785  ((int32_t)29692)  /* FIX(3.624509785) */

GLOBAL(void)
jsimd_idct_2x2_lsx(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
                   JDIMENSION output_col)
{
  JCOEFPTR quantptr = (JCOEFPTR)dct_table;
  __m128i coef0, coef1, coef3, coef5, coef7;
  __m128i quant0, quant1, quant3, quant5, quant7;
  __m128i tmp0, tmp10;
  __m128i tmp0_h, tmp0_l, tmp10_h, tmp10_l;
  __m128i const128 = { 128 };
  __m128i dcval0, dcval1, dcval3, dcval5, dcval7;
  __m128i dst0_h, dst1_h, dst3_h, dst5_h, dst7_h, dst0_l, dst1_l, dst3_l, dst5_l, dst7_l;

  v4i32 const072 = {-FIX_0_720959822,-FIX_0_720959822,-FIX_0_720959822,-FIX_0_720959822};
  v4i32 const085 = { FIX_0_850430095, FIX_0_850430095, FIX_0_850430095, FIX_0_850430095};
  v4i32 const127 = { FIX_1_272758580, FIX_1_272758580, FIX_1_272758580, FIX_1_272758580};
  v4i32 const362 = { FIX_3_624509785, FIX_3_624509785, FIX_3_624509785, FIX_3_624509785};
  v4i32 const0 = { FIX_0_850430095, -FIX_0_720959822, FIX_0_850430095, -FIX_0_720959822};
  v4i32 const1 = { FIX_3_624509785, -FIX_1_272758580, FIX_3_624509785, -FIX_1_272758580};

  coef0 = LSX_LD(coef_block);
  LSX_LD_4(coef_block + DCTSIZE, 16, coef1, coef3, coef5, coef7);
  quant0 = LSX_LD(quantptr);

  const128 = __lsx_vreplvei_h(const128, 0);

  tmp0 = coef1 | coef3 | coef5 | coef7;

  if (__lsx_bz_v(tmp0)) {
    /* AC terms all zero */
    dcval0 = __lsx_vmul_h(coef0, quant0);
    LSX_UNPCKLH_W_H(dcval0, dst0_l, dst0_h);
    dst0_h = __lsx_vslli_w(dst0_h, PASS1_BITS);
    dst0_l = __lsx_vslli_w(dst0_l, PASS1_BITS);

    dst1_h = dst0_h;
    dst1_l = dst0_l;
  } else {
    LSX_LD_4(quantptr + 8, 16, quant1, quant3, quant5, quant7);

    dcval0 = __lsx_vmul_h(coef0, quant0);
    dcval1 = __lsx_vmul_h(coef1, quant1);
    dcval3 = __lsx_vmul_h(coef3, quant3);
    dcval5 = __lsx_vmul_h(coef5, quant5);
    dcval7 = __lsx_vmul_h(coef7, quant7);

    /* Even part */
    LSX_UNPCKLH_W_H(dcval0, dst0_l, dst0_h);
    dst0_h = __lsx_vslli_w(dst0_h, CONST_BITS + 2);
    dst0_l = __lsx_vslli_w(dst0_l, CONST_BITS + 2);
    tmp10_h = dst0_h;
    tmp10_l = dst0_l;

    /* Odd part */
    LSX_UNPCKLH_W_H_4(dcval1, dcval3, dcval5, dcval7, dst1_l, dst1_h,
                      dst3_l, dst3_h, dst5_l, dst5_h, dst7_l, dst7_h);

    tmp0_h = __lsx_vmul_w(dst7_h, (__m128i)const072);
    tmp0_h += __lsx_vmul_w(dst5_h, (__m128i)const085);
    tmp0_h -= __lsx_vmul_w(dst3_h, (__m128i)const127);
    tmp0_h += __lsx_vmul_w(dst1_h, (__m128i)const362);

    tmp0_l = __lsx_vmul_w(dst7_l, (__m128i)const072);
    tmp0_l += __lsx_vmul_w(dst5_l, (__m128i)const085);
    tmp0_l -= __lsx_vmul_w(dst3_l, (__m128i)const127);
    tmp0_l += __lsx_vmul_w(dst1_l, (__m128i)const362);

    LSX_BUTTERFLY_4(v4i32, tmp10_h, tmp10_l, tmp0_l, tmp0_h,
                    dst0_h, dst0_l, dst1_l, dst1_h);

    LSX_SRARI_W_4(dst0_h, dst0_l, dst1_h, dst1_l, CONST_BITS - PASS1_BITS +2);
  }

  /* Even part */
  tmp10 = __lsx_vilvl_w(dst1_h, dst0_h);
  tmp10 = __lsx_vslli_w(tmp10, CONST_BITS + 2);

  /* Odd part */
  LSX_PCKOD_W(dst1_l, dst0_l, dst0_l); // 1 3 1 3
  LSX_PCKOD_W(dst1_h, dst0_h, dst0_h); // 5 7 5 7

  tmp0 = __lsx_vmulwev_d_w(dst0_l, (__m128i)const0);
  dst0_l = __lsx_vmaddwod_d_w(tmp0, dst0_l, (__m128i)const0);

  tmp0 = __lsx_vmulwev_d_w(dst0_h, (__m128i)const1);
  dst0_h = __lsx_vmaddwod_d_w(tmp0, dst0_h, (__m128i)const1);

  tmp0 = __lsx_vadd_w(dst0_l, dst0_h);
  tmp0 = __lsx_vpickev_w(tmp0, tmp0);

  dst0_h = __lsx_vadd_w(tmp10, tmp0);
  dst1_h = __lsx_vsub_w(tmp10, tmp0);

  dst0_h = __lsx_vsrari_w(dst0_h, CONST_BITS + PASS1_BITS + 3 + 2);
  dst1_h = __lsx_vsrari_w(dst1_h, CONST_BITS + PASS1_BITS + 3 + 2);

  dcval0 = __lsx_vpickev_d(dst1_h, dst0_h);
  dcval0 = __lsx_vadd_h(dcval0, const128);
  LSX_CLIP_H_0_255(dcval0, dcval0);

  __lsx_vstelm_b(dcval0, output_buf[0] + output_col, 0, 0);
  __lsx_vstelm_b(dcval0, output_buf[1] + output_col, 0, 4);
  __lsx_vstelm_b(dcval0, output_buf[0] + output_col + 1, 0, 8);
  __lsx_vstelm_b(dcval0, output_buf[1] + output_col + 1, 0, 12);
}

GLOBAL(void)
jsimd_idct_4x4_lsx(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
                   JDIMENSION output_col)
{
  JCOEFPTR quantptr = (JCOEFPTR)dct_table;
  __m128i coef0, coef1, coef2, coef3, coef5, coef6, coef7;
  __m128i quant0, quant1, quant2, quant3, quant5, quant6, quant7;
  __m128i tmp0, res;
  __m128i tmp0_h, tmp0_l, tmp2_h, tmp2_l, tmp10_h, tmp10_l, tmp12_h, tmp12_l;
  __m128i const128 = { 128 };
  __m128i dst0_h, dst1_h, dst2_h, dst3_h;
  __m128i dst0_l, dst1_l, dst2_l, dst3_l;
  __m128i d0_h, d1_h, d2_h, d3_h, d5_h, d6_h, d7_h;
  __m128i d0_l, d1_l, d2_l, d3_l, d5_l, d6_l, d7_l;
  __m128i dcval0, dcval1, dcval2, dcval3;
  __m128i dcval5, dcval6, dcval7;

  v4i32 const184 = { FIX_1_847759065, FIX_1_847759065, FIX_1_847759065, FIX_1_847759065};
  v4i32 const076 = {-FIX_0_765366865,-FIX_0_765366865,-FIX_0_765366865,-FIX_0_765366865};
  v4i32 const021 = {-FIX_0_211164243,-FIX_0_211164243,-FIX_0_211164243,-FIX_0_211164243};
  v4i32 const145 = { FIX_1_451774981, FIX_1_451774981, FIX_1_451774981, FIX_1_451774981};
  v4i32 const217 = { FIX_2_172734803, FIX_2_172734803, FIX_2_172734803, FIX_2_172734803};
  v4i32 const106 = { FIX_1_061594337, FIX_1_061594337, FIX_1_061594337, FIX_1_061594337};
  v4i32 const050 = {-FIX_0_509795579,-FIX_0_509795579,-FIX_0_509795579,-FIX_0_509795579};
  v4i32 const060 = { FIX_0_601344887, FIX_0_601344887, FIX_0_601344887, FIX_0_601344887};
  v4i32 const089 = { FIX_0_899976223, FIX_0_899976223, FIX_0_899976223, FIX_0_899976223};
  v4i32 const256 = { FIX_2_562915447, FIX_2_562915447, FIX_2_562915447, FIX_2_562915447};

  LSX_LD_4(coef_block, 8, coef0, coef1, coef2, coef3);
  LSX_LD_2(coef_block + 5 * DCTSIZE, 8, coef5, coef6);
  coef7 = LSX_LD(coef_block + 7 * DCTSIZE);
  quant0 = LSX_LD(quantptr);

  const128 = __lsx_vreplvei_h(const128, 0);
  tmp0 = coef1 | coef2 | coef3 | coef5 | coef6 | coef7;
  if (__lsx_bz_v(tmp0)) {
    dcval0 = __lsx_vmul_h(coef0, quant0);
    LSX_UNPCKLH_W_H(dcval0, dst0_l, dst0_h);
    dst0_h = __lsx_vslli_w(dst0_h, PASS1_BITS);
    dst0_l = __lsx_vslli_w(dst0_l, PASS1_BITS);

    d0_h = __lsx_vreplvei_w(dst0_h, 0);
    d1_h = __lsx_vreplvei_w(dst0_h, 1);
    d2_h = __lsx_vreplvei_w(dst0_h, 2);
    d3_h = __lsx_vreplvei_w(dst0_h, 3);

    d5_h = __lsx_vreplvei_w(dst0_l, 1);
    d6_h = __lsx_vreplvei_w(dst0_l, 2);
    d7_h = __lsx_vreplvei_w(dst0_l, 3);
  } else {
    LSX_LD_4(quantptr, 8, quant0, quant1, quant2, quant3);
    LSX_LD_2(quantptr + 5 * DCTSIZE, 8, quant5, quant6);
    quant7 = LSX_LD(quantptr + 7 * DCTSIZE);

    dcval0 = __lsx_vmul_h(coef0, quant0);
    dcval1 = __lsx_vmul_h(coef1, quant1);
    dcval2 = __lsx_vmul_h(coef2, quant2);
    dcval3 = __lsx_vmul_h(coef3, quant3);

    dcval5 = __lsx_vmul_h(coef5, quant5);
    dcval6 = __lsx_vmul_h(coef6, quant6);
    dcval7 = __lsx_vmul_h(coef7, quant7);

    /* Even part */
    LSX_UNPCKLH_W_H(dcval0, d0_l, d0_h);
    LSX_UNPCKLH_W_H(dcval2, d2_l, d2_h);
    LSX_UNPCKLH_W_H(dcval6, d6_l, d6_h);

    d0_h = __lsx_vslli_w(d0_h, CONST_BITS + 1);
    d0_l = __lsx_vslli_w(d0_l, CONST_BITS + 1);

    d2_h = __lsx_vmul_w(d2_h, (__m128i)const184);
    d2_h = __lsx_vmadd_w(d2_h, d6_h, (__m128i)const076);

    d2_l = __lsx_vmul_w(d2_l, (__m128i)const184);
    d2_l = __lsx_vmadd_w(d2_l, d6_l, (__m128i)const076);

    LSX_BUTTERFLY_4(v4i32, d0_h, d0_l, d2_l, d2_h,
                    tmp10_h, tmp10_l, tmp12_l, tmp12_h);

    /* Odd part */
    LSX_UNPCKLH_W_H(dcval1, d1_l, d1_h);
    LSX_UNPCKLH_W_H(dcval3, d3_l, d3_h);
    LSX_UNPCKLH_W_H(dcval5, d5_l, d5_h);
    LSX_UNPCKLH_W_H(dcval7, d7_l, d7_h);

    tmp0_h = __lsx_vmul_w(d7_h, (__m128i)const021);
    tmp0_h = __lsx_vmadd_w(tmp0_h, d5_h, (__m128i)const145);
    tmp0_h = __lsx_vmsub_w(tmp0_h, d3_h, (__m128i)const217);
    tmp0_h = __lsx_vmadd_w(tmp0_h, d1_h, (__m128i)const106);

    tmp0_l = __lsx_vmul_w(d7_l, (__m128i)const021);
    tmp0_l = __lsx_vmadd_w(tmp0_l, d5_l, (__m128i)const145);
    tmp0_l = __lsx_vmsub_w(tmp0_l, d3_l, (__m128i)const217);
    tmp0_l = __lsx_vmadd_w(tmp0_l, d1_l, (__m128i)const106);

    tmp2_h = __lsx_vmul_w(d7_h, (__m128i)const050);
    tmp2_h = __lsx_vmsub_w(tmp2_h, d5_h, (__m128i)const060);
    tmp2_h = __lsx_vmadd_w(tmp2_h, d3_h, (__m128i)const089);
    tmp2_h = __lsx_vmadd_w(tmp2_h, d1_h, (__m128i)const256);

    tmp2_l = __lsx_vmul_w(d7_l, (__m128i)const050);
    tmp2_l = __lsx_vmsub_w(tmp2_l, d5_l, (__m128i)const060);
    tmp2_l = __lsx_vmadd_w(tmp2_l, d3_l, (__m128i)const089);
    tmp2_l = __lsx_vmadd_w(tmp2_l, d1_l, (__m128i)const256);

    LSX_BUTTERFLY_4(v4i32, tmp10_h, tmp12_h, tmp0_h, tmp2_h,
                    dst0_h, dst1_h, dst2_h, dst3_h);

    LSX_BUTTERFLY_4(v4i32, tmp10_l, tmp12_l, tmp0_l, tmp2_l,
                    dst0_l, dst1_l, dst2_l, dst3_l);

    LSX_SRARI_W_4(dst0_h, dst1_h, dst2_h, dst3_h, CONST_BITS - PASS1_BITS + 1);
    LSX_SRARI_W_4(dst0_l, dst1_l, dst2_l, dst3_l, CONST_BITS - PASS1_BITS + 1);

    LSX_TRANSPOSE4x4_W(dst0_h, dst1_h, dst2_h, dst3_h, d0_h, d1_h, d2_h, d3_h);

    dcval0 = __lsx_vilvl_w(dst1_l, dst0_l);
    dcval1 = __lsx_vilvh_w(dst1_l, dst0_l);
    dcval2 = __lsx_vilvl_w(dst3_l, dst2_l);
    dcval3 = __lsx_vilvh_w(dst3_l, dst2_l);

    d5_h = __lsx_vilvh_d(dcval2, dcval0);
    d6_h = __lsx_vilvl_d(dcval3, dcval1);
    d7_h = __lsx_vilvh_d(dcval3, dcval1);
  }

  /* Even part */
  d0_h = __lsx_vslli_w(d0_h, CONST_BITS + 1);

  d2_h = __lsx_vmul_w(d2_h, (__m128i)const184);
  d2_h = __lsx_vmadd_w(d2_h, d6_h, (__m128i)const076);
  tmp10_h = __lsx_vadd_w(d0_h, d2_h);
  tmp12_h = __lsx_vsub_w(d0_h, d2_h);

  /* Odd part */
  tmp0_h = __lsx_vmul_w(d7_h, (__m128i)const021);
  tmp0_h = __lsx_vmadd_w(tmp0_h, d5_h, (__m128i)const145);
  tmp0_h = __lsx_vmsub_w(tmp0_h, d3_h, (__m128i)const217);
  tmp0_h = __lsx_vmadd_w(tmp0_h, d1_h, (__m128i)const106);

  tmp2_h = __lsx_vmul_w(d7_h, (__m128i)const050);
  tmp2_h = __lsx_vmsub_w(tmp2_h, d5_h, (__m128i)const060);
  tmp2_h = __lsx_vmadd_w(tmp2_h, d3_h, (__m128i)const089);
  tmp2_h = __lsx_vmadd_w(tmp2_h, d1_h, (__m128i)const256);

  LSX_BUTTERFLY_4(v4i32, tmp10_h, tmp12_h, tmp0_h, tmp2_h,
                  dst0_h, dst1_h, dst2_h, dst3_h);

  LSX_SRARI_W_4(dst0_h, dst1_h, dst2_h, dst3_h, CONST_BITS + PASS1_BITS + 3 + 1);

  LSX_TRANSPOSE4x4_W(dst0_h, dst1_h, dst2_h, dst3_h, dst0_h, dst1_h, dst2_h, dst3_h);

  dcval0 = __lsx_vpickev_h(dst1_h, dst0_h);
  dcval1 = __lsx_vpickev_h(dst3_h, dst2_h);
  dcval0 = __lsx_vadd_h(dcval0, const128);
  dcval1 = __lsx_vadd_h(dcval1, const128);
  LSX_CLIP_H_0_255(dcval0, dcval0);
  LSX_CLIP_H_0_255(dcval1, dcval1);
  res = __lsx_vpickev_b(dcval1, dcval0);

  __lsx_vstelm_w(res, output_buf[0] + output_col, 0, 0);
  __lsx_vstelm_w(res, output_buf[1] + output_col, 0, 1);
  __lsx_vstelm_w(res, output_buf[2] + output_col, 0, 2);
  __lsx_vstelm_w(res, output_buf[3] + output_col, 0, 3);
}
