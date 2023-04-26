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
#include "jmacros_lasx.h"


#define FIX_0_382683433       ((int32_t)( 98 << 8))  /* FIX(0.382683433) */
#define FIX_0_541196100       ((int32_t)(139 << 8))  /* FIX(0.541196100) */
#define FIX_0_707106781       ((int32_t)(181 << 8))  /* FIX(0.707106781) */
#define FIX_1_306562965       ((int32_t)(334 << 8))  /* FIX(1.306562965) */

#define DO_FDCT_IFAST(src0, src1, src2, src3, src4, src5, src6, src7, \
                      dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7) { \
  __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7; \
  __m256i tmp10, tmp11, tmp12, tmp13; \
  __m256i z1, z2, z3, z4, z5, z11, z13; \
\
  LASX_BUTTERFLY_8(v16i16, src0, src1, src2, src3, src4, src5, src6, src7, \
                   tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7); \
\
  LASX_BUTTERFLY_4(v16i16, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13); \
\
  dst0 = __lasx_xvadd_h(tmp10, tmp11); \
  dst4 = __lasx_xvsub_h(tmp10, tmp11); \
\
  tmp12 = __lasx_xvadd_h(tmp12, tmp13); \
  LASX_UNPCK_L_W_H(tmp12, tmp12); \
  z1 = __lasx_xvmul_w(tmp12, const7071); \
  LASX_PCKOD_H(zero, z1, z1); \
\
  dst2 = __lasx_xvadd_h(tmp13, z1); \
  dst6 = __lasx_xvsub_h(tmp13, z1); \
\
  tmp10 = __lasx_xvadd_h(tmp4, tmp5); \
  tmp11 = __lasx_xvadd_h(tmp5, tmp6); \
  tmp12 = __lasx_xvadd_h(tmp6, tmp7); \
  z5 = __lasx_xvsub_h(tmp10, tmp12); \
\
  LASX_UNPCK_L_W_H_4(z5, tmp10, tmp11, tmp12, z5, tmp10, tmp11, tmp12); \
  z5 = __lasx_xvmul_w(z5, const3826); \
  z2 = __lasx_xvmul_w(tmp10, const5411); \
  z4 = __lasx_xvmul_w(tmp12, const3065); \
  z3 = __lasx_xvmul_w(tmp11, const7071); \
\
  LASX_PCKOD_H_4(zero, z5, zero, z2, zero, z4, zero, z3, z5, z2, z4, z3); \
  z2 = __lasx_xvadd_h(z2, z5); \
  z4 = __lasx_xvadd_h(z4, z5); \
\
  z11 = __lasx_xvadd_h(tmp7, z3); \
  z13 = __lasx_xvsub_h(tmp7, z3); \
\
  LASX_BUTTERFLY_4(v16i16, z11, z13, z2, z4, dst1, dst5, dst3, dst7); \
}

GLOBAL(void)
jsimd_fdct_ifast_lasx(DCTELEM *data)
{
  __m256i zero = __lasx_xvldi(0);
  __m256i src0, src1, src2, src3, src4, src5, src6, src7;
  __m256i dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;

  __m256i const7071 = {FIX_0_707106781};
  __m256i const3826 = {FIX_0_382683433};
  __m256i const5411 = {FIX_0_541196100};
  __m256i const3065 = {FIX_1_306562965};

  const7071 = __lasx_xvreplve0_w(const7071);
  const3826 = __lasx_xvreplve0_w(const3826);
  const5411 = __lasx_xvreplve0_w(const5411);
  const3065 = __lasx_xvreplve0_w(const3065);

  LASX_LD_4(data, 16, src0, src2, src4, src6);

  src1 = __lasx_xvpermi_q(zero, src0, 0x31);
  src3 = __lasx_xvpermi_q(zero, src2, 0x31);
  src5 = __lasx_xvpermi_q(zero, src4, 0x31);
  src7 = __lasx_xvpermi_q(zero, src6, 0x31);

  LASX_TRANSPOSE8x8_H_128SV(src0, src1, src2, src3, src4, src5, src6, src7,
                            src0, src1, src2, src3, src4, src5, src6, src7);

  DO_FDCT_IFAST(src0, src1, src2, src3, src4, src5, src6, src7,
                dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  LASX_TRANSPOSE8x8_H_128SV(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                            src0, src1, src2, src3, src4, src5, src6, src7);

  DO_FDCT_IFAST(src0, src1, src2, src3, src4, src5, src6, src7,
                dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  dst0 = __lasx_xvpermi_q(dst1, dst0, 0x20);
  dst2 = __lasx_xvpermi_q(dst3, dst2, 0x20);
  dst4 = __lasx_xvpermi_q(dst5, dst4, 0x20);
  dst6 = __lasx_xvpermi_q(dst7, dst6, 0x20);

  LASX_ST_4(dst0, dst2, dst4, dst6, data, 16);
}
