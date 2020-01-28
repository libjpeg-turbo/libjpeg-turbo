/*
 * jdmrgext-neon.c - merged upsampling/color conversion (Arm NEON)
 *
 * Copyright (C) 2020 Arm Limited. All Rights Reserved.
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

/* This file is included by jdmerge-neon.c. */

/*
 * These routines perform simple chroma upsampling - h2v1 or h2v2 - followed by
 * YCbCr -> RGB color conversion all in the same function.
 *
 * As with the standalone functions, YCbCr -> RGB conversion is defined by the
 * following equations:
 *    R = Y                        + 1.40200 * (Cr - 128)
 *    G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)
 *    B = Y + 1.77200 * (Cb - 128)
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.3441467 = 11277 * 2^-15
 *    0.7141418 = 23401 * 2^-15
 *    1.4020386 = 22971 * 2^-14
 *    1.7720337 = 29033 * 2^-14
 * These constants are defined in jdmerge-neon.c.
 *
 * Rounding is used when descaling to ensure correct results.
 */

/*
 * Notes on safe memory access for merged upsampling/YCbCr -> RGB conversion
 * routines:
 *
 * Input memory buffers can be safely overread up to the next multiple of
 * ALIGN_SIZE bytes since they are always allocated by alloc_sarray() in
 * jmemmgr.c.
 *
 * The output buffer cannot safely be written beyond output_width since the
 * TurboJPEG API permits it to be allocated with or without padding up to the
 * next multiple of ALIGN_SIZE bytes.
 */

/*
 * Upsample and color convert from YCbCr -> RGB for the case of 2:1 horizontal.
 */

void jsimd_h2v1_merged_upsample_neon(JDIMENSION output_width,
                                     JSAMPIMAGE input_buf,
                                     JDIMENSION in_row_group_ctr,
                                     JSAMPARRAY output_buf)
{
  JSAMPROW outptr;
  /* Pointers to Y, Cb and Cr data. */
  JSAMPROW inptr0, inptr1, inptr2;

  int16x8_t neg_128 = vdupq_n_s16(-128);

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];

  int cols_remaining = output_width;
  for (; cols_remaining >= 16; cols_remaining -= 16) {
    /* Load Y-values such that even pixel indices are in one vector and odd */
    /* pixel indices are in another vector. */
    uint8x8x2_t y = vld2_u8(inptr0);
    uint8x8_t cb = vld1_u8(inptr1);
    uint8x8_t cr = vld1_u8(inptr2);
    /* Subtract 128 from Cb and Cr. */
    int16x8_t cr_128 = vreinterpretq_s16_u16(
                            vaddw_u8(vreinterpretq_u16_s16(neg_128), cr));
    int16x8_t cb_128 = vreinterpretq_s16_u16(
                            vaddw_u8(vreinterpretq_u16_s16(neg_128), cb));
    /* Compute G-Y: - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) */
    int32x4_t g_sub_y_l = vmull_n_s16(vget_low_s16(cb_128), -F_0_344);
    int32x4_t g_sub_y_h = vmull_n_s16(vget_high_s16(cb_128), -F_0_344);
    g_sub_y_l = vmlsl_n_s16(g_sub_y_l, vget_low_s16(cr_128), F_0_714);
    g_sub_y_h = vmlsl_n_s16(g_sub_y_h, vget_high_s16(cr_128), F_0_714);
    /* Descale G components: shift right 15, round and narrow to 16-bit. */
    int16x8_t g_sub_y = vcombine_s16(vrshrn_n_s32(g_sub_y_l, 15),
                                     vrshrn_n_s32(g_sub_y_h, 15));
    /* Compute R-Y: 1.40200 * (Cr - 128) */
    int16x8_t r_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cr_128, 1), F_1_402);
    /* Compute B-Y: 1.77200 * (Cb - 128) */
    int16x8_t b_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cb_128, 1), F_1_772);
    /* Add Y and duplicate chroma components; upsampling horizontally. */
    int16x8_t g_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y.val[0]));
    int16x8_t r_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y.val[0]));
    int16x8_t b_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y.val[0]));
    int16x8_t g_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y.val[1]));
    int16x8_t r_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y.val[1]));
    int16x8_t b_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y.val[1]));
    /* Convert each component to unsigned and narrow, clamping to [0-255]. */
    /* Interleave pixel channel values having odd and even pixel indices. */
    uint8x8x2_t r = vzip_u8(vqmovun_s16(r_even), vqmovun_s16(r_odd));
    uint8x8x2_t g = vzip_u8(vqmovun_s16(g_even), vqmovun_s16(g_odd));
    uint8x8x2_t b = vzip_u8(vqmovun_s16(b_even), vqmovun_s16(b_odd));

