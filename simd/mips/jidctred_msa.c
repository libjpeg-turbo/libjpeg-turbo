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
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"
#include "jmacros_msa.h"


#define DCTSIZE     8
#define CONST_BITS  13
#define PASS1_BITS  2

#define FIX_0_765366865  ((int32_t)6270)   /* FIX(0.765366865) */
#define FIX_0_899976223  ((int32_t)7373)   /* FIX(0.899976223) */
#define FIX_1_847759065  ((int32_t)15137)  /* FIX(1.847759065) */
#define FIX_2_562915447  ((int32_t)20995)  /* FIX(2.562915447) */
#define FIX_0_211164243  ((int32_t)1730)   /* FIX(0.211164243) */
#define FIX_0_509795579  ((int32_t)4176)   /* FIX(0.509795579) */
#define FIX_0_601344887  ((int32_t)4926)   /* FIX(0.601344887) */
#define FIX_0_720959822  ((int32_t)5906)   /* FIX(0.720959822) */
#define FIX_0_850430095  ((int32_t)6967)   /* FIX(0.850430095) */
#define FIX_1_061594337  ((int32_t)8697)   /* FIX(1.061594337) */
#define FIX_1_272758580  ((int32_t)10426)  /* FIX(1.272758580) */
#define FIX_1_451774981  ((int32_t)11893)  /* FIX(1.451774981) */
#define FIX_2_172734803  ((int32_t)17799)  /* FIX(2.172734803) */
#define FIX_3_624509785  ((int32_t)29692)  /* FIX(3.624509785) */

