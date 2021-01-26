/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014, 2020, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2021, Ilya Kurdyukov <jpegqs@gmail.com> for BaseALT, Ltd
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

/* ACCURATE INTEGER FORWARD DCT */

#include "jsimd_e2k.h"


#define F_0_298  2446   /* FIX(0.298631336) */
#define F_0_390  3196   /* FIX(0.390180644) */
#define F_0_541  4433   /* FIX(0.541196100) */
#define F_0_765  6270   /* FIX(0.765366865) */
#define F_0_899  7373   /* FIX(0.899976223) */
#define F_1_175  9633   /* FIX(1.175875602) */
#define F_1_501  12299  /* FIX(1.501321110) */
#define F_1_847  15137  /* FIX(1.847759065) */
#define F_1_961  16069  /* FIX(1.961570560) */
#define F_2_053  16819  /* FIX(2.053119869) */
#define F_2_562  20995  /* FIX(2.562915447) */
#define F_3_072  25172  /* FIX(3.072711026) */

#define CONST_BITS  13
#define PASS1_BITS  2
#define DESCALE_P1  (CONST_BITS - PASS1_BITS)
#define DESCALE_P2  (CONST_BITS + PASS1_BITS)


#define DO_FDCT_COMMON(PASS) { \
  /* (Original) \
   * z1 = (tmp12 + tmp13) * 0.541196100; \
   * data2 = z1 + tmp13 * 0.765366865; \
   * data6 = z1 + tmp12 * -1.847759065; \
   * \
   * (This implementation) \
   * data2 = tmp13 * (0.541196100 + 0.765366865) + tmp12 * 0.541196100; \
   * data6 = tmp13 * 0.541196100 + tmp12 * (0.541196100 - 1.847759065); \
   */ \
  \
  tmp1312l = _mm_unpacklo_epi16(tmp13, tmp12); \
  tmp1312h = _mm_unpackhi_epi16(tmp13, tmp12); \
  \
  out2l = _mm_add_epi32(_mm_madd_epi16(tmp1312l, pw_f130_f054), pd_descale_p##PASS); \
  out2h = _mm_add_epi32(_mm_madd_epi16(tmp1312h, pw_f130_f054), pd_descale_p##PASS); \
  out6l = _mm_add_epi32(_mm_madd_epi16(tmp1312l, pw_f054_mf130), pd_descale_p##PASS); \
  out6h = _mm_add_epi32(_mm_madd_epi16(tmp1312h, pw_f054_mf130), pd_descale_p##PASS); \
  \
  out2l = _mm_srai_epi32(out2l, DESCALE_P##PASS); \
  out2h = _mm_srai_epi32(out2h, DESCALE_P##PASS); \
  out6l = _mm_srai_epi32(out6l, DESCALE_P##PASS); \
  out6h = _mm_srai_epi32(out6h, DESCALE_P##PASS); \
  \
  out2 = _mm_packs_epi32(out2l, out2h); \
  out6 = _mm_packs_epi32(out6l, out6h); \
  \
  /* Odd part */ \
  \
  z3 = _mm_add_epi16(tmp4, tmp6); \
  z4 = _mm_add_epi16(tmp5, tmp7); \
  \
  /* (Original) \
   * z5 = (z3 + z4) * 1.175875602; \
   * z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644; \
   * z3 += z5;  z4 += z5; \
   * \
   * (This implementation) \
   * z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602; \
   * z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644); \
   */ \
  \
  z34l = _mm_unpacklo_epi16(z3, z4); \
  z34h = _mm_unpackhi_epi16(z3, z4); \
  \
  z3l = _mm_add_epi32(_mm_madd_epi16(z34l, pw_mf078_f117), pd_descale_p##PASS); \
  z3h = _mm_add_epi32(_mm_madd_epi16(z34h, pw_mf078_f117), pd_descale_p##PASS); \
  z4l = _mm_add_epi32(_mm_madd_epi16(z34l, pw_f117_f078), pd_descale_p##PASS); \
  z4h = _mm_add_epi32(_mm_madd_epi16(z34h, pw_f117_f078), pd_descale_p##PASS); \
  \
  /* (Original) \
   * z1 = tmp4 + tmp7;  z2 = tmp5 + tmp6; \
   * tmp4 = tmp4 * 0.298631336;  tmp5 = tmp5 * 2.053119869; \
   * tmp6 = tmp6 * 3.072711026;  tmp7 = tmp7 * 1.501321110; \
   * z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447; \
   * data7 = tmp4 + z1 + z3;  data5 = tmp5 + z2 + z4; \
   * data3 = tmp6 + z2 + z3;  data1 = tmp7 + z1 + z4; \
   * \
   * (This implementation) \
   * tmp4 = tmp4 * (0.298631336 - 0.899976223) + tmp7 * -0.899976223; \
   * tmp5 = tmp5 * (2.053119869 - 2.562915447) + tmp6 * -2.562915447; \
   * tmp6 = tmp5 * -2.562915447 + tmp6 * (3.072711026 - 2.562915447); \
   * tmp7 = tmp4 * -0.899976223 + tmp7 * (1.501321110 - 0.899976223); \
   * data7 = tmp4 + z3;  data5 = tmp5 + z4; \
   * data3 = tmp6 + z3;  data1 = tmp7 + z4; \
   */ \
  \
  tmp47l = _mm_unpacklo_epi16(tmp4, tmp7); \
  tmp47h = _mm_unpackhi_epi16(tmp4, tmp7); \
  \
  out7l = _mm_add_epi32(_mm_madd_epi16(tmp47l, pw_mf060_mf089), z3l); \
  out7h = _mm_add_epi32(_mm_madd_epi16(tmp47h, pw_mf060_mf089), z3h); \
  out1l = _mm_add_epi32(_mm_madd_epi16(tmp47l, pw_mf089_f060), z4l); \
  out1h = _mm_add_epi32(_mm_madd_epi16(tmp47h, pw_mf089_f060), z4h); \
  \
  out7l = _mm_srai_epi32(out7l, DESCALE_P##PASS); \
  out7h = _mm_srai_epi32(out7h, DESCALE_P##PASS); \
  out1l = _mm_srai_epi32(out1l, DESCALE_P##PASS); \
  out1h = _mm_srai_epi32(out1h, DESCALE_P##PASS); \
  \
  out7 = _mm_packs_epi32(out7l, out7h); \
  out1 = _mm_packs_epi32(out1l, out1h); \
  \
  tmp56l = _mm_unpacklo_epi16(tmp5, tmp6); \
  tmp56h = _mm_unpackhi_epi16(tmp5, tmp6); \
  \
  out5l = _mm_add_epi32(_mm_madd_epi16(tmp56l, pw_mf050_mf256), z4l); \
  out5h = _mm_add_epi32(_mm_madd_epi16(tmp56h, pw_mf050_mf256), z4h); \
  out3l = _mm_add_epi32(_mm_madd_epi16(tmp56l, pw_mf256_f050), z3l); \
  out3h = _mm_add_epi32(_mm_madd_epi16(tmp56h, pw_mf256_f050), z3h); \
  \
  out5l = _mm_srai_epi32(out5l, DESCALE_P##PASS); \
  out5h = _mm_srai_epi32(out5h, DESCALE_P##PASS); \
  out3l = _mm_srai_epi32(out3l, DESCALE_P##PASS); \
  out3h = _mm_srai_epi32(out3h, DESCALE_P##PASS); \
  \
  out5 = _mm_packs_epi32(out5l, out5h); \
  out3 = _mm_packs_epi32(out3l, out3h); \
}

#define DO_FDCT_PASS1() { \
  /* Even part */ \
  \
  tmp10 = _mm_add_epi16(tmp0, tmp3); \
  tmp13 = _mm_sub_epi16(tmp0, tmp3); \
  tmp11 = _mm_add_epi16(tmp1, tmp2); \
  tmp12 = _mm_sub_epi16(tmp1, tmp2); \
  \
  out0  = _mm_add_epi16(tmp10, tmp11); \
  out0  = _mm_slli_epi16(out0, PASS1_BITS); \
  out4  = _mm_sub_epi16(tmp10, tmp11); \
  out4  = _mm_slli_epi16(out4, PASS1_BITS); \
  \
  DO_FDCT_COMMON(1); \
}

#define DO_FDCT_PASS2() { \
  /* Even part */ \
  \
  tmp10 = _mm_add_epi16(tmp0, tmp3); \
  tmp13 = _mm_sub_epi16(tmp0, tmp3); \
  tmp11 = _mm_add_epi16(tmp1, tmp2); \
  tmp12 = _mm_sub_epi16(tmp1, tmp2); \
  \
  out0  = _mm_add_epi16(tmp10, tmp11); \
  out0  = _mm_add_epi16(out0, pw_descale_p2x); \
  out0  = _mm_srai_epi16(out0, PASS1_BITS); \
  out4  = _mm_sub_epi16(tmp10, tmp11); \
  out4  = _mm_add_epi16(out4, pw_descale_p2x); \
  out4  = _mm_srai_epi16(out4, PASS1_BITS); \
  \
  DO_FDCT_COMMON(2); \
}


void jsimd_fdct_islow_e2k(DCTELEM *data)
{
  __m128i row0, row1, row2, row3, row4, row5, row6, row7,
    col0, col1, col2, col3, col4, col5, col6, col7,
    tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13,
    tmp47l, tmp47h, tmp56l, tmp56h, tmp1312l, tmp1312h,
    z3, z4, z34l, z34h,
    out0, out1, out2, out3, out4, out5, out6, out7;
  __m128i z3l, z3h, z4l, z4h,
    out1l, out1h, out2l, out2h, out3l, out3h, out5l, out5h, out6l, out6h,
    out7l, out7h;

  /* Constants */
  __m128i pw_f130_f054 = _mm_setr_epi16(__4X2(F_0_541 + F_0_765, F_0_541)),
    pw_f054_mf130 = _mm_setr_epi16(__4X2(F_0_541, F_0_541 - F_1_847)),
    pw_mf078_f117 = _mm_setr_epi16(__4X2(F_1_175 - F_1_961, F_1_175)),
    pw_f117_f078 = _mm_setr_epi16(__4X2(F_1_175, F_1_175 - F_0_390)),
    pw_mf060_mf089 = _mm_setr_epi16(__4X2(F_0_298 - F_0_899, -F_0_899)),
    pw_mf089_f060 = _mm_setr_epi16(__4X2(-F_0_899, F_1_501 - F_0_899)),
    pw_mf050_mf256 = _mm_setr_epi16(__4X2(F_2_053 - F_2_562, -F_2_562)),
    pw_mf256_f050 = _mm_setr_epi16(__4X2(-F_2_562, F_3_072 - F_2_562)),
    pw_descale_p2x = _mm_set1_epi16(1 << (PASS1_BITS - 1)),
    pd_descale_p1 = _mm_set1_epi32(1 << (DESCALE_P1 - 1)),
    pd_descale_p2 = _mm_set1_epi32(1 << (DESCALE_P2 - 1));

  /* Pass 1: process rows */

  row0 = VEC_LD(data + 0 * 8);
  row1 = VEC_LD(data + 1 * 8);
  row2 = VEC_LD(data + 2 * 8);
  row3 = VEC_LD(data + 3 * 8);
  row4 = VEC_LD(data + 4 * 8);
  row5 = VEC_LD(data + 5 * 8);
  row6 = VEC_LD(data + 6 * 8);
  row7 = VEC_LD(data + 7 * 8);

  TRANSPOSE(row, col);

  tmp0 = _mm_add_epi16(col0, col7);
  tmp7 = _mm_sub_epi16(col0, col7);
  tmp1 = _mm_add_epi16(col1, col6);
  tmp6 = _mm_sub_epi16(col1, col6);
  tmp2 = _mm_add_epi16(col2, col5);
  tmp5 = _mm_sub_epi16(col2, col5);
  tmp3 = _mm_add_epi16(col3, col4);
  tmp4 = _mm_sub_epi16(col3, col4);

  DO_FDCT_PASS1();

  /* Pass 2: process columns */

  TRANSPOSE(out, row);

  tmp0 = _mm_add_epi16(row0, row7);
  tmp7 = _mm_sub_epi16(row0, row7);
  tmp1 = _mm_add_epi16(row1, row6);
  tmp6 = _mm_sub_epi16(row1, row6);
  tmp2 = _mm_add_epi16(row2, row5);
  tmp5 = _mm_sub_epi16(row2, row5);
  tmp3 = _mm_add_epi16(row3, row4);
  tmp4 = _mm_sub_epi16(row3, row4);

  DO_FDCT_PASS2();

  VEC_ST(data + 0 * 8, out0);
  VEC_ST(data + 1 * 8, out1);
  VEC_ST(data + 2 * 8, out2);
  VEC_ST(data + 3 * 8, out3);
  VEC_ST(data + 4 * 8, out4);
  VEC_ST(data + 5 * 8, out5);
  VEC_ST(data + 6 * 8, out6);
  VEC_ST(data + 7 * 8, out7);
}
