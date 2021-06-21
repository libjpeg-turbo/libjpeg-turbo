/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014-2015, 2020, D. R. Commander.  All Rights Reserved.
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

/* ACCURATE INTEGER INVERSE DCT */

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
#define DESCALE_P2  (CONST_BITS + PASS1_BITS + 3)


#define DO_IDCT(in, PASS) { \
  /* Even part \
   * \
   * (Original) \
   * z1 = (z2 + z3) * 0.541196100; \
   * tmp2 = z1 + z3 * -1.847759065; \
   * tmp3 = z1 + z2 * 0.765366865; \
   * \
   * (This implementation) \
   * tmp2 = z2 * 0.541196100 + z3 * (0.541196100 - 1.847759065); \
   * tmp3 = z2 * (0.541196100 + 0.765366865) + z3 * 0.541196100; \
   */ \
  \
  in##26l = _mm_unpacklo_epi16(in##2, in##6); \
  in##26h = _mm_unpackhi_epi16(in##2, in##6); \
  \
  tmp3l = _mm_madd_epi16(in##26l, pw_f130_f054); \
  tmp3h = _mm_madd_epi16(in##26h, pw_f130_f054); \
  tmp2l = _mm_madd_epi16(in##26l, pw_f054_mf130); \
  tmp2h = _mm_madd_epi16(in##26h, pw_f054_mf130); \
  \
  tmp0 = _mm_add_epi16(in##0, in##4); \
  tmp1 = _mm_sub_epi16(in##0, in##4); \
  \
  tmp0l = _mm_unpacklo_epi16(pw_zero, tmp0); \
  tmp0h = _mm_unpackhi_epi16(pw_zero, tmp0); \
  tmp0l = _mm_srai_epi32(tmp0l, 16 - CONST_BITS); \
  tmp0h = _mm_srai_epi32(tmp0h, 16 - CONST_BITS); \
  tmp0l = _mm_add_epi32(tmp0l, pd_descale_p##PASS); \
  tmp0h = _mm_add_epi32(tmp0h, pd_descale_p##PASS); \
  \
  tmp10l = _mm_add_epi32(tmp0l, tmp3l); \
  tmp10h = _mm_add_epi32(tmp0h, tmp3h); \
  tmp13l = _mm_sub_epi32(tmp0l, tmp3l); \
  tmp13h = _mm_sub_epi32(tmp0h, tmp3h); \
  \
  tmp1l = _mm_unpacklo_epi16(pw_zero, tmp1); \
  tmp1h = _mm_unpackhi_epi16(pw_zero, tmp1); \
  tmp1l = _mm_srai_epi32(tmp1l, 16 - CONST_BITS); \
  tmp1h = _mm_srai_epi32(tmp1h, 16 - CONST_BITS); \
  tmp1l = _mm_add_epi32(tmp1l, pd_descale_p##PASS); \
  tmp1h = _mm_add_epi32(tmp1h, pd_descale_p##PASS); \
  \
  tmp11l = _mm_add_epi32(tmp1l, tmp2l); \
  tmp11h = _mm_add_epi32(tmp1h, tmp2h); \
  tmp12l = _mm_sub_epi32(tmp1l, tmp2l); \
  tmp12h = _mm_sub_epi32(tmp1h, tmp2h); \
  \
  /* Odd part */ \
  \
  z3 = _mm_add_epi16(in##3, in##7); \
  z4 = _mm_add_epi16(in##1, in##5); \
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
  z3l = _mm_madd_epi16(z34l, pw_mf078_f117); \
  z3h = _mm_madd_epi16(z34h, pw_mf078_f117); \
  z4l = _mm_madd_epi16(z34l, pw_f117_f078); \
  z4h = _mm_madd_epi16(z34h, pw_f117_f078); \
  \
  /* (Original) \
   * z1 = tmp0 + tmp3;  z2 = tmp1 + tmp2; \
   * tmp0 = tmp0 * 0.298631336;  tmp1 = tmp1 * 2.053119869; \
   * tmp2 = tmp2 * 3.072711026;  tmp3 = tmp3 * 1.501321110; \
   * z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447; \
   * tmp0 += z1 + z3;  tmp1 += z2 + z4; \
   * tmp2 += z2 + z3;  tmp3 += z1 + z4; \
   * \
   * (This implementation) \
   * tmp0 = tmp0 * (0.298631336 - 0.899976223) + tmp3 * -0.899976223; \
   * tmp1 = tmp1 * (2.053119869 - 2.562915447) + tmp2 * -2.562915447; \
   * tmp2 = tmp1 * -2.562915447 + tmp2 * (3.072711026 - 2.562915447); \
   * tmp3 = tmp0 * -0.899976223 + tmp3 * (1.501321110 - 0.899976223); \
   * tmp0 += z3;  tmp1 += z4; \
   * tmp2 += z3;  tmp3 += z4; \
   */ \
  \
  in##71l = _mm_unpacklo_epi16(in##7, in##1); \
  in##71h = _mm_unpackhi_epi16(in##7, in##1); \
  \
  tmp0l = _mm_add_epi32(_mm_madd_epi16(in##71l, pw_mf060_mf089), z3l); \
  tmp0h = _mm_add_epi32(_mm_madd_epi16(in##71h, pw_mf060_mf089), z3h); \
  tmp3l = _mm_add_epi32(_mm_madd_epi16(in##71l, pw_mf089_f060), z4l); \
  tmp3h = _mm_add_epi32(_mm_madd_epi16(in##71h, pw_mf089_f060), z4h); \
  \
  in##53l = _mm_unpacklo_epi16(in##5, in##3); \
  in##53h = _mm_unpackhi_epi16(in##5, in##3); \
  \
  tmp1l = _mm_add_epi32(_mm_madd_epi16(in##53l, pw_mf050_mf256), z4l); \
  tmp1h = _mm_add_epi32(_mm_madd_epi16(in##53h, pw_mf050_mf256), z4h); \
  tmp2l = _mm_add_epi32(_mm_madd_epi16(in##53l, pw_mf256_f050), z3l); \
  tmp2h = _mm_add_epi32(_mm_madd_epi16(in##53h, pw_mf256_f050), z3h); \
  \
  /* Final output stage */ \
  \
  out0l = _mm_add_epi32(tmp10l, tmp3l); \
  out0h = _mm_add_epi32(tmp10h, tmp3h); \
  out7l = _mm_sub_epi32(tmp10l, tmp3l); \
  out7h = _mm_sub_epi32(tmp10h, tmp3h); \
  \
  out0l = _mm_srai_epi32(out0l, DESCALE_P##PASS); \
  out0h = _mm_srai_epi32(out0h, DESCALE_P##PASS); \
  out7l = _mm_srai_epi32(out7l, DESCALE_P##PASS); \
  out7h = _mm_srai_epi32(out7h, DESCALE_P##PASS); \
  \
  out0 = _mm_packs_epi32(out0l, out0h); \
  out7 = _mm_packs_epi32(out7l, out7h); \
  \
  out1l = _mm_add_epi32(tmp11l, tmp2l); \
  out1h = _mm_add_epi32(tmp11h, tmp2h); \
  out6l = _mm_sub_epi32(tmp11l, tmp2l); \
  out6h = _mm_sub_epi32(tmp11h, tmp2h); \
  \
  out1l = _mm_srai_epi32(out1l, DESCALE_P##PASS); \
  out1h = _mm_srai_epi32(out1h, DESCALE_P##PASS); \
  out6l = _mm_srai_epi32(out6l, DESCALE_P##PASS); \
  out6h = _mm_srai_epi32(out6h, DESCALE_P##PASS); \
  \
  out1 = _mm_packs_epi32(out1l, out1h); \
  out6 = _mm_packs_epi32(out6l, out6h); \
  \
  out2l = _mm_add_epi32(tmp12l, tmp1l); \
  out2h = _mm_add_epi32(tmp12h, tmp1h); \
  out5l = _mm_sub_epi32(tmp12l, tmp1l); \
  out5h = _mm_sub_epi32(tmp12h, tmp1h); \
  \
  out2l = _mm_srai_epi32(out2l, DESCALE_P##PASS); \
  out2h = _mm_srai_epi32(out2h, DESCALE_P##PASS); \
  out5l = _mm_srai_epi32(out5l, DESCALE_P##PASS); \
  out5h = _mm_srai_epi32(out5h, DESCALE_P##PASS); \
  \
  out2 = _mm_packs_epi32(out2l, out2h); \
  out5 = _mm_packs_epi32(out5l, out5h); \
  \
  out3l = _mm_add_epi32(tmp13l, tmp0l); \
  out3h = _mm_add_epi32(tmp13h, tmp0h); \
  out4l = _mm_sub_epi32(tmp13l, tmp0l); \
  out4h = _mm_sub_epi32(tmp13h, tmp0h); \
  \
  out3l = _mm_srai_epi32(out3l, DESCALE_P##PASS); \
  out3h = _mm_srai_epi32(out3h, DESCALE_P##PASS); \
  out4l = _mm_srai_epi32(out4l, DESCALE_P##PASS); \
  out4h = _mm_srai_epi32(out4h, DESCALE_P##PASS); \
  \
  out3 = _mm_packs_epi32(out3l, out3h); \
  out4 = _mm_packs_epi32(out4l, out4h); \
}


void jsimd_idct_islow_e2k(void *dct_table_, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
  short *dct_table = (short *)dct_table_;

  __m128i row0, row1, row2, row3, row4, row5, row6, row7,
    col0, col1, col2, col3, col4, col5, col6, col7,
    quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7,
    tmp0, tmp1, tmp2, tmp3, z3, z4,
    z34l, z34h, col71l, col71h, col26l, col26h, col53l, col53h,
    row71l, row71h, row26l, row26h, row53l, row53h,
    out0, out1, out2, out3, out4, out5, out6, out7;
  __m128i tmp0l, tmp0h, tmp1l, tmp1h, tmp2l, tmp2h, tmp3l, tmp3h,
    tmp10l, tmp10h, tmp11l, tmp11h, tmp12l, tmp12h, tmp13l, tmp13h,
    z3l, z3h, z4l, z4h,
    out0l, out0h, out1l, out1h, out2l, out2h, out3l, out3h, out4l, out4h,
    out5l, out5h, out6l, out6h, out7l, out7h;

  /* Constants */
  __m128i pw_zero = _mm_setzero_si128(),
    pw_f130_f054 = _mm_setr_epi16(__4X2(F_0_541 + F_0_765, F_0_541)),
    pw_f054_mf130 = _mm_setr_epi16(__4X2(F_0_541, F_0_541 - F_1_847)),
    pw_mf078_f117 = _mm_setr_epi16(__4X2(F_1_175 - F_1_961, F_1_175)),
    pw_f117_f078 = _mm_setr_epi16(__4X2(F_1_175, F_1_175 - F_0_390)),
    pw_mf060_mf089 = _mm_setr_epi16(__4X2(F_0_298 - F_0_899, -F_0_899)),
    pw_mf089_f060 = _mm_setr_epi16(__4X2(-F_0_899, F_1_501 - F_0_899)),
    pw_mf050_mf256 = _mm_setr_epi16(__4X2(F_2_053 - F_2_562, -F_2_562)),
    pw_mf256_f050 = _mm_setr_epi16(__4X2(-F_2_562, F_3_072 - F_2_562)),
    pd_descale_p1 = _mm_set1_epi32(1 << (DESCALE_P1 - 1)),
    pd_descale_p2 = _mm_set1_epi32(1 << (DESCALE_P2 - 1));

  /* Pass 1: process columns */

  col0 = VEC_LD(coef_block + 0 * 8);
  col1 = VEC_LD(coef_block + 1 * 8);
  col2 = VEC_LD(coef_block + 2 * 8);
  col3 = VEC_LD(coef_block + 3 * 8);
  col4 = VEC_LD(coef_block + 4 * 8);
  col5 = VEC_LD(coef_block + 5 * 8);
  col6 = VEC_LD(coef_block + 6 * 8);
  col7 = VEC_LD(coef_block + 7 * 8);

  tmp1 = _mm_or_si128(col1, col2);
  tmp2 = _mm_or_si128(col3, col4);
  tmp1 = _mm_or_si128(tmp1, tmp2);
  tmp3 = _mm_or_si128(col5, col6);
  tmp3 = _mm_or_si128(tmp3, col7);
  tmp1 = _mm_or_si128(tmp1, tmp3);

  quant0 = VEC_LD(dct_table);
  col0 = _mm_mullo_epi16(col0, quant0);

  if (VEC_ISZERO(tmp1)) {
    /* AC terms all zero */

    col0 = _mm_slli_epi16(col0, PASS1_BITS);

    row0 = VEC_SPLAT(col0, 0);
    row1 = VEC_SPLAT(col0, 1);
    row2 = VEC_SPLAT(col0, 2);
    row3 = VEC_SPLAT(col0, 3);
    row4 = VEC_SPLAT(col0, 4);
    row5 = VEC_SPLAT(col0, 5);
    row6 = VEC_SPLAT(col0, 6);
    row7 = VEC_SPLAT(col0, 7);

  } else {

    quant1 = VEC_LD(dct_table + 1 * 8);
    quant2 = VEC_LD(dct_table + 2 * 8);
    quant3 = VEC_LD(dct_table + 3 * 8);
    quant4 = VEC_LD(dct_table + 4 * 8);
    quant5 = VEC_LD(dct_table + 5 * 8);
    quant6 = VEC_LD(dct_table + 6 * 8);
    quant7 = VEC_LD(dct_table + 7 * 8);

    col1 = _mm_mullo_epi16(col1, quant1);
    col2 = _mm_mullo_epi16(col2, quant2);
    col3 = _mm_mullo_epi16(col3, quant3);
    col4 = _mm_mullo_epi16(col4, quant4);
    col5 = _mm_mullo_epi16(col5, quant5);
    col6 = _mm_mullo_epi16(col6, quant6);
    col7 = _mm_mullo_epi16(col7, quant7);

    DO_IDCT(col, 1);

    TRANSPOSE(out, row);
  }

  /* Pass 2: process rows */

  DO_IDCT(row, 2);

  IDCT_SAVE();
}
