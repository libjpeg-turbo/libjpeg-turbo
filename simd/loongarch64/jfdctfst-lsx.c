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
#include "jmacros_lsx.h"

#define FIX_0_382683433       ((int32_t)( 98 << 8))  /* FIX(0.382683433) */
#define FIX_0_541196100       ((int32_t)(139 << 8))  /* FIX(0.541196100) */
#define FIX_0_707106781       ((int32_t)(181 << 8))  /* FIX(0.707106781) */
#define FIX_1_306562965       ((int32_t)(334 << 8))  /* FIX(1.306562965) */

#define DO_FDCT_IFAST(src0, src1, src2, src3, src4, src5, src6, src7,         \
                      dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7) {       \
  __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;                     \
  __m128i tmp10, tmp11, tmp12, tmp13;                                         \
  __m128i z1, z2, z3, z4, z5, z11, z13;                                       \
  __m128i z1_h, z1_l, z2_h, z2_l, z3_h, z3_l, z4_h, z4_l, z5_h, z5_l;         \
                                                                              \
  LSX_BUTTERFLY_8(v8i16, src0, src1, src2, src3, src4, src5, src6, src7,      \
                  tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);            \
                                                                              \
  LSX_BUTTERFLY_4(v8i16, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13); \
                                                                              \
  dst0 = __lsx_vadd_h(tmp10, tmp11);                                          \
  dst4 = __lsx_vsub_h(tmp10, tmp11);                                          \
                                                                              \
  tmp12 = __lsx_vadd_h(tmp12, tmp13);                                         \
  LSX_UNPCKLH_W_H(tmp12, z1_h, z1_l);                                         \
  z1_h = __lsx_vmul_w(z1_h, const7071);                                       \
  z1_l = __lsx_vmul_w(z1_l, const7071);                                       \
  LSX_PCKOD_H(z1_h, z1_l, z1);                                                \
                                                                              \
  dst2 = __lsx_vadd_h(tmp13, z1);                                             \
  dst6 = __lsx_vsub_h(tmp13, z1);                                             \
                                                                              \
  tmp10 = __lsx_vadd_h(tmp4, tmp5);                                           \
  tmp11 = __lsx_vadd_h(tmp5, tmp6);                                           \
  tmp12 = __lsx_vadd_h(tmp6, tmp7);                                           \
  z5 = __lsx_vsub_h(tmp10, tmp12);                                            \
                                                                              \
  LSX_UNPCKLH_W_H_4(z5, tmp10, tmp12, tmp11, z5_h, z5_l, z2_h, z2_l,          \
                    z4_h, z4_l, z3_h, z3_l);                                  \
  z2_l = __lsx_vmul_w(z2_l, const5411);                                       \
  z2_h = __lsx_vmul_w(z2_h, const5411);                                       \
  z3_l = __lsx_vmul_w(z3_l, const7071);                                       \
  z3_h = __lsx_vmul_w(z3_h, const7071);                                       \
  z4_l = __lsx_vmul_w(z4_l, const3065);                                       \
  z4_h = __lsx_vmul_w(z4_h, const3065);                                       \
  z5_l = __lsx_vmul_w(z5_l, const3826);                                       \
  z5_h = __lsx_vmul_w(z5_h, const3826);                                       \
                                                                              \
  LSX_PCKOD_H_4(z2_h, z2_l, z3_h, z3_l, z4_h, z4_l, z5_h, z5_l,               \
		z2, z3, z4, z5);                                              \
  z2 = __lsx_vadd_h(z2, z5);                                                  \
  z4 = __lsx_vadd_h(z4, z5);                                                  \
                                                                              \
  z11 = __lsx_vadd_h(tmp7, z3);                                               \
  z13 = __lsx_vsub_h(tmp7, z3);                                               \
                                                                              \
  LSX_BUTTERFLY_4(v8i16, z11, z13, z2, z4, dst1, dst5, dst3, dst7);           \
}

GLOBAL(void)
jsimd_fdct_ifast_lsx(DCTELEM *data)
{
  __m128i src0, src1, src2, src3, src4, src5, src6, src7;
  __m128i dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;

  __m128i const7071 = {FIX_0_707106781};
  __m128i const3826 = {FIX_0_382683433};
  __m128i const5411 = {FIX_0_541196100};
  __m128i const3065 = {FIX_1_306562965};

  const7071 = __lsx_vreplvei_w(const7071, 0);
  const3826 = __lsx_vreplvei_w(const3826, 0);
  const5411 = __lsx_vreplvei_w(const5411, 0);
  const3065 = __lsx_vreplvei_w(const3065, 0);

  LSX_LD_8(data, 8, src0, src1, src2, src3, src4, src5, src6, src7);

  LSX_TRANSPOSE8x8_H(src0, src1, src2, src3, src4, src5, src6, src7,
                     src0, src1, src2, src3, src4, src5, src6, src7);

  DO_FDCT_IFAST(src0, src1, src2, src3, src4, src5, src6, src7,
                dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  LSX_TRANSPOSE8x8_H(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                     src0, src1, src2, src3, src4, src5, src6, src7);

  DO_FDCT_IFAST(src0, src1, src2, src3, src4, src5, src6, src7,
                dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);

  LSX_ST_8(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, data, 8);
}
