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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "jmacros_lsx.h"

/*
 * Fancy processing for the common case of 2:1 horizontal and 1:1 vertical.
 * The centers of the output pixels are 1/4 and 3/4 of the way between input
 * pixel centers.
*/
GLOBAL(void)
jsimd_h2v1_fancy_upsample_lsx(int max_v_samp_factor,
                              JDIMENSION downsampled_width,
                              JSAMPARRAY input_data,
                              JSAMPARRAY *output_data_ptr)
{
  int invalue, row, col;
  JSAMPROW srcptr, outptr;
  JSAMPARRAY output_data = *output_data_ptr;
  __m128i src0, src1, src2, out0, out1, tmp0, tmp1;
  __m128i src0_l, src0_h, src1_l, src1_h, src2_l, src2_h;

  for (row = 0; row < max_v_samp_factor; row++) {
    srcptr = input_data[row];
    outptr = output_data[row];
    col = downsampled_width;

    for (; col >= 16; col -= 16) {
      src0 = LSX_LD(srcptr - 1);
      src1 = LSX_LD(srcptr);
      src2 = LSX_LD(srcptr + 1);

      srcptr += 16;

      LSX_UNPCKLH_HU_BU(src0, src0_h, src0_l);
      LSX_UNPCKLH_HU_BU(src1, src1_h, src1_l);
      LSX_UNPCKLH_HU_BU(src2, src2_h, src2_l);

      tmp0 = __lsx_vslli_h(src1_l, 1);
      tmp1 = __lsx_vslli_h(src1_h, 1);
      src1_l = __lsx_vadd_h(src1_l, tmp0);
      src1_h = __lsx_vadd_h(src1_h, tmp1);

      src0_l = __lsx_vadd_h(src0_l, src1_l);
      src0_h = __lsx_vadd_h(src0_h, src1_h);
      src0_l = __lsx_vaddi_hu(src0_l, 1);
      src0_h = __lsx_vaddi_hu(src0_h, 1);

      src2_l = __lsx_vadd_h(src2_l, src1_l);
      src2_h = __lsx_vadd_h(src2_h, src1_h);
      src2_l = __lsx_vaddi_hu(src2_l, 2);
      src2_h = __lsx_vaddi_hu(src2_h, 2);

      src0_l = __lsx_vsrai_h(src0_l, 2);
      src0_h = __lsx_vsrai_h(src0_h, 2);
      src2_l = __lsx_vsrai_h(src2_l, 2);
      src2_h = __lsx_vsrai_h(src2_h, 2);

      out0 = __lsx_vpackev_b(src2_l, src0_l);
      out1 = __lsx_vpackev_b(src2_h, src0_h);

      LSX_ST_2(out0, out1, outptr, 16);
      outptr += 32;
    }

    if (col >= 8) {
      src0 = LSX_LD(srcptr - 1);
      src1 = LSX_LD(srcptr);
      src2 = LSX_LD(srcptr + 1);

      srcptr += 8;
      col -= 8;

      LSX_UNPCKL_HU_BU(src0, src0_l);
      LSX_UNPCKL_HU_BU(src1, src1_l);
      LSX_UNPCKL_HU_BU(src2, src2_l);

      tmp0 = __lsx_vslli_h(src1_l, 1);
      src1_l = __lsx_vadd_h(src1_l, tmp0);

      src0_l = __lsx_vadd_h(src0_l, src1_l);
      src0_l = __lsx_vaddi_hu(src0_l, 1);

      src2_l = __lsx_vadd_h(src2_l, src1_l);
      src2_l = __lsx_vaddi_hu(src2_l, 2);

      src0_l = __lsx_vsrai_h(src0_l, 2);
      src2_l = __lsx_vsrai_h(src2_l, 2);

      out0 = __lsx_vpackev_b(src2_l, src0_l);
      LSX_ST(out0, outptr);
      outptr += 16;
    }

    for(; col > 0; col--) {
      invalue = GETJSAMPLE(*srcptr++) * 3;
      *outptr++ = (JSAMPLE) ((invalue + GETJSAMPLE(srcptr[-2]) + 1) >> 2);
      *outptr++ = (JSAMPLE) ((invalue + GETJSAMPLE(*srcptr) + 2) >> 2);
    }

    /* update fisrt col */
    *(output_data[row]) = *(input_data[row]);
    /* update last col */
    *(output_data[row] + (downsampled_width << 1) - 1) = *(input_data[row] + downsampled_width - 1);
  }
}

