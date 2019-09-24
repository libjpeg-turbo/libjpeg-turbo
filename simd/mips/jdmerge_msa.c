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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "jconfigint.h"
#include "jmacros_msa.h"


#define FIX_1_40200  0x166e9
#define FIX_0_34414  0x581a
#define FIX_0_71414  0xb6d2
#define FIX_1_77200  0x1c5a2
#define FIX_0_28586  (65536 - FIX_0_71414)
#define GETSAMPLE(value)  ((int)(value))

/* Original
    G = Y - 0.34414 * Cb - 0.71414 * Cr */
/* This implementation
    G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */

#define CALC_G4_FRM_YCC(y_h0, y_h1, y_h2, y_h3, cb_h0, cb_h1, cr_h0, cr_h1, \
out_g0, out_g1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
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
  ADD4(y_h2, tmp0_m, y_h3, tmp1_m, y_h0, tmp0_m, y_h1, tmp1_m, \
       tmp2_m, tmp3_m, tmp0_m, tmp1_m); \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_g0, out_g1); \
}

#define CALC_G2_FRM_YCC(y_h0, y_h1, cb_h0, cr_h0, out_g0, out_g1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m; \
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
  ADD2(y_h1, tmp0_m, y_h0, tmp0_m, tmp2_m, tmp0_m); \
  CLIP_SH2_0_255(tmp0_m, tmp2_m); \
  PCKEV_B2_SB(tmp0_m, tmp0_m, tmp2_m, tmp2_m, out_g0, out_g1); \
}

#define CALC_R4_FRM_YCC(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, cr_w2, cr_w3, \
out_r0, out_r1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v4i32 const0_m = { FIX_1_40200, FIX_1_40200, FIX_1_40200, FIX_1_40200 }; \
  \
  MUL4(const0_m, cr_w0, const0_m, cr_w1, const0_m, cr_w2, const0_m, cr_w3, \
       out0_m, out1_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD4(y_h2, tmp0_m, y_h3, tmp1_m, y_h0, tmp0_m, y_h1, tmp1_m, \
       tmp2_m, tmp3_m, tmp0_m, tmp1_m); \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_r0, out_r1); \
}

#define CALC_R2_FRM_YCC(y_h0, y_h1, cr_w0, cr_w1, out_r0, out_r1) { \
  v8i16 tmp0_m, tmp2_m; \
  v4i32 out0_m, out1_m; \
  v4i32 const0_m = { FIX_1_40200, FIX_1_40200, FIX_1_40200, FIX_1_40200 }; \
  \
  MUL2(const0_m, cr_w0, const0_m, cr_w1, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  ADD2(y_h1, tmp0_m, y_h0, tmp0_m, tmp2_m, tmp0_m); \
  CLIP_SH2_0_255(tmp0_m, tmp2_m); \
  PCKEV_B2_SB(tmp0_m, tmp0_m, tmp2_m, tmp2_m, out_r0, out_r1); \
}

#define CALC_B4_FRM_YCC(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, cb_w2, cb_w3, \
out_b0, out_b1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v4i32 const0_m = { FIX_1_77200, FIX_1_77200, FIX_1_77200, FIX_1_77200 }; \
  \
  MUL4(const0_m, cb_w0, const0_m, cb_w1, const0_m, cb_w2, const0_m, cb_w3, \
       out0_m, out1_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD4(y_h2, tmp0_m, y_h3, tmp1_m, y_h0, tmp0_m, y_h1, tmp1_m, \
       tmp2_m, tmp3_m, tmp0_m, tmp1_m); \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_b0, out_b1); \
}

#define CALC_B2_FRM_YCC(y_h0, y_h1, cb_w0, cb_w1, out_b0, out_b1) { \
  v8i16 tmp0_m, tmp2_m; \
  v4i32 out0_m, out1_m; \
  v4i32 const0_m = { FIX_1_77200, FIX_1_77200, FIX_1_77200, FIX_1_77200 }; \
  \
  MUL2(const0_m, cb_w0, const0_m, cb_w1, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  ADD2(y_h1, tmp0_m, y_h0, tmp0_m, tmp2_m, tmp0_m); \
  CLIP_SH2_0_255(tmp0_m, tmp2_m); \
  PCKEV_B2_SB(tmp0_m, tmp0_m, tmp2_m, tmp2_m, out_b0, out_b1); \
}

