/*
 * MIPS MSA optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * All rights reserved.
 * Authors: Vikram Dattu (vikram.dattu@imgtec.com)
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

#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "jmacros_msa.h"


GLOBAL(void)
jsimd_h2v1_downsample_msa(JDIMENSION image_width, int max_v_samp_factor,
                          JDIMENSION v_samp_factor, JDIMENSION width_blocks,
                          JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  unsigned char end_pixel;
  int outcol, outrow;
  JSAMPROW inptr, outptr;
  unsigned short *tmp_ptr;
  v16i8 src0, src1, src2, src3, src4, src5, src6, src7, dst0, dst1;
  v8i16 tmp0_r, tmp0_l, tmp1_r, tmp1_l;
  const v8i16 bias = { 0, 1, 0, 1, 0, 1, 0, 1 };
  JDIMENSION out_width = width_blocks * 8;

  for (outrow = 0; outrow < v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr = input_data[outrow];

    outcol = 0;

    for (; outcol < (out_width & ~0x3F); outcol += 64) {
      LD_SB8(inptr, 16, src0, src1, src2, src3, src4, src5, src6, src7);
      inptr += 128;

      HADD_UB2_SH(src0, src1, tmp0_r, tmp0_l);
      HADD_UB2_SH(src2, src3, tmp1_r, tmp1_l);
      ADD4(tmp0_r, bias, tmp0_l, bias, tmp1_r, bias, tmp1_l, bias,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      SRAI_H4_SH(tmp0_r, tmp0_l, tmp1_r, tmp1_l, 1);
      dst0 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);
      dst1 = __msa_pckev_b((v16i8)tmp1_l, (v16i8)tmp1_r);
      ST_SB2(dst0, dst1, outptr, 16);
      outptr += 32;

      HADD_UB2_SH(src4, src5, tmp0_r, tmp0_l);
      HADD_UB2_SH(src6, src7, tmp1_r, tmp1_l);
      ADD4(tmp0_r, bias, tmp0_l, bias, tmp1_r, bias, tmp1_l, bias,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      SRAI_H4_SH(tmp0_r, tmp0_l, tmp1_r, tmp1_l, 1);
      dst0 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);
      dst1 = __msa_pckev_b((v16i8)tmp1_l, (v16i8)tmp1_r);

      ST_SB2(dst0, dst1, outptr, 16);
      outptr += 32;
    }

    if (outcol < (out_width & ~0x1F)) {
      LD_SB4(inptr, 16, src0, src1, src2, src3);
      inptr += 64;

      HADD_UB2_SH(src0, src1, tmp0_r, tmp0_l);
      HADD_UB2_SH(src2, src3, tmp1_r, tmp1_l);
      ADD4(tmp0_r, bias, tmp0_l, bias, tmp1_r, bias, tmp1_l, bias,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      SRAI_H4_SH(tmp0_r, tmp0_l, tmp1_r, tmp1_l, 1);
      dst0 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);
      dst1 = __msa_pckev_b((v16i8)tmp1_l, (v16i8)tmp1_r);

      ST_SB2(dst0, dst1, outptr, 16);
      outptr += 32;
      outcol += 32;
    }

    if (outcol < (out_width & ~0xF)) {
      LD_SB2(inptr, 16, src0, src1);
      inptr += 32;

      HADD_UB2_SH(src0, src1, tmp0_r, tmp0_l);
      ADD2(tmp0_r, bias, tmp0_l, bias, tmp0_r, tmp0_l);
      SRAI_H2_SH(tmp0_r, tmp0_l, 1);
      dst0 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);

      ST_SB(dst0, outptr);
      outptr += 16;
      outcol += 16;
    }

    if (outcol < out_width) {
      src0 = LD_SB(inptr);
      tmp0_r = (v8i16)__msa_hadd_u_h((v16u8)src0, (v16u8)src0);
      tmp0_r = tmp0_r + bias;
      tmp0_r = MSA_SRAI_H(tmp0_r, 1);
      dst0 = __msa_pckev_b((v16i8)tmp0_r, (v16i8)tmp0_r);

      ST8x1_UB(dst0, outptr);
    }

    outptr = output_data[outrow] + (image_width >> 1);
    end_pixel = *(input_data[outrow] + image_width - 1);

    if (image_width & 0x2) {
      *outptr++ = end_pixel;
    }

    tmp_ptr = (unsigned short *)outptr;

    for (outcol = (image_width >> 1); outcol < out_width; outcol += 2) {
      *tmp_ptr++ = (end_pixel << 8) + end_pixel;
    }
  }
}

GLOBAL(void)
jsimd_h2v2_downsample_msa(JDIMENSION image_width, int max_v_samp_factor,
                          JDIMENSION v_samp_factor, JDIMENSION width_blocks,
                          JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow = 0, outrow, end_pixel0, end_pixel1, sum, outcol;
  JSAMPROW inptr0, inptr1, outptr;
  unsigned short *tmp_ptr;
  v16i8 src0, src1, src2, src3, src4, src5, src6, src7, dst0, dst1;
  v16i8 src8, src9, src10, src11, src12, src13, src14, src15, dst2, dst3;
  v8i16 tmp0_r, tmp0_l, tmp1_r, tmp1_l, tmp2_r, tmp2_l, tmp3_r, tmp3_l;
  const v8i16 bias = { 1, 2, 1, 2, 1, 2, 1, 2 };
  JDIMENSION out_width = width_blocks * 8;

  for (outrow = 0; outrow < v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];

    outcol = 0;

    for (; outcol < (out_width & ~0x3F); outcol += 64) {
      LD_SB8(inptr0, 16, src0, src1, src2, src3, src4, src5, src6, src7);
      LD_SB8(inptr1, 16, src8, src9, src10, src11, src12, src13, src14, src15);
      inptr0 += 128;
      inptr1 += 128;

      HADD_UB4_SH(src0, src1, src2, src3, tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      HADD_UB4_SH(src8, src9, src10, src11, tmp2_r, tmp2_l, tmp3_r, tmp3_l);
      ADD4(tmp0_r, tmp2_r, tmp0_l, tmp2_l, tmp1_r, tmp3_r, tmp1_l, tmp3_l,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      ADD4(tmp0_r, bias, tmp0_l, bias, tmp1_r, bias, tmp1_l, bias,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      SRAI_H4_SH(tmp0_r, tmp0_l, tmp1_r, tmp1_l, 2);
      PCKEV_B2_SB(tmp0_l, tmp0_r, tmp1_l, tmp1_r, dst0, dst1);

      HADD_UB4_SH(src4, src5, src6, src7, tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      HADD_UB4_SH(src12, src13, src14, src15, tmp2_r, tmp2_l, tmp3_r, tmp3_l);
      ADD4(tmp0_r, tmp2_r, tmp0_l, tmp2_l, tmp1_r, tmp3_r, tmp1_l, tmp3_l,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      ADD4(tmp0_r, bias, tmp0_l, bias, tmp1_r, bias, tmp1_l, bias,
           tmp0_r, tmp0_l, tmp1_r, tmp1_l);
      SRAI_H4_SH(tmp0_r, tmp0_l, tmp1_r, tmp1_l, 2);
      PCKEV_B2_SB(tmp0_l, tmp0_r, tmp1_l, tmp1_r, dst2, dst3);

      ST_SB4(dst0, dst1, dst2, dst3, outptr, 16);
      outptr += 64;
    }

    if (outcol < (out_width & ~0x1F)) {
      LD_SB4(inptr0, 16, src0, src1, src4, src5);
      LD_SB4(inptr1, 16, src2, src3, src6, src7);
      inptr0 += 64;
      inptr1 += 64;

      HADD_UB2_SH(src0, src1, tmp0_r, tmp0_l);
      HADD_UB2_SH(src2, src3, tmp1_r, tmp1_l);
      ADD2(tmp0_r, tmp1_r, tmp0_l, tmp1_l, tmp0_r, tmp0_l);
      ADD2(tmp0_r, bias, tmp0_l, bias, tmp0_r, tmp0_l);
      SRAI_H2_SH(tmp0_r, tmp0_l, 2);
      dst0 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);

      HADD_UB2_SH(src4, src5, tmp0_r, tmp0_l);
      HADD_UB2_SH(src6, src7, tmp1_r, tmp1_l);
      ADD2(tmp0_r, tmp1_r, tmp0_l, tmp1_l, tmp0_r, tmp0_l);
      ADD2(tmp0_r, bias, tmp0_l, bias, tmp0_r, tmp0_l);
      SRAI_H2_SH(tmp0_r, tmp0_l, 2);
      dst1 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);

      ST_SB2(dst0, dst1, outptr, 16);
      outptr += 32;
      outcol += 32;
    }

    if (outcol < (out_width & ~0xF)) {
      LD_SB2(inptr0, 16, src0, src1);
      LD_SB2(inptr1, 16, src2, src3);
      inptr0 += 32;
      inptr1 += 32;

      HADD_UB2_SH(src0, src1, tmp0_r, tmp0_l);
      HADD_UB2_SH(src2, src3, tmp1_r, tmp1_l);
      ADD2(tmp0_r, tmp1_r, tmp0_l, tmp1_l, tmp0_r, tmp0_l);
      ADD2(tmp0_r, bias, tmp0_l, bias, tmp0_r, tmp0_l);
      SRAI_H2_SH(tmp0_r, tmp0_l, 2);
      dst0 = __msa_pckev_b((v16i8)tmp0_l, (v16i8)tmp0_r);

      ST_SB(dst0, outptr);
      outptr += 16;
      outcol += 16;
    }

    if (outcol < out_width) {
      src0 = LD_SB(inptr0);
      src2 = LD_SB(inptr1);

      HADD_UB2_SH(src0, src2, tmp0_r, tmp1_r);
      tmp0_r += tmp1_r;
      tmp0_r = tmp0_r + bias;
      tmp0_r = MSA_SRAI_H(tmp0_r, 2);
      dst0 = __msa_pckev_b((v16i8)tmp0_r, (v16i8)tmp0_r);

      ST8x1_UB(dst0, outptr);
    }

    outptr = output_data[outrow] + (image_width >> 1);
    end_pixel0 = (int)(*(input_data[inrow] + image_width - 1));
    end_pixel1 = (int)(*(input_data[inrow + 1] + image_width - 1));
    sum = end_pixel0 + end_pixel1;

    if (image_width & 0x2) {
      *outptr++ = (unsigned char)((sum + 1) >> 1);
    }

    tmp_ptr = (unsigned short *)outptr;

    for (outcol = (image_width >> 1); outcol < out_width; outcol += 2) {
      *tmp_ptr++ = (((sum + 1) >> 1) << 8) + (sum >> 1);
    }

    inrow += 2;
  }
}
