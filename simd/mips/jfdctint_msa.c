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

/* Local macros */
#define FDCT_ISLOW_ODD_MULTIPLY(in0, in1, in2, in3, in4, in5, in6, in7, in8, \
in9, in10, in11, in12, in13, in14, in15, const0, const1) \
  in0 = in0 * __msa_splati_w(const0, 0); \
  in1 = in1 * __msa_splati_w(const0, 0); \
  in2 = in2 * __msa_splati_w(const0, 1); \
  in3 = in3 * __msa_splati_w(const0, 1); \
  in4 = in4 * __msa_splati_w(const0, 2); \
  in5 = in5 * __msa_splati_w(const0, 2); \
  in6 = in6 * __msa_splati_w(const0, 3); \
  in7 = in7 * __msa_splati_w(const0, 3); \
  in8 = in8 * __msa_splati_w(const1, 0); \
  in9 = in9 * __msa_splati_w(const1, 0); \
  in10 = in10 * __msa_splati_w(const1, 1); \
  in11 = in11 * __msa_splati_w(const1, 1); \
  in12 = in12 * __msa_splati_w(const1, 2); \
  in13 = in13 * __msa_splati_w(const1, 2); \
  in14 = in14 * __msa_splati_w(const1, 3); \
  in15 = in15 * __msa_splati_w(const1, 3);

