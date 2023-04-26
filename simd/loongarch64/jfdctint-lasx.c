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
jsimd_fdct_islow_lasx(DCTELEM *data)
{
  __m256i temp;
  __m256i val0, val1, val2, val3, val4, val5, val6, val7;
  __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  __m256i tmp10, tmp11, tmp12, tmp13;
  __m256i tmp4_l, tmp5_l, tmp6_l, tmp7_l;
  __m256i dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  __m256i z1_l, z2_l, z3_l, z4_l, z5_l, z12_l, z13_l;
  __m256i zero = __lasx_xvldi(0);

  __m256i const5411 = {FIX_0_541196100};
  __m256i const7653 = {FIX_0_765366865};
  __m256i const8477 = {-FIX_1_847759065};
  __m256i const1758 = {FIX_1_175875602};
  __m256i const8999 = {-FIX_0_899976223};
  __m256i const5629 = {-FIX_2_562915447};
  __m256i const9615 = {-FIX_1_961570560};
  __m256i const3901 = {-FIX_0_390180644};
  __m256i const2986 = {FIX_0_298631336};
  __m256i const0531 = {FIX_2_053119869};
  __m256i const0727 = {FIX_3_072711026};
  __m256i const5013 = {FIX_1_501321110};

  const5411 = __lasx_xvreplve0_w(const5411);
  const7653 = __lasx_xvreplve0_w(const7653);
  const8477 = __lasx_xvreplve0_w(const8477);
  const1758 = __lasx_xvreplve0_w(const1758);
  const8999 = __lasx_xvreplve0_w(const8999);
  const5629 = __lasx_xvreplve0_w(const5629);
  const9615 = __lasx_xvreplve0_w(const9615);
  const3901 = __lasx_xvreplve0_w(const3901);
  const2986 = __lasx_xvreplve0_w(const2986);
  const0531 = __lasx_xvreplve0_w(const0531);
  const0727 = __lasx_xvreplve0_w(const0727);
  const5013 = __lasx_xvreplve0_w(const5013);

  LASX_LD_4(data, 16, val0, val2, val4, val6);
  val1 = __lasx_xvpermi_q(zero, val0, 0x31);
  val3 = __lasx_xvpermi_q(zero, val2, 0x31);
  val5 = __lasx_xvpermi_q(zero, val4, 0x31);
  val7 = __lasx_xvpermi_q(zero, val6, 0x31);

  /* Pass1 */

  LASX_TRANSPOSE8x8_H_128SV(val0, val1, val2, val3, val4, val5, val6, val7,
                            val0, val1, val2, val3, val4, val5, val6, val7);

  LASX_BUTTERFLY_8(v16i16, val0, val1, val2, val3, val4, val5, val6, val7,
                   tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);

  LASX_BUTTERFLY_4(v16i16, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  temp = __lasx_xvadd_h(tmp10, tmp11);
  dst0 = __lasx_xvslli_h(temp, PASS1_BITS);
  temp = __lasx_xvsub_h(tmp10, tmp11);
  dst4 = __lasx_xvslli_h(temp, PASS1_BITS);

  LASX_UNPCK_L_W_H_2(tmp12, tmp13, z12_l, z13_l);
  tmp12 = __lasx_xvadd_h(tmp12, tmp13);
  LASX_UNPCK_L_W_H(tmp12, z1_l);

  z1_l = __lasx_xvmul_w(z1_l, const5411);
  z12_l = __lasx_xvmadd_w(z1_l, z12_l, const8477);
  z13_l = __lasx_xvmadd_w(z1_l, z13_l, const7653);

  z12_l = __lasx_xvsrari_w(z12_l, (CONST_BITS - PASS1_BITS));
  z13_l = __lasx_xvsrari_w(z13_l, (CONST_BITS - PASS1_BITS));
  LASX_PCKEV_H_2(zero, z12_l, zero, z13_l, dst6, dst2);

  tmp10 = __lasx_xvadd_h(tmp4, tmp7);
  tmp11 = __lasx_xvadd_h(tmp5, tmp6);
  tmp12 = __lasx_xvadd_h(tmp4, tmp6);
  tmp13 = __lasx_xvadd_h(tmp5, tmp7);

  LASX_UNPCK_L_W_H_4(tmp10, tmp11, tmp12, tmp13, z1_l, z2_l, z3_l, z4_l);

  z5_l = __lasx_xvadd_w(z3_l, z4_l);
  z5_l = __lasx_xvmul_w(z5_l, const1758);

  LASX_UNPCK_L_W_H_4(tmp4, tmp5, tmp6, tmp7, tmp4_l, tmp5_l, tmp6_l, tmp7_l);

  z1_l = __lasx_xvmul_w(z1_l, const8999);
  z2_l = __lasx_xvmul_w(z2_l, const5629);
  z3_l = __lasx_xvmadd_w(z5_l, z3_l, const9615);
  z4_l = __lasx_xvmadd_w(z5_l, z4_l, const3901);

  tmp4_l = __lasx_xvmadd_w(z1_l, tmp4_l, const2986);
  tmp5_l = __lasx_xvmadd_w(z2_l, tmp5_l, const0531);
  tmp6_l = __lasx_xvmadd_w(z2_l, tmp6_l, const0727);
  tmp7_l = __lasx_xvmadd_w(z1_l, tmp7_l, const5013);

  tmp4_l = __lasx_xvadd_w(tmp4_l, z3_l);
  tmp5_l = __lasx_xvadd_w(tmp5_l, z4_l);
  tmp6_l = __lasx_xvadd_w(tmp6_l, z3_l);
  tmp7_l = __lasx_xvadd_w(tmp7_l, z4_l);

  tmp4_l = __lasx_xvsrari_w(tmp4_l, (CONST_BITS - PASS1_BITS));
  tmp5_l = __lasx_xvsrari_w(tmp5_l, (CONST_BITS - PASS1_BITS));
  tmp6_l = __lasx_xvsrari_w(tmp6_l, (CONST_BITS - PASS1_BITS));
  tmp7_l = __lasx_xvsrari_w(tmp7_l, (CONST_BITS - PASS1_BITS));

  LASX_PCKEV_H_2(zero, tmp4_l, zero, tmp5_l, dst7, dst5);
  LASX_PCKEV_H_2(zero, tmp6_l, zero, tmp7_l, dst3, dst1);

  /* Pass 2 */

  LASX_TRANSPOSE8x8_H_128SV(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                            val0, val1, val2, val3, val4, val5, val6, val7);

  LASX_BUTTERFLY_8(v16i16, val0, val1, val2, val3, val4, val5, val6, val7,
                   tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);

  LASX_BUTTERFLY_4(v16i16, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  dst0 = __lasx_xvadd_h(tmp10, tmp11);
  dst4 = __lasx_xvsub_h(tmp10, tmp11);
  LASX_SRARI_H_2(dst0, dst4, dst0, dst4, PASS1_BITS);

  LASX_UNPCK_L_W_H_2(tmp12, tmp13, z12_l, z13_l);
  tmp12 = __lasx_xvadd_h(tmp12, tmp13);
  LASX_UNPCK_L_W_H(tmp12, z1_l);

  z1_l = __lasx_xvmul_w(z1_l, const5411);
  z12_l = __lasx_xvmadd_w(z1_l, z12_l, const8477);
  z13_l = __lasx_xvmadd_w(z1_l, z13_l, const7653);

  z12_l = __lasx_xvsrari_w(z12_l, (CONST_BITS + PASS1_BITS));
  z13_l = __lasx_xvsrari_w(z13_l, (CONST_BITS + PASS1_BITS));
  LASX_PCKEV_H_2(zero, z12_l, zero, z13_l, dst6, dst2);

  tmp10 = __lasx_xvadd_h(tmp4, tmp7);
  tmp11 = __lasx_xvadd_h(tmp5, tmp6);
  tmp12 = __lasx_xvadd_h(tmp4, tmp6);
  tmp13 = __lasx_xvadd_h(tmp5, tmp7);

  LASX_UNPCK_L_W_H_4(tmp10, tmp11, tmp12, tmp13, z1_l, z2_l, z3_l, z4_l);

  z5_l = __lasx_xvadd_w(z3_l, z4_l);
  z5_l = __lasx_xvmul_w(z5_l, const1758);

  LASX_UNPCK_L_W_H_4(tmp4, tmp5, tmp6, tmp7, tmp4_l, tmp5_l, tmp6_l, tmp7_l);

  z1_l = __lasx_xvmul_w(z1_l, const8999);
  z2_l = __lasx_xvmul_w(z2_l, const5629);
  z3_l = __lasx_xvmadd_w(z5_l, z3_l, const9615);
  z4_l = __lasx_xvmadd_w(z5_l, z4_l, const3901);

  tmp4_l = __lasx_xvmadd_w(z1_l, tmp4_l, const2986);
  tmp5_l = __lasx_xvmadd_w(z2_l, tmp5_l, const0531);
  tmp6_l = __lasx_xvmadd_w(z2_l, tmp6_l, const0727);
  tmp7_l = __lasx_xvmadd_w(z1_l, tmp7_l, const5013);

  tmp4_l = __lasx_xvadd_w(tmp4_l, z3_l);
  tmp5_l = __lasx_xvadd_w(tmp5_l, z4_l);
  tmp6_l = __lasx_xvadd_w(tmp6_l, z3_l);
  tmp7_l = __lasx_xvadd_w(tmp7_l, z4_l);

  tmp4_l = __lasx_xvsrari_w(tmp4_l, (CONST_BITS + PASS1_BITS));
  tmp5_l = __lasx_xvsrari_w(tmp5_l, (CONST_BITS + PASS1_BITS));
  tmp6_l = __lasx_xvsrari_w(tmp6_l, (CONST_BITS + PASS1_BITS));
  tmp7_l = __lasx_xvsrari_w(tmp7_l, (CONST_BITS + PASS1_BITS));

  LASX_PCKEV_H_2(zero, tmp4_l, zero, tmp5_l, dst7, dst5);
  LASX_PCKEV_H_2(zero, tmp6_l, zero, tmp7_l, dst3, dst1);

  dst0 = __lasx_xvpermi_q(dst1, dst0, 0x20);
  dst2 = __lasx_xvpermi_q(dst3, dst2, 0x20);
  dst4 = __lasx_xvpermi_q(dst5, dst4, 0x20);
  dst6 = __lasx_xvpermi_q(dst7, dst6, 0x20);

  LASX_ST_4(dst0, dst2, dst4, dst6, data, 16);
}
