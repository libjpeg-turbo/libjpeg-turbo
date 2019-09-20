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

/* INTEGER QUANTIZATION AND SAMPLE CONVERSION */

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "jmacros_msa.h"


GLOBAL(void)
jsimd_convsamp_msa(JSAMPARRAY sample_data, JDIMENSION start_col,
                   DCTELEM *workspace)
{
  v16u8 val0, val1, val2, val3, val4, val5, val6, val7;
  v16u8 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  v8i16 dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  v16u8 reg_128 = (v16u8)__msa_ldi_b(128);

  val0 = LD_UB(sample_data[0] + start_col);
  val1 = LD_UB(sample_data[1] + start_col);
  val2 = LD_UB(sample_data[2] + start_col);
  val3 = LD_UB(sample_data[3] + start_col);
  val4 = LD_UB(sample_data[4] + start_col);
  val5 = LD_UB(sample_data[5] + start_col);
  val6 = LD_UB(sample_data[6] + start_col);
  val7 = LD_UB(sample_data[7] + start_col);

  /* Interleave with CENTERSAMPLE */
  ILVR_B4_UB(val0, reg_128, val1, reg_128, val2, reg_128, val3, reg_128,
             tmp0, tmp1, tmp2, tmp3);
  ILVR_B4_UB(val4, reg_128, val5, reg_128, val6, reg_128, val7, reg_128,
             tmp4, tmp5, tmp6, tmp7);

  /* Subtract CENTRESAMPLE from u8 sample */
  HSUB_UB4_SH(tmp0, tmp1, tmp2, tmp3, dst0, dst1, dst2, dst3);
  HSUB_UB4_SH(tmp4, tmp5, tmp6, tmp7, dst4, dst5, dst6, dst7);

  /* Store results */
  ST_SH8(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7, workspace, 8);
}

GLOBAL(void)
jsimd_quantize_msa(JCOEFPTR coef_block, DCTELEM *divisors, DCTELEM *workspace)
{
  DCTELEM *recip, *corr, *shift;
  JDIMENSION cnt;
  v4i32 tmp0_r, tmp0_l, tmp1_r, tmp1_l, tmp2_r, tmp2_l, tmp3_r, tmp3_l;
  v4i32 shift0_r, shift1_r, shift2_r, shift3_r, shift0_l, shift1_l, shift2_l;
  v4i32 shift3_l, recip0_r, recip0_l, recip1_r, recip1_l;
  v4i32 recip2_r, recip2_l, recip3_r, recip3_l;
  v8i16 val0, val1, val2, val3, recip0, recip1, recip2, recip3;
  v8i16 corr0, corr1, corr2, corr3, shift0, shift1, shift2, shift3;
  v8i16 sign0, sign1, sign2, sign3, zero = { 0 };

  recip = divisors + 0 * 64;
  corr = divisors + 1 * 64;
  shift = divisors + 3 * 64;

  for (cnt = 0; cnt < 2; cnt++) {
    /* Load data, recip, corr, shift vectors */
    LD_SH4((workspace + cnt * 32), 8, val0, val1, val2, val3);
    LD_SH4((recip + cnt * 32), 8, recip0, recip1, recip2, recip3);
    LD_SH4((corr + cnt * 32), 8, corr0, corr1, corr2, corr3);
    LD_SH4((shift + cnt * 32), 8, shift0, shift1, shift2, shift3);

    /* Sign */
    sign0 = MSA_SRAI_H(val0, 15);
    sign1 = MSA_SRAI_H(val1, 15);
    sign2 = MSA_SRAI_H(val2, 15);
    sign3 = MSA_SRAI_H(val3, 15);

    /* Calculate absolute */
    XOR_V4_SH(val0, sign0, val1, sign1, val2, sign2, val3, sign3,
              val0, val1, val2, val3);
    SUB4(val0, sign0, val1, sign1, val2, sign2, val3, sign3,
         val0, val1, val2, val3);

    /* Add correction factor */
    ADD4(val0, corr0, val1, corr1, val2, corr2, val3, corr3,
         val0, val1, val2, val3);

    /* Prepare for multiplication; Promote to 32 bit */
    ILVRL_H2_SW(zero, val0, tmp0_r, tmp0_l);
    ILVRL_H2_SW(zero, val1, tmp1_r, tmp1_l);
    ILVRL_H2_SW(zero, val2, tmp2_r, tmp2_l);
    ILVRL_H2_SW(zero, val3, tmp3_r, tmp3_l);
    ILVRL_H2_SW(zero, recip0, recip0_r, recip0_l);
    ILVRL_H2_SW(zero, recip1, recip1_r, recip1_l);
    ILVRL_H2_SW(zero, recip2, recip2_r, recip2_l);
    ILVRL_H2_SW(zero, recip3, recip3_r, recip3_l);

    /* Multiply: val * recip */
    MUL4(tmp0_r, recip0_r, tmp0_l, recip0_l, tmp1_r, recip1_r,
         tmp1_l, recip1_l, tmp0_r, tmp0_l, tmp1_r, tmp1_l);
    MUL4(tmp2_r, recip2_r, tmp2_l, recip2_l, tmp3_r, recip3_r,
         tmp3_l, recip3_l, tmp2_r, tmp2_l, tmp3_r, tmp3_l);

    /* Promote shift to 32 bit */
    ILVRL_H2_SW(zero, shift0, shift0_r, shift0_l);
    ILVRL_H2_SW(zero, shift1, shift1_r, shift1_l);
    ILVRL_H2_SW(zero, shift2, shift2_r, shift2_l);
    ILVRL_H2_SW(zero, shift3, shift3_r, shift3_l);

    /* Shift by promoted shift register values */
    tmp0_r >>= shift0_r;
    tmp1_r >>= shift1_r;
    tmp2_r >>= shift2_r;
    tmp3_r >>= shift3_r;
    tmp0_l >>= shift0_l;
    tmp1_l >>= shift1_l;
    tmp2_l >>= shift2_l;
    tmp3_l >>= shift3_l;

    /* We need to right shift by 16 and then pckev. */
    /* Instead we'll do pckod. */
    PCKOD_H2_SH(tmp0_l, tmp0_r, tmp1_l, tmp1_r, val0, val1);
    PCKOD_H2_SH(tmp2_l, tmp2_r, tmp3_l, tmp3_r, val2, val3);

    /* Apply back original signs */
    XOR_V4_SH(val0, sign0, val1, sign1, val2, sign2, val3, sign3,
              val0, val1, val2, val3);
    SUB4(val0, sign0, val1, sign1, val2, sign2, val3, sign3,
         val0, val1, val2, val3);

    /* Store results */
    ST_SH4(val0, val1, val2, val3, (coef_block + cnt * 32), 8);
  }
}
