/*
 * jidctfst-rvv.c - fast integer IDCT
 *
 * Copyright (C) 2020, Arm Limited.  All Rights Reserved.
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
#include "jsimd_rvv.h"


#define FIX_1_082  277              /* FIX(1.082392200) */
#define FIX_1_414  362              /* FIX(1.414213562) */
#define FIX_1_847  473              /* FIX(1.847759065) */
#define FIX_2_613  669              /* FIX(2.613125930) */

#if BITS_IN_JSAMPLE == 8
#define CONST_BITS  8
#define PASS1_BITS  2
#else
#define CONST_BITS  8
#define PASS1_BITS  1           /* lose a little precision to avoid overflow */
#endif


#define DO_IDCT(in) { \
    /* Even part */ \
    tmp10 = vadd_vv_i16m1(in##0, in##4, vl); \
    tmp11 = vsub_vv_i16m1(in##0, in##4, vl); \
    tmp13 = vadd_vv_i16m1(in##2, in##6, vl); \
    \
    tmp12 = vsub_vv_i16m1(in##2, in##6, vl); \
    pdt = vwmul_vx_i32m2(tmp12, FIX_1_414, vl); \
    tmp12 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    tmp12 = vsub_vv_i16m1(tmp12, tmp13, vl); \
    \
    tmp0 = vadd_vv_i16m1(tmp10, tmp13, vl); \
    tmp3 = vsub_vv_i16m1(tmp10, tmp13, vl); \
    tmp1 = vadd_vv_i16m1(tmp11, tmp12, vl); \
    tmp2 = vsub_vv_i16m1(tmp11, tmp12, vl); \
    \
    /* Odd part */ \
    z13 = vadd_vv_i16m1(in##5, in##3, vl); \
    z10 = vsub_vv_i16m1(in##5, in##3, vl); \
    z11 = vadd_vv_i16m1(in##1, in##7, vl); \
    z12 = vsub_vv_i16m1(in##1, in##7, vl); \
    \
    tmp7 = vadd_vv_i16m1(z11, z13, vl); \
    tmp11 = vsub_vv_i16m1(z11, z13, vl); \
    pdt = vwmul_vx_i32m2(tmp11, FIX_1_414, vl); \
    tmp11 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    \
    z5 = vadd_vv_i16m1(z10, z12, vl); \
    pdt = vwmul_vx_i32m2(z5, FIX_1_847, vl); \
    z5 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    \
    pdt = vwmul_vx_i32m2(z12, FIX_1_082, vl); \
    tmp10 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    tmp10 = vsub_vv_i16m1(tmp10, z5, vl); \
    pdt = vwmul_vx_i32m2(z10, -FIX_2_613, vl); \
    tmp12 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    tmp12 = vadd_vv_i16m1(z5, tmp12, vl); \
    \
    tmp6 = vsub_vv_i16m1(tmp12, tmp7, vl); \
    tmp5 = vsub_vv_i16m1(tmp11, tmp6, vl); \
    tmp4 = vadd_vv_i16m1(tmp10, tmp5, vl); \
}


/* TODO: how many bits does a vector register has at least? */
void jsimd_idct_ifast_rvv(void *dct_table, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
    IFAST_MULT_TYPE *quantptr = dct_table;
    DCTELEM workspace[DCTSIZE2];

    vint8mf2_t dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
    vint16m1_t row0, row1, row2, row3, row4, row5, row6, row7,
               col0, col1, col2, col3, col4, col5, col6, col7,
               tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
               tmp10, tmp11, tmp12, tmp13,
               z1, z2, z3, z4, z5, z10, z11, z12, z13,
               out0, out1, out2, out3, out4, out5, out6, out7,
               quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7,
               bitmap;
    vint32m2_t pdt, wrc;
    vbool16_t mask;
    size_t vl = vsetvl_e16m1(DCTSIZE), 
           col_stride = DCTSIZE * sizeof(DCTELEM);

    /* Pass 1: process columns from input, store into work array. */
    /* Load DCT coefficients. */
    row0 = vle16_v_i16m1(coef_block + DCTSIZE * 0, vl);
    row1 = vle16_v_i16m1(coef_block + DCTSIZE * 1, vl);
    row2 = vle16_v_i16m1(coef_block + DCTSIZE * 2, vl);
    row3 = vle16_v_i16m1(coef_block + DCTSIZE * 3, vl);
    row4 = vle16_v_i16m1(coef_block + DCTSIZE * 4, vl);
    row5 = vle16_v_i16m1(coef_block + DCTSIZE * 5, vl);
    row6 = vle16_v_i16m1(coef_block + DCTSIZE * 6, vl);
    row7 = vle16_v_i16m1(coef_block + DCTSIZE * 7, vl);

    /* Load quantization table. */
    quant0 = vle16_v_i16m1(quantptr + 0 * DCTSIZE, vl);
    quant1 = vle16_v_i16m1(quantptr + 1 * DCTSIZE, vl);
    quant2 = vle16_v_i16m1(quantptr + 2 * DCTSIZE, vl);
    quant3 = vle16_v_i16m1(quantptr + 3 * DCTSIZE, vl);
    quant4 = vle16_v_i16m1(quantptr + 4 * DCTSIZE, vl);
    quant5 = vle16_v_i16m1(quantptr + 5 * DCTSIZE, vl);
    quant6 = vle16_v_i16m1(quantptr + 6 * DCTSIZE, vl);
    quant7 = vle16_v_i16m1(quantptr + 7 * DCTSIZE, vl);

    row0 = vmul_vv_i16m1(row0, quant0, vl);
    row1 = vmul_vv_i16m1(row1, quant1, vl);
    row2 = vmul_vv_i16m1(row2, quant2, vl);
    row3 = vmul_vv_i16m1(row3, quant3, vl);
    row4 = vmul_vv_i16m1(row4, quant4, vl);
    row5 = vmul_vv_i16m1(row5, quant5, vl);
    row6 = vmul_vv_i16m1(row6, quant6, vl);
    row7 = vmul_vv_i16m1(row7, quant7, vl);

    DO_IDCT(row);

    out0 = vadd_vv_i16m1(tmp0, tmp7, vl);
    out7 = vsub_vv_i16m1(tmp0, tmp7, vl);
    out1 = vadd_vv_i16m1(tmp1, tmp6, vl);
    out6 = vsub_vv_i16m1(tmp1, tmp6, vl);
    out2 = vadd_vv_i16m1(tmp2, tmp5, vl);
    out5 = vsub_vv_i16m1(tmp2, tmp5, vl);
    out4 = vadd_vv_i16m1(tmp3, tmp4, vl);
    out3 = vsub_vv_i16m1(tmp3, tmp4, vl);

    /* Store rows */
    vse16_v_i16m1(workspace + DCTSIZE * 0, out0, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 1, out1, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 2, out2, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 3, out3, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 4, out4, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 5, out5, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 6, out6, vl);
    vse16_v_i16m1(workspace + DCTSIZE * 7, out7, vl);


    /* Pass 2: process rows from work array, store into output array. */
    /* Load columns */
    col0 = vlse16_v_i16m1(workspace + 0, col_stride, vl);
    col1 = vlse16_v_i16m1(workspace + 1, col_stride, vl);
    col2 = vlse16_v_i16m1(workspace + 2, col_stride, vl);
    col3 = vlse16_v_i16m1(workspace + 3, col_stride, vl);
    col4 = vlse16_v_i16m1(workspace + 4, col_stride, vl);
    col5 = vlse16_v_i16m1(workspace + 5, col_stride, vl);
    col6 = vlse16_v_i16m1(workspace + 6, col_stride, vl);
    col7 = vlse16_v_i16m1(workspace + 7, col_stride, vl);

    DO_IDCT(col);

    out0 = vadd_vv_i16m1(tmp0, tmp7, vl);
    out7 = vsub_vv_i16m1(tmp0, tmp7, vl);
    out1 = vadd_vv_i16m1(tmp1, tmp6, vl);
    out6 = vsub_vv_i16m1(tmp1, tmp6, vl);
    out2 = vadd_vv_i16m1(tmp2, tmp5, vl);
    out5 = vsub_vv_i16m1(tmp2, tmp5, vl);
    out4 = vadd_vv_i16m1(tmp3, tmp4, vl);
    out3 = vsub_vv_i16m1(tmp3, tmp4, vl);

    /* Transpose matrix */
    /* Store columns */
    vsse16_v_i16m1(workspace + 0, col_stride, out0, vl);
    vsse16_v_i16m1(workspace + 1, col_stride, out1, vl);
    vsse16_v_i16m1(workspace + 2, col_stride, out2, vl);
    vsse16_v_i16m1(workspace + 3, col_stride, out3, vl);
    vsse16_v_i16m1(workspace + 4, col_stride, out4, vl);
    vsse16_v_i16m1(workspace + 5, col_stride, out5, vl);
    vsse16_v_i16m1(workspace + 6, col_stride, out6, vl);
    vsse16_v_i16m1(workspace + 7, col_stride, out7, vl);

    out0 = vle16_v_i16m1(workspace + DCTSIZE * 0, vl);
    out1 = vle16_v_i16m1(workspace + DCTSIZE * 1, vl);
    out2 = vle16_v_i16m1(workspace + DCTSIZE * 2, vl);
    out3 = vle16_v_i16m1(workspace + DCTSIZE * 3, vl);
    out4 = vle16_v_i16m1(workspace + DCTSIZE * 4, vl);
    out5 = vle16_v_i16m1(workspace + DCTSIZE * 5, vl);
    out6 = vle16_v_i16m1(workspace + DCTSIZE * 6, vl);
    out7 = vle16_v_i16m1(workspace + DCTSIZE * 7, vl);

    out0 = vsra_vx_i16m1(out0, PASS1_BITS + 3, vl);
    out1 = vsra_vx_i16m1(out1, PASS1_BITS + 3, vl);
    out2 = vsra_vx_i16m1(out2, PASS1_BITS + 3, vl);
    out3 = vsra_vx_i16m1(out3, PASS1_BITS + 3, vl);
    out4 = vsra_vx_i16m1(out4, PASS1_BITS + 3, vl);
    out5 = vsra_vx_i16m1(out5, PASS1_BITS + 3, vl);
    out6 = vsra_vx_i16m1(out6, PASS1_BITS + 3, vl);
    out7 = vsra_vx_i16m1(out7, PASS1_BITS + 3, vl);

    out0 = vadd_vx_i16m1(out0, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out0, 0, vl);
    out0 = vmerge_vxm_i16m1(mask, out0, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out0, MAXJSAMPLE, vl);
    out0 = vmerge_vxm_i16m1(mask, out0, MAXJSAMPLE, vl);
    
    out1 = vadd_vx_i16m1(out1, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out1, 0, vl);
    out1 = vmerge_vxm_i16m1(mask, out1, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out1, MAXJSAMPLE, vl);
    out1 = vmerge_vxm_i16m1(mask, out1, MAXJSAMPLE, vl);

    out2 = vadd_vx_i16m1(out2, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out2, 0, vl);
    out2 = vmerge_vxm_i16m1(mask, out2, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out2, MAXJSAMPLE, vl);
    out2 = vmerge_vxm_i16m1(mask, out2, MAXJSAMPLE, vl);

    out3 = vadd_vx_i16m1(out3, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out3, 0, vl);
    out3 = vmerge_vxm_i16m1(mask, out3, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out3, MAXJSAMPLE, vl);
    out3 = vmerge_vxm_i16m1(mask, out3, MAXJSAMPLE, vl);

    out4 = vadd_vx_i16m1(out4, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out4, 0, vl);
    out4 = vmerge_vxm_i16m1(mask, out4, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out4, MAXJSAMPLE, vl);
    out4 = vmerge_vxm_i16m1(mask, out4, MAXJSAMPLE, vl);

    out5 = vadd_vx_i16m1(out5, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out5, 0, vl);
    out5 = vmerge_vxm_i16m1(mask, out5, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out5, MAXJSAMPLE, vl);
    out5 = vmerge_vxm_i16m1(mask, out5, MAXJSAMPLE, vl);

    out6 = vadd_vx_i16m1(out6, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out6, 0, vl);
    out6 = vmerge_vxm_i16m1(mask, out6, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out6, MAXJSAMPLE, vl);
    out6 = vmerge_vxm_i16m1(mask, out6, MAXJSAMPLE, vl);

    out7 = vadd_vx_i16m1(out7, CENTERJSAMPLE, vl);
    /* Range limit */
    mask = vmslt_vx_i16m1_b16(out7, 0, vl);
    out7 = vmerge_vxm_i16m1(mask, out7, 0, vl);
    mask = vmsgt_vx_i16m1_b16(out7, MAXJSAMPLE, vl);
    out7 = vmerge_vxm_i16m1(mask, out7, MAXJSAMPLE, vl);

    dst0 = vnsra_wx_i8mf2(out0, 0, vl);
    dst1 = vnsra_wx_i8mf2(out1, 0, vl);
    dst2 = vnsra_wx_i8mf2(out2, 0, vl);
    dst3 = vnsra_wx_i8mf2(out3, 0, vl);
    dst4 = vnsra_wx_i8mf2(out4, 0, vl);
    dst5 = vnsra_wx_i8mf2(out5, 0, vl);
    dst6 = vnsra_wx_i8mf2(out6, 0, vl);
    dst7 = vnsra_wx_i8mf2(out7, 0, vl);

    vse8_v_i8mf2(output_buf[0] + output_col, dst0, vl);
    vse8_v_i8mf2(output_buf[1] + output_col, dst1, vl);
    vse8_v_i8mf2(output_buf[2] + output_col, dst2, vl);
    vse8_v_i8mf2(output_buf[3] + output_col, dst3, vl);
    vse8_v_i8mf2(output_buf[4] + output_col, dst4, vl);
    vse8_v_i8mf2(output_buf[5] + output_col, dst5, vl);
    vse8_v_i8mf2(output_buf[6] + output_col, dst6, vl);
    vse8_v_i8mf2(output_buf[7] + output_col, dst7, vl);

}