/*
 * Fancy processing for the common case of 2:1 horizontal and 2:1 vertical.
 * Again a triangle filter; see comments for h2v1 case, above.
 *
 * It is OK for us to reference the adjacent input rows because we demanded
 * context from the main buffer controller (see initialization code).
 */
GLOBAL(void)
jsimd_h2v2_fancy_upsample_lsx(int max_v_samp_factor,
                               JDIMENSION downsampled_width,
                               JSAMPARRAY input_data,
                               JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW srcptr0, srcptr1, srcptr2, outptr0, outptr1;
  int thissum0, lastsum0, nextsum0, thissum1, lastsum1, nextsum1;
  int col, srcrow = 0, outrow = 0, tmp0, tmp1, tmp2;
  __m128i tmp0_l, tmp1_l, tmp2_l, tmp0_h, tmp1_h, tmp2_h;
  __m128i pre0, pre1, pre2, cur0, cur1, cur2, nxt0, nxt1, nxt2;
  __m128i presum0_l, presum1_l, cursum0_l, cursum1_l, nxtsum0_l, nxtsum1_l;
  __m128i presum0_h, presum1_h, cursum0_h, cursum1_h, nxtsum0_h, nxtsum1_h;
  __m128i out00_l, out01_l, out10_l, out11_l;
  __m128i out00_h, out01_h, out10_h, out11_h;
  __m128i const3 = {3};

  const3 = __lsx_vreplvei_h(const3, 0);
  while (outrow < max_v_samp_factor) {
    srcptr0 = input_data[srcrow];
    srcptr1 = input_data[srcrow - 1];
    srcptr2 = input_data[srcrow + 1];

    outptr0 = output_data[outrow];
    outptr1 = output_data[outrow + 1];
    col = downsampled_width;

    for (; col >= 16; col -= 16) {
      pre0 = LSX_LD(srcptr0 - 1);
      pre1 = LSX_LD(srcptr1 - 1);
      pre2 = LSX_LD(srcptr2 - 1);

      cur0 = LSX_LD(srcptr0);
      cur1 = LSX_LD(srcptr1);
      cur2 = LSX_LD(srcptr2);

      nxt0 = LSX_LD(srcptr0 + 1);
      nxt1 = LSX_LD(srcptr1 + 1);
      nxt2 = LSX_LD(srcptr2 + 1);

      srcptr0 += 16;
      srcptr1 += 16;
      srcptr2 += 16;

      LSX_UNPCKLH_HU_BU(pre0, tmp0_h, tmp0_l);
      LSX_UNPCKLH_HU_BU(pre1, tmp1_h, tmp1_l);
      LSX_UNPCKLH_HU_BU(pre2, tmp2_h, tmp2_l);

      presum0_l = __lsx_vmadd_h(tmp1_l, tmp0_l, const3);
      presum0_h = __lsx_vmadd_h(tmp1_h, tmp0_h, const3);
      presum1_l = __lsx_vmadd_h(tmp2_l, tmp0_l, const3);
      presum1_h = __lsx_vmadd_h(tmp2_h, tmp0_h, const3);

      LSX_UNPCKLH_HU_BU(cur0, tmp0_h, tmp0_l);
      LSX_UNPCKLH_HU_BU(cur1, tmp1_h, tmp1_l);
      LSX_UNPCKLH_HU_BU(cur2, tmp2_h, tmp2_l);

      cursum0_l = __lsx_vmadd_h(tmp1_l, tmp0_l, const3);
      cursum0_h = __lsx_vmadd_h(tmp1_h, tmp0_h, const3);
      cursum1_l = __lsx_vmadd_h(tmp2_l, tmp0_l, const3);
      cursum1_h = __lsx_vmadd_h(tmp2_h, tmp0_h, const3);

      LSX_UNPCKLH_HU_BU(nxt0, tmp0_h, tmp0_l);
      LSX_UNPCKLH_HU_BU(nxt1, tmp1_h, tmp1_l);
      LSX_UNPCKLH_HU_BU(nxt2, tmp2_h, tmp2_l);

      nxtsum0_l = __lsx_vmadd_h(tmp1_l, tmp0_l, const3);
      nxtsum0_h = __lsx_vmadd_h(tmp1_h, tmp0_h, const3);
      nxtsum1_l = __lsx_vmadd_h(tmp2_l, tmp0_l, const3);
      nxtsum1_h = __lsx_vmadd_h(tmp2_h, tmp0_h, const3);

      /* outptr0 */
      out00_l = __lsx_vmadd_h(presum0_l, cursum0_l, const3);
      out00_l = __lsx_vaddi_hu(out00_l, 8);
      out00_l = __lsx_vsrai_h(out00_l, 4);
      out01_l = __lsx_vmadd_h(nxtsum0_l, cursum0_l, const3);
      out01_l = __lsx_vaddi_hu(out01_l, 7);
      out01_l = __lsx_vsrai_h(out01_l, 4);
      out00_h = __lsx_vmadd_h(presum0_h, cursum0_h, const3);
      out00_h = __lsx_vaddi_hu(out00_h, 8);
      out00_h = __lsx_vsrai_h(out00_h, 4);
      out01_h = __lsx_vmadd_h(nxtsum0_h, cursum0_h, const3);
      out01_h = __lsx_vaddi_hu(out01_h, 7);
      out01_h = __lsx_vsrai_h(out01_h, 4);

      /* outptr1 */
      out10_l = __lsx_vmadd_h(presum1_l, cursum1_l, const3);
      out10_l = __lsx_vaddi_hu(out10_l, 8);
      out10_l = __lsx_vsrai_h(out10_l, 4);
      out11_l = __lsx_vmadd_h(nxtsum1_l, cursum1_l, const3);
      out11_l = __lsx_vaddi_hu(out11_l, 7);
      out11_l = __lsx_vsrai_h(out11_l, 4);
      out10_h = __lsx_vmadd_h(presum1_h, cursum1_h, const3);
      out10_h = __lsx_vaddi_hu(out10_h, 8);
      out10_h = __lsx_vsrai_h(out10_h, 4);
      out11_h = __lsx_vmadd_h(nxtsum1_h, cursum1_h, const3);
      out11_h = __lsx_vaddi_hu(out11_h, 7);
      out11_h = __lsx_vsrai_h(out11_h, 4);

      out00_l = __lsx_vpackev_b(out01_l, out00_l);
      out10_l = __lsx_vpackev_b(out11_l, out10_l);
      out00_h = __lsx_vpackev_b(out01_h, out00_h);
      out10_h = __lsx_vpackev_b(out11_h, out10_h);

      LSX_ST_2(out00_l, out00_h, outptr0, 16);
      LSX_ST_2(out10_l, out10_h, outptr1, 16);
      outptr0 += 32;
      outptr1 += 32;
    }

    if (col >= 8) {
      pre0 = LSX_LD(srcptr0 - 1);
      pre1 = LSX_LD(srcptr1 - 1);
      pre2 = LSX_LD(srcptr2 - 1);

      cur0 = LSX_LD(srcptr0);
      cur1 = LSX_LD(srcptr1);
      cur2 = LSX_LD(srcptr2);

      nxt0 = LSX_LD(srcptr0 + 1);
      nxt1 = LSX_LD(srcptr1 + 1);
      nxt2 = LSX_LD(srcptr2 + 1);

      srcptr0 += 8;
      srcptr1 += 8;
      srcptr2 += 8;
      col -= 8;

      LSX_UNPCKL_HU_BU(pre0, tmp0_l);
      LSX_UNPCKL_HU_BU(pre1, tmp1_l);
      LSX_UNPCKL_HU_BU(pre2, tmp2_l);

      presum0_l = __lsx_vmadd_h(tmp1_l, tmp0_l, const3);
      presum1_l = __lsx_vmadd_h(tmp2_l, tmp0_l, const3);

      LSX_UNPCKL_HU_BU(cur0, tmp0_l);
      LSX_UNPCKL_HU_BU(cur1, tmp1_l);
      LSX_UNPCKL_HU_BU(cur2, tmp2_l);

      cursum0_l = __lsx_vmadd_h(tmp1_l, tmp0_l, const3);
      cursum1_l = __lsx_vmadd_h(tmp2_l, tmp0_l, const3);

      LSX_UNPCKL_HU_BU(nxt0, tmp0_l);
      LSX_UNPCKL_HU_BU(nxt1, tmp1_l);
      LSX_UNPCKL_HU_BU(nxt2, tmp2_l);

      nxtsum0_l = __lsx_vmadd_h(tmp1_l, tmp0_l, const3);
      nxtsum1_l = __lsx_vmadd_h(tmp2_l, tmp0_l, const3);

      /* outptr0 */
      out00_l = __lsx_vmadd_h(presum0_l, cursum0_l, const3);
      out00_l = __lsx_vaddi_hu(out00_l, 8);
      out00_l = __lsx_vsrai_h(out00_l, 4);
      out01_l = __lsx_vmadd_h(nxtsum0_l, cursum0_l, const3);
      out01_l = __lsx_vaddi_hu(out01_l, 7);
      out01_l = __lsx_vsrai_h(out01_l, 4);

      /* outptr1 */
      out10_l = __lsx_vmadd_h(presum1_l, cursum1_l, const3);
      out10_l = __lsx_vaddi_hu(out10_l, 8);
      out10_l = __lsx_vsrai_h(out10_l, 4);
      out11_l = __lsx_vmadd_h(nxtsum1_l, cursum1_l, const3);
      out11_l = __lsx_vaddi_hu(out11_l, 7);
      out11_l = __lsx_vsrai_h(out11_l, 4);

      out00_l = __lsx_vpackev_b(out01_l, out00_l);
      out10_l = __lsx_vpackev_b(out11_l, out10_l);

      LSX_ST(out00_l, outptr0);
      LSX_ST(out10_l, outptr1);
      outptr0 += 16;
      outptr1 += 16;
    }

    for (; col > 0; col--) {
      tmp0 = (*srcptr0++) * 3;
      tmp1 = (*(srcptr0 - 2)) * 3;
      tmp2 = (*(srcptr0)) * 3;

      thissum0 = tmp0 + GETJSAMPLE(*srcptr1++);
      lastsum0 = tmp1 + GETJSAMPLE(*(srcptr1 - 2));
      nextsum0 = tmp2 + GETJSAMPLE(*(srcptr1));
      *outptr0++ = (JSAMPLE) ((thissum0 * 3 + lastsum0 + 8) >> 4);
      *outptr0++ = (JSAMPLE) ((thissum0 * 3 + nextsum0 + 7) >> 4);

      thissum1 = tmp0 + GETJSAMPLE(*srcptr2++);
      lastsum1 = tmp1 + GETJSAMPLE(*(srcptr2 - 2));
      nextsum1 = tmp2 + GETJSAMPLE(*(srcptr2));
      *outptr1++ = (JSAMPLE) ((thissum1 * 3 + lastsum1 + 8) >> 4);
      *outptr1++ = (JSAMPLE) ((thissum1 * 3 + nextsum1 + 7) >> 4);
    }

    /* update fisrt col for v = 0 */
    thissum0 = GETJSAMPLE(*input_data[srcrow]) * 3 + GETJSAMPLE(*input_data[srcrow - 1]);
    *(output_data[outrow]) = (JSAMPLE) ((thissum0 * 4 + 8) >> 4);

    /* update last col for v = 0 */
    thissum0 = GETJSAMPLE(*(input_data[srcrow] + downsampled_width - 1)) * 3 +
               GETJSAMPLE(*(input_data[srcrow - 1] + downsampled_width - 1));
    *(output_data[outrow] + (downsampled_width << 1) - 1) = (JSAMPLE) ((thissum0 * 4 + 7) >> 4);

    /* update fisrt col for v = 1 */
    thissum0 = GETJSAMPLE(*input_data[srcrow]) * 3 + GETJSAMPLE(*input_data[srcrow + 1]);
    *(output_data[outrow + 1]) = (JSAMPLE) ((thissum0 * 4 + 8) >> 4);

    /* update last col for v = 1 */
    thissum0 = GETJSAMPLE(*(input_data[srcrow] + downsampled_width - 1)) * 3 +
               GETJSAMPLE(*(input_data[srcrow + 1] + downsampled_width - 1));
    *(output_data[outrow + 1] + (downsampled_width << 1) - 1) = (JSAMPLE) ((thissum0 * 4 + 7) >> 4);

    srcrow++;
    outrow += 2;
  }
}

