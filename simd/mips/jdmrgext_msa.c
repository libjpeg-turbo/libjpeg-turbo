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

#include "../../jinclude.h"
#include "../../jpeglib.h"

#include "jmacros_msa.h"

#define FIX_1_40200  0x166e9
#define FIX_0_34414  0x581a
#define FIX_0_71414  0xb6d2
#define FIX_1_77200  0x1c5a2
#define FIX_0_28586  (65536 - FIX_0_71414)

#define ROUND_POWER_OF_TWO(value, n)  (((value) + (1 << ((n) - 1))) >> (n))

static inline unsigned char clip_pixel (int val) {
    return ((val & ~0xFF) ? ((-val) >> 31) & 0xFF : val);
}

#define GETSAMPLE(value)  ((int) (value))

/* Original
    G = Y - 0.34414 * Cb - 0.71414 * Cr */
/* This implementation
    G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */

#define CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,  \
                        out_g0, out_g1) {                                    \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                      \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                      \
  v8i16 const0_m = {-FIX_0_34414, FIX_0_28586, -FIX_0_34414,                 \
                    FIX_0_28586, -FIX_0_34414, FIX_0_28586,                  \
                    -FIX_0_34414, FIX_0_28586};                              \
                                                                             \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m);                                 \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m);                                     \
  out0_m = MSA_SLLI_W(out0_m, 16);                                           \
  out1_m = MSA_SLLI_W(out1_m, 16);                                           \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m);          \
  ILVRL_H2_SH(cr_h1, cb_h1, tmp0_m, tmp1_m);                                 \
  UNPCK_SH_SW((-cr_h1), out2_m, out3_m);                                     \
  out2_m = MSA_SLLI_W(out2_m, 16);                                           \
  out3_m = MSA_SLLI_W(out3_m, 16);                                           \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out2_m, out3_m);          \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                           \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);               \
  ADD4(y_h2, tmp0_m, y_h3, tmp1_m, y_h0, tmp0_m, y_h1, tmp1_m, tmp2_m,       \
       tmp3_m, tmp0_m, tmp1_m);                                              \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m);                            \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_g0, out_g1);               \
}

#define CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, out_g0, out_g1) {  \
  v8i16 tmp0_m, tmp1_m, tmp2_m;                                      \
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
  ADD2(y_h1, tmp0_m, y_h0, tmp0_m, tmp2_m, tmp0_m);                  \
  CLIP_SH2_0_255(tmp0_m, tmp2_m);                                    \
  PCKEV_B2_SB(tmp0_m, tmp0_m, tmp2_m, tmp2_m, out_g0, out_g1);       \
}

#define CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,  \
                        out_r0, out_r1) {                                    \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                      \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                      \
  v4i32 const_1_40200_m = {FIX_1_40200, FIX_1_40200, FIX_1_40200,            \
                           FIX_1_40200};                                     \
                                                                             \
  MUL4(const_1_40200_m, cr_w0, const_1_40200_m, cr_w1,                       \
       const_1_40200_m, cr_w2, const_1_40200_m, cr_w3,                       \
       out0_m, out1_m, out2_m, out3_m);                                      \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                           \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);               \
  ADD4(y_h2, tmp0_m, y_h3, tmp1_m, y_h0, tmp0_m, y_h1, tmp1_m, tmp2_m,       \
       tmp3_m, tmp0_m, tmp1_m);                                              \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m);                            \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_r0, out_r1);               \
}

#define CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, out_r0, out_r1) {      \
  v8i16 tmp0_m, tmp2_m;                                                  \
  v4i32 out0_m, out1_m;                                                  \
  v4i32 const_1_40200_m = {FIX_1_40200, FIX_1_40200, FIX_1_40200,        \
                           FIX_1_40200};                                 \
                                                                         \
  MUL2(const_1_40200_m, cr_w0, const_1_40200_m, cr_w1, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                       \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);                \
  ADD2(y_h1, tmp0_m, y_h0, tmp0_m, tmp2_m, tmp0_m);                      \
  CLIP_SH2_0_255(tmp0_m, tmp2_m);                                        \
  PCKEV_B2_SB(tmp0_m, tmp0_m, tmp2_m, tmp2_m, out_r0, out_r1);           \
}

#define CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,  \
                        out_b0, out_b1) {                                    \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                      \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                      \
  v4i32 const_1_77200_m = {FIX_1_77200, FIX_1_77200, FIX_1_77200,            \
                           FIX_1_77200};                                     \
                                                                             \
  MUL4(const_1_77200_m, cb_w0, const_1_77200_m, cb_w1,                       \
       const_1_77200_m, cb_w2, const_1_77200_m, cb_w3,                       \
       out0_m, out1_m, out2_m, out3_m);                                      \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                           \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);               \
  ADD4(y_h2, tmp0_m, y_h3, tmp1_m, y_h0, tmp0_m, y_h1, tmp1_m, tmp2_m,       \
       tmp3_m, tmp0_m, tmp1_m);                                              \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m);                            \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_b0, out_b1);               \
}

#define CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, out_b0, out_b1) {      \
  v8i16 tmp0_m, tmp2_m;                                                  \
  v4i32 out0_m, out1_m;                                                  \
  v4i32 const_1_77200_m = {FIX_1_77200, FIX_1_77200, FIX_1_77200,        \
                           FIX_1_77200};                                 \
                                                                         \
  MUL2(const_1_77200_m, cb_w0, const_1_77200_m, cb_w1, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                       \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);                \
  ADD2(y_h1, tmp0_m, y_h0, tmp0_m, tmp2_m, tmp0_m);                      \
  CLIP_SH2_0_255(tmp0_m, tmp2_m);                                        \
  PCKEV_B2_SB(tmp0_m, tmp0_m, tmp2_m, tmp2_m, out_b0, out_b1);           \
}

#define CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7,      \
                         cb_h0, cb_h1, cr_h0, cr_h1, out_g0, out_g1, out_g2,  \
                         out_g3) {                                            \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m;       \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                       \
  v8i16 const0_m = {-FIX_0_34414, FIX_0_28586, -FIX_0_34414,                  \
                    FIX_0_28586, -FIX_0_34414, FIX_0_28586,                   \
                    -FIX_0_34414, FIX_0_28586};                               \
                                                                              \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m);                                  \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m);                                      \
  out0_m = MSA_SLLI_W(out0_m, 16);                                            \
  out1_m = MSA_SLLI_W(out1_m, 16);                                            \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m);           \
  ILVRL_H2_SH(cr_h1, cb_h1, tmp0_m, tmp1_m);                                  \
  UNPCK_SH_SW((-cr_h1), out2_m, out3_m);                                      \
  out2_m = MSA_SLLI_W(out2_m, 16);                                            \
  out3_m = MSA_SLLI_W(out3_m, 16);                                            \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out2_m, out3_m);           \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                            \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);                \
  ADD4(y_h4, tmp0_m, y_h5, tmp1_m, y_h6, tmp0_m, y_h7, tmp1_m, tmp4_m,        \
       tmp5_m, tmp6_m, tmp7_m);                                               \
  CLIP_SH4_0_255(tmp4_m, tmp5_m, tmp6_m, tmp7_m);                             \
  PCKEV_B2_SB(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out_g2, out_g3);                \
  ADD4(y_h3, tmp1_m, y_h2, tmp0_m, y_h1, tmp1_m, y_h0, tmp0_m, tmp3_m,        \
       tmp2_m, tmp1_m, tmp0_m);                                               \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m);                             \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_g0, out_g1);                \
}