#ifdef RGB_ALPHA
    uint8x16x4_t rgba;
    rgba.val[RGB_RED] = vcombine_u8(r.val[0], r.val[1]);
    rgba.val[RGB_GREEN] = vcombine_u8(g.val[0], g.val[1]);
    rgba.val[RGB_BLUE] = vcombine_u8(b.val[0], b.val[1]);
    /* Set alpha channel to opaque (0xFF). */
    rgba.val[RGB_ALPHA] = vdupq_n_u8(0xFF);
    /* Store RGBA pixel data to memory. */
    vst4q_u8(outptr, rgba);
#else
    uint8x16x3_t rgb;
    rgb.val[RGB_RED] = vcombine_u8(r.val[0], r.val[1]);
    rgb.val[RGB_GREEN] = vcombine_u8(g.val[0], g.val[1]);
    rgb.val[RGB_BLUE] = vcombine_u8(b.val[0], b.val[1]);
    /* Store RGB pixel data to memory. */
    vst3q_u8(outptr, rgb);
#endif

    /* Increment pointers. */
    inptr0 += 16;
    inptr1 += 8;
    inptr2 += 8;
    outptr += (RGB_PIXELSIZE * 16);
  }

  if (cols_remaining > 0) {
    /* Load y-values such that even pixel indices are in one vector and odd */
    /* pixel indices are in another vector. */
    uint8x8x2_t y = vld2_u8(inptr0);
    uint8x8_t cb = vld1_u8(inptr1);
    uint8x8_t cr = vld1_u8(inptr2);
    /* Subtract 128 from Cb and Cr. */
    int16x8_t cr_128 = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(neg_128), cr));
    int16x8_t cb_128 = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(neg_128), cb));
    /* Compute G-Y: - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) */
    int32x4_t g_sub_y_l = vmull_n_s16(vget_low_s16(cb_128), -F_0_344);
    int32x4_t g_sub_y_h = vmull_n_s16(vget_high_s16(cb_128), -F_0_344);
    g_sub_y_l = vmlsl_n_s16(g_sub_y_l, vget_low_s16(cr_128), F_0_714);
    g_sub_y_h = vmlsl_n_s16(g_sub_y_h, vget_high_s16(cr_128), F_0_714);
    /* Descale G components: shift right 15, round and narrow to 16-bit. */
    int16x8_t g_sub_y = vcombine_s16(vrshrn_n_s32(g_sub_y_l, 15),
                                     vrshrn_n_s32(g_sub_y_h, 15));
    /* Compute R-Y: 1.40200 * (Cr - 128) */
    int16x8_t r_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cr_128, 1), F_1_402);
    /* Compute B-Y: 1.77200 * (Cb - 128) */
    int16x8_t b_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cb_128, 1), F_1_772);
    /* Add Y and duplicate chroma components - upsample horizontally. */
    int16x8_t g_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y.val[0]));
    int16x8_t r_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y.val[0]));
    int16x8_t b_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y.val[0]));
    int16x8_t g_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y.val[1]));
    int16x8_t r_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y.val[1]));
    int16x8_t b_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y.val[1]));
    /* Convert each component to unsigned and narrow, clamping to [0-255]. */
    /* Interleave pixel channel values having odd and even pixel indices. */
    uint8x8x2_t r = vzip_u8(vqmovun_s16(r_even), vqmovun_s16(r_odd));
    uint8x8x2_t g = vzip_u8(vqmovun_s16(g_even), vqmovun_s16(g_odd));
    uint8x8x2_t b = vzip_u8(vqmovun_s16(b_even), vqmovun_s16(b_odd));