/*
 * Fast processing for the common case of 2:1 horizontal and 1:1 vertical.
 * It's still a box filter.
 */

GLOBAL(void)
jsimd_h2v1_upsample_lsx(int max_v_samp_factor,
                         JDIMENSION output_width,
                         JSAMPARRAY input_data,
                         JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  int col, loop, srcrow;
  JSAMPROW srcptr, outptr;
  __m128i src0, src0_l, src0_h;

  loop = output_width & ~31;
  for (srcrow = 0; srcrow < max_v_samp_factor; srcrow++) {
    srcptr = input_data[srcrow];
    outptr = output_data[srcrow];
    col = 0;

    for (; (col << 1) < loop; col += 16) {
      src0 = LSX_LD(srcptr + col);
      LSX_ILVLH_B(src0, src0, src0_h, src0_l);
      LSX_ST_2(src0_l, src0_h, outptr + (col << 1), 16);
    }

    for (; (col << 1) < output_width; col += 8) {
      src0 = LSX_LD(srcptr + col);
      LSX_ILVL_B(src0, src0, src0_l);
      LSX_ST(src0_l, outptr + (col << 1));
    }
  }
}


GLOBAL(void)
jsimd_h2v2_upsample_lsx(int max_v_samp_factor,
                         JDIMENSION output_width,
                         JSAMPARRAY input_data,
                         JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  int col, loop, srcrow, outrow;
  JSAMPROW srcptr, outptr0, outptr1;
  __m128i src0, src0_l, src0_h;

  loop = output_width & ~31;
  for (srcrow = 0, outrow = 0; outrow < max_v_samp_factor; srcrow++) {
    srcptr = input_data[srcrow];
    outptr0 = output_data[outrow++];
    outptr1 = output_data[outrow++];
    col = 0;

    for (; (col << 1) < loop; col += 16) {
      src0 = LSX_LD(srcptr + col);
      LSX_ILVLH_B(src0, src0, src0_h, src0_l);
      LSX_ST_2(src0_l, src0_h, outptr0 + (col << 1), 16);
      LSX_ST_2(src0_l, src0_h, outptr1 + (col << 1), 16);
    }

    for (; (col << 1) < output_width; col += 8) {
      src0 = LSX_LD(srcptr + col);
      LSX_ILVL_B(src0, src0, src0_l);
      LSX_ST(src0_l, outptr0 + (col << 1));
      LSX_ST(src0_l, outptr1 + (col << 1));
    }
  }
}