#define CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, out_g0,  \
                         out_g1) {                                      \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                 \
  v4i32 out0_m, out1_m;                                                 \
  v8i16 const0_m = {-FIX_0_34414, FIX_0_28586, -FIX_0_34414,            \
                    FIX_0_28586, -FIX_0_34414, FIX_0_28586,             \
                    -FIX_0_34414, FIX_0_28586};                         \
                                                                        \
  ILVRL_H2_SH(cr_h0, cb_h0, tmp0_m, tmp1_m);                            \
  UNPCK_SH_SW((-cr_h0), out0_m, out1_m);                                \
  out0_m = MSA_SLLI_W(out0_m, 16);                                      \
  out1_m = MSA_SLLI_W(out1_m, 16);                                      \
  DPADD_SH2_SW(tmp0_m, tmp1_m, const0_m, const0_m, out0_m, out1_m);     \
  SRARI_W2_SW(out0_m, out1_m, 16);                                      \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);               \
  ADD4(y_h3, tmp0_m, y_h2, tmp0_m, y_h1, tmp0_m, y_h0, tmp0_m, tmp3_m,  \
       tmp1_m, tmp2_m, tmp0_m);                                         \
  CLIP_SH4_0_255(tmp0_m, tmp2_m, tmp1_m, tmp3_m);                       \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_g0, out_g1);          \
}

#define CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7,      \
                         cr_w0, cr_w1, cr_w2, cr_w3, out_r0, out_r1, out_r2,  \
                         out_r3) {                                            \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m;       \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                       \
  v4i32 const_1_40200_m = {FIX_1_40200, FIX_1_40200, FIX_1_40200,             \
                           FIX_1_40200};                                      \
                                                                              \
  MUL4(const_1_40200_m, cr_w0, const_1_40200_m, cr_w1,                        \
       const_1_40200_m, cr_w2, const_1_40200_m, cr_w3,                        \
       out0_m, out1_m, out2_m, out3_m);                                       \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                            \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);                \
  ADD4(y_h4, tmp0_m, y_h5, tmp1_m, y_h6, tmp0_m, y_h7, tmp1_m, tmp4_m,        \
       tmp5_m, tmp6_m, tmp7_m);                                               \
  CLIP_SH4_0_255(tmp4_m, tmp5_m, tmp6_m, tmp7_m);                             \
  PCKEV_B2_SB(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out_r2, out_r3);                \
  ADD4(y_h3, tmp1_m, y_h2, tmp0_m, y_h1, tmp1_m, y_h0, tmp0_m, tmp3_m,        \
       tmp2_m, tmp1_m, tmp0_m);                                               \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m);                             \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_r0, out_r1);                \
}

#define CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, out_r0,   \
                         out_r1) {                                       \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                  \
  v4i32 out0_m, out1_m;                                                  \
  v4i32 const_1_40200_m = {FIX_1_40200, FIX_1_40200, FIX_1_40200,        \
                           FIX_1_40200};                                 \
                                                                         \
  MUL2(const_1_40200_m, cr_w0, const_1_40200_m, cr_w1, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                       \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);                \
  ADD4(y_h3, tmp0_m, y_h2, tmp0_m, y_h1, tmp0_m, y_h0, tmp0_m, tmp3_m,   \
       tmp1_m, tmp2_m, tmp0_m);                                          \
  CLIP_SH4_0_255(tmp0_m, tmp2_m, tmp1_m, tmp3_m);                        \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_r0, out_r1);           \
}

#define CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7,      \
                         cb_w0, cb_w1, cb_w2, cb_w3, out_b0, out_b1, out_b2,  \
                         out_b3) {                                            \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m;       \
  v4i32 out0_m, out1_m, out2_m, out3_m;                                       \
  v4i32 const_1_77200_m = {FIX_1_77200, FIX_1_77200, FIX_1_77200,             \
                           FIX_1_77200};                                      \
                                                                              \
  MUL4(const_1_77200_m, cb_w0, const_1_77200_m, cb_w1,                        \
       const_1_77200_m, cb_w2, const_1_77200_m, cb_w3,                        \
       out0_m, out1_m, out2_m, out3_m);                                       \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16);                            \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);                \
  ADD4(y_h4, tmp0_m, y_h5, tmp1_m, y_h6, tmp0_m, y_h7, tmp1_m, tmp4_m,        \
       tmp5_m, tmp6_m, tmp7_m);                                               \
  CLIP_SH4_0_255(tmp4_m, tmp5_m, tmp6_m, tmp7_m);                             \
  PCKEV_B2_SB(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out_b2, out_b3);                \
  ADD4(y_h3, tmp1_m, y_h2, tmp0_m, y_h1, tmp1_m, y_h0, tmp0_m, tmp3_m,        \
       tmp2_m, tmp1_m, tmp0_m);                                               \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m);                             \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_b0, out_b1);                \
}

#define CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, out_b0,   \
                         out_b1) {                                       \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                  \
  v4i32 out0_m, out1_m;                                                  \
  v4i32 const_1_77200_m = {FIX_1_77200, FIX_1_77200, FIX_1_77200,        \
                           FIX_1_77200};                                 \
                                                                         \
  MUL2(const_1_77200_m, cb_w0, const_1_77200_m, cb_w1, out0_m, out1_m);  \
  SRARI_W2_SW(out0_m, out1_m, 16);                                       \
  tmp0_m = __msa_pckev_h((v8i16) out1_m, (v8i16) out0_m);                \
  ADD4(y_h3, tmp0_m, y_h2, tmp0_m, y_h1, tmp0_m, y_h0, tmp0_m, tmp3_m,   \
       tmp1_m, tmp2_m, tmp0_m);                                          \
  CLIP_SH4_0_255(tmp0_m, tmp2_m, tmp1_m, tmp3_m);                        \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_b0, out_b1);           \
}

void
yuv_rgb_upsample_h2v1_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb,
                           JSAMPROW p_in_cr, JSAMPROW p_out,
                           JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 mask_rgb0 = {0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10};
  v16u8 mask_rgb1 = {11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5};
  v16u8 mask_rgb2 = {26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31};
  v16u8 out0, out1, out2, out3, out4, out5, input_y0 = {0}, input_y1;
  v16i8 tmp0, tmp1, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_r1, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_b1, out_rgb3, out_b1, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_b1, out_rgb3, out_b1, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;
    ST_UB2(out4, out5, p_out, 16);
    p_out += 16 * 2;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    tmp1 = LD_SB(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) zero, tmp1);
    tmp1 = __msa_pckod_b((v16i8) zero, tmp1);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);

    VSHF_B2_UB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    ST_UB(out0, p_out);
    p_out += 16;
    ST_UB(out1, p_out);
    p_out += 16;
    ST_UB(out2, p_out);
    p_out += 16;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8) zero, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) zero, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_r0);
    VSHF_B2_UB(out_rgb0, out_b0, out_rgb0, out_b0, mask_rgb0, mask_rgb1,
               out0, out1);

    ST_UB(out0, p_out);
    p_out += 16;
    ST8x1_UB(out1, p_out);
    p_out += 8;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out += 3;

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out += 3;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
  }
}