#ifdef RGB_ALPHA
    uint8x8x4_t rgba_h;
    rgba_h.val[RGB_RED] = r.val[1];
    rgba_h.val[RGB_GREEN] = g.val[1];
    rgba_h.val[RGB_BLUE] = b.val[1];
    /* Set alpha channel to opaque (0xFF). */
    rgba_h.val[RGB_ALPHA] = vdup_n_u8(0xFF);
    uint8x8x4_t rgba_l;
    rgba_l.val[RGB_RED] = r.val[0];
    rgba_l.val[RGB_GREEN] = g.val[0];
    rgba_l.val[RGB_BLUE] = b.val[0];
    /* Set alpha channel to opaque (0xFF). */
    rgba_l.val[RGB_ALPHA] = vdup_n_u8(0xFF);
    /* Store RGBA pixel data to memory. */
    switch (cols_remaining) {
    case 15 :
      vst4_lane_u8(outptr + 14 * RGB_PIXELSIZE, rgba_h, 6);
    case 14 :
      vst4_lane_u8(outptr + 13 * RGB_PIXELSIZE, rgba_h, 5);
    case 13 :
      vst4_lane_u8(outptr + 12 * RGB_PIXELSIZE, rgba_h, 4);
    case 12 :
      vst4_lane_u8(outptr + 11 * RGB_PIXELSIZE, rgba_h, 3);
    case 11 :
      vst4_lane_u8(outptr + 10 * RGB_PIXELSIZE, rgba_h, 2);
    case 10 :
      vst4_lane_u8(outptr + 9 * RGB_PIXELSIZE, rgba_h, 1);
    case 9 :
      vst4_lane_u8(outptr + 8 * RGB_PIXELSIZE, rgba_h, 0);
    case 8 :
      vst4_u8(outptr, rgba_l);
      break;
    case 7 :
      vst4_lane_u8(outptr + 6 * RGB_PIXELSIZE, rgba_l, 6);
    case 6 :
      vst4_lane_u8(outptr + 5 * RGB_PIXELSIZE, rgba_l, 5);
    case 5 :
      vst4_lane_u8(outptr + 4 * RGB_PIXELSIZE, rgba_l, 4);
    case 4 :
      vst4_lane_u8(outptr + 3 * RGB_PIXELSIZE, rgba_l, 3);
    case 3 :
      vst4_lane_u8(outptr + 2 * RGB_PIXELSIZE, rgba_l, 2);
    case 2 :
      vst4_lane_u8(outptr + RGB_PIXELSIZE, rgba_l, 1);
    case 1 :
      vst4_lane_u8(outptr, rgba_l, 0);
    default :
      break;
    }
#else
    uint8x8x3_t rgb_h;
    rgb_h.val[RGB_RED] = r.val[1];
    rgb_h.val[RGB_GREEN] = g.val[1];
    rgb_h.val[RGB_BLUE] = b.val[1];
    uint8x8x3_t rgb_l;
    rgb_l.val[RGB_RED] = r.val[0];
    rgb_l.val[RGB_GREEN] = g.val[0];
    rgb_l.val[RGB_BLUE] = b.val[0];
    /* Store RGB pixel data to memory. */
    switch (cols_remaining) {
    case 15 :
      vst3_lane_u8(outptr + 14 * RGB_PIXELSIZE, rgb_h, 6);
    case 14 :
      vst3_lane_u8(outptr + 13 * RGB_PIXELSIZE, rgb_h, 5);
    case 13 :
      vst3_lane_u8(outptr + 12 * RGB_PIXELSIZE, rgb_h, 4);
    case 12 :
      vst3_lane_u8(outptr + 11 * RGB_PIXELSIZE, rgb_h, 3);
    case 11 :
      vst3_lane_u8(outptr + 10 * RGB_PIXELSIZE, rgb_h, 2);
    case 10 :
      vst3_lane_u8(outptr + 9 * RGB_PIXELSIZE, rgb_h, 1);
    case 9 :
      vst3_lane_u8(outptr + 8 * RGB_PIXELSIZE, rgb_h, 0);
    case 8 :
      vst3_u8(outptr, rgb_l);
      break;
    case 7 :
      vst3_lane_u8(outptr + 6 * RGB_PIXELSIZE, rgb_l, 6);
    case 6 :
      vst3_lane_u8(outptr + 5 * RGB_PIXELSIZE, rgb_l, 5);
    case 5 :
      vst3_lane_u8(outptr + 4 * RGB_PIXELSIZE, rgb_l, 4);
    case 4 :
      vst3_lane_u8(outptr + 3 * RGB_PIXELSIZE, rgb_l, 3);
    case 3 :
      vst3_lane_u8(outptr + 2 * RGB_PIXELSIZE, rgb_l, 2);
    case 2 :
      vst3_lane_u8(outptr + RGB_PIXELSIZE, rgb_l, 1);
    case 1 :
      vst3_lane_u8(outptr, rgb_l, 0);
    default :
      break;
    }
