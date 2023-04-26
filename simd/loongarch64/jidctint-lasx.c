/*
 * LOONGARCH LASX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2021 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Jin Bo (jinbo@loongson.cn)
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
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "jmacros_lasx.h"

#define DCTSIZE     8
#define CONST_BITS  13
#define PASS1_BITS  2
#define SHIFT_11 (CONST_BITS-PASS1_BITS)
#define SHIFT_18 (CONST_BITS+PASS1_BITS+3)

#define FIX_0_298631336  ((int32_t)2446)   /* FIX(0.298631336) */
#define FIX_0_390180644  ((int32_t)3196)   /* FIX(0.390180644) */
#define FIX_0_541196100  ((int32_t)4433)   /* FIX(0.541196100) */
#define FIX_0_765366865  ((int32_t)6270)   /* FIX(0.765366865) */
#define FIX_0_899976223  ((int32_t)7373)   /* FIX(0.899976223) */
#define FIX_1_175875602  ((int32_t)9633)   /* FIX(1.175875602) */
#define FIX_1_501321110  ((int32_t)12299)  /* FIX(1.501321110) */
#define FIX_1_847759065  ((int32_t)15137)  /* FIX(1.847759065) */
#define FIX_1_961570560  ((int32_t)16069)  /* FIX(1.961570560) */
#define FIX_2_053119869  ((int32_t)16819)  /* FIX(2.053119869) */
#define FIX_2_562915447  ((int32_t)20995)  /* FIX(2.562915447) */
#define FIX_3_072711026  ((int32_t)25172)  /* FIX(3.072711026) */

#define LASX_RANGE_PACK(_in1, _in2, _out1, _out2) \
{ \
  __m256i _pack_1_2; \
  _pack_1_2 = __lasx_xvpickev_h(_in2, _in1); \
  _pack_1_2 = __lasx_xvpermi_d(_pack_1_2, 0xd8); \
  _pack_1_2 = __lasx_xvadd_h(_pack_1_2, const128); \
  LASX_CLIP_H_0_255(_pack_1_2, _pack_1_2); \
  _pack_1_2 = __lasx_xvpickev_b(zero, _pack_1_2); \
  _out1 = __lasx_xvpermi_q(_pack_1_2, zero, 0x02); \
  _out2 = __lasx_xvpermi_q(_pack_1_2, zero, 0x13); \
}

#define LASX_MUL_W_H(_in0, _in1, _out0, _out1) \
{ \
  __m256i _tmp0, _tmp1, _tmp2, _tmp3; \
  LASX_UNPCKLH_W_H(_in0, _tmp1, _tmp0); \
  LASX_UNPCKLH_W_H(_in1, _tmp3, _tmp2); \
  _out0 = __lasx_xvmul_w(_tmp0, _tmp2); \
  _out1 = __lasx_xvmul_w(_tmp1, _tmp3); \
}