void
yuv_bgr_upsample_h2v1_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb,
                           JSAMPROW p_in_cr, JSAMPROW p_out,
                           JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 mask_rgb0 = {0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10};
  v16u8 mask_rgb1 = {11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5};
  v16u8 mask_rgb2 = {26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31};
  v16u8 out0, out1, out2, out3, out4, out5, input_y0 = {0}, input_y1;
  v16i8 tmp0, tmp1, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_b1, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);
    VSHF_B2_UB(out_rgb2, out_r1, out_rgb3, out_r1, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_r1, out_rgb3, out_r1, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;
    ST_UB2(out4, out5, p_out, 16);
    p_out += 16 * 2;
  }

  if (remaining_wd >= 16) {
     uint64_t in_cb, in_cr;
     v16i8 input_cbcr = {0};

     tmp1 = LD_SB(p_in_y);
     in_cb = LD(p_in_cb);
     in_cr = LD(p_in_cr);

     p_in_y += 16;
     p_in_cb += 8;
     p_in_cr += 8;

     tmp0 = __msa_pckev_b((v16i8) zero, tmp1);
     tmp1 = __msa_pckod_b((v16i8) zero, tmp1);
     input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
     UNPCK_UB_SH(input_y0, y_h0, y_h1);

     input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
     input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

     input_cbcr -= const_128;

     UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
     UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
     UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

     CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
     out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
     CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
     out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
     CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
     out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

     ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);

     VSHF_B2_UB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb0, mask_rgb2,
                out0, out2);
     VSHF_B2_SB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb1, mask_rgb1,
                tmp0, tmp1);
     out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
     out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

     ST_UB(out0, p_out);
     p_out += 16;
     ST_UB(out1, p_out);
     p_out += 16;
     ST_UB(out2, p_out);
     p_out += 16;

     remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8) zero, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) zero, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_b0);
    VSHF_B2_UB(out_rgb0, out_r0, out_rgb0, out_r0, mask_rgb0, mask_rgb1,
               out0, out1);

    ST_UB(out0, p_out);
    p_out += 16;
    ST8x1_UB(out1, p_out);
    p_out += 8;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out += 3;

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out += 3;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    y = (int) (*p_in_y);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
  }
}

void
yuv_rgba_upsample_h2v1_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb,
                            JSAMPROW p_in_cr, JSAMPROW p_out,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 tmp0, tmp1, tmp2, tmp3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_r1, out_rgb2, out_rgb3);
    ILVRL_B2_SB(alpha, out_b0, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_b1, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out, 16);
    p_out += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    tmp1 = LD_SB(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) zero, tmp1);
    tmp1 = __msa_pckod_b((v16i8) zero, tmp1);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(alpha, out_b0, tmp0, tmp1);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;
    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8) zero, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) zero, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_r0);
    tmp0 = (v16i8) __msa_ilvr_b(alpha, (v16i8) out_b0);
    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);

    ST_UB2(out0, out1, p_out, 16);
    p_out += 32;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out[3] = 0xFF;
    p_out += 4;

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out[3] = 0xFF;
    p_out += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out[3] = 0xFF;
  }
}

void
yuv_bgra_upsample_h2v1_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb,
                            JSAMPROW p_in_cr, JSAMPROW p_out,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 tmp0, tmp1, tmp2, tmp3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_b1, out_rgb2, out_rgb3);
    ILVRL_B2_SB(alpha, out_r0, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_r1, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out, 16);
    p_out += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    tmp1 = LD_SB(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) zero, tmp1);
    tmp1 = __msa_pckod_b((v16i8) zero, tmp1);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(alpha, out_r0, tmp0, tmp1);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8) zero, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) zero, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_b0);
    tmp0 = (v16i8) __msa_ilvr_b(alpha, (v16i8) out_r0);
    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);

    ST_UB2(out0, out1, p_out, 16);
    p_out += 32;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out[3] = 0xFF;
    p_out += 4;

    y = (int) (*p_in_y++);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out[3] = 0xFF;
    p_out += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    y = (int) (*p_in_y);
    p_out[0] = clip_pixel(y + cred);
    p_out[1] = clip_pixel(y + cgreen);
    p_out[2] = clip_pixel(y + cblue);
    p_out[3] = 0xFF;
  }
}

void
yuv_argb_upsample_h2v1_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb,
                            JSAMPROW p_in_cr, JSAMPROW p_out,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 tmp0, tmp1, tmp2, tmp3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_b0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_b1, out_g1, out_rgb2, out_rgb3);
    ILVRL_B2_SB(out_r0, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_r1, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out, 16);
    p_out += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    tmp1 = LD_SB(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) zero, tmp1);
    tmp1 = __msa_pckod_b((v16i8) zero, tmp1);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    ILVRL_B2_SB(out_b0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_r0, alpha, tmp0, tmp1);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8) zero, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) zero, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_b0, (v16i8) out_g0);
    tmp0 = (v16i8) __msa_ilvr_b((v16i8) out_r0, alpha);
    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);

    ST_UB2(out0, out1, p_out, 16);
    p_out += 32;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y++);
    p_out[0] = 0xFF;
    p_out[1] = clip_pixel(y + cred);
    p_out[2] = clip_pixel(y + cgreen);
    p_out[3] = clip_pixel(y + cblue);
    p_out += 4;

    y = (int) (*p_in_y++);
    p_out[0] = 0xFF;
    p_out[1] = clip_pixel(y + cred);
    p_out[2] = clip_pixel(y + cgreen);
    p_out[3] = clip_pixel(y + cblue);
    p_out += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y);
    p_out[0] = 0xFF;
    p_out[1] = clip_pixel(y + cred);
    p_out[2] = clip_pixel(y + cgreen);
    p_out[3] = clip_pixel(y + cblue);
  }
}