#endif
  }
}


/*
 * Upsample and color convert from YCbCr -> RGB for the case of 2:1 horizontal
 * and 2:1 vertical.
 *
 * See above for details of color conversion and safe memory buffer access.
 */

void jsimd_h2v2_merged_upsample_neon(JDIMENSION output_width,
                                     JSAMPIMAGE input_buf,
                                     JDIMENSION in_row_group_ctr,
                                     JSAMPARRAY output_buf)
{
  JSAMPROW outptr0, outptr1;
  /* Pointers to Y (both rows), Cb and Cr data. */
  JSAMPROW inptr0_0, inptr0_1, inptr1, inptr2;

  int16x8_t neg_128 = vdupq_n_s16(-128);

  inptr0_0 = input_buf[0][in_row_group_ctr * 2];
  inptr0_1 = input_buf[0][in_row_group_ctr * 2 + 1];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr0 = output_buf[0];
  outptr1 = output_buf[1];

  int cols_remaining = output_width;
  for (; cols_remaining >= 16; cols_remaining -= 16) {
    /* Load Y-values such that even pixel indices are in one vector and odd */
    /* pixel indices are in another vector. */
    uint8x8x2_t y0 = vld2_u8(inptr0_0);
    uint8x8x2_t y1 = vld2_u8(inptr0_1);
    uint8x8_t cb = vld1_u8(inptr1);
    uint8x8_t cr = vld1_u8(inptr2);
    /* Subtract 128 from Cb and Cr. */
    int16x8_t cr_128 = vreinterpretq_s16_u16(
                            vaddw_u8(vreinterpretq_u16_s16(neg_128), cr));
    int16x8_t cb_128 = vreinterpretq_s16_u16(
                            vaddw_u8(vreinterpretq_u16_s16(neg_128), cb));
    /* Compute G-Y: - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) */
    int32x4_t g_sub_y_l = vmull_n_s16(vget_low_s16(cb_128), -F_0_344);
    int32x4_t g_sub_y_h = vmull_n_s16(vget_high_s16(cb_128), -F_0_344);
    g_sub_y_l = vmlsl_n_s16(g_sub_y_l, vget_low_s16(cr_128), F_0_714);
    g_sub_y_h = vmlsl_n_s16(g_sub_y_h, vget_high_s16(cr_128), F_0_714);
    /* Descale G components: shift right 15, round and narrow to 16-bit. */
    int16x8_t g_sub_y = vcombine_s16(vrshrn_n_s32(g_sub_y_l, 15),
                                     vrshrn_n_s32(g_sub_y_h, 15));
    /* Compute R-Y: 1.40200 * (Cr - 128) */
    int16x8_t r_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cr_128, 1), F_1_402);
    /* Compute B-Y: 1.77200 * (Cb - 128) */
    int16x8_t b_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cb_128, 1), F_1_772);
    /* Add Y and duplicate chroma components - upsample horizontally. */
    int16x8_t g0_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y0.val[0]));
    int16x8_t r0_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y0.val[0]));
    int16x8_t b0_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y0.val[0]));
    int16x8_t g0_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y0.val[1]));
    int16x8_t r0_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y0.val[1]));
    int16x8_t b0_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y0.val[1]));
    int16x8_t g1_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y1.val[0]));
    int16x8_t r1_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y1.val[0]));
    int16x8_t b1_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y1.val[0]));
    int16x8_t g1_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y1.val[1]));
    int16x8_t r1_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y1.val[1]));
    int16x8_t b1_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y1.val[1]));
    /* Convert each component to unsigned and narrow, clamping to [0-255]. */
    /* Interleave pixel channel values having odd and even pixel indices. */
    uint8x8x2_t r0 = vzip_u8(vqmovun_s16(r0_even), vqmovun_s16(r0_odd));
    uint8x8x2_t r1 = vzip_u8(vqmovun_s16(r1_even), vqmovun_s16(r1_odd));
    uint8x8x2_t g0 = vzip_u8(vqmovun_s16(g0_even), vqmovun_s16(g0_odd));
    uint8x8x2_t g1 = vzip_u8(vqmovun_s16(g1_even), vqmovun_s16(g1_odd));
    uint8x8x2_t b0 = vzip_u8(vqmovun_s16(b0_even), vqmovun_s16(b0_odd));
    uint8x8x2_t b1 = vzip_u8(vqmovun_s16(b1_even), vqmovun_s16(b1_odd));

