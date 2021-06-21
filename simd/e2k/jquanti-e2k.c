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

/* INTEGER QUANTIZATION AND SAMPLE CONVERSION */

#include "jsimd_e2k.h"

#define LOAD_ROW(row) in##row = VEC_LD8(sample_data[row] + start_col)

void jsimd_convsamp_e2k(JSAMPARRAY sample_data, JDIMENSION start_col,
                        DCTELEM *workspace)
{
  __m128i in0, in1, in2, in3, in4, in5, in6, in7;
  __m128i out0, out1, out2, out3, out4, out5, out6, out7;

  /* Constants */
  __m128i pw_cj = _mm_set1_epi16(CENTERJSAMPLE),
    pb_zero = _mm_setzero_si128();

  LOAD_ROW(0);
  LOAD_ROW(1);
  LOAD_ROW(2);
  LOAD_ROW(3);
  LOAD_ROW(4);
  LOAD_ROW(5);
  LOAD_ROW(6);
  LOAD_ROW(7);

  out0 = _mm_unpacklo_epi8(in0, pb_zero);
  out1 = _mm_unpacklo_epi8(in1, pb_zero);
  out2 = _mm_unpacklo_epi8(in2, pb_zero);
  out3 = _mm_unpacklo_epi8(in3, pb_zero);
  out4 = _mm_unpacklo_epi8(in4, pb_zero);
  out5 = _mm_unpacklo_epi8(in5, pb_zero);
  out6 = _mm_unpacklo_epi8(in6, pb_zero);
  out7 = _mm_unpacklo_epi8(in7, pb_zero);

  out0 = _mm_sub_epi16(out0, pw_cj);
  out1 = _mm_sub_epi16(out1, pw_cj);
  out2 = _mm_sub_epi16(out2, pw_cj);
  out3 = _mm_sub_epi16(out3, pw_cj);
  out4 = _mm_sub_epi16(out4, pw_cj);
  out5 = _mm_sub_epi16(out5, pw_cj);
  out6 = _mm_sub_epi16(out6, pw_cj);
  out7 = _mm_sub_epi16(out7, pw_cj);

  VEC_ST(workspace + 0 * 8, out0);
  VEC_ST(workspace + 1 * 8, out1);
  VEC_ST(workspace + 2 * 8, out2);
  VEC_ST(workspace + 3 * 8, out3);
  VEC_ST(workspace + 4 * 8, out4);
  VEC_ST(workspace + 5 * 8, out5);
  VEC_ST(workspace + 6 * 8, out6);
  VEC_ST(workspace + 7 * 8, out7);
}


#define WORD_BIT  16
#define MULTIPLY(vs0, vs1, out) out = _mm_mulhi_epu16(vs0, vs1)

