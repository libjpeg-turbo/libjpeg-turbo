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

/* INTEGER QUANTIZATION AND SAMPLE CONVERSION */

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "jmacros_lsx.h"

#define LSX_MUL_WU_HU(_in0, _in1, _out0, _out1) \
{                                               \
  __m128i _tmp0, _tmp1, _tmp2, _tmp3;           \
  LSX_UNPCKLH_WU_HU(_in0, _tmp1, _tmp0);        \
  LSX_UNPCKLH_WU_HU(_in1, _tmp3, _tmp2);        \
  _out0 = __lsx_vmul_w(_tmp0, _tmp2);           \
  _out1 = __lsx_vmul_w(_tmp1, _tmp3);           \
}

GLOBAL(void)
jsimd_convsamp_lsx(JSAMPARRAY sample_data, JDIMENSION start_col,
                   DCTELEM *workspace)
{
  __m128i src0, src1, src2, src3, src4, src5, src6, src7;
  __m128i dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  __m128i const128 = {128};

  const128 = __lsx_vreplvei_h(const128, 0);

  src0 = LSX_LD(sample_data[0] + start_col);
  src1 = LSX_LD(sample_data[1] + start_col);
  src2 = LSX_LD(sample_data[2] + start_col);
  src3 = LSX_LD(sample_data[3] + start_col);
  src4 = LSX_LD(sample_data[4] + start_col);
  src5 = LSX_LD(sample_data[5] + start_col);
  src6 = LSX_LD(sample_data[6] + start_col);
  src7 = LSX_LD(sample_data[7] + start_col);

  LSX_UNPCK_L_HU_BU_8(src0, src1, src2, src3, src4, src5, src6, src7,
                      src0, src1, src2, src3, src4, src5, src6, src7);

  dst0 = __lsx_vsub_h(src0, const128);
  dst1 = __lsx_vsub_h(src1, const128);
  dst2 = __lsx_vsub_h(src2, const128);
  dst3 = __lsx_vsub_h(src3, const128);
  dst4 = __lsx_vsub_h(src4, const128);
  dst5 = __lsx_vsub_h(src5, const128);
  dst6 = __lsx_vsub_h(src6, const128);
  dst7 = __lsx_vsub_h(src7, const128);

  LSX_ST_8(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, workspace, 8);
}

GLOBAL(void)
jsimd_quantize_lsx(JCOEFPTR coef_block, DCTELEM *divisors,
                   DCTELEM *workspace)
{
  DCTELEM *recip, *corr, *shift;
  JDIMENSION cnt;
  __m128i src0, src1, src2, src3, rec0, rec1, rec2, rec3;
  __m128i cor0, cor1, cor2, cor3, shi0, shi1, shi2, shi3;
  __m128i src0_l, src0_h, src1_l, src1_h, src2_l, src2_h;
  __m128i src3_l, src3_h, shi0_l, shi0_h, shi1_l, shi1_h;
  __m128i shi2_l, shi2_h, shi3_l, shi3_h;
  __m128i sin0, sin1, sin2, sin3;
  __m128i zero = __lsx_vldi(0);

  recip = divisors;
  corr  = divisors + 64;
  shift = divisors + 192;

  for (cnt = 0; cnt < 2; cnt++) {
    LSX_LD_4(workspace + cnt * 32, 8, src0, src1 ,src2, src3);
    LSX_LD_4(recip + cnt * 32, 8, rec0, rec1, rec2, rec3);
    LSX_LD_4(corr + cnt * 32, 8, cor0, cor1, cor2, cor3);
    LSX_LD_4(shift + cnt * 32, 8, shi0, shi1, shi2, shi3);

    /* Get signs */
    sin0 = __lsx_vsrai_h(src0, 15);
    sin1 = __lsx_vsrai_h(src1, 15);
    sin2 = __lsx_vsrai_h(src2, 15);
    sin3 = __lsx_vsrai_h(src3, 15);

    /* Abs */
    src0 = __lsx_vadda_h(src0, zero);
    src1 = __lsx_vadda_h(src1, zero);
    src2 = __lsx_vadda_h(src2, zero);
    src3 = __lsx_vadda_h(src3, zero);

    src0 = __lsx_vadd_h(src0, cor0);
    src1 = __lsx_vadd_h(src1, cor1);
    src2 = __lsx_vadd_h(src2, cor2);
    src3 = __lsx_vadd_h(src3, cor3);

    /* Multiply */
    LSX_MUL_WU_HU(src0, rec0, src0_l, src0_h);
    LSX_MUL_WU_HU(src1, rec1, src1_l, src1_h);
    LSX_MUL_WU_HU(src2, rec2, src2_l, src2_h);
    LSX_MUL_WU_HU(src3, rec3, src3_l, src3_h);

    /* Shift */
    LSX_UNPCKLH_W_H(shi0, shi0_h, shi0_l);
    LSX_UNPCKLH_W_H(shi1, shi1_h, shi1_l);
    LSX_UNPCKLH_W_H(shi2, shi2_h, shi2_l);
    LSX_UNPCKLH_W_H(shi3, shi3_h, shi3_l);

    src0_l = __lsx_vsrl_w(src0_l, shi0_l);
    src0_h = __lsx_vsrl_w(src0_h, shi0_h);
    src1_l = __lsx_vsrl_w(src1_l, shi1_l);
    src1_h = __lsx_vsrl_w(src1_h, shi1_h);
    src2_l = __lsx_vsrl_w(src2_l, shi2_l);
    src2_h = __lsx_vsrl_w(src2_h, shi2_h);
    src3_l = __lsx_vsrl_w(src3_l, shi3_l);
    src3_h = __lsx_vsrl_w(src3_h, shi3_h);

    LSX_PCKOD_H_4(src0_h, src0_l, src1_h, src1_l, src2_h, src2_l,
                  src3_h, src3_l, src0, src1, src2, src3);

    /* Apply back signs */
    src0 = __lsx_vxor_v(src0, sin0);
    src1 = __lsx_vxor_v(src1, sin1);
    src2 = __lsx_vxor_v(src2, sin2);
    src3 = __lsx_vxor_v(src3, sin3);

    src0 = __lsx_vsub_h(src0, sin0);
    src1 = __lsx_vsub_h(src1, sin1);
    src2 = __lsx_vsub_h(src2, sin2);
    src3 = __lsx_vsub_h(src3, sin3);

    LSX_ST_4(src0, src1, src2, src3, (coef_block + cnt * 32), 8);
  }
}