#ifdef RGB_ALPHA
    uint8x16x4_t rgba0, rgba1;
    rgba0.val[RGB_RED] = vcombine_u8(r0.val[0], r0.val[1]);
    rgba1.val[RGB_RED] = vcombine_u8(r1.val[0], r1.val[1]);
    rgba0.val[RGB_GREEN] = vcombine_u8(g0.val[0], g0.val[1]);
    rgba1.val[RGB_GREEN] = vcombine_u8(g1.val[0], g1.val[1]);
    rgba0.val[RGB_BLUE] = vcombine_u8(b0.val[0], b0.val[1]);
    rgba1.val[RGB_BLUE] = vcombine_u8(b1.val[0], b1.val[1]);
    /* Set alpha channel to opaque (0xFF). */
    rgba0.val[RGB_ALPHA] = vdupq_n_u8(0xFF);
    rgba1.val[RGB_ALPHA] = vdupq_n_u8(0xFF);
    /* Store RGBA pixel data to memory. */
    vst4q_u8(outptr0, rgba0);
    vst4q_u8(outptr1, rgba1);
#else
    uint8x16x3_t rgb0, rgb1;
    rgb0.val[RGB_RED] = vcombine_u8(r0.val[0], r0.val[1]);
    rgb1.val[RGB_RED] = vcombine_u8(r1.val[0], r1.val[1]);
    rgb0.val[RGB_GREEN] = vcombine_u8(g0.val[0], g0.val[1]);
    rgb1.val[RGB_GREEN] = vcombine_u8(g1.val[0], g1.val[1]);
    rgb0.val[RGB_BLUE] = vcombine_u8(b0.val[0], b0.val[1]);
    rgb1.val[RGB_BLUE] = vcombine_u8(b1.val[0], b1.val[1]);
    /* Store RGB pixel data to memory. */
    vst3q_u8(outptr0, rgb0);
    vst3q_u8(outptr1, rgb1);