void
yuv_abgr_upsample_h2v1_msa (JSAMPROW p_in_y, JSAMPROW p_in_cb,
                            JSAMPROW p_in_cr, JSAMPROW p_out,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 tmp0, tmp1, tmp2, tmp3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, out_rgb0, out_rgb1, out_rgb2, out_rgb3;
  v16i8  out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1;
  v8i16 y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y, 16, tmp0, tmp1);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3,
                    tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_r0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_r1, out_g1, out_rgb2, out_rgb3);
    ILVRL_B2_SB(out_b0, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_b1, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out, 16);
    p_out += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    tmp1 = LD_SB(p_in_y);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) zero, tmp1);
    tmp1 = __msa_pckod_b((v16i8) zero, tmp1);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    ILVRL_B2_SB(out_r0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_b0, alpha, tmp0, tmp1);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);

    ST_UB4(out0, out1, out2, out3, p_out, 16);
    p_out += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y;
    v16i8 input_cbcr = {0};

    in_y = LD(p_in_y);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y);
    tmp0 = __msa_pckev_b((v16i8) zero, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) zero, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV(y_h0, y_h1, cr_w0, cr_w1, tmp0, tmp1);
    out_r0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_G2_FRM_YUV(y_h0, y_h1, cb_h0, cr_h0, tmp0, tmp1);
    out_g0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);
    CALC_B2_FRM_YUV(y_h0, y_h1, cb_w0, cb_w1, tmp0, tmp1);
    out_b0 = (v16i8) __msa_ilvr_b((v16i8) tmp1, (v16i8) tmp0);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_r0, (v16i8) out_g0);
    tmp0 = (v16i8) __msa_ilvr_b((v16i8) out_b0, alpha);
    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);

    ST_UB2(out0, out1, p_out, 16);
    p_out += 32;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y++);
    p_out[0] = 0xFF;
    p_out[1] = clip_pixel(y + cred);
    p_out[2] = clip_pixel(y + cgreen);
    p_out[3] = clip_pixel(y + cblue);
    p_out += 4;

    y = (int) (*p_in_y++);
    p_out[0] = 0xFF;
    p_out[1] = clip_pixel(y + cred);
    p_out[2] = clip_pixel(y + cgreen);
    p_out[3] = clip_pixel(y + cblue);
    p_out += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y);
    p_out[0] = 0xFF;
    p_out[1] = clip_pixel(y + cred);
    p_out[2] = clip_pixel(y + cgreen);
    p_out[3] = clip_pixel(y + cblue);
  }
}

void
yuv_rgb_upsample_h2v2_msa (JSAMPROW p_in_y0, JSAMPROW p_in_y1,
                           JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                           JSAMPROW p_out0, JSAMPROW p_out1,
                           JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 mask_rgb0 = {0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10};
  v16u8 mask_rgb1 = {11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5};
  v16u8 mask_rgb2 = {26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31};
  v16u8 out0, out1, out2, out3, out4, out5;
  v16u8 input_y0 = {0}, input_y1 = {0}, input_y2, input_y3;
  v16i8 out_rgb0, out_rgb1, out_rgb2, out_rgb3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, tmp0, tmp1, tmp2, tmp3;
  v16i8 out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16i8 out_r2, out_g2, out_b2, out_r3, out_g3, out_b3;
  v8i16 y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7;
  v8i16 cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y0, 16, tmp0, tmp1);
    LD_SB2(p_in_y1, 16, tmp2, tmp3);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y0 += 32;
    p_in_y1 += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);
    input_y2 = (v16u8) __msa_pckev_b(tmp3, tmp2);
    input_y3 = (v16u8) __msa_pckod_b(tmp3, tmp2);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_UB_SH(input_y2, y_h4, y_h5);
    UNPCK_UB_SH(input_y3, y_h6, y_h7);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_h0,
                     cb_h1, cr_h0, cr_h1, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    ILVRL_B2_SB(tmp3, tmp2, out_g2, out_g3);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cr_w0,
                     cr_w1, cr_w2, cr_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    ILVRL_B2_SB(tmp3, tmp2, out_r2, out_r3);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_w0,
                     cb_w1, cb_w2, cb_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);
    ILVRL_B2_SB(tmp3, tmp2, out_b2, out_b3);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_r1, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_b1, out_rgb3, out_b1, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_b1, out_rgb3, out_b1, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB4(out0, out1, out2, out3, p_out0, 16);
    p_out0 += 64;
    ST_UB2(out4, out5, p_out0, 16);
    p_out0 += 32;

    ILVRL_B2_SB(out_g2, out_r2, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g3, out_r3, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_b2, out_rgb1, out_b2, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_b2, out_rgb1, out_b2, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_b3, out_rgb3, out_b3, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_b3, out_rgb3, out_b3, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB4(out0, out1, out2, out3, p_out1, 16);
    p_out1 += 64;
    ST_UB2(out4, out5, p_out1, 16);
    p_out1 += 32;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    input_y0 = LD_UB(p_in_y0);
    input_y1 = LD_UB(p_in_y1);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y0 += 16;
    p_in_y1 += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_r1, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_b0, out_rgb1, out_b0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_b1, out_rgb3, out_b1, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_b1, out_rgb3, out_b1, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB(out0, p_out0);
    p_out0 += 16;
    ST_UB(out1, p_out0);
    p_out0 += 16;
    ST_UB(out2, p_out0);
    p_out0 += 16;

    ST_UB(out3, p_out1);
    p_out1 += 16;
    ST_UB(out4, p_out1);
    p_out1 += 16;
    ST_UB(out5, p_out1);
    p_out1 += 16;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y0, in_y1;
    v16i8 input_cbcr = {0};

    in_y0 = LD(p_in_y0);
    in_y1 = LD(p_in_y1);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y0 += 8;
    p_in_y1 += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y0);
    input_y1 = (v16u8) __msa_insert_d((v2i64) input_y1, 0, in_y1);
    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_r0);
    out_rgb1 = (v16i8) __msa_ilvr_b((v16i8) out_g1, (v16i8) out_r1);

    VSHF_B2_UB(out_rgb0, out_b0, out_rgb0, out_b0, mask_rgb0, mask_rgb1,
               out0, out1);
    VSHF_B2_UB(out_rgb1, out_b1, out_rgb1, out_b1, mask_rgb0, mask_rgb1,
               out2, out3);

    ST_UB(out0, p_out0);
    p_out0 += 16;
    ST8x1_UB(out1, p_out0);
    p_out0 += 8;

    ST_UB(out2, p_out1);
    p_out1 += 16;
    ST8x1_UB(out3, p_out1);
    p_out1 += 8;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0 += 3;

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0 += 3;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1 += 3;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1 += 3;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y0);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);

    y = (int) (*p_in_y1);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
  }
}

