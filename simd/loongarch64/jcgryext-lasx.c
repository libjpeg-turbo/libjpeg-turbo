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


GLOBAL(void)
jsimd_rgb_gray_convert_lasx(JDIMENSION img_width, JSAMPARRAY input_buf,
                           JSAMPIMAGE output_buf, JDIMENSION output_row,
                           int num_rows)
{
  JSAMPROW inptr, outptr0;
  JDIMENSION col, col_remainings;
  __m256i r, g, b;
  __m256i src0, src1, src2;
  __m256i const1959 = {19595};
  __m256i const3847 = {38470};
  __m256i const7471 = {7471};
  int rgb_stride = RGB_PIXELSIZE << 5;

#if RGB_PIXELSIZE == 3
  v32i8 mask = {0,1,3,4,6,7,9,10,12,13,15,16,18,19,21,22,
                8,9,11,12,14,15,17,18,20,21,23,24,26,27,29,30};
  v32i8 mask1 = (v32i8)__lasx_xvaddi_bu((__m256i)mask, 1);
#else
  __m256i src3;
#endif

  const1959 = __lasx_xvreplve0_w(const1959);
  const3847 = __lasx_xvreplve0_w(const3847);
  const7471 = __lasx_xvreplve0_w(const7471);

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    output_row++;
    col = img_width;

    LASX_LD_2(inptr, 32, src0, src1);

    for (; col >= 32; col -= 32) {
#if RGB_PIXELSIZE == 4

      LASX_LD_2(inptr + 64, 32, src2, src3);

#if RGB_RED == 0
      /* rgbx */
      {
        __m256i rgl, rgh, bxl, bxh;
        LASX_PCKEV_H_2(src1, src0, src3, src2, rgl, rgh);
        LASX_PCKEV_B(rgh, rgl, r);
        LASX_PCKOD_B(rgh, rgl, g);
        LASX_PCKOD_H_2(src1, src0, src3, src2, bxl, bxh);
        LASX_PCKEV_B(bxh, bxl, b);
      }
#endif

#if RGB_RED == 1
      /* xrgb */
      {
        __m256i xrl, xrh, gbl, gbh;
        LASX_PCKEV_H_2(src1, src0, src3, src2, xrl, xrh);
        LASX_PCKOD_B(xrh, xrl, r);
        LASX_PCKOD_H_2(src1, src0, src3, src2, gbl, gbh);
        LASX_PCKEV_B(gbh, gbl, g);
        LASX_PCKOD_B(gbh, gbl, b);
      }
#endif

#if RGB_RED == 2
      /* bgrx */
      {
        __m256i bgl, bgh, rxl, rxh;
        LASX_PCKEV_H_2(src1, src0, src3, src2, bgl, bgh);
        LASX_PCKEV_B(bgh, bgl, b);
        LASX_PCKOD_B(bgh, bgl, g);
        LASX_PCKOD_H_2(src1, src0, src3, src2, rxl, rxh);
        LASX_PCKEV_B(rxh, rxl, r);
      }
#endif

#if RGB_RED == 3
      /* xbgr */
      {
        __m256i xbl, xbh, grl, grh;
        LASX_PCKEV_H_2(src1, src0, src3, src2, xbl, xbh);
        LASX_PCKOD_B(xbh, xbl, b);
        LASX_PCKOD_H_2(src1, src0, src3, src2, grl, grh);
        LASX_PCKEV_B(grh, grl, g);
        LASX_PCKOD_B(grh, grl, r);
      }
#endif

#else /* end RGB_PIXELSIZE == 4 */

      src2 = LASX_LD(inptr + 64);

#if RGB_RED == 0
      /* rgb */
      {
        __m256i rgl, rgh, gbl, gbh;
        __m256i src01, src12;
        src01 = __lasx_xvpermi_q(src1, src0, 0x21);
        src12 = __lasx_xvpermi_q(src2, src1, 0x21);
        rgl = __lasx_xvshuf_b(src01, src0, (__m256i)mask);
        rgh = __lasx_xvshuf_b(src2, src12, (__m256i)mask);
        LASX_PCKEV_B(rgh, rgl, r);
        LASX_PCKOD_B(rgh, rgl, g);
        gbl = __lasx_xvshuf_b(src01, src0, (__m256i)mask1);
        gbh = __lasx_xvshuf_b(src2, src12, (__m256i)mask1);
        LASX_PCKOD_B(gbh, gbl, b);
      }
#endif

#if RGB_RED == 2
      /* bgr */
      {
        __m256i bgl, bgh, grl, grh;
        __m256i src01, src12;
        src01 = __lasx_xvpermi_q(src1, src0, 0x21);
        src12 = __lasx_xvpermi_q(src2, src1, 0x21);
        bgl = __lasx_xvshuf_b(src01, src0, (__m256i)mask);
        bgh = __lasx_xvshuf_b(src2, src12, (__m256i)mask);
        LASX_PCKEV_B(bgh, bgl, b);
        LASX_PCKOD_B(bgh, bgl, g);
        grl = __lasx_xvshuf_b(src01, src0, (__m256i)mask1);
        grh = __lasx_xvshuf_b(src2, src12, (__m256i)mask1);
        LASX_PCKOD_B(grh, grl, r);
      }
#endif

#endif /* end RGB_PIXELSIZE == 3 */
      {
        __m256i r0, r1, r2, r3;
        __m256i g0, g1, g2, g3;
        __m256i b0, b1, b2, b3;
        __m256i y0, y1, y2, y3;

        LASX_UNPCK_WU_BU_4(r, r0, r1, r2, r3);
        LASX_UNPCK_WU_BU_4(g, g0, g1, g2, g3);
        LASX_UNPCK_WU_BU_4(b, b0, b1, b2, b3);

        /* y */
        LASX_CALC_Y(r0, g0, b0, y0);
        LASX_CALC_Y(r1, g1, b1, y1);
        LASX_CALC_Y(r2, g2, b2, y2);
        LASX_CALC_Y(r3, g3, b3, y3);
        LASX_SRARI_W_4(y0, y1, y2, y3, SCALEBITS);
        LASX_PCKEV_H_2(y1, y0, y3, y2, y0, y2);
        LASX_PCKEV_B(y2, y0, y0);

        LASX_ST(y0, outptr0);
        inptr += rgb_stride;
        LASX_LD_2(inptr, 32, src0, src1);
        outptr0 += 32;
      }
    }/* for(...) */

    col_remainings = col;
    for (col = 0; col < col_remainings; col++) {
      JDIMENSION r = GETSAMPLE(inptr[RGB_RED]);
      JDIMENSION g = GETSAMPLE(inptr[RGB_GREEN]);
      JDIMENSION b = GETSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;
      outptr0[col] = (uint8_t)((19595 * r + 38470 * g + 7471 * b +
                               ONE_HALF) >> SCALEBITS);
    }
  }
}
