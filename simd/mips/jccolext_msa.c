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

/* RGB --> YCC CONVERSION */

#if RGB_RED == 0 || RGB_RED == 1
#define var_c  rg0_r
#define var_d  rg0_l
#define var_e  rg1_r
#define var_f  rg1_l
#define var_g  gb0_r
#define var_h  gb0_l
#define var_i  gb1_r
#define var_j  gb1_l
#if RGB_RED == 0
#define var_a  r0
#define var_b  b0
#else /* RGB_RED == 1 */
#define var_a  b0
#define var_b  r0
#endif
#else /* RGB_RED == 2 || RGB_RED == 3 */
#define var_c  gb0_r
#define var_d  gb0_l
#define var_e  gb1_r
#define var_f  gb1_l
#define var_g  rg0_r
#define var_h  rg0_l
#define var_i  rg1_r
#define var_j  rg1_l
#if RGB_RED == 2
#define var_a  b0
#define var_b  r0
#else /* RGB_RED == 3 */
#define var_a  r0
#define var_b  b0
#endif
#endif

#if RGB_PIXELSIZE == 4
#if RGB_RED == 0 || RGB_RED == 2
#define var_k  rg0
#define var_l  rg1
#define var_m  gb0
#define var_n  gb1
#define fun_a  PCKEV_H2_SH
#define fun_b  __msa_pckev_b
#define fun_c  __msa_pckod_b
#define fun_d  __msa_pckev_h
#else /* RGB_RED == 1 || RGB_RED == 3 */
#define var_k  gb0
#define var_l  gb1
#define var_m  rg0
#define var_n  rg1
#define fun_a  PCKOD_H2_SH
#define fun_b  __msa_pckod_b
#define fun_c  __msa_pckev_b
#define fun_d  __msa_pckod_h
#endif
#endif

