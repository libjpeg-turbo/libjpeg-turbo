/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014-2015, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2014, Jay Foad.  All Rights Reserved.
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

/* This file is included by jcgray-e2k.c */


void jsimd_rgb_gray_convert_e2k(JDIMENSION img_width, JSAMPARRAY input_buf,
                                JSAMPIMAGE output_buf,
                                JDIMENSION output_row, int num_rows)
{
  JSAMPROW inptr, outptr;
  int pitch = img_width * RGB_PIXELSIZE, num_cols;

  __m128i pb_zero = _mm_setzero_si128();
  __m128i pb_shuf0 = _mm_setr_epi8(RGBG_INDEX);
#if RGB_PIXELSIZE == 4
  __m128i rgb3 = pb_zero;
#else
  __m128i pb_shuf4 = _mm_setr_epi8(RGBG_INDEX4(RGBG_INDEX));
#endif
  __m128i rgb0, rgb1 = pb_zero, rgb2 = pb_zero,
    rgbg0, rgbg1, rgbg2, rgbg3, rg0, rg1, rg2, rg3, bg0, bg1, bg2, bg3;
  __m128i y, yl, yh, y0, y1, y2, y3;

  /* Constants */
  __m128i pw_f0299_f0337 = _mm_setr_epi16(__4X2(F_0_299, F_0_337)),
    pw_f0114_f0250 = _mm_setr_epi16(__4X2(F_0_114, F_0_250)),
    pd_onehalf = _mm_set1_epi32(ONE_HALF);

  if (pitch > 0)
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr = output_buf[0][output_row];
    output_row++;

    for (num_cols = pitch; num_cols > 0;
         num_cols -= RGB_PIXELSIZE * 16, inptr += RGB_PIXELSIZE * 16,
         outptr += 16) {

      /* Little endian */
      rgb0 = VEC_LD(inptr);
      if (num_cols > 16) rgb1 = VEC_LD(inptr + 16);
      if (num_cols > 32) rgb2 = VEC_LD(inptr + 32);
#if RGB_PIXELSIZE == 4
      if (num_cols > 48) rgb3 = VEC_LD(inptr + 48);
#endif

#if RGB_PIXELSIZE == 3
      /* rgb0 = R0 G0 B0 R1 G1 B1 R2 G2 B2 R3 G3 B3 R4 G4 B4 R5
       * rgb1 = G5 B5 R6 G6 B6 R7 G7 B7 R8 G8 B8 R9 G9 B9 Ra Ga
       * rgb2 = Ba Rb Gb Bb Rc Gc Bc Rd Gd Bd Re Ge Be Rf Gf Bf
       */
      rgbg0 = _mm_shuffle_epi8(rgb0, pb_shuf0);
      rgbg1 = _mm_shuffle_epi8(VEC_ALIGNR8(rgb1, rgb0), pb_shuf4);
      rgbg2 = _mm_shuffle_epi8(VEC_ALIGNR8(rgb2, rgb1), pb_shuf0);
      rgbg3 = _mm_shuffle_epi8(rgb2, pb_shuf4);
#else
      /* rgb0 = R0 G0 B0 X0 R1 G1 B1 X1 R2 G2 B2 X2 R3 G3 B3 X3
       * rgb1 = R4 G4 B4 X4 R5 G5 B5 X5 R6 G6 B6 X6 R7 G7 B7 X7
       * rgb2 = R8 G8 B8 X8 R9 G9 B9 X9 Ra Ga Ba Xa Rb Gb Bb Xb
       * rgb3 = Rc Gc Bc Xc Rd Gd Bd Xd Re Ge Be Xe Rf Gf Bf Xf
       */
      rgbg0 = _mm_shuffle_epi8(rgb0, pb_shuf0);
      rgbg1 = _mm_shuffle_epi8(rgb1, pb_shuf0);
      rgbg2 = _mm_shuffle_epi8(rgb2, pb_shuf0);
      rgbg3 = _mm_shuffle_epi8(rgb3, pb_shuf0);
#endif
      /* rgbg0 = R0 G0 R1 G1 R2 G2 R3 G3 B0 G0 B1 G1 B2 G2 B3 G3
       * rgbg1 = R4 G4 R5 G5 R6 G6 R7 G7 B4 G4 B5 G5 B6 G6 B7 G7
       * rgbg2 = R8 G8 R9 G9 Ra Ga Rb Gb B8 G8 B9 G9 Ba Ga Bb Gb
       * rgbg3 = Rc Gc Rd Gd Re Ge Rf Gf Bc Gc Bd Gd Be Ge Bf Gf
       *
       * rg0 = R0 G0 R1 G1 R2 G2 R3 G3
       * bg0 = B0 G0 B1 G1 B2 G2 B3 G3
       * ...
       */
      rg0 = _mm_unpacklo_epi8(rgbg0, pb_zero);
      bg0 = _mm_unpackhi_epi8(rgbg0, pb_zero);
      rg1 = _mm_unpacklo_epi8(rgbg1, pb_zero);
      bg1 = _mm_unpackhi_epi8(rgbg1, pb_zero);
      rg2 = _mm_unpacklo_epi8(rgbg2, pb_zero);
      bg2 = _mm_unpackhi_epi8(rgbg2, pb_zero);
      rg3 = _mm_unpacklo_epi8(rgbg3, pb_zero);
      bg3 = _mm_unpackhi_epi8(rgbg3, pb_zero);

      /* (Original)
       * Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
       *
       * (This implementation)
       * Y  =  0.29900 * R + 0.33700 * G + 0.11400 * B + 0.25000 * G
       */

      /* Calculate Y values */

      y0 = _mm_add_epi32(_mm_madd_epi16(rg0, pw_f0299_f0337), pd_onehalf);
      y1 = _mm_add_epi32(_mm_madd_epi16(rg1, pw_f0299_f0337), pd_onehalf);
      y2 = _mm_add_epi32(_mm_madd_epi16(rg2, pw_f0299_f0337), pd_onehalf);
      y3 = _mm_add_epi32(_mm_madd_epi16(rg3, pw_f0299_f0337), pd_onehalf);
      y0 = _mm_add_epi32(_mm_madd_epi16(bg0, pw_f0114_f0250), y0);
      y1 = _mm_add_epi32(_mm_madd_epi16(bg1, pw_f0114_f0250), y1);
      y2 = _mm_add_epi32(_mm_madd_epi16(bg2, pw_f0114_f0250), y2);
      y3 = _mm_add_epi32(_mm_madd_epi16(bg3, pw_f0114_f0250), y3);

      yl = PACK_HIGH16(y0, y1);
      yh = PACK_HIGH16(y2, y3);
      y = _mm_packus_epi16(yl, yh);
      VEC_ST(outptr, y);
    }
  }
}
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#ifdef jsimd_rgb_gray_convert_e2k
#undef jsimd_rgb_gray_convert_e2k
#endif

