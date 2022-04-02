/*
 * Risc-V vector extension optimizations for libjpeg-turbo.
 *
 * Copyright (C) 2020, Arm Limited.  All Rights Reserved.
 * Copyright (C) 2020, D. R. Commander.  All Rights Reserved.
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

/* This file is included by jdmerge-rvv.c. */

/* These routines combine simple (non-fancy, i.e. non-smooth) h2v1 or h2v2
 * chroma upsampling and YCbCr -> RGB color conversion into a single function.
 *
 * As with the standalone functions, YCbCr -> RGB conversion is defined by the
 * following equations:
 *    R = Y                        + 1.40200 * (Cr - 128)
 *    G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)
 *    B = Y + 1.77200 * (Cb - 128)
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.3441467 = 11277 * 2^-15
 *    0.7141418 = 23401 * 2^-15
 *    1.4020386 = 22971 * 2^-14
 *    1.7720337 = 29033 * 2^-14
 * These constants are defined in jdmerge-neon.c.
 *
 * To ensure correct results, rounding is used when descaling.
 */


void jsimd_h2v1_merged_upsample_rvv(JDIMENSION output_width,
                                    JSAMPIMAGE input_buf,
                                    JDIMENSION in_row_group_ctr,
                                    JSAMPARRAY output_buf)
{
    JSAMPROW outptr;
    /* Pointers to Y, Cb, and Cr data */
    JSAMPROW inptr0, inptr1, inptr2;
    int pitch = output_width * RGB_PIXELSIZE, width, yloop;
#if BITS_IN_JSAMPLE == 8
    vuint8m1_t src;
    vint8m1_t dest;
#endif
    size_t vl, vl_odd, cols;
    vl = vsetvl_e16m2((output_width + 1) / 2);
    ptrdiff_t bstride;
    vuint16m2_t y0, y1, cb, cr;
    vint32m4_t tmp0;                                    /* '32' stands for '32-bit' here */
    vint16m2_t sy0, sy1, scb, scr,                      /* 's' stands for 'signed' here */
               r_sub_y, g_sub_y, b_sub_y, 
               r, g, b;          
    vbool8_t mask;

    /* Constants. */
#if RGB_PIXELSIZE == 4
#if BITS_IN_JSAMPLE == 8
    uint8_t alpha[1] = { 0xFF };
    vuint8m1_t alpha_v = vlse8_v_u8m1(alpha, 0, vl);
#else           /* BITS_IN_JSAMPLE == 12 */
    uint16_t alpha[1] = { 0xFF };
    vuint16m2_t alpha_v = vlse16_v_u16m2(alpha, 0, vl);
#endif
#endif

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    for (width = output_width; width > 0; 
         inptr0 += 2 * vl, inptr1 += vl, inptr2 += vl,
         outptr += cols, width -= (vl + vl_odd)) {
        /* Set vl for each iteration. */
        vl = vsetvl_e16m2((width + 1) >> 1);
        vl_odd = ((vl == (width + 1) >> 1) && (width & 1))? (vl - 1): vl;
        cols = (vl + vl_odd) * RGB_PIXELSIZE;
        bstride = RGB_PIXELSIZE * sizeof(JSAMPLE);

        /* Load R, G, B channels as vectors from inptr. */
#if BITS_IN_JSAMPLE == 8
        /* Extending to vuint16m4_t type for following multiply calculation. */
        /* Y component values with even-numbered indices. */
        src = vlse8_v_u8m1(inptr0, 2 * sizeof(JSAMPLE), vl);
        y0 = vwaddu_vx_u16m2(src, 0, vl);       /* Widening to vuint16m4_t type */
        /* Y component values with odd-numbered indices. */
        src = vlse8_v_u8m1(inptr0 + 1, 2 * sizeof(JSAMPLE), vl_odd);
        y1 = vwaddu_vx_u16m2(src, 0, vl_odd);   /* Widening to vuint16m4_t type */
        src = vle8_v_u8m1(inptr1, vl);
        cb = vwaddu_vx_u16m2(src, 0, vl);       /* Widening to vuint16m4_t type */
        src = vle8_v_u8m1(inptr2, vl);
        cr = vwaddu_vx_u16m2(src, 0, vl);       /* Widening to vuint16m4_t type */
#else                                     /* BITS_IN_JSAMPLE == 12 */
        y0 = vlse16_v_u16m2(inptr0, 2 * sizeof(JSAMPLE), vl);
        y1 = vlse16_v_u16m2(inptr0 + 1, 2 * sizeof(JSAMPLE), vl_odd);
        cb = vle16_v_u16m2(inptr1, vl);
        cr = vle16_v_u16m2(inptr2, vl);
#endif

        /* (Original)
         * R = Y                + 1.40200 * (Cr - CENTERJSAMPLE)
         * G = Y - 0.34414 * (Cb - CENTERJSAMPLE) - 0.71414 * (Cr - CENTERJSAMPLE)
         * B = Y + 1.77200 * (Cb - CENTERJSAMPLE)
         *
         * (This implementation)
         * R = Y                + 0.40200 * (Cr - CENTERJSAMPLE) + (Cr - CENTERJSAMPLE)
         * G = Y - 0.34414 * (Cb - CENTERJSAMPLE) + 0.28586 * (Cr - CENTERJSAMPLE) - (Cr - CENTERSAMPLE)
         * B = Y - 0.22800 * (Cb - CENTERJSAMPLE) + (Cb - CENTERJSAMPLE) + (Cb - CENTERJSAMPLE)
         * (Because 16-bit can only represent values < 1.)
         */
        sy0 = vreinterpret_v_u16m2_i16m2(y0);
        sy1 = vreinterpret_v_u16m2_i16m2(y1);
        scb = vreinterpret_v_u16m2_i16m2(cb);
        scr = vreinterpret_v_u16m2_i16m2(cr);

        scb = vsub_vx_i16m2(scb, CENTERJSAMPLE, vl);      /* Cb - CENTERJSAMPLE */
        scr = vsub_vx_i16m2(scr, CENTERJSAMPLE, vl);      /* Cr - CENTERJSAMPLE */

        /* Calculate R-Y values */
        tmp0 = vwmul_vx_i32m4(scr, F_0_402, vl);
        tmp0 = vadd_vx_i32m4(tmp0, ONE_HALF, vl);               /* Proper rounding. */
        r_sub_y = vnsra_wx_i16m2(tmp0, SCALEBITS, vl);
        r_sub_y = vadd_vv_i16m2(r_sub_y, scr, vl);

        /* Calculate G-Y values */
        tmp0 = vwmul_vx_i32m4(scb, -F_0_344, vl);
        tmp0 = vwmacc_vx_i32m4(tmp0, F_0_285, scr, vl);
        tmp0 = vadd_vx_i32m4(tmp0, ONE_HALF, vl);               /* Proper rounding. */
        g_sub_y = vnsra_wx_i16m2(tmp0, SCALEBITS, vl);
        g_sub_y = vsub_vv_i16m2(g_sub_y, scr, vl);

        /* Calculate B-Y values */
        tmp0 = vwmul_vx_i32m4(scb, -F_0_228, vl);
        tmp0 = vadd_vx_i32m4(tmp0, ONE_HALF, vl);               /* Proper rounding. */
        b_sub_y = vnsra_wx_i16m2(tmp0, SCALEBITS, vl);
        b_sub_y = vadd_vv_i16m2(b_sub_y, scb, vl);
        b_sub_y = vadd_vv_i16m2(b_sub_y, scb, vl);

        /* Compute R, G, B values with even-numbered indices. */
        r = vadd_vv_i16m2(r_sub_y, sy0, vl);
        /* Range limit */
        mask = vmslt_vx_i16m2_b8(r, 0, vl);
        r = vmerge_vxm_i16m2(mask, r, 0, vl);
        mask = vmsgt_vx_i16m2_b8(r, MAXJSAMPLE, vl);
        r = vmerge_vxm_i16m2(mask, r, MAXJSAMPLE, vl);

        g = vadd_vv_i16m2(g_sub_y, sy0, vl);
        /* Range limit */
        mask = vmslt_vx_i16m2_b8(g, 0, vl);
        g = vmerge_vxm_i16m2(mask, g, 0, vl);
        mask = vmsgt_vx_i16m2_b8(g, MAXJSAMPLE, vl);
        g = vmerge_vxm_i16m2(mask, g, MAXJSAMPLE, vl);

        b = vadd_vv_i16m2(b_sub_y, sy0, vl);
        /* Range limit */
        mask = vmslt_vx_i16m2_b8(b, 0, vl);
        b = vmerge_vxm_i16m2(mask, b, 0, vl);
        mask = vmsgt_vx_i16m2_b8(b, MAXJSAMPLE, vl);
        b = vmerge_vxm_i16m2(mask, b, MAXJSAMPLE, vl);
        /* Narrow to 8-bit and store to memory. */
#if BITS_IN_JSAMPLE == 8
        dest = vnsra_wx_i8m1(r, 0, vl);     /* Narrowing from 16-bit to 8-bit. */
        vsse8_v_i8m1(outptr + RGB_RED, 2 * bstride, dest, vl);
        dest = vnsra_wx_i8m1(g, 0, vl);     /* Narrowing from 16-bit to 8-bit. */
        vsse8_v_i8m1(outptr + RGB_GREEN, 2 * bstride, dest, vl);
        dest = vnsra_wx_i8m1(b, 0, vl);     /* Narrowing from 16-bit to 8-bit. */
        vsse8_v_i8m1(outptr + RGB_BLUE, 2 * bstride, dest, vl);
#else   /* BITS_IN_JSAMPLE == 12 */
        vsse16_v_u16m2(outptr + RGB_RED, 2 * bstride, r, vl);
        vsse16_v_u16m2(outptr + RGB_GREEN, 2 * bstride, g, vl);
        vsse16_v_u16m2(outptr + RGB_BLUE, 2 * bstride, b, vl);
#endif
        /* Deal with alpha channel. */
#if RGB_PIXELSIZE == 4
#if BITS_IN_JSAMPLE == 8
        vsse8_v_u8m1(outptr + RGB_ALPHA, 2 * bstride, alpha_v, vl);
#else           /* BITS_IN_JSAMPLE == 12 */
        vsse16_v_u16m2(outptr + RGB_ALPHA, 2 * bstride, alpha_v, vl);
#endif
#endif

        /* Compute R, G, B values with odd-numbered indices. */
        r = vadd_vv_i16m2(r_sub_y, sy1, vl_odd);
        /* Range limit */
        mask = vmslt_vx_i16m2_b8(r, 0, vl_odd);
        r = vmerge_vxm_i16m2(mask, r, 0, vl_odd);
        mask = vmsgt_vx_i16m2_b8(r, MAXJSAMPLE, vl_odd);
        r = vmerge_vxm_i16m2(mask, r, MAXJSAMPLE, vl_odd);

        g = vadd_vv_i16m2(g_sub_y, sy1, vl_odd);
        /* Range limit */
        mask = vmslt_vx_i16m2_b8(g, 0, vl_odd);
        g = vmerge_vxm_i16m2(mask, g, 0, vl_odd);
        mask = vmsgt_vx_i16m2_b8(g, MAXJSAMPLE, vl_odd);
        g = vmerge_vxm_i16m2(mask, g, MAXJSAMPLE, vl_odd);

        b = vadd_vv_i16m2(b_sub_y, sy1, vl_odd);
        /* Range limit */
        mask = vmslt_vx_i16m2_b8(b, 0, vl_odd);
        b = vmerge_vxm_i16m2(mask, b, 0, vl_odd);
        mask = vmsgt_vx_i16m2_b8(b, MAXJSAMPLE, vl_odd);
        b = vmerge_vxm_i16m2(mask, b, MAXJSAMPLE, vl_odd);
        /* Narrow to 8-bit and store to memory. */
#if BITS_IN_JSAMPLE == 8
        dest = vnsra_wx_i8m1(r, 0, vl_odd);     /* Narrowing from 16-bit to 8-bit. */
        vsse8_v_i8m1(outptr + RGB_PIXELSIZE + RGB_RED, 2 * bstride, dest, vl_odd);
        dest = vnsra_wx_i8m1(g, 0, vl_odd);     /* Narrowing from 16-bit to 8-bit. */
        vsse8_v_i8m1(outptr + RGB_PIXELSIZE + RGB_GREEN, 2 * bstride, dest, vl_odd);
        dest = vnsra_wx_i8m1(b, 0, vl_odd);     /* Narrowing from 16-bit to 8-bit. */
        vsse8_v_i8m1(outptr + RGB_PIXELSIZE + RGB_BLUE, 2 * bstride, dest, vl_odd);
#else   /* BITS_IN_JSAMPLE == 12 */
        vsse16_v_i16m2(outptr + RGB_PIXELSIZE + RGB_RED, 2 * bstride, r, vl_odd);
        vsse16_v_i16m2(outptr + RGB_PIXELSIZE + RGB_GREEN, 2 * bstride, g, vl_odd);
        vsse16_v_i16m2(outptr + RGB_PIXELSIZE + RGB_BLUE, 2 * bstride, b, vl_odd);
#endif
        /* Deal with alpha channel. */
#if RGB_PIXELSIZE == 4
#if BITS_IN_JSAMPLE == 8
        vsse8_v_u8m1(outptr + RGB_PIXELSIZE + RGB_ALPHA, 2 * bstride, alpha_v, vl_odd);
#else           /* BITS_IN_JSAMPLE == 12 */
        vsse16_v_u16m2(outptr + RGB_PIXELSIZE + RGB_ALPHA, 2 * bstride, alpha_v, vl_odd);
#endif
#endif
    }
}


void jsimd_h2v2_merged_upsample_rvv(JDIMENSION output_width,
                                    JSAMPIMAGE input_buf,
                                    JDIMENSION in_row_group_ctr,
                                    JSAMPARRAY output_buf)
{
    JSAMPROW inptr, outptr;

    inptr = input_buf[0][in_row_group_ctr];
    outptr = output_buf[0];

    input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2];
    jsimd_h2v1_merged_upsample_rvv(output_width, input_buf, in_row_group_ctr,
                                   output_buf);

    input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2 + 1];
    output_buf[0] = output_buf[1];
    jsimd_h2v1_merged_upsample_rvv(output_width, input_buf, in_row_group_ctr,
                                   output_buf);

    input_buf[0][in_row_group_ctr] = inptr;
    output_buf[0] = outptr;
}