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

#include "../../jinclude.h"
#include "../../jpeglib.h"

#include "jmacros_msa.h"

#define FIX_1_40200  0x166e9
#define FIX_0_34414  0x581a
#define FIX_0_71414  0xb6d2
#define FIX_1_77200  0x1c5a2
#define FIX_0_28586  (65536 - FIX_0_71414)

#define ROUND_POWER_OF_TWO(value, n)  (((value) + (1 << ((n) - 1))) >> (n))

static inline unsigned char clip_pixel (int val)
{
    return ((val & ~0xFF) ? ((-val) >> 31) & 0xFF : val);
}

/* Original
    G = Y - 0.34414 * Cb - 0.71414 * Cr */
/* This implementation
    G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */

#define CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0) {  \
  v8i16 tmp0_m, tmp1_m;                                                    \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                    \
  v8i16 const0_m = {-FIX_0_34414, FIX_0_28586, -FIX_0_34414,               \
                    FIX_0_28586, -FIX_0_34414, FIX_0_28586,                \
                    -FIX_0_34414, FIX_0_28586};                            \
                                                                           \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m);                               \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m);                                   \
  out0_m = MSA_SLLI_W(out0_m, 16);                                         \
  out1_m = MSA_SLLI_W(out1_m, 16);                                         \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m);        \
  ILVRL_H2_SH(cr_h1, cb_h1, tmp0_m, tmp1_m);                               \
  UNPCK_SH_SW((-cr_h1), out2_m, out3_m);                                   \
  out2_m = MSA_SLLI_W(out2_m, 16);                                         \
  out3_m = MSA_SLLI_W(out3_m, 16);                                         \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out2_m, out3_m);        \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                         \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);             \
  ADD2(y_h0, tmp0_m, y_h1, tmp1_m, tmp0_m, tmp1_m);                        \
  CLIP_SH2_0_255(tmp0_m, tmp1_m);                                          \
  out_g0 = __msa_pckev_b((v16i8) tmp1_m, (v16i8) tmp0_m);                  \
}

#define CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0) {                \
  v8i16 tmp0_m, tmp1_m;                                              \
  v4i32 out0_m, out1_m;                                              \
  v8i16 const0_m = {-FIX_0_34414, FIX_0_28586, -FIX_0_34414,         \
                    FIX_0_28586, -FIX_0_34414, FIX_0_28586,          \
                    -FIX_0_34414, FIX_0_28586};                      \
                                                                     \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m);                         \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m);                             \
  out0_m = MSA_SLLI_W(out0_m, 16);                                   \
  out1_m = MSA_SLLI_W(out1_m, 16);                                   \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                   \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);            \
  tmp0_m += y_h0;                                                    \
  tmp0_m = CLIP_SH_0_255(tmp0_m);                                    \
  out_g0 = __msa_pckev_b((v16i8) tmp0_m, (v16i8) tmp0_m);            \
}

#define CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0) {  \
  v8i16 tmp0_m, tmp1_m;                                                    \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                    \
  v4i32 const_1_40200_m = {FIX_1_40200, FIX_1_40200, FIX_1_40200,          \
                           FIX_1_40200};                                   \
                                                                           \
  MUL4(const_1_40200_m, cr_w0, const_1_40200_m, cr_w1,                     \
       const_1_40200_m, cr_w2, const_1_40200_m, cr_w3,                     \
       out0_m, out1_m, out2_m, out3_m);                                    \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                         \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);             \
  ADD2(y_h0, tmp0_m, y_h1, tmp1_m, tmp0_m, tmp1_m);                        \
  CLIP_SH2_0_255(tmp0_m, tmp1_m);                                          \
  out_r0 = __msa_pckev_b((v16i8) tmp1_m, (v16i8) tmp0_m);                  \
}

#define CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0) {                    \
  v8i16 tmp0_m;                                                          \
  v4i32 out0_m, out1_m;                                                  \
  v4i32 const_1_40200_m = {FIX_1_40200, FIX_1_40200, FIX_1_40200,        \
                           FIX_1_40200};                                 \
                                                                         \
  MUL2(const_1_40200_m, cr_w0, const_1_40200_m, cr_w1, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                       \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);                \
  tmp0_m += y_h0;                                                        \
  tmp0_m = CLIP_SH_0_255(tmp0_m);                                        \
  out_r0 = __msa_pckev_b((v16i8) tmp0_m, (v16i8) tmp0_m);                \
}

