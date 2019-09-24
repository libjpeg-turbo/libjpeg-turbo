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


#define DCTSIZE          8
#define PASS1_BITS       2
#define CONST_BITS_FAST  8

#define FIX_1_082392200       ((int32_t)277)  /* FIX(1.082392200) */
#define FIX_1_414213562       ((int32_t)362)  /* FIX(1.414213562) */
#define FIX_1_847759065_fast  ((int32_t)473)  /* FIX(1.847759065) */
#define FIX_2_613125930       ((int32_t)669)  /* FIX(2.613125930) */

GLOBAL(void)
jsimd_idct_ifast_msa(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                     JCOEFPTR coef_block, JSAMPARRAY output_buf,
                     JDIMENSION output_col)
{
  v16u8 res0, res1, res2, res3, res4, res5, res6, res7;
  v8i16 val0, val1, val2, val3, val4, val5, val6, val7;
  v8i16 dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  v8i16 quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  v8i16 tmp10, tmp11, tmp12, tmp13, tmp14;
  v8i16 z5, z10, z11, z12, z13;
  v4i32 z5_r, z5_l, z12_r, z12_l, z10_r, z10_l;
  v4i32 tmp11_r, tmp11_l, tmp12_r, tmp12_l;
  v8i16 reg_128 = __msa_ldi_h(128);
  v4i32 tmp, tmp_r;
  JCOEFPTR quantptr = compptr->dct_table;
  v4i32 const0 = { FIX_1_414213562, FIX_1_847759065_fast,
                   FIX_1_082392200, -FIX_2_613125930
                 };

  /* Dequantize */
  LD_SH8(coef_block, DCTSIZE, val0, val1, val2, val3, val4, val5, val6, val7);

  /* Handle 0 AC terms */
  tmp_r = (v4i32)(val1 | val2 | val3 | val4 | val5 | val6 | val7);
  if (__msa_test_bz_v((v16u8)tmp_r)) {
    quant0 = LD_SH(quantptr);

    tmp0 = val0 * quant0;
    SPLATI_H4_SH(tmp0, 0, 1, 2, 3, dst0, dst1, dst2, dst3);
    SPLATI_H4_SH(tmp0, 4, 5, 6, 7, dst4, dst5, dst6, dst7);
  } else { /* if (!__msa_test_bz_v((v16u8)tmp_r)) */
    /* Pass 1: process columns. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */
    LD_SH8(quantptr, DCTSIZE, quant0, quant1, quant2, quant3,
           quant4, quant5, quant6, quant7);
    MUL4(val0, quant0, val1, quant1, val2, quant2, val3, quant3,
         val0, val1, val2, val3);
    MUL4(val4, quant4, val5, quant5, val6, quant6, val7, quant7,
         val4, val5, val6, val7);

    /* Even Part */
    BUTTERFLY_4(val0, val2, val6, val4, tmp10, tmp13, tmp12, tmp11);

    tmp = __msa_splati_w(const0, 0);
    tmp14 = val2 - val6;
    UNPCK_SH_SW(tmp14, tmp12_r, tmp12_l);
    MUL2(tmp12_r, tmp, tmp12_l, tmp, tmp12_r, tmp12_l);
    tmp12_r = MSA_SRAI_W(tmp12_r, CONST_BITS_FAST);
    tmp12_l = MSA_SRAI_W(tmp12_l, CONST_BITS_FAST);
    tmp12 = __msa_pckev_h((v8i16)tmp12_l, (v8i16)tmp12_r);
    tmp12 = tmp12 - tmp13;

    BUTTERFLY_4(tmp10, tmp11, tmp12, tmp13, tmp0, tmp1, tmp2, tmp3);

    /* Odd Part */
    BUTTERFLY_4(val5, val1, val7, val3, z13, z11, z12, z10);

    tmp7 = z11 + z13;

    /* tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); */
    tmp14 = z11 - z13;
    UNPCK_SH_SW(tmp14, tmp11_r, tmp11_l);
    MUL2(tmp11_r, tmp, tmp11_l, tmp, tmp11_r, tmp11_l);
    tmp11_r = MSA_SRAI_W(tmp11_r, CONST_BITS_FAST);
    tmp11_l = MSA_SRAI_W(tmp11_l, CONST_BITS_FAST);
    tmp11 = __msa_pckev_h((v8i16)tmp11_l, (v8i16)tmp11_r);

    /* z5 = MULTIPLY(z10 + z12, FIX_1_847759065);  */
    tmp = __msa_splati_w(const0, 1);
    tmp14 = z10 + z12;
    UNPCK_SH_SW(tmp14, z5_r, z5_l);
    MUL2(z5_r, tmp, z5_l, tmp, z5_r, z5_l);
    z5_r = MSA_SRAI_W(z5_r, CONST_BITS_FAST);
    z5_l = MSA_SRAI_W(z5_l, CONST_BITS_FAST);
    z5 = __msa_pckev_h((v8i16)z5_l, (v8i16)z5_r);

    /* tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; */
    tmp = __msa_splati_w(const0, 2);
    UNPCK_SH_SW(z12, z12_r, z12_l);
    MUL2(z12_r, tmp, z12_l, tmp, z12_r, z12_l);
    z12_r = MSA_SRAI_W(z12_r, CONST_BITS_FAST);
    z12_l = MSA_SRAI_W(z12_l, CONST_BITS_FAST);
    tmp10 = __msa_pckev_h((v8i16)z12_l, (v8i16)z12_r);
    tmp10 -= z5;

    /* tmp12 = MULTIPLY(z10, -FIX_2_613125930) + z5; */
    tmp = __msa_splati_w(const0, 3);
    UNPCK_SH_SW(z10, z10_r, z10_l);
    MUL2(z10_r, tmp, z10_l, tmp, z10_r, z10_l);
    z10_r = MSA_SRAI_W(z10_r, CONST_BITS_FAST);
    z10_l = MSA_SRAI_W(z10_l, CONST_BITS_FAST);
    tmp12 = __msa_pckev_h((v8i16)z10_l, (v8i16)z10_r);
    tmp12 += z5;

    tmp6 = tmp12 - tmp7;        /* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    BUTTERFLY_8(tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
                dst0, dst1, dst2, dst4, dst3, dst5, dst6, dst7);

    TRANSPOSE8x8_SH_SH(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7,
                       dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7);
  }

  /* Pass 2: process rows. */
  /* Even part */
  BUTTERFLY_4(dst0, dst2, dst6, dst4, tmp10, tmp13, tmp12, tmp11);

  /* tmp12 = MULTIPLY(dst2 - dst6, FIX_1_414213562) - tmp13;*/
  tmp = __msa_splati_w(const0, 0);
  tmp14 = dst2 - dst6;
  UNPCK_SH_SW(tmp14, tmp12_r, tmp12_l);
  MUL2(tmp12_r, tmp, tmp12_l, tmp, tmp12_r, tmp12_l);
  tmp12_r = MSA_SRAI_W(tmp12_r, CONST_BITS_FAST);
  tmp12_l = MSA_SRAI_W(tmp12_l, CONST_BITS_FAST);
  tmp12 = __msa_pckev_h((v8i16)tmp12_l, (v8i16)tmp12_r);
  tmp12 = tmp12 - tmp13;

  BUTTERFLY_4(tmp10, tmp11, tmp12, tmp13, tmp0, tmp1, tmp2, tmp3);

  /* Odd part */
  BUTTERFLY_4(dst5, dst1, dst7, dst3, z13, z11, z12, z10);

  tmp7 = z11 + z13;

  /* tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); */
  tmp14 = z11 - z13;
  UNPCK_SH_SW(tmp14, tmp11_r, tmp11_l);
  MUL2(tmp11_r, tmp, tmp11_l, tmp, tmp11_r, tmp11_l);
  tmp11_r = MSA_SRAI_W(tmp11_r, CONST_BITS_FAST);
  tmp11_l = MSA_SRAI_W(tmp11_l, CONST_BITS_FAST);
  tmp11 = __msa_pckev_h((v8i16)tmp11_l, (v8i16)tmp11_r);

  /* z5 = MULTIPLY(z10 + z12, FIX_1_847759065);  */
  tmp = __msa_splati_w(const0, 1);
  tmp14 = z10 + z12;
  UNPCK_SH_SW(tmp14, z5_r, z5_l);
  MUL2(z5_r, tmp, z5_l, tmp, z5_r, z5_l);
  z5_r = MSA_SRAI_W(z5_r, CONST_BITS_FAST);
  z5_l = MSA_SRAI_W(z5_l, CONST_BITS_FAST);
  z5 = __msa_pckev_h((v8i16)z5_l, (v8i16)z5_r);

  /* tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; */
  tmp = __msa_splati_w(const0, 2);
  UNPCK_SH_SW(z12, z12_r, z12_l);
  MUL2(z12_r, tmp, z12_l, tmp, z12_r, z12_l);
  z12_r = MSA_SRAI_W(z12_r, CONST_BITS_FAST);
  z12_l = MSA_SRAI_W(z12_l, CONST_BITS_FAST);
  tmp10 = __msa_pckev_h((v8i16)z12_l, (v8i16)z12_r);
  tmp10 -= z5;

  /* tmp12 = MULTIPLY(z10, -FIX_2_613125930) + z5; */
  tmp = __msa_splati_w(const0, 3);
  UNPCK_SH_SW(z10, z10_r, z10_l);
  MUL2(z10_r, tmp, z10_l, tmp, z10_r, z10_l);
  z10_r = MSA_SRAI_W(z10_r, CONST_BITS_FAST);
  z10_l = MSA_SRAI_W(z10_l, CONST_BITS_FAST);
  tmp12 = __msa_pckev_h((v8i16)z10_l, (v8i16)z10_r);
  tmp12 += z5;

  tmp6 = tmp12 - tmp7;        /* phase 2 */
  tmp5 = tmp11 - tmp6;
  tmp4 = tmp10 + tmp5;

  dst0 = CLIP_SH_0_255(MSA_SRAI_H((tmp0 + tmp7), (PASS1_BITS + 3)) + reg_128);
  dst7 = CLIP_SH_0_255(MSA_SRAI_H((tmp0 - tmp7), (PASS1_BITS + 3)) + reg_128);
  dst1 = CLIP_SH_0_255(MSA_SRAI_H((tmp1 + tmp6), (PASS1_BITS + 3)) + reg_128);
  dst6 = CLIP_SH_0_255(MSA_SRAI_H((tmp1 - tmp6), (PASS1_BITS + 3)) + reg_128);
  dst2 = CLIP_SH_0_255(MSA_SRAI_H((tmp2 + tmp5), (PASS1_BITS + 3)) + reg_128);
  dst5 = CLIP_SH_0_255(MSA_SRAI_H((tmp2 - tmp5), (PASS1_BITS + 3)) + reg_128);
  dst4 = CLIP_SH_0_255(MSA_SRAI_H((tmp3 + tmp4), (PASS1_BITS + 3)) + reg_128);
  dst3 = CLIP_SH_0_255(MSA_SRAI_H((tmp3 - tmp4), (PASS1_BITS + 3)) + reg_128);

  PCKEV_B4_UB(dst0, dst0, dst1, dst1, dst2, dst2, dst3, dst3,
              res0, res1, res2, res3);
  PCKEV_B4_UB(dst4, dst4, dst5, dst5, dst6, dst6, dst7, dst7,
              res4, res5, res6, res7);

  TRANSPOSE8x8_UB_UB(res0, res1, res2, res3, res4, res5, res6, res7,
                     res0, res1, res2, res3, res4, res5, res6, res7);

  ST8x1_UB(res0, output_buf[0]);
  ST8x1_UB(res1, output_buf[1]);
  ST8x1_UB(res2, output_buf[2]);
  ST8x1_UB(res3, output_buf[3]);
  ST8x1_UB(res4, output_buf[4]);
  ST8x1_UB(res5, output_buf[5]);
  ST8x1_UB(res6, output_buf[6]);
  ST8x1_UB(res7, output_buf[7]);
}
