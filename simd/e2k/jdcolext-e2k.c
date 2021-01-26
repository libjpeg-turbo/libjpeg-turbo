/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2015, D. R. Commander.  All Rights Reserved.
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

/* This file is included by jdcolor-e2k.c */

#ifdef HAVE_EML
#if RGB_RED == 0 && RGB_GREEN == 1 && RGB_BLUE == 2 && RGB_PIXELSIZE == 3
#define EML_CONV_FN eml_Video_ColorYCC2RGB444_8U
#elif RGB_RED == 1 && RGB_GREEN == 2 && RGB_BLUE == 3 && RGB_PIXELSIZE == 4
#define EML_CONV_FN eml_Video_ColorYCC2ARGB444_8U
#elif RGB_RED == 2 && RGB_GREEN == 1 && RGB_BLUE == 0 && RGB_PIXELSIZE == 4
#define EML_CONV_FN eml_Video_ColorYCC2BGRA444_8U
#endif
#endif

void jsimd_ycc_rgb_convert_e2k(JDIMENSION out_width, JSAMPIMAGE input_buf,
                               JDIMENSION input_row, JSAMPARRAY output_buf,
                               int num_rows)
{
  JSAMPROW outptr, inptr0, inptr1, inptr2;
  int pitch = out_width * RGB_PIXELSIZE, num_cols;
  unsigned char __attribute__((aligned(16))) tmpbuf[RGB_PIXELSIZE * 16];

  __m128i rgb0, rgb1, rgb2, rgb3, y, cb, cr;
  __m128i rg0, rg1, bx0, bx1, yl, yh, cbl, cbh,
    crl, crh, rl, rh, gl, gh, bl, bh, g0w, g1w, g2w, g3w;
  __m128i g0, g1, g2, g3;

  /* Constants
   * NOTE: The >> 1 is to compensate for the fact that vec_madds() returns 17
   * high-order bits, not 16.
   */
  __m128i pw_f0402 = _mm_set1_epi16(F_0_402 >> 1),
    pw_mf0228 = _mm_set1_epi16(-F_0_228 >> 1),
    pw_mf0344_f0285 = _mm_setr_epi16(__4X2(-F_0_344, F_0_285)),
    pb_255 = _mm_set1_epi8(-1),
    pw_cj = _mm_set1_epi16(CENTERJSAMPLE),
    pd_onehalf = _mm_set1_epi32(ONE_HALF),
    pb_zero = _mm_setzero_si128();
#if RGB_PIXELSIZE == 3
    CONV_RGBX_RGB_INIT
#endif

  if (pitch > 0)
  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;

#ifdef EML_CONV_FN
    if (jsimd_use_eml &&
        !(((size_t)inptr0 | (size_t)inptr1 | (size_t)inptr2 |
        (size_t)outptr) & 7)) {
      EML_CONV_FN(inptr0, inptr1, inptr2, outptr, out_width);
    } else
#undef EML_CONV_FN
#endif
    for (num_cols = pitch; num_cols > 0;
         num_cols -= RGB_PIXELSIZE * 16, outptr += RGB_PIXELSIZE * 16,
         inptr0 += 16, inptr1 += 16, inptr2 += 16) {

      y = VEC_LD(inptr0);
      yl = _mm_unpacklo_epi8(y, pb_zero);
      yh = _mm_unpackhi_epi8(y, pb_zero);

      cb = VEC_LD(inptr1);
      cbl = _mm_unpacklo_epi8(cb, pb_zero);
      cbh = _mm_unpackhi_epi8(cb, pb_zero);
      cbl = _mm_sub_epi16(cbl, pw_cj);
      cbh = _mm_sub_epi16(cbh, pw_cj);

      cr = VEC_LD(inptr2);
      crl = _mm_unpacklo_epi8(cr, pb_zero);
      crh = _mm_unpackhi_epi8(cr, pb_zero);
      crl = _mm_sub_epi16(crl, pw_cj);
      crh = _mm_sub_epi16(crh, pw_cj);

      /* (Original)
       * R = Y                + 1.40200 * Cr
       * G = Y - 0.34414 * Cb - 0.71414 * Cr
       * B = Y + 1.77200 * Cb
       *
       * (This implementation)
       * R = Y                + 0.40200 * Cr + Cr
       * G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr
       * B = Y - 0.22800 * Cb + Cb + Cb
       */
      bl = _mm_mulhrs_epi16(cbl, pw_mf0228);
      bh = _mm_mulhrs_epi16(cbh, pw_mf0228);
      bl = _mm_add_epi16(bl, _mm_add_epi16(cbl, cbl));
      bh = _mm_add_epi16(bh, _mm_add_epi16(cbh, cbh));
      bl = _mm_add_epi16(bl, yl);
      bh = _mm_add_epi16(bh, yh);

      rl = _mm_mulhrs_epi16(crl, pw_f0402);
      rh = _mm_mulhrs_epi16(crh, pw_f0402);
      rl = _mm_add_epi16(rl, crl);
      rh = _mm_add_epi16(rh, crh);
      rl = _mm_add_epi16(rl, yl);
      rh = _mm_add_epi16(rh, yh);

      g0w = _mm_unpacklo_epi16(cbl, crl);
      g1w = _mm_unpackhi_epi16(cbl, crl);
      g0 = _mm_add_epi32(_mm_madd_epi16(g0w, pw_mf0344_f0285), pd_onehalf);
      g1 = _mm_add_epi32(_mm_madd_epi16(g1w, pw_mf0344_f0285), pd_onehalf);
      g2w = _mm_unpacklo_epi16(cbh, crh);
      g3w = _mm_unpackhi_epi16(cbh, crh);
      g2 = _mm_add_epi32(_mm_madd_epi16(g2w, pw_mf0344_f0285), pd_onehalf);
      g3 = _mm_add_epi32(_mm_madd_epi16(g3w, pw_mf0344_f0285), pd_onehalf);

      gl = PACK_HIGH16(g0, g1);
      gh = PACK_HIGH16(g2, g3);
      gl = _mm_sub_epi16(gl, crl);
      gh = _mm_sub_epi16(gh, crh);
      gl = _mm_add_epi16(gl, yl);
      gh = _mm_add_epi16(gh, yh);

      rl = _mm_packus_epi16(rl, rh);
      gl = _mm_packus_epi16(gl, gh);
      bl = _mm_packus_epi16(bl, bh);

#if RGB_RED == 0
#define C0 rl
#elif RGB_GREEN == 0
#define C0 gl
#elif RGB_BLUE == 0
#define C0 bl
#else
#define C0 pb_255
#endif

#if RGB_RED == 1
#define C1 rl
#elif RGB_GREEN == 1
#define C1 gl
#elif RGB_BLUE == 1
#define C1 bl
#else
#define C1 pb_255
#endif

#if RGB_RED == 2
#define C2 rl
#elif RGB_GREEN == 2
#define C2 gl
#elif RGB_BLUE == 2
#define C2 bl
#else
#define C2 pb_255
#endif

#if RGB_RED == 3
#define C3 rl
#elif RGB_GREEN == 3
#define C3 gl
#elif RGB_BLUE == 3
#define C3 bl
#else
#define C3 pb_255
#endif
      rg0 = _mm_unpacklo_epi8(C0, C1);
      rg1 = _mm_unpackhi_epi8(C0, C1);
      bx0 = _mm_unpacklo_epi8(C2, C3);
      bx1 = _mm_unpackhi_epi8(C2, C3);
#undef C0
#undef C1
#undef C2
#undef C3

      rgb0 = _mm_unpacklo_epi16(rg0, bx0);
      rgb1 = _mm_unpackhi_epi16(rg0, bx0);
      rgb2 = _mm_unpacklo_epi16(rg1, bx1);
      rgb3 = _mm_unpackhi_epi16(rg1, bx1);

#if RGB_PIXELSIZE == 3
      CONV_RGBX_RGB
#endif

      if (__builtin_expect(num_cols < RGB_PIXELSIZE * 16 && (num_cols & 15), 0)) {
        /* Slow path */
        VEC_ST(tmpbuf, rgb0);
        VEC_ST(tmpbuf + 16, rgb1);
        VEC_ST(tmpbuf + 32, rgb2);
#if RGB_PIXELSIZE == 4
        VEC_ST(tmpbuf + 48, rgb3);
#endif
        memcpy(outptr, tmpbuf, min(num_cols, RGB_PIXELSIZE * 16));
        break;
      } else {
        /* Fast path */
        VEC_ST(outptr, rgb0);
        if (num_cols > 16) VEC_ST(outptr + 16, rgb1);
        if (num_cols > 32) VEC_ST(outptr + 32, rgb2);
#if RGB_PIXELSIZE == 4
        if (num_cols > 48) VEC_ST(outptr + 48, rgb3);
#endif
      }
    }
  }
}
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#ifdef jsimd_ycc_rgb_convert_e2k
#undef jsimd_ycc_rgb_convert_e2k
#endif

