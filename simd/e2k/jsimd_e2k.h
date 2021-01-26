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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"
#include "jsimd_api_e2k.h"
#include <stdint.h>
#include <smmintrin.h> /* SSE4.1 */


/* Common code */

#define __4X2(a, b)  a, b, a, b, a, b, a, b

#if defined(__e2k__) && __iset__ >= 2

#if __iset__ < 5
/* 11111111 11222222 22222333 33333333
   RGBxRGBx RGBxRGBx RGBxRGBx RGBxRGBx
   RGBRGBRG   BRGBRGBR    GBRGBRGB     */
#define CONV_RGBX_RGB_INIT uint64_t \
  rgb_index0 = 0x0908060504020100ll, \
  rgb_index1 = 0x0c0a090806050402ll, \
  rgb_index2 = 0x0e0d0c0a09080605ll;

#define CONV_RGBX_RGB { \
  union { __m128i v; uint64_t d[2]; } a = { rgb0 }, \
    b = { rgb1 }, c = { rgb2 }, d = { rgb3 }; \
  a.d[0] = __builtin_e2k_pshufb(a.d[1], a.d[0], rgb_index0); \
  a.d[1] = __builtin_e2k_pshufb(b.d[0], a.d[1], rgb_index1); \
  b.d[0] = __builtin_e2k_pshufb(b.d[1], b.d[0], rgb_index2); \
  b.d[1] = __builtin_e2k_pshufb(c.d[1], c.d[0], rgb_index0); \
  c.d[0] = __builtin_e2k_pshufb(d.d[0], c.d[1], rgb_index1); \
  c.d[1] = __builtin_e2k_pshufb(d.d[1], d.d[0], rgb_index2); \
  rgb0 = a.v; rgb1 = b.v; rgb2 = c.v; \
}

static inline __m128i pack_high16(__m128i a, __m128i b) {
  union { __m128i v; uint64_t d[2]; } l = { a }, h = { b }, x;
  uint64_t index = 0x0f0e0b0a07060302ll;
  x.d[0] = __builtin_e2k_pshufb(l.d[1], l.d[0], index);
  x.d[1] = __builtin_e2k_pshufb(h.d[1], h.d[0], index);
  return x.v;
}
#else
#define CONV_RGBX_RGB_INIT __m128i \
  rgb_index0 = _mm_setr_epi8( \
     0,  1,  2,  4,   5,  6,  8,  9,  10, 12, 13, 14,  16, 17, 18, 20), \
  rgb_index1 = _mm_setr_epi8( \
     5,  6,  8,  9,  10, 12, 13, 14,  16, 17, 18, 20,  21, 22, 24, 25), \
  rgb_index2 = _mm_setr_epi8( \
    10, 12, 13, 14,  16, 17, 18, 20,  21, 22, 24, 25,  26, 28, 29, 30);

#define CONV_RGBX_RGB \
  rgb0 = (__m128i)__builtin_e2k_qppermb(rgb1, rgb0, rgb_index0); \
  rgb1 = (__m128i)__builtin_e2k_qppermb(rgb2, rgb1, rgb_index1); \
  rgb2 = (__m128i)__builtin_e2k_qppermb(rgb3, rgb2, rgb_index2);

static inline __m128i pack_high16(__m128i a, __m128i b) {
  __m128i index = _mm_setr_epi8(
    2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31);
  return (__m128i)__builtin_e2k_qppermb(b, a, index);
}
#endif

static inline __m128i vec_alignr8(__m128i a, __m128i b) {
  union { __m128i v; uint64_t d[2]; } l = { b }, h = { a }, x;
  x.d[0] = l.d[1];
  x.d[1] = h.d[0];
  return x.v;
}

static inline uint64_t vec_isnonzero(__m128i a) {
  union { __m128i v; uint64_t d[2]; } x = { a };
  return x.d[0] | x.d[1];
}

#define PACK_HIGH16(a, b) pack_high16(a, b)
#define VEC_ALIGNR8(a, b) vec_alignr8(a, b)
#define VEC_ISZERO(a) !vec_isnonzero(a)
#else

#define CONV_RGBX_RGB_INIT __m128i \
    rgb_index4 = _mm_setr_epi8(-1, -1, -1, -1, RGB_INDEX), \
    rgb_index0 = _mm_setr_epi8(RGB_INDEX, -1, -1, -1, -1);

#define CONV_RGBX_RGB \
  rgb0 = _mm_shuffle_epi8(rgb0, rgb_index4); \
  rgb1 = _mm_shuffle_epi8(rgb1, rgb_index0); \
  rgb2 = _mm_shuffle_epi8(rgb2, rgb_index4); \
  rgb3 = _mm_shuffle_epi8(rgb3, rgb_index0); \
  rgb0 = _mm_alignr_epi8(rgb1, rgb0, 4); \
  rgb1 = _mm_or_si128(_mm_bsrli_si128(rgb1, 4), _mm_bslli_si128(rgb2, 4)); \
  rgb2 = _mm_alignr_epi8(rgb3, rgb2, 12);

