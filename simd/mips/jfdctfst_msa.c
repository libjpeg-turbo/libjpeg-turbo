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
#include "../jsimd.h"
#include "jmacros_msa.h"


#define FIX_0_382683433       ((int32_t)( 98 * 256))  /* FIX(0.382683433) */
#define FIX_0_541196100_fast  ((int32_t)(139 * 256))  /* FIX(0.541196100) */
#define FIX_0_707106781       ((int32_t)(181 * 256))  /* FIX(0.707106781) */
#define FIX_1_306562965       ((int32_t)(334 * 256))  /* FIX(1.306562965) */

/* Do one pass of FDCT_IFAST */
#define FDCT_IFAST_1PASS(val0, val1, val2, val3, val4, val5, val6, val7, \
dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, const0) { \
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7; \
  v8i16 tmp10, tmp11, tmp12, tmp13; \
  v8i16 z1, z2, z3, z4, z5, z11, z13; \
  v4i32 z1_r, z1_l, z2_r, z2_l, z3_r, z3_l, z4_r, z4_l, z5_r, z5_l; \
  \
  BUTTERFLY_8(val0, val1, val2, val3, val4, val5, val6, val7, \
              tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7); \
  \
  BUTTERFLY_4(tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13); \
  \
  dst0 = tmp10 + tmp11; \
  dst4 = tmp10 - tmp11; \
  \
  tmp12 = tmp12 + tmp13; \
  UNPCK_SH_SW(tmp12, z1_r, z1_l); \
  z1_r = z1_r * __msa_splati_w(const0, 0); \
  z1_l = z1_l * __msa_splati_w(const0, 0); \
  z1 = __msa_pckod_h((v8i16)z1_l, (v8i16)z1_r); \
  \
  dst2 = tmp13 + z1; \
  dst6 = tmp13 - z1; \
  \
  /* Odd part */ \
  tmp10 = tmp4 + tmp5; \
  tmp11 = tmp5 + tmp6; \
  tmp12 = tmp6 + tmp7; \
  z5 = tmp10 - tmp12; \
  \
  UNPCK_SH4_SW(z5, tmp10, tmp12, tmp11, z5_r, z5_l, z2_r, z2_l, \
               z4_r, z4_l, z3_r, z3_l); \
  \
  z5_r = z5_r * __msa_splati_w(const0, 1); \
  z5_l = z5_l * __msa_splati_w(const0, 1); \
  z2_r = z2_r * __msa_splati_w(const0, 2); \
  z2_l = z2_l * __msa_splati_w(const0, 2); \
  z4_r = z4_r * __msa_splati_w(const0, 3); \
  z4_l = z4_l * __msa_splati_w(const0, 3); \
  z3_r = z3_r * __msa_splati_w(const0, 0); \
  z3_l = z3_l * __msa_splati_w(const0, 0); \
  \
  /* Descale */ \
  PCKOD_H2_SH(z5_l, z5_r, z2_l, z2_r, z5, z2); \
  PCKOD_H2_SH(z4_l, z4_r, z3_l, z3_r, z4, z3); \
  ADD2(z2, z5, z4, z5, z2, z4); \
  \
  z11 = tmp7 + z3; \
  z13 = tmp7 - z3; \
  \
  dst5 = z13 + z2; \
  dst3 = z13 - z2; \
  dst1 = z11 + z4; \
  dst7 = z11 - z4; \
}

GLOBAL(void)
jsimd_fdct_ifast_msa(DCTELEM *data)
{
  v8i16 val0, val1, val2, val3, val4, val5, val6, val7;
  v8i16 dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  v4i32 const0 = { FIX_0_707106781, FIX_0_382683433,
                   FIX_0_541196100_fast, FIX_1_306562965
                 };

  /* Load 8 rows */
  LD_SH8(data, DCTSIZE, val0, val1, val2, val3, val4, val5, val6, val7);

  /* Pass1 */
  /* Transpose */
  TRANSPOSE8x8_SH_SH(val0, val1, val2, val3, val4, val5, val6, val7,
                     val0, val1, val2, val3, val4, val5, val6, val7);
  FDCT_IFAST_1PASS(val0, val1, val2, val3, val4, val5, val6, val7,
                   dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, const0);

  /* Pass 2 */
  /* Transpose */
  TRANSPOSE8x8_SH_SH(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                     val0, val1, val2, val3, val4, val5, val6, val7);
  FDCT_IFAST_1PASS(val0, val1, val2, val3, val4, val5, val6, val7,
                   dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, const0);

  /* Store transformed data */
  ST_SH8(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, data, 8);
}
