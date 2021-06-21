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

/* FLOAT INVERSE DCT */

#include "jsimd_e2k.h"

#define DO_IDCT(in, out) { \
  /* Even part */ \
  \
  tmp10 = _mm_add_ps(in##0, in##4); \
  tmp11 = _mm_sub_ps(in##0, in##4); \
  \
  tmp13 = _mm_add_ps(in##2, in##6); \
  tmp12 = _mm_sub_ps(in##2, in##6); \
  tmp12 = _mm_sub_ps(_mm_mul_ps(tmp12, pd_f1414), tmp13); \
  \
  tmp0 = _mm_add_ps(tmp10, tmp13); \
  tmp3 = _mm_sub_ps(tmp10, tmp13); \
  tmp1 = _mm_add_ps(tmp11, tmp12); \
  tmp2 = _mm_sub_ps(tmp11, tmp12); \
  \
  /* Odd part */ \
  \
  z13 = _mm_add_ps(in##5, in##3); \
  z10 = _mm_sub_ps(in##5, in##3); \
  z11 = _mm_add_ps(in##1, in##7); \
  z12 = _mm_sub_ps(in##1, in##7); \
  \
  tmp7 = _mm_add_ps(z11, z13); \
  tmp11 = _mm_sub_ps(z11, z13); \
  tmp11 = _mm_mul_ps(tmp11, pd_f1414); \
  \
  z5 = _mm_mul_ps(_mm_add_ps(z10, z12), pd_f1847); \
  tmp10 = _mm_sub_ps(z5, _mm_mul_ps(z12, pd_f1082)); \
  tmp12 = _mm_sub_ps(z5, _mm_mul_ps(z10, pd_f2613)); \
  \
  tmp6 = _mm_sub_ps(tmp12, tmp7); \
  tmp5 = _mm_sub_ps(tmp11, tmp6); \
  tmp4 = _mm_sub_ps(tmp10, tmp5); \
  \
  out##0 = _mm_add_ps(tmp0, tmp7); \
  out##7 = _mm_sub_ps(tmp0, tmp7); \
  out##1 = _mm_add_ps(tmp1, tmp6); \
  out##6 = _mm_sub_ps(tmp1, tmp6); \
  out##2 = _mm_add_ps(tmp2, tmp5); \
  out##5 = _mm_sub_ps(tmp2, tmp5); \
  out##3 = _mm_add_ps(tmp3, tmp4); \
  out##4 = _mm_sub_ps(tmp3, tmp4); \
}

#define QUANT_MUL(a, b, c, d, l, lo, i) \
  out0 = _mm_srai_epi32(_mm_unpack##lo##_epi16(col##a, col##a), 16); \
  out1 = _mm_srai_epi32(_mm_unpack##lo##_epi16(col##b, col##b), 16); \
  out2 = _mm_srai_epi32(_mm_unpack##lo##_epi16(col##c, col##c), 16); \
  out3 = _mm_srai_epi32(_mm_unpack##lo##_epi16(col##d, col##d), 16); \
  l##a = _mm_cvtepi32_ps(out0); \
  l##b = _mm_cvtepi32_ps(out1); \
  l##c = _mm_cvtepi32_ps(out2); \
  l##d = _mm_cvtepi32_ps(out3); \
  l##a = _mm_mul_ps(l##a, _mm_load_ps(dct_table + a * 8 + i)); \
  l##b = _mm_mul_ps(l##b, _mm_load_ps(dct_table + b * 8 + i)); \
  l##c = _mm_mul_ps(l##c, _mm_load_ps(dct_table + c * 8 + i)); \
  l##d = _mm_mul_ps(l##d, _mm_load_ps(dct_table + d * 8 + i));


void jsimd_idct_float_e2k(void *dct_table_, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
  float *dct_table = (float *)dct_table_;

  __m128i col0, col1, col2, col3, col4, col5, col6, col7,
    out0, out1, out2, out3, out4, out5, out6, out7, row0, row1, row2, row3;
  __m128 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
    tmp10, tmp11, tmp12, tmp13, z5, z10, z11, z12, z13;
  __m128 l0, l1, l2, l3, l4, l5, l6, l7;
  __m128 h0, h1, h2, h3, h4, h5, h6, h7;
  __m128 x0, x1, x2, x3, x4, x5, x6, x7;
  __m128 y0, y1, y2, y3, y4, y5, y6, y7;

  /* Constants */
  __m128 pd_f1414 = _mm_set1_ps(1.414213562f),
    pd_f1847 = _mm_set1_ps(1.847759065f),
    pd_f1082 = _mm_set1_ps(1.082392200f),
    pd_f2613 = _mm_set1_ps(2.613125930f);

  /* Pass 1: process columns */

  col0 = VEC_LD(coef_block + 0 * 8);
  col1 = VEC_LD(coef_block + 1 * 8);
  col2 = VEC_LD(coef_block + 2 * 8);
  col3 = VEC_LD(coef_block + 3 * 8);
  col4 = VEC_LD(coef_block + 4 * 8);
  col5 = VEC_LD(coef_block + 5 * 8);
  col6 = VEC_LD(coef_block + 6 * 8);
  col7 = VEC_LD(coef_block + 7 * 8);

  out1 = _mm_or_si128(col1, col2);
  out2 = _mm_or_si128(col3, col4);
  out1 = _mm_or_si128(out1, out2);
  out3 = _mm_or_si128(col5, col6);
  out3 = _mm_or_si128(out3, col7);
  out1 = _mm_or_si128(out1, out3);

  if (VEC_ISZERO(out1)) {
    /* AC terms all zero */

    out0 = _mm_srai_epi32(_mm_unpacklo_epi16(col0, col0), 16);
    out1 = _mm_srai_epi32(_mm_unpackhi_epi16(col0, col0), 16);
    tmp0 = _mm_cvtepi32_ps(out0);
    tmp1 = _mm_cvtepi32_ps(out1);
    tmp0 = _mm_mul_ps(tmp0, _mm_load_ps(dct_table));
    tmp1 = _mm_mul_ps(tmp1, _mm_load_ps(dct_table + 4));

    l0 = h0 = _mm_shuffle_ps(tmp0, tmp0, 0x00);
    l1 = h1 = _mm_shuffle_ps(tmp0, tmp0, 0x55);
    l2 = h2 = _mm_shuffle_ps(tmp0, tmp0, 0xaa);
    l3 = h3 = _mm_shuffle_ps(tmp0, tmp0, 0xff);
    l4 = h4 = _mm_shuffle_ps(tmp1, tmp1, 0x00);
    l5 = h5 = _mm_shuffle_ps(tmp1, tmp1, 0x55);
    l6 = h6 = _mm_shuffle_ps(tmp1, tmp1, 0xaa);
    l7 = h7 = _mm_shuffle_ps(tmp1, tmp1, 0xff);

  } else {

    QUANT_MUL(0, 2, 4, 6, l, lo, 0)
    QUANT_MUL(1, 3, 5, 7, l, lo, 0)
    DO_IDCT(l, x);

    QUANT_MUL(0, 2, 4, 6, h, hi, 4)
    QUANT_MUL(1, 3, 5, 7, h, hi, 4)
    DO_IDCT(h, y);

    TRANSPOSE_FLOAT(x0, x1, x2, x3, l0, l1, l2, l3)
    TRANSPOSE_FLOAT(x4, x5, x6, x7, h0, h1, h2, h3)
    TRANSPOSE_FLOAT(y0, y1, y2, y3, l4, l5, l6, l7)
    TRANSPOSE_FLOAT(y4, y5, y6, y7, h4, h5, h6, h7)
  }

  /* Pass 2: process rows */

  DO_IDCT(l, x);
  DO_IDCT(h, y);

#ifdef JSIMD_SAME_ROUNDING
#define OUT_ROUND(i) \
  tmp0 = _mm_add_ps(_mm_mul_ps(x##i, pd_f0125), pd_cj_rnd); \
  tmp1 = _mm_add_ps(_mm_mul_ps(y##i, pd_f0125), pd_cj_rnd); \
  out##i = _mm_packs_epi32(_mm_cvttps_epi32(tmp0), _mm_cvttps_epi32(tmp1));

  {
    __m128 pd_cj_rnd = _mm_set1_ps(0.5f + CENTERJSAMPLE),
      pd_f0125 = _mm_set1_ps(0.125f);

    OUT_ROUND(0) OUT_ROUND(1)
    OUT_ROUND(2) OUT_ROUND(3)
    OUT_ROUND(4) OUT_ROUND(5)
    OUT_ROUND(6) OUT_ROUND(7)
  }
  row0 = _mm_packus_epi16(out0, out1);
  row1 = _mm_packus_epi16(out2, out3);
  row2 = _mm_packus_epi16(out4, out5);
  row3 = _mm_packus_epi16(out6, out7);

  TRANSPOSE8(row, col) TRANSPOSE8(col, row) TRANSPOSE8(row, col)
#else  /* faster, slightly differ in rounding */
#define OUT_ROUND(i, a, b) out##i = _mm_blendv_epi8( \
  _mm_slli_epi32(_mm_castps_si128(_mm_add_ps(b, pd_round)), 16), \
  _mm_castps_si128(_mm_add_ps(a, pd_round)), pd_mask);

  {
    __m128i pd_mask = _mm_set1_epi32(0xffff);
    __m128 pd_round = _mm_set1_ps((3 << 22 | CENTERJSAMPLE) * 8);

    OUT_ROUND(0, x0, x4) OUT_ROUND(1, y0, y4)
    OUT_ROUND(2, x1, x5) OUT_ROUND(3, y1, y5)
    OUT_ROUND(4, x2, x6) OUT_ROUND(5, y2, y6)
    OUT_ROUND(6, x3, x7) OUT_ROUND(7, y3, y7)
  }
  row0 = _mm_packus_epi16(out0, out1);
  row1 = _mm_packus_epi16(out2, out3);
  row2 = _mm_packus_epi16(out4, out5);
  row3 = _mm_packus_epi16(out6, out7);

  TRANSPOSE8(row, out) TRANSPOSE8(out, col)
#endif
  VEC_STL(output_buf[0] + output_col, col0);
  VEC_STH(output_buf[1] + output_col, col0);
  VEC_STL(output_buf[2] + output_col, col1);
  VEC_STH(output_buf[3] + output_col, col1);
  VEC_STL(output_buf[4] + output_col, col2);
  VEC_STH(output_buf[5] + output_col, col2);
  VEC_STL(output_buf[6] + output_col, col3);
  VEC_STH(output_buf[7] + output_col, col3);
}