void
yuv_bgr_upsample_h2v2_msa (JSAMPROW p_in_y0, JSAMPROW p_in_y1,
                           JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                           JSAMPROW p_out0, JSAMPROW p_out1,
                           JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 mask_rgb0 = {0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19, 8, 9, 20, 10};
  v16u8 mask_rgb1 = {11, 21, 12, 13, 22, 14, 15, 23, 0, 1, 24, 2, 3, 25, 4, 5};
  v16u8 mask_rgb2 = {26, 6, 7, 27, 8, 9, 28, 10, 11, 29, 12, 13, 30, 14, 15, 31};
  v16u8 out0, out1, out2, out3, out4, out5;
  v16u8 input_y0 = {0}, input_y1 = {0}, input_y2, input_y3;
  v16i8 out_rgb0, out_rgb1, out_rgb2, out_rgb3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, tmp0, tmp1, tmp2, tmp3;
  v16i8 out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16i8 out_r2, out_g2, out_b2, out_r3, out_g3, out_b3;
  v8i16 y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7;
  v8i16 cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3, zero = {0};

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y0, 16, tmp0, tmp1);
    LD_SB2(p_in_y1, 16, tmp2, tmp3);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y0 += 32;
    p_in_y1 += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);
    input_y2 = (v16u8) __msa_pckev_b(tmp3, tmp2);
    input_y3 = (v16u8) __msa_pckod_b(tmp3, tmp2);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_UB_SH(input_y2, y_h4, y_h5);
    UNPCK_UB_SH(input_y3, y_h6, y_h7);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_h0,
                     cb_h1, cr_h0, cr_h1, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    ILVRL_B2_SB(tmp3, tmp2, out_g2, out_g3);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cr_w0,
                     cr_w1, cr_w2, cr_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    ILVRL_B2_SB(tmp3, tmp2, out_r2, out_r3);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_w0,
                     cb_w1, cb_w2, cb_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);
    ILVRL_B2_SB(tmp3, tmp2, out_b2, out_b3);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_b1, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_r1, out_rgb3, out_r1, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_r1, out_rgb3, out_r1, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB4(out0, out1, out2, out3, p_out0, 16);
    p_out0 += 64;
    ST_UB2(out4, out5, p_out0, 16);
    p_out0 += 32;

    ILVRL_B2_SB(out_g2, out_b2, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g3, out_b3, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_r2, out_rgb1, out_r2, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_r2, out_rgb1, out_r2, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_r3, out_rgb3, out_r3, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_r3, out_rgb3, out_r3, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB4(out0, out1, out2, out3, p_out1, 16);
    p_out1 += 64;
    ST_UB2(out4, out5, p_out1, 16);
    p_out1 += 32;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    input_y0 = LD_UB(p_in_y0);
    input_y1 = LD_UB(p_in_y1);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y0 += 16;
    p_in_y1 += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_b1, out_rgb2, out_rgb3);

    VSHF_B2_UB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb0, mask_rgb2,
               out0, out2);
    VSHF_B2_SB(out_rgb0, out_r0, out_rgb1, out_r0, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out1 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out1 = (v16u8) __msa_pckev_d((v2i64) out1, (v2i64) tmp0);

    VSHF_B2_UB(out_rgb2, out_r1, out_rgb3, out_r1, mask_rgb0, mask_rgb2,
               out3, out5);
    VSHF_B2_SB(out_rgb2, out_r1, out_rgb3, out_r1, mask_rgb1, mask_rgb1,
               tmp0, tmp1);
    out4 = (v16u8) __msa_sldi_b((v16i8) zero, tmp1, 8);
    out4 = (v16u8) __msa_pckev_d((v2i64) out4, (v2i64) tmp0);

    ST_UB(out0, p_out0);
    p_out0 += 16;
    ST_UB(out1, p_out0);
    p_out0 += 16;
    ST_UB(out2, p_out0);
    p_out0 += 16;

    ST_UB(out3, p_out1);
    p_out1 += 16;
    ST_UB(out4, p_out1);
    p_out1 += 16;
    ST_UB(out5, p_out1);
    p_out1 += 16;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y0, in_y1;
    v16i8 input_cbcr = {0};

    in_y0 = LD(p_in_y0);
    in_y1 = LD(p_in_y1);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y0 += 8;
    p_in_y1 += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y0);
    input_y1 = (v16u8) __msa_insert_d((v2i64) input_y1, 0, in_y1);
    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    out_rgb0 = (v16i8) __msa_ilvr_b((v16i8) out_g0, (v16i8) out_b0);
    out_rgb1 = (v16i8) __msa_ilvr_b((v16i8) out_g1, (v16i8) out_b1);

    VSHF_B2_UB(out_rgb0, out_r0, out_rgb0, out_r0, mask_rgb0, mask_rgb1,
               out0, out1);
    VSHF_B2_UB(out_rgb1, out_r1, out_rgb1, out_r1, mask_rgb0, mask_rgb1,
               out2, out3);

    ST_UB(out0, p_out0);
    p_out0 += 16;
    ST8x1_UB(out1, p_out0);
    p_out0 += 8;

    ST_UB(out2, p_out1);
    p_out1 += 16;
    ST8x1_UB(out3, p_out1);
    p_out1 += 8;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0 += 3;

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0 += 3;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1 += 3;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1 += 3;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y0);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);

    y = (int) (*p_in_y1);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
  }
}

void
yuv_rgba_upsample_h2v2_msa (JSAMPROW p_in_y0, JSAMPROW p_in_y1,
                            JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                            JSAMPROW p_out0, JSAMPROW p_out1,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1 = {0}, input_y2, input_y3;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 out_rgb0, out_rgb1, out_rgb2, out_rgb3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, tmp0, tmp1, tmp2, tmp3;
  v16i8 out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16i8 out_r2, out_g2, out_b2, out_r3, out_g3, out_b3;
  v8i16 y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7;
  v8i16 cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3;

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y0, 16, tmp0, tmp1);
    LD_SB2(p_in_y1, 16, tmp2, tmp3);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y0 += 32;
    p_in_y1 += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);
    input_y2 = (v16u8) __msa_pckev_b(tmp3, tmp2);
    input_y3 = (v16u8) __msa_pckod_b(tmp3, tmp2);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_UB_SH(input_y2, y_h4, y_h5);
    UNPCK_UB_SH(input_y3, y_h6, y_h7);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_h0,
                     cb_h1, cr_h0, cr_h1, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    ILVRL_B2_SB(tmp3, tmp2, out_g2, out_g3);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cr_w0,
                     cr_w1, cr_w2, cr_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    ILVRL_B2_SB(tmp3, tmp2, out_r2, out_r3);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_w0,
                     cb_w1, cb_w2, cb_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);
    ILVRL_B2_SB(tmp3, tmp2, out_b2, out_b3);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_r1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(alpha, out_b0, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_b1, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out0, 16);
    p_out0 += 16 * 8;

    ILVRL_B2_SB(out_g2, out_r2, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g3, out_r3, out_rgb2, out_rgb3);

    ILVRL_B2_SB(alpha, out_b2, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_b3, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    input_y0 = LD_UB(p_in_y0);
    input_y1 = LD_UB(p_in_y1);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y0 += 16;
    p_in_y1 += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_r0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_r1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(alpha, out_b0, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_b1, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB4(out0, out1, out2, out3, p_out0, 16);
    p_out0 += 16 * 4;
    ST_UB4(out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y0, in_y1;
    v16i8 input_cbcr = {0};

    in_y0 = LD(p_in_y0);
    in_y1 = LD(p_in_y1);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y0 += 8;
    p_in_y1 += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y0);
    input_y1 = (v16u8) __msa_insert_d((v2i64) input_y1, 0, in_y1);
    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    out_rgb0 = __msa_ilvr_b(out_g0, out_r0);
    out_rgb1 = __msa_ilvr_b(out_g1, out_r1);
    tmp0 = __msa_ilvr_b(alpha, out_b0);
    tmp1 = __msa_ilvr_b(alpha, out_b1);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);

    ST_UB2(out0, out1, p_out0, 16);
    p_out0 += 16 * 2;
    ST_UB2(out2, out3, p_out1, 16);
    p_out1 += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0[3] = 0xFF;
    p_out0 += 4;

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0[3] = 0xFF;
    p_out0 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1[3] = 0xFF;
    p_out1 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1[3] = 0xFF;
    p_out1 += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y0);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0[3] = 0xFF;

    y = (int) (*p_in_y1);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1[3] = 0xFF;
  }
}