#define VEC_ALIGNR8(a, b) _mm_alignr_epi8(a, b, 8)

#define PACK_HIGH16(a, b) \
  _mm_packs_epi32(_mm_srai_epi32(a, 16), _mm_srai_epi32(b, 16))

#define VEC_ISZERO(a) (_mm_movemask_epi8(_mm_cmpeq_epi8(a, _mm_setzero_si128())) == 0xffff)
#endif

#define TRANSPOSE_FLOAT(a, b, c, d, e, f, g, h) \
  tmp0 = _mm_unpacklo_ps(a, c); \
  tmp1 = _mm_unpackhi_ps(a, c); \
  tmp2 = _mm_unpacklo_ps(b, d); \
  tmp3 = _mm_unpackhi_ps(b, d); \
  e = _mm_unpacklo_ps(tmp0, tmp2); \
  f = _mm_unpackhi_ps(tmp0, tmp2); \
  g = _mm_unpacklo_ps(tmp1, tmp3); \
  h = _mm_unpackhi_ps(tmp1, tmp3);

#define TRANSPOSE16(a, b) \
  b##0 = _mm_unpacklo_epi16(a##0, a##4); \
  b##1 = _mm_unpackhi_epi16(a##0, a##4); \
  b##2 = _mm_unpacklo_epi16(a##1, a##5); \
  b##3 = _mm_unpackhi_epi16(a##1, a##5); \
  b##4 = _mm_unpacklo_epi16(a##2, a##6); \
  b##5 = _mm_unpackhi_epi16(a##2, a##6); \
  b##6 = _mm_unpacklo_epi16(a##3, a##7); \
  b##7 = _mm_unpackhi_epi16(a##3, a##7);

#define TRANSPOSE8(a, b) \
  b##0 = _mm_unpacklo_epi8(a##0, a##2); \
  b##1 = _mm_unpackhi_epi8(a##0, a##2); \
  b##2 = _mm_unpacklo_epi8(a##1, a##3); \
  b##3 = _mm_unpackhi_epi8(a##1, a##3);

#define TRANSPOSE(a, b) \
  TRANSPOSE16(a, b) TRANSPOSE16(b, a) TRANSPOSE16(a, b)

#define IDCT_SAVE() { \
  __m128i pb_cj = _mm_set1_epi8((int8_t)CENTERJSAMPLE); \
  \
  row0 = _mm_xor_si128(_mm_packs_epi16(out0, out1), pb_cj); \
  row1 = _mm_xor_si128(_mm_packs_epi16(out2, out3), pb_cj); \
  row2 = _mm_xor_si128(_mm_packs_epi16(out4, out5), pb_cj); \
  row3 = _mm_xor_si128(_mm_packs_epi16(out6, out7), pb_cj); \
  \
  TRANSPOSE8(row, col) TRANSPOSE8(col, row) TRANSPOSE8(row, col) \
  \
  VEC_STL(output_buf[0] + output_col, col0); \
  VEC_STH(output_buf[1] + output_col, col0); \
  VEC_STL(output_buf[2] + output_col, col1); \
  VEC_STH(output_buf[3] + output_col, col1); \
  VEC_STL(output_buf[4] + output_col, col2); \
  VEC_STH(output_buf[5] + output_col, col2); \
  VEC_STL(output_buf[6] + output_col, col3); \
  VEC_STH(output_buf[7] + output_col, col3); \
}

#ifndef min
#define min(a, b)  ((a) < (b) ? (a) : (b))
#endif

#define VEC_LD(a)     _mm_loadu_si128((const __m128i*)(a))
#define VEC_ST(a, b)  _mm_storeu_si128((__m128i*)(a), b)
#define VEC_LD8(a)    _mm_loadl_epi64((const __m128i*)(a))
#define VEC_STL(a, b) _mm_storel_epi64((__m128i*)(a), b)
#if 1
#define VEC_STH(a, b) _mm_storeh_pd((double*)(a), _mm_castsi128_pd(b));
#else
#define VEC_STH(a, b) _mm_storel_epi64((__m128i*)(a), _mm_bsrli_si128(b, 8))
#endif

#if 1
#define VEC_SPLAT(v, i) _mm_shuffle_epi8(v, _mm_set1_epi16((i) * 2 | ((i) * 2 + 1) << 8))
#else
#define VEC_SPLAT(v, i) _mm_set1_epi16(_mm_extract_epi16(v, i))
#endif