#define CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0) {  \
  v8i16 tmp0_m, tmp1_m;                                                    \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                    \
  v4i32 const_1_77200_m = {FIX_1_77200, FIX_1_77200, FIX_1_77200,          \
                           FIX_1_77200};                                   \
                                                                           \
  MUL4(const_1_77200_m, cb_w0, const_1_77200_m, cb_w1,                     \
       const_1_77200_m, cb_w2, const_1_77200_m, cb_w3,                     \
       out0_m, out1_m, out2_m, out3_m);                                    \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                         \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);             \
  ADD2(y_h0, tmp0_m, y_h1, tmp1_m, tmp0_m, tmp1_m);                        \
  CLIP_SH2_0_255(tmp0_m, tmp1_m);                                          \
  out_b0 = __msa_pckev_b((v16i8) tmp1_m, (v16i8) tmp0_m);                  \
}

#define CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0) {                    \
  v8i16 tmp0_m;                                                          \
  v4i32 out0_m, out1_m;                                                  \
  v4i32 const_1_77200_m = {FIX_1_77200, FIX_1_77200, FIX_1_77200,        \
                           FIX_1_77200};                                 \
                                                                         \
  MUL2(const_1_77200_m, cb_w0, const_1_77200_m, cb_w1, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                       \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);                \
  tmp0_m += y_h0;                                                        \
  tmp0_m = CLIP_SH_0_255(tmp0_m);                                        \
  out_b0 = __msa_pckev_b((v16i8) tmp0_m, (v16i8) tmp0_m);                \
}

void
yuv_rgb_convert_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                     JSAMPROW p_rgb, JDIMENSION out_width)
{
  int y, cb, cr;
  unsigned int col, num_cols_mul_16 = out_width >> 4;
  unsigned int remaining_wd = out_width & 0xF;
  v16u8 mask_rgb0 = {0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10};
  v16u8 mask_rgb1 = {11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5};
  v16u8 mask_rgb2 = {26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31};
  v16u8 tmp0, tmp1, out0, out1, out2, input_y = {0};
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, const_128 = __msa_ldi_b(128);
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};
  v16i8  out_r0, out_g0, out_b0;

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

    CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);

    VSHF_B2_UB(out_rgb0, out_b0, out_rgb0, out_b0, mask_rgb0, mask_rgb1,
               out0, tmp0);
    VSHF_B2_UB(out_rgb1, out_b0, out_rgb1, out_b0, mask_rgb1, mask_rgb2,
               tmp1, out2);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, (v16i8) tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    ST_UB(out0, p_rgb);
    p_rgb += 16;
    ST_UB(out1, p_rgb);
    p_rgb += 16;
    ST_UB(out2, p_rgb);
    p_rgb += 16;
  }

  if (remaining_wd >= 8) {
    uint64_t in_y, in_cb, in_cr;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 8;
    p_in_cb += 8;
    p_in_cr += 8;

    input_y = (v16u8) __msa_insert_d((v2i64) input_y, 0, in_y);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    y_h0 = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) input_y);
    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0);
    CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0);
    CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_r0);
    VSHF_B2_UB(out_rgb0, out_b0, out_rgb0, out_b0, mask_rgb0, mask_rgb1,
               out0, out1);

    ST_UB(out0, p_rgb);
    p_rgb += 16;
    ST8x1_UB(out1, p_rgb);
    p_rgb += 8;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col++) {
    y  = (int) (p_in_y[col]);
    cb = (int) (p_in_cb[col]) - 128;
    cr = (int) (p_in_cr[col]) - 128;

    /* Range-limiting is essential due to noise introduced by DCT losses. */
    p_rgb[0] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
    p_rgb[1] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                                   FIX_0_71414 * cr), 16));
    p_rgb[2] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
    p_rgb += 3;
  }
}

