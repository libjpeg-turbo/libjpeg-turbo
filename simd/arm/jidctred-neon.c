/*
 * jidctred-neon.c - reduced-size IDCT (Arm Neon)
 *
 * Copyright (C) 2020 Arm Limited. All Rights Reserved.
 *
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
#include "../../jconfigint.h"
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"

#include <arm_neon.h>

#define CONST_BITS  13
#define PASS1_BITS  2

#define F_0_720  5906
#define F_0_850  6967
#define F_1_272  10426
#define F_3_624  29692

/*
 * 'jsimd_idct_2x2_neon' is an inverse-DCT function for getting reduced-size
 * 2x2 pixels output from an 8x8 DCT block. It uses the same calculations and
 * produces exactly the same output as IJG's original 'jpeg_idct_2x2' function
 * from jpeg-6b, which can be found in jidctred.c.
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.720959822 =  5906 * 2^-13
 *    0.850430095 =  6967 * 2^-13
 *    1.272758580 = 10426 * 2^-13
 *    3.624509785 = 29692 * 2^-13
 *
 * See jidctred.c for further details of the 2x2 reduced IDCT algorithm. Where
 * possible, the variable names and comments here in 'jsimd_idct_2x2_neon'
 * match up with those in 'jpeg_idct_2x2'.
 *
 * NOTE: jpeg-8 has an improved implementation of the 2x2 inverse-DCT which
 *       requires fewer arithmetic operations and hence should be faster. The
 *       primary purpose of this particular Neon optimized function is bit
 *       exact compatibility with jpeg-6b.
 */

ALIGN(16) static const int16_t jsimd_idct_2x2_neon_consts[] = {
  -F_0_720, F_0_850, -F_1_272, F_3_624
};

void jsimd_idct_2x2_neon(void *dct_table,
                         JCOEFPTR coef_block,
                         JSAMPARRAY output_buf,
                         JDIMENSION output_col)
{
  ISLOW_MULT_TYPE *quantptr = dct_table;

  /* Load DCT coefficients. */
  int16x8_t row0 = vld1q_s16(coef_block + 0 * DCTSIZE);
  int16x8_t row1 = vld1q_s16(coef_block + 1 * DCTSIZE);
  int16x8_t row3 = vld1q_s16(coef_block + 3 * DCTSIZE);
  int16x8_t row5 = vld1q_s16(coef_block + 5 * DCTSIZE);
  int16x8_t row7 = vld1q_s16(coef_block + 7 * DCTSIZE);

  /* Load DCT quantization table. */
  int16x8_t quant_row0 = vld1q_s16(quantptr + 0 * DCTSIZE);
  int16x8_t quant_row1 = vld1q_s16(quantptr + 1 * DCTSIZE);
  int16x8_t quant_row3 = vld1q_s16(quantptr + 3 * DCTSIZE);
  int16x8_t quant_row5 = vld1q_s16(quantptr + 5 * DCTSIZE);
  int16x8_t quant_row7 = vld1q_s16(quantptr + 7 * DCTSIZE);

  /* Dequantize DCT coefficients. */
  row0 = vmulq_s16(row0, quant_row0);
  row1 = vmulq_s16(row1, quant_row1);
  row3 = vmulq_s16(row3, quant_row3);
  row5 = vmulq_s16(row5, quant_row5);
  row7 = vmulq_s16(row7, quant_row7);

  /* Load IDCT conversion constants. */
  const int16x4_t consts = vld1_s16(jsimd_idct_2x2_neon_consts);

  /* Pass 1: process input columns; put results in vectors row0 and row1. */
  /* Even part. */
  int32x4_t tmp10_l = vshll_n_s16(vget_low_s16(row0), CONST_BITS + 2);
  int32x4_t tmp10_h = vshll_n_s16(vget_high_s16(row0), CONST_BITS + 2);

  /* Odd part. */
  int32x4_t tmp0_l = vmull_lane_s16(vget_low_s16(row1), consts, 3);
  tmp0_l = vmlal_lane_s16(tmp0_l, vget_low_s16(row3), consts, 2);
  tmp0_l = vmlal_lane_s16(tmp0_l, vget_low_s16(row5), consts, 1);
  tmp0_l = vmlal_lane_s16(tmp0_l, vget_low_s16(row7), consts, 0);
  int32x4_t tmp0_h = vmull_lane_s16(vget_high_s16(row1), consts, 3);
  tmp0_h = vmlal_lane_s16(tmp0_h, vget_high_s16(row3), consts, 2);
  tmp0_h = vmlal_lane_s16(tmp0_h, vget_high_s16(row5), consts, 1);
  tmp0_h = vmlal_lane_s16(tmp0_h, vget_high_s16(row7), consts, 0);

  /* Final output stage: descale and narrow to 16-bit. */
  row0 = vcombine_s16(vrshrn_n_s32(vaddq_s32(tmp10_l, tmp0_l), CONST_BITS),
                      vrshrn_n_s32(vaddq_s32(tmp10_h, tmp0_h), CONST_BITS));
  row1 = vcombine_s16(vrshrn_n_s32(vsubq_s32(tmp10_l, tmp0_l), CONST_BITS),
                      vrshrn_n_s32(vsubq_s32(tmp10_h, tmp0_h), CONST_BITS));

  /* Transpose two rows ready for second pass. */
  int16x8x2_t cols_0246_1357 = vtrnq_s16(row0, row1);
  int16x8_t cols_0246 = cols_0246_1357.val[0];
  int16x8_t cols_1357 = cols_0246_1357.val[1];
  /* Duplicate columns such that each is accessible in its own vector. */
  int32x4x2_t cols_1155_3377 = vtrnq_s32(vreinterpretq_s32_s16(cols_1357),
                                         vreinterpretq_s32_s16(cols_1357));
  int16x8_t cols_1155 = vreinterpretq_s16_s32(cols_1155_3377.val[0]);
  int16x8_t cols_3377 = vreinterpretq_s16_s32(cols_1155_3377.val[1]);

  /* Pass 2: process 2 rows, store to output array. */
  /* Even part: only interested in col0; top half of tmp10 is "don't care". */
  int32x4_t tmp10 = vshll_n_s16(vget_low_s16(cols_0246), CONST_BITS + 2);

  /* Odd part. Only interested in bottom half of tmp0. */
  int32x4_t tmp0 = vmull_lane_s16(vget_low_s16(cols_1155), consts, 3);
  tmp0 = vmlal_lane_s16(tmp0, vget_low_s16(cols_3377), consts, 2);
  tmp0 = vmlal_lane_s16(tmp0, vget_high_s16(cols_1155), consts, 1);
  tmp0 = vmlal_lane_s16(tmp0, vget_high_s16(cols_3377), consts, 0);

  /* Final output stage: descale and clamp to range [0-255]. */
  int16x8_t output_s16 = vcombine_s16(vaddhn_s32(tmp10, tmp0),
                                      vsubhn_s32(tmp10, tmp0));
  output_s16 = vrsraq_n_s16(vdupq_n_s16(CENTERJSAMPLE), output_s16,
                            CONST_BITS + PASS1_BITS + 3 + 2 - 16);
  /* Narrow to 8-bit and convert to unsigned. */
  uint8x8_t output_u8 = vqmovun_s16(output_s16);

  /* Store 2x2 block to memory. */
  vst1_lane_u8(output_buf[0] + output_col, output_u8, 0);
  vst1_lane_u8(output_buf[1] + output_col, output_u8, 1);
  vst1_lane_u8(output_buf[0] + output_col + 1, output_u8, 4);
  vst1_lane_u8(output_buf[1] + output_col + 1, output_u8, 5);
}
