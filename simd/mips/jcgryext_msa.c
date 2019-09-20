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

/* RGB --> GRAYSCALE CONVERSION */

#if RGB_PIXELSIZE == 4
#if RGB_RED == 0 || RGB_RED == 2
#define var_a  rg0
#define var_b  rg1
#define var_c  gb0
#define var_d  gb1
#define fun_a  PCKEV_H2_SH
#define fun_b  __msa_pckev_h
#else /* RGB_RED == 1 || RGB_RED == 3 */
#define var_a  gb0
#define var_b  gb1
#define var_c  rg0
#define var_d  rg1
#define fun_a  PCKOD_H2_SH
#define fun_b  __msa_pckod_h
#endif
#endif

GLOBAL(void)
jsimd_rgb_gray_convert_msa(JDIMENSION img_width, JSAMPARRAY input_buf,
                           JSAMPIMAGE output_buf, JDIMENSION output_row,
                           int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  JDIMENSION col, r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, out_y, out_y1;
  v8i16 rg0, gb0, rg1, gb1, mult, zero = { 0 };
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v8i16 coeff = COEFF_CONST;
#if RGB_PIXELSIZE == 3
  const v16i8 rg0_mask =
    { 0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22 };
  const v16i8 gb0_mask =
    { 1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 };
  const v16i8 rg1_mask =
    { 8, 9, 11, 12, 14, 15, 17, 18, 20, 21, 23, 24, 26, 27, 29, 30 };
  const v16i8 gb1_mask =
    { 9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 24, 25, 27, 28, 30, 31 };
#else /* RGB_PIXELSIZE == 4 */
  v16u8 in6, in7;
#endif

  while (--num_rows >= 0) {
    inptr = *input_buf++;

    for (col = 0; col < (img_width & ~0x1F); col += 32) {
#if RGB_PIXELSIZE == 3
      LD_UB6(inptr, 16, in0, in1, in2, in3, in4, in5);

      VSHF_B2_SH(in0, in1, in1, in2, rg0_mask, rg1_mask, rg0, rg1);
      VSHF_B2_SH(in0, in1, in1, in2, gb0_mask, gb1_mask, gb0, gb1);
#else /* RGB_PIXELSIZE == 4 */
      LD_UB8(inptr, 16, in0, in1, in2, in3, in4, in5, in6, in7);

      fun_a(in1, in0, in3, in2, var_a, var_b);

      SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
      SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
      PCKEV_H2_SH(in1, in0, in3, in2, var_c, var_d);
#endif

      UNPCK_UB_SH(rg0, rg0_r, rg0_l);
      UNPCK_UB_SH(rg1, rg1_r, rg1_l);
      UNPCK_UB_SH(gb0, gb0_r, gb0_l);
      UNPCK_UB_SH(gb1, gb1_r, gb1_l);

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

#if RGB_PIXELSIZE == 3
      VSHF_B2_SH(in3, in4, in4, in5, rg0_mask, rg1_mask, rg0, rg1);
      VSHF_B2_SH(in3, in4, in4, in5, gb0_mask, gb1_mask, gb0, gb1);
#else /* RGB_PIXELSIZE == 4 */
      fun_a(in5, in4, in7, in6, var_a, var_b);

      SLDI_B2_UB(in4, in5, in4, in5, in4, in5, 1);
      SLDI_B2_UB(in6, in7, in6, in7, in6, in7, 1);
      PCKEV_H2_SH(in5, in4, in7, in6, var_c, var_d);
#endif

      UNPCK_UB_SH(rg0, rg0_r, rg0_l);
      UNPCK_UB_SH(rg1, rg1_r, rg1_l);
      UNPCK_UB_SH(gb0, gb0_r, gb0_l);
      UNPCK_UB_SH(gb1, gb1_r, gb1_l);

      /* RGB --> Y */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 0);
      DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                  tmp0, tmp1, tmp2, tmp3);
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 1);
      DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                   tmp0, tmp1, tmp2, tmp3);
      SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
      PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
      out_y1 = (v16u8)__msa_pckev_b((v16i8)tmp2, (v16i8)tmp0);

      ST_UB2(out_y, out_y1, (p_out_y + col), 16);

      inptr += 32 * RGB_PIXELSIZE;
    }

    for (; col < (img_width & ~0xF); col += 16) {
#if RGB_PIXELSIZE == 3
      LD_UB3(inptr, 16, in0, in1, in2);

      VSHF_B2_SH(in0, in1, in1, in2, rg0_mask, rg1_mask, rg0, rg1);
      VSHF_B2_SH(in0, in1, in1, in2, gb0_mask, gb1_mask, gb0, gb1);
#else /* RGB_PIXELSIZE == 4 */
      LD_UB4(inptr, 16, in0, in1, in2, in3);

      fun_a(in1, in0, in3, in2, var_a, var_b);

      SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
      SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
      PCKEV_H2_SH(in1, in0, in3, in2, var_c, var_d);
#endif

      UNPCK_UB_SH(rg0, rg0_r, rg0_l);
      UNPCK_UB_SH(rg1, rg1_r, rg1_l);
      UNPCK_UB_SH(gb0, gb0_r, gb0_l);
      UNPCK_UB_SH(gb1, gb1_r, gb1_l);

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

      inptr += 16 * RGB_PIXELSIZE;
    }

    if ((img_width & 0xF) >= 8) {
      LD_UB2(inptr, 16, in0, in1);

#if RGB_PIXELSIZE == 3
      VSHF_B2_SH(in0, in1, in0, in1, rg0_mask, gb0_mask, rg0, gb0);
#else /* RGB_PIXELSIZE == 4 */
      var_a = fun_b((v8i16)in1, (v8i16)in0);

      SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
      var_b = __msa_pckev_h((v8i16)in1, (v8i16)in0);
#endif

      UNPCK_UB_SH(rg0, rg0_r, rg0_l);
      UNPCK_UB_SH(gb0, gb0_r, gb0_l);

      /* RGB --> Y */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 0);
      DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 1);
      DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
      SRARI_W2_SW(tmp0, tmp1, SCALE);
      tmp0 = (v4i32)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
      out_y = (v16u8)__msa_pckev_b((v16i8)zero, (v16i8)tmp0);
      ST8x1_UB(out_y, (p_out_y + col));

      col += 8;
      inptr += 8 * RGB_PIXELSIZE;
    }

    if ((img_width & 0x7) >= 4) {
      int out_val;
      in0 = LD_UB(inptr);

#if RGB_PIXELSIZE == 3
      VSHF_B2_SH(in0, in0, in0, in0, rg0_mask, gb0_mask, rg0, gb0);
#else /* RGB_PIXELSIZE == 4 */
      var_a = fun_b((v8i16)in0, (v8i16)in0);

      in0 = (v16u8)__msa_sldi_b((v16i8)in0, (v16i8)in0, 1);
      var_b = __msa_pckev_h((v8i16)in0, (v8i16)in0);
#endif

      rg0_r = (v8i16)__msa_ilvr_b((v16i8)zero, (v16i8)rg0);
      gb0_r = (v8i16)__msa_ilvr_b((v16i8)zero, (v16i8)gb0);

      /* RGB --> Y */
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 0);
      tmp0 = __msa_dotp_s_w((v8i16)rg0_r, (v8i16)mult);
      mult = (v8i16)__msa_splati_w((v4i32)coeff, 1);
      tmp0 = __msa_dpadd_s_w(tmp0, (v8i16)gb0_r, (v8i16)mult);
      tmp0 = __msa_srari_w(tmp0, SCALE);
      tmp0 = (v4i32)__msa_pckev_h((v8i16)zero, (v8i16)tmp0);
      out_y = (v16u8)__msa_pckev_b((v16i8)zero, (v16i8)tmp0);
      out_val = __msa_copy_s_w((v4i32)out_y, 0);
      SW(out_val, (p_out_y + col));

      col += 4;
      inptr += 4 * RGB_PIXELSIZE;
    }

    for (; col < img_width; col++) {
      r = GETSAMPLE(inptr[RGB_RED]);
      g = GETSAMPLE(inptr[RGB_GREEN]);
      b = GETSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;

      p_out_y[col] = (uint8_t)((19595 * r + 38470 * g + 7471 * b +
                                ONE_HALF) >> SCALE);
    }
  }
}

#if RGB_PIXELSIZE == 4
#undef var_a
#undef var_b
#undef var_c
#undef var_d
#undef fun_a
#undef fun_b
#endif