#endif

    /* Increment pointers. */
    inptr0_0 += 16;
    inptr0_1 += 16;
    inptr1 += 8;
    inptr2 += 8;
    outptr0 += (RGB_PIXELSIZE * 16);
    outptr1 += (RGB_PIXELSIZE * 16);
  }

  if (cols_remaining > 0) {
    /* Load Y-values such that even pixel indices are in one vector and */
    /* odd pixel indices are in another vector. */
    uint8x8x2_t y0 = vld2_u8(inptr0_0);
    uint8x8x2_t y1 = vld2_u8(inptr0_1);
    uint8x8_t cb = vld1_u8(inptr1);
    uint8x8_t cr = vld1_u8(inptr2);
    /* Subtract 128 from Cb and Cr. */
    int16x8_t cr_128 = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(neg_128), cr));
    int16x8_t cb_128 = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(neg_128), cb));
    /* Compute G-Y: - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) */
    int32x4_t g_sub_y_l = vmull_n_s16(vget_low_s16(cb_128), -F_0_344);
    int32x4_t g_sub_y_h = vmull_n_s16(vget_high_s16(cb_128), -F_0_344);
    g_sub_y_l = vmlsl_n_s16(g_sub_y_l, vget_low_s16(cr_128), F_0_714);
    g_sub_y_h = vmlsl_n_s16(g_sub_y_h, vget_high_s16(cr_128), F_0_714);
    /* Descale G components: shift right 15, round and narrow to 16-bit. */
    int16x8_t g_sub_y = vcombine_s16(vrshrn_n_s32(g_sub_y_l, 15),
                                     vrshrn_n_s32(g_sub_y_h, 15));
    /* Compute R-Y: 1.40200 * (Cr - 128) */
    int16x8_t r_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cr_128, 1), F_1_402);
    /* Compute B-Y: 1.77200 * (Cb - 128) */
    int16x8_t b_sub_y = vqrdmulhq_n_s16(vshlq_n_s16(cb_128, 1), F_1_772);
    /* Add Y and duplicate chroma components - upsample horizontally. */
    int16x8_t g0_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y0.val[0]));
    int16x8_t r0_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y0.val[0]));
    int16x8_t b0_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y0.val[0]));
    int16x8_t g0_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y0.val[1]));
    int16x8_t r0_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y0.val[1]));
    int16x8_t b0_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y0.val[1]));
    int16x8_t g1_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y1.val[0]));
    int16x8_t r1_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y1.val[0]));
    int16x8_t b1_even = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y1.val[0]));
    int16x8_t g1_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(g_sub_y), y1.val[1]));
    int16x8_t r1_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(r_sub_y), y1.val[1]));
    int16x8_t b1_odd = vreinterpretq_s16_u16(
                          vaddw_u8(vreinterpretq_u16_s16(b_sub_y), y1.val[1]));
    /* Convert each component to unsigned and narrow, clamping to [0-255]. */
    /* Interleave pixel channel values having odd and even pixel indices. */
    uint8x8x2_t r0 = vzip_u8(vqmovun_s16(r0_even), vqmovun_s16(r0_odd));
    uint8x8x2_t r1 = vzip_u8(vqmovun_s16(r1_even), vqmovun_s16(r1_odd));
    uint8x8x2_t g0 = vzip_u8(vqmovun_s16(g0_even), vqmovun_s16(g0_odd));
    uint8x8x2_t g1 = vzip_u8(vqmovun_s16(g1_even), vqmovun_s16(g1_odd));
    uint8x8x2_t b0 = vzip_u8(vqmovun_s16(b0_even), vqmovun_s16(b0_odd));
    uint8x8x2_t b1 = vzip_u8(vqmovun_s16(b1_even), vqmovun_s16(b1_odd));

