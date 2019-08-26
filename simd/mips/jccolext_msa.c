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

#include "../../jinclude.h"
#include "../../jpeglib.h"

#include "jmacros_msa.h"

#define SCALE 16
#define ONE_HALF (1 << (SCALE - 1))
#define CBCR_OFFSET (128 << SCALE)
#define GETSAMPLE(value) ((int) (value))

void
rgb_yuv_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JSAMPROW p_out_cb,
                     JSAMPROW p_out_cr, JDIMENSION width)
{
  JDIMENSION r, g, b;
  JDIMENSION col, width_mul_16 = width & 0xFFF0;
  v16u8 in0, in1, in2, out_y, out_u, out_v;
  v8i16 r0, b0, r1, b1, rg0, gb0, rg1, gb1, mult, zero = {0};
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  v16i8 rg0_mask = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22};
  v16i8 rg1_mask, gb0_mask, gb1_mask;
  v8i16 const0 = {19595, 16384, 22086, 7471, -11059, -21709, -27439, -5329};
  v4i32 offset_minus1 = __msa_fill_w(((128 << SCALE) - 1));

  LD_UB2(inptr, 16, in0, in1);

  for (col = 0; col < width_mul_16; col += 16) {
    gb0_mask = MSA_ADDVI_B(rg0_mask, 1);
    rg1_mask = MSA_ADDVI_B(rg0_mask, 8);
    gb1_mask = MSA_ADDVI_B(gb0_mask, 8);

    in2 = LD_UB(inptr + 32);

    VSHF_B2_SH(in0, in1, in1, in2, rg0_mask, rg1_mask, rg0, rg1);
    VSHF_B2_SH(in0, in1, in1, in2, gb0_mask, gb1_mask, gb0, gb1);
    r0 = (v8i16) __msa_pckev_b((v16i8) rg1, (v16i8) rg0);
    b0 = (v8i16) __msa_pckod_b((v16i8) gb1, (v16i8) gb0);

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
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                tmp0, tmp1, tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                 tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                 b0_r, b0_l, b1_r, b1_l);
    ADD4(b0_r, offset_minus1, b0_l, offset_minus1, b1_r, offset_minus1,
         b1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_u = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                 r0_r, r0_l, r1_r, r1_l);
    ADD4(r0_r, offset_minus1, r0_l, offset_minus1, r1_r, offset_minus1,
         r1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_v = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_v, (p_out_cr + col));

    inptr += 16 * 3;
    LD_UB2(inptr, 16, in0, in1);
  }

  if ((width - width_mul_16) >= 8) {
    gb0_mask = MSA_ADDVI_B(rg0_mask, 1);

    VSHF_B2_SH(in0, in1, in0, in1, rg0_mask, gb0_mask, rg0, gb0);
    r0 = (v8i16) __msa_pckev_b((v16i8) zero, (v16i8) rg0);
    b0 = (v8i16) __msa_pckod_b((v16i8) zero, (v16i8) gb0);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH2_SW(rg0_r, rg0_l, mult, mult, b0_r, b0_l);
    ADD2(b0_r, offset_minus1, b0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_u = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, r0_r, r0_l);
    ADD2(r0_r, offset_minus1, r0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_v = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_v, (p_out_cr + col));
    col += 8;
    inptr += 8 * 3;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[0]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[2]);
    inptr += 3;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b
                               + ONE_HALF) >> SCALE);
    p_out_cb[col] = (uint8_t) ((-11059 * r + -21709 * g + 32768 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    p_out_cr[col] = (uint8_t) ((32768 * r + -27439 * g + -5329 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
  }
}

void
bgr_yuv_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JSAMPROW p_out_cb,
                     JSAMPROW p_out_cr, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  int width_mul_16 = width & 0xFFF0;
  v16u8 in0, in1, in2, out_y, out_u, out_v;
  v8i16 r0, b0, r1, b1, bg0, gr0, bg1, gr1, mult, zero = {0};
  v8i16 bg0_r, gr0_r, bg1_r, gr1_r, bg0_l, gr0_l, bg1_l, gr1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  v16i8 bg0_mask = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22};
  v16i8 bg1_mask, gr0_mask, gr1_mask;
  v8i16 const0 = {7471, 22086, 16384, 19595, -21709, -11059, -5329, -27439};
  v4i32 offset_minus1 = __msa_fill_w(((128 << SCALE) - 1));

  LD_UB2(inptr, 16, in0, in1);

  for (col = 0; col < width_mul_16; col += 16) {
    gr0_mask = MSA_ADDVI_B(bg0_mask, 1);
    bg1_mask = MSA_ADDVI_B(bg0_mask, 8);
    gr1_mask = MSA_ADDVI_B(gr0_mask, 8);

    in2 = LD_UB(inptr + 32);

    VSHF_B2_SH(in0, in1, in1, in2, bg0_mask, bg1_mask, bg0, bg1);
    VSHF_B2_SH(in0, in1, in1, in2, gr0_mask, gr1_mask, gr0, gr1);
    b0 = (v8i16) __msa_pckev_b((v16i8) bg1, (v16i8) bg0);
    r0 = (v8i16) __msa_pckod_b((v16i8) gr1, (v16i8) gr0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);
    ILVL_B2_SH(zero, r0, zero, b0, r1, b1);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    ILVRL_H2_SW(zero, r1, r1_r, r1_l);
    ILVRL_H2_SW(zero, b1, b1_r, b1_l);
    SLLI_W4_SW(r0_r, r1_r, b0_r, b1_r, 15);
    SLLI_W4_SW(r0_l, r1_l, b0_l, b1_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult,
                tmp0, tmp1, tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult,
                 tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult,
                 b0_r, b0_l, b1_r, b1_l);
    ADD4(b0_r, offset_minus1, b0_l, offset_minus1, b1_r, offset_minus1,
         b1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_u = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult,
                 r0_r, r0_l, r1_r, r1_l);
    ADD4(r0_r, offset_minus1, r0_l, offset_minus1, r1_r, offset_minus1,
         r1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_v = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_v, (p_out_cr + col));

    inptr += 16 * 3;
    LD_UB2(inptr, 16, in0, in1);
  }

  if ((width - width_mul_16) >= 8) {
    gr0_mask = MSA_ADDVI_B(bg0_mask, 1);

    VSHF_B2_SH(in0, in1, in0, in1, bg0_mask, gr0_mask, bg0, gr0);
    b0 = (v8i16) __msa_pckev_b((v16i8) zero, (v16i8) bg0);
    r0 = (v8i16) __msa_pckod_b((v16i8) zero, (v16i8) gr0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(bg0_r, bg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, b0_r, b0_l);
    ADD2(b0_r, offset_minus1, b0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_u = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH2_SW(bg0_r, bg0_l, mult, mult, r0_r, r0_l);
    ADD2(r0_r, offset_minus1, r0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_v = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_v, (p_out_cr + col));
    col += 8;
    inptr += 8 * 3;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[2]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[0]);
    inptr += 3;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b
                               + ONE_HALF) >> SCALE);
    p_out_cb[col] = (uint8_t) ((-11059 * r + -21709 * g + 32768 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    p_out_cr[col] = (uint8_t) ((32768 * r + -27439 * g + -5329 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
  }
}

void
rgbx_yuv_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JSAMPROW p_out_cb,
                      JSAMPROW p_out_cr, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  int width_mul_16 = width & 0xFFF0;
  v16u8 in0, in1, in2, in3, out_y, out_u, out_v;
  v8i16 r0, b0, r1, b1, rg0, gb0, rg1, gb1, mult, zero = {0};
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  v8i16 const0 = {19595, 16384, 22086, 7471, -11059, -21709, -27439, -5329};
  v4i32 offset_minus1 = __msa_fill_w(((128 << SCALE) - 1));

  LD_UB2(inptr, 16, in0, in1);

  for (col = 0; col < width_mul_16; col += 16) {
    LD_UB2((inptr + 32), 16, in2, in3);
    PCKEV_H2_SH(in1, in0, in3, in2, rg0, rg1);
    r0 = (v8i16) __msa_pckev_b((v16i8) rg1, (v16i8) rg0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    in2 = (v16u8) __msa_sldi_b((v16i8) in2, (v16i8) in2, 1);
    in3 = (v16u8) __msa_sldi_b((v16i8) in3, (v16i8) in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, gb0, gb1);
    b0 = (v8i16) __msa_pckod_b((v16i8) gb1, (v16i8) gb0);

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
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                tmp0, tmp1, tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                 tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                 b0_r, b0_l, b1_r, b1_l);
    ADD4(b0_r, offset_minus1, b0_l, offset_minus1, b1_r, offset_minus1,
         b1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_u = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                 r0_r, r0_l, r1_r, r1_l);
    ADD4(r0_r, offset_minus1, r0_l, offset_minus1, r1_r, offset_minus1,
         r1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_v = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_v, (p_out_cr + col));

    inptr += 16 * 4;
    LD_UB2(inptr, 16, in0, in1);
  }

  if ((width - width_mul_16) >= 8) {
    rg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);
    r0 = (v8i16) __msa_pckev_b((v16i8) zero, (v16i8) rg0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    gb0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);
    b0 = (v8i16) __msa_pckod_b((v16i8) zero, (v16i8) gb0);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH2_SW(rg0_r, rg0_l, mult, mult, b0_r, b0_l);
    ADD2(b0_r, offset_minus1, b0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_u = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, r0_r, r0_l);
    ADD2(r0_r, offset_minus1, r0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_v = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_v, (p_out_cr + col));
    col += 8;
    inptr += 8 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[0]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[2]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b
                               + ONE_HALF) >> SCALE);
    p_out_cb[col] = (uint8_t) ((-11059 * r + -21709 * g + 32768 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    p_out_cr[col] = (uint8_t) ((32768 * r + -27439 * g + -5329 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
  }
}

void
bgrx_yuv_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JSAMPROW p_out_cb,
                      JSAMPROW p_out_cr, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  int width_mul_16 = width & 0xFFF0;
  v16u8 in0, in1, in2, in3, out_y, out_u, out_v;
  v8i16 r0, b0, r1, b1, bg0, gr0, bg1, gr1, mult, zero = {0};
  v8i16 bg0_r, gr0_r, bg1_r, gr1_r, bg0_l, gr0_l, bg1_l, gr1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  v8i16 const0 = {7471, 22086, 16384, 19595, -21709, -11059, -5329, -27439};
  v4i32 offset_minus1 = __msa_fill_w(((128 << SCALE) - 1));

  LD_UB2(inptr, 16, in0, in1);

  for (col = 0; col < width_mul_16; col += 16) {
    LD_UB2((inptr + 32), 16, in2, in3);
    PCKEV_H2_SH(in1, in0, in3, in2, bg0, bg1);
    b0 = (v8i16) __msa_pckev_b((v16i8) bg1, (v16i8) bg0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    in2 = (v16u8) __msa_sldi_b((v16i8) in2, (v16i8) in2, 1);
    in3 = (v16u8) __msa_sldi_b((v16i8) in3, (v16i8) in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, gr0, gr1);
    r0 = (v8i16) __msa_pckod_b((v16i8) gr1, (v16i8) gr0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);
    ILVL_B2_SH(zero, r0, zero, b0, r1, b1);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    ILVRL_H2_SW(zero, r1, r1_r, r1_l);
    ILVRL_H2_SW(zero, b1, b1_r, b1_l);
    SLLI_W4_SW(r0_r, r1_r, b0_r, b1_r, 15);
    SLLI_W4_SW(r0_l, r1_l, b0_l, b1_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult,
                tmp0, tmp1, tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult,
                 tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult,
                 b0_r, b0_l, b1_r, b1_l);
    ADD4(b0_r, offset_minus1, b0_l, offset_minus1, b1_r, offset_minus1,
         b1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_u = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult,
                 r0_r, r0_l, r1_r, r1_l);
    ADD4(r0_r, offset_minus1, r0_l, offset_minus1, r1_r, offset_minus1,
         r1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_v = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_v, (p_out_cr + col));

    inptr += 16 * 4;
    LD_UB2(inptr, 16, in0, in1);
  }

  if ((width - width_mul_16) >= 8) {
    bg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);
    b0 = (v8i16) __msa_pckev_b((v16i8) zero, (v16i8) bg0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    gr0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);
    r0 = (v8i16) __msa_pckod_b((v16i8) zero, (v16i8) gr0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(bg0_r, bg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, b0_r, b0_l);
    ADD2(b0_r, offset_minus1, b0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_u = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH2_SW(bg0_r, bg0_l, mult, mult, r0_r, r0_l);
    ADD2(r0_r, offset_minus1, r0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_v = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_v, (p_out_cr + col));
    col += 8;
    inptr += 8 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[2]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[0]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b
                               + ONE_HALF) >> SCALE);
    p_out_cb[col] = (uint8_t) ((-11059 * r + -21709 * g + 32768 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    p_out_cr[col] = (uint8_t) ((32768 * r + -27439 * g + -5329 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
  }
}

void
xrgb_yuv_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JSAMPROW p_out_cb,
                      JSAMPROW p_out_cr, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  int width_mul_16 = width & 0xFFF0;
  v16u8 in0, in1, in2, in3, out_y, out_u, out_v;
  v8i16 r0, b0, r1, b1, rg0, gb0, rg1, gb1, mult, zero = {0};
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  v8i16 const0 = {19595, 16384, 22086, 7471, -11059, -21709, -27439, -5329};
  v4i32 offset_minus1 = __msa_fill_w(((128 << SCALE) - 1));

  LD_UB2(inptr, 16, in0, in1);

  for (col = 0; col < width_mul_16; col += 16) {
    LD_UB2((inptr + 32), 16, in2, in3);
    PCKOD_H2_SH(in1, in0, in3, in2, gb0, gb1);
    b0 = (v8i16) __msa_pckod_b((v16i8) gb1, (v16i8) gb0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    in2 = (v16u8) __msa_sldi_b((v16i8) in2, (v16i8) in2, 1);
    in3 = (v16u8) __msa_sldi_b((v16i8) in3, (v16i8) in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, rg0, rg1);
    r0 = (v8i16) __msa_pckev_b((v16i8) rg1, (v16i8) rg0);

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
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                tmp0, tmp1, tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                 tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult,
                 b0_r, b0_l, b1_r, b1_l);
    ADD4(b0_r, offset_minus1, b0_l, offset_minus1, b1_r, offset_minus1,
         b1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_u = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult,
                 r0_r, r0_l, r1_r, r1_l);
    ADD4(r0_r, offset_minus1, r0_l, offset_minus1, r1_r, offset_minus1,
         r1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_v = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_v, (p_out_cr + col));

    inptr += 16 * 4;
    LD_UB2(inptr, 16, in0, in1);
  }

  if ((width - width_mul_16) >= 8) {
    gb0 = __msa_pckod_h((v8i16) in1, (v8i16) in0);
    b0 = (v8i16) __msa_pckod_b((v16i8) zero, (v16i8) gb0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    rg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);
    r0 = (v8i16) __msa_pckev_b((v16i8) zero, (v16i8) rg0);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH2_SW(rg0_r, rg0_l, mult, mult, b0_r, b0_l);
    ADD2(b0_r, offset_minus1, b0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_u = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, r0_r, r0_l);
    ADD2(r0_r, offset_minus1, r0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_v = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_v, (p_out_cr + col));
    col += 8;
    inptr += 8 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[1]);
    g = GETSAMPLE(inptr[2]);
    b = GETSAMPLE(inptr[3]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b
                               + ONE_HALF) >> SCALE);
    p_out_cb[col] = (uint8_t) ((-11059 * r + -21709 * g + 32768 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    p_out_cr[col] = (uint8_t) ((32768 * r + -27439 * g + -5329 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
  }
}

void
xbgr_yuv_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JSAMPROW p_out_cb,
                      JSAMPROW p_out_cr, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  int width_mul_16 = width & 0xFFF0;
  v16u8 in0, in1, in2, in3, out_y, out_u, out_v;
  v8i16 r0, b0, r1, b1, bg0, gr0, bg1, gr1, mult, zero = {0};
  v8i16 bg0_r, gr0_r, bg1_r, gr1_r, bg0_l, gr0_l, bg1_l, gr1_l;
  v4i32 r0_r, r0_l, r1_r, r1_l, b0_r, b0_l, b1_r, b1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  v8i16 const0 = {7471, 22086, 16384, 19595, -21709, -11059, -5329, -27439};
  v4i32 offset_minus1 = __msa_fill_w(((128 << SCALE) - 1));

  LD_UB2(inptr, 16, in0, in1);

  for (col = 0; col < width_mul_16; col += 16) {
    LD_UB2((inptr + 32), 16, in2, in3);
    PCKOD_H2_SH(in1, in0, in3, in2, gr0, gr1);
    r0 = (v8i16) __msa_pckod_b((v16i8) gr1, (v16i8) gr0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    in2 = (v16u8) __msa_sldi_b((v16i8) in2, (v16i8) in2, 1);
    in3 = (v16u8) __msa_sldi_b((v16i8) in3, (v16i8) in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, bg0, bg1);
    b0 = (v8i16) __msa_pckev_b((v16i8) bg1, (v16i8) bg0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);
    ILVL_B2_SH(zero, r0, zero, b0, r1, b1);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    ILVRL_H2_SW(zero, r1, r1_r, r1_l);
    ILVRL_H2_SW(zero, b1, b1_r, b1_l);
    SLLI_W4_SW(r0_r, r1_r, b0_r, b1_r, 15);
    SLLI_W4_SW(r0_l, r1_l, b0_l, b1_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult,
                tmp0, tmp1, tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult,
                 tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult,
                 b0_r, b0_l, b1_r, b1_l);
    ADD4(b0_r, offset_minus1, b0_l, offset_minus1, b1_r, offset_minus1,
         b1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_u = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult,
                 r0_r, r0_l, r1_r, r1_l);
    ADD4(r0_r, offset_minus1, r0_l, offset_minus1, r1_r, offset_minus1,
         r1_l, offset_minus1, tmp0, tmp1, tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_v = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_v, (p_out_cr + col));

    inptr += 16 * 4;
    LD_UB2(inptr, 16, in0, in1);
  }

  if ((width - width_mul_16) >= 8) {
    gr0 = __msa_pckod_h((v8i16) in1, (v8i16) in0);
    r0 = (v8i16) __msa_pckod_b((v16i8) zero, (v16i8) gr0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    in1 = (v16u8) __msa_sldi_b((v16i8) in1, (v16i8) in1, 1);
    bg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);
    b0 = (v8i16) __msa_pckev_b((v16i8) zero, (v16i8) bg0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    ILVR_B2_SH(zero, r0, zero, b0, r0, b0);
    ILVRL_H2_SW(zero, r0, r0_r, r0_l);
    ILVRL_H2_SW(zero, b0, b0_r, b0_l);
    SLLI_W4_SW(r0_r, r0_l, b0_r, b0_l, 15);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(bg0_r, bg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    /* RGB --> U */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 2);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, b0_r, b0_l);
    ADD2(b0_r, offset_minus1, b0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_u = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_u, (p_out_cb + col));

    /* RGB --> V */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 3);
    DPADD_SH2_SW(bg0_r, bg0_l, mult, mult, r0_r, r0_l);
    ADD2(r0_r, offset_minus1, r0_l, offset_minus1, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_v = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_v, (p_out_cr + col));
    col += 8;
    inptr += 8 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[3]);
    g = GETSAMPLE(inptr[2]);
    b = GETSAMPLE(inptr[1]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b
                               + ONE_HALF) >> SCALE);
    p_out_cb[col] = (uint8_t) ((-11059 * r + -21709 * g + 32768 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
    p_out_cr[col] = (uint8_t) ((32768 * r + -27439 * g + -5329 * b
                                + CBCR_OFFSET - 1 + ONE_HALF) >> SCALE);
  }
}

GLOBAL(void)
jsimd_rgb_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                           JSAMPIMAGE output_buf, JDIMENSION output_row,
                           int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
      p_out_y = output_buf[0][output_row];
      p_out_cb = output_buf[1][output_row];
      p_out_cr = output_buf[2][output_row];
      inptr = *input_buf++;
      output_row++;
      rgb_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}

GLOBAL(void)
jsimd_extrgb_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                              JSAMPIMAGE output_buf, JDIMENSION output_row,
                              int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;
    rgb_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}

GLOBAL(void)
jsimd_extbgr_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                              JSAMPIMAGE output_buf, JDIMENSION output_row,
                              int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;
    bgr_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}

GLOBAL(void)
jsimd_extrgbx_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf, JDIMENSION output_row,
                               int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;
    rgbx_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}

GLOBAL(void)
jsimd_extbgrx_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf, JDIMENSION output_row,
                               int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;
    bgrx_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}

GLOBAL(void)
jsimd_extxrgb_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf, JDIMENSION output_row,
                               int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;
    xrgb_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}

GLOBAL(void)
jsimd_extxbgr_ycc_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf, JDIMENSION output_row,
                               int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW p_out_y, p_out_cb, p_out_cr;

  while (--num_rows >= 0) {
    p_out_y = output_buf[0][output_row];
    p_out_cb = output_buf[1][output_row];
    p_out_cr = output_buf[2][output_row];
    inptr = *input_buf++;
    output_row++;
    xbgr_yuv_convert_msa(inptr, p_out_y, p_out_cb, p_out_cr, img_width);
  }
}
