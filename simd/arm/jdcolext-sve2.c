/*
 * Colorspace conversion (Arm SVE2)
 *
 * Copyright (C) 2020, Arm Limited.  All Rights Reserved.
 * Copyright (C) 2020, D. R. Commander.  All Rights Reserved.
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

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/* This file is included by jdcolor-sve2.c. */


/* YCbCr -> RGB conversion is defined by the following equations:
 *    R = Y                        + 1.40200 * (Cr - 128)
 *    G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)
 *    B = Y + 1.77200 * (Cb - 128)
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.3441467 = 11277 * 2^-15
 *    0.7141418 = 23401 * 2^-15
 *    1.4020386 = 22971 * 2^-14
 *    1.7720337 = 29033 * 2^-14
 * These constants are defined in jdcolor-sve2.c.
 *
 * To ensure correct results, rounding is used when descaling.
 */

/* Notes on safe memory access for YCbCr -> RGB conversion routines:
 *
 * Input memory buffers can be safely overread up to the next multiple of
 * ALIGN_SIZE bytes, since they are always allocated by alloc_sarray() in
 * jmemmgr.c.
 *
 * The output buffer cannot safely be written beyond output_width, since
 * output_buf points to a possibly unpadded row in the decompressed image
 * buffer allocated by the calling program.
 */
