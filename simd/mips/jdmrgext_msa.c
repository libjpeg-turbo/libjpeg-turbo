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

/* Merged upsample: YCC --> RGB CONVERSION + Upsample */

#if RGB_RED == 0
#define var_a  out_r0
#define var_b  out_b0
#define var_c  out_r1
#define var_d  out_b1
#elif RGB_RED == 1 || RGB_RED == 3
#define var_a  out_g0
#define var_b  alpha
#define var_c  out_g1
#define var_d  alpha
#else /* RGB_RED == 2 */
#define var_a  out_b0
#define var_b  out_r0
#define var_c  out_b1
#define var_d  out_r1
#endif

#if RGB_PIXELSIZE == 4
#if RGB_RED == 0 || RGB_RED == 2
#define var_e  out_g0
#define var_f  alpha
#define var_g  out_g1
#define var_h  alpha
#define var_i  out_rgb0
#define var_j  out_rgb1
#define var_k  out_rgb2
#define var_l  out_rgb3
#define var_m  tmp0
#define var_n  tmp1
#define var_o  tmp2
#define var_p  tmp3
#else /* RGB_RED == 1 || RGB_RED == 3 */
#define var_i  tmp0
#define var_j  tmp1
#define var_k  tmp2
#define var_l  tmp3
#define var_m  out_rgb0
#define var_n  out_rgb1
#define var_o  out_rgb2
#define var_p  out_rgb3
#if RGB_RED == 1
#define var_e  out_b0
#define var_f  out_r0
#define var_g  out_b1
#define var_h  out_r1
#else /* RGB_RED == 3 */
#define var_e  out_r0
#define var_f  out_b0
#define var_g  out_r1
#define var_h  out_b1
#endif
#endif
#endif

