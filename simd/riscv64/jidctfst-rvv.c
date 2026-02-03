/*
 * Fast Integer Inverse DCT (64-bit RVV 1.0)
 *
 * Copyright (C) 2014, 2026, D. R. Commander.
 * Copyright (C) 2022-2023, Institute of Software, Chinese Academy of Sciences.
 *                          Author:  Zhiyuan Tan
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

#include "../jsimdint.h"
#include <riscv_vector.h>


#define FIX_1_082  277  /* FIX(1.082392200) */
#define FIX_1_414  362  /* FIX(1.414213562) */
#define FIX_1_847  473  /* FIX(1.847759065) */
#define FIX_2_613  669  /* FIX(2.613125930) */

#define CONST_BITS  8
#define PASS1_BITS  2


#define DO_IDCT(in) { \
  /* Even part */ \
  tmp10 = __riscv_vadd_vv_i16m1(in##0, in##4, vl); \
  tmp11 = __riscv_vsub_vv_i16m1(in##0, in##4, vl); \
  \
  tmp13 = __riscv_vadd_vv_i16m1(in##2, in##6, vl); \
  tmp12 = __riscv_vsub_vv_i16m1(in##2, in##6, vl); \
  tmp12_32 = __riscv_vwmul_vx_i32m2(tmp12, FIX_1_414, vl); \
  tmp12 = __riscv_vnsra_wx_i16m1(tmp12_32, CONST_BITS, vl); \
  tmp12 = __riscv_vsub_vv_i16m1(tmp12, tmp13, vl); \
  \
  tmp0 = __riscv_vadd_vv_i16m1(tmp10, tmp13, vl); \
  tmp3 = __riscv_vsub_vv_i16m1(tmp10, tmp13, vl); \
  tmp1 = __riscv_vadd_vv_i16m1(tmp11, tmp12, vl); \
  tmp2 = __riscv_vsub_vv_i16m1(tmp11, tmp12, vl); \
  \
  /* Odd part */ \
  z13 = __riscv_vadd_vv_i16m1(in##5, in##3, vl); \
  z10 = __riscv_vsub_vv_i16m1(in##5, in##3, vl); \
  z11 = __riscv_vadd_vv_i16m1(in##1, in##7, vl); \
  z12 = __riscv_vsub_vv_i16m1(in##1, in##7, vl); \
  \
  tmp11 = __riscv_vsub_vv_i16m1(z11, z13, vl); \
  tmp11_32 = __riscv_vwmul_vx_i32m2(tmp11, FIX_1_414, vl); \
  tmp11 = __riscv_vnsra_wx_i16m1(tmp11_32, CONST_BITS, vl); \
  \
  tmp7 = __riscv_vadd_vv_i16m1(z11, z13, vl); \
  \
  z5 = __riscv_vadd_vv_i16m1(z10, z12, vl); \
  z5_32 = __riscv_vwmul_vx_i32m2(z5, FIX_1_847, vl); \
  z5 = __riscv_vnsra_wx_i16m1(z5_32, CONST_BITS, vl); \
  \
  tmp10_32 = __riscv_vwmul_vx_i32m2(z12, FIX_1_082, vl); \
  tmp10 = __riscv_vnsra_wx_i16m1(tmp10_32, CONST_BITS, vl); \
  tmp10 = __riscv_vsub_vv_i16m1(tmp10, z5, vl); \
  tmp12_32 = __riscv_vwmul_vx_i32m2(z10, -FIX_2_613, vl); \
  tmp12 = __riscv_vnsra_wx_i16m1(tmp12_32, CONST_BITS, vl); \
  tmp12 = __riscv_vadd_vv_i16m1(z5, tmp12, vl); \
  \
  tmp6 = __riscv_vsub_vv_i16m1(tmp12, tmp7, vl); \
  tmp5 = __riscv_vsub_vv_i16m1(tmp11, tmp6, vl); \
  tmp4 = __riscv_vadd_vv_i16m1(tmp10, tmp5, vl); \
}


HIDDEN void
jsimd_idct_ifast_rvv(void *dct_table, JCOEFPTR coef_block,
                     JSAMPARRAY output_buf, JDIMENSION output_col)
{
  IFAST_MULT_TYPE *quantptr = dct_table;
  DCTELEM workspace[DCTSIZE2];

  vuint8mf2_t dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
  vint16m1_t row0, row1, row2, row3, row4, row5, row6, row7,
    col0, col1, col2, col3, col4, col5, col6, col7,
    quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7,
    tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13,
    z5, z10, z11, z12, z13,
    out0, out1, out2, out3, out4, out5, out6, out7;
  vint32m2_t tmp10_32, tmp11_32, tmp12_32, z5_32;
  vbool16_t mask;

  /* The minimum register width (VLEN) for standard CPUs in RVV 1.0 is
   * 128 bits.  Thus, this should always be 8.
   */
  size_t vl = __riscv_vsetvl_e16m1(DCTSIZE),
    col_stride = DCTSIZE * sizeof(DCTELEM);

  /* Pass 1: process columns from input, store into work array. */

  /* Load row vectors. */
  row0 = __riscv_vle16_v_i16m1(coef_block + 0 * DCTSIZE, vl);
  row1 = __riscv_vle16_v_i16m1(coef_block + 1 * DCTSIZE, vl);
  row2 = __riscv_vle16_v_i16m1(coef_block + 2 * DCTSIZE, vl);
  row3 = __riscv_vle16_v_i16m1(coef_block + 3 * DCTSIZE, vl);
  row4 = __riscv_vle16_v_i16m1(coef_block + 4 * DCTSIZE, vl);
  row5 = __riscv_vle16_v_i16m1(coef_block + 5 * DCTSIZE, vl);
  row6 = __riscv_vle16_v_i16m1(coef_block + 6 * DCTSIZE, vl);
  row7 = __riscv_vle16_v_i16m1(coef_block + 7 * DCTSIZE, vl);

  /* Load quantization table. */
  quant0 = __riscv_vle16_v_i16m1(quantptr + 0 * DCTSIZE, vl);
  quant1 = __riscv_vle16_v_i16m1(quantptr + 1 * DCTSIZE, vl);
  quant2 = __riscv_vle16_v_i16m1(quantptr + 2 * DCTSIZE, vl);
  quant3 = __riscv_vle16_v_i16m1(quantptr + 3 * DCTSIZE, vl);
  quant4 = __riscv_vle16_v_i16m1(quantptr + 4 * DCTSIZE, vl);
  quant5 = __riscv_vle16_v_i16m1(quantptr + 5 * DCTSIZE, vl);
  quant6 = __riscv_vle16_v_i16m1(quantptr + 6 * DCTSIZE, vl);
  quant7 = __riscv_vle16_v_i16m1(quantptr + 7 * DCTSIZE, vl);

  row0 = __riscv_vmul_vv_i16m1(row0, quant0, vl);
  row1 = __riscv_vmul_vv_i16m1(row1, quant1, vl);
  row2 = __riscv_vmul_vv_i16m1(row2, quant2, vl);
  row3 = __riscv_vmul_vv_i16m1(row3, quant3, vl);
  row4 = __riscv_vmul_vv_i16m1(row4, quant4, vl);
  row5 = __riscv_vmul_vv_i16m1(row5, quant5, vl);
  row6 = __riscv_vmul_vv_i16m1(row6, quant6, vl);
  row7 = __riscv_vmul_vv_i16m1(row7, quant7, vl);

  DO_IDCT(row);

  out0 = __riscv_vadd_vv_i16m1(tmp0, tmp7, vl);
  out7 = __riscv_vsub_vv_i16m1(tmp0, tmp7, vl);
  out1 = __riscv_vadd_vv_i16m1(tmp1, tmp6, vl);
  out6 = __riscv_vsub_vv_i16m1(tmp1, tmp6, vl);
  out2 = __riscv_vadd_vv_i16m1(tmp2, tmp5, vl);
  out5 = __riscv_vsub_vv_i16m1(tmp2, tmp5, vl);
  out4 = __riscv_vadd_vv_i16m1(tmp3, tmp4, vl);
  out3 = __riscv_vsub_vv_i16m1(tmp3, tmp4, vl);

  /* Store row vectors. */
  __riscv_vse16_v_i16m1(workspace + 0 * DCTSIZE, out0, vl);
  __riscv_vse16_v_i16m1(workspace + 1 * DCTSIZE, out1, vl);
  __riscv_vse16_v_i16m1(workspace + 2 * DCTSIZE, out2, vl);
  __riscv_vse16_v_i16m1(workspace + 3 * DCTSIZE, out3, vl);
  __riscv_vse16_v_i16m1(workspace + 4 * DCTSIZE, out4, vl);
  __riscv_vse16_v_i16m1(workspace + 5 * DCTSIZE, out5, vl);
  __riscv_vse16_v_i16m1(workspace + 6 * DCTSIZE, out6, vl);
  __riscv_vse16_v_i16m1(workspace + 7 * DCTSIZE, out7, vl);

  /* Pass 2: process rows from work array, store into output array. */

  /* Load column vectors.  We would normally load the row vectors directly and
   * transpose them, to avoid non-contiguous memory accesses, but using a
   * strided load is currently faster with RVV than an in-register matrix
   * transpose.
   */
  col0 = __riscv_vlse16_v_i16m1(workspace + 0, col_stride, vl);
  col1 = __riscv_vlse16_v_i16m1(workspace + 1, col_stride, vl);
  col2 = __riscv_vlse16_v_i16m1(workspace + 2, col_stride, vl);
  col3 = __riscv_vlse16_v_i16m1(workspace + 3, col_stride, vl);
  col4 = __riscv_vlse16_v_i16m1(workspace + 4, col_stride, vl);
  col5 = __riscv_vlse16_v_i16m1(workspace + 5, col_stride, vl);
  col6 = __riscv_vlse16_v_i16m1(workspace + 6, col_stride, vl);
  col7 = __riscv_vlse16_v_i16m1(workspace + 7, col_stride, vl);

  DO_IDCT(col);

  out0 = __riscv_vadd_vv_i16m1(tmp0, tmp7, vl);
  out7 = __riscv_vsub_vv_i16m1(tmp0, tmp7, vl);
  out1 = __riscv_vadd_vv_i16m1(tmp1, tmp6, vl);
  out6 = __riscv_vsub_vv_i16m1(tmp1, tmp6, vl);
  out2 = __riscv_vadd_vv_i16m1(tmp2, tmp5, vl);
  out5 = __riscv_vsub_vv_i16m1(tmp2, tmp5, vl);
  out4 = __riscv_vadd_vv_i16m1(tmp3, tmp4, vl);
  out3 = __riscv_vsub_vv_i16m1(tmp3, tmp4, vl);

  /* Store column vectors. */
  __riscv_vsse16_v_i16m1(workspace + 0, col_stride, out0, vl);
  __riscv_vsse16_v_i16m1(workspace + 1, col_stride, out1, vl);
  __riscv_vsse16_v_i16m1(workspace + 2, col_stride, out2, vl);
  __riscv_vsse16_v_i16m1(workspace + 3, col_stride, out3, vl);
  __riscv_vsse16_v_i16m1(workspace + 4, col_stride, out4, vl);
  __riscv_vsse16_v_i16m1(workspace + 5, col_stride, out5, vl);
  __riscv_vsse16_v_i16m1(workspace + 6, col_stride, out6, vl);
  __riscv_vsse16_v_i16m1(workspace + 7, col_stride, out7, vl);

  out0 = __riscv_vle16_v_i16m1(workspace + 0 * DCTSIZE, vl);
  out1 = __riscv_vle16_v_i16m1(workspace + 1 * DCTSIZE, vl);
  out2 = __riscv_vle16_v_i16m1(workspace + 2 * DCTSIZE, vl);
  out3 = __riscv_vle16_v_i16m1(workspace + 3 * DCTSIZE, vl);
  out4 = __riscv_vle16_v_i16m1(workspace + 4 * DCTSIZE, vl);
  out5 = __riscv_vle16_v_i16m1(workspace + 5 * DCTSIZE, vl);
  out6 = __riscv_vle16_v_i16m1(workspace + 6 * DCTSIZE, vl);
  out7 = __riscv_vle16_v_i16m1(workspace + 7 * DCTSIZE, vl);

  out0 = __riscv_vsra_vx_i16m1(out0, PASS1_BITS + 3, vl);
  out1 = __riscv_vsra_vx_i16m1(out1, PASS1_BITS + 3, vl);
  out2 = __riscv_vsra_vx_i16m1(out2, PASS1_BITS + 3, vl);
  out3 = __riscv_vsra_vx_i16m1(out3, PASS1_BITS + 3, vl);
  out4 = __riscv_vsra_vx_i16m1(out4, PASS1_BITS + 3, vl);
  out5 = __riscv_vsra_vx_i16m1(out5, PASS1_BITS + 3, vl);
  out6 = __riscv_vsra_vx_i16m1(out6, PASS1_BITS + 3, vl);
  out7 = __riscv_vsra_vx_i16m1(out7, PASS1_BITS + 3, vl);

  out0 = __riscv_vadd_vx_i16m1(out0, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out0, 0, vl);
  out0 = __riscv_vmerge_vxm_i16m1(out0, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out0, MAXJSAMPLE, vl);
  out0 = __riscv_vmerge_vxm_i16m1(out0, MAXJSAMPLE, mask, vl);

  out1 = __riscv_vadd_vx_i16m1(out1, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out1, 0, vl);
  out1 = __riscv_vmerge_vxm_i16m1(out1, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out1, MAXJSAMPLE, vl);
  out1 = __riscv_vmerge_vxm_i16m1(out1, MAXJSAMPLE, mask, vl);

  out2 = __riscv_vadd_vx_i16m1(out2, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out2, 0, vl);
  out2 = __riscv_vmerge_vxm_i16m1(out2, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out2, MAXJSAMPLE, vl);
  out2 = __riscv_vmerge_vxm_i16m1(out2, MAXJSAMPLE, mask, vl);

  out3 = __riscv_vadd_vx_i16m1(out3, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out3, 0, vl);
  out3 = __riscv_vmerge_vxm_i16m1(out3, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out3, MAXJSAMPLE, vl);
  out3 = __riscv_vmerge_vxm_i16m1(out3, MAXJSAMPLE, mask, vl);

  out4 = __riscv_vadd_vx_i16m1(out4, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out4, 0, vl);
  out4 = __riscv_vmerge_vxm_i16m1(out4, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out4, MAXJSAMPLE, vl);
  out4 = __riscv_vmerge_vxm_i16m1(out4, MAXJSAMPLE, mask, vl);

  out5 = __riscv_vadd_vx_i16m1(out5, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out5, 0, vl);
  out5 = __riscv_vmerge_vxm_i16m1(out5, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out5, MAXJSAMPLE, vl);
  out5 = __riscv_vmerge_vxm_i16m1(out5, MAXJSAMPLE, mask, vl);

  out6 = __riscv_vadd_vx_i16m1(out6, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out6, 0, vl);
  out6 = __riscv_vmerge_vxm_i16m1(out6, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out6, MAXJSAMPLE, vl);
  out6 = __riscv_vmerge_vxm_i16m1(out6, MAXJSAMPLE, mask, vl);

  out7 = __riscv_vadd_vx_i16m1(out7, CENTERJSAMPLE, vl);
  /* Range limit */
  mask = __riscv_vmslt_vx_i16m1_b16(out7, 0, vl);
  out7 = __riscv_vmerge_vxm_i16m1(out7, 0, mask, vl);
  mask = __riscv_vmsgt_vx_i16m1_b16(out7, MAXJSAMPLE, vl);
  out7 = __riscv_vmerge_vxm_i16m1(out7, MAXJSAMPLE, mask, vl);

  dst0 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out0, 0, vl));
  dst1 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out1, 0, vl));
  dst2 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out2, 0, vl));
  dst3 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out3, 0, vl));
  dst4 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out4, 0, vl));
  dst5 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out5, 0, vl));
  dst6 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out6, 0, vl));
  dst7 =
    __riscv_vreinterpret_v_i8mf2_u8mf2(__riscv_vnsra_wx_i8mf2(out7, 0, vl));

  __riscv_vse8_v_u8mf2(output_buf[0] + output_col, dst0, vl);
  __riscv_vse8_v_u8mf2(output_buf[1] + output_col, dst1, vl);
  __riscv_vse8_v_u8mf2(output_buf[2] + output_col, dst2, vl);
  __riscv_vse8_v_u8mf2(output_buf[3] + output_col, dst3, vl);
  __riscv_vse8_v_u8mf2(output_buf[4] + output_col, dst4, vl);
  __riscv_vse8_v_u8mf2(output_buf[5] + output_col, dst5, vl);
  __riscv_vse8_v_u8mf2(output_buf[6] + output_col, dst6, vl);
  __riscv_vse8_v_u8mf2(output_buf[7] + output_col, dst7, vl);
}
