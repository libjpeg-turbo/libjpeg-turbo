/*
 * jquanti-rvv.c - sample data conversion and quantization
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
/* INTEGER QUANTIZATION AND SAMPLE CONVERSION */
#include "jsimd_rvv.h"


void jsimd_convsamp_rvv(JSAMPARRAY sample_data, JDIMENSION start_col,
                        DCTELEM *workspace)
{
    vuint8m1_t in0, in1, in2, in3, in4, in5, in6, in7;
    vuint16m2_t row0, row1, row2, row3, row4, row5, row6, row7;
    vint16m2_t out0, out1, out2, out3, out4, out5, out6, out7;
    size_t vl = vsetvl_e16m2(DCTSIZE);

    in0 = vle8_v_u8m1(sample_data[0] + start_col, vl);
    in1 = vle8_v_u8m1(sample_data[1] + start_col, vl);
    in2 = vle8_v_u8m1(sample_data[2] + start_col, vl);
    in3 = vle8_v_u8m1(sample_data[3] + start_col, vl);
    in4 = vle8_v_u8m1(sample_data[4] + start_col, vl);
    in5 = vle8_v_u8m1(sample_data[5] + start_col, vl);
    in6 = vle8_v_u8m1(sample_data[6] + start_col, vl);
    in7 = vle8_v_u8m1(sample_data[7] + start_col, vl);

    row0 = vwaddu_vx_u16m2(in0, 0, vl);
    row1 = vwaddu_vx_u16m2(in1, 0, vl);
    row2 = vwaddu_vx_u16m2(in2, 0, vl);
    row3 = vwaddu_vx_u16m2(in3, 0, vl);
    row4 = vwaddu_vx_u16m2(in4, 0, vl);
    row5 = vwaddu_vx_u16m2(in5, 0, vl);
    row6 = vwaddu_vx_u16m2(in6, 0, vl);
    row7 = vwaddu_vx_u16m2(in7, 0, vl);

    out0 = vreinterpret_v_u16m2_i16m2(row0);
    out1 = vreinterpret_v_u16m2_i16m2(row1);
    out2 = vreinterpret_v_u16m2_i16m2(row2);
    out3 = vreinterpret_v_u16m2_i16m2(row3);
    out4 = vreinterpret_v_u16m2_i16m2(row4);
    out5 = vreinterpret_v_u16m2_i16m2(row5);
    out6 = vreinterpret_v_u16m2_i16m2(row6);
    out7 = vreinterpret_v_u16m2_i16m2(row7);

    out0 = vsub_vx_i16m2(out0, CENTERJSAMPLE, vl);
    out1 = vsub_vx_i16m2(out1, CENTERJSAMPLE, vl);
    out2 = vsub_vx_i16m2(out2, CENTERJSAMPLE, vl);
    out3 = vsub_vx_i16m2(out3, CENTERJSAMPLE, vl);
    out4 = vsub_vx_i16m2(out4, CENTERJSAMPLE, vl);
    out5 = vsub_vx_i16m2(out5, CENTERJSAMPLE, vl);
    out6 = vsub_vx_i16m2(out6, CENTERJSAMPLE, vl);
    out7 = vsub_vx_i16m2(out7, CENTERJSAMPLE, vl);

    vse16_v_i16m2(workspace + 0 * DCTSIZE, out0, vl);
    vse16_v_i16m2(workspace + 1 * DCTSIZE, out1, vl);
    vse16_v_i16m2(workspace + 2 * DCTSIZE, out2, vl);
    vse16_v_i16m2(workspace + 3 * DCTSIZE, out3, vl);
    vse16_v_i16m2(workspace + 4 * DCTSIZE, out4, vl);
    vse16_v_i16m2(workspace + 5 * DCTSIZE, out5, vl);
    vse16_v_i16m2(workspace + 6 * DCTSIZE, out6, vl);
    vse16_v_i16m2(workspace + 7 * DCTSIZE, out7, vl);
}


void jsimd_quantize_rvv(JCOEFPTR coef_block, DCTELEM *divisors,
                        DCTELEM *workspace)
{
    JCOEFPTR out_ptr = coef_block;
    DCTELEM *in_ptr = workspace;
    UDCTELEM *recip_ptr = (UDCTELEM *)divisors;
    UDCTELEM *corr_ptr = (UDCTELEM *)divisors + DCTSIZE2;
    DCTELEM *shift_ptr = divisors + 3 * DCTSIZE2;
    int cols;

    vint16m4_t out, shift;
    vuint16m4_t temp, recip, corr, ushift;
    vuint32m8_t product;
    vbool4_t mask;
    size_t vl;

    for (cols = DCTSIZE2; cols > 0; cols -= vl,
         in_ptr += vl, out_ptr += vl, recip_ptr += vl, corr_ptr += vl, shift_ptr += vl)
    {
        vl = vsetvl_e16m4(cols);
        /* Load needed variables. */
        out = vle16_v_i16m4(in_ptr, vl);
        recip = vle16_v_u16m4(recip_ptr, vl);
        corr = vle16_v_u16m4(corr_ptr, vl);
        shift = vle16_v_i16m4(shift_ptr, vl);

        /* Mask set to 1 where elements are negative. */
        mask = vmslt_vx_i16m4_b4(out, 0, vl);
        out = vneg_v_i16m4_m(mask, out, out, vl);
        temp = vreinterpret_v_i16m4_u16m4(out);

        temp = vadd_vv_u16m4(temp, corr, vl);
        product = vwmulu_vv_u32m8(temp, recip, vl);
        shift = vadd_vx_i16m4(shift, sizeof(DCTELEM) * 8, vl);
        ushift = vreinterpret_v_i16m4_u16m4(shift);
        temp = vnsrl_wv_u16m4(product, ushift, vl);

        out = vreinterpret_v_u16m4_i16m4(temp);
        out = vneg_v_i16m4_m(mask, out, out, vl);
        vse16_v_i16m4(out_ptr, out, vl);
    }
}