#ifdef RGB_ALPHA
    uint8x8x4_t rgba0_h, rgba1_h;
    rgba0_h.val[RGB_RED] = r0.val[1];
    rgba1_h.val[RGB_RED] = r1.val[1];
    rgba0_h.val[RGB_GREEN] = g0.val[1];
    rgba1_h.val[RGB_GREEN] = g1.val[1];
    rgba0_h.val[RGB_BLUE] = b0.val[1];
    rgba1_h.val[RGB_BLUE] = b1.val[1];
    /* Set alpha channel to opaque (0xFF). */
    rgba0_h.val[RGB_ALPHA] = vdup_n_u8(0xFF);
    rgba1_h.val[RGB_ALPHA] = vdup_n_u8(0xFF);

    uint8x8x4_t rgba0_l, rgba1_l;
    rgba0_l.val[RGB_RED] = r0.val[0];
    rgba1_l.val[RGB_RED] = r1.val[0];
    rgba0_l.val[RGB_GREEN] = g0.val[0];
    rgba1_l.val[RGB_GREEN] = g1.val[0];
    rgba0_l.val[RGB_BLUE] = b0.val[0];
    rgba1_l.val[RGB_BLUE] = b1.val[0];
    /* Set alpha channel to opaque (0xFF). */
    rgba0_l.val[RGB_ALPHA] = vdup_n_u8(0xFF);
    rgba1_l.val[RGB_ALPHA] = vdup_n_u8(0xFF);
    /* Store RGBA pixel data to memory. */
    switch (cols_remaining) {
    case 15 :
      vst4_lane_u8(outptr0 + 14 * RGB_PIXELSIZE, rgba0_h, 6);
      vst4_lane_u8(outptr1 + 14 * RGB_PIXELSIZE, rgba1_h, 6);
    case 14 :
      vst4_lane_u8(outptr0 + 13 * RGB_PIXELSIZE, rgba0_h, 5);
      vst4_lane_u8(outptr1 + 13 * RGB_PIXELSIZE, rgba1_h, 5);
    case 13 :
      vst4_lane_u8(outptr0 + 12 * RGB_PIXELSIZE, rgba0_h, 4);
      vst4_lane_u8(outptr1 + 12 * RGB_PIXELSIZE, rgba1_h, 4);
    case 12 :
      vst4_lane_u8(outptr0 + 11 * RGB_PIXELSIZE, rgba0_h, 3);
      vst4_lane_u8(outptr1 + 11 * RGB_PIXELSIZE, rgba1_h, 3);
    case 11 :
      vst4_lane_u8(outptr0 + 10 * RGB_PIXELSIZE, rgba0_h, 2);
      vst4_lane_u8(outptr1 + 10 * RGB_PIXELSIZE, rgba1_h, 2);
    case 10 :
      vst4_lane_u8(outptr0 + 9 * RGB_PIXELSIZE, rgba0_h, 1);
      vst4_lane_u8(outptr1 + 9 * RGB_PIXELSIZE, rgba1_h, 1);
    case 9 :
      vst4_lane_u8(outptr0 + 8 * RGB_PIXELSIZE, rgba0_h, 0);
      vst4_lane_u8(outptr1 + 8 * RGB_PIXELSIZE, rgba1_h, 0);
    case 8 :
      vst4_u8(outptr0, rgba0_l);
      vst4_u8(outptr1, rgba1_l);
      break;
    case 7 :
      vst4_lane_u8(outptr0 + 6 * RGB_PIXELSIZE, rgba0_l, 6);
      vst4_lane_u8(outptr1 + 6 * RGB_PIXELSIZE, rgba1_l, 6);
    case 6 :
      vst4_lane_u8(outptr0 + 5 * RGB_PIXELSIZE, rgba0_l, 5);
      vst4_lane_u8(outptr1 + 5 * RGB_PIXELSIZE, rgba1_l, 5);
    case 5 :
      vst4_lane_u8(outptr0 + 4 * RGB_PIXELSIZE, rgba0_l, 4);
      vst4_lane_u8(outptr1 + 4 * RGB_PIXELSIZE, rgba1_l, 4);
    case 4 :
      vst4_lane_u8(outptr0 + 3 * RGB_PIXELSIZE, rgba0_l, 3);
      vst4_lane_u8(outptr1 + 3 * RGB_PIXELSIZE, rgba1_l, 3);
    case 3 :
      vst4_lane_u8(outptr0 + 2 * RGB_PIXELSIZE, rgba0_l, 2);
      vst4_lane_u8(outptr1 + 2 * RGB_PIXELSIZE, rgba1_l, 2);
    case 2 :
      vst4_lane_u8(outptr0 + 1 * RGB_PIXELSIZE, rgba0_l, 1);
      vst4_lane_u8(outptr1 + 1 * RGB_PIXELSIZE, rgba1_l, 1);
    case 1 :
      vst4_lane_u8(outptr0, rgba0_l, 0);
      vst4_lane_u8(outptr1, rgba1_l, 0);
    default :
      break;
    }
