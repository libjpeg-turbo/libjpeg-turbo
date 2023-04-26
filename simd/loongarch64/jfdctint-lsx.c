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

#define CONST_BITS  13
#define PASS1_BITS  2

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

GLOBAL(void)
jsimd_fdct_islow_lsx(DCTELEM *data)
{
  __m128i temp;
  __m128i val0, val1, val2, val3, val4, val5, val6, val7;
  __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  __m128i tmp10, tmp11, tmp12, tmp13;
  __m128i tmp4_h, tmp4_l, tmp5_h, tmp5_l, tmp6_h, tmp6_l, tmp7_h, tmp7_l;
  __m128i dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  __m128i z1_h, z1_l, z2_h, z2_l, z3_h, z3_l, z4_h, z4_l, z5_h, z5_l;
  __m128i z12_h, z12_l, z13_h, z13_l;

  __m128i const5411 = {FIX_0_541196100};
  __m128i const7653 = {FIX_0_765366865};
  __m128i const8477 = {-FIX_1_847759065};
  __m128i const1758 = {FIX_1_175875602};
  __m128i const8999 = {-FIX_0_899976223};
  __m128i const5629 = {-FIX_2_562915447};
  __m128i const9615 = {-FIX_1_961570560};
  __m128i const3901 = {-FIX_0_390180644};
  __m128i const2986 = {FIX_0_298631336};
  __m128i const0531 = {FIX_2_053119869};
  __m128i const0727 = {FIX_3_072711026};
  __m128i const5013 = {FIX_1_501321110};

  const5411 = __lsx_vreplvei_w(const5411, 0);
  const7653 = __lsx_vreplvei_w(const7653, 0);
  const8477 = __lsx_vreplvei_w(const8477, 0);
  const1758 = __lsx_vreplvei_w(const1758, 0);
  const8999 = __lsx_vreplvei_w(const8999, 0);
  const5629 = __lsx_vreplvei_w(const5629, 0);
  const9615 = __lsx_vreplvei_w(const9615, 0);
  const3901 = __lsx_vreplvei_w(const3901, 0);
  const2986 = __lsx_vreplvei_w(const2986, 0);
  const0531 = __lsx_vreplvei_w(const0531, 0);
  const0727 = __lsx_vreplvei_w(const0727, 0);
  const5013 = __lsx_vreplvei_w(const5013, 0);

  LSX_LD_8(data, 8, val0, val1, val2, val3, val4, val5, val6, val7);

  /* Pass1 */

  LSX_TRANSPOSE8x8_H(val0, val1, val2, val3, val4, val5, val6, val7,
                     val0, val1, val2, val3, val4, val5, val6, val7);

  LSX_BUTTERFLY_8(v8i16, val0, val1, val2, val3, val4, val5, val6, val7,
                  tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);

  LSX_BUTTERFLY_4(v8i16, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  temp = __lsx_vadd_h(tmp10, tmp11);
  dst0 = __lsx_vslli_h(temp, PASS1_BITS);
  temp = __lsx_vsub_h(tmp10, tmp11);
  dst4 = __lsx_vslli_h(temp, PASS1_BITS);

  LSX_UNPCKLH_W_H_2(tmp12, tmp13, z12_h, z12_l, z13_h, z13_l);
  tmp12 = __lsx_vadd_h(tmp12, tmp13);
  LSX_UNPCKLH_W_H(tmp12, z1_h, z1_l);

  z1_h = __lsx_vmul_w(z1_h, const5411);
  z1_l = __lsx_vmul_w(z1_l, const5411);

  z12_h = __lsx_vmadd_w(z1_h, z12_h, const8477);
  z12_l = __lsx_vmadd_w(z1_l, z12_l, const8477);
  z13_h = __lsx_vmadd_w(z1_h, z13_h, const7653);
  z13_l = __lsx_vmadd_w(z1_l, z13_l, const7653);

  LSX_SRARI_W_4(z12_l, z12_h, z13_l, z13_h, (CONST_BITS - PASS1_BITS));
  LSX_PCKEV_H_2(z12_h, z12_l, z13_h, z13_l, dst6, dst2);

  tmp10 = __lsx_vadd_h(tmp4, tmp7);
  tmp11 = __lsx_vadd_h(tmp5, tmp6);
  tmp12 = __lsx_vadd_h(tmp4, tmp6);
  tmp13 = __lsx_vadd_h(tmp5, tmp7);

  LSX_UNPCKLH_W_H_4(tmp10, tmp11, tmp12, tmp13, z1_h, z1_l, z2_h, z2_l, z3_h,
		    z3_l, z4_h, z4_l);
  LSX_ADD_W_2(z3_h, z4_h, z3_l, z4_l, z5_h, z5_l);

  z5_h = __lsx_vmul_w(z5_h, const1758);
  z5_l = __lsx_vmul_w(z5_l, const1758);

  LSX_UNPCKLH_W_H_4(tmp4, tmp5, tmp6, tmp7, tmp4_h, tmp4_l, tmp5_h, tmp5_l,
		    tmp6_h, tmp6_l, tmp7_h, tmp7_l);

  z1_h = __lsx_vmul_w(z1_h, const8999);
  z1_l = __lsx_vmul_w(z1_l, const8999);
  z2_h = __lsx_vmul_w(z2_h, const5629);
  z2_l = __lsx_vmul_w(z2_l, const5629);
  z3_h = __lsx_vmadd_w(z5_h, z3_h, const9615);
  z3_l = __lsx_vmadd_w(z5_l, z3_l, const9615);
  z4_h = __lsx_vmadd_w(z5_h, z4_h, const3901);
  z4_l = __lsx_vmadd_w(z5_l, z4_l, const3901);

  tmp4_h = __lsx_vmadd_w(z1_h, tmp4_h, const2986);
  tmp5_h = __lsx_vmadd_w(z2_h, tmp5_h, const0531);
  tmp6_h = __lsx_vmadd_w(z2_h, tmp6_h, const0727);
  tmp7_h = __lsx_vmadd_w(z1_h, tmp7_h, const5013);
  tmp4_l = __lsx_vmadd_w(z1_l, tmp4_l, const2986);
  tmp5_l = __lsx_vmadd_w(z2_l, tmp5_l, const0531);
  tmp6_l = __lsx_vmadd_w(z2_l, tmp6_l, const0727);
  tmp7_l = __lsx_vmadd_w(z1_l, tmp7_l, const5013);

  tmp4_h = __lsx_vadd_w(tmp4_h, z3_h);
  tmp5_h = __lsx_vadd_w(tmp5_h, z4_h);
  tmp6_h = __lsx_vadd_w(tmp6_h, z3_h);
  tmp7_h = __lsx_vadd_w(tmp7_h, z4_h);
  tmp4_l = __lsx_vadd_w(tmp4_l, z3_l);
  tmp5_l = __lsx_vadd_w(tmp5_l, z4_l);
  tmp6_l = __lsx_vadd_w(tmp6_l, z3_l);
  tmp7_l = __lsx_vadd_w(tmp7_l, z4_l);

  LSX_SRARI_W_4(tmp4_h, tmp5_h, tmp6_h, tmp7_h, (CONST_BITS - PASS1_BITS));
  LSX_SRARI_W_4(tmp4_l, tmp5_l, tmp6_l, tmp7_l, (CONST_BITS - PASS1_BITS));

  LSX_PCKEV_H_2(tmp4_h, tmp4_l, tmp5_h, tmp5_l, dst7, dst5);
  LSX_PCKEV_H_2(tmp6_h, tmp6_l, tmp7_h, tmp7_l, dst3, dst1);

  /* Pass 2 */

  LSX_TRANSPOSE8x8_H(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                     val0, val1, val2, val3, val4, val5, val6, val7);

  LSX_BUTTERFLY_8(v8i16, val0, val1, val2, val3, val4, val5, val6, val7,
                  tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);

  LSX_BUTTERFLY_4(v8i16, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  dst0 = __lsx_vadd_h(tmp10, tmp11);
  dst4 = __lsx_vsub_h(tmp10, tmp11);
  LSX_SRARI_H_2(dst0, dst4, dst0, dst4, PASS1_BITS);

  LSX_UNPCKLH_W_H_2(tmp12, tmp13, z12_h, z12_l, z13_h, z13_l);

  tmp10 = __lsx_vadd_h(tmp12, tmp13);
  LSX_UNPCKLH_W_H(tmp10, z1_h, z1_l);

  z1_h = __lsx_vmul_w(z1_h, const5411);
  z1_l = __lsx_vmul_w(z1_l, const5411);

  z12_h = __lsx_vmadd_w(z1_h, z12_h, const8477);
  z12_l = __lsx_vmadd_w(z1_l, z12_l, const8477);
  z13_h = __lsx_vmadd_w(z1_h, z13_h, const7653);
  z13_l = __lsx_vmadd_w(z1_l, z13_l, const7653);

  z12_h = __lsx_vsrari_w(z12_h, (CONST_BITS + PASS1_BITS));
  z12_l = __lsx_vsrari_w(z12_l, (CONST_BITS + PASS1_BITS));
  z13_h = __lsx_vsrari_w(z13_h, (CONST_BITS + PASS1_BITS));
  z13_l = __lsx_vsrari_w(z13_l, (CONST_BITS + PASS1_BITS));
  LSX_PCKEV_H_2(z12_h, z12_l, z13_h, z13_l, dst6, dst2);

  LSX_ADD_H_4(tmp4, tmp7, tmp5, tmp6, tmp4, tmp6, tmp5, tmp7,
	      tmp10, tmp11, tmp12, tmp13);

  LSX_UNPCKLH_W_H_4(tmp10, tmp11, tmp12, tmp13, z1_h, z1_l, z2_h, z2_l,
		    z3_h, z3_l, z4_h, z4_l);

  LSX_ADD_W_2(z3_h, z4_h, z3_l, z4_l, z5_h, z5_l);

  z5_h = __lsx_vmul_w(z5_h, const1758);
  z5_l = __lsx_vmul_w(z5_l, const1758);

  LSX_UNPCKLH_W_H_4(tmp4, tmp5, tmp6, tmp7, tmp4_h, tmp4_l, tmp5_h, tmp5_l,
		    tmp6_h, tmp6_l, tmp7_h, tmp7_l);

  z1_h = __lsx_vmul_w(z1_h, const8999);
  z1_l = __lsx_vmul_w(z1_l, const8999);
  z2_h = __lsx_vmul_w(z2_h, const5629);
  z2_l = __lsx_vmul_w(z2_l, const5629);
  z3_h = __lsx_vmadd_w(z5_h, z3_h, const9615);
  z3_l = __lsx_vmadd_w(z5_l, z3_l, const9615);
  z4_h = __lsx_vmadd_w(z5_h, z4_h, const3901);
  z4_l = __lsx_vmadd_w(z5_l, z4_l, const3901);

  tmp4_h = __lsx_vmadd_w(z1_h, tmp4_h, const2986);
  tmp5_h = __lsx_vmadd_w(z2_h, tmp5_h, const0531);
  tmp6_h = __lsx_vmadd_w(z2_h, tmp6_h, const0727);
  tmp7_h = __lsx_vmadd_w(z1_h, tmp7_h, const5013);
  tmp4_l = __lsx_vmadd_w(z1_l, tmp4_l, const2986);
  tmp5_l = __lsx_vmadd_w(z2_l, tmp5_l, const0531);
  tmp6_l = __lsx_vmadd_w(z2_l, tmp6_l, const0727);
  tmp7_l = __lsx_vmadd_w(z1_l, tmp7_l, const5013);

  tmp4_h = __lsx_vadd_w(tmp4_h, z3_h);
  tmp5_h = __lsx_vadd_w(tmp5_h, z4_h);
  tmp6_h = __lsx_vadd_w(tmp6_h, z3_h);
  tmp7_h = __lsx_vadd_w(tmp7_h, z4_h);
  tmp4_l = __lsx_vadd_w(tmp4_l, z3_l);
  tmp5_l = __lsx_vadd_w(tmp5_l, z4_l);
  tmp6_l = __lsx_vadd_w(tmp6_l, z3_l);
  tmp7_l = __lsx_vadd_w(tmp7_l, z4_l);

  LSX_SRARI_W_4(tmp4_h, tmp5_h, tmp6_h, tmp7_h, (CONST_BITS + PASS1_BITS));
  LSX_SRARI_W_4(tmp4_l, tmp5_l, tmp6_l, tmp7_l, (CONST_BITS + PASS1_BITS));

  LSX_PCKEV_H_2(tmp4_h, tmp4_l, tmp5_h, tmp5_l, dst7, dst5);
  LSX_PCKEV_H_2(tmp6_h, tmp6_l, tmp7_h, tmp7_l, dst3, dst1);

  LSX_ST_8(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, data, 8);
}
