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

/* INTEGER QUANTIZATION AND SAMPLE CONVERSION */

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "jmacros_lasx.h"

#define LASX_MUL_WU_HU(_in0, _in1, _out0, _out1) \
{ \
  __m256i _tmp0, _tmp1, _tmp2, _tmp3; \
  LASX_UNPCKLH_WU_HU(_in0, _tmp1, _tmp0); \
  LASX_UNPCKLH_WU_HU(_in1, _tmp3, _tmp2); \
  _out0 = __lasx_xvmul_w(_tmp0, _tmp2); \
  _out1 = __lasx_xvmul_w(_tmp1, _tmp3); \
}

GLOBAL(void)
jsimd_convsamp_lasx(JSAMPARRAY sample_data, JDIMENSION start_col,
                    DCTELEM *workspace)
{
  __m256i src0, src1, src2, src3, src4, src5, src6, src7;
  __m256i dst0, dst1, dst2, dst3;
  __m256i const128 = {128};

  const128 = __lasx_xvreplve0_h(const128);

  src0 = LASX_LD(sample_data[0] + start_col);
  src1 = LASX_LD(sample_data[1] + start_col);
  src2 = LASX_LD(sample_data[2] + start_col);
  src3 = LASX_LD(sample_data[3] + start_col);
  src4 = LASX_LD(sample_data[4] + start_col);
  src5 = LASX_LD(sample_data[5] + start_col);
  src6 = LASX_LD(sample_data[6] + start_col);
  src7 = LASX_LD(sample_data[7] + start_col);

  src0 = __lasx_xvpackev_d(src1, src0);
  src2 = __lasx_xvpackev_d(src3, src2);
  src4 = __lasx_xvpackev_d(src5, src4);
  src6 = __lasx_xvpackev_d(src7, src6);

  LASX_UNPCK_L_HU_BU_4(src0, src2, src4, src6, src0, src2, src4, src6);

  dst0 = __lasx_xvsub_h(src0, const128);
  dst1 = __lasx_xvsub_h(src2, const128);
  dst2 = __lasx_xvsub_h(src4, const128);
  dst3 = __lasx_xvsub_h(src6, const128);

  LASX_ST_4(dst0, dst1, dst2, dst3, workspace, 16);
}

GLOBAL(void)
jsimd_quantize_lasx(JCOEFPTR coef_block, DCTELEM *divisors,
                    DCTELEM *workspace)
{
  DCTELEM *recip, *corr, *shift;
  __m256i src0, src1, src2, src3, rec0, rec1, rec2, rec3;
  __m256i cor0, cor1, cor2, cor3, shi0, shi1, shi2, shi3;
  __m256i src0_l, src0_h, src1_l, src1_h, src2_l, src2_h;
  __m256i src3_l, src3_h, shi0_l, shi0_h, shi1_l, shi1_h;
  __m256i shi2_l, shi2_h, shi3_l, shi3_h;
  __m256i sin0, sin1, sin2, sin3;
  __m256i zero = __lasx_xvldi(0);

  recip = divisors;
  corr  = divisors + 64;
  shift = divisors + 192;

  LASX_LD_4(workspace, 16, src0, src1 ,src2, src3);
  LASX_LD_4(recip, 16, rec0, rec1, rec2, rec3);
  LASX_LD_4(corr, 16, cor0, cor1, cor2, cor3);
  LASX_LD_4(shift, 16, shi0, shi1, shi2, shi3);

  /* Get signs */
  sin0 = __lasx_xvsrai_h(src0, 15);
  sin1 = __lasx_xvsrai_h(src1, 15);
  sin2 = __lasx_xvsrai_h(src2, 15);
  sin3 = __lasx_xvsrai_h(src3, 15);

  /* Abs */
  src0 = __lasx_xvadda_h(src0, zero);
  src1 = __lasx_xvadda_h(src1, zero);
  src2 = __lasx_xvadda_h(src2, zero);
  src3 = __lasx_xvadda_h(src3, zero);

  src0 = __lasx_xvadd_h(src0, cor0);
  src1 = __lasx_xvadd_h(src1, cor1);
  src2 = __lasx_xvadd_h(src2, cor2);
  src3 = __lasx_xvadd_h(src3, cor3);

  /* Multiply */
  LASX_MUL_WU_HU(src0, rec0, src0_l, src0_h);
  LASX_MUL_WU_HU(src1, rec1, src1_l, src1_h);
  LASX_MUL_WU_HU(src2, rec2, src2_l, src2_h);
  LASX_MUL_WU_HU(src3, rec3, src3_l, src3_h);

  /* Shift */
  LASX_UNPCKLH_W_H(shi0, shi0_h, shi0_l);
  LASX_UNPCKLH_W_H(shi1, shi1_h, shi1_l);
  LASX_UNPCKLH_W_H(shi2, shi2_h, shi2_l);
  LASX_UNPCKLH_W_H(shi3, shi3_h, shi3_l);

  src0_l = __lasx_xvsrl_w(src0_l, shi0_l);
  src0_h = __lasx_xvsrl_w(src0_h, shi0_h);
  src1_l = __lasx_xvsrl_w(src1_l, shi1_l);
  src1_h = __lasx_xvsrl_w(src1_h, shi1_h);
  src2_l = __lasx_xvsrl_w(src2_l, shi2_l);
  src2_h = __lasx_xvsrl_w(src2_h, shi2_h);
  src3_l = __lasx_xvsrl_w(src3_l, shi3_l);
  src3_h = __lasx_xvsrl_w(src3_h, shi3_h);

  LASX_PCKOD_H_4(src0_h, src0_l, src1_h, src1_l, src2_h, src2_l,
                 src3_h, src3_l, src0, src1, src2, src3);

  /* Apply back signs */
  src0 = __lasx_xvxor_v(src0, sin0);
  src1 = __lasx_xvxor_v(src1, sin1);
  src2 = __lasx_xvxor_v(src2, sin2);
  src3 = __lasx_xvxor_v(src3, sin3);

  src0 = __lasx_xvsub_h(src0, sin0);
  src1 = __lasx_xvsub_h(src1, sin1);
  src2 = __lasx_xvsub_h(src2, sin2);
  src3 = __lasx_xvsub_h(src3, sin3);

  LASX_ST_4(src0, src1, src2, src3, coef_block, 16);
}
