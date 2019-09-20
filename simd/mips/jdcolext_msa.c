/*
 * MIPS MSA optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * All rights reserved.
 * Authors: Vikram Dattu (vikram.dattu@imgtec.com)
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

/* YCC --> RGB CONVERSION */

#if RGB_RED == 0
#define var_a  out_r0
#define var_b  out_b0
#elif RGB_RED == 1 || RGB_RED == 3
#define var_a  out_g0
#define var_b  alpha
#else /* RGB_RED == 2 */
#define var_a  out_b0
#define var_b  out_r0
#endif

#if RGB_PIXELSIZE == 4
#if RGB_RED == 0 || RGB_RED == 2
#define var_c  out_g0
#define var_d  alpha
#define var_e  out_rgb0
#define var_f  out_rgb1
#define var_g  out_ba0
#define var_h  out_ba1
#else /* RGB_RED == 1 || RGB_RED == 3 */
#define var_e  out_ba0
#define var_f  out_ba1
#define var_g  out_rgb0
#define var_h  out_rgb1
#if RGB_RED == 1
#define var_c  out_b0
#define var_d  out_r0
#else /* RGB_RED == 3 */
#define var_c  out_r0
#define var_d  out_b0
#endif
#endif
#endif

GLOBAL(void)
jsimd_ycc_rgb_convert_msa(JDIMENSION out_width,
                          JSAMPIMAGE input_buf, JDIMENSION input_row,
                          JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW p_in_y, p_in_cb, p_in_cr;
  register JSAMPROW p_rgb;
  int y, cb, cr;
  JDIMENSION col, remaining_wd, num_cols_mul_16 = out_width >> 4;
  v16u8 out0, out1, out2, input_y = { 0 };
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, const_128 = __msa_ldi_b(128);
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = { 0 };
  v16i8 out_r0, out_g0, out_b0;
#if RGB_PIXELSIZE == 3
  v16u8 tmp0, tmp1;
  const v16u8 mask_rgb0 =
    { 0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10 };
  const v16u8 mask_rgb1 =
    { 11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5 };
  const v16u8 mask_rgb2 =
    { 26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31 };
#else /* RGB_PIXELSIZE == 4 */
  v16u8 out3;
  v16i8 out_ba0, out_ba1, alpha = __msa_ldi_b(0xFF);
#endif

  while (--num_rows >= 0) {
    p_in_y = input_buf[0][input_row];
    p_in_cb = input_buf[1][input_row];
    p_in_cr = input_buf[2][input_row];
    p_rgb = *output_buf++;
    input_row++;
    remaining_wd = out_width & 0xF;

    for (col = num_cols_mul_16; col--;) {
      input_y = LD_UB(p_in_y);
      input_cb = LD_SB(p_in_cb);
      input_cr = LD_SB(p_in_cr);

      p_in_y += 16;
      p_in_cb += 16;
      p_in_cr += 16;

      input_cb -= const_128;
      input_cr -= const_128;

      UNPCK_UB_SH(input_y, y_h0, y_h1);
      UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
      UNPCK_SB_SH(input_cr, cr_h0, cr_h1);
      CALC_G4_FRM_YCC(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

      UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
      UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
      CALC_R4_FRM_YCC(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

      UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
      UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
      CALC_B4_FRM_YCC(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

#if RGB_PIXELSIZE == 3
      ILVRL_B2_SB(out_g0, var_a, out_rgb0, out_rgb1);
      VSHF_B2_UB(out_rgb0, var_b, out_rgb0, var_b, mask_rgb0, mask_rgb1,
                 out0, tmp0);
      VSHF_B2_UB(out_rgb1, var_b, out_rgb1, var_b, mask_rgb1, mask_rgb2,
                 tmp1, out2);

      out1 = (v16u8)__msa_sldi_b((v16i8)zero, (v16i8)tmp1, 8);
      out1 = (v16u8)__msa_pckev_d((v2i64)out1, (v2i64)tmp0);

      ST_UB(out0, p_rgb);
      p_rgb += 16;
      ST_UB(out1, p_rgb);
      p_rgb += 16;
      ST_UB(out2, p_rgb);
      p_rgb += 16;
#else /* RGB_PIXELSIZE == 4 */
      ILVRL_B2_SB(var_c, var_a, out_rgb0, out_rgb1);
      ILVRL_B2_SB(var_d, var_b, out_ba0, out_ba1);
      ILVRL_H2_UB(var_g, var_e, out0, out1);
      ILVRL_H2_UB(var_h, var_f, out2, out3);

      ST_UB4(out0, out1, out2, out3, p_rgb, 16);
      p_rgb += 16 * 4;
#endif
    }

    if (remaining_wd >= 8) {
      uint64_t in_y, in_cb, in_cr;
      v16i8 input_cbcr = { 0 };

      in_y = LD(p_in_y);
      in_cb = LD(p_in_cb);
      in_cr = LD(p_in_cr);

      p_in_y += 8;
      p_in_cb += 8;
      p_in_cr += 8;

      input_y = (v16u8)__msa_insert_d((v2i64)input_y, 0, in_y);
      input_cbcr = (v16i8)__msa_insert_d((v2i64)input_cbcr, 0, in_cb);
      input_cbcr = (v16i8)__msa_insert_d((v2i64)input_cbcr, 1, in_cr);
      input_cbcr -= const_128;

      y_h0 = (v8i16)__msa_ilvr_b((v16i8)zero, (v16i8)input_y);
      UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
      UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
      UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

      CALC_R2_FRM_YCC(y_h0, cr_w0, cr_w1, out_r0);
      CALC_G2_FRM_YCC(y_h0, cb_h0, cr_h0, out_g0);
      CALC_B2_FRM_YCC(y_h0, cb_w0, cb_w1, out_b0);

#if RGB_PIXELSIZE == 3
      out_rgb0 = (v16i8)__msa_ilvr_b((v16i8)out_g0, (v16i8)var_a);
      VSHF_B2_UB(out_rgb0, var_b, out_rgb0, var_b, mask_rgb0, mask_rgb1,
                 out0, out1);

      ST_UB(out0, p_rgb);
      p_rgb += 16;
      ST8x1_UB(out1, p_rgb);
      p_rgb += 8;
#else /* RGB_PIXELSIZE == 4 */
      out_rgb0 = (v16i8)__msa_ilvr_b((v16i8)var_c, (v16i8)var_a);
      out_ba0 = (v16i8)__msa_ilvr_b(var_d, (v16i8)var_b);
      ILVRL_H2_UB(var_g, var_e, out0, out1);

      ST_UB2(out0, out1, p_rgb, 16);
      p_rgb += 16 * 2;
#endif

      remaining_wd -= 8;
    }

    for (col = 0; col < remaining_wd; col++) {
      y  = (int)(p_in_y[col]);
      cb = (int)(p_in_cb[col]) - 128;
      cr = (int)(p_in_cr[col]) - 128;

      /* Range-limiting is essential due to noise introduced by DCT losses. */
      p_rgb[RGB_RED] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 *
                                  cr, 16));
      p_rgb[RGB_GREEN] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) *
                                    cb - FIX_0_71414 * cr), 16));
      p_rgb[RGB_BLUE] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 *
                                   cb, 16));
#ifdef RGB_ALPHA
      p_rgb[RGB_ALPHA] = 0xFF;
#endif
      p_rgb += RGB_PIXELSIZE;
    }
  }
}

#undef var_a
#undef var_b
#if RGB_PIXELSIZE == 4
#undef var_c
#undef var_d
#undef var_e
#undef var_f
#undef var_g
#undef var_h
#endif
