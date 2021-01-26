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

/* This file is included by jdmerge-e2k.c */


void jsimd_h2v1_merged_upsample_e2k(JDIMENSION output_width,
                                    JSAMPIMAGE input_buf,
                                    JDIMENSION in_row_group_ctr,
                                    JSAMPARRAY output_buf)
{
  JSAMPROW outptr, inptr0, inptr1, inptr2;
  int pitch = output_width * RGB_PIXELSIZE, num_cols, yloop;
  unsigned char __attribute__((aligned(16))) tmpbuf[RGB_PIXELSIZE * 16];

  __m128i rgb0, rgb1, rgb2, rgb3, y, cb, cr;
  __m128i rg0, rg1, bx0, bx1, ye, yo, cbl, cbh,
    crl, crh, r_yl, r_yh, g_yl, g_yh, b_yl, b_yh, g_y0w, g_y1w, g_y2w, g_y3w,
    rl, rh, gl, gh, bl, bh, re, ro, ge, go, be, bo;
  __m128i g_y0, g_y1, g_y2, g_y3;

  /* Constants
   * NOTE: The >> 1 is to compensate for the fact that vec_madds() returns 17
   * high-order bits, not 16.
   */
  __m128i pw_f0402 = _mm_set1_epi16(F_0_402 >> 1),
    pw_mf0228 = _mm_set1_epi16(-F_0_228 >> 1),
    pw_mf0344_f0285 = _mm_setr_epi16(__4X2(-F_0_344, F_0_285)),
    pb_255 = _mm_set1_epi8(-1),
    even_mask = _mm_set1_epi16(255),
    pw_cj = _mm_set1_epi16(CENTERJSAMPLE),
    pd_onehalf = _mm_set1_epi32(ONE_HALF),
    pb_zero = _mm_setzero_si128();
#if RGB_PIXELSIZE == 3
    CONV_RGBX_RGB_INIT
#endif

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];

  for (num_cols = pitch; num_cols > 0; inptr1 += 16, inptr2 += 16) {

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
    b_yl = _mm_mulhrs_epi16(cbl, pw_mf0228);
    b_yh = _mm_mulhrs_epi16(cbh, pw_mf0228);
    b_yl = _mm_add_epi16(b_yl, _mm_add_epi16(cbl, cbl));
    b_yh = _mm_add_epi16(b_yh, _mm_add_epi16(cbh, cbh));

    r_yl = _mm_mulhrs_epi16(crl, pw_f0402);
    r_yh = _mm_mulhrs_epi16(crh, pw_f0402);
    r_yl = _mm_add_epi16(r_yl, crl);
    r_yh = _mm_add_epi16(r_yh, crh);

    g_y0w = _mm_unpacklo_epi16(cbl, crl);
    g_y1w = _mm_unpackhi_epi16(cbl, crl);
    g_y0 = _mm_add_epi32(_mm_madd_epi16(g_y0w, pw_mf0344_f0285), pd_onehalf);
    g_y1 = _mm_add_epi32(_mm_madd_epi16(g_y1w, pw_mf0344_f0285), pd_onehalf);
    g_y2w = _mm_unpacklo_epi16(cbh, crh);
    g_y3w = _mm_unpackhi_epi16(cbh, crh);
    g_y2 = _mm_add_epi32(_mm_madd_epi16(g_y2w, pw_mf0344_f0285), pd_onehalf);
    g_y3 = _mm_add_epi32(_mm_madd_epi16(g_y3w, pw_mf0344_f0285), pd_onehalf);

    g_yl = PACK_HIGH16(g_y0, g_y1);
    g_yh = PACK_HIGH16(g_y2, g_y3);
    g_yl = _mm_sub_epi16(g_yl, crl);
    g_yh = _mm_sub_epi16(g_yh, crh);

    for (yloop = 0; yloop < 2 && num_cols > 0; yloop++,
         num_cols -= RGB_PIXELSIZE * 16,
         outptr += RGB_PIXELSIZE * 16, inptr0 += 16) {

      y = VEC_LD(inptr0);
      ye = _mm_and_si128(y, even_mask);
      yo = _mm_srli_epi16(y, 8);

      if (yloop == 0) {
        be = _mm_add_epi16(b_yl, ye);
        bo = _mm_add_epi16(b_yl, yo);
        re = _mm_add_epi16(r_yl, ye);
        ro = _mm_add_epi16(r_yl, yo);
        ge = _mm_add_epi16(g_yl, ye);
        go = _mm_add_epi16(g_yl, yo);
      } else {
        be = _mm_add_epi16(b_yh, ye);
        bo = _mm_add_epi16(b_yh, yo);
        re = _mm_add_epi16(r_yh, ye);
        ro = _mm_add_epi16(r_yh, yo);
        ge = _mm_add_epi16(g_yh, ye);
        go = _mm_add_epi16(g_yh, yo);
      }

      rl = _mm_unpacklo_epi16(re, ro);
      rh = _mm_unpackhi_epi16(re, ro);
      gl = _mm_unpacklo_epi16(ge, go);
      gh = _mm_unpackhi_epi16(ge, go);
      bl = _mm_unpacklo_epi16(be, bo);
      bh = _mm_unpackhi_epi16(be, bo);

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


void jsimd_h2v2_merged_upsample_e2k(JDIMENSION output_width,
                                    JSAMPIMAGE input_buf,
                                    JDIMENSION in_row_group_ctr,
                                    JSAMPARRAY output_buf)
{
  JSAMPROW inptr, outptr;

  inptr = input_buf[0][in_row_group_ctr];
  outptr = output_buf[0];

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2];
  jsimd_h2v1_merged_upsample_e2k(output_width, input_buf, in_row_group_ctr,
                                 output_buf);

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2 + 1];
  output_buf[0] = output_buf[1];
  jsimd_h2v1_merged_upsample_e2k(output_width, input_buf, in_row_group_ctr,
                                 output_buf);

  input_buf[0][in_row_group_ctr] = inptr;
  output_buf[0] = outptr;
}
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#ifdef jsimd_h2v1_merged_upsample_e2k
#undef jsimd_h2v1_merged_upsample_e2k
#undef jsimd_h2v2_merged_upsample_e2k
#endif

