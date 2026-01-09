/*
 * Colorspace conversion (64-bit Arm SVE2)
 *
 * Copyright (C) 2020, Arm Limited.  All Rights Reserved.
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

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/* This file is included by jccolor-sve2.c */


/* RGB -> YCbCr conversion is defined by the following equations:
 *    Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *    Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + 128
 *    Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + 128
 *
 * Avoid floating point arithmetic by using shifted integer constants:
 *    0.29899597 = 19595 * 2^-16
 *    0.58700561 = 38470 * 2^-16
 *    0.11399841 =  7471 * 2^-16
 *    0.16874695 = 11059 * 2^-16
 *    0.33125305 = 21709 * 2^-16
 *    0.50000000 = 32768 * 2^-16
 *    0.41868592 = 27439 * 2^-16
 *    0.08131409 =  5329 * 2^-16
 * These constants are defined in jccolor-neon.c
 *
 * We add the fixed-point equivalent of 0.5 to Cb and Cr, which effectively
 * rounds up or down the result via integer truncation.
 */

#ifdef STREAMING_SVE
__arm_locally_streaming
#endif
void jsimd_rgb_ycc_convert_sve2(JDIMENSION image_width, JSAMPARRAY input_buf,
                                JSAMPIMAGE output_buf, JDIMENSION output_row,
                                int num_rows)
{
  /* Pointer to RGB(X/A) input data */
  JSAMPROW inptr;
  /* Pointers to Y, Cb, and Cr output data */
  JSAMPROW outptr0, outptr1, outptr2;

  /* Set up conversion constants. */
  // We need to make sure the constants are present in each quadword
  // due to how SVE2 handles lane-based instructions. Duplicate across quadwords.
  svbool_t Pg = svwhilelt_b16(0, 8);
  const svuint16_t consts = svdupq_lane(svld1(Pg, jsimd_rgb_ycc_neon_consts), 0);
  const svuint32_t scaled_128_5 = svdup_u32((128 << 16) + 32767);

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;

    for (int cols = 0; cols < image_width; cols += svcntb()) {
      Pg = svwhilelt_b8_s32(cols, image_width);

#if RGB_PIXELSIZE == 4
      svuint8x4_t input_pixels = svld4(Pg, inptr);
      svuint16_t r_b = svmovlb(svget4(input_pixels, RGB_RED));
      svuint16_t g_b = svmovlb(svget4(input_pixels, RGB_GREEN));
      svuint16_t b_b = svmovlb(svget4(input_pixels, RGB_BLUE));
      svuint16_t r_t = svmovlt(svget4(input_pixels, RGB_RED));
      svuint16_t g_t = svmovlt(svget4(input_pixels, RGB_GREEN));
      svuint16_t b_t = svmovlt(svget4(input_pixels, RGB_BLUE));
#else
      svuint8x3_t input_pixels = svld3(Pg, inptr);
      svuint16_t r_b = svmovlb(svget3(input_pixels, RGB_RED));
      svuint16_t g_b = svmovlb(svget3(input_pixels, RGB_GREEN));
      svuint16_t b_b = svmovlb(svget3(input_pixels, RGB_BLUE));
      svuint16_t r_t = svmovlt(svget3(input_pixels, RGB_RED));
      svuint16_t g_t = svmovlt(svget3(input_pixels, RGB_GREEN));
      svuint16_t b_t = svmovlt(svget3(input_pixels, RGB_BLUE));
#endif
      /* Compute Y = 0.29900 * R + 0.58700 * G + 0.11400 * B */
      svuint32_t y_bb = svmullb_lane(r_b, consts, 0);
      y_bb = svmlalb_lane(y_bb, g_b, consts, 1);
      y_bb = svmlalb_lane(y_bb, b_b, consts, 2);
      svuint32_t y_bt = svmullt_lane(r_b, consts, 0);
      y_bt = svmlalt_lane(y_bt, g_b, consts, 1);
      y_bt = svmlalt_lane(y_bt, b_b, consts, 2);
      svuint32_t y_tb = svmullb_lane(r_t, consts, 0);
      y_tb = svmlalb_lane(y_tb, g_t, consts, 1);
      y_tb = svmlalb_lane(y_tb, b_t, consts, 2);
      svuint32_t y_tt = svmullt_lane(r_t, consts, 0);
      y_tt = svmlalt_lane(y_tt, g_t, consts, 1);
      y_tt = svmlalt_lane(y_tt, b_t, consts, 2);

      /* Compute Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + 128 */
      svuint32_t cb_bb = scaled_128_5;
      cb_bb = svmlslb_lane(cb_bb, r_b, consts, 3);
      cb_bb = svmlslb_lane(cb_bb, g_b, consts, 4);
      cb_bb = svmlalb_lane(cb_bb, b_b, consts, 5);
      svuint32_t cb_bt = scaled_128_5;
      cb_bt = svmlslt_lane(cb_bt, r_b, consts, 3);
      cb_bt = svmlslt_lane(cb_bt, g_b, consts, 4);
      cb_bt = svmlalt_lane(cb_bt, b_b, consts, 5);
      svuint32_t cb_tb = scaled_128_5;
      cb_tb = svmlslb_lane(cb_tb, r_t, consts, 3);
      cb_tb = svmlslb_lane(cb_tb, g_t, consts, 4);
      cb_tb = svmlalb_lane(cb_tb, b_t, consts, 5);
      svuint32_t cb_tt = scaled_128_5;
      cb_tt = svmlslt_lane(cb_tt, r_t, consts, 3);
      cb_tt = svmlslt_lane(cb_tt, g_t, consts, 4);
      cb_tt = svmlalt_lane(cb_tt, b_t, consts, 5);

      /* Compute Cr = 0.50000 * R - 0.41869 * G - 0.08131 * B  + 128 */
      svuint32_t cr_bb = scaled_128_5;
      cr_bb = svmlalb_lane(cr_bb, r_b, consts, 5);
      cr_bb = svmlslb_lane(cr_bb, g_b, consts, 6);
      cr_bb = svmlslb_lane(cr_bb, b_b, consts, 7);
      svuint32_t cr_bt = scaled_128_5;
      cr_bt = svmlalt_lane(cr_bt, r_b, consts, 5);
      cr_bt = svmlslt_lane(cr_bt, g_b, consts, 6);
      cr_bt = svmlslt_lane(cr_bt, b_b, consts, 7);
      svuint32_t cr_tb = scaled_128_5;
      cr_tb = svmlalb_lane(cr_tb, r_t, consts, 5);
      cr_tb = svmlslb_lane(cr_tb, g_t, consts, 6);
      cr_tb = svmlslb_lane(cr_tb, b_t, consts, 7);
      svuint32_t cr_tt = scaled_128_5;
      cr_tt = svmlalt_lane(cr_tt, r_t, consts, 5);
      cr_tt = svmlslt_lane(cr_tt, g_t, consts, 6);
      cr_tt = svmlslt_lane(cr_tt, b_t, consts, 7);

      /* Descale Y values (rounding right shift) and narrow to 16-bit. */
      svuint16_t y_b = svrshrnb(y_bb, 16);
      y_b = svrshrnt(y_b, y_bt, 16);
      svuint16_t y_t = svrshrnb(y_tb, 16);
      y_t = svrshrnt(y_t, y_tt, 16);
      /* Descale Cb values (right shift) and narrow to 16-bit. */
      svuint16_t cb_b = svshrnb(cb_bb, 16);
      cb_b = svshrnt(cb_b, cb_bt, 16);
      svuint16_t cb_t = svshrnb(cb_tb, 16);
      cb_t = svshrnt(cb_t, cb_tt, 16);
      /* Descale Cr values (right shift) and narrow to 16-bit. */
      svuint16_t cr_b = svshrnb(cr_bb, 16);
      cr_b = svshrnt(cr_b, cr_bt, 16);
      svuint16_t cr_t = svshrnb(cr_tb, 16);
      cr_t = svshrnt(cr_t, cr_tt, 16);
      /* Narrow Y, Cb, and Cr values to 8-bit and store to memory.  Buffer
       * overwrite is permitted up to the next multiple of ALIGN_SIZE bytes.
       */
      svuint8_t y = svqxtnb(y_b);
      y = svqxtnt(y, y_t);
      svuint8_t cb = svqxtnb(cb_b);
      cb = svqxtnt(cb, cb_t);
      svuint8_t cr = svqxtnb(cr_b);
      cr = svqxtnt(cr, cr_t);
      svst1(Pg, outptr0, y);
      svst1(Pg, outptr1, cb);
      svst1(Pg, outptr2, cr);

      /* Increment pointers. */
      inptr += (svcntb() * RGB_PIXELSIZE);
      outptr0 += svcntb();
      outptr1 += svcntb();
      outptr2 += svcntb();
    }
  }
}
