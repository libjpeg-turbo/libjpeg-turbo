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

#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "jcsample.h"
#include "jmacros_lsx.h"

GLOBAL(void)
jsimd_h2v1_downsample_lsx(JDIMENSION image_width, int max_v_samp_factor,
                          JDIMENSION v_samp_factor, JDIMENSION width_blocks,
                          JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int outrow, bias;
  JDIMENSION outcol;
  JDIMENSION output_cols = width_blocks << 3;
  JSAMPROW inptr, outptr;
  v8i16 plus01 = {0,1,0,1,0,1,0,1};
  __m128i src0, src1;

  expand_right_edge(input_data, max_v_samp_factor,
                    image_width, output_cols << 1);

  for (outrow = 0; outrow < v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr = input_data[outrow];
    outcol = output_cols;

    for (; outcol >= 16; outcol -= 16) {
      LSX_LD_2(inptr, 16, src0, src1);
      src0 = __lsx_vhaddw_hu_bu(src0, src0);
      src1 = __lsx_vhaddw_hu_bu(src1, src1);
      src0 = __lsx_vadd_h(src0, (__m128i)plus01);
      src1 = __lsx_vadd_h(src1, (__m128i)plus01);
      src0 = __lsx_vsrai_h(src0, 1);
      src1 = __lsx_vsrai_h(src1, 1);
      LSX_PCKEV_B(src1, src0, src0);
      LSX_ST(src0, outptr);
      outptr += 16;
      inptr += 32;
    }

    if (outcol >= 8) {
      src0 = LSX_LD(inptr);
      src0 = __lsx_vhaddw_hu_bu(src0, src0);
      src0 = __lsx_vadd_h(src0, (__m128i)plus01);
      src0 = __lsx_vsrai_h(src0, 1);
      src0 = __lsx_vpickev_b(src0, src0);
      __lsx_vstelm_d(src0, outptr, 0, 0);
      outptr += 8;
      inptr += 16;
      outcol -= 8;
    }

    if (outcol >= 4) {
      src0 = LSX_LD(inptr);
      src0 = __lsx_vhaddw_hu_bu(src0, src0);
      src0 = __lsx_vadd_h(src0, (__m128i)plus01);
      src0 = __lsx_vsrai_h(src0, 1);
      src0 = __lsx_vpickev_b(src0, src0);
      __lsx_vstelm_w(src0, outptr, 0, 0);
      outptr += 4;
      inptr += 8;
      outcol -= 4;
    }

    bias = 0;
    for (; outcol > 0; outcol--) {
      *outptr++ = (JSAMPLE) ((GETJSAMPLE(*inptr) + GETJSAMPLE(inptr[1])
                              + bias) >> 1);
      bias ^= 1;
      inptr += 2;
    }
  }
}

GLOBAL(void)
jsimd_h2v2_downsample_lsx(JDIMENSION image_width, int max_v_samp_factor,
                          JDIMENSION v_samp_factor, JDIMENSION width_blocks,
                          JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow, outrow, bias;
  JDIMENSION outcol;
  JDIMENSION output_cols = width_blocks << 3;
  JSAMPROW inptr0, inptr1, outptr;
  v8i16 plus12 = {1,2,1,2,1,2,1,2};
  __m128i src0, src1, src2, src3;

  expand_right_edge(input_data, max_v_samp_factor,
                    image_width, output_cols << 1);
  inrow = 0;
  for (outrow = 0; outrow < v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];
    outcol = output_cols;

    for (; outcol >= 16; outcol -= 16) {
      LSX_LD_2(inptr0, 16, src0, src1);
      LSX_LD_2(inptr1, 16, src2, src3);
      src0 = __lsx_vhaddw_hu_bu(src0, src0);
      src1 = __lsx_vhaddw_hu_bu(src1, src1);
      src2 = __lsx_vhaddw_hu_bu(src2, src2);
      src3 = __lsx_vhaddw_hu_bu(src3, src3);
      src0 = __lsx_vadd_h(src0, src2);
      src1 = __lsx_vadd_h(src1, src3);
      src0 = __lsx_vadd_h(src0, (__m128i)plus12);
      src1 = __lsx_vadd_h(src1, (__m128i)plus12);
      src0 = __lsx_vsrai_h(src0, 2);
      src1 = __lsx_vsrai_h(src1, 2);
      LSX_PCKEV_B(src1, src0, src0);
      LSX_ST(src0, outptr);
      outptr += 16;
      inptr0 += 32;
      inptr1 += 32;
    }

    if (outcol >= 8) {
      src0 = LSX_LD(inptr0);
      src1 = LSX_LD(inptr1);
      src0 = __lsx_vhaddw_hu_bu(src0, src0);
      src1 = __lsx_vhaddw_hu_bu(src1, src1);
      src0 = __lsx_vadd_h(src0, src1);
      src0 = __lsx_vadd_h(src0, (__m128i)plus12);
      src0 = __lsx_vsrai_h(src0, 2);
      src0 = __lsx_vpickev_b(src0, src0);
      __lsx_vstelm_d(src0, outptr, 0, 0);
      outptr += 8;
      inptr0 += 16;
      inptr1 += 16;
      outcol -= 8;
    }

    if (outcol >= 4) {
      src0 = LSX_LD(inptr0);
      src1 = LSX_LD(inptr1);
      src0 = __lsx_vhaddw_hu_bu(src0, src0);
      src1 = __lsx_vhaddw_hu_bu(src1, src1);
      src0 = __lsx_vadd_h(src0, src1);
      src0 = __lsx_vadd_h(src0, (__m128i)plus12);
      src0 = __lsx_vsrai_h(src0, 2);
      src0 = __lsx_vpickev_b(src0, src0);
      __lsx_vstelm_w(src0, outptr, 0, 0);
      outptr += 4;
      inptr0 += 8;
      inptr1 += 8;
      outcol -= 4;
    }

    bias = 1;
    for (; outcol > 0; outcol--) {
      *outptr++ = (JSAMPLE) ((GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[1]) +
                              GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[1])
                              + bias) >> 2);
      bias ^= 3;
      inptr0 += 2; inptr1 += 2;
    }
    inrow += 2;
  }
}
