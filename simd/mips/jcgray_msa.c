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

#include "../../jinclude.h"
#include "../../jpeglib.h"

#include "jmacros_msa.h"

#define SCALE 16
#define ONE_HALF (1 << (SCALE - 1))
#define CBCR_OFFSET (128 << SCALE)
#define GETSAMPLE(value) ((int) (value))

void
rgb_grey_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, out_y, out_y1;
  v8i16 rg0, gb0, rg1, gb1, mult, zero = {0};
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v16i8 rg0_mask = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21,
                          22};
  const v16i8 gb0_mask = {1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22,
                          23};
  const v16i8 rg1_mask = {8, 9, 11, 12, 14, 15, 17, 18, 20, 21, 23, 24, 26, 27,
                          29, 30};
  const v16i8 gb1_mask = {9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 24, 25, 27, 28,
                          30, 31};
  const v8i16 const0 = {19595, 16384, 22086, 7471, 0, 0, 0, 0};

  col = 0;

  for (; col < (width & ~0x1F); col += 32) {
    LD_UB6(inptr, 16, in0, in1, in2, in3, in4, in5);

    VSHF_B2_SH(in0, in1, in1, in2, rg0_mask, rg1_mask, rg0, rg1);
    VSHF_B2_SH(in0, in1, in1, in2, gb0_mask, gb1_mask, gb0, gb1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    VSHF_B2_SH(in3, in4, in4, in5, rg0_mask, rg1_mask, rg0, rg1);
    VSHF_B2_SH(in3, in4, in4, in5, gb0_mask, gb1_mask, gb0, gb1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y1 = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    ST_UB2(out_y, out_y1, (p_out_y + col), 16);

    inptr += 32 * 3;
  }

  for (; col < (width & ~0xF); col += 16) {
    LD_UB3(inptr, 16, in0, in1, in2);

    VSHF_B2_SH(in0, in1, in1, in2, rg0_mask, rg1_mask, rg0, rg1);
    VSHF_B2_SH(in0, in1, in1, in2, gb0_mask, gb1_mask, gb0, gb1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    inptr += 16 * 3;
  }

  if ((width & 0xF) >= 8) {
    LD_UB2(inptr, 16, in0, in1);

    VSHF_B2_SH(in0, in1, in0, in1, rg0_mask, gb0_mask, rg0, gb0);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    col += 8;
    inptr += 8 * 3;
  }

  if ((width & 0x7) >= 4) {
    int out_val;
    in0 = LD_UB(inptr);

    VSHF_B2_SH(in0, in0, in0, in0, rg0_mask, gb0_mask, rg0, gb0);

    rg0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) rg0);
    gb0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) gb0);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    tmp0 = __msa_dotp_s_w((v8i16) rg0_r, (v8i16) mult);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    tmp0 = __msa_dpadd_s_w(tmp0, (v8i16) gb0_r, (v8i16) mult);
    tmp0 = __msa_srari_w(tmp0, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) zero, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    out_val = __msa_copy_s_w((v4i32) out_y, 0);
    SW(out_val, (p_out_y + col));

    col += 4;
    inptr += 4 * 3;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[0]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[2]);
    inptr += 3;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b + ONE_HALF)
                              >> SCALE);
  }
}

