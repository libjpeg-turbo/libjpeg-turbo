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
#define CONST_BITS_FAST 8

#define FIX_1_082392200 ((int32_t)277)  /* FIX(1.082392200) */
#define FIX_1_414213562 ((int32_t)362)  /* FIX(1.414213562) */
#define FIX_1_847759065 ((int32_t)473)  /* FIX(1.847759065) */
#define FIX_2_613125930 ((int32_t)669)  /* FIX(2.613125930) */

GLOBAL(void)
jsimd_idct_ifast_lsx(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
                     JDIMENSION output_col)
{
  JCOEFPTR quantptr = (JCOEFPTR)dct_table;
  __m128i coef0, coef1, coef2, coef3, coef4, coef5, coef6, coef7;
  __m128i quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7;
  __m128i tmp0, tmp1, tmp2, tmp3;
  __m128i tmp4, tmp5, tmp6, tmp7;
  __m128i tmp10, tmp11, tmp12, tmp13;
  __m128i tmp11_h, tmp11_l, tmp12_h, tmp12_l;
  __m128i dst0, dst1, dst2, dst3;
  __m128i dst4, dst5, dst6, dst7;
  __m128i z5, z10, z11, z12, z13;
  __m128i z5_h, z5_l, z10_h, z10_l, z12_h, z12_l;
  __m128i const128 = { 128 };
  __m128i const1414 = { FIX_1_414213562 };
  __m128i const1847 = { FIX_1_847759065 };
  __m128i const1082 = { FIX_1_082392200 };
  __m128i const2613 = { -FIX_2_613125930 };

  const2613 = __lsx_vreplvei_w(const2613, 0);
  const1082 = __lsx_vreplvei_w(const1082, 0);
  const1847 = __lsx_vreplvei_w(const1847, 0);
  const1414 = __lsx_vreplvei_w(const1414, 0);
  const128 = __lsx_vreplvei_h(const128, 0);

  LSX_LD_4(coef_block, 8, coef0, coef1, coef2, coef3);
  LSX_LD_4(coef_block + 32, 8, coef4, coef5, coef6, coef7);

  tmp0 = coef1 | coef2 | coef3 | coef4 | coef5 | coef6 | coef7;

  if (__lsx_bz_v(tmp0)) {
    /* AC terms all zero */
    quant0 = LSX_LD(quantptr);
    tmp0 = __lsx_vmul_h(coef0, quant0);

    dst0 = __lsx_vreplvei_h(tmp0, 0);
    dst1 = __lsx_vreplvei_h(tmp0, 1);
    dst2 = __lsx_vreplvei_h(tmp0, 2);
    dst3 = __lsx_vreplvei_h(tmp0, 3);
    dst4 = __lsx_vreplvei_h(tmp0, 4);
    dst5 = __lsx_vreplvei_h(tmp0, 5);
    dst6 = __lsx_vreplvei_h(tmp0, 6);
    dst7 = __lsx_vreplvei_h(tmp0, 7);
  } else {
    LSX_LD_4(quantptr, 8, quant0, quant1, quant2, quant3);
    LSX_LD_4(quantptr + 32, 8, quant4, quant5, quant6, quant7);

    dst0 = __lsx_vmul_h(coef0, quant0);
    dst1 = __lsx_vmul_h(coef1, quant1);
    dst2 = __lsx_vmul_h(coef2, quant2);
    dst3 = __lsx_vmul_h(coef3, quant3);
    dst4 = __lsx_vmul_h(coef4, quant4);
    dst5 = __lsx_vmul_h(coef5, quant5);
    dst6 = __lsx_vmul_h(coef6, quant6);
    dst7 = __lsx_vmul_h(coef7, quant7);

    /* Even part */
    LSX_BUTTERFLY_4(v8i16, dst0, dst2, dst6, dst4,
                    tmp10, tmp13, tmp12, tmp11);

    LSX_UNPCKLH_W_H(tmp12, tmp12_l, tmp12_h);
    tmp12_h = __lsx_vmul_w(tmp12_h, const1414);
    tmp12_l = __lsx_vmul_w(tmp12_l, const1414);

    tmp12_h = __lsx_vsrai_w(tmp12_h, CONST_BITS_FAST);
    tmp12_l = __lsx_vsrai_w(tmp12_l, CONST_BITS_FAST);

    LSX_PCKEV_H(tmp12_l, tmp12_h, tmp12);
    tmp12 = __lsx_vsub_h(tmp12, tmp13);

    LSX_BUTTERFLY_4(v8i16, tmp10, tmp11, tmp12, tmp13,
                    tmp0, tmp1, tmp2, tmp3);

    /* Odd part */
    LSX_BUTTERFLY_4(v8i16, dst5, dst1, dst7, dst3,
                    z13, z11, z12, z10);

    tmp7 = __lsx_vadd_h(z11, z13);

    tmp11 = __lsx_vsub_h(z11, z13);
    LSX_UNPCKLH_W_H(tmp11, tmp11_l, tmp11_h);
    tmp11_h = __lsx_vmul_w(tmp11_h, const1414);
    tmp11_l = __lsx_vmul_w(tmp11_l, const1414);
    tmp11_h = __lsx_vsrai_w(tmp11_h, CONST_BITS_FAST);
    tmp11_l = __lsx_vsrai_w(tmp11_l, CONST_BITS_FAST);
    LSX_PCKEV_H(tmp11_l, tmp11_h, tmp11);

    z5 = __lsx_vadd_h(z10, z12);
    LSX_UNPCKLH_W_H(z5, z5_l, z5_h);
    z5_h = __lsx_vmul_w(z5_h, const1847);
    z5_l = __lsx_vmul_w(z5_l, const1847);
    z5_h = __lsx_vsrai_w(z5_h, CONST_BITS_FAST);
    z5_l = __lsx_vsrai_w(z5_l, CONST_BITS_FAST);
    LSX_PCKEV_H(z5_l, z5_h, z5);

    LSX_UNPCKLH_W_H(z12, z12_l, z12_h);
    z12_h = __lsx_vmul_w(z12_h, const1082);
    z12_l = __lsx_vmul_w(z12_l, const1082);
    z12_h = __lsx_vsrai_w(z12_h, CONST_BITS_FAST);
    z12_l = __lsx_vsrai_w(z12_l, CONST_BITS_FAST);
    LSX_PCKEV_H(z12_l, z12_h, tmp10);
    tmp10 = __lsx_vsub_h(tmp10, z5);

    LSX_UNPCKLH_W_H(z10, z10_l, z10_h);
    z10_h = __lsx_vmul_w(z10_h, const2613);
    z10_l = __lsx_vmul_w(z10_l, const2613);
    z10_h = __lsx_vsrai_w(z10_h, CONST_BITS_FAST);
    z10_l = __lsx_vsrai_w(z10_l, CONST_BITS_FAST);
    LSX_PCKEV_H(z10_l, z10_h, tmp12);
    tmp12 = __lsx_vadd_h(tmp12, z5);

    tmp6 = __lsx_vsub_h(tmp12, tmp7);
    tmp5 = __lsx_vsub_h(tmp11, tmp6);
    tmp4 = __lsx_vadd_h(tmp10, tmp5);

    LSX_BUTTERFLY_8(v8i16, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
                    dst0, dst1, dst2, dst4, dst3, dst5, dst6, dst7);

    LSX_TRANSPOSE8x8_H(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                       dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);
  }

  /* Even part */
  LSX_BUTTERFLY_4(v8i16, dst0, dst2, dst6, dst4,
                  tmp10, tmp13, tmp12, tmp11);

  LSX_UNPCKLH_W_H(tmp12, tmp12_l, tmp12_h);
  tmp12_h = __lsx_vmul_w(tmp12_h, const1414);
  tmp12_l = __lsx_vmul_w(tmp12_l, const1414);
  tmp12_h = __lsx_vsrai_w(tmp12_h, CONST_BITS_FAST);
  tmp12_l = __lsx_vsrai_w(tmp12_l, CONST_BITS_FAST);
  LSX_PCKEV_H(tmp12_l, tmp12_h, tmp12);
  tmp12 = __lsx_vsub_h(tmp12, tmp13);

  LSX_BUTTERFLY_4(v8i16, tmp10, tmp11, tmp12, tmp13,
                  tmp0, tmp1, tmp2, tmp3);

  /* Odd part */
  LSX_BUTTERFLY_4(v8i16, dst5, dst1, dst7, dst3,
                  z13, z11, z12, z10);

  tmp7 = __lsx_vadd_h(z11, z13);

  tmp11 = __lsx_vsub_h(z11, z13);
  LSX_UNPCKLH_W_H(tmp11, tmp11_l, tmp11_h);
  tmp11_h = __lsx_vmul_w(tmp11_h, const1414);
  tmp11_l = __lsx_vmul_w(tmp11_l, const1414);
  tmp11_h = __lsx_vsrai_w(tmp11_h, CONST_BITS_FAST);
  tmp11_l = __lsx_vsrai_w(tmp11_l, CONST_BITS_FAST);
  LSX_PCKEV_H(tmp11_l, tmp11_h, tmp11);

  z5 = __lsx_vadd_h(z10, z12);
  LSX_UNPCKLH_W_H(z5, z5_l, z5_h);
  z5_h = __lsx_vmul_w(z5_h, const1847);
  z5_l = __lsx_vmul_w(z5_l, const1847);
  z5_h = __lsx_vsrai_w(z5_h, CONST_BITS_FAST);
  z5_l = __lsx_vsrai_w(z5_l, CONST_BITS_FAST);
  LSX_PCKEV_H(z5_l, z5_h, z5);

  LSX_UNPCKLH_W_H(z12, z12_l, z12_h);
  z12_h = __lsx_vmul_w(z12_h, const1082);
  z12_l = __lsx_vmul_w(z12_l, const1082);
  z12_h = __lsx_vsrai_w(z12_h, CONST_BITS_FAST);
  z12_l = __lsx_vsrai_w(z12_l, CONST_BITS_FAST);
  LSX_PCKEV_H(z12_l, z12_h, tmp10);
  tmp10 = __lsx_vsub_h(tmp10, z5);

  LSX_UNPCKLH_W_H(z10, z10_l, z10_h);
  z10_h = __lsx_vmul_w(z10_h, const2613);
  z10_l = __lsx_vmul_w(z10_l, const2613);
  z10_h = __lsx_vsrai_w(z10_h, CONST_BITS_FAST);
  z10_l = __lsx_vsrai_w(z10_l, CONST_BITS_FAST);
  LSX_PCKEV_H(z10_l, z10_h, tmp12);
  tmp12 = __lsx_vadd_h(tmp12, z5);

  tmp6 = __lsx_vsub_h(tmp12, tmp7);
  tmp5 = __lsx_vsub_h(tmp11, tmp6);
  tmp4 = __lsx_vadd_h(tmp10, tmp5);

  LSX_BUTTERFLY_8(v8i16, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
                  dst0, dst1, dst2, dst4, dst3, dst5, dst6, dst7);

  dst0 = __lsx_vsrai_h(dst0, PASS1_BITS + 3);
  dst1 = __lsx_vsrai_h(dst1, PASS1_BITS + 3);
  dst2 = __lsx_vsrai_h(dst2, PASS1_BITS + 3);
  dst3 = __lsx_vsrai_h(dst3, PASS1_BITS + 3);
  dst4 = __lsx_vsrai_h(dst4, PASS1_BITS + 3);
  dst5 = __lsx_vsrai_h(dst5, PASS1_BITS + 3);
  dst6 = __lsx_vsrai_h(dst6, PASS1_BITS + 3);
  dst7 = __lsx_vsrai_h(dst7, PASS1_BITS + 3);

  dst0 = __lsx_vadd_h(dst0, const128);
  dst1 = __lsx_vadd_h(dst1, const128);
  dst2 = __lsx_vadd_h(dst2, const128);
  dst3 = __lsx_vadd_h(dst3, const128);
  dst4 = __lsx_vadd_h(dst4, const128);
  dst5 = __lsx_vadd_h(dst5, const128);
  dst6 = __lsx_vadd_h(dst6, const128);
  dst7 = __lsx_vadd_h(dst7, const128);

  LSX_CLIP_H_0_255_4(dst0, dst1, dst2, dst3,
                     dst0, dst1, dst2, dst3);
  LSX_CLIP_H_0_255_4(dst4, dst5, dst6, dst7,
                     dst4, dst5, dst6, dst7);

  LSX_PCKEV_B_8(dst0, dst0, dst1, dst1, dst2, dst2, dst3, dst3,
                dst4, dst4, dst5, dst5, dst6, dst6, dst7, dst7,
                dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  LSX_TRANSPOSE8x8_B(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                     dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  __lsx_vstelm_d(dst0, output_buf[0] + output_col, 0, 0);
  __lsx_vstelm_d(dst1, output_buf[1] + output_col, 0, 0);
  __lsx_vstelm_d(dst2, output_buf[2] + output_col, 0, 0);
  __lsx_vstelm_d(dst3, output_buf[3] + output_col, 0, 0);
  __lsx_vstelm_d(dst4, output_buf[4] + output_col, 0, 0);
  __lsx_vstelm_d(dst5, output_buf[5] + output_col, 0, 0);
  __lsx_vstelm_d(dst6, output_buf[6] + output_col, 0, 0);
  __lsx_vstelm_d(dst7, output_buf[7] + output_col, 0, 0);
}