#else
    uint8x8x3_t rgb0_h, rgb1_h;
    rgb0_h.val[RGB_RED] = r0.val[1];
    rgb1_h.val[RGB_RED] = r1.val[1];
    rgb0_h.val[RGB_GREEN] = g0.val[1];
    rgb1_h.val[RGB_GREEN] = g1.val[1];
    rgb0_h.val[RGB_BLUE] = b0.val[1];
    rgb1_h.val[RGB_BLUE] = b1.val[1];

    uint8x8x3_t rgb0_l, rgb1_l;
    rgb0_l.val[RGB_RED] = r0.val[0];
    rgb1_l.val[RGB_RED] = r1.val[0];
    rgb0_l.val[RGB_GREEN] = g0.val[0];
    rgb1_l.val[RGB_GREEN] = g1.val[0];
    rgb0_l.val[RGB_BLUE] = b0.val[0];
    rgb1_l.val[RGB_BLUE] = b1.val[0];
    /* Store RGB pixel data to memory. */
    switch (cols_remaining) {
    case 15 :
      vst3_lane_u8(outptr0 + 14 * RGB_PIXELSIZE, rgb0_h, 6);
      vst3_lane_u8(outptr1 + 14 * RGB_PIXELSIZE, rgb1_h, 6);
    case 14 :
      vst3_lane_u8(outptr0 + 13 * RGB_PIXELSIZE, rgb0_h, 5);
      vst3_lane_u8(outptr1 + 13 * RGB_PIXELSIZE, rgb1_h, 5);
    case 13 :
      vst3_lane_u8(outptr0 + 12 * RGB_PIXELSIZE, rgb0_h, 4);
      vst3_lane_u8(outptr1 + 12 * RGB_PIXELSIZE, rgb1_h, 4);
    case 12 :
      vst3_lane_u8(outptr0 + 11 * RGB_PIXELSIZE, rgb0_h, 3);
      vst3_lane_u8(outptr1 + 11 * RGB_PIXELSIZE, rgb1_h, 3);
    case 11 :
      vst3_lane_u8(outptr0 + 10 * RGB_PIXELSIZE, rgb0_h, 2);
      vst3_lane_u8(outptr1 + 10 * RGB_PIXELSIZE, rgb1_h, 2);
    case 10 :
      vst3_lane_u8(outptr0 + 9 * RGB_PIXELSIZE, rgb0_h, 1);
      vst3_lane_u8(outptr1 + 9 * RGB_PIXELSIZE, rgb1_h, 1);
    case 9 :
      vst3_lane_u8(outptr0 + 8 * RGB_PIXELSIZE, rgb0_h, 0);
      vst3_lane_u8(outptr1 + 8 * RGB_PIXELSIZE, rgb1_h, 0);
    case 8 :
      vst3_u8(outptr0, rgb0_l);
      vst3_u8(outptr1, rgb1_l);
      break;
    case 7 :
      vst3_lane_u8(outptr0 + 6 * RGB_PIXELSIZE, rgb0_l, 6);
      vst3_lane_u8(outptr1 + 6 * RGB_PIXELSIZE, rgb1_l, 6);
    case 6 :
      vst3_lane_u8(outptr0 + 5 * RGB_PIXELSIZE, rgb0_l, 5);
      vst3_lane_u8(outptr1 + 5 * RGB_PIXELSIZE, rgb1_l, 5);
    case 5 :
      vst3_lane_u8(outptr0 + 4 * RGB_PIXELSIZE, rgb0_l, 4);
      vst3_lane_u8(outptr1 + 4 * RGB_PIXELSIZE, rgb1_l, 4);
    case 4 :
      vst3_lane_u8(outptr0 + 3 * RGB_PIXELSIZE, rgb0_l, 3);
      vst3_lane_u8(outptr1 + 3 * RGB_PIXELSIZE, rgb1_l, 3);
    case 3 :
      vst3_lane_u8(outptr0 + 2 * RGB_PIXELSIZE, rgb0_l, 2);
      vst3_lane_u8(outptr1 + 2 * RGB_PIXELSIZE, rgb1_l, 2);
    case 2 :
      vst3_lane_u8(outptr0 + 1 * RGB_PIXELSIZE, rgb0_l, 1);
      vst3_lane_u8(outptr1 + 1 * RGB_PIXELSIZE, rgb1_l, 1);
    case 1 :
      vst3_lane_u8(outptr0, rgb0_l, 0);
      vst3_lane_u8(outptr1, rgb1_l, 0);
    default :
      break;
    }
#endif
  }
}