GLOBAL(void)
jsimd_idct_islow_lasx(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                      JCOEFPTR coef_block, JSAMPARRAY output_buf,
                      JDIMENSION output_col)
{
  JCOEFPTR quantptr = compptr->dct_table;
  __m256i coef01, coef23, coef45, coef67;
  __m256i quant01, quant23, quant45, quant67;
  __m256i zero = __lasx_xvldi(0);
  __m256i tmp0, tmp1, tmp2, tmp3;
  __m256i tmp10, tmp11, tmp12, tmp13;

  __m256i dcval0, dcval1, dcval2, dcval3;
  __m256i dcval4, dcval5, dcval6, dcval7;

  __m256i z1, z2, z3, z4, z5;

  __m256i const100 = { FIX_0_541196100 };
  __m256i const065 = {-FIX_1_847759065 };
  __m256i const865 = { FIX_0_765366865 };
  __m256i const602 = { FIX_1_175875602 };
  __m256i const223 = {-FIX_0_899976223 };
  __m256i const447 = {-FIX_2_562915447 };
  __m256i const560 = {-FIX_1_961570560 };
  __m256i const644 = {-FIX_0_390180644 };
  __m256i const336 = { FIX_0_298631336 };
  __m256i const869 = { FIX_2_053119869 };
  __m256i const026 = { FIX_3_072711026 };
  __m256i const110 = { FIX_1_501321110 };

  __m256i const128 = {128};

  const100 = __lasx_xvreplve0_w(const100);
  const065 = __lasx_xvreplve0_w(const065);
  const865 = __lasx_xvreplve0_w(const865);
  const602 = __lasx_xvreplve0_w(const602);

  const223 = __lasx_xvreplve0_w(const223);
  const447 = __lasx_xvreplve0_w(const447);
  const560 = __lasx_xvreplve0_w(const560);
  const644 = __lasx_xvreplve0_w(const644);

  const336 = __lasx_xvreplve0_w(const336);
  const869 = __lasx_xvreplve0_w(const869);
  const026 = __lasx_xvreplve0_w(const026);
  const110 = __lasx_xvreplve0_w(const110);

  const128 = __lasx_xvreplve0_h(const128);

  LASX_LD_4(coef_block, 16, coef01, coef23, coef45, coef67);
  quant01 = LASX_LD(quantptr);

  tmp0 = __lasx_xvpermi_q(coef01, zero, 0x13);
  tmp0 = __lasx_xvor_v(tmp0, coef23);
  tmp0 = __lasx_xvor_v(tmp0, coef45);
  tmp0 = __lasx_xvor_v(tmp0, coef67);

  if (__lasx_xbz_v(tmp0)) {
    /* AC terms all zero */
    __m256i coef0, quant0;
    LASX_UNPCKL_W_H(coef01, coef0);
    LASX_UNPCKL_W_H(quant01, quant0);
    dcval0 = __lasx_xvmul_w(coef0, quant0);
    dcval0 = __lasx_xvslli_w(dcval0, PASS1_BITS);
    tmp1  = __lasx_xvpermi_q(dcval0, dcval0, 0x02);/* low */
    tmp2  = __lasx_xvpermi_q(dcval0, dcval0, 0x13);/* high */
    dcval0 = __lasx_xvrepl128vei_w(tmp1, 0);
    dcval1 = __lasx_xvrepl128vei_w(tmp1, 1);
    dcval2 = __lasx_xvrepl128vei_w(tmp1, 2);
    dcval3 = __lasx_xvrepl128vei_w(tmp1, 3);
    dcval4 = __lasx_xvrepl128vei_w(tmp2, 0);
    dcval5 = __lasx_xvrepl128vei_w(tmp2, 1);
    dcval6 = __lasx_xvrepl128vei_w(tmp2, 2);
    dcval7 = __lasx_xvrepl128vei_w(tmp2, 3);
  } else {
    LASX_LD_4(quantptr, 16, quant01, quant23, quant45, quant67);

    LASX_MUL_W_H(coef01, quant01, dcval0, dcval1);
    LASX_MUL_W_H(coef23, quant23, dcval2, dcval3);
    LASX_MUL_W_H(coef45, quant45, dcval4, dcval5);
    LASX_MUL_W_H(coef67, quant67, dcval6, dcval7);

    /* Even part */

    z1   = __lasx_xvadd_w(dcval2, dcval6);
    z1   = __lasx_xvmul_w(z1, const100);
    tmp2 = __lasx_xvmadd_w(z1, dcval6, const065);
    tmp3 = __lasx_xvmadd_w(z1, dcval2, const865);

    tmp0 = __lasx_xvadd_w(dcval0, dcval4);
    tmp1 = __lasx_xvsub_w(dcval0, dcval4);
    tmp0 = __lasx_xvslli_w(tmp0, CONST_BITS);
    tmp1 = __lasx_xvslli_w(tmp1, CONST_BITS);

    LASX_BUTTERFLY_4(v8i32, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

    /* Odd part */

    z1   = __lasx_xvadd_w(dcval1, dcval7);
    z2   = __lasx_xvadd_w(dcval3, dcval5);
    z3   = __lasx_xvadd_w(dcval3, dcval7);
    z4   = __lasx_xvadd_w(dcval1, dcval5);

    z5   = __lasx_xvadd_w(z3, z4);
    z5   = __lasx_xvmul_w(z5, const602);

    z1   = __lasx_xvmul_w(z1, const223);
    z2   = __lasx_xvmul_w(z2, const447);
    z3   = __lasx_xvmadd_w(z5, z3, const560);
    z4   = __lasx_xvmadd_w(z5, z4, const644);

    tmp0 = __lasx_xvadd_w(z1, z3);
    tmp1 = __lasx_xvadd_w(z2, z4);
    tmp2 = __lasx_xvadd_w(z2, z3);
    tmp3 = __lasx_xvadd_w(z1, z4);

    tmp0 = __lasx_xvmadd_w(tmp0, dcval7, const336);
    tmp1 = __lasx_xvmadd_w(tmp1, dcval5, const869);
    tmp2 = __lasx_xvmadd_w(tmp2, dcval3, const026);
    tmp3 = __lasx_xvmadd_w(tmp3, dcval1, const110);

    dcval0 = __lasx_xvadd_w(tmp10, tmp3);
    dcval0 = __lasx_xvsrari_w(dcval0, SHIFT_11);

    dcval1 = __lasx_xvadd_w(tmp11, tmp2);
    dcval1 = __lasx_xvsrari_w(dcval1, SHIFT_11);

    dcval2 = __lasx_xvadd_w(tmp12, tmp1);
    dcval2 = __lasx_xvsrari_w(dcval2, SHIFT_11);

    dcval3 = __lasx_xvadd_w(tmp13, tmp0);
    dcval3 = __lasx_xvsrari_w(dcval3, SHIFT_11);

    dcval4 = __lasx_xvsub_w(tmp13, tmp0);
    dcval4 = __lasx_xvsrari_w(dcval4, SHIFT_11);

    dcval5 = __lasx_xvsub_w(tmp12, tmp1);
    dcval5 = __lasx_xvsrari_w(dcval5, SHIFT_11);

    dcval6 = __lasx_xvsub_w(tmp11, tmp2);
    dcval6 = __lasx_xvsrari_w(dcval6, SHIFT_11);

    dcval7 = __lasx_xvsub_w(tmp10, tmp3);
    dcval7 = __lasx_xvsrari_w(dcval7, SHIFT_11);

    LASX_TRANSPOSE8x8_W(dcval0, dcval1, dcval2, dcval3, dcval4, dcval5, dcval6, dcval7,
                        dcval0, dcval1, dcval2, dcval3, dcval4, dcval5, dcval6, dcval7);

  }

  /* Even part */

  z1   = __lasx_xvadd_w(dcval2, dcval6);
  z1   = __lasx_xvmul_w(z1, const100);
  tmp2 = __lasx_xvmadd_w(z1, dcval6, const065);
  tmp3 = __lasx_xvmadd_w(z1, dcval2, const865);

  tmp0 = __lasx_xvadd_w(dcval0, dcval4);
  tmp1 = __lasx_xvsub_w(dcval0, dcval4);
  tmp0 = __lasx_xvslli_w(tmp0, CONST_BITS);
  tmp1 = __lasx_xvslli_w(tmp1, CONST_BITS);

  LASX_BUTTERFLY_4(v8i32, tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13);

  /* Odd part */

  z1   = __lasx_xvadd_w(dcval1, dcval7);
  z2   = __lasx_xvadd_w(dcval3, dcval5);
  z3   = __lasx_xvadd_w(dcval3, dcval7);
  z4   = __lasx_xvadd_w(dcval1, dcval5);

  z5   = __lasx_xvadd_w(z3, z4);
  z5   = __lasx_xvmul_w(z5, const602);

  z1   = __lasx_xvmul_w(z1, const223);
  z2   = __lasx_xvmul_w(z2, const447);
  z3   = __lasx_xvmadd_w(z5, z3, const560);
  z4   = __lasx_xvmadd_w(z5, z4, const644);

  tmp0 = __lasx_xvadd_w(z1, z3);
  tmp1 = __lasx_xvadd_w(z2, z4);
  tmp2 = __lasx_xvadd_w(z2, z3);
  tmp3 = __lasx_xvadd_w(z1, z4);

  tmp0 = __lasx_xvmadd_w(tmp0, dcval7, const336);
  tmp1 = __lasx_xvmadd_w(tmp1, dcval5, const869);
  tmp2 = __lasx_xvmadd_w(tmp2, dcval3, const026);
  tmp3 = __lasx_xvmadd_w(tmp3, dcval1, const110);

  /*
   * outptr[0], outptr[1]
   */
  dcval0 = __lasx_xvadd_w(tmp10, tmp3);
  dcval0 = __lasx_xvsrari_w(dcval0, SHIFT_18);
  dcval1 = __lasx_xvadd_w(tmp11, tmp2);
  dcval1 = __lasx_xvsrari_w(dcval1, SHIFT_18);
  LASX_RANGE_PACK(dcval0, dcval1, dcval0, dcval1);

  /*
   * outptr[2], outptr[3]
   */
  dcval2 = __lasx_xvadd_w(tmp12, tmp1);
  dcval2 = __lasx_xvsrari_w(dcval2, SHIFT_18);
  dcval3 = __lasx_xvadd_w(tmp13, tmp0);
  dcval3 = __lasx_xvsrari_w(dcval3, SHIFT_18);
  LASX_RANGE_PACK(dcval2, dcval3, dcval2, dcval3);

  /*
   * outptr[4], outptr[5]
   */
  dcval4 = __lasx_xvsub_w(tmp13, tmp0);
  dcval4 = __lasx_xvsrari_w(dcval4, SHIFT_18);
  dcval5 = __lasx_xvsub_w(tmp12, tmp1);
  dcval5 = __lasx_xvsrari_w(dcval5, SHIFT_18);
  LASX_RANGE_PACK(dcval4, dcval5, dcval4, dcval5);

  /*
   * outptr[6], outptr[7]
   */
  dcval6 = __lasx_xvsub_w(tmp11, tmp2);
  dcval6 = __lasx_xvsrari_w(dcval6, SHIFT_18);
  dcval7 = __lasx_xvsub_w(tmp10, tmp3);
  dcval7 = __lasx_xvsrari_w(dcval7, SHIFT_18);
  LASX_RANGE_PACK(dcval6, dcval7, dcval6, dcval7);

  LASX_TRANSPOSE8x8_B(dcval0, dcval1, dcval2, dcval3, dcval4, dcval5, dcval6, dcval7,
                      dcval0, dcval1, dcval2, dcval3, dcval4, dcval5, dcval6, dcval7);

  __lasx_xvstelm_d(dcval0, output_buf[0], 0, 0);
  __lasx_xvstelm_d(dcval1, output_buf[1], 0, 0);
  __lasx_xvstelm_d(dcval2, output_buf[2], 0, 0);
  __lasx_xvstelm_d(dcval3, output_buf[3], 0, 0);
  __lasx_xvstelm_d(dcval4, output_buf[4], 0, 0);
  __lasx_xvstelm_d(dcval5, output_buf[5], 0, 0);
  __lasx_xvstelm_d(dcval6, output_buf[6], 0, 0);
  __lasx_xvstelm_d(dcval7, output_buf[7], 0, 0);
}