GLOBAL(void)
jsimd_fdct_islow_msa(DCTELEM *data)
{
  v8i16 val0, val1, val2, val3, val4, val5, val6, val7;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  v4i32 tmp4_r, tmp5_r, tmp6_r, tmp7_r, tmp4_l, tmp5_l, tmp6_l, tmp7_l;
  v8i16 tmp10, tmp11, tmp12, tmp13;
  v8i16 dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  v4i32 z1_r, z1_l, z2_r, z2_l, z3_r, z3_l, z4_r, z4_l;
  v4i32 z5_r, z5_l, z12_r, z12_l, z13_r, z13_l;
  v4i32 const0 =
    { FIX_0_298631336, FIX_2_053119869, FIX_3_072711026, FIX_1_501321110 };
  v4i32 const1 =
    { -FIX_0_899976223, -FIX_2_562915447, -FIX_1_961570560, -FIX_0_390180644 };
  v4i32 const2 =
    { FIX_0_541196100, -FIX_1_847759065, FIX_0_765366865, FIX_1_175875602 };

  /* Load 8 rows */
  LD_SH8(data, DCTSIZE, val0, val1, val2, val3, val4, val5, val6, val7);

  /* Pass1 */
  /* Transpose */
  TRANSPOSE8x8_SH_SH(val0, val1, val2, val3, val4, val5, val6, val7,
                     val0, val1, val2, val3, val4, val5, val6, val7);

  BUTTERFLY_8(val0, val1, val2, val3, val4, val5, val6, val7,
              tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);

  BUTTERFLY_4(tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  dst0 = MSA_SLLI_H((tmp10 + tmp11), PASS1_BITS);
  dst4 = MSA_SLLI_H((tmp10 - tmp11), PASS1_BITS);

  UNPCK_SH_SW(tmp12, z12_r, z12_l);
  UNPCK_SH_SW(tmp13, z13_r, z13_l);

  tmp12 = tmp12 + tmp13;
  UNPCK_SH_SW(tmp12, z1_r, z1_l);

  z1_r = z1_r * __msa_splati_w(const2, 0);
  z1_l = z1_l * __msa_splati_w(const2, 0);

  z12_r = z1_r + z12_r * __msa_splati_w(const2, 1);
  z12_l = z1_l + z12_l * __msa_splati_w(const2, 1);
  z13_r = z1_r + z13_r * __msa_splati_w(const2, 2);
  z13_l = z1_l + z13_l * __msa_splati_w(const2, 2);

  /* Descale */
  SRARI_W4_SW(z12_l, z12_r, z13_l, z13_r, (CONST_BITS - PASS1_BITS));
  PCKEV_H2_SH(z12_l, z12_r, z13_l, z13_r, dst6, dst2);

  ADD4(tmp4, tmp7, tmp5, tmp6, tmp4, tmp6, tmp5, tmp7,
       tmp10, tmp11, tmp12, tmp13);

  UNPCK_SH4_SW(tmp10, tmp11, tmp12, tmp13, z1_r, z1_l, z2_r, z2_l, z3_r, z3_l,
               z4_r, z4_l);
  ADD2(z3_r, z4_r, z3_l, z4_l, z5_r, z5_l);

  z5_r = z5_r * __msa_splati_w(const2, 3);
  z5_l = z5_l * __msa_splati_w(const2, 3);

  UNPCK_SH4_SW(tmp4, tmp5, tmp6, tmp7, tmp4_r, tmp4_l, tmp5_r, tmp5_l,
               tmp6_r, tmp6_l, tmp7_r, tmp7_l);
  FDCT_ISLOW_ODD_MULTIPLY(tmp4_r, tmp4_l, tmp5_r, tmp5_l, tmp6_r, tmp6_l,
                          tmp7_r, tmp7_l, z1_r, z1_l, z2_r, z2_l, z3_r, z3_l,
                          z4_r, z4_l, const0, const1);

  ADD4(z3_r, z5_r, z3_l, z5_l, z4_r, z5_r, z4_l, z5_l,
       z3_r, z3_l, z4_r, z4_l);

  tmp4_r += z1_r + z3_r;
  tmp5_r += z2_r + z4_r;
  tmp6_r += z2_r + z3_r;
  tmp7_r += z1_r + z4_r;
  tmp4_l += z1_l + z3_l;
  tmp5_l += z2_l + z4_l;
  tmp6_l += z2_l + z3_l;
  tmp7_l += z1_l + z4_l;

  /* Descale */
  SRARI_W4_SW(tmp4_r, tmp5_r, tmp6_r, tmp7_r, (CONST_BITS - PASS1_BITS));
  SRARI_W4_SW(tmp4_l, tmp5_l, tmp6_l, tmp7_l, (CONST_BITS - PASS1_BITS));
  PCKEV_H2_SH(tmp4_l, tmp4_r, tmp5_l, tmp5_r, dst7, dst5);
  PCKEV_H2_SH(tmp6_l, tmp6_r, tmp7_l, tmp7_r, dst3, dst1);

  /* Pass 2 */

  /* Transpose */
  TRANSPOSE8x8_SH_SH(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                     val0, val1, val2, val3, val4, val5, val6, val7);

  BUTTERFLY_8(val0, val1, val2, val3, val4, val5, val6, val7,
              tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);

  BUTTERFLY_4(tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  dst0 = tmp10 + tmp11;
  dst4 = tmp10 - tmp11;
  SRARI_H2_SH(dst0, dst4, PASS1_BITS);

  tmp10 = tmp12 + tmp13;
  UNPCK_SH_SW(tmp10, z1_r, z1_l);

  z1_r = z1_r * __msa_splati_w(const2, 0);
  z1_l = z1_l * __msa_splati_w(const2, 0);

  UNPCK_SH_SW(tmp12, z12_r, z12_l);
  UNPCK_SH_SW(tmp13, z13_r, z13_l);

  z12_r = z1_r + z12_r * __msa_splati_w(const2, 1);
  z12_l = z1_l + z12_l * __msa_splati_w(const2, 1);
  z13_r = z1_r + z13_r * __msa_splati_w(const2, 2);
  z13_l = z1_l + z13_l * __msa_splati_w(const2, 2);

  /* Descale */
  SRARI_W4_SW(z12_l, z12_r, z13_l, z13_r, (CONST_BITS + PASS1_BITS));
  PCKEV_H2_SH(z12_l, z12_r, z13_l, z13_r, dst6, dst2);

  ADD4(tmp4, tmp7, tmp5, tmp6, tmp4, tmp6, tmp5, tmp7,
       tmp10, tmp11, tmp12, tmp13);

  UNPCK_SH4_SW(tmp10, tmp11, tmp12, tmp13, z1_r, z1_l, z2_r, z2_l, z3_r, z3_l,
               z4_r, z4_l);
  ADD2(z3_r, z4_r, z3_l, z4_l, z5_r, z5_l);

  z5_r = z5_r * __msa_splati_w(const2, 3);
  z5_l = z5_l * __msa_splati_w(const2, 3);

  UNPCK_SH4_SW(tmp4, tmp5, tmp6, tmp7, tmp4_r, tmp4_l, tmp5_r, tmp5_l,
               tmp6_r, tmp6_l, tmp7_r, tmp7_l);
  FDCT_ISLOW_ODD_MULTIPLY(tmp4_r, tmp4_l, tmp5_r, tmp5_l, tmp6_r, tmp6_l,
                          tmp7_r, tmp7_l, z1_r, z1_l, z2_r, z2_l, z3_r, z3_l,
                          z4_r, z4_l, const0, const1);

  ADD4(z3_r, z5_r, z3_l, z5_l, z4_r, z5_r, z4_l, z5_l,
       z3_r, z3_l, z4_r, z4_l);

  tmp4_r += z1_r + z3_r;
  tmp5_r += z2_r + z4_r;
  tmp6_r += z2_r + z3_r;
  tmp7_r += z1_r + z4_r;
  tmp4_l += z1_l + z3_l;
  tmp5_l += z2_l + z4_l;
  tmp6_l += z2_l + z3_l;
  tmp7_l += z1_l + z4_l;

  /* Descale */
  SRARI_W4_SW(tmp4_r, tmp5_r, tmp6_r, tmp7_r, (CONST_BITS + PASS1_BITS));
  SRARI_W4_SW(tmp4_l, tmp5_l, tmp6_l, tmp7_l, (CONST_BITS + PASS1_BITS));
  PCKEV_H2_SH(tmp4_l, tmp4_r, tmp5_l, tmp5_r, dst7, dst5);
  PCKEV_H2_SH(tmp6_l, tmp6_r, tmp7_l, tmp7_r, dst3, dst1);

  ST_SH8(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, data, 8);
}
