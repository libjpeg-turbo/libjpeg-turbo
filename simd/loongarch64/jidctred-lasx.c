/*
 * LOONGARCH LASX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2021 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Jin Bo (jinbo@loongson.cn)
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
#include "jmacros_lasx.h"

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
jsimd_idct_2x2_lasx(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
                    JDIMENSION output_col)
{
  JCOEFPTR quantptr = (JCOEFPTR)dct_table;
  __m256i coef01, coef23, coef45, coef67;
  __m256i quant01, quant23, quant45, quant67;
  __m256i tmp0, tmp1, tmp10;
  __m256i zero = __lasx_xvldi(0);
  __m256i const128 = { 128 };
  __m256i dcval0, dcval1, dcval2, dcval3;
  __m256i dcval4, dcval5, dcval6, dcval7;

  v8i32 const072 =
    {-FIX_0_720959822,-FIX_0_720959822,-FIX_0_720959822,-FIX_0_720959822,
     -FIX_0_720959822,-FIX_0_720959822,-FIX_0_720959822,-FIX_0_720959822 };
  v8i32 const085 =
    { FIX_0_850430095, FIX_0_850430095, FIX_0_850430095, FIX_0_850430095,
      FIX_0_850430095, FIX_0_850430095, FIX_0_850430095, FIX_0_850430095 };
  v8i32 const127 =
    { FIX_1_272758580, FIX_1_272758580, FIX_1_272758580, FIX_1_272758580,
      FIX_1_272758580, FIX_1_272758580, FIX_1_272758580, FIX_1_272758580 };
  v8i32 const362 =
    { FIX_3_624509785, FIX_3_624509785, FIX_3_624509785, FIX_3_624509785,
      FIX_3_624509785, FIX_3_624509785, FIX_3_624509785, FIX_3_624509785 };
  v8i32 constall =
    { FIX_3_624509785,-FIX_1_272758580, FIX_0_850430095,-FIX_0_720959822,
      FIX_3_624509785,-FIX_1_272758580, FIX_0_850430095,-FIX_0_720959822};

  LASX_LD_4(coef_block, 16, coef01, coef23, coef45, coef67);
  quant01 = LASX_LD(quantptr);

  const128 = __lasx_xvreplve0_h(const128);

  dcval1 = __lasx_xvpermi_q(zero, coef01, 0x31);
  dcval3 = __lasx_xvpermi_q(zero, coef23, 0x31);
  dcval5 = __lasx_xvpermi_q(zero, coef45, 0x31);
  dcval7 = __lasx_xvpermi_q(zero, coef67, 0x31);
  tmp0 = dcval1 | dcval3 | dcval5 | dcval7;

  if (__lasx_xbz_v(tmp0)) {
    /* AC terms all zero */
    dcval0 = __lasx_xvmul_h(coef01, quant01);
    LASX_UNPCK_L_W_H(dcval0, dcval0);
    dcval0 = __lasx_xvslli_w(dcval0, PASS1_BITS);
    dcval1 = dcval0;
  } else {
    quant23 = LASX_LD(quantptr + 16);
    LASX_LD_2(quantptr + 32, 16, quant45, quant67);

    dcval0 = __lasx_xvmul_h(coef01, quant01);
    dcval2 = __lasx_xvmul_h(coef23, quant23);
    dcval4 = __lasx_xvmul_h(coef45, quant45);
    dcval6 = __lasx_xvmul_h(coef67, quant67);
    dcval1 = __lasx_xvpermi_q(zero, dcval0, 0x31);
    dcval3 = __lasx_xvpermi_q(zero, dcval2, 0x31);
    dcval5 = __lasx_xvpermi_q(zero, dcval4, 0x31);
    dcval7 = __lasx_xvpermi_q(zero, dcval6, 0x31);

    /* Even part */
    LASX_UNPCK_L_W_H(dcval0, tmp10);
    tmp10 = __lasx_xvslli_w(tmp10, CONST_BITS + 2);

    /* Odd part */
    LASX_UNPCK_L_W_H_4(dcval1, dcval3, dcval5, dcval7,
                       dcval1, dcval3, dcval5, dcval7);
    tmp0 = __lasx_xvmul_w(dcval7, (__m256i)const072);
    tmp0 += __lasx_xvmul_w(dcval5, (__m256i)const085);
    tmp0 -= __lasx_xvmul_w(dcval3, (__m256i)const127);
    tmp0 += __lasx_xvmul_w(dcval1, (__m256i)const362);

    dcval0 = __lasx_xvadd_w(tmp10, tmp0);
    dcval1 = __lasx_xvsub_w(tmp10, tmp0);
    dcval0 = __lasx_xvsrari_w(dcval0, CONST_BITS - PASS1_BITS + 2);
    dcval1 = __lasx_xvsrari_w(dcval1, CONST_BITS - PASS1_BITS + 2);
  }

  /* Even part */
  tmp10 = __lasx_xvpackev_w(dcval1, dcval0);
  tmp10 = __lasx_xvslli_w(tmp10, CONST_BITS + 2);

  /* Odd part */
  LASX_PCKOD_W(dcval1, dcval0, tmp0);// 1 3 5 7 1 3 5 7
  tmp0 = __lasx_xvmul_w(tmp0, (__m256i)constall);
  tmp0 = __lasx_xvhaddw_d_w(tmp0, tmp0);// 1+3 5+7 1+3 5+7
  tmp0 = __lasx_xvhaddw_q_d(tmp0, tmp0);// 1+3+5+7 1+3+5+7
  tmp1 = __lasx_xvpermi_q(zero, tmp0, 0x31);
  tmp0 = __lasx_xvpackev_w(tmp1, tmp0);

  dcval0 = __lasx_xvadd_w(tmp10, tmp0);
  dcval1 = __lasx_xvsub_w(tmp10, tmp0);

  dcval0 = __lasx_xvpackev_d(dcval1, dcval0);
  dcval0 = __lasx_xvsrari_w(dcval0, CONST_BITS + PASS1_BITS + 3 + 2);
  dcval0 = __lasx_xvadd_h(dcval0, const128);
  LASX_CLIP_H_0_255(dcval0, dcval0);

  __lasx_xvstelm_b(dcval0, output_buf[0] + output_col, 0, 0);
  __lasx_xvstelm_b(dcval0, output_buf[1] + output_col, 0, 4);
  __lasx_xvstelm_b(dcval0, output_buf[0] + output_col + 1, 0, 8);
  __lasx_xvstelm_b(dcval0, output_buf[1] + output_col + 1, 0, 12);
}