void
yuv_bgra_upsample_h2v2_msa (JSAMPROW p_in_y0, JSAMPROW p_in_y1,
                            JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                            JSAMPROW p_out0, JSAMPROW p_out1,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1 = {0}, input_y2, input_y3;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 out_rgb0, out_rgb1, out_rgb2, out_rgb3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, tmp0, tmp1, tmp2, tmp3;
  v16i8 out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16i8 out_r2, out_g2, out_b2, out_r3, out_g3, out_b3;
  v8i16 y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7;
  v8i16 cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3;

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y0, 16, tmp0, tmp1);
    LD_SB2(p_in_y1, 16, tmp2, tmp3);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y0 += 32;
    p_in_y1 += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);
    input_y2 = (v16u8) __msa_pckev_b(tmp3, tmp2);
    input_y3 = (v16u8) __msa_pckod_b(tmp3, tmp2);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_UB_SH(input_y2, y_h4, y_h5);
    UNPCK_UB_SH(input_y3, y_h6, y_h7);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_h0,
                     cb_h1, cr_h0, cr_h1, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    ILVRL_B2_SB(tmp3, tmp2, out_g2, out_g3);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cr_w0,
                     cr_w1, cr_w2, cr_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    ILVRL_B2_SB(tmp3, tmp2, out_r2, out_r3);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_w0,
                     cb_w1, cb_w2, cb_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);
    ILVRL_B2_SB(tmp3, tmp2, out_b2, out_b3);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_b1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(alpha, out_r0, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_r1, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out0, 16);
    p_out0 += 16 * 8;

    ILVRL_B2_SB(out_g2, out_b2, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g3, out_b3, out_rgb2, out_rgb3);

    ILVRL_B2_SB(alpha, out_r2, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_r3, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    input_y0 = LD_UB(p_in_y0);
    input_y1 = LD_UB(p_in_y1);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y0 += 16;
    p_in_y1 += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_g0, out_b0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_g1, out_b1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(alpha, out_r0, tmp0, tmp1);
    ILVRL_B2_SB(alpha, out_r1, tmp2, tmp3);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);
    ILVRL_H2_UB(tmp2, out_rgb2, out4, out5);
    ILVRL_H2_UB(tmp3, out_rgb3, out6, out7);

    ST_UB4(out0, out1, out2, out3, p_out0, 16);
    p_out0 += 16 * 4;
    ST_UB4(out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y0, in_y1;
    v16i8 input_cbcr = {0};

    in_y0 = LD(p_in_y0);
    in_y1 = LD(p_in_y1);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y0 += 8;
    p_in_y1 += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y0);
    input_y1 = (v16u8) __msa_insert_d((v2i64) input_y1, 0, in_y1);
    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    out_rgb0 = __msa_ilvr_b(out_g0, out_b0);
    out_rgb1 = __msa_ilvr_b(out_g1, out_b1);
    tmp0 = __msa_ilvr_b(alpha, out_r0);
    tmp1 = __msa_ilvr_b(alpha, out_r1);

    ILVRL_H2_UB(tmp0, out_rgb0, out0, out1);
    ILVRL_H2_UB(tmp1, out_rgb1, out2, out3);

    ST_UB2(out0, out1, p_out0, 16);
    p_out0 += 16 * 2;
    ST_UB2(out2, out3, p_out1, 16);
    p_out1 += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0[3] = 0xFF;
    p_out0 += 4;

    y = (int) (*p_in_y0++);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0[3] = 0xFF;
    p_out0 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1[3] = 0xFF;
    p_out1 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1[3] = 0xFF;
    p_out1 += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y0);
    p_out0[0] = clip_pixel(y + cred);
    p_out0[1] = clip_pixel(y + cgreen);
    p_out0[2] = clip_pixel(y + cblue);
    p_out0[3] = 0xFF;

    y = (int) (*p_in_y1);
    p_out1[0] = clip_pixel(y + cred);
    p_out1[1] = clip_pixel(y + cgreen);
    p_out1[2] = clip_pixel(y + cblue);
    p_out1[3] = 0xFF;
  }
}

void
yuv_argb_upsample_h2v2_msa (JSAMPROW p_in_y0, JSAMPROW p_in_y1,
                            JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                            JSAMPROW p_out0, JSAMPROW p_out1,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1 = {0}, input_y2, input_y3;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 out_rgb0, out_rgb1, out_rgb2, out_rgb3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, tmp0, tmp1, tmp2, tmp3;
  v16i8 out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16i8 out_r2, out_g2, out_b2, out_r3, out_g3, out_b3;
  v8i16 y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7;
  v8i16 cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3;

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y0, 16, tmp0, tmp1);
    LD_SB2(p_in_y1, 16, tmp2, tmp3);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y0 += 32;
    p_in_y1 += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);
    input_y2 = (v16u8) __msa_pckev_b(tmp3, tmp2);
    input_y3 = (v16u8) __msa_pckod_b(tmp3, tmp2);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_UB_SH(input_y2, y_h4, y_h5);
    UNPCK_UB_SH(input_y3, y_h6, y_h7);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_h0,
                     cb_h1, cr_h0, cr_h1, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    ILVRL_B2_SB(tmp3, tmp2, out_g2, out_g3);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cr_w0,
                     cr_w1, cr_w2, cr_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    ILVRL_B2_SB(tmp3, tmp2, out_r2, out_r3);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_w0,
                     cb_w1, cb_w2, cb_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);
    ILVRL_B2_SB(tmp3, tmp2, out_b2, out_b3);

    ILVRL_B2_SB(out_b0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_b1, out_g1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(out_r0, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_r1, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out0, 16);
    p_out0 += 16 * 8;

    ILVRL_B2_SB(out_b2, out_g2, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_b3, out_g3, out_rgb2, out_rgb3);

    ILVRL_B2_SB(out_r2, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_r3, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    input_y0 = LD_UB(p_in_y0);
    input_y1 = LD_UB(p_in_y1);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y0 += 16;
    p_in_y1 += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_b0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_b1, out_g1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(out_r0, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_r1, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB4(out0, out1, out2, out3, p_out0, 16);
    p_out0 += 16 * 4;
    ST_UB4(out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y0, in_y1;
    v16i8 input_cbcr = {0};

    in_y0 = LD(p_in_y0);
    in_y1 = LD(p_in_y1);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y0 += 8;
    p_in_y1 += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y0);
    input_y1 = (v16u8) __msa_insert_d((v2i64) input_y1, 0, in_y1);
    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    out_rgb0 = __msa_ilvr_b(out_b0, out_g0);
    out_rgb1 = __msa_ilvr_b(out_b1, out_g1);
    tmp0 = __msa_ilvr_b(out_r0, alpha);
    tmp1 = __msa_ilvr_b(out_r1, alpha);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);

    ST_UB2(out0, out1, p_out0, 16);
    p_out0 += 16 * 2;
    ST_UB2(out2, out3, p_out1, 16);
    p_out1 += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y0++);
    p_out0[0] = 0xFF;
    p_out0[1] = clip_pixel(y + cred);
    p_out0[2] = clip_pixel(y + cgreen);
    p_out0[3] = clip_pixel(y + cblue);
    p_out0 += 4;

    y = (int) (*p_in_y0++);
    p_out0[0] = 0xFF;
    p_out0[1] = clip_pixel(y + cred);
    p_out0[2] = clip_pixel(y + cgreen);
    p_out0[3] = clip_pixel(y + cblue);
    p_out0 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = 0xFF;
    p_out1[1] = clip_pixel(y + cred);
    p_out1[2] = clip_pixel(y + cgreen);
    p_out1[3] = clip_pixel(y + cblue);
    p_out1 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = 0xFF;
    p_out1[1] = clip_pixel(y + cred);
    p_out1[2] = clip_pixel(y + cgreen);
    p_out1[3] = clip_pixel(y + cblue);
    p_out1 += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);

    y = (int) (*p_in_y0);
    p_out0[0] = 0xFF;
    p_out0[1] = clip_pixel(y + cred);
    p_out0[2] = clip_pixel(y + cgreen);
    p_out0[3] = clip_pixel(y + cblue);

    y = (int) (*p_in_y1);
    p_out1[0] = 0xFF;
    p_out1[1] = clip_pixel(y + cred);
    p_out1[2] = clip_pixel(y + cgreen);
    p_out1[3] = clip_pixel(y + cblue);
  }
}