void
bgr_grey_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, out_y, out_y1;
  v8i16 bg0, gr0, bg1, gr1, mult, zero = {0};
  v8i16 bg0_r, gr0_r, bg1_r, gr1_r, bg0_l, gr0_l, bg1_l, gr1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v16i8 bg0_mask = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21,
                          22};
  const v16i8 gr0_mask = {1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22,
                          23};
  const v16i8 bg1_mask = {8, 9, 11, 12, 14, 15, 17, 18, 20, 21, 23, 24, 26, 27,
                          29, 30};
  const v16i8 gr1_mask = {9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 24, 25, 27, 28,
                          30, 31};
  const v8i16 const0 = {7471, 22086, 16384, 19595, 0, 0, 0, 0};

  col = 0;

  for (; col < (width & ~0x1F); col += 32) {
    LD_UB6(inptr, 16, in0, in1, in2, in3, in4, in5);

    VSHF_B2_SH(in0, in1, in1, in2, bg0_mask, bg1_mask, bg0, bg1);
    VSHF_B2_SH(in0, in1, in1, in2, gr0_mask, gr1_mask, gr0, gr1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    VSHF_B2_SH(in3, in4, in4, in5, bg0_mask, bg1_mask, bg0, bg1);
    VSHF_B2_SH(in3, in4, in4, in5, gr0_mask, gr1_mask, gr0, gr1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y1 = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    ST_UB2(out_y, out_y1, (p_out_y + col), 16);

    inptr += 32 * 3;
  }

  for (; col < (width & ~0xF); col += 16) {
    LD_UB3(inptr, 16, in0, in1, in2);

    VSHF_B2_SH(in0, in1, in1, in2, bg0_mask, bg1_mask, bg0, bg1);
    VSHF_B2_SH(in0, in1, in1, in2, gr0_mask, gr1_mask, gr0, gr1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    inptr += 16 * 3;
  }

  if ((width & 0xF) >= 8) {
    LD_UB2(inptr, 16, in0, in1);

    VSHF_B2_SH(in0, in1, in0, in1, bg0_mask, gr0_mask, bg0, gr0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(bg0_r, bg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    col += 8;
    inptr += 8 * 3;
  }

  if ((width & 0x7) >= 4) {
    int out_val;
    in0 = LD_UB(inptr);

    VSHF_B2_SH(in0, in0, in0, in0, bg0_mask, gr0_mask, bg0, gr0);

    bg0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) bg0);
    gr0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) gr0);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    tmp0 = __msa_dotp_s_w((v8i16) bg0_r, (v8i16) mult);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    tmp0 = __msa_dpadd_s_w(tmp0, (v8i16) gr0_r, (v8i16) mult);
    tmp0 = __msa_srari_w(tmp0, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) zero, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    out_val = __msa_copy_s_w((v4i32) out_y, 0);
    SW(out_val, (p_out_y + col));

    col += 4;
    inptr += 4 * 3;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[2]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[0]);
    inptr += 3;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b + ONE_HALF)
                              >> SCALE);
  }
}

void
rgbx_grey_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, in6, in7, out_y, out_y1;
  v8i16 rg0, gb0, rg1, gb1, mult, zero = {0};
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v8i16 const0 = {19595, 16384, 22086, 7471, 0, 0, 0, 0};

  col = 0;

  for (; col < (width & ~0x1F); col += 32) {
    LD_UB8(inptr, 16, in0, in1, in2, in3, in4, in5, in6, in7);

    PCKEV_H2_SH(in1, in0, in3, in2, rg0, rg1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, gb0, gb1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    PCKEV_H2_SH(in5, in4, in7, in6, rg0, rg1);

    SLDI_B2_UB(in4, in5, in4, in5, in4, in5, 1);
    SLDI_B2_UB(in6, in7, in6, in7, in6, in7, 1);
    PCKEV_H2_SH(in5, in4, in7, in6, gb0, gb1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y1 = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    ST_UB2(out_y, out_y1, (p_out_y + col), 16);

    inptr += 32 * 4;
  }

  for (; col < (width & ~0xF); col += 16) {
    LD_UB4(inptr, 16, in0, in1, in2, in3);
    PCKEV_H2_SH(in1, in0, in3, in2, rg0, rg1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, gb0, gb1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    inptr += 16 * 4;
  }

  if ((width & 0xF) >= 8) {
    LD_UB2(inptr, 16, in0, in1);
    rg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    gb0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    col += 8;
    inptr += 8 * 4;
  }

  if ((width & 0x7) >= 4) {
    int out_val;
    in0 = LD_UB(inptr);

    rg0 = __msa_pckev_h((v8i16) in0, (v8i16) in0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    gb0 = __msa_pckev_h((v8i16) in0, (v8i16) in0);

    rg0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) rg0);
    gb0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) gb0);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    tmp0 = __msa_dotp_s_w((v8i16) rg0_r, (v8i16) mult);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    tmp0 = __msa_dpadd_s_w(tmp0, (v8i16) gb0_r, (v8i16) mult);
    tmp0 = __msa_srari_w(tmp0, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) zero, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    out_val = __msa_copy_s_w((v4i32) out_y, 0);
    SW(out_val, (p_out_y + col));

    col += 4;
    inptr += 4 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[0]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[2]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b + ONE_HALF)
                              >> SCALE);
  }
}

