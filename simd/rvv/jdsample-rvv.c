/*
 * Risc-V vector extension optimizations for libjpeg-turbo
 *
 * Copyright (C) 2015, 2018-2019, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2016-2017, Loongson Technology Corporation Limited, BeiJing.
 *                          All Rights Reserved.
 * Authors:  ZhuChen     <zhuchen@loongson.cn>
 *           CaiWanwei   <caiwanwei@loongson.cn>
 *           SunZhangzhi <sunzhangzhi-cq@loongson.cn>
 *
 * Based on the x86 SIMD extension for IJG JPEG library
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
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

/* CHROMA UPSAMPLING */

#include "jsimd_rvv.h"


void jsimd_h2v1_fancy_upsample_rvv(int max_v_samp_factor,
                                   JDIMENSION downsampled_width,
                                   JSAMPARRAY input_data,
                                   JSAMPARRAY *output_data_ptr)
{
    JSAMPARRAY output_data = *output_data_ptr;
    JSAMPROW inptr, outptr;
    int inrow, incol;
    size_t vl;

    vuint8m2_t this, shift, p_odd, p_even;
    /* '_u16' suffix denotes 16-bit unsigned int type. */
    vuint16m4_t p_odd_u16, p_even_u16;

    for (inrow = 0; inrow < max_v_samp_factor; inrow++)
    {
        inptr = input_data[inrow];
        outptr = output_data[inrow];
        /* First pixel component value in this row of the original image */
        *outptr++ = (JSAMPLE)GETJSAMPLE(*inptr);

        for (incol = downsampled_width - 1; incol > 0;
             incol -= vl, inptr += vl, outptr += 2 * vl)
        {
            vl = vsetvl_e16m4(incol);

            /* Load smaples and samples with offset 1. */
            this = vle8_v_u8m2(inptr, vl);
            shift = vle8_v_u8m2(inptr + 1, vl);

            /* p1(upsampled) = (3 * s0 + s1 + 2) / 4 */
            p_odd_u16 = vwmulu_vx_u16m4(this, 3, vl);
            p_odd_u16 = vwaddu_wv_u16m4(p_odd_u16, shift, vl);
            p_odd_u16 = vadd_vx_u16m4(p_odd_u16, 2, vl);        /* Add bias */
            /* p2(upsampled) = (3 * s1 + s0 + 1) / 4 */
            p_even_u16 = vwmulu_vx_u16m4(shift, 3, vl);
            p_even_u16 = vwaddu_wv_u16m4(p_even_u16, this, vl);
            p_even_u16 = vadd_vx_u16m4(p_even_u16, 1, vl);      /* Add bias */

            /* Right-shift by 2 (divide by 4) and narrow to 8-bit. */
            p_odd = vnsrl_wx_u8m2(p_odd_u16, 2, vl);
            p_even = vnsrl_wx_u8m2(p_even_u16, 2, vl);
            /* Strided store to memory. */
            vsse8_v_u8m2(outptr, 2 * sizeof(JSAMPLE), p_odd, vl);
            vsse8_v_u8m2(outptr + 1, 2 * sizeof(JSAMPLE), p_even, vl);
        }

        /* Last pixel component value in this row of the original image */
        *outptr = (JSAMPLE)GETJSAMPLE(*inptr);
    }
}