void
yuv_bgr_convert_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                     JSAMPROW p_rgb, JDIMENSION out_width)
{
  int32_t y, cb, cr;
  uint32_t col, num_cols_mul_16 = out_width >> 4;
  uint32_t remaining_wd = out_width & 0xF;
  v16u8 mask_rgb0 = {0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10};
  v16u8 mask_rgb1 = {11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5};
  v16u8 mask_rgb2 = {26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31};
  v16u8 tmp0, tmp1, out0, out1, out2, input_y = {0};
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, const_128 = __msa_ldi_b(128);
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};
  v16i8  out_r0, out_g0, out_b0;

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

    CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);

    VSHF_B2_UB(out_rgb0, out_r0, out_rgb0, out_r0, mask_rgb0, mask_rgb1,
               out0, tmp0);
    VSHF_B2_UB(out_rgb1, out_r0, out_rgb1, out_r0, mask_rgb1, mask_rgb2,
               tmp1, out2);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, (v16i8) tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    ST_UB(out0, p_rgb);
    p_rgb += 16;
    ST_UB(out1, p_rgb);
    p_rgb += 16;
    ST_UB(out2, p_rgb);
    p_rgb += 16;
  }

  if (remaining_wd >= 8) {
    uint64_t in_y, in_cb, in_cr;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 8;
    p_in_cb += 8;
    p_in_cr += 8;

    input_y = (v16u8) __msa_insert_d((v2i64) input_y, 0, in_y);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    y_h0 = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) input_y);
    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0);
    CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0);
    CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_b0);
    VSHF_B2_UB(out_rgb0, out_r0, out_rgb0, out_r0, mask_rgb0, mask_rgb1,
               out0, out1);

    ST_UB(out0, p_rgb);
    p_rgb += 16;
    ST8x1_UB(out1, p_rgb);
    p_rgb += 8;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col++) {
    y  = (int) (p_in_y[col]);
    cb = (int) (p_in_cb[col]) - 128;
    cr = (int) (p_in_cr[col]) - 128;

    /* Range-limiting is essential due to noise introduced by DCT losses. */
    p_rgb[0] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
    p_rgb[1] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                                   FIX_0_71414 * cr), 16));
    p_rgb[2] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
    p_rgb += 3;
  }
}

void
yuv_rgba_convert_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                      JSAMPROW p_rgb, JDIMENSION out_width)
{
  int y, cb, cr;
  unsigned int col, num_cols_mul_16 = out_width >> 4;
  unsigned int remaining_wd = out_width & 0xF;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 const_128 = __msa_ldi_b(128);
  v16u8 out0, out1, out2, out3, input_y = {0};
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_ba0, out_ba1;
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};
  v16i8  out_r0, out_g0, out_b0;

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

    CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(alpha, out_b0, out_ba0, out_ba1);

    ILVRL_H2_UB(out_ba0, out_rgb0, out0, out1);
    ILVRL_H2_UB(out_ba1, out_rgb1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_rgb, 16);
    p_rgb += 16 * 4;
  }

  if (remaining_wd >= 8) {
    uint64_t in_y, in_cb, in_cr;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 8;
    p_in_cb += 8;
    p_in_cr += 8;

    input_y = (v16u8) __msa_insert_d((v2i64) input_y, 0, in_y);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    y_h0 = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) input_y);
    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0);
    CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0);
    CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_r0);
    out_ba0 = (v16i8) __msa_ilvr_b(alpha, (v16i8) out_b0);
    ILVRL_H2_UB(out_ba0, out_rgb0, out0, out1);

    ST_UB2(out0, out1, p_rgb, 16);
    p_rgb += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col++) {
    y  = (int) (p_in_y[col]);
    cb = (int) (p_in_cb[col]) - 128;
    cr = (int) (p_in_cr[col]) - 128;

    /* Range-limiting is essential due to noise introduced by DCT losses. */
    p_rgb[0] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
    p_rgb[1] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                                   FIX_0_71414 * cr), 16));
    p_rgb[2] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
    p_rgb[3] = 0xFF;
    p_rgb += 4;
  }
}

