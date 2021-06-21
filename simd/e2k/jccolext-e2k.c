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

/* This file is included by jccolor-e2k.c */

#ifdef HAVE_EML
#if RGB_RED == 0 && RGB_GREEN == 1 && RGB_BLUE == 2 && RGB_PIXELSIZE == 3
#define EML_CONV_FN eml_Video_ColorRGB2YCC444_8U
#elif RGB_RED == 1 && RGB_GREEN == 2 && RGB_BLUE == 3 && RGB_PIXELSIZE == 4
#define EML_CONV_FN eml_Video_ColorARGB2YCC444_8U
#elif RGB_RED == 2 && RGB_GREEN == 1 && RGB_BLUE == 0 && RGB_PIXELSIZE == 4
#define EML_CONV_FN eml_Video_ColorBGRA2YCC444_8U
#elif RGB_RED == 3 && RGB_GREEN == 2 && RGB_BLUE == 1 && RGB_PIXELSIZE == 4
#define EML_CONV_FN eml_Video_ColorABGR2YCC444_8U
#endif
#endif

void jsimd_rgb_ycc_convert_e2k(JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf,
                               JDIMENSION output_row, int num_rows)
{
  JSAMPROW inptr, outptr0, outptr1, outptr2;
  int pitch = img_width * RGB_PIXELSIZE, num_cols;
  unsigned char __attribute__((aligned(16))) tmpbuf[RGB_PIXELSIZE * 16];

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
  __m128i cb, cr, crl, crh, cbl, cbh;
  __m128i cr0, cr1, cr2, cr3, cb0, cb1, cb2, cb3;

  /* Constants */
  __m128i pw_f0299_f0337 = _mm_setr_epi16(__4X2(F_0_299, F_0_337)),
    pw_f0114_f0250 = _mm_setr_epi16(__4X2(F_0_114, F_0_250)),
    pw_mf016_mf033 = _mm_setr_epi16(__4X2(-F_0_168, -F_0_331)),
    pw_mf008_mf041 = _mm_setr_epi16(__4X2(-F_0_081, -F_0_418)),
    pw_mf050_f000 = _mm_setr_epi16(__4X2(-F_0_500, 0)),
    pd_onehalf = _mm_set1_epi32(ONE_HALF),
    pd_onehalfm1_cj = _mm_set1_epi32(ONE_HALF - 1 + (CENTERJSAMPLE << SCALEBITS));

  if (pitch > 0)
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;

#ifdef EML_CONV_FN
    if (jsimd_use_eml && !(((size_t)inptr |
        (size_t)outptr0 | (size_t)outptr1 | (size_t)outptr2) & 7)) {
      EML_CONV_FN(inptr, outptr0, outptr1, outptr2, img_width);
    } else
#undef EML_CONV_FN
#endif
    for (num_cols = pitch; num_cols > 0;
         num_cols -= RGB_PIXELSIZE * 16, inptr += RGB_PIXELSIZE * 16,
         outptr0 += 16, outptr1 += 16, outptr2 += 16) {

        if (num_cols < RGB_PIXELSIZE * 16 && (num_cols & 15)) {
          /* Slow path */
          memcpy(tmpbuf, inptr, min(num_cols, RGB_PIXELSIZE * 16));
          rgb0 = VEC_LD(tmpbuf);
          rgb1 = VEC_LD(tmpbuf + 16);
          rgb2 = VEC_LD(tmpbuf + 32);
#if RGB_PIXELSIZE == 4
          rgb3 = VEC_LD(tmpbuf + 48);
#endif
        } else {
          /* Fast path */
          rgb0 = VEC_LD(inptr);
          if (num_cols > 16) rgb1 = VEC_LD(inptr + 16);
          if (num_cols > 32) rgb2 = VEC_LD(inptr + 32);
#if RGB_PIXELSIZE == 4
          if (num_cols > 48) rgb3 = VEC_LD(inptr + 48);
#endif
        }

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
       * Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B + CENTERJSAMPLE
       * Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B + CENTERJSAMPLE
       *
       * (This implementation)
       * Y  =  0.29900 * R + 0.33700 * G + 0.11400 * B + 0.25000 * G
       * Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B + CENTERJSAMPLE
       * Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B + CENTERJSAMPLE
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
      VEC_ST(outptr0, y);

      /* Calculate Cb values */
      cb0 = _mm_add_epi32(_mm_madd_epi16(rg0, pw_mf016_mf033), pd_onehalfm1_cj);
      cb1 = _mm_add_epi32(_mm_madd_epi16(rg1, pw_mf016_mf033), pd_onehalfm1_cj);
      cb2 = _mm_add_epi32(_mm_madd_epi16(rg2, pw_mf016_mf033), pd_onehalfm1_cj);
      cb3 = _mm_add_epi32(_mm_madd_epi16(rg3, pw_mf016_mf033), pd_onehalfm1_cj);
      cb0 = _mm_sub_epi32(cb0, _mm_madd_epi16(bg0, pw_mf050_f000));
      cb1 = _mm_sub_epi32(cb1, _mm_madd_epi16(bg1, pw_mf050_f000));
      cb2 = _mm_sub_epi32(cb2, _mm_madd_epi16(bg2, pw_mf050_f000));
      cb3 = _mm_sub_epi32(cb3, _mm_madd_epi16(bg3, pw_mf050_f000));

      cbl = PACK_HIGH16(cb0, cb1);
      cbh = PACK_HIGH16(cb2, cb3);
      cb = _mm_packus_epi16(cbl, cbh);
      VEC_ST(outptr1, cb);

      /* Calculate Cr values */
      cr0 = _mm_add_epi32(_mm_madd_epi16(bg0, pw_mf008_mf041), pd_onehalfm1_cj);
      cr1 = _mm_add_epi32(_mm_madd_epi16(bg1, pw_mf008_mf041), pd_onehalfm1_cj);
      cr2 = _mm_add_epi32(_mm_madd_epi16(bg2, pw_mf008_mf041), pd_onehalfm1_cj);
      cr3 = _mm_add_epi32(_mm_madd_epi16(bg3, pw_mf008_mf041), pd_onehalfm1_cj);
      cr0 = _mm_sub_epi32(cr0, _mm_madd_epi16(rg0, pw_mf050_f000));
      cr1 = _mm_sub_epi32(cr1, _mm_madd_epi16(rg1, pw_mf050_f000));
      cr2 = _mm_sub_epi32(cr2, _mm_madd_epi16(rg2, pw_mf050_f000));
      cr3 = _mm_sub_epi32(cr3, _mm_madd_epi16(rg3, pw_mf050_f000));

      crl = PACK_HIGH16(cr0, cr1);
      crh = PACK_HIGH16(cr2, cr3);
      cr = _mm_packus_epi16(crl, crh);
      VEC_ST(outptr2, cr);
    }
  }
}
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#ifdef jsimd_rgb_ycc_convert_e2k
#undef jsimd_rgb_ycc_convert_e2k
#endif