void jsimd_quantize_e2k(JCOEFPTR coef_block, DCTELEM *divisors,
                        DCTELEM *workspace)
{
  __m128i row0, row1, row2, row3, row4, row5, row6, row7,
    row0s, row1s, row2s, row3s, row4s, row5s, row6s, row7s,
    corr0, corr1, corr2, corr3, corr4, corr5, corr6, corr7,
    recip0, recip1, recip2, recip3, recip4, recip5, recip6, recip7,
    scale0, scale1, scale2, scale3, scale4, scale5, scale6, scale7;

#if defined(__e2k__) && __iset__ < 3
  row0 = VEC_LD(workspace + 0 * 8);
  row1 = VEC_LD(workspace + 1 * 8);
  row2 = VEC_LD(workspace + 2 * 8);
  row3 = VEC_LD(workspace + 3 * 8);
  row4 = VEC_LD(workspace + 4 * 8);
  row5 = VEC_LD(workspace + 5 * 8);
  row6 = VEC_LD(workspace + 6 * 8);
  row7 = VEC_LD(workspace + 7 * 8);

  /* Branch-less absolute value */
  row0s = _mm_srai_epi16(row0, WORD_BIT - 1);
  row1s = _mm_srai_epi16(row1, WORD_BIT - 1);
  row2s = _mm_srai_epi16(row2, WORD_BIT - 1);
  row3s = _mm_srai_epi16(row3, WORD_BIT - 1);
  row4s = _mm_srai_epi16(row4, WORD_BIT - 1);
  row5s = _mm_srai_epi16(row5, WORD_BIT - 1);
  row6s = _mm_srai_epi16(row6, WORD_BIT - 1);
  row7s = _mm_srai_epi16(row7, WORD_BIT - 1);
  row0 = _mm_xor_si128(row0, row0s);
  row1 = _mm_xor_si128(row1, row1s);
  row2 = _mm_xor_si128(row2, row2s);
  row3 = _mm_xor_si128(row3, row3s);
  row4 = _mm_xor_si128(row4, row4s);
  row5 = _mm_xor_si128(row5, row5s);
  row6 = _mm_xor_si128(row6, row6s);
  row7 = _mm_xor_si128(row7, row7s);
  row0 = _mm_sub_epi16(row0, row0s);
  row1 = _mm_sub_epi16(row1, row1s);
  row2 = _mm_sub_epi16(row2, row2s);
  row3 = _mm_sub_epi16(row3, row3s);
  row4 = _mm_sub_epi16(row4, row4s);
  row5 = _mm_sub_epi16(row5, row5s);
  row6 = _mm_sub_epi16(row6, row6s);
  row7 = _mm_sub_epi16(row7, row7s);
#else
  row0s = VEC_LD(workspace + 0 * 8);
  row1s = VEC_LD(workspace + 1 * 8);
  row2s = VEC_LD(workspace + 2 * 8);
  row3s = VEC_LD(workspace + 3 * 8);
  row4s = VEC_LD(workspace + 4 * 8);
  row5s = VEC_LD(workspace + 5 * 8);
  row6s = VEC_LD(workspace + 6 * 8);
  row7s = VEC_LD(workspace + 7 * 8);
  row0 = _mm_abs_epi16(row0s);
  row1 = _mm_abs_epi16(row1s);
  row2 = _mm_abs_epi16(row2s);
  row3 = _mm_abs_epi16(row3s);
  row4 = _mm_abs_epi16(row4s);
  row5 = _mm_abs_epi16(row5s);
  row6 = _mm_abs_epi16(row6s);
  row7 = _mm_abs_epi16(row7s);
#endif

  corr0 = VEC_LD(divisors + DCTSIZE2 + 0 * 8);
  corr1 = VEC_LD(divisors + DCTSIZE2 + 1 * 8);
  corr2 = VEC_LD(divisors + DCTSIZE2 + 2 * 8);
  corr3 = VEC_LD(divisors + DCTSIZE2 + 3 * 8);
  corr4 = VEC_LD(divisors + DCTSIZE2 + 4 * 8);
  corr5 = VEC_LD(divisors + DCTSIZE2 + 5 * 8);
  corr6 = VEC_LD(divisors + DCTSIZE2 + 6 * 8);
  corr7 = VEC_LD(divisors + DCTSIZE2 + 7 * 8);

  row0 = _mm_add_epi16(row0, corr0);
  row1 = _mm_add_epi16(row1, corr1);
  row2 = _mm_add_epi16(row2, corr2);
  row3 = _mm_add_epi16(row3, corr3);
  row4 = _mm_add_epi16(row4, corr4);
  row5 = _mm_add_epi16(row5, corr5);
  row6 = _mm_add_epi16(row6, corr6);
  row7 = _mm_add_epi16(row7, corr7);

  recip0 = VEC_LD(divisors + 0 * 8);
  recip1 = VEC_LD(divisors + 1 * 8);
  recip2 = VEC_LD(divisors + 2 * 8);
  recip3 = VEC_LD(divisors + 3 * 8);
  recip4 = VEC_LD(divisors + 4 * 8);
  recip5 = VEC_LD(divisors + 5 * 8);
  recip6 = VEC_LD(divisors + 6 * 8);
  recip7 = VEC_LD(divisors + 7 * 8);

  MULTIPLY(row0, recip0, row0);
  MULTIPLY(row1, recip1, row1);
  MULTIPLY(row2, recip2, row2);
  MULTIPLY(row3, recip3, row3);
  MULTIPLY(row4, recip4, row4);
  MULTIPLY(row5, recip5, row5);
  MULTIPLY(row6, recip6, row6);
  MULTIPLY(row7, recip7, row7);

  scale0 = VEC_LD(divisors + DCTSIZE2 * 2 + 0 * 8);
  scale1 = VEC_LD(divisors + DCTSIZE2 * 2 + 1 * 8);
  scale2 = VEC_LD(divisors + DCTSIZE2 * 2 + 2 * 8);
  scale3 = VEC_LD(divisors + DCTSIZE2 * 2 + 3 * 8);
  scale4 = VEC_LD(divisors + DCTSIZE2 * 2 + 4 * 8);
  scale5 = VEC_LD(divisors + DCTSIZE2 * 2 + 5 * 8);
  scale6 = VEC_LD(divisors + DCTSIZE2 * 2 + 6 * 8);
  scale7 = VEC_LD(divisors + DCTSIZE2 * 2 + 7 * 8);

  MULTIPLY(row0, scale0, row0);
  MULTIPLY(row1, scale1, row1);
  MULTIPLY(row2, scale2, row2);
  MULTIPLY(row3, scale3, row3);
  MULTIPLY(row4, scale4, row4);
  MULTIPLY(row5, scale5, row5);
  MULTIPLY(row6, scale6, row6);
  MULTIPLY(row7, scale7, row7);

#if defined(__e2k__) && __iset__ < 3
  row0 = _mm_xor_si128(row0, row0s);
  row1 = _mm_xor_si128(row1, row1s);
  row2 = _mm_xor_si128(row2, row2s);
  row3 = _mm_xor_si128(row3, row3s);
  row4 = _mm_xor_si128(row4, row4s);
  row5 = _mm_xor_si128(row5, row5s);
  row6 = _mm_xor_si128(row6, row6s);
  row7 = _mm_xor_si128(row7, row7s);
  row0 = _mm_sub_epi16(row0, row0s);
  row1 = _mm_sub_epi16(row1, row1s);
  row2 = _mm_sub_epi16(row2, row2s);
  row3 = _mm_sub_epi16(row3, row3s);
  row4 = _mm_sub_epi16(row4, row4s);
  row5 = _mm_sub_epi16(row5, row5s);
  row6 = _mm_sub_epi16(row6, row6s);
  row7 = _mm_sub_epi16(row7, row7s);
#else
  row0 = _mm_sign_epi16(row0, row0s);
  row1 = _mm_sign_epi16(row1, row1s);
  row2 = _mm_sign_epi16(row2, row2s);
  row3 = _mm_sign_epi16(row3, row3s);
  row4 = _mm_sign_epi16(row4, row4s);
  row5 = _mm_sign_epi16(row5, row5s);
  row6 = _mm_sign_epi16(row6, row6s);
  row7 = _mm_sign_epi16(row7, row7s);
#endif

  VEC_ST(coef_block + 0 * 8, row0);
  VEC_ST(coef_block + 1 * 8, row1);
  VEC_ST(coef_block + 2 * 8, row2);
  VEC_ST(coef_block + 3 * 8, row3);
  VEC_ST(coef_block + 4 * 8, row4);
  VEC_ST(coef_block + 5 * 8, row5);
  VEC_ST(coef_block + 6 * 8, row6);
  VEC_ST(coef_block + 7 * 8, row7);
}