void
bgrx_grey_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, in6, in7, out_y, out_y1;
  v8i16 bg0, gr0, bg1, gr1, mult, zero = {0};
  v8i16 bg0_r, gr0_r, bg1_r, gr1_r, bg0_l, gr0_l, bg1_l, gr1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v8i16 const0 = {7471, 22086, 16384, 19595, 0, 0, 0, 0};

  col = 0;

  for (; col < (width & ~0x1F); col += 32) {
    LD_UB8(inptr, 16, in0, in1, in2, in3, in4, in5, in6, in7);

    PCKEV_H2_SH(in1, in0, in3, in2, bg0, bg1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, gr0, gr1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    PCKEV_H2_SH(in5, in4, in7, in6, bg0, bg1);

    SLDI_B2_UB(in4, in5, in4, in5, in4, in5, 1);
    SLDI_B2_UB(in6, in7, in6, in7, in6, in7, 1);
    PCKEV_H2_SH(in5, in4, in7, in6, gr0, gr1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y1 = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    ST_UB2(out_y, out_y1, (p_out_y + col), 16);

    inptr += 32 * 4;
  }

  for (; col < (width & ~0xF); col += 16) {
    LD_UB4(inptr, 16, in0, in1, in2, in3);

    PCKEV_H2_SH(in1, in0, in3, in2, bg0, bg1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, gr0, gr1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    inptr += 16 * 4;
  }

  if ((width & 0xF) >= 8) {
    LD_UB2(inptr, 16, in0, in1);
    bg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    gr0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(bg0_r, bg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    col += 8;
    inptr += 8 * 4;
  }

  if ((width & 0x7) >= 4) {
    int out_val;
    in0 = LD_UB(inptr);

    bg0 = __msa_pckev_h((v8i16) in0, (v8i16) in0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    gr0 = __msa_pckev_h((v8i16) in0, (v8i16) in0);

    bg0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) bg0);
    gr0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) gr0);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    tmp0 = __msa_dotp_s_w((v8i16) bg0_r, (v8i16) mult);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    tmp0 = __msa_dpadd_s_w(tmp0, (v8i16) gr0_r, (v8i16) mult);
    tmp0 = __msa_srari_w(tmp0, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) zero, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    out_val = __msa_copy_s_w((v4i32) out_y, 0);
    SW(out_val, (p_out_y + col));

    col += 4;
    inptr += 4 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[2]);
    g = GETSAMPLE(inptr[1]);
    b = GETSAMPLE(inptr[0]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b + ONE_HALF)
                              >> SCALE);
  }
}