void
yuv_abgr_upsample_h2v2_msa (JSAMPROW p_in_y0, JSAMPROW p_in_y1,
                            JSAMPROW p_in_cb, JSAMPROW p_in_cr,
                            JSAMPROW p_out0, JSAMPROW p_out1,
                            JDIMENSION out_width)
{
  int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION col, num_cols_mul_32 = out_width >> 5;
  JDIMENSION remaining_wd = out_width & 0x1E;
  v16u8 out0, out1, out2, out3, out4, out5, out6, out7;
  v16u8 input_y0 = {0}, input_y1 = {0}, input_y2, input_y3;
  v16i8 alpha = __msa_ldi_b(0xFF);
  v16i8 out_rgb0, out_rgb1, out_rgb2, out_rgb3, const_128 = __msa_ldi_b(128);
  v16i8 input_cb, input_cr, tmp0, tmp1, tmp2, tmp3;
  v16i8 out_r0, out_g0, out_b0, out_r1, out_g1, out_b1;
  v16i8 out_r2, out_g2, out_b2, out_r3, out_g3, out_b3;
  v8i16 y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7;
  v8i16 cb_h0, cb_h1, cr_h0, cr_h1;
  v4i32 cb_w0, cb_w1, cb_w2, cb_w3, cr_w0, cr_w1, cr_w2, cr_w3;

  for (col = num_cols_mul_32; col--;) {
    LD_SB2(p_in_y0, 16, tmp0, tmp1);
    LD_SB2(p_in_y1, 16, tmp2, tmp3);
    input_cb = LD_SB(p_in_cb);
    input_cr = LD_SB(p_in_cr);

    p_in_y0 += 32;
    p_in_y1 += 32;
    p_in_cb += 16;
    p_in_cr += 16;

    input_cb -= const_128;
    input_cr -= const_128;

    input_y0 = (v16u8) __msa_pckev_b(tmp1, tmp0);
    input_y1 = (v16u8) __msa_pckod_b(tmp1, tmp0);
    input_y2 = (v16u8) __msa_pckev_b(tmp3, tmp2);
    input_y3 = (v16u8) __msa_pckod_b(tmp3, tmp2);

    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);
    UNPCK_UB_SH(input_y2, y_h4, y_h5);
    UNPCK_UB_SH(input_y3, y_h6, y_h7);
    UNPCK_SB_SH(input_cb, cb_h0, cb_h1);
    UNPCK_SB_SH(input_cr, cr_h0, cr_h1);

    CALC_G4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_h0,
                     cb_h1, cr_h0, cr_h1, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    ILVRL_B2_SB(tmp3, tmp2, out_g2, out_g3);

    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);
    UNPCK_SH_SW(cr_h1, cr_w2, cr_w3);
    CALC_R4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cr_w0,
                     cr_w1, cr_w2, cr_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    ILVRL_B2_SB(tmp3, tmp2, out_r2, out_r3);

    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cb_h1, cb_w2, cb_w3);
    CALC_B4_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, cb_w0,
                     cb_w1, cb_w2, cb_w3, tmp0, tmp1, tmp2, tmp3);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);
    ILVRL_B2_SB(tmp3, tmp2, out_b2, out_b3);

    ILVRL_B2_SB(out_r0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_r1, out_g1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(out_b0, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_b1, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out0, 16);
    p_out0 += 16 * 8;

    ILVRL_B2_SB(out_r2, out_g2, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_r3, out_g3, out_rgb2, out_rgb3);

    ILVRL_B2_SB(out_b2, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_b3, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 8;
  }

  if (remaining_wd >= 16) {
    uint64_t in_cb, in_cr;
    v16i8 input_cbcr = {0};

    input_y0 = LD_UB(p_in_y0);
    input_y1 = LD_UB(p_in_y1);
    in_cb = LD(p_in_cb);
    in_cr = LD(p_in_cr);

    p_in_y0 += 16;
    p_in_y1 += 16;
    p_in_cb += 8;
    p_in_cr += 8;

    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_d((v2i64) input_cbcr, 1, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    ILVRL_B2_SB(out_r0, out_g0, out_rgb0, out_rgb1);
    ILVRL_B2_SB(out_r1, out_g1, out_rgb2, out_rgb3);

    ILVRL_B2_SB(out_b0, alpha, tmp0, tmp1);
    ILVRL_B2_SB(out_b1, alpha, tmp2, tmp3);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);
    ILVRL_H2_UB(out_rgb2, tmp2, out4, out5);
    ILVRL_H2_UB(out_rgb3, tmp3, out6, out7);

    ST_UB4(out0, out1, out2, out3, p_out0, 16);
    p_out0 += 16 * 4;
    ST_UB4(out4, out5, out6, out7, p_out1, 16);
    p_out1 += 16 * 4;

    remaining_wd -= 16;
  }

  if (remaining_wd >= 8) {
    JDIMENSION in_cb, in_cr;
    uint64_t in_y0, in_y1;
    v16i8 input_cbcr = {0};

    in_y0 = LD(p_in_y0);
    in_y1 = LD(p_in_y1);
    in_cb = LW(p_in_cb);
    in_cr = LW(p_in_cr);

    p_in_y0 += 8;
    p_in_y1 += 8;
    p_in_cb += 4;
    p_in_cr += 4;

    input_y0 = (v16u8) __msa_insert_d((v2i64) input_y0, 0, in_y0);
    input_y1 = (v16u8) __msa_insert_d((v2i64) input_y1, 0, in_y1);
    tmp0 = __msa_pckev_b((v16i8) input_y1, (v16i8) input_y0);
    tmp1 = __msa_pckod_b((v16i8) input_y1, (v16i8) input_y0);
    input_y0 = (v16u8) __msa_pckev_d((v2i64) tmp1, (v2i64) tmp0);
    input_y1 = (v16u8) __msa_pckod_d((v2i64) tmp1, (v2i64) tmp0);
    UNPCK_UB_SH(input_y0, y_h0, y_h1);
    UNPCK_UB_SH(input_y1, y_h2, y_h3);

    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 0, in_cb);
    input_cbcr = (v16i8) __msa_insert_w((v4i32) input_cbcr, 2, in_cr);

    input_cbcr -= const_128;

    UNPCK_SB_SH(input_cbcr, cb_h0, cr_h0);
    UNPCK_SH_SW(cb_h0, cb_w0, cb_w1);
    UNPCK_SH_SW(cr_h0, cr_w0, cr_w1);

    CALC_R2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_r0, out_r1);
    CALC_G2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_g0, out_g1);
    CALC_B2_FRM_YUV2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, tmp0, tmp1);
    ILVRL_B2_SB(tmp1, tmp0, out_b0, out_b1);

    out_rgb0 = __msa_ilvr_b(out_r0, out_g0);
    out_rgb1 = __msa_ilvr_b(out_r1, out_g1);
    tmp0 = __msa_ilvr_b(out_b0, alpha);
    tmp1 = __msa_ilvr_b(out_b1, alpha);

    ILVRL_H2_UB(out_rgb0, tmp0, out0, out1);
    ILVRL_H2_UB(out_rgb1, tmp1, out2, out3);

    ST_UB2(out0, out1, p_out0, 16);
    p_out0 += 16 * 2;
    ST_UB2(out2, out3, p_out1, 16);
    p_out1 += 16 * 2;

    remaining_wd -= 8;
  }

  for (col = 0; col < remaining_wd; col += 2) {
    cb = (int) (*p_in_cb++) - 128;
    cr = (int) (*p_in_cr++) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y0++);
    p_out0[0] = 0xFF;
    p_out0[1] = clip_pixel(y + cred);
    p_out0[2] = clip_pixel(y + cgreen);
    p_out0[3] = clip_pixel(y + cblue);
    p_out0 += 4;

    y = (int) (*p_in_y0++);
    p_out0[0] = 0xFF;
    p_out0[1] = clip_pixel(y + cred);
    p_out0[2] = clip_pixel(y + cgreen);
    p_out0[3] = clip_pixel(y + cblue);
    p_out0 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = 0xFF;
    p_out1[1] = clip_pixel(y + cred);
    p_out1[2] = clip_pixel(y + cgreen);
    p_out1[3] = clip_pixel(y + cblue);
    p_out1 += 4;

    y = (int) (*p_in_y1++);
    p_out1[0] = 0xFF;
    p_out1[1] = clip_pixel(y + cred);
    p_out1[2] = clip_pixel(y + cgreen);
    p_out1[3] = clip_pixel(y + cblue);
    p_out1 += 4;
  }

  if (out_width & 1) {
    cb = (int) (*p_in_cb) - 128;
    cr = (int) (*p_in_cr) - 128;
    cred = ROUND_POWER_OF_TWO(FIX_1_77200 * cb, 16);
    cgreen = ROUND_POWER_OF_TWO(((-FIX_0_34414) * cb - FIX_0_71414 * cr), 16);
    cblue = ROUND_POWER_OF_TWO(FIX_1_40200 * cr, 16);

    y = (int) (*p_in_y0);
    p_out0[0] = 0xFF;
    p_out0[1] = clip_pixel(y + cred);
    p_out0[2] = clip_pixel(y + cgreen);
    p_out0[3] = clip_pixel(y + cblue);

    y = (int) (*p_in_y1);
    p_out1[0] = 0xFF;
    p_out1[1] = clip_pixel(y + cred);
    p_out1[2] = clip_pixel(y + cgreen);
    p_out1[3] = clip_pixel(y + cblue);
  }
}