#ifdef STREAMING_SVE
__arm_locally_streaming
#endif
void jsimd_ycc_rgb_convert_sve2(JDIMENSION output_width, JSAMPIMAGE input_buf,
                                JDIMENSION input_row, JSAMPARRAY output_buf,
                                int num_rows)
{
  JSAMPROW outptr;
  /* Pointers to Y, Cb, and Cr data */
  JSAMPROW inptr0, inptr1, inptr2;

  svbool_t Pg = svwhilelt_b16(0, 4);
  // We need to make sure the constants are present in each quadword
  // due to how SVE2 handles lane-based instructions. Duplicate across quadwords.
  const svint16_t consts = svdupq_lane(svld1_s16(Pg, jsimd_ycc_rgb_convert_neon_consts), 0);
  const svint16_t neg_128 = svdup_n_s16(-128);

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;

    // Process however many bytes the SVL will allow us at a time.
    for (unsigned cols = 0; cols < output_width; cols += svcntb()) {
      Pg = svwhilelt_b8_u32(cols, output_width);
      svuint8_t y = svld1(Pg, inptr0);
      svuint8_t cb = svld1(Pg, inptr1);
      svuint8_t cr = svld1(Pg, inptr2);

      /* Subtract 128 from Cb and Cr. */
      svint16_t cr_128_b = svreinterpret_s16_u16(svaddwb(svreinterpret_u16_s16(neg_128), cr));
      svint16_t cr_128_t = svreinterpret_s16_u16(svaddwt(svreinterpret_u16_s16(neg_128), cr));

      svint16_t cb_128_b = svreinterpret_s16_u16(svaddwb(svreinterpret_u16_s16(neg_128), cb));
      svint16_t cb_128_t = svreinterpret_s16_u16(svaddwt(svreinterpret_u16_s16(neg_128), cb));

      /* Compute G-Y: 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) */
      svint32_t g_sub_y_bb = svmullb_lane(cb_128_b, consts, 0);
      svint32_t g_sub_y_bt = svmullt_lane(cb_128_b, consts, 0);
      svint32_t g_sub_y_tb = svmullb_lane(cb_128_t, consts, 0);
      svint32_t g_sub_y_tt = svmullt_lane(cb_128_t, consts, 0);
      g_sub_y_bb = svmlslb_lane(g_sub_y_bb, cr_128_b, consts, 1);
      g_sub_y_bt = svmlslt_lane(g_sub_y_bt, cr_128_b, consts, 1);
      g_sub_y_tb = svmlslb_lane(g_sub_y_tb, cr_128_t, consts, 1);
      g_sub_y_tt = svmlslt_lane(g_sub_y_tt, cr_128_t, consts, 1);

      /* Descale G components: shift right 15, round, and narrow to 16-bit. */
      svint16_t g_sub_y_b = svrshrnb(g_sub_y_bb, 15);
      g_sub_y_b = svrshrnt(g_sub_y_b, g_sub_y_bt, 15);

      svint16_t g_sub_y_t = svrshrnb(g_sub_y_tb, 15);
      g_sub_y_t = svrshrnt(g_sub_y_t, g_sub_y_tt, 15);

      // /* Compute R-Y: 1.40200 * (Cr - 128) */
      // svint16_t r_sub_y_b = svqrdmulh_lane(svlsl_x(svptrue_b16(), cr_128_b, 1), consts, 2);
      // svint16_t r_sub_y_t = svqrdmulh_lane(svlsl_x(svptrue_b16(), cr_128_t, 1), consts, 2);
      //
      // /* Compute B-Y: 1.77200 * (Cb - 128) */
      // svint16_t b_sub_y_b = svqrdmulh_lane(svlsl_x(svptrue_b16(), cb_128_b, 1), consts, 3);
      // svint16_t b_sub_y_t = svqrdmulh_lane(svlsl_x(svptrue_b16(), cb_128_t, 1), consts, 3);

      /* Compute R-Y: 1.40200 * (Cr - 128) */
      svint16_t r_sub_y_b = svqrdmulh_lane(svadd_x(svptrue_b16(), cr_128_b, cr_128_b), consts, 2);
      svint16_t r_sub_y_t = svqrdmulh_lane(svadd_x(svptrue_b16(), cr_128_t, cr_128_t), consts, 2);

      /* Compute B-Y: 1.77200 * (Cb - 128) */
      svint16_t b_sub_y_b = svqrdmulh_lane(svadd_x(svptrue_b16(), cb_128_b, cb_128_b), consts, 3);
      svint16_t b_sub_y_t = svqrdmulh_lane(svadd_x(svptrue_b16(), cb_128_t, cb_128_t), consts, 3);

      /* Add Y. */
      svint16_t r_b = svreinterpret_s16_u16(svaddwb(svreinterpret_u16_s16(r_sub_y_b), y));
      svint16_t r_t = svreinterpret_s16_u16(svaddwt(svreinterpret_u16_s16(r_sub_y_t), y));

      svint16_t b_b = svreinterpret_s16_u16(svaddwb(svreinterpret_u16_s16(b_sub_y_b), y));
      svint16_t b_t = svreinterpret_s16_u16(svaddwt(svreinterpret_u16_s16(b_sub_y_t), y));

      svint16_t g_b = svreinterpret_s16_u16(svaddwb(svreinterpret_u16_s16(g_sub_y_b), y));
      svint16_t g_t = svreinterpret_s16_u16(svaddwt(svreinterpret_u16_s16(g_sub_y_t), y));

#if RGB_PIXELSIZE == 4
      /* Convert each component to unsigned and narrow, clamping to [0-255]. */
      svuint8x4_t rgba = svundef4_u8();
      svuint8_t r = svqxtunb(r_b);
      r = svqxtunt(r, r_t);
      rgba = svset4(rgba, RGB_RED, r);
      svuint8_t g = svqxtunb(g_b);
      g = svqxtunt(g, g_t);
      rgba = svset4(rgba, RGB_GREEN, g);
      svuint8_t b = svqxtunb(b_b);
      b = svqxtunt(b, b_t);
      rgba = svset4(rgba, RGB_BLUE, b);
      svuint8_t a = svdup_u8(0xFF);
      rgba = svset4(rgba, RGB_ALPHA, a);
      svst4(Pg, outptr, rgba);
#elif RGB_PIXELSIZE == 3
      /* Convert each component to unsigned and narrow, clamping to [0-255]. */
      svuint8x3_t rgb = svundef3_u8();
      svuint8_t r = svqxtunb(r_b);
      r = svqxtunt(r, r_t);
      rgb = svset3(rgb, RGB_RED, r);
      svuint8_t g = svqxtunb(g_b);
      g = svqxtunt(g, g_t);
      rgb = svset3(rgb, RGB_GREEN, g);
      svuint8_t b = svqxtunb(b_b);
      b = svqxtunt(b, b_t);
      rgb = svset3(rgb, RGB_BLUE, b);
      svst3(Pg, outptr, rgb);
#else
      svuint16_t rgb565_b = svqshlu_x(svptrue_b16(), r_b, 8);
      rgb565_b = svsri(rgb565_b, svqshlu_x(svptrue_b16(), g_b, 8), 5);
      rgb565_b = svsri(rgb565_b, svqshlu_x(svptrue_b16(), b_b, 8), 11);
      svuint16_t rgb565_t = svqshlu_x(svptrue_b16(), r_t, 8);
      rgb565_t = svsri(rgb565_t, svqshlu_x(svptrue_b16(), g_t, 8), 5);
      rgb565_t = svsri(rgb565_t, svqshlu_x(svptrue_b16(), b_t, 8), 11);
      // Merge and store.
      svuint16_t rgb565_l = svzip1(rgb565_b, rgb565_t);
      svuint16_t rgb565_h = svzip2(rgb565_b, rgb565_t);
      // Need to split predicate since it is based on 8-bit elements.
      svbool_t Pg_lo = svunpklo(Pg), Pg_hi = svunpkhi(Pg);
      svst1(Pg_lo, (uint16_t *) outptr, rgb565_l);
      svst1(Pg_hi, (uint16_t *) outptr + svcnth(), rgb565_h);
#endif
      /* Increment pointers. */
      inptr0 += svcntb();
      inptr1 += svcntb();
      inptr2 += svcntb();
      outptr += (RGB_PIXELSIZE * svcntb());
    }
  }
}