void jsimd_h2v2_fancy_upsample_rvv(int max_v_samp_factor,
                                   JDIMENSION downsampled_width,
                                   JSAMPARRAY input_data,
                                   JSAMPARRAY *output_data_ptr)
{
    JSAMPARRAY output_data = *output_data_ptr;
    JSAMPROW inptr_1, inptr0, inptr1, outptr0, outptr1;
    int inrow, outrow, incol;
    size_t vl;

    /* Suffixes 0 and 1 (after 'odd' or 'even') denote the upper and lower rows of
     * output pixels, respectively.
     */
    vuint8m1_t row_1, row0, row1, row_1rs1, row0rs1, row1rs1,
                p_odd0, p_even0, p_odd1, p_even1;
    /* '_u16' suffix denotes 16-bit unsigned int type, 'rs1' means right shift by 1. 
     * 'ctr' suffix denotes the center row for upsampling computing, and 'sd' suffix
     * denotes the row upper or lower than the center row for upsampling computing.
     */
    vuint16m2_t row_1_u16, row_1rs1_u16, row0_u16, row0rs1_u16, row1_u16, row1rs1_u16,
                s0_add_3s1_ctr, s1_add_3s0_ctr, s0_add_3s1_sd, s1_add_3s0_sd,
                p_odd0_u16, p_even0_u16, p_odd1_u16, p_even1_u16;

    for (inrow = 0, outrow = 0; outrow < max_v_samp_factor; inrow++)
    {
        inptr_1 = input_data[inrow - 1];
        inptr0 = input_data[inrow];
        inptr1 = input_data[inrow + 1];
        outptr0 = output_data[outrow++];
        outptr1 = output_data[outrow++];

        /* First pixel component value in this row of the original image */
        int s0colsum0 = GETJSAMPLE(*inptr0) * 3 + GETJSAMPLE(*inptr_1);
        *outptr0++ = (JSAMPLE)((s0colsum0 * 4 + 8) >> 4);
        int s0colsum1 = GETJSAMPLE(*inptr0) * 3 + GETJSAMPLE(*inptr1);
        *outptr1++ = (JSAMPLE)((s0colsum1 * 4 + 8) >> 4);

        for (incol = downsampled_width - 1; incol > 0;
             incol -= vl, inptr_1 += vl, inptr0 += vl, inptr1 += vl,
             outptr0 += 2 * vl, outptr1 += 2 * vl)
        {
            vl = vsetvl_e16m2(incol);

            /* Load smaples and samples with offset 1. */
            row_1 = vle8_v_u8m1(inptr_1, vl);
            row_1rs1 = vle8_v_u8m1(inptr_1 + 1, vl);
            row0 = vle8_v_u8m1(inptr0, vl);
            row0rs1 = vle8_v_u8m1(inptr0 + 1, vl);
            row1 = vle8_v_u8m1(inptr1, vl);
            row1rs1 = vle8_v_u8m1(inptr1 + 1, vl);
            /* Widen to vuint16m1_t type. */
            row_1_u16 = vwaddu_vx_u16m2(row_1, 0, vl);
            row_1rs1_u16 = vwaddu_vx_u16m2(row_1rs1, 0, vl);
            row0_u16 = vwaddu_vx_u16m2(row0, 0, vl);
            row0rs1_u16 = vwaddu_vx_u16m2(row0rs1, 0, vl);
            row1_u16 = vwaddu_vx_u16m2(row1, 0, vl);
            row1rs1_u16 = vwaddu_vx_u16m2(row1rs1, 0, vl);


            /* Compute pixels for output in row 0. */
            /* Step 1: Blend samples vertically in columns s0 and s1. */
            /* 3 * s0A + s1A (row -1) */
            s1_add_3s0_sd = vmul_vx_u16m2(row_1_u16, 3, vl);
            s1_add_3s0_sd = vadd_vv_u16m2(s1_add_3s0_sd, row_1rs1_u16, vl);
            /* 3 * s1A + s0A (row -1) */
            s0_add_3s1_sd = vmul_vx_u16m2(row_1rs1_u16, 3, vl);
            s0_add_3s1_sd = vadd_vv_u16m2(s0_add_3s1_sd, row_1_u16, vl);
            /* 3 * (3 * s0B + s1B) (row 0) */
            s1_add_3s0_ctr = vmul_vx_u16m2(row0_u16, 3, vl);
            s1_add_3s0_ctr = vadd_vv_u16m2(s1_add_3s0_ctr, row0rs1_u16, vl);
            s1_add_3s0_ctr = vmul_vx_u16m2(s1_add_3s0_ctr, 3, vl);
            /* 3 * (3 * s1B + s0B) (row 0) */
            s0_add_3s1_ctr = vmul_vx_u16m2(row0rs1_u16, 3, vl);
            s0_add_3s1_ctr = vadd_vv_u16m2(s0_add_3s1_ctr, row0_u16, vl);
            s0_add_3s1_ctr = vmul_vx_u16m2(s0_add_3s1_ctr, 3, vl);

            /* Step 2: Blend the already-blended columns. */
            /* p13: (3 * (3 * s0B + s1B) + (3 * s0A + s1A) + 7) / 16 */
            p_odd0_u16 = vadd_vv_u16m2(s1_add_3s0_ctr, s1_add_3s0_sd, vl);
            p_odd0_u16 = vadd_vx_u16m2(p_odd0_u16, 7, vl);          /* Add bias */
            /* p14: (3 * (3 * s1B + s0B) + (3 * s1A + s0A) + 8) / 16 */
            p_even0_u16 = vadd_vv_u16m2(s0_add_3s1_ctr, s0_add_3s1_sd, vl);
            p_even0_u16 = vadd_vx_u16m2(p_even0_u16, 8, vl);        /* Add bias */

            /* Right-shift by 4 (divide by 16), narrow to 8-bit, and combine. */
            p_odd0 = vnsrl_wx_u8m1(p_odd0_u16, 4, vl);
            p_even0 = vnsrl_wx_u8m1(p_even0_u16, 4, vl);
            /* Strided store to memory. */
            vsse8_v_u8m1(outptr0, 2 * sizeof(JSAMPLE), p_odd0, vl);
            vsse8_v_u8m1(outptr0 + 1, 2 * sizeof(JSAMPLE), p_even0, vl);


            /* Compute pixels for output in row 1. */
            /* Step 1: Blend samples vertically in columns s0 and s1. */
            /* 3 * s0C + s1C (row 1) */
            s1_add_3s0_sd = vmul_vx_u16m2(row1_u16, 3, vl);
            s1_add_3s0_sd = vadd_vv_u16m2(s1_add_3s0_sd, row1rs1_u16, vl);
            /* 3 * s1C + s0C (row 1) */
            s0_add_3s1_sd = vmul_vx_u16m2(row1rs1_u16, 3, vl);
            s0_add_3s1_sd = vadd_vv_u16m2(s0_add_3s1_sd, row1_u16, vl);

            /* Step 2: Blend the already-blended columns. */
            /* p19: (3 * (3 * s0B + s1B) + (3 * s0C + s1C) + 7) / 16 */
            p_odd1_u16 = vadd_vv_u16m2(s1_add_3s0_ctr, s1_add_3s0_sd, vl);
            p_odd1_u16 = vadd_vx_u16m2(p_odd1_u16, 7, vl);          /* Add bias */
            /* p20: (3 * (3 * s1B + s0B) + (3 * s1C + s0C) + 8) / 16 */
            p_even1_u16 = vadd_vv_u16m2(s0_add_3s1_ctr, s0_add_3s1_sd, vl);
            p_even1_u16 = vadd_vx_u16m2(p_even1_u16, 8, vl);        /* Add bias */

            /* Right-shift by 4 (divide by 16), narrow to 8-bit, and combine. */
            p_odd1 = vnsrl_wx_u8m1(p_odd1_u16, 4, vl);
            p_even1 = vnsrl_wx_u8m1(p_even1_u16, 4, vl);
            /* Strided store to memory. */
            vsse8_v_u8m1(outptr1, 2 * sizeof(JSAMPLE), p_odd1, vl);
            vsse8_v_u8m1(outptr1 + 1, 2 * sizeof(JSAMPLE), p_even1, vl);
        }

        /* Last pixel component value in this row of the original image */
        int s1colsum0 = GETJSAMPLE(*inptr0) * 3 + GETJSAMPLE(*inptr_1);
        *outptr0 = (JSAMPLE)((s1colsum0 * 4 + 7) >> 4);
        int s1colsum1 = GETJSAMPLE(*inptr0) * 3 + GETJSAMPLE(*inptr1);
        *outptr1 = (JSAMPLE)((s1colsum1 * 4 + 7) >> 4);
        
    }
}