GLOBAL(void)
jsimd_idct_2x2_msa(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                   JCOEFPTR coef_block, JSAMPARRAY output_buf,
                   JDIMENSION output_col)
{
  uint8_t out0, out1, out2, out3;
  v8i16 res, val0, val1, val3, val5, val7;
  v8i16 quant0, quant1, quant3, quant5, quant7;
  v4i32 tmp0, tmp10, tmp0_r, tmp0_l, tmp10_r, tmp10_l;
  v4i32 dst0_r, dst1_r, dst0_l, dst1_l;
  v4i32 d0_r, d1_r, d3_r, d5_r, d7_r, d0_l, d1_l, d3_l, d5_l, d7_l;
  JCOEFPTR quantptr = compptr->dct_table;
  v4i32 const0 =
    { FIX_3_624509785, -FIX_1_272758580, FIX_3_624509785, -FIX_1_272758580 };
  v4i32 const1 =
    { FIX_0_850430095, -FIX_0_720959822, FIX_0_850430095, -FIX_0_720959822 };

  /* Pass 1: process columns from input, store into work array. */

  /* Load coeff values */
  val0 = LD_SH(coef_block);
  LD_SH4(coef_block + DCTSIZE, DCTSIZE * 2, val1, val3, val5, val7);

  /* Load quant value */
  quant0 = LD_SH(quantptr);
  res = val1 | val3 | val5 | val7;

  if (__msa_test_bz_v((v16u8)res)) {
    val0 = val0 * quant0;
    UNPCK_SH_SW(val0, dst0_r, dst0_l);
    dst0_r = MSA_SLLI_W(dst0_r, PASS1_BITS);
    dst0_l = MSA_SLLI_W(dst0_l, PASS1_BITS);

    dst1_r = dst0_r;
    dst1_l = dst0_l;
  } else {
    /* Pass 1: process columns. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */

    /* Load remaining quant values */
    LD_SH4(quantptr + DCTSIZE, 2 * DCTSIZE, quant1, quant3, quant5, quant7);

    /* Dequantize */
    val0 *= quant0;
    MUL4(val1, quant1, val3, quant3, val5, quant5, val7, quant7,
         val1, val3, val5, val7);

    /* Even Part */
    UNPCK_SH_SW(val0, d0_r, d0_l);
    d0_r = MSA_SLLI_W(d0_r, (CONST_BITS + 2));
    d0_l = MSA_SLLI_W(d0_l, (CONST_BITS + 2));
    tmp10_r = d0_r;
    tmp10_l = d0_l;

    /* Odd Part */
    UNPCK_SH_SW(val1, d1_r, d1_l); /* z4 */
    UNPCK_SH_SW(val3, d3_r, d3_l); /* z3 */
    UNPCK_SH_SW(val5, d5_r, d5_l); /* z2 */
    UNPCK_SH_SW(val7, d7_r, d7_l); /* z1 */

    tmp0_r = d7_r * __msa_fill_w(-FIX_0_720959822) +
             d5_r * __msa_fill_w(FIX_0_850430095) -
             d3_r * __msa_fill_w(FIX_1_272758580) +
             d1_r * __msa_fill_w(FIX_3_624509785);
    tmp0_l = d7_l * __msa_fill_w(-FIX_0_720959822) +
             d5_l * __msa_fill_w(FIX_0_850430095) -
             d3_l * __msa_fill_w(FIX_1_272758580) +
             d1_l * __msa_fill_w(FIX_3_624509785);

    BUTTERFLY_4(tmp10_r, tmp10_l, tmp0_l, tmp0_r,
                dst0_r, dst0_l, dst1_l, dst1_r);

    /* Descale */
    SRARI_W4_SW(dst0_r, dst0_l, dst1_r, dst1_l, CONST_BITS - PASS1_BITS + 2);
  }

  /* Get 0th row */
  tmp10 = __msa_ilvr_w(dst1_r, dst0_r);
  tmp10 = MSA_SLLI_W(tmp10, (CONST_BITS + 2));

  /* Ignore 0 2 4 and 6th row */
  dst0_l = __msa_pckod_w(dst1_l, dst0_l); /* 1 3 1 3 */
  dst0_r = __msa_pckod_w(dst1_r, dst0_r); /* 5 7 5 7 */

  dst0_l = (v4i32)__msa_dotp_s_d(dst0_l, const1);
  dst0_r = (v4i32)__msa_dotp_s_d(dst0_r, const0);

  tmp0 = dst0_l + dst0_r;
  tmp0 = __msa_pckev_w(tmp0, tmp0);

  dst0_r = tmp10 + tmp0;
  dst1_r = tmp10 - tmp0;

  /* Descale */
  SRARI_W2_SW(dst0_r, dst1_r, CONST_BITS + PASS1_BITS + 3 + 2);

  res = (v8i16)__msa_pckev_d((v2i64)dst1_r, (v2i64)dst0_r);
  res = MSA_ADDVI_H(res, 128);
  res = CLIP_SH_0_255(res);

  out0 = __msa_copy_u_b((v16i8)res, 0);
  out1 = __msa_copy_u_b((v16i8)res, 4);
  out2 = __msa_copy_u_b((v16i8)res, 8);
  out3 = __msa_copy_u_b((v16i8)res, 12);

  *(output_buf[0]) = out0;
  *(output_buf[0] + 1) = out2;
  *(output_buf[1]) = out1;
  *(output_buf[1] + 1) = out3;
}