void
yuv_bgra_convert_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                      JSAMPROW p_rgb, JDIMENSION out_width)
{
  int y, cb, cr;
  unsigned int col, num_cols_mul_16 = out_width >> 4;
  unsigned int remaining_wd = out_width & 0xF;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 const_128 = __msa_ldi_b(128);
  v16u8 out0, out1, out2, out3, input_y = {0};
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_ra0, out_ra1;
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};
  v16i8  out_r0, out_g0, out_b0;

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

    CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(alpha, out_r0, out_ra0, out_ra1);

    ILVRL_H2_UB(out_ra0, out_rgb0, out0, out1);
    ILVRL_H2_UB(out_ra1, out_rgb1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_rgb, 16);
    p_rgb += 16 * 4;
  }

  if (remaining_wd >= 8) {
    uint64_t in_y, in_cb, in_cr;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 8;
    p_in_cb += 8;
    p_in_cr += 8;

    input_y = (v16u8) __msa_insert_d((v2i64) input_y, 0, in_y);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    y_h0 = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) input_y);
    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0);
    CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0);
    CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_b0);
    out_ra0 = (v16i8) __msa_ilvr_b(alpha, (v16i8) out_r0);
    ILVRL_H2_UB(out_ra0, out_rgb0, out0, out1);

    ST_UB2(out0, out1, p_rgb, 16);
    p_rgb += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col++) {
    y  = (int) (p_in_y[col]);
    cb = (int) (p_in_cb[col]) - 128;
    cr = (int) (p_in_cr[col]) - 128;

    p_rgb[0] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
    p_rgb[1] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                                   FIX_0_71414 * cr), 16));
    p_rgb[2] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
    p_rgb[3] = 0xFF;
    p_rgb += 4;
  }
}

void
yuv_argb_convert_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                      JSAMPROW p_rgb, JDIMENSION out_width)
{
  int y, cb, cr;
  unsigned int col, num_cols_mul_16 = out_width >> 4;
  unsigned int remaining_wd = out_width & 0xF;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 const_128 = __msa_ldi_b(128);
  v16u8 out0, out1, out2, out3, input_y = {0};
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_ar0, out_ar1;
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};
  v16i8  out_r0, out_g0, out_b0;

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

    CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

    ILVRL_B2_SB(out_b0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_r0, alpha, out_ar0, out_ar1);

    ILVRL_H2_UB(out_rgb0, out_ar0, out0, out1);
    ILVRL_H2_UB(out_rgb1, out_ar1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_rgb, 16);
    p_rgb += 16 * 4;
  }

  if (remaining_wd >= 8) {
    uint64_t in_y, in_cb, in_cr;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 8;
    p_in_cb += 8;
    p_in_cr += 8;

    input_y = (v16u8) __msa_insert_d((v2i64) input_y, 0, in_y);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    y_h0 = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) input_y);
    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0);
    CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0);
    CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_b0, (v16i8) out_g0);
    out_ar0 = (v16i8) __msa_ilvr_b((v16i8) out_r0, alpha);
    ILVRL_H2_UB(out_rgb0, out_ar0, out0, out1);

    ST_UB2(out0, out1, p_rgb, 16);
    p_rgb += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col++) {
    y  = (int) (p_in_y[col]);
    cb = (int) (p_in_cb[col]) - 128;
    cr = (int) (p_in_cr[col]) - 128;

    p_rgb[0] = 0xFF;
    p_rgb[1] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
    p_rgb[2] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                                   FIX_0_71414 * cr), 16));
    p_rgb[3] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
    p_rgb += 4;
  }
}

