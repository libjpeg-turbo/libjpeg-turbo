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

/* TODO: Deal with 12-bit RGB colorspace. */
void jsimd_rgb_ycc_convert_rvv(JDIMENSION img_width, JSAMPARRAY input_buf,
                               JSAMPIMAGE output_buf,
                               JDIMENSION output_row, int num_rows)
{
    JSAMPROW inptr, outptr0, outptr1, outptr2;
    vuint16m4_t r, g, b, y;
    vuint32m8_t tmp0, tmp1;
#if BITS_IN_JSAMPLE == 8
    vuint8m2_t dest, src;
#endif
    size_t pitch = img_width * RGB_PIXELSIZE, num_cols, i;


    while (--num_rows >= 0)
    {
        inptr = *input_buf++;
        outptr0 = output_buf[0][output_row];
        outptr1 = output_buf[1][output_row];
        outptr2 = output_buf[2][output_row];
        output_row++;


        num_cols = pitch;
        while (num_cols > 0)
        {
            /* Set vl for each iteration. */
            size_t vl = vsetvl_e16m4(num_cols / RGB_PIXELSIZE);
            size_t cols = vl * RGB_PIXELSIZE;
            ptrdiff_t bstride = RGB_PIXELSIZE * sizeof(JSAMPLE);

            /* Load R, G, B channels as vectors from inptr. */
#if BITS_IN_JSAMPLE == 8
            /* Extending to vuint16m4_t type for following multiply calculation. */
            src = vlse8_v_u8m2(inptr + RGB_RED, bstride, vl);
            r = vwaddu_vx_u16m4(src, 0, vl);                        /* Widening to vuint16m4_t type */
            src = vlse8_v_u8m2(inptr + RGB_GREEN, bstride, vl);
            g = vwaddu_vx_u16m4(src, 0, vl);                        /* Widening to vuint16m4_t type */
            src = vlse8_v_u8m2(inptr + RGB_BLUE, bstride, vl);
            b = vwaddu_vx_u16m4(src, 0, vl);                        /* Widening to vuint16m4_t type */
#else   /* BITS_IN_JSAMPLE == 12 */
            r = vlse16_v_u16m4(inptr + RGB_RED, bstride, vl);
            g = vlse16_v_u16m4(inptr + RGB_GREEN, bstride, vl);
            b = vlse16_v_u16m4(inptr + RGB_BLUE, bstride, vl);
#endif
            

            /* (Original)
            * Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
            * Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B + CENTERJSAMPLE
            * Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B + CENTERJSAMPLE
            */
            /* Calculate Y values */
            tmp0 = vwmulu_vx_u32m8(r, F_0_299, vl);
            tmp0 = vwmaccu_vx_u32m8(tmp0, F_0_587, g, vl);
            tmp0 = vwmaccu_vx_u32m8(tmp0, F_0_114, b, vl);
            /* Proper rounding. */
            tmp0 = vadd_vx_u32m8(tmp0, ONE_HALF, vl);
            y = vnsrl_wx_u16m4(tmp0, SCALEBITS, vl);
            /* TODO: Figure out whether big-endian or little-endian would be different. */
#if BITS_IN_JSAMPLE == 8
            dest = vnsrl_wx_u8m2(y, 0, vl); /* Narrowing from 16-bit to 8-bit. */
            vse8_v_u8m2(outptr0, dest, vl);
#else /* BITS_IN_JSAMPLE == 12 */
            vse16_v_u16m4(outptr0, y, vl);
#endif

            /* Calculate Cb values */
            tmp0 = vwmulu_vx_u32m8(b, F_0_500, vl);
            tmp0 = vadd_vx_u32m8(tmp0, SCALED_CENTERJSAMPLE, vl);
            tmp1 = vwmulu_vx_u32m8(g, F_0_331, vl);
            tmp0 = vsub_vv_u32m8(tmp0, tmp1, vl);
            tmp1 = vwmulu_vx_u32m8(r, F_0_168, vl);
            tmp0 = vsub_vv_u32m8(tmp0, tmp1, vl);
            /* Proper rounding. */
            tmp0 = vadd_vx_u32m8(tmp0, ONE_HALF - 1, vl);
            y = vnsrl_wx_u16m4(tmp0, SCALEBITS, vl);
            /* TODO: Figure out whether big-endian or little-endian would be different. */
#if BITS_IN_JSAMPLE == 8
            dest = vnsrl_wx_u8m2(y, 0, vl); /* Narrowing from 16-bit to 8-bit. */
            vse8_v_u8m2(outptr1, dest, vl);
#else /* BITS_IN_JSAMPLE == 12 */
            vse16_v_u16m4(outptr1, y, vl);
#endif

            /* Calculate Cr values */
            tmp0 = vwmulu_vx_u32m8(r, F_0_500, vl);
            tmp0 = vadd_vx_u32m8(tmp0, SCALED_CENTERJSAMPLE, vl);
            tmp1 = vwmulu_vx_u32m8(g, F_0_418, vl);
            tmp0 = vsub_vv_u32m8(tmp0, tmp1, vl);
            tmp1 = vwmulu_vx_u32m8(b, F_0_081, vl);
            tmp0 = vsub_vv_u32m8(tmp0, tmp1, vl);
            /* Proper rounding. */
            tmp0 = vadd_vx_u32m8(tmp0, ONE_HALF - 1, vl);
            y = vnsrl_wx_u16m4(tmp0, SCALEBITS, vl);
            /* TODO: Figure out whether big-endian or little-endian would be different. */
#if BITS_IN_JSAMPLE == 8
            dest = vnsrl_wx_u8m2(y, 0, vl); /* Narrowing from 16-bit to 8-bit. */
            vse8_v_u8m2(outptr2, dest, vl);
#else /* BITS_IN_JSAMPLE == 12 */
            vse16_v_u16m4(outptr2, y, vl);
#endif

            // for (int i = 0; i < vl; i++) {
            //     printf("r%d/g%d/b%d -> ", inptr[i * RGB_PIXELSIZE + RGB_RED], inptr[i * RGB_PIXELSIZE + RGB_GREEN], inptr[i * RGB_PIXELSIZE + RGB_BLUE]);
            //     printf("y%d/cb%d/cr%d\n", outptr0[i], outptr1[i], outptr2[i]);
            // }

            /* Move the pointer to the right place. */
            inptr += cols;
            outptr0 += vl;      outptr1 += vl;      outptr2 += vl;
            num_cols -= cols;
        }
    }
}