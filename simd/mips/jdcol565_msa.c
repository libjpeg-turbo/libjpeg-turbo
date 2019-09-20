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

/* YCC --> RGB565 CONVERSION */

#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "jconfigint.h"
#include "jmacros_msa.h"


#define FIX_1_40200  0x166e9
#define FIX_0_34414  0x581a
#define FIX_0_71414  0xb6d2
#define FIX_1_77200  0x1c5a2
#define FIX_0_28586  (65536 - FIX_0_71414)

/* Original            ::: G = Y - 0.34414 * Cb - 0.71414 * Cr
   This implementation ::: G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */

#define CALC_G4_FRM_YCC(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, \
out_g0, out_g1) { \
  v8i16 tmp0_m, tmp1_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v8i16 const0_m = { -FIX_0_34414, FIX_0_28586, -FIX_0_34414, FIX_0_28586, \
                     -FIX_0_34414, FIX_0_28586, -FIX_0_34414, FIX_0_28586 \
                   }; \
  \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m); \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m); \
  out0_m = MSA_SLLI_W(out0_m, 16); \
  out1_m = MSA_SLLI_W(out1_m, 16); \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m); \
  ILVRL_H2_SH(cr_h1, cb_h1, tmp0_m, tmp1_m); \
  UNPCK_SH_SW((-cr_h1), out2_m, out3_m); \
  out2_m = MSA_SLLI_W(out2_m, 16); \
  out3_m = MSA_SLLI_W(out3_m, 16); \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD2(y_h0, tmp0_m, y_h1, tmp1_m, out_g0, out_g1); \
  CLIP_SH2_0_255(out_g0, out_g1); \
}

#define CALC_G2_FRM_YCC(y_h0, cb_h0, cr_h0, out_g0) { \
  v8i16 tmp0_m, tmp1_m; \
  v4i32 out0_m, out1_m; \
  v8i16 const0_m = { -FIX_0_34414, FIX_0_28586, -FIX_0_34414, FIX_0_28586, \
                     -FIX_0_34414, FIX_0_28586, -FIX_0_34414, FIX_0_28586 \
                   }; \
  \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m); \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m); \
  out0_m = MSA_SLLI_W(out0_m, 16); \
  out1_m = MSA_SLLI_W(out1_m, 16); \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  tmp0_m += y_h0; \
  out_g0 = CLIP_SH_0_255(tmp0_m); \
}