void
yuv_abgr_convert_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                      JSAMPROW p_rgb, JDIMENSION out_width)
{
  int y, cb, cr;
  unsigned int col, num_cols_mul_16 = out_width >> 4;
  unsigned int remaining_wd = out_width & 0xF;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 const_128 = __msa_ldi_b(128);
  v16u8 out0, out1, out2, out3, input_y = {0};
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_ab0, out_ab1;
  v8i16 y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};
  v16i8  out_r0, out_g0, out_b0;

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

    CALC_G4_FRM_YUV(y_h0, y_h1, cb_h0, cb_h1, cr_h0, cr_h1, out_g0);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, cr_w2, cr_w3, out_r0);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, cb_w2, cb_w3, out_b0);

    ILVRL_B2_SB(out_r0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_b0, alpha, out_ab0, out_ab1);

    ILVRL_H2_UB(out_rgb0, out_ab0, out0, out1);
    ILVRL_H2_UB(out_rgb1, out_ab1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_rgb, 16);
    p_rgb += 16 * 4;
  }

  if (remaining_wd >= 8) {
    uint64_t in_y, in_cb, in_cr;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 8;
    p_in_cb += 8;
    p_in_cr += 8;

    input_y = (v16u8) __msa_insert_d((v2i64) input_y, 0, in_y);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    y_h0 = (v8i16) __msa_ilvr_b((v16i8) zero, (v16i8) input_y);
    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, cr_w0, cr_w1, out_r0);
    CALC_G2_FRM_YUV(y_h0, cb_h0, cr_h0, out_g0);
    CALC_B2_FRM_YUV(y_h0, cb_w0, cb_w1, out_b0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_r0, (v16i8) out_g0);
    out_ab0 = (v16i8) __msa_ilvr_b((v16i8) out_b0, alpha);
    ILVRL_H2_UB(out_rgb0, out_ab0, out0, out1);

    ST_UB2(out0, out1, p_rgb, 16);
    p_rgb += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col++) {
    y  = (int) (p_in_y[col]);
    cb = (int) (p_in_cb[col]) - 128;
    cr = (int) (p_in_cr[col]) - 128;

    p_rgb[0] = 0xFF;
    p_rgb[1] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16));
    p_rgb[2] = clip_pixel(y + ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb -
                                                   FIX_0_71414 * cr), 16));
    p_rgb[3] = clip_pixel(y + ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16));
    p_rgb += 4;
  }
}

void
jsimd_ycc_extrgb_convert_msa (JDIMENSION out_width,
                              JSAMPIMAGE input_buf, unsigned int input_row,
                              JSAMPARRAY output_buf, int num_rows)
{
  while (--num_rows >= 0) {
    yuv_rgb_convert_msa(input_buf[0][input_row], input_buf[1][input_row],
                        input_buf[2][input_row], *output_buf++, out_width);
    input_row++;
  }
}

void
jsimd_ycc_extrgbx_convert_msa (JDIMENSION out_width,
                               JSAMPIMAGE input_buf, unsigned int input_row,
                               JSAMPARRAY output_buf, int num_rows)
{
  while (--num_rows >= 0) {
    yuv_rgba_convert_msa(input_buf[0][input_row], input_buf[1][input_row],
                         input_buf[2][input_row], *output_buf++, out_width);
    input_row++;
  }
}

void
jsimd_ycc_extbgr_convert_msa (JDIMENSION out_width,
                              JSAMPIMAGE input_buf, unsigned int input_row,
                              JSAMPARRAY output_buf, int num_rows)
{
  while (--num_rows >= 0) {
    yuv_bgr_convert_msa(input_buf[0][input_row], input_buf[1][input_row],
                        input_buf[2][input_row], *output_buf++, out_width);
    input_row++;
  }
}

void
jsimd_ycc_extbgrx_convert_msa (JDIMENSION out_width,
                               JSAMPIMAGE input_buf, unsigned int input_row,
                               JSAMPARRAY output_buf, int num_rows)
{
  while (--num_rows >= 0) {
    yuv_bgra_convert_msa(input_buf[0][input_row], input_buf[1][input_row],
                         input_buf[2][input_row], *output_buf++, out_width);
    input_row++;
  }
}

void
jsimd_ycc_extxbgr_convert_msa (JDIMENSION out_width,
                               JSAMPIMAGE input_buf, unsigned int input_row,
                               JSAMPARRAY output_buf, int num_rows)
{
  while (--num_rows >= 0) {
    yuv_abgr_convert_msa(input_buf[0][input_row], input_buf[1][input_row],
                         input_buf[2][input_row], *output_buf++, out_width);
    input_row++;
  }
}

void
jsimd_ycc_extxrgb_convert_msa (JDIMENSION out_width,
                               JSAMPIMAGE input_buf, unsigned int input_row,
                               JSAMPARRAY output_buf, int num_rows)
{
  while (--num_rows >= 0) {
    yuv_argb_convert_msa(input_buf[0][input_row], input_buf[1][input_row],
                         input_buf[2][input_row], *output_buf++, out_width);
    input_row++;
  }
}