GLOBAL(void)
jsimd_rgb_ycc_convert_msa(JDIMENSION img_width, JSAMPARRAY input_buf,
                          JSAMPIMAGE output_buf, JDIMENSION output_row,
                          int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;
  JDIMENSION r, g, b;
  JDIMENSION col, width_mul_16 = img_width & 0xFFF0;
  v16u8 in0, in1, in2, out_y, out_cb, out_cr;
  v8i16 r0, b0, r1, b1, rg0, gb0, rg1, gb1, mult, zero = { 0 };
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v8i16 coeff = COEFF_CONST;
  v4i32 offset_minus = __msa_fill_w(((128 << SCALE) - 1));
#if RGB_PIXELSIZE == 3
  const v16i8 rg0_mask =
    { 0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22 };
  v16i8 rg1_mask, gb0_mask, gb1_mask;
#else /* RGB_PIXELSIZE == 4 */
  v16u8 in3;
#endif

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;

    LD_UB2(inptr, 16, in0, in1);

    for (col = 0; col < width_mul_16; col += 16) {
#if RGB_PIXELSIZE == 3
      rg1_mask = MSA_ADDVI_B(rg0_mask, 8);
      gb0_mask = MSA_ADDVI_B(rg0_mask, 1);
      gb1_mask = MSA_ADDVI_B(gb0_mask, 8);

      in2 = LD_UB(inptr + 32);

      VSHF_B2_SH(in0, in1, in1, in2, rg0_mask, rg1_mask, rg0, rg1);
      VSHF_B2_SH(in0, in1, in1, in2, gb0_mask, gb1_mask, gb0, gb1);

      var_a = (v8i16)__msa_pckev_b((v16i8)rg1, (v16i8)rg0);
      var_b = (v8i16)__msa_pckod_b((v16i8)gb1, (v16i8)gb0);
#else /* RGB_PIXELSIZE == 4 */
      LD_UB2((inptr + 32), 16, in2, in3);

      fun_a(in1, in0, in3, in2, var_k, var_l);
      var_a = (v8i16)fun_b((v16i8)var_l, (v16i8)var_k);

      in0 = (v16u8)__msa_sldi_b((v16i8)in0, (v16i8)in0, 1);
      in1 = (v16u8)__msa_sldi_b((v16i8)in1, (v16i8)in1, 1);
      in2 = (v16u8)__msa_sldi_b((v16i8)in2, (v16i8)in2, 1);
      in3 = (v16u8)__msa_sldi_b((v16i8)in3, (v16i8)in3, 1);

      PCKEV_H2_SH(in1, in0, in3, in2, var_m, var_n);
      var_b = (v8i16)fun_c((v16i8)var_n, (v16i8)var_m);
#endif

      UNPCK_UB_SH(rg0, rg0_r, rg0_l);
      UNPCK_UB_SH(rg1, rg1_r, rg1_l);
      UNPCK_UB_SH(gb0, gb0_r, gb0_l);
      UNPCK_UB_SH(gb1, gb1_r, gb1_l);
      ILVL_B2_SH(zero, r0, zero, b0, r1, b1);
      ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
      ILVRL_H2_SW(zero, r0, r0_r, r0_l);
      ILVRL_H2_SW(zero, b0, b0_r, b0_l);
      ILVRL_H2_SW(zero, r1, r1_r, r1_l);
      ILVRL_H2_SW(zero, b1, b1_r, b1_l);
      SLLI_W4_SW(r0_r, r1_r, b0_r, b1_r, 15);
      SLLI_W4_SW(r0_l, r1_l, b0_l, b1_l, 15);

      /* RGB --> Y */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 0);
      DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                  tmp0, tmp1, tmp2, tmp3);
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 1);
      DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                   tmp0, tmp1, tmp2, tmp3);
      SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
      PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
      out_y = (v16u8)__msa_pckev_b((v16i8)tmp2, (v16i8)tmp0);
      ST_UB(out_y, (p_out_y + col));

      /* RGB --> Cb */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 2);
      DPADD_SH4_SW(var_c, var_d, var_e, var_f, mult, mult, mult, mult,
                   b0_r, b0_l, b1_r, b1_l);
      ADD4(b0_r, offset_minus, b0_l, offset_minus, b1_r, offset_minus,
           b1_l, offset_minus, tmp0, tmp1, tmp2, tmp3);
      SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
      PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
      out_cb = (v16u8)__msa_pckev_b((v16i8)tmp2, (v16i8)tmp0);
      ST_UB(out_cb, (p_out_cb + col));

      /* RGB --> Cr */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 3);
      DPADD_SH4_SW(var_g, var_h, var_i, var_j, mult, mult, mult, mult,
                   r0_r, r0_l, r1_r, r1_l);
      ADD4(r0_r, offset_minus, r0_l, offset_minus, r1_r, offset_minus,
           r1_l, offset_minus, tmp0, tmp1, tmp2, tmp3);
      SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
      PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
      out_cr = (v16u8)__msa_pckev_b((v16i8)tmp2, (v16i8)tmp0);
      ST_UB(out_cr, (p_out_cr + col));

      inptr += 16 * RGB_PIXELSIZE;
      LD_UB2(inptr, 16, in0, in1);
    }

    if (img_width - width_mul_16 >= 8) {
#if RGB_PIXELSIZE == 3
      gb0_mask = MSA_ADDVI_B(rg0_mask, 1);

      VSHF_B2_SH(in0, in1, in0, in1, rg0_mask, gb0_mask, rg0, gb0);

      var_a = (v8i16)__msa_pckev_b((v16i8)zero, (v16i8)rg0);
      var_b = (v8i16)__msa_pckod_b((v16i8)zero, (v16i8)gb0);
#else /* RGB_PIXELSIZE == 4 */
      var_k = fun_d((v8i16)in1, (v8i16)in0);
      var_a = (v8i16)fun_b((v16i8)zero, (v16i8)var_k);

      in0 = (v16u8)__msa_sldi_b((v16i8)in0, (v16i8)in0, 1);
      in1 = (v16u8)__msa_sldi_b((v16i8)in1, (v16i8)in1, 1);

      var_m = __msa_pckev_h((v8i16)in1, (v8i16)in0);
      var_b = (v8i16)fun_c((v16i8)zero, (v16i8)var_m);
#endif

      UNPCK_UB_SH(rg0, rg0_r, rg0_l);
      UNPCK_UB_SH(gb0, gb0_r, gb0_l);
      ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
      ILVRL_H2_SW(zero, r0, r0_r, r0_l);
      ILVRL_H2_SW(zero, b0, b0_r, b0_l);
      SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

      /* RGB --> Y */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 0);
      DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 1);
      DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
      SRARI_W2_SW(tmp0, tmp1, SCALE);
      tmp0 = (v4i32)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
      out_y = (v16u8)__msa_pckev_b((v16i8)zero, (v16i8)tmp0);
      ST8x1_UB(out_y, (p_out_y + col));

      /* RGB --> Cb */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 2);
      DPADD_SH2_SW(var_c, var_d, mult, mult, b0_r, b0_l);
      ADD2(b0_r, offset_minus, b0_l, offset_minus, tmp0, tmp1);
      SRARI_W2_SW(tmp0, tmp1, SCALE);
      tmp0 = (v4i32)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
      out_cb = (v16u8)__msa_pckev_b((v16i8)zero, (v16i8)tmp0);
      ST8x1_UB(out_cb, (p_out_cb + col));

      /* RGB --> Cr */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 3);
      DPADD_SH2_SW(var_g, var_h, mult, mult, r0_r, r0_l);
      ADD2(r0_r, offset_minus, r0_l, offset_minus, tmp0, tmp1);
      SRARI_W2_SW(tmp0, tmp1, SCALE);
      tmp0 = (v4i32)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
      out_cr = (v16u8)__msa_pckev_b((v16i8)zero, (v16i8)tmp0);
      ST8x1_UB(out_cr, (p_out_cr + col));

      col += 8;
      inptr += 8 * RGB_PIXELSIZE;
    }

    for (; col < img_width; col++) {
      r = GETSAMPLE(inptr[RGB_RED]);
      g = GETSAMPLE(inptr[RGB_GREEN]);
      b = GETSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;

      p_out_y[col] = (uint8_t)((19595 * r + 38470 * g + 7471 * b +
                                ONE_HALF) >> SCALE);
      p_out_cb[col] = (uint8_t)((-11059 * r + -21709 * g + 32768 * b +
                                 CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
      p_out_cr[col] = (uint8_t)((32768 * r + -27439 * g + -5329 * b +
                                 CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    }
  }
}

#undef var_a
#undef var_b
#undef var_c
#undef var_d
#undef var_e
#undef var_f
#undef var_g
#undef var_h
#undef var_i
#undef var_j
#if RGB_PIXELSIZE == 4
#undef var_k
#undef var_l
#undef var_m
#undef var_n
#undef fun_a
#undef fun_b
#undef fun_c
#undef fun_d
#endif