#define CALC_G4_FRM_YCC2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, \
cb_h0, cb_h1, cr_h0, cr_h1, out_g0, out_g1, out_g2, out_g3) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m; \
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
  ADD4(y_h4, tmp0_m, y_h5, tmp1_m, y_h6, tmp0_m, y_h7, tmp1_m, \
       tmp4_m, tmp5_m, tmp6_m, tmp7_m); \
  CLIP_SH4_0_255(tmp4_m, tmp5_m, tmp6_m, tmp7_m); \
  PCKEV_B2_SB(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out_g2, out_g3); \
  ADD4(y_h3, tmp1_m, y_h2, tmp0_m, y_h1, tmp1_m, y_h0, tmp0_m, \
       tmp3_m, tmp2_m, tmp1_m, tmp0_m); \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_g0, out_g1); \
}

#define CALC_G2_FRM_YCC2(y_h0, y_h1, y_h2, y_h3, cb_h0, cr_h0, \
out_g0, out_g1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
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
  ADD4(y_h3, tmp0_m, y_h2, tmp0_m, y_h1, tmp0_m, y_h0, tmp0_m, \
       tmp3_m, tmp1_m, tmp2_m, tmp0_m); \
  CLIP_SH4_0_255(tmp0_m, tmp2_m, tmp1_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_g0, out_g1); \
}

#define CALC_R4_FRM_YCC2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, \
cr_w0, cr_w1, cr_w2, cr_w3, out_r0, out_r1, out_r2, out_r3) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v4i32 const0_m = { FIX_1_40200, FIX_1_40200, FIX_1_40200, FIX_1_40200 }; \
  \
  MUL4(const0_m, cr_w0, const0_m, cr_w1, const0_m, cr_w2, const0_m, cr_w3, \
       out0_m, out1_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD4(y_h4, tmp0_m, y_h5, tmp1_m, y_h6, tmp0_m, y_h7, tmp1_m, \
       tmp4_m, tmp5_m, tmp6_m, tmp7_m); \
  CLIP_SH4_0_255(tmp4_m, tmp5_m, tmp6_m, tmp7_m); \
  PCKEV_B2_SB(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out_r2, out_r3); \
  ADD4(y_h3, tmp1_m, y_h2, tmp0_m, y_h1, tmp1_m, y_h0, tmp0_m, \
       tmp3_m, tmp2_m, tmp1_m, tmp0_m); \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_r0, out_r1); \
}

#define CALC_R2_FRM_YCC2(y_h0, y_h1, y_h2, y_h3, cr_w0, cr_w1, \
out_r0, out_r1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
  v4i32 out0_m, out1_m; \
  v4i32 const0_m = { FIX_1_40200, FIX_1_40200, FIX_1_40200, FIX_1_40200 }; \
  \
  MUL2(const0_m, cr_w0, const0_m, cr_w1, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  ADD4(y_h3, tmp0_m, y_h2, tmp0_m, y_h1, tmp0_m, y_h0, tmp0_m, \
       tmp3_m, tmp1_m, tmp2_m, tmp0_m); \
  CLIP_SH4_0_255(tmp0_m, tmp2_m, tmp1_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_r0, out_r1); \
}

#define CALC_B4_FRM_YCC2(y_h0, y_h1, y_h2, y_h3, y_h4, y_h5, y_h6, y_h7, \
cb_w0, cb_w1, cb_w2, cb_w3, out_b0, out_b1, out_b2, out_b3) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m; \
  v4i32 out0_m, out1_m, out2_m, out3_m; \
  v4i32 const0_m = { FIX_1_77200, FIX_1_77200, FIX_1_77200, FIX_1_77200 }; \
  \
  MUL4(const0_m, cb_w0, const0_m, cb_w1, const0_m, cb_w2, const0_m, cb_w3, \
       out0_m, out1_m, out2_m, out3_m); \
  SRARI_W4_SW(out0_m, out1_m, out2_m, out3_m, 16); \
  PCKEV_H2_SH(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m); \
  ADD4(y_h4, tmp0_m, y_h5, tmp1_m, y_h6, tmp0_m, y_h7, tmp1_m, \
       tmp4_m, tmp5_m, tmp6_m, tmp7_m); \
  CLIP_SH4_0_255(tmp4_m, tmp5_m, tmp6_m, tmp7_m); \
  PCKEV_B2_SB(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out_b2, out_b3); \
  ADD4(y_h3, tmp1_m, y_h2, tmp0_m, y_h1, tmp1_m, y_h0, tmp0_m, \
       tmp3_m, tmp2_m, tmp1_m, tmp0_m); \
  CLIP_SH4_0_255(tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_b0, out_b1); \
}

