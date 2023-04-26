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
#define CONST_BITS_FAST 8

#define FIX_1_082392200 ((int32_t)277)  /* FIX(1.082392200) */
#define FIX_1_414213562 ((int32_t)362)  /* FIX(1.414213562) */
#define FIX_1_847759065 ((int32_t)473)  /* FIX(1.847759065) */
#define FIX_2_613125930 ((int32_t)669)  /* FIX(2.613125930) */

GLOBAL(void)
jsimd_idct_ifast_lasx(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
                      JDIMENSION output_col)
{
  JCOEFPTR quantptr = (JCOEFPTR)dct_table;
  __m256i coef01, coef23, coef45, coef67;
  __m256i quant01, quant23, quant45, quant67;
  __m256i zero = __lasx_xvldi(0);
  __m256i tmp0, tmp1, tmp2, tmp3;
  __m256i tmp4, tmp5, tmp6, tmp7;
  __m256i tmp10, tmp11, tmp12, tmp13;
  __m256i dst0, dst1, dst2, dst3;
  __m256i dst4, dst5, dst6, dst7;
  __m256i z5, z10, z11, z12, z13;
  __m256i const128 = { 128 };
  __m256i const1414 = { FIX_1_414213562 };
  __m256i const1847 = { FIX_1_847759065 };
  __m256i const1082 = { FIX_1_082392200 };
  __m256i const2613 = { -FIX_2_613125930 };

  const2613 = __lasx_xvreplve0_w(const2613);
  const1082 = __lasx_xvreplve0_w(const1082);
  const1847 = __lasx_xvreplve0_w(const1847);
  const1414 = __lasx_xvreplve0_w(const1414);
  const128 = __lasx_xvreplve0_h(const128);

  LASX_LD_4(coef_block, 16, coef01, coef23, coef45, coef67);

  tmp0 = __lasx_xvpermi_q(zero, coef01, 0x31);
  tmp0 = __lasx_xvor_v(tmp0, coef23);
  tmp0 = __lasx_xvor_v(tmp0, coef45);
  tmp0 = __lasx_xvor_v(tmp0, coef67);

  if (__lasx_xbz_v(tmp0)) {
    /* AC terms all zero */
    quant01 = LASX_LD(quantptr);
    dst0 = __lasx_xvmul_h(coef01, quant01);
    tmp1  = __lasx_xvpermi_q(dst0, dst0, 0x20);
    dst0 = __lasx_xvrepl128vei_h(tmp1, 0);
    dst1 = __lasx_xvrepl128vei_h(tmp1, 1);
    dst2 = __lasx_xvrepl128vei_h(tmp1, 2);
    dst3 = __lasx_xvrepl128vei_h(tmp1, 3);
    dst4 = __lasx_xvrepl128vei_h(tmp1, 4);
    dst5 = __lasx_xvrepl128vei_h(tmp1, 5);
    dst6 = __lasx_xvrepl128vei_h(tmp1, 6);
    dst7 = __lasx_xvrepl128vei_h(tmp1, 7);
  } else {
    LASX_LD_4(quantptr, 16, quant01, quant23, quant45, quant67);

    dst0 = __lasx_xvmul_h(coef01, quant01);
    dst2 = __lasx_xvmul_h(coef23, quant23);
    dst4 = __lasx_xvmul_h(coef45, quant45);
    dst6 = __lasx_xvmul_h(coef67, quant67);
    dst1 = __lasx_xvpermi_q(zero, dst0, 0x31);
    dst0 = __lasx_xvpermi_q(zero, dst0, 0x20);
    dst3 = __lasx_xvpermi_q(zero, dst2, 0x31);
    dst2 = __lasx_xvpermi_q(zero, dst2, 0x20);
    dst5 = __lasx_xvpermi_q(zero, dst4, 0x31);
    dst4 = __lasx_xvpermi_q(zero, dst4, 0x20);
    dst7 = __lasx_xvpermi_q(zero, dst6, 0x31);
    dst6 = __lasx_xvpermi_q(zero, dst6, 0x20);

    /* Even part */
    LASX_BUTTERFLY_4(v16i16, dst0, dst2, dst6, dst4,
                     tmp10, tmp13, tmp12, tmp11);

    LASX_UNPCK_L_W_H(tmp12, tmp12);
    tmp12 = __lasx_xvmul_w(tmp12, const1414);
    tmp12 = __lasx_xvsrai_w(tmp12, CONST_BITS_FAST);
    LASX_PCKEV_H(zero, tmp12, tmp12);
    tmp12 = __lasx_xvsub_h(tmp12, tmp13);

    LASX_BUTTERFLY_4(v16i16, tmp10, tmp11, tmp12, tmp13,
                     tmp0, tmp1, tmp2, tmp3);

    /* Odd part */
    LASX_BUTTERFLY_4(v16i16, dst5, dst1, dst7, dst3,
                     z13, z11, z12, z10);

    tmp7 = __lasx_xvadd_h(z11, z13);

    tmp11 = __lasx_xvsub_h(z11, z13);
    LASX_UNPCK_L_W_H(tmp11, tmp11);
    tmp11 = __lasx_xvmul_w(tmp11, const1414);
    tmp11 = __lasx_xvsrai_w(tmp11, CONST_BITS_FAST);
    LASX_PCKEV_H(zero, tmp11, tmp11);

    z5 = __lasx_xvadd_h(z10, z12);
    LASX_UNPCK_L_W_H(z5, z5);
    z5 = __lasx_xvmul_w(z5, const1847);
    z5 = __lasx_xvsrai_w(z5, CONST_BITS_FAST);
    LASX_PCKEV_H(zero, z5, z5);

    LASX_UNPCK_L_W_H(z12, tmp10);
    tmp10 = __lasx_xvmul_w(tmp10, const1082);
    tmp10 = __lasx_xvsrai_w(tmp10, CONST_BITS_FAST);
    LASX_PCKEV_H(zero, tmp10, tmp10);
    tmp10 = __lasx_xvsub_h(tmp10, z5);

    LASX_UNPCK_L_W_H(z10, tmp12);
    tmp12 = __lasx_xvmul_w(tmp12, const2613);
    tmp12 = __lasx_xvsrai_w(tmp12, CONST_BITS_FAST);
    LASX_PCKEV_H(zero, tmp12, tmp12);
    tmp12 = __lasx_xvadd_h(tmp12, z5);

    tmp6 = __lasx_xvsub_h(tmp12, tmp7);
    tmp5 = __lasx_xvsub_h(tmp11, tmp6);
    tmp4 = __lasx_xvadd_h(tmp10, tmp5);

    LASX_BUTTERFLY_8(v16i16, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
                     dst0, dst1, dst2, dst4, dst3, dst5, dst6, dst7);

    LASX_TRANSPOSE8x8_H_128SV(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                              dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);
  }

  /* Even part */
  LASX_BUTTERFLY_4(v16i16, dst0, dst2, dst6, dst4,
                   tmp10, tmp13, tmp12, tmp11);

  LASX_UNPCK_L_W_H(tmp12, tmp12);
  tmp12 = __lasx_xvmul_w(tmp12, const1414);
  tmp12 = __lasx_xvsrai_w(tmp12, CONST_BITS_FAST);
  LASX_PCKEV_H(zero, tmp12, tmp12);
  tmp12 = __lasx_xvsub_h(tmp12, tmp13);

  LASX_BUTTERFLY_4(v16i16, tmp10, tmp11, tmp12, tmp13,
                   tmp0, tmp1, tmp2, tmp3);

  /* Odd part */
  LASX_BUTTERFLY_4(v16i16, dst5, dst1, dst7, dst3,
                   z13, z11, z12, z10);

  tmp7 = __lasx_xvadd_h(z11, z13);

  tmp11 = __lasx_xvsub_h(z11, z13);
  LASX_UNPCK_L_W_H(tmp11, tmp11);
  tmp11 = __lasx_xvmul_w(tmp11, const1414);
  tmp11 = __lasx_xvsrai_w(tmp11, CONST_BITS_FAST);
  LASX_PCKEV_H(zero, tmp11, tmp11);

  z5 = __lasx_xvadd_h(z10, z12);
  LASX_UNPCK_L_W_H(z5, z5);
  z5 = __lasx_xvmul_w(z5, const1847);
  z5 = __lasx_xvsrai_w(z5, CONST_BITS_FAST);
  LASX_PCKEV_H(zero, z5, z5);

  LASX_UNPCK_L_W_H(z12, tmp10);
  tmp10 = __lasx_xvmul_w(tmp10, const1082);
  tmp10 = __lasx_xvsrai_w(tmp10, CONST_BITS_FAST);
  LASX_PCKEV_H(zero, tmp10, tmp10);
  tmp10 = __lasx_xvsub_h(tmp10, z5);

  LASX_UNPCK_L_W_H(z10, tmp12);
  tmp12 = __lasx_xvmul_w(tmp12, const2613);
  tmp12 = __lasx_xvsrai_w(tmp12, CONST_BITS_FAST);
  LASX_PCKEV_H(zero, tmp12, tmp12);
  tmp12 = __lasx_xvadd_h(tmp12, z5);

  tmp6 = __lasx_xvsub_h(tmp12, tmp7);
  tmp5 = __lasx_xvsub_h(tmp11, tmp6);
  tmp4 = __lasx_xvadd_h(tmp10, tmp5);

  LASX_BUTTERFLY_8(v16i16, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
                   dst0, dst1, dst2, dst4, dst3, dst5, dst6, dst7);

  dst0 = __lasx_xvsrai_h(dst0, PASS1_BITS + 3);
  dst1 = __lasx_xvsrai_h(dst1, PASS1_BITS + 3);
  dst2 = __lasx_xvsrai_h(dst2, PASS1_BITS + 3);
  dst3 = __lasx_xvsrai_h(dst3, PASS1_BITS + 3);
  dst4 = __lasx_xvsrai_h(dst4, PASS1_BITS + 3);
  dst5 = __lasx_xvsrai_h(dst5, PASS1_BITS + 3);
  dst6 = __lasx_xvsrai_h(dst6, PASS1_BITS + 3);
  dst7 = __lasx_xvsrai_h(dst7, PASS1_BITS + 3);

  dst0 = __lasx_xvadd_h(dst0, const128);
  dst1 = __lasx_xvadd_h(dst1, const128);
  dst2 = __lasx_xvadd_h(dst2, const128);
  dst3 = __lasx_xvadd_h(dst3, const128);
  dst4 = __lasx_xvadd_h(dst4, const128);
  dst5 = __lasx_xvadd_h(dst5, const128);
  dst6 = __lasx_xvadd_h(dst6, const128);
  dst7 = __lasx_xvadd_h(dst7, const128);

  LASX_CLIP_H_0_255_4(dst0, dst1, dst2, dst3,
                      dst0, dst1, dst2, dst3);
  LASX_CLIP_H_0_255_4(dst4, dst5, dst6, dst7,
                      dst4, dst5, dst6, dst7);

  LASX_PCKEV_B_8(zero, dst0, zero, dst1, zero, dst2, zero, dst3,
                 zero, dst4, zero, dst5, zero, dst6, zero, dst7,
                 dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  LASX_TRANSPOSE8x8_B(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                      dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  __lasx_xvstelm_d(dst0, output_buf[0] + output_col, 0, 0);
  __lasx_xvstelm_d(dst1, output_buf[1] + output_col, 0, 0);
  __lasx_xvstelm_d(dst2, output_buf[2] + output_col, 0, 0);
  __lasx_xvstelm_d(dst3, output_buf[3] + output_col, 0, 0);
  __lasx_xvstelm_d(dst4, output_buf[4] + output_col, 0, 0);
  __lasx_xvstelm_d(dst5, output_buf[5] + output_col, 0, 0);
  __lasx_xvstelm_d(dst6, output_buf[6] + output_col, 0, 0);
  __lasx_xvstelm_d(dst7, output_buf[7] + output_col, 0, 0);
}
