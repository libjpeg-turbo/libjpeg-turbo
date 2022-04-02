/*
 * Risc-V vector extension optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014-2015, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2014, Jay Foad.  All Rights Reserved.
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

/* This file is included by jccolor-rvv.c */


void jsimd_ycc_rgb_convert_rvv(JDIMENSION out_width, JSAMPIMAGE input_buf,
                               JDIMENSION input_row, JSAMPARRAY output_buf,
                               int num_rows)
{
    JSAMPROW outptr, inptr0, inptr1, inptr2;
    vuint16m4_t y, cb, cr;
    vint32m8_t tmp0, tmp1;                /* '32' stands for '32-bit' here */
    vint16m4_t sy, scb, scr, r, tmp;                 /* 's' stands for 'signed' here */
    vbool4_t mask;
#if BITS_IN_JSAMPLE == 8
    vuint8m2_t src;
    vint8m2_t dest;
#endif
    size_t pitch = out_width * RGB_PIXELSIZE, num_cols, 
           cols, i, vl;
    vl = vsetvl_e16m4(pitch);
    ptrdiff_t bstride;

    /* Constants. */
#if RGB_PIXELSIZE == 4
#if BITS_IN_JSAMPLE == 8
    uint8_t alpha[1] = { 0xFF };
    vuint8m2_t alpha_v = vlse8_v_u8m2(alpha, 0, vl);
#else           /* BITS_IN_JSAMPLE == 12 */
    uint16_t alpha[1] = { 0xFF };
    vuint16m4_t alpha_v = vlse16_v_u16m4(alpha, 0, vl);
#endif
#endif


    while (--num_rows >= 0)
    {
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;

        num_cols = pitch;
        while (num_cols > 0)
        {
            /* Set vl for each iteration. */
            vl = vsetvl_e16m4(num_cols / RGB_PIXELSIZE);
            cols = vl * RGB_PIXELSIZE;
            bstride = RGB_PIXELSIZE * sizeof(JSAMPLE);

            /* Load R, G, B channels as vectors from inptr. */
#if BITS_IN_JSAMPLE == 8
            /* Extending to vuint16m4_t type for following multiply calculation. */
            src = vle8_v_u8m2(inptr0, vl);
            y = vwaddu_vx_u16m4(src, 0, vl);                /* Widening to vuint16m4_t type */
            src = vle8_v_u8m2(inptr1, vl);
            cb = vwaddu_vx_u16m4(src, 0, vl);               /* Widening to vuint16m4_t type */
            src = vle8_v_u8m2(inptr2, vl);
            cr = vwaddu_vx_u16m4(src, 0, vl);               /* Widening to vuint16m4_t type */
#else   /* BITS_IN_JSAMPLE == 12 */
            y = vle16_v_u16m4(inptr0, vl);
            cb = vle16_v_u16m4(inptr1, vl);
            cr = vle16_v_u16m4(inptr2, vl);
#endif

            /* (Original)
             * R = Y                + 1.40200 * (Cr - CENTERJSAMPLE)
             * G = Y - 0.34414 * (Cb - CENTERJSAMPLE) - 0.71414 * (Cr - CENTERJSAMPLE)
             * B = Y + 1.77200 * (Cb - CENTERJSAMPLE)
             * 
             * (This implementation)
             * R = Y + (Cr - CENTERJSAMPLE) + 0.40200 * (Cr - CENTERJSAMPLE)
             * G = Y - 0.34414 * (Cb - CENTERJSAMPLE) + 0.28586 * (Cr - CENTERJSAMPLE) - (Cr - CENTERSAMPLE)
             * B = Y + (Cb - CENTERJSAMPLE) + (Cb - CENTERJSAMPLE) - 0.22800 * (Cb - CENTERJSAMPLE)
             */
            sy = vreinterpret_v_u16m4_i16m4(y);
            scb = vreinterpret_v_u16m4_i16m4(cb);
            scr = vreinterpret_v_u16m4_i16m4(cr);

            scb = vsub_vx_i16m4(scb, CENTERJSAMPLE, vl);      /* Cb - CENTERJSAMPLE */
            scr = vsub_vx_i16m4(scr, CENTERJSAMPLE, vl);      /* Cr - CENTERJSAMPLE */

            /* Calculate R values */
            r = vadd_vv_i16m4(sy, scr, vl);
            tmp0 = vwmul_vx_i32m8(scr, F_0_402, vl);
            tmp0 = vadd_vx_i32m8(tmp0, ONE_HALF, vl);               /* Proper rounding. */
            tmp = vnsra_wx_i16m4(tmp0, SCALEBITS, vl);
            r = vadd_vv_i16m4(r, tmp, vl);
            /* Range limit */
            mask = vmslt_vx_i16m4_b4(r, 0, vl);
            r = vmerge_vxm_i16m4(mask, r, 0, vl);
            mask = vmsgt_vx_i16m4_b4(r, MAXJSAMPLE, vl);
            r = vmerge_vxm_i16m4(mask, r, MAXJSAMPLE, vl);
            /* TODO: Figure out whether big-endian or little-endian would be different. */
#if BITS_IN_JSAMPLE == 8
            dest = vnsra_wx_i8m2(r, 0, vl);     /* Narrowing from 16-bit to 8-bit. */
            vsse8_v_i8m2(outptr + RGB_RED, bstride, dest, vl);
#else   /* BITS_IN_JSAMPLE == 12 */
            vsse16_v_i16m4(outptr + RGB_RED, bstride, r, vl);
#endif

            /* Calculate G values */
            tmp0 = vwmul_vx_i32m8(scb, -F_0_344, vl);
            tmp0 = vwmacc_vx_i32m8(tmp0, F_0_285, scr, vl);
            tmp0 = vadd_vx_i32m8(tmp0, ONE_HALF, vl);           /* Proper rounding. */
            r = vnsra_wx_i16m4(tmp0, SCALEBITS, vl);
            r = vadd_vv_i16m4(r, sy, vl);
            r = vsub_vv_i16m4(r, scr, vl);
            /* Range limit */
            mask = vmslt_vx_i16m4_b4(r, 0, vl);
            r = vmerge_vxm_i16m4(mask, r, 0, vl);
            mask = vmsgt_vx_i16m4_b4(r, MAXJSAMPLE, vl);
            r = vmerge_vxm_i16m4(mask, r, MAXJSAMPLE, vl);
            /* TODO: Figure out whether big-endian or little-endian would be different. */
#if BITS_IN_JSAMPLE == 8
            dest = vnsra_wx_i8m2(r, 0, vl);     /* Narrowing from 16-bit to 8-bit. */
            vsse8_v_i8m2(outptr + RGB_GREEN, bstride, dest, vl);
#else   /* BITS_IN_JSAMPLE == 12 */
            vsse16_v_i16m4(outptr + RGB_GREEN, bstride, r, vl);
#endif

            /* Calculate B values */
            r = vadd_vv_i16m4(sy, scb, vl);
            r = vadd_vv_i16m4(r, scb, vl);
            tmp0 = vwmul_vx_i32m8(scb, -F_0_228, vl);
            tmp0 = vadd_vx_i32m8(tmp0, ONE_HALF, vl);               /* Proper rounding. */
            tmp = vnsra_wx_i16m4(tmp0, SCALEBITS, vl);
            r = vadd_vv_i16m4(r, tmp, vl);
            /* Range limit */
            mask = vmslt_vx_i16m4_b4(r, 0, vl);
            r = vmerge_vxm_i16m4(mask, r, 0, vl);
            mask = vmsgt_vx_i16m4_b4(r, MAXJSAMPLE, vl);
            r = vmerge_vxm_i16m4(mask, r, MAXJSAMPLE, vl);
            /* TODO: Figure out whether big-endian or little-endian would be different. */
#if BITS_IN_JSAMPLE == 8
            dest = vnsra_wx_i8m2(r, 0, vl);     /* Narrowing from 16-bit to 8-bit. */
            vsse8_v_i8m2(outptr + RGB_BLUE, bstride, dest, vl);
#else   /* BITS_IN_JSAMPLE == 12 */
            vsse16_v_i16m4(outptr + RGB_BLUE, bstride, r, vl);
#endif

            /* Store alpha channel values. */
#if RGB_PIXELSIZE == 4
#if BITS_IN_JSAMPLE == 8
            vsse8_v_u8m2(outptr + RGB_ALPHA, bstride, alpha_v, vl);
#else           /* BITS_IN_JSAMPLE == 12 */
            vsse16_v_u16m4(outptr + RGB_ALPHA, bstride, alpha_v, vl);
#endif
#endif

            /* Move the pointer to the right place. */
            outptr += cols;
            inptr0 += vl;   inptr1 += vl;   inptr2 += vl;
            num_cols -= cols;
        }
    }
}