#define CALC_B2_FRM_YCC2(y_h0, y_h1, y_h2, y_h3, cb_w0, cb_w1, \
out_b0, out_b1) { \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
  v4i32 out0_m, out1_m; \
  v4i32 const0_m = { FIX_1_77200, FIX_1_77200, FIX_1_77200, FIX_1_77200 }; \
  \
  MUL2(const0_m, cb_w0, const0_m, cb_w1, out0_m, out1_m); \
  SRARI_W2_SW(out0_m, out1_m, 16); \
  tmp0_m = __msa_pckev_h((v8i16)out1_m, (v8i16)out0_m); \
  ADD4(y_h3, tmp0_m, y_h2, tmp0_m, y_h1, tmp0_m, y_h0, tmp0_m, \
       tmp3_m, tmp1_m, tmp2_m, tmp0_m); \
  CLIP_SH4_0_255(tmp0_m, tmp2_m, tmp1_m, tmp3_m); \
  PCKEV_B2_SB(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out_b0, out_b1); \
}

#define ROUND_POWER_OF_TWO(value, n)  (((value) + (1 << ((n) - 1))) >> (n))

static INLINE unsigned char clip_pixel(int val)
{
  return ((val & ~0xFF) ? ((-val) >> 31) & 0xFF : val);
}

#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED        EXT_RGB_RED
#define RGB_GREEN      EXT_RGB_GREEN
#define RGB_BLUE       EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define jsimd_h2v1_merged_upsample_msa  jsimd_h2v1_extrgb_merged_upsample_msa
#define jsimd_h2v2_merged_upsample_msa  jsimd_h2v2_extrgb_merged_upsample_msa
#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_msa
#undef jsimd_h2v2_merged_upsample_msa

#define RGB_RED        EXT_BGR_RED
#define RGB_GREEN      EXT_BGR_GREEN
#define RGB_BLUE       EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define jsimd_h2v1_merged_upsample_msa  jsimd_h2v1_extbgr_merged_upsample_msa
#define jsimd_h2v2_merged_upsample_msa  jsimd_h2v2_extbgr_merged_upsample_msa
#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_msa
#undef jsimd_h2v2_merged_upsample_msa

#define RGB_RED        EXT_RGBX_RED
#define RGB_GREEN      EXT_RGBX_GREEN
#define RGB_BLUE       EXT_RGBX_BLUE
#define RGB_ALPHA      3
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define jsimd_h2v1_merged_upsample_msa  jsimd_h2v1_extrgbx_merged_upsample_msa
#define jsimd_h2v2_merged_upsample_msa  jsimd_h2v2_extrgbx_merged_upsample_msa
#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_msa
#undef jsimd_h2v2_merged_upsample_msa

#define RGB_RED        EXT_BGRX_RED
#define RGB_GREEN      EXT_BGRX_GREEN
#define RGB_BLUE       EXT_BGRX_BLUE
#define RGB_ALPHA      3
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define jsimd_h2v1_merged_upsample_msa  jsimd_h2v1_extbgrx_merged_upsample_msa
#define jsimd_h2v2_merged_upsample_msa  jsimd_h2v2_extbgrx_merged_upsample_msa
#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_msa
#undef jsimd_h2v2_merged_upsample_msa

#define RGB_RED        EXT_XRGB_RED
#define RGB_GREEN      EXT_XRGB_GREEN
#define RGB_BLUE       EXT_XRGB_BLUE
#define RGB_ALPHA      0
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define jsimd_h2v1_merged_upsample_msa  jsimd_h2v1_extxrgb_merged_upsample_msa
#define jsimd_h2v2_merged_upsample_msa  jsimd_h2v2_extxrgb_merged_upsample_msa
#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_msa
#undef jsimd_h2v2_merged_upsample_msa

#define RGB_RED        EXT_XBGR_RED
#define RGB_GREEN      EXT_XBGR_GREEN
#define RGB_BLUE       EXT_XBGR_BLUE
#define RGB_ALPHA      0
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define jsimd_h2v1_merged_upsample_msa  jsimd_h2v1_extxbgr_merged_upsample_msa
#define jsimd_h2v2_merged_upsample_msa  jsimd_h2v2_extxbgr_merged_upsample_msa
#include "jdmrgext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_msa
#undef jsimd_h2v2_merged_upsample_msa