#define CALC_R4_FRM_YCC(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, \
out_r0, out_r1) { \
  v8i16 tmp0_m, tmp1_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v4i32 const0_m = { FIX_1_40200, FIX_1_40200, FIX_1_40200, FIX_1_40200 }; \
  \
  MUL4(const0_m, cr_w0, const0_m, cr_w1, const0_m, cr_w2, const0_m, cr_w3, \
       out0_m, out1_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD2(y_h0, tmp0_m, y_h1, tmp1_m, out_r0, out_r1); \
  CLIP_SH2_0_255(out_r0, out_r1); \
}

#define CALC_R2_FRM_YCC(y_h0, cr_w0, cr_w1, out_r0) { \
  v8i16 tmp0_m; \
  v4i32 out0_m, out1_m; \
  v4i32 const0_m = { FIX_1_40200, FIX_1_40200, FIX_1_40200, FIX_1_40200 }; \
  \
  MUL2(const0_m, cr_w0, const0_m, cr_w1, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  tmp0_m += y_h0; \
  out_r0 = CLIP_SH_0_255(tmp0_m); \
}

#define CALC_B4_FRM_YCC(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, \
out_b0, out_b1) { \
  v8i16 tmp0_m, tmp1_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v4i32 const0_m = { FIX_1_77200, FIX_1_77200, FIX_1_77200, FIX_1_77200 }; \
  \
  MUL4(const0_m, cb_w0, const0_m, cb_w1, const0_m, cb_w2, const0_m, cb_w3, \
       out0_m, out1_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD2(y_h0, tmp0_m, y_h1, tmp1_m, out_b0, out_b1); \
  CLIP_SH2_0_255(out_b0, out_b1); \
}

#define CALC_B2_FRM_YCC(y_h0, cb_w0, cb_w1, out_b0) { \
  v8i16 tmp0_m; \
  v4i32 out0_m, out1_m; \
  v4i32 const0_m = { FIX_1_77200, FIX_1_77200, FIX_1_77200, FIX_1_77200 }; \
  \
  MUL2(const0_m, cb_w0, const0_m, cb_w1, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  tmp0_m += y_h0; \
  out_b0 = CLIP_SH_0_255(tmp0_m); \
}

#define ROUND_POWER_OF_TWO(value, n)  (((value) + (1 << ((n) - 1))) >> (n))

static INLINE unsigned char clip_pixel(int val)
{
  return ((val & ~0xFF) ? ((-val) >> 31) & 0xFF : val);
}

GLOBAL(void)
jsimd_ycc_rgb565_convert_msa(JDIMENSION out_width,
                             JSAMPIMAGE input_buf, JDIMENSION input_row,
                             JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW p_in_y, p_in_cb, p_in_cr;
  register JSAMPROW p_rgb;
  int r, g, b, y, cb, cr;
  JDIMENSION col, remaining_wd, num_cols_mul_16 = out_width >> 4;
  v16u8 out0, out1, input_y = { 0 };
  v16i8 input_cb, input_cr, const_128 = __msa_ldi_b(128);
  v8i16 out_r0, out_r1, out_g0, out_g1, out_b0, out_b1;
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = { 0 };

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

      CALC_G4_FRM_YCC(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0, out_g1);

      UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
      UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
      CALC_R4_FRM_YCC(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0, out_r1);

      UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
      UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
      CALC_B4_FRM_YCC(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0, out_b1);

      out_r0 = MSA_SLLI_B(out_r0, 8);
      out_r1 = MSA_SLLI_B(out_r1, 8);
      out_g0 = MSA_SLLI_B(out_g0, 3);
      out_g1 = MSA_SLLI_B(out_g1, 3);
      out_b0 = MSA_SRAI_B(out_b0, 3);
      out_b1 = MSA_SRAI_B(out_b1, 3);

      out0 = (v16u8)__msa_binsli_h((v8u16)out_g0, (v8u16)out_r0, 4);
      out1 = (v16u8)__msa_binsli_h((v8u16)out_g1, (v8u16)out_r1, 4);
      out0 = (v16u8)__msa_binsli_h((v8u16)out_b0, (v8u16)out0, 10);
      out1 = (v16u8)__msa_binsli_h((v8u16)out_b1, (v8u16)out1, 10);

      ST_UB(out0, p_rgb);
      p_rgb += 16;
      ST_UB(out1, p_rgb);
      p_rgb += 16;
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

      out_r0 = MSA_SLLI_B(out_r0, 8);
      out_g0 = MSA_SLLI_B(out_g0, 3);
      out_b0 = MSA_SRAI_B(out_b0, 3);

      out0 = (v16u8)__msa_binsli_h((v8u16)out_g0, (v8u16)out_r0, 4);
      out0 = (v16u8)__msa_binsli_h((v8u16)out_b0, (v8u16)out0, 10);

      ST_UB(out0, p_rgb);
      p_rgb += 16;

      remaining_wd -= 8;
    }

    for (col = 0; col < remaining_wd; col++) {
      y  = (int)(p_in_y[col]);
      cb = (int)(p_in_cb[col]) - 128;
      cr = (int)(p_in_cr[col]) - 128;

      /* Range-limiting is essential due to noise introduced by DCT losses. */
      r = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
      g = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                             FIX_0_71414 * cr), 16));
      b = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
      *p_rgb++ = ((g << 3) & 0xE0) | (b >> 3);
      *p_rgb++ = (r & 0xF8) | (g >> 5);
    }
  }
}