void
xrgb_grey_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, in6, in7, out_y, out_y1;
  v8i16 rg0, gb0, rg1, gb1, mult, zero = {0};
  v8i16 rg0_r, gb0_r, rg1_r, gb1_r, rg0_l, gb0_l, rg1_l, gb1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v8i16 const0 = {19595, 16384, 22086, 7471, 0, 0, 0, 0};

  col = 0;

  for (; col < (width & ~0x1F); col += 32) {
    LD_UB8(inptr, 16, in0, in1, in2, in3, in4, in5, in6, in7);

    PCKOD_H2_SH(in1, in0, in3, in2, gb0, gb1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, rg0, rg1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    PCKOD_H2_SH(in5, in4, in7, in6, gb0, gb1);

    SLDI_B2_UB(in4, in5, in4, in5, in4, in5, 1);
    SLDI_B2_UB(in6, in7, in6, in7, in6, in7, 1);
    PCKEV_H2_SH(in5, in4, in7, in6, rg0, rg1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y1 = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    ST_UB2(out_y, out_y1, (p_out_y + col), 16);

    inptr += 32 * 4;
  }

  for (; col < (width & ~0xF); col += 16) {
    LD_UB4(inptr, 16, in0, in1, in2, in3);

    PCKOD_H2_SH(in1, in0, in3, in2, gb0, gb1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, rg0, rg1);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(rg1, rg1_r, rg1_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);
    UNPCK_UB_SH(gb1, gb1_r, gb1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(rg0_r, rg0_l, rg1_r, rg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gb0_r, gb0_l, gb1_r, gb1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    inptr += 16 * 4;
  }

  if ((width & 0xF) >= 8) {
    LD_UB2(inptr, 16, in0, in1);

    gb0 = __msa_pckod_h((v8i16) in1, (v8i16) in0);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    rg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);

    UNPCK_UB_SH(rg0, rg0_r, rg0_l);
    UNPCK_UB_SH(gb0, gb0_r, gb0_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(rg0_r, rg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gb0_r, gb0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    col += 8;
    inptr += 8 * 4;
  }

  if ((width & 0x7) >= 4) {
    int out_val;
    in0 = LD_UB(inptr);

    gb0 = __msa_pckod_h((v8i16) in0, (v8i16) in0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    rg0 = __msa_pckev_h((v8i16) in0, (v8i16) in0);

    rg0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) rg0);
    gb0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) gb0);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    tmp0 = __msa_dotp_s_w((v8i16) rg0_r, (v8i16) mult);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    tmp0 = __msa_dpadd_s_w(tmp0, (v8i16) gb0_r, (v8i16) mult);
    tmp0 = __msa_srari_w(tmp0, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) zero, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    out_val = __msa_copy_s_w((v4i32) out_y, 0);
    SW(out_val, (p_out_y + col));

    col += 4;
    inptr += 4 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[1]);
    g = GETSAMPLE(inptr[2]);
    b = GETSAMPLE(inptr[3]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b + ONE_HALF)
                              >> SCALE);
  }
}

void
xbgr_grey_convert_msa (JSAMPROW inptr, JSAMPROW p_out_y, JDIMENSION width)
{
  JDIMENSION col;
  int r, g, b;
  v16u8 in0, in1, in2, in3, in4, in5, in6, in7, out_y, out_y1;
  v8i16 bg0, gr0, bg1, gr1, mult, zero = {0};
  v8i16 bg0_r, gr0_r, bg1_r, gr1_r, bg0_l, gr0_l, bg1_l, gr1_l;
  v4i32 tmp0, tmp1, tmp2, tmp3;
  const v8i16 const0 = {7471, 22086, 16384, 19595, 0, 0, 0, 0};

  col = 0;

  for (; col < (width & ~0x1F); col += 32) {
    LD_UB8(inptr, 16, in0, in1, in2, in3, in4, in5, in6, in7);

    PCKOD_H2_SH(in1, in0, in3, in2, gr0, gr1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, bg0, bg1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    PCKOD_H2_SH(in5, in4, in7, in6, gr0, gr1);

    SLDI_B2_UB(in4, in5, in4, in5, in4, in5, 1);
    SLDI_B2_UB(in6, in7, in6, in7, in6, in7, 1);
    PCKEV_H2_SH(in5, in4, in7, in6, bg0, bg1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y1 = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);

    ST_UB2(out_y, out_y1, (p_out_y + col), 16);

    inptr += 32 * 4;
  }

  for (; col < (width & ~0xF); col += 16) {
    LD_UB4(inptr, 16, in0, in1, in2, in3);

    PCKOD_H2_SH(in1, in0, in3, in2, gr0, gr1);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    SLDI_B2_UB(in2, in3, in2, in3, in2, in3, 1);
    PCKEV_H2_SH(in1, in0, in3, in2, bg0, bg1);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(bg1, bg1_r, bg1_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);
    UNPCK_UB_SH(gr1, gr1_r, gr1_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH4_SW(bg0_r, bg0_l, bg1_r, bg1_l, mult, mult, mult, mult, tmp0, tmp1,
                tmp2, tmp3);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH4_SW(gr0_r, gr0_l, gr1_r, gr1_l, mult, mult, mult, mult, tmp0, tmp1,
                 tmp2, tmp3);
    SRARI_W4_SW(tmp0, tmp1, tmp2, tmp3, SCALE);
    PCKEV_H2_SW(tmp1, tmp0, tmp3, tmp2, tmp0, tmp2);
    out_y = (v16u8) __msa_pckev_b((v16i8) tmp2, (v16i8) tmp0);
    ST_UB(out_y, (p_out_y + col));

    inptr += 16 * 4;
  }

  if ((width & 0xF) >= 8) {
    LD_UB2(inptr, 16, in0, in1);

    gr0 = __msa_pckod_h((v8i16) in1, (v8i16) in0);

    SLDI_B2_UB(in0, in1, in0, in1, in0, in1, 1);
    bg0 = __msa_pckev_h((v8i16) in1, (v8i16) in0);

    UNPCK_UB_SH(bg0, bg0_r, bg0_l);
    UNPCK_UB_SH(gr0, gr0_r, gr0_l);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    DOTP_SH2_SW(bg0_r, bg0_l, mult, mult, tmp0, tmp1);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    DPADD_SH2_SW(gr0_r, gr0_l, mult, mult, tmp0, tmp1);
    SRARI_W2_SW(tmp0, tmp1, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) tmp1, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    ST8x1_UB(out_y, (p_out_y + col));

    col += 8;
    inptr += 8 * 4;
  }

  if ((width & 0x7) >= 4) {
    int out_val;
    in0 = LD_UB(inptr);

    gr0 = __msa_pckod_h((v8i16) in0, (v8i16) in0);

    in0 = (v16u8) __msa_sldi_b((v16i8) in0, (v16i8) in0, 1);
    bg0 = __msa_pckev_h((v8i16) in0, (v8i16) in0);

    bg0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) bg0);
    gr0_r = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) gr0);

    /* RGB --> Y */
    mult = (v8i16) __msa_splati_w((v4i32) const0, 0);
    tmp0 = __msa_dotp_s_w((v8i16) bg0_r, (v8i16) mult);
    mult = (v8i16) __msa_splati_w((v4i32) const0, 1);
    tmp0 = __msa_dpadd_s_w(tmp0, (v8i16) gr0_r, (v8i16) mult);
    tmp0 = __msa_srari_w(tmp0, SCALE);
    tmp0 = (v4i32) __msa_pckev_h((v8i16) zero, (v8i16) tmp0);
    out_y = (v16u8) __msa_pckev_b((v16i8) zero, (v16i8) tmp0);
    out_val = __msa_copy_s_w((v4i32) out_y, 0);
    SW(out_val, (p_out_y + col));

    col += 4;
    inptr += 4 * 4;
  }

  for (; col < width; col++) {
    r = GETSAMPLE(inptr[3]);
    g = GETSAMPLE(inptr[2]);
    b = GETSAMPLE(inptr[1]);
    inptr += 4;

    p_out_y[col] = (uint8_t) ((19595 * r + 38470 * g + 7471 * b + ONE_HALF)
                              >> SCALE);
  }
}