GLOBAL(void)
jsimd_idct_4x4_lasx(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
                    JDIMENSION output_col)
{
  JCOEFPTR quantptr = (JCOEFPTR)dct_table;
  __m256i coef01, coef23, coef45, coef67;
  __m256i quant01, quant23, quant45, quant67;
  __m256i tmp0, tmp2, tmp10, tmp12;
  __m256i zero = __lasx_xvldi(0);
  __m256i const128 = { 128 };
  __m256i dcval0, dcval1, dcval2, dcval3;
  __m256i dcval4, dcval5, dcval6, dcval7;

  v8i32 const184 =
    { FIX_1_847759065, FIX_1_847759065, FIX_1_847759065, FIX_1_847759065,
      FIX_1_847759065, FIX_1_847759065, FIX_1_847759065, FIX_1_847759065 };
  v8i32 const076 =
    {-FIX_0_765366865,-FIX_0_765366865,-FIX_0_765366865,-FIX_0_765366865,
     -FIX_0_765366865,-FIX_0_765366865,-FIX_0_765366865,-FIX_0_765366865 };
  v8i32 const021 =
    {-FIX_0_211164243,-FIX_0_211164243,-FIX_0_211164243,-FIX_0_211164243,
     -FIX_0_211164243,-FIX_0_211164243,-FIX_0_211164243,-FIX_0_211164243 };
  v8i32 const145 =
    { FIX_1_451774981, FIX_1_451774981, FIX_1_451774981, FIX_1_451774981,
      FIX_1_451774981, FIX_1_451774981, FIX_1_451774981, FIX_1_451774981 };
  v8i32 const217 =
    { FIX_2_172734803, FIX_2_172734803, FIX_2_172734803, FIX_2_172734803,
      FIX_2_172734803, FIX_2_172734803, FIX_2_172734803, FIX_2_172734803 };
  v8i32 const106 =
    { FIX_1_061594337, FIX_1_061594337, FIX_1_061594337, FIX_1_061594337,
      FIX_1_061594337, FIX_1_061594337, FIX_1_061594337, FIX_1_061594337 };
  v8i32 const050 =
    {-FIX_0_509795579,-FIX_0_509795579,-FIX_0_509795579,-FIX_0_509795579,
     -FIX_0_509795579,-FIX_0_509795579,-FIX_0_509795579,-FIX_0_509795579 };
  v8i32 const060 =
    { FIX_0_601344887, FIX_0_601344887, FIX_0_601344887, FIX_0_601344887,
      FIX_0_601344887, FIX_0_601344887, FIX_0_601344887, FIX_0_601344887 };
  v8i32 const089 =
    { FIX_0_899976223, FIX_0_899976223, FIX_0_899976223, FIX_0_899976223,
      FIX_0_899976223, FIX_0_899976223, FIX_0_899976223, FIX_0_899976223 };
  v8i32 const256 =
    { FIX_2_562915447, FIX_2_562915447, FIX_2_562915447, FIX_2_562915447,
      FIX_2_562915447, FIX_2_562915447, FIX_2_562915447, FIX_2_562915447 };

  LASX_LD_4(coef_block, 16, coef01, coef23, coef45, coef67);
  quant01 = LASX_LD(quantptr);
  dcval1 = __lasx_xvpermi_q(zero, coef01, 0x31);
  dcval5 = __lasx_xvpermi_q(zero, coef45, 0x31);

  const128 = __lasx_xvreplve0_h(const128);
  tmp0 = dcval1 | coef23 | dcval5 | coef67;
  if (__lasx_xbz_v(tmp0)) {
    tmp0 = __lasx_xvmul_h(coef01, quant01);
    LASX_UNPCK_L_W_H(tmp0, tmp0);
    tmp0 = __lasx_xvslli_w(tmp0, PASS1_BITS);
    dcval0 = __lasx_xvrepl128vei_w(tmp0, 0);
    dcval1 = __lasx_xvrepl128vei_w(tmp0, 1);
    dcval2 = __lasx_xvrepl128vei_w(tmp0, 2);
    dcval3 = __lasx_xvrepl128vei_w(tmp0, 3);
    dcval5 = __lasx_xvpermi_q(zero, dcval1, 0x31);
    dcval6 = __lasx_xvpermi_q(zero, dcval2, 0x31);
    dcval7 = __lasx_xvpermi_q(zero, dcval3, 0x31);
  } else {
    quant23 = LASX_LD(quantptr + 16);
    LASX_LD_2(quantptr + 32, 16, quant45, quant67);

    dcval0 = __lasx_xvmul_h(coef01, quant01);
    dcval2 = __lasx_xvmul_h(coef23, quant23);
    dcval4 = __lasx_xvmul_h(coef45, quant45);
    dcval6 = __lasx_xvmul_h(coef67, quant67);
    dcval1 = __lasx_xvpermi_q(zero, dcval0, 0x31);
    dcval3 = __lasx_xvpermi_q(zero, dcval2, 0x31);
    dcval5 = __lasx_xvpermi_q(zero, dcval4, 0x31);
    dcval7 = __lasx_xvpermi_q(zero, dcval6, 0x31);

    /* Even part */
    LASX_UNPCK_L_W_H(dcval0, dcval0);
    LASX_UNPCK_L_W_H(dcval2, dcval2);
    LASX_UNPCK_L_W_H(dcval6, dcval6);
    tmp0 = __lasx_xvslli_w(dcval0, CONST_BITS + 1);
    tmp2 = __lasx_xvmul_w(dcval2, (__m256i)const184);
    tmp2 = __lasx_xvmadd_w(tmp2, dcval6, (__m256i)const076);
    tmp10 = __lasx_xvadd_w(tmp0, tmp2);
    tmp12 = __lasx_xvsub_w(tmp0, tmp2);

    /* Odd part */
    LASX_UNPCK_L_W_H(dcval1, dcval1);
    LASX_UNPCK_L_W_H(dcval3, dcval3);
    LASX_UNPCK_L_W_H(dcval5, dcval5);
    LASX_UNPCK_L_W_H(dcval7, dcval7);

    tmp0 = __lasx_xvmul_w(dcval7, (__m256i)const021);
    tmp0 = __lasx_xvmadd_w(tmp0, dcval5, (__m256i)const145);
    tmp0 = __lasx_xvmsub_w(tmp0, dcval3, (__m256i)const217);
    tmp0 = __lasx_xvmadd_w(tmp0, dcval1, (__m256i)const106);

    tmp2 = __lasx_xvmul_w(dcval7, (__m256i)const050);
    tmp2 = __lasx_xvmsub_w(tmp2, dcval5, (__m256i)const060);
    tmp2 = __lasx_xvmadd_w(tmp2, dcval3, (__m256i)const089);
    tmp2 = __lasx_xvmadd_w(tmp2, dcval1, (__m256i)const256);

    LASX_BUTTERFLY_4(v8i32, tmp10, tmp12, tmp0, tmp2,
                     dcval0, dcval1, dcval2, dcval3);

    LASX_SRARI_W_4(dcval0, dcval1, dcval2, dcval3,
                   CONST_BITS - PASS1_BITS + 1);

    LASX_TRANSPOSE4x4_W_128SV(dcval0, dcval1, dcval2, dcval3,
                              dcval0, dcval1, dcval2, dcval3);

    dcval5 = __lasx_xvpermi_q(zero, dcval1, 0x31);
    dcval6 = __lasx_xvpermi_q(zero, dcval2, 0x31);
    dcval7 = __lasx_xvpermi_q(zero, dcval3, 0x31);
  }

  /* Even part */
  tmp0 = __lasx_xvslli_w(dcval0, CONST_BITS + 1);

  tmp2 = __lasx_xvmul_w(dcval2, (__m256i)const184);
  tmp2 = __lasx_xvmadd_w(tmp2, dcval6, (__m256i)const076);
  tmp10 = __lasx_xvadd_w(tmp0, tmp2);
  tmp12 = __lasx_xvsub_w(tmp0, tmp2);

  /* Odd part */
  tmp0 = __lasx_xvmul_w(dcval7, (__m256i)const021);
  tmp0 = __lasx_xvmadd_w(tmp0, dcval5, (__m256i)const145);
  tmp0 = __lasx_xvmsub_w(tmp0, dcval3, (__m256i)const217);
  tmp0 = __lasx_xvmadd_w(tmp0, dcval1, (__m256i)const106);

  tmp2 = __lasx_xvmul_w(dcval7, (__m256i)const050);
  tmp2 = __lasx_xvmsub_w(tmp2, dcval5, (__m256i)const060);
  tmp2 = __lasx_xvmadd_w(tmp2, dcval3, (__m256i)const089);
  tmp2 = __lasx_xvmadd_w(tmp2, dcval1, (__m256i)const256);

  LASX_BUTTERFLY_4(v8i32, tmp10, tmp12, tmp0, tmp2,
                   dcval0, dcval1, dcval2, dcval3);

  LASX_SRARI_W_4(dcval0, dcval1, dcval2, dcval3,
                 CONST_BITS + PASS1_BITS + 3 + 1);

  LASX_TRANSPOSE4x4_W_128SV(dcval0, dcval1, dcval2, dcval3,
                            dcval0, dcval1, dcval2, dcval3);

  LASX_PCKEV_Q_2(dcval1, dcval0, dcval3, dcval2, dcval0, dcval1);
  LASX_PCKEV_H(dcval1, dcval0, dcval0);
  dcval0 = __lasx_xvadd_h(dcval0, const128);
  LASX_CLIP_H_0_255(dcval0, dcval0);
  LASX_PCKEV_B_128SV(zero, dcval0, dcval0);

  __lasx_xvstelm_w(dcval0, output_buf[0] + output_col, 0, 0);
  __lasx_xvstelm_w(dcval0, output_buf[1] + output_col, 0, 1);
  __lasx_xvstelm_w(dcval0, output_buf[2] + output_col, 0, 4);
  __lasx_xvstelm_w(dcval0, output_buf[3] + output_col, 0, 5);
}
