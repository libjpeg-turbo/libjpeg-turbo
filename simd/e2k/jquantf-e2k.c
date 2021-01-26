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

/* FLOAT QUANTIZATION AND SAMPLE CONVERSION */

#include "jsimd_e2k.h"

#define LOAD_ROW(row) in##row = VEC_LD8(sample_data[row] + start_col)
#define STORE_ROW(i) \
  in0 = _mm_unpacklo_epi16(out##i, pb_zero); \
  in1 = _mm_unpackhi_epi16(out##i, pb_zero); \
  in0 = _mm_sub_epi32(in0, pd_cj); \
  in1 = _mm_sub_epi32(in1, pd_cj); \
  _mm_storeu_ps(workspace + i * 8, _mm_cvtepi32_ps(in0)); \
  _mm_storeu_ps(workspace + i * 8 + 4, _mm_cvtepi32_ps(in1));

void jsimd_convsamp_float_e2k(JSAMPARRAY sample_data, JDIMENSION start_col,
                              FAST_FLOAT *workspace)
{
  __m128i in0, in1, in2, in3, in4, in5, in6, in7;
  __m128i out0, out1, out2, out3, out4, out5, out6, out7;

  /* Constants */
  __m128i pd_cj = _mm_set1_epi32(CENTERJSAMPLE),
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

  STORE_ROW(0)
  STORE_ROW(1)
  STORE_ROW(2)
  STORE_ROW(3)
  STORE_ROW(4)
  STORE_ROW(5)
  STORE_ROW(6)
  STORE_ROW(7)
}

void jsimd_quantize_float_e2k(JCOEFPTR coef_block, FAST_FLOAT *divisors,
                              FAST_FLOAT *workspace)
{
  int i = 0;
  __m128 row0, row1, row2, row3, recip0, recip1, recip2, recip3;
  __m128i out0, out1;
#ifdef JSIMD_SAME_ROUNDING
  __m128 pd_f16k5 = _mm_set1_ps(16384.5f);
  __m128i pw_m16k = _mm_set1_epi16(-16384);
#endif

  for (; i < 4; i++, workspace += 16, divisors += 16, coef_block += 16) {
    row0 = _mm_loadu_ps(workspace + 0 * 4);
    row1 = _mm_loadu_ps(workspace + 1 * 4);
    row2 = _mm_loadu_ps(workspace + 2 * 4);
    row3 = _mm_loadu_ps(workspace + 3 * 4);

    recip0 = _mm_loadu_ps(divisors + 0 * 4);
    recip1 = _mm_loadu_ps(divisors + 1 * 4);
    recip2 = _mm_loadu_ps(divisors + 2 * 4);
    recip3 = _mm_loadu_ps(divisors + 3 * 4);

    row0 = _mm_mul_ps(row0, recip0);
    row1 = _mm_mul_ps(row1, recip1);
    row2 = _mm_mul_ps(row2, recip2);
    row3 = _mm_mul_ps(row3, recip3);

#ifdef JSIMD_SAME_ROUNDING
    row0 = _mm_add_ps(row0, pd_f16k5);
    row1 = _mm_add_ps(row1, pd_f16k5);
    row2 = _mm_add_ps(row2, pd_f16k5);
    row3 = _mm_add_ps(row3, pd_f16k5);

    out0 = _mm_packs_epi32(_mm_cvttps_epi32(row0), _mm_cvttps_epi32(row1));
    out1 = _mm_packs_epi32(_mm_cvttps_epi32(row2), _mm_cvttps_epi32(row3));

    out0 = _mm_add_epi16(out0, pw_m16k);
    out1 = _mm_add_epi16(out1, pw_m16k);
#else
    out0 = _mm_packs_epi32(_mm_cvtps_epi32(row0), _mm_cvtps_epi32(row1));
    out1 = _mm_packs_epi32(_mm_cvtps_epi32(row2), _mm_cvtps_epi32(row3));
#endif
    VEC_ST(coef_block, out0);
    VEC_ST(coef_block + 8, out1);
  }
}