/* These are rarely used (mainly just for decompressing YCCK images) */

void jsimd_h2v1_upsample_rvv(int max_v_samp_factor,
                             JDIMENSION output_width,
                             JSAMPARRAY input_data,
                             JSAMPARRAY *output_data_ptr)
{
    JSAMPARRAY output_data = *output_data_ptr;
    JSAMPROW inptr, outptr;
    int inrow, outcol;
    size_t vl;

    vuint8m8_t samples;

    for (inrow = 0; inrow < max_v_samp_factor; inrow++)
    {
        inptr = input_data[inrow];
        outptr = output_data[inrow];

        /* p0(upsampled) = p1(upsampled) = s0
         * p2(upsampled) = p3(upsampled) = s1 
         */
        for (outcol = output_width; outcol > 0;
             outcol -= 2 * vl, inptr += vl, outptr += 2 * vl)
        {
            vl = vsetvl_e8m8((outcol + 1) / 2);

            samples = vle8_v_u8m8(inptr, vl);

            vsse8_v_u8m8(outptr, 2 * sizeof(JSAMPLE), samples, vl);
            vsse8_v_u8m8(outptr + 1, 2 * sizeof(JSAMPLE), samples, vl);
        }

    }
}


void jsimd_h2v2_upsample_rvv(int max_v_samp_factor,
                             JDIMENSION output_width,
                             JSAMPARRAY input_data,
                             JSAMPARRAY *output_data_ptr)
{
    JSAMPARRAY output_data = *output_data_ptr;
    JSAMPROW inptr, outptr0, outptr1;
    int inrow, outrow, outcol;
    size_t vl;

    vuint8m8_t samples;

    for (inrow = 0, outrow = 0; outrow < max_v_samp_factor; inrow++)
    {
        inptr = input_data[inrow];
        outptr0 = output_data[outrow++];
        outptr1 = output_data[outrow++];

        /* p0(upsampled) = p1(upsampled) = p4(upsampled) = p5(upsampled) = s0A
         * p2(upsampled) = p3(upsampled) = p6(upsampled) = p7(upsampled) = s1A
         * p8(upsampled) = p9(upsampled) = p12(upsampled) = p13(upsampled) = s0B
         * p10(upsampled) = p11(upsampled) = p14(upsampled) = p15(upsampled) = s1B
         */
        for (outcol = output_width; outcol > 0;
             outcol -= 2 * vl, inptr += vl, outptr0 += 2 * vl, outptr1 += 2 * vl)
        {
            vl = vsetvl_e8m8((outcol + 1) / 2);

            samples = vle8_v_u8m8(inptr, vl);

            vsse8_v_u8m8(outptr0, 2 * sizeof(JSAMPLE), samples, vl);
            vsse8_v_u8m8(outptr0 + 1, 2 * sizeof(JSAMPLE), samples, vl);
            vsse8_v_u8m8(outptr1, 2 * sizeof(JSAMPLE), samples, vl);
            vsse8_v_u8m8(outptr1 + 1, 2 * sizeof(JSAMPLE), samples, vl);
        }
    }
}