void
jsimd_h2v1_merged_upsample_msa (JDIMENSION output_width, JSAMPIMAGE input_buf,
                                JDIMENSION in_row_group_ctr,
                                JSAMPARRAY output_buf)
{
  yuv_rgb_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                            input_buf[1][in_row_group_ctr],
                            input_buf[2][in_row_group_ctr],
                            output_buf[0], output_width);
}

void
jsimd_h2v1_extrgb_merged_upsample_msa (JDIMENSION output_width,
                                       JSAMPIMAGE input_buf,
                                       JDIMENSION in_row_group_ctr,
                                       JSAMPARRAY output_buf)
{
  yuv_rgb_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                            input_buf[1][in_row_group_ctr],
                            input_buf[2][in_row_group_ctr],
                            output_buf[0], output_width);
}

void
jsimd_h2v1_extbgr_merged_upsample_msa (JDIMENSION output_width,
                                       JSAMPIMAGE input_buf,
                                       JDIMENSION in_row_group_ctr,
                                       JSAMPARRAY output_buf)
{
  yuv_bgr_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                            input_buf[1][in_row_group_ctr],
                            input_buf[2][in_row_group_ctr],
                            output_buf[0], output_width);
}

void
jsimd_h2v1_extrgbx_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_rgba_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_width);
}

void
jsimd_h2v1_extbgrx_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_bgra_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_width);
}

void
jsimd_h2v1_extxrgb_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_argb_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_width);
}

void
jsimd_h2v1_extxbgr_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_abgr_upsample_h2v1_msa(input_buf[0][in_row_group_ctr],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_width);
}

void
jsimd_h2v2_merged_upsample_msa (JDIMENSION output_width,
                                JSAMPIMAGE input_buf,
                                JDIMENSION in_row_group_ctr,
                                JSAMPARRAY output_buf)
{
  yuv_rgb_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                            input_buf[0][in_row_group_ctr * 2 + 1],
                            input_buf[1][in_row_group_ctr],
                            input_buf[2][in_row_group_ctr],
                            output_buf[0], output_buf[1], output_width);
}

void
jsimd_h2v2_extrgb_merged_upsample_msa (JDIMENSION output_width,
                                       JSAMPIMAGE input_buf,
                                       JDIMENSION in_row_group_ctr,
                                       JSAMPARRAY output_buf)
{
  yuv_rgb_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                            input_buf[0][in_row_group_ctr * 2 + 1],
                            input_buf[1][in_row_group_ctr],
                            input_buf[2][in_row_group_ctr],
                            output_buf[0], output_buf[1], output_width);
}

void
jsimd_h2v2_extbgr_merged_upsample_msa (JDIMENSION output_width,
                                       JSAMPIMAGE input_buf,
                                       JDIMENSION in_row_group_ctr,
                                       JSAMPARRAY output_buf)
{
  yuv_bgr_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                            input_buf[0][in_row_group_ctr * 2 + 1],
                            input_buf[1][in_row_group_ctr],
                            input_buf[2][in_row_group_ctr],
                            output_buf[0], output_buf[1], output_width);
}

void
jsimd_h2v2_extrgbx_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_rgba_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                             input_buf[0][in_row_group_ctr * 2 + 1],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_buf[1], output_width);
}

void
jsimd_h2v2_extbgrx_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_bgra_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                             input_buf[0][in_row_group_ctr * 2 + 1],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_buf[1], output_width);
}

void
jsimd_h2v2_extxrgb_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_argb_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                             input_buf[0][in_row_group_ctr * 2 + 1],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_buf[1], output_width);
}

void
jsimd_h2v2_extxbgr_merged_upsample_msa (JDIMENSION output_width,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf)
{
  yuv_abgr_upsample_h2v2_msa(input_buf[0][in_row_group_ctr * 2],
                             input_buf[0][in_row_group_ctr * 2 + 1],
                             input_buf[1][in_row_group_ctr],
                             input_buf[2][in_row_group_ctr],
                             output_buf[0], output_buf[1], output_width);
}
