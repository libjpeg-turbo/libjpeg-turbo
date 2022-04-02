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

/* CHROMA DOWNSAMPLING */

#include "jsimd_rvv.h"
#include "jcsample.h"

/*
 * Downsample pixel values of a single component.
 * This version handles the common case of 2:1 horizontal and 1:1 vertical,
 * without smoothing.
 *
 * A note about the "bias" calculations: when rounding fractional values to
 * integer, we do not want to always round 0.5 up to the next integer.
 * If we did that, we'd introduce a noticeable bias towards larger values.
 * Instead, this code is arranged so that 0.5 will be rounded up or down at
 * alternate pixel locations (a simple ordered dither pattern).
 */
void jsimd_h2v1_downsample_rvv(JDIMENSION image_width,
                               int max_v_samp_factor,
                               JDIMENSION v_samp_factor,
                               JDIMENSION width_in_blocks,
                               JSAMPARRAY input_data,
                               JSAMPARRAY output_data)
{
  // printf("img_width: %d\nmax_v_samp_factor: %d\nv_samp_factor: %d\nwidth_in_blocks: %d\n", 
  //                                   image_width, max_v_samp_factor, v_samp_factor, width_in_blocks);
  int outrow, outcol, i, zr_one = 0;
  JDIMENSION output_cols = width_in_blocks * DCTSIZE;
  JSAMPROW inptr, outptr;

  vuint8m2_t this, adjct, out;
  /* '_w' suffix denotes widened type. */
  vuint16m4_t this_w, adjct_w, bias_w, out_w;

  /* TODO: If there exists a better way to generate bias sequence. */
  /* Bias */
  size_t vl = vsetvl_e16m4(output_cols * 2);
  uint16_t *bias = NULL;
  if (NULL == (bias = (uint16_t *)malloc(vl * sizeof(uint16_t))))
  {
    /* TODO: throw exception here. */
    printf("\nmem alloc error!\n");
    return;
  }
  for (i = 0; i < vl; i++)
  {
    bias[i] = zr_one;
    zr_one ^= 1;
  }

  expand_right_edge(input_data, max_v_samp_factor, image_width,
                    output_cols * 2);

  for (outrow = 0; outrow < v_samp_factor; outrow++)
  {
    outptr = output_data[outrow];
    inptr = input_data[outrow];

    for (outcol = output_cols; outcol > 0;
         outcol -= vl, inptr += vl * 2, outptr += vl)
    {
      vl = vsetvl_e16m4(outcol * 2);

      /* Load samples and the adjacent ones. */
      this = vlse8_v_u8m2(inptr, 2 * sizeof(JSAMPLE), vl);
      adjct = vlse8_v_u8m2(inptr + 1, 2 * sizeof(JSAMPLE), vl);

      /* Widen to vuint16m4_t type. */
      this_w = vwaddu_vx_u16m4(this, 0, vl);
      adjct_w = vwaddu_vx_u16m4(adjct, 0, vl);
      bias_w = vle16_v_u16m4(bias, vl);

      /* Add adjacent pixel values and add bias. */
      out_w = vadd_vv_u16m4(this_w, adjct_w, vl);
      out_w = vadd_vv_u16m4(out_w, bias_w, vl);

      /* Divide total by 2, narrow to 8-bit, and store. */
      out = vnsrl_wx_u8m2(out_w, 1, vl);
      vse8_v_u8m2(outptr, out, vl);
    }
  }

  /* Free pointer to bias. */
  free(bias);
}

void jsimd_h2v2_downsample_rvv(JDIMENSION image_width, int max_v_samp_factor,
                               JDIMENSION v_samp_factor,
                               JDIMENSION width_in_blocks,
                               JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow, outrow, outcol, i, one_two = 1;
  JDIMENSION output_cols = width_in_blocks * DCTSIZE;
  JSAMPROW inptr0, inptr1, outptr;

  vuint8m2_t this0, adjct0, this1, adjct1, out;
  /* '_w' suffix denotes widened type. */
  vuint16m4_t this_w, adjct_w, bias_w, out_w;

  /* TODO: If there exists a better way to generate bias sequence. */
  /* Bias */
  size_t vl = vsetvl_e16m4(output_cols * 2);
  uint16_t *bias = NULL;
  if (NULL == (bias = (uint16_t *)malloc(vl * sizeof(uint16_t))))
  {
    /* TODO: throw exception here. */
    return;
  }
  for (i = 0; i < vl; i++)
  {
    bias[i] = one_two;
    one_two ^= 3;
  }

  expand_right_edge(input_data, max_v_samp_factor, image_width,
                    output_cols * 2);

  for (inrow = 0, outrow = 0; outrow < v_samp_factor;
       inrow += 2, outrow++)
  {
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];
    outptr = output_data[outrow];

    for (outcol = output_cols; outcol > 0;
         outcol -= vl, inptr0 += vl * 2, inptr1 += vl * 2, outptr += vl)
    {
      vl = vsetvl_e16m4(outcol * 2);

      /* Load samples and the adjacent ones of two rows. */
      this0 = vlse8_v_u8m2(inptr0, 2 * sizeof(JSAMPLE), vl);
      adjct0 = vlse8_v_u8m2(inptr0 + 1, 2 * sizeof(JSAMPLE), vl);
      this1 = vlse8_v_u8m2(inptr1, 2 * sizeof(JSAMPLE), vl);
      adjct1 = vlse8_v_u8m2(inptr1 + 1, 2 * sizeof(JSAMPLE), vl);

      /* Widen samples in row 0 and bias to vuint16m4_t type. */
      this_w = vwaddu_vx_u16m4(this0, 0, vl);
      adjct_w = vwaddu_vx_u16m4(adjct0, 0, vl);
      bias_w = vle16_v_u16m4(bias, vl);

      /* Add adjacent pixel values in row 0 and add bias. */
      out_w = vadd_vv_u16m4(this_w, adjct_w, vl);
      out_w = vadd_vv_u16m4(out_w, bias_w, vl);

      /* Widen samples of in row 1 to vuint16m4_t type. */
      this_w = vwaddu_vx_u16m4(this1, 0, vl);
      adjct_w = vwaddu_vx_u16m4(adjct1, 0, vl);

      /* Add adjacent pixel values in row 1 and add accumulate. */
      out_w = vadd_vv_u16m4(out_w, this_w, vl);
      out_w = vadd_vv_u16m4(out_w, adjct_w, vl);

      /* Divide total by 4, narrow to 8-bit, and store. */
      out = vnsrl_wx_u8m2(out_w, 2, vl);
      vse8_v_u8m2(outptr, out, vl);
    }
  }

  /* Free pointer to bias. */
  free(bias);
}