GLOBAL(void)
jsimd_h2v1_merged_upsample_msa(JDIMENSION output_width, JSAMPIMAGE input_buf,
                               JDIMENSION in_row_group_ctr,
                               JSAMPARRAY output_buf)
{
  register JSAMPROW p_in_y, p_in_cb, p_in_cr;
  register JSAMPROW p_out;
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = output_width >> 5;
  JDIMENSION remaining_wd = output_width & 0x1E;
  v16u8 out0, out1, out2, out3, out4, out5, input_y0 = { 0 }, input_y1;
  v16i8 tmp0, tmp1, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = { 0 };
#if RGB_PIXELSIZE == 3
  const v16u8 mask_rgb0 =
    { 0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10 };
  const v16u8 mask_rgb1 =
    { 11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5 };
  const v16u8 mask_rgb2 =
    { 26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31 };
#else /* RGB_PIXELSIZE == 4 */
  v16u8 out6, out7;
  v16i8 tmp2, tmp3, alpha = __msa_ldi_b(0xFF);
#endif

  p_in_y = input_buf[0][in_row_group_ctr];
  p_in_cb = input_buf[1][in_row_group_ctr];
  p_in_cr = input_buf[2][in_row_group_ctr];
  p_out = output_buf[0];

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8)__msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8)__msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YCC(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YCC(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YCC(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

#if RGB_PIXELSIZE == 3
    ILVRL_B2_SB(out_g0, var_a, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, var_c, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, var_b, out_rgb1, var_b, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, var_b, out_rgb1, var_b, mask_rgb1, mask_rgb1,
               tmp0, tmp1);

    out1 = (v16u8)__msa_sldi_b((v16i8)zero, tmp1, 8);
    out1 = (v16u8)__msa_pckev_d((v2i64)out1, (v2i64)tmp0);

    VSHF_B2_UB(out_rgb2, var_d, out_rgb3, var_d, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, var_d, out_rgb3, var_d, mask_rgb1, mask_rgb1,
               tmp0, tmp1);

    out4 = (v16u8)__msa_sldi_b((v16i8)zero, tmp1, 8);
    out4 = (v16u8)__msa_pckev_d((v2i64)out4, (v2i64)tmp0);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;
    ST_UB2(out4, out5, p_out, 16);
    p_out += 16 * 2;
#else /* RGB_PIXELSIZE == 4 */
    ILVRL_B2_SB(var_e, var_a, out_rgb0, out_rgb1);
    ILVRL_B2_SB(var_g, var_c, out_rgb2, out_rgb3);
    ILVRL_B2_SB(var_f, var_b, tmp0, tmp1);
    ILVRL_B2_SB(var_h, var_d, tmp2, tmp3);

    ILVRL_H2_UB(var_m, var_i, out0, out1);
    ILVRL_H2_UB(var_n, var_j, out2, out3);
    ILVRL_H2_UB(var_o, var_k, out4, out5);
    ILVRL_H2_UB(var_p, var_l, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out, 16);
    p_out += 16 * 8;
#endif
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = { 0 };

    tmp1 = LD_SB(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8)zero, tmp1);
    tmp1 = __msa_pckod_b((v16i8)zero, tmp1);
    input_y0 = (v16u8)__msa_pckev_d((v2i64)tmp1, (v2i64)tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8)__msa_insert_d((v2i64)input_cbcr, 0, in_cb);
    input_cbcr = (v16i8)__msa_insert_d((v2i64)input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YCC(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8)__msa_ilvr_b((v16i8)tmp1, (v16i8)tmp0);
    CALC_G2_FRM_YCC(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8)__msa_ilvr_b((v16i8)tmp1, (v16i8)tmp0);
    CALC_B2_FRM_YCC(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8)__msa_ilvr_b((v16i8)tmp1, (v16i8)tmp0);

#if RGB_PIXELSIZE == 3
    ILVRL_B2_SB(out_g0, var_a, out_rgb0, out_rgb1);
    VSHF_B2_UB(out_rgb0, var_b, out_rgb1, var_b, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, var_b, out_rgb1, var_b, mask_rgb1, mask_rgb1,
               tmp0, tmp1);

    out1 = (v16u8)__msa_sldi_b((v16i8)zero, tmp1, 8);
    out1 = (v16u8)__msa_pckev_d((v2i64)out1, (v2i64)tmp0);

    ST_UB(out0, p_out);
    p_out += 16;
    ST_UB(out1, p_out);
    p_out += 16;
    ST_UB(out2, p_out);
    p_out += 16;
#else /* RGB_PIXELSIZE == 4 */
    ILVRL_B2_SB(var_e, var_a, out_rgb0, out_rgb1);
    ILVRL_B2_SB(var_f, var_b, tmp0, tmp1);

    ILVRL_H2_UB(var_m, var_i, out0, out1);
    ILVRL_H2_UB(var_n, var_j, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;
#endif

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = { 0 };

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8)__msa_insert_d((v2i64)input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8)zero, (v16i8)input_y0);
    tmp1 = __msa_pckod_b((v16i8)zero, (v16i8)input_y0);
    input_y0 = (v16u8)__msa_pckev_d((v2i64)tmp1, (v2i64)tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8)__msa_insert_w((v4i32)input_cbcr, 0, in_cb);
    input_cbcr = (v16i8)__msa_insert_w((v4i32)input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YCC(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8)__msa_ilvr_b((v16i8)tmp1, (v16i8)tmp0);
    CALC_G2_FRM_YCC(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8)__msa_ilvr_b((v16i8)tmp1, (v16i8)tmp0);
    CALC_B2_FRM_YCC(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8)__msa_ilvr_b((v16i8)tmp1, (v16i8)tmp0);

#if RGB_PIXELSIZE == 3
    out_rgb0 = (v16i8)__msa_ilvr_b((v16i8)out_g0, (v16i8)var_a);
    VSHF_B2_UB(out_rgb0, var_b, out_rgb0, var_b, mask_rgb0, mask_rgb1,
               out0, out1);

    ST_UB(out0, p_out);
    p_out += 16;
    ST8x1_UB(out1, p_out);
    p_out += 8;
#else /* RGB_PIXELSIZE == 4 */
    out_rgb0 = (v16i8)__msa_ilvr_b((v16i8)var_e, (v16i8)var_a);
    tmp0 = (v16i8)__msa_ilvr_b(var_f, (v16i8)var_b);

    ILVRL_H2_UB(var_m, var_i, out0, out1);

    ST_UB2(out0, out1, p_out, 16);
    p_out += 32;
#endif

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int)(*p_in_cb++) - 128;
    cr = (int)(*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int)(*p_in_y++);
    p_out[RGB_RED] = clip_pixel(y + cred);
    p_out[RGB_GREEN] = clip_pixel(y + cgreen);
    p_out[RGB_BLUE] = clip_pixel(y + cblue);
#ifdef RGB_ALPHA
    p_out[RGB_ALPHA] = 0xFF;
#endif
    p_out += RGB_PIXELSIZE;

    y = (int)(*p_in_y++);
    p_out[RGB_RED] = clip_pixel(y + cred);
    p_out[RGB_GREEN] = clip_pixel(y + cgreen);
    p_out[RGB_BLUE] = clip_pixel(y + cblue);
#ifdef RGB_ALPHA
    p_out[RGB_ALPHA] = 0xFF;
#endif
    p_out += RGB_PIXELSIZE;
  }

  if (output_width & 1) {
    cb = (int)(*p_in_cb) - 128;
    cr = (int)(*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int)(*p_in_y);
    p_out[RGB_RED] = clip_pixel(y + cred);
    p_out[RGB_GREEN] = clip_pixel(y + cgreen);
    p_out[RGB_BLUE] = clip_pixel(y + cblue);
#ifdef RGB_ALPHA
    p_out[RGB_ALPHA] = 0xFF;
#endif
  }
}

GLOBAL(void)
jsimd_h2v2_merged_upsample_msa(JDIMENSION output_width,
                               JSAMPIMAGE input_buf,
                               JDIMENSION in_row_group_ctr,
                               JSAMPARRAY output_buf)
{
  JSAMPROW inptr, outptr;

  inptr = input_buf[0][in_row_group_ctr];
  outptr = output_buf[0];

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2];
  jsimd_h2v1_merged_upsample_msa(output_width, input_buf, in_row_group_ctr,
                                 output_buf);

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2 + 1];
  output_buf[0] = output_buf[1];
  jsimd_h2v1_merged_upsample_msa(output_width, input_buf, in_row_group_ctr,
                                 output_buf);

  input_buf[0][in_row_group_ctr] = inptr;
  output_buf[0] = outptr;
}

#undef var_a
#undef var_b
#undef var_c
#undef var_d
#if RGB_PIXELSIZE == 4
#undef var_e
#undef var_f
#undef var_g
#undef var_h
#undef var_i
#undef var_j
#undef var_k
#undef var_l
#undef var_m
#undef var_n
#undef var_o
#undef var_p
#endif