void
jsimd_rgb_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                            JSAMPIMAGE output_buf, JDIMENSION output_row,
                            int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    rgb_grey_convert_msa(inptr, p_out_y, img_width);
  }
}

void
jsimd_extrgb_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf, JDIMENSION output_row,
                               int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    rgb_grey_convert_msa(inptr, p_out_y, img_width);
  }
}

void
jsimd_extrgbx_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                                JSAMPIMAGE output_buf, JDIMENSION output_row,
                                int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    rgbx_grey_convert_msa(inptr, p_out_y, img_width);
  }
}

void
jsimd_extbgr_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf, JDIMENSION output_row,
                               int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    bgr_grey_convert_msa(inptr, p_out_y, img_width);
  }
}

void
jsimd_extbgrx_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                                JSAMPIMAGE output_buf, JDIMENSION output_row,
                                int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
      inptr = *input_buf++;
      bgrx_grey_convert_msa(inptr, p_out_y, img_width);
  }
}

void
jsimd_extxbgr_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                            JSAMPIMAGE output_buf, JDIMENSION output_row,
                            int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    xbgr_grey_convert_msa(inptr, p_out_y, img_width);
  }
}

void
jsimd_extxrgb_gray_convert_msa (JDIMENSION img_width, JSAMPARRAY input_buf,
                                JSAMPIMAGE output_buf, JDIMENSION output_row,
                                int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW p_out_y = output_buf[0][output_row];
  while (--num_rows >= 0) {
    inptr = *input_buf++;
    xrgb_grey_convert_msa(inptr, p_out_y, img_width);
  }
}
