/*
 * LOONGARCH LSX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2023 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Song Ding (songding@loongson.cn)
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
jsimd_rgb_gray_convert_lsx(JDIMENSION img_width, JSAMPARRAY input_buf,
                           JSAMPIMAGE output_buf, JDIMENSION output_row,
                           int num_rows)
{
  JSAMPROW inptr, outptr0;
  JDIMENSION col, col_remainings;
  __m128i r, g, b;
  __m128i src0, src1, src2;
  __m128i const1959 = {19595};
  __m128i const3847 = {38470};
  __m128i const7471 = {7471};
  int rgb_stride = RGB_PIXELSIZE << 4;

#if RGB_PIXELSIZE == 3
  v16i8 mask = {0,1,3,4,6,7,9,10,12,13,15,24,26,27,29,30};
  v16i8 mask1 = (v16i8)__lsx_vaddi_bu((__m128i)mask, 1);
#else
  __m128i src3;
#endif

  const1959 = __lsx_vreplvei_w(const1959, 0);
  const3847 = __lsx_vreplvei_w(const3847, 0);
  const7471 = __lsx_vreplvei_w(const7471, 0);

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    output_row++;
    col = img_width;

    LSX_LD_2(inptr, 16, src0, src1);

    for (; col >= 16; col -= 16) {
#if RGB_PIXELSIZE == 4

      LSX_LD_2(inptr + 32, 16, src2, src3);

#if RGB_RED == 0
      /* rgbx */
      {
        __m128i rgl, rgh, bxl, bxh;
        LSX_PCKEV_H_2(src1, src0, src3, src2, rgl, rgh);
        LSX_PCKEV_B(rgh, rgl, r);
        LSX_PCKOD_B(rgh, rgl, g);
        LSX_PCKOD_H_2(src1, src0, src3, src2, bxl, bxh);
        LSX_PCKEV_B(bxh, bxl, b);
      }
#endif

#if RGB_RED == 1
      /* xrgb */
      {
        __m128i xrl, xrh, gbl, gbh;
        LSX_PCKEV_H_2(src1, src0, src3, src2, xrl, xrh);
        LSX_PCKOD_B(xrh, xrl, r);
        LSX_PCKOD_H_2(src1, src0, src3, src2, gbl, gbh);
        LSX_PCKEV_B(gbh, gbl, g);
        LSX_PCKOD_B(gbh, gbl, b);
      }
#endif

#if RGB_RED == 2
      /* bgrx */
      {
        __m128i bgl, bgh, rxl, rxh;
        LSX_PCKEV_H_2(src1, src0, src3, src2, bgl, bgh);
        LSX_PCKEV_B(bgh, bgl, b);
        LSX_PCKOD_B(bgh, bgl, g);
        LSX_PCKOD_H_2(src1, src0, src3, src2, rxl, rxh);
        LSX_PCKEV_B(rxh, rxl, r);
      }
#endif

#if RGB_RED == 3
      /* xbgr */
      {
        __m128i xbl, xbh, grl, grh;
        LSX_PCKEV_H_2(src1, src0, src3, src2, xbl, xbh);
        LSX_PCKOD_B(xbh, xbl, b);
        LSX_PCKOD_H_2(src1, src0, src3, src2, grl, grh);
        LSX_PCKEV_B(grh, grl, g);
        LSX_PCKOD_B(grh, grl, r);
      }
#endif

#else /* end RGB_PIXELSIZE == 4 */

      src2 = LSX_LD(inptr + 32);

#if RGB_RED == 0
      /* rgb */
      {
        __m128i rgl, rgh, gbl, gbh;
        __m128i src01, src12;
        src01 = __lsx_vpermi_w(src1, src0, 0x4E);
        src12 = __lsx_vpermi_w(src2, src1, 0x4E);
        rgl = __lsx_vshuf_b(src01, src0, (__m128i)mask);
        rgh = __lsx_vshuf_b(src2, src12, (__m128i)mask);
        LSX_PCKEV_B(rgh, rgl, r);
        LSX_PCKOD_B(rgh, rgl, g);
        gbl = __lsx_vshuf_b(src01, src0, (__m128i)mask1);
        gbh = __lsx_vshuf_b(src2, src12, (__m128i)mask1);
        LSX_PCKOD_B(gbh, gbl, b);
      }
#endif

#if RGB_RED == 2
      /* bgr */
      {
        __m128i bgl, bgh, grl, grh;
        __m128i src01, src12;
        src01 = __lsx_vpermi_w(src1, src0, 0x4E);
        src12 = __lsx_vpermi_w(src2, src1, 0x4E);
        bgl = __lsx_vshuf_b(src01, src0, (__m128i)mask);
        bgh = __lsx_vshuf_b(src2, src12, (__m128i)mask);
        LSX_PCKEV_B(bgh, bgl, b);
        LSX_PCKOD_B(bgh, bgl, g);
        grl = __lsx_vshuf_b(src01, src0, (__m128i)mask1);
        grh = __lsx_vshuf_b(src2, src12, (__m128i)mask1);
        LSX_PCKOD_B(grh, grl, r);
      }
#endif

#endif /* end RGB_PIXELSIZE == 3 */
      {
        __m128i r0, r1, r2, r3;
        __m128i g0, g1, g2, g3;
        __m128i b0, b1, b2, b3;
        __m128i y0, y1, y2, y3;

        LSX_UNPCK_WU_BU_4(r, r0, r1, r2, r3);
        LSX_UNPCK_WU_BU_4(g, g0, g1, g2, g3);
        LSX_UNPCK_WU_BU_4(b, b0, b1, b2, b3);

        /* y */
        LSX_CALC_Y(r0, g0, b0, y0);
        LSX_CALC_Y(r1, g1, b1, y1);
        LSX_CALC_Y(r2, g2, b2, y2);
        LSX_CALC_Y(r3, g3, b3, y3);
        LSX_SRARI_W_4(y0, y1, y2, y3, SCALEBITS);
        LSX_PCKEV_H_2(y1, y0, y3, y2, y0, y2);
        LSX_PCKEV_B(y2, y0, y0);

        LSX_ST(y0, outptr0);
        inptr += rgb_stride;
        LSX_LD_2(inptr, 16, src0, src1);
        outptr0 += 16;
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
