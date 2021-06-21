/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014-2015, D. R. Commander.  All Rights Reserved.
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

/* FAST INTEGER INVERSE DCT */

#include "jsimd_e2k.h"


#define F_1_082  277              /* FIX(1.082392200) */
#define F_1_414  362              /* FIX(1.414213562) */
#define F_1_847  473              /* FIX(1.847759065) */
#define F_2_613  669              /* FIX(2.613125930) */
#define F_1_613  (F_2_613 - 256)  /* FIX(2.613125930) - FIX(1) */

#define CONST_BITS  8
#define PASS1_BITS  2
#define PRE_MULTIPLY_SCALE_BITS  2
#define CONST_SHIFT  (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)


#define DO_IDCT(in) { \
  /* Even part */ \
  \
  tmp10 = _mm_add_epi16(in##0, in##4); \
  tmp11 = _mm_sub_epi16(in##0, in##4); \
  tmp13 = _mm_add_epi16(in##2, in##6); \
  \
  tmp12 = _mm_sub_epi16(in##2, in##6); \
  tmp12 = _mm_slli_epi16(tmp12, PRE_MULTIPLY_SCALE_BITS); \
  tmp12 = _mm_mulhi_epi16(tmp12, pw_F1414); \
  tmp12 = _mm_sub_epi16(tmp12, tmp13); \
  \
  tmp0 = _mm_add_epi16(tmp10, tmp13); \
  tmp3 = _mm_sub_epi16(tmp10, tmp13); \
  tmp1 = _mm_add_epi16(tmp11, tmp12); \
  tmp2 = _mm_sub_epi16(tmp11, tmp12); \
  \
  /* Odd part */ \
  \
  z13 = _mm_add_epi16(in##5, in##3); \
  z10 = _mm_sub_epi16(in##5, in##3); \
  z10s = _mm_slli_epi16(z10, PRE_MULTIPLY_SCALE_BITS); \
  z11 = _mm_add_epi16(in##1, in##7); \
  z12s = _mm_sub_epi16(in##1, in##7); \
  z12s = _mm_slli_epi16(z12s, PRE_MULTIPLY_SCALE_BITS); \
  \
  tmp11 = _mm_sub_epi16(z11, z13); \
  tmp11 = _mm_slli_epi16(tmp11, PRE_MULTIPLY_SCALE_BITS); \
  tmp11 = _mm_mulhi_epi16(tmp11, pw_F1414); \
  \
  tmp7 = _mm_add_epi16(z11, z13); \
  \
  /* To avoid overflow... \
   * \
   * (Original) \
   * tmp12 = -2.613125930 * z10 + z5; \
   * \
   * (This implementation) \
   * tmp12 = (-1.613125930 - 1) * z10 + z5; \
   *       = -1.613125930 * z10 - z10 + z5; \
   */ \
  \
  z5 = _mm_add_epi16(z10s, z12s); \
  z5 = _mm_mulhi_epi16(z5, pw_F1847); \
  \
  tmp10 = _mm_mulhi_epi16(z12s, pw_F1082); \
  tmp10 = _mm_sub_epi16(tmp10, z5); \
  tmp12 = _mm_add_epi16(_mm_mulhi_epi16(z10s, pw_MF1613), z5); \
  tmp12 = _mm_sub_epi16(tmp12, z10); \
  \
  tmp6 = _mm_sub_epi16(tmp12, tmp7); \
  tmp5 = _mm_sub_epi16(tmp11, tmp6); \
  tmp4 = _mm_add_epi16(tmp10, tmp5); \
  \
  out0 = _mm_add_epi16(tmp0, tmp7); \
  out1 = _mm_add_epi16(tmp1, tmp6); \
  out2 = _mm_add_epi16(tmp2, tmp5); \
  out3 = _mm_sub_epi16(tmp3, tmp4); \
  out4 = _mm_add_epi16(tmp3, tmp4); \
  out5 = _mm_sub_epi16(tmp2, tmp5); \
  out6 = _mm_sub_epi16(tmp1, tmp6); \
  out7 = _mm_sub_epi16(tmp0, tmp7); \
}


void jsimd_idct_ifast_e2k(void *dct_table_, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
  short *dct_table = (short *)dct_table_;

  __m128i row0, row1, row2, row3, row4, row5, row6, row7,
    col0, col1, col2, col3, col4, col5, col6, col7,
    quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7,
    tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13,
    z5, z10, z10s, z11, z12s, z13,
    out0, out1, out2, out3, out4, out5, out6, out7;

  /* Constants */
  __m128i pw_F1414 = _mm_set1_epi16(F_1_414 << CONST_SHIFT),
    pw_F1847 = _mm_set1_epi16(F_1_847 << CONST_SHIFT),
    pw_MF1613 = _mm_set1_epi16(-F_1_613 << CONST_SHIFT),
    pw_F1082 = _mm_set1_epi16(F_1_082 << CONST_SHIFT);

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

    DO_IDCT(col);

    TRANSPOSE(out, row);
  }

  /* Pass 2: process rows */

  DO_IDCT(row);

  out0 = _mm_srai_epi16(out0, PASS1_BITS + 3);
  out1 = _mm_srai_epi16(out1, PASS1_BITS + 3);
  out2 = _mm_srai_epi16(out2, PASS1_BITS + 3);
  out3 = _mm_srai_epi16(out3, PASS1_BITS + 3);
  out4 = _mm_srai_epi16(out4, PASS1_BITS + 3);
  out5 = _mm_srai_epi16(out5, PASS1_BITS + 3);
  out6 = _mm_srai_epi16(out6, PASS1_BITS + 3);
  out7 = _mm_srai_epi16(out7, PASS1_BITS + 3);

  IDCT_SAVE();
}