GLOBAL(void)
jsimd_idct_4x4_msa(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                   JCOEFPTR coef_block, JSAMPARRAY output_buf,
                   JDIMENSION output_col)
{
  uint32_t out0, out1, out2, out3;
  v8i16 val0, val1, val2, val3, val5, val6, val7;
  v8i16 quant0, quant1, quant2, quant3, quant5, quant6, quant7;
  v4i32 tmp2_r, tmp2_l, tmp0_r, tmp0_l, tmp10_r, tmp10_l, tmp12_r, tmp12_l;
  v4i32 res, dst0_r, dst1_r, dst2_r, dst3_r, dst0_l, dst1_l, dst2_l, dst3_l;
  v4i32 d0_r, d1_r, d2_r, d3_r, d5_r, d6_r, d7_r;
  v4i32 d0_l, d1_l, d2_l, d3_l, d5_l, d6_l, d7_l;
  v8i16 reg_128 = __msa_ldi_h(128);
  JCOEFPTR quantptr = compptr->dct_table;

  /* Pass 1: process columns from input, store into work array. */

  /* Load coeff values */
  LD_SH4(coef_block, DCTSIZE, val0, val1, val2, val3);
  LD_SH2(coef_block + 5 * DCTSIZE, DCTSIZE, val5, val6);
  val7 = LD_SH(coef_block + 7 * DCTSIZE);

  /* Load quant value */
  quant0 = LD_SH(quantptr);
  res = (v4i32)(val1 | val2 | val3 | val5 | val6 | val7);
  if (__msa_test_bz_v((v16u8)res)) {
    val0 = val0 * quant0;
    UNPCK_SH_SW(val0, dst0_r, dst0_l);
    dst0_r = MSA_SLLI_W(dst0_r, PASS1_BITS);
    dst0_l = MSA_SLLI_W(dst0_l, PASS1_BITS);

    SPLATI_W4_SW(dst0_r, d0_r, d1_r, d2_r, d3_r);
    SPLATI_W2_SW(dst0_l, 2, d6_r, d7_r);
    d5_r = __msa_splati_w(dst0_l, 1);
  } else {
    /* Pass 1: process columns. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */

    /* Load remaining quant values */
    LD_SH2(quantptr + DCTSIZE, DCTSIZE, quant1, quant2);
    quant3 = LD_SH(quantptr + 3 * DCTSIZE);
    LD_SH2(quantptr + 5 * DCTSIZE, DCTSIZE, quant5, quant6);
    quant7 = LD_SH(quantptr + 7 * DCTSIZE);

    /* Dequantize */
    MUL4(val0, quant0, val1, quant1, val2, quant2, val3, quant3,
         val0, val1, val2, val3);
    MUL2(val5, quant5, val6, quant6, val5, val6);
    val7 *= quant7;

    /* Even Part */
    UNPCK_SH_SW(val0, d0_r, d0_l);
    UNPCK_SH_SW(val2, d2_r, d2_l);
    UNPCK_SH_SW(val6, d6_r, d6_l);

    d0_r = MSA_SLLI_W(d0_r, (CONST_BITS + 1));
    d0_l = MSA_SLLI_W(d0_l, (CONST_BITS + 1));
    /* tmp2 = MULTIPLY(z2, FIX_1_847759065) + MULTIPLY(z3, -FIX_0_765366865); */
    d2_r = d2_r * __msa_fill_w(FIX_1_847759065);
    d2_l = d2_l * __msa_fill_w(FIX_1_847759065);
    d6_r = d6_r * __msa_fill_w(-FIX_0_765366865);
    d6_l = d6_l * __msa_fill_w(-FIX_0_765366865);

    d2_r += d6_r;
    d2_l += d6_l;

    BUTTERFLY_4(d0_r, d0_l, d2_l, d2_r, tmp10_r, tmp10_l, tmp12_l, tmp12_r);

    /* Odd Part */
    UNPCK_SH_SW(val1, d1_r, d1_l); /* z4 */
    UNPCK_SH_SW(val3, d3_r, d3_l); /* z3 */
    UNPCK_SH_SW(val5, d5_r, d5_l); /* z2 */
    UNPCK_SH_SW(val7, d7_r, d7_l); /* z1 */

    tmp0_r = d7_r * __msa_fill_w(-FIX_0_211164243) +
             d5_r * __msa_fill_w(FIX_1_451774981) -
             d3_r * __msa_fill_w(FIX_2_172734803) +
             d1_r * __msa_fill_w(FIX_1_061594337);
    tmp0_l = d7_l * __msa_fill_w(-FIX_0_211164243) +
             d5_l * __msa_fill_w(FIX_1_451774981) -
             d3_l * __msa_fill_w(FIX_2_172734803) +
             d1_l * __msa_fill_w(FIX_1_061594337);

    tmp2_r = d7_r * __msa_fill_w(-FIX_0_509795579) -
             d5_r * __msa_fill_w(FIX_0_601344887) +
             d3_r * __msa_fill_w(FIX_0_899976223) +
             d1_r * __msa_fill_w(FIX_2_562915447);
    tmp2_l = d7_l * __msa_fill_w(-FIX_0_509795579) -
             d5_l * __msa_fill_w(FIX_0_601344887) +
             d3_l * __msa_fill_w(FIX_0_899976223) +
             d1_l * __msa_fill_w(FIX_2_562915447);

    BUTTERFLY_4(tmp10_r, tmp12_r, tmp0_r, tmp2_r,
                dst0_r, dst1_r, dst2_r, dst3_r);
    BUTTERFLY_4(tmp10_l, tmp12_l, tmp0_l, tmp2_l,
                dst0_l, dst1_l, dst2_l, dst3_l);

    /* Descale */
    SRARI_W4_SW(dst0_r, dst0_l, dst1_r, dst1_l, CONST_BITS - PASS1_BITS + 1);
    SRARI_W4_SW(dst2_r, dst2_l, dst3_r, dst3_l, CONST_BITS - PASS1_BITS + 1);

    TRANSPOSE4x4_SW_SW(dst0_r, dst1_r, dst2_r, dst3_r, d0_r, d1_r, d2_r, d3_r);

    /* Partial transpose!, one register ignored */
    ILVRL_W2_SH(dst1_l, dst0_l, val0, val1);
    ILVRL_W2_SH(dst3_l, dst2_l, val2, val3);

    d5_r = (v4i32)__msa_ilvl_d((v2i64)val2, (v2i64)val0);
    d6_r = (v4i32)__msa_ilvr_d((v2i64)val3, (v2i64)val1);
    d7_r = (v4i32)__msa_ilvl_d((v2i64)val3, (v2i64)val1);
  }

  /* Pass 2: process 4 rows from work array, store into output array. */

  /* Even Part */
  d0_r = MSA_SLLI_W(d0_r, (CONST_BITS + 1));

  d2_r = d2_r * __msa_fill_w(FIX_1_847759065);
  d6_r = d6_r * __msa_fill_w(-FIX_0_765366865);
  d2_r += d6_r;

  tmp10_r = d0_r + d2_r;
  tmp12_r = d0_r - d2_r;

  /* Odd Part */
  tmp0_r = d7_r * __msa_fill_w(-FIX_0_211164243) +
           d5_r * __msa_fill_w(FIX_1_451774981) -
           d3_r * __msa_fill_w(FIX_2_172734803) +
           d1_r * __msa_fill_w(FIX_1_061594337);

  tmp2_r = d7_r * __msa_fill_w(-FIX_0_509795579) -
           d5_r * __msa_fill_w(FIX_0_601344887) +
           d3_r * __msa_fill_w(FIX_0_899976223) +
           d1_r * __msa_fill_w(FIX_2_562915447);

  BUTTERFLY_4(tmp10_r, tmp12_r, tmp0_r, tmp2_r, dst0_r, dst1_r, dst2_r, dst3_r);

  /* Descale */
  SRARI_W4_SW(dst0_r, dst1_r, dst2_r, dst3_r, CONST_BITS + PASS1_BITS + 3 + 1);
  TRANSPOSE4x4_SW_SW(dst0_r, dst1_r, dst2_r, dst3_r,
                     dst0_r, dst1_r, dst2_r, dst3_r);
  val0 = __msa_pckev_h((v8i16)dst1_r, (v8i16)dst0_r);
  val1 = __msa_pckev_h((v8i16)dst3_r, (v8i16)dst2_r);
  val0 = __msa_adds_s_h(val0, reg_128);
  val1 = __msa_adds_s_h(val1, reg_128);
  CLIP_SH2_0_255(val0, val1);
  res = (v4i32)__msa_pckev_b((v16i8)val1, (v16i8)val0);

  /* Macro intentionally not used! */
  out0 = __msa_copy_u_w(res, 0);
  out1 = __msa_copy_u_w(res, 1);
  out2 = __msa_copy_u_w(res, 2);
  out3 = __msa_copy_u_w(res, 3);

  SW(out0, output_buf[0]);
  SW(out1, output_buf[1]);
  SW(out2, output_buf[2]);
  SW(out3, output_buf[3]);
}
