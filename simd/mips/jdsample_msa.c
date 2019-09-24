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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "jmacros_msa.h"


/*
 * Fancy processing for the common case of 2:1 horizontal and 1:1 vertical.
 *
 * The centers of the output pixels are 1/4 and 3/4 of the way between input
 * pixel centers.
 *
 * height = 1 to 4, width = multiple of 16
*/
GLOBAL(void)
jsimd_h2v1_fancy_upsample_msa(int max_v_samp_factor,
                              JDIMENSION downsampled_width,
                              JSAMPARRAY input_data,
                              JSAMPARRAY *output_data_ptr)
{
  JDIMENSION col, row, width = downsampled_width;
  JSAMPARRAY dst = *output_data_ptr;
  v8i16 left_r, left_l, cur_r, cur_l, right_r, right_l, zero = { 0 };
  v16i8 out0, out1, src2, src0, src1, left, right;

  for (row = 0; row < max_v_samp_factor; row++) {
    /* Prepare first 16 width column */
    src1 = LD_SB(input_data[row]);
    src2 = LD_SB(input_data[row] + 16);
    src0 = __msa_splati_b(src1, 0);

    for (col = 0; col < width; col += 16) {
      left = __msa_sldi_b(src1, src0, 15); /* -1 0 1 2 ... 12 13 14 */
      right = __msa_sldi_b(src2, src1, 1); /* 1 2 3 4 ... 14 15 16 */

      /* Promote data to 16 bit */
      ILVRL_B2_SH(zero, left, left_r, left_l);
      ILVRL_B2_SH(zero, right, right_r, right_l);
      ILVRL_B2_SH(zero, src1, cur_r, cur_l);

      cur_l += MSA_SLLI_H(cur_l, 1);
      cur_r += MSA_SLLI_H(cur_r, 1);

      left_l += MSA_ADDVI_H(cur_l, 1);
      left_r += MSA_ADDVI_H(cur_r, 1);

      right_l += MSA_ADDVI_H(cur_l, 2);
      right_r += MSA_ADDVI_H(cur_r, 2);

      SRAI_H4_SH(left_r, left_l, right_r, right_l, 2);
      PCKEV_B2_SB(left_l, left_r, right_l, right_r, left, right);
      ILVRL_B2_SB(right, left, out0, out1);

      ST_SB2(out0, out1, dst[row] + (col << 1), 16);

      /* Prepare for next 16 width column */
      src0 = src1;
      src1 = src2;
      src2 = LD_SB(input_data[row] + col + 32);
    }

    /* Re-substitute last pixel */
    *(dst[row] + 2 * width - 1) = *(input_data[row] + width - 1);
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
jsimd_h2v2_fancy_upsample_msa(int max_v_samp_factor,
                              JDIMENSION downsampled_width,
                              JSAMPARRAY input_data,
                              JSAMPARRAY *output_data_ptr)
{
  JDIMENSION col, width = downsampled_width;
  JSAMPARRAY dst = *output_data_ptr;
  int inrow = 0, outrow = 0;
  JSAMPROW inptr0, inptr1, inptr2, outptr0, outptr1;
  v8i16 lastcolsum0_r, lastcolsum0_l, lastcolsum1_r, lastcolsum1_l;
  v8i16 thiscolsum0_l, thiscolsum0_r, thiscolsum1_r, thiscolsum1_l;
  v8i16 nextcolsum0_r, nextcolsum0_l, nextcolsum1_r, nextcolsum1_l;
  v8i16 dst00_r, dst00_l, dst01_r, dst01_l;
  v8i16 dst10_r, dst10_l, dst11_r, dst11_l;
  v8i16 cur0_r, cur1_r, cur2_r, cur0_l, cur1_l, cur2_l;
  v8i16 const_3 = __msa_ldi_h(3);
  v16i8 tmp0_l, tmp1_l, tmp2_l, tmp0_r, tmp1_r, tmp2_r, zero = { 0 };
  v16i8 left0, left1, left2, cur0, cur1, cur2, right0, right1, right2;
  v16i8 dst00, dst01, dst10, dst11, out00, out01, out10, out11;

  while (outrow < max_v_samp_factor) {
    /* Current row; Nearest to output pixel */
    inptr0 = input_data[inrow];

    /* Above row */
    inptr1 = input_data[inrow - 1];
    /* Below row */
    inptr2 = input_data[inrow + 1];

    /* Output pointers */
    outptr0 = dst[outrow];
    outptr1 = dst[outrow + 1];

    cur0 = LD_SB(inptr0);
    cur1 = LD_SB(inptr1);
    cur2 = LD_SB(inptr2);
    tmp0_r = LD_SB(inptr0 + 16);
    tmp1_r = LD_SB(inptr1 + 16);
    tmp2_r = LD_SB(inptr2 + 16);
    tmp0_l = __msa_splati_b(cur0, 0);
    tmp1_l = __msa_splati_b(cur1, 0);
    tmp2_l = __msa_splati_b(cur2, 0);

    for (col = 0; col < width; col += 16) {
      left0 = __msa_sldi_b(cur0, tmp0_l, 15); /* -1 0 1 2 ... 12 13 14 */
      left1 = __msa_sldi_b(cur1, tmp1_l, 15); /* -1 0 1 2 ... 12 13 14 */
      left2 = __msa_sldi_b(cur2, tmp2_l, 15); /* -1 0 1 2 ... 12 13 14 */
      right0 = __msa_sldi_b(tmp0_r, cur0, 1); /* 1 2 3 4 ... 14 15 16 */
      right1 = __msa_sldi_b(tmp1_r, cur1, 1); /* 1 2 3 4 ... 14 15 16 */
      right2 = __msa_sldi_b(tmp2_r, cur2, 1); /* 1 2 3 4 ... 14 15 16 */

      ILVRL_B2_SH(zero, left0, cur0_r, cur0_l);
      ILVRL_B2_SH(zero, left1, cur1_r, cur1_l);
      ILVRL_B2_SH(zero, left2, cur2_r, cur2_l);
      cur0_r *= const_3;
      cur0_l *= const_3;
      lastcolsum0_r = cur0_r + cur1_r;
      lastcolsum1_r = cur0_r + cur2_r;
      lastcolsum0_l = cur0_l + cur1_l;
      lastcolsum1_l = cur0_l + cur2_l;

      ILVRL_B2_SH(zero, cur0, cur0_r, cur0_l);
      ILVRL_B2_SH(zero, cur1, cur1_r, cur1_l);
      ILVRL_B2_SH(zero, cur2, cur2_r, cur2_l);
      cur0_r *= const_3;
      cur0_l *= const_3;
      thiscolsum0_r = cur0_r + cur1_r;
      thiscolsum1_r = cur0_r + cur2_r;
      thiscolsum0_l = cur0_l + cur1_l;
      thiscolsum1_l = cur0_l + cur2_l;

      ILVRL_B2_SH(zero, right0, cur0_r, cur0_l);
      ILVRL_B2_SH(zero, right1, cur1_r, cur1_l);
      ILVRL_B2_SH(zero, right2, cur2_r, cur2_l);
      cur0_r *= const_3;
      cur0_l *= const_3;
      nextcolsum0_r = cur0_r + cur1_r;
      nextcolsum1_r = cur0_r + cur2_r;
      nextcolsum0_l = cur0_l + cur1_l;
      nextcolsum1_l = cur0_l + cur2_l;

      /* Prepare for next 16 width column */
      tmp0_l = cur0;
      tmp1_l = cur1;
      tmp2_l = cur2;
      cur0 = tmp0_r;
      cur1 = tmp1_r;
      cur2 = tmp2_r;
      tmp0_r = LD_SB(inptr0 + col + 32);
      tmp1_r = LD_SB(inptr1 + col + 32);
      tmp2_r = LD_SB(inptr2 + col + 32);

      /* outptr0 */
      dst00_r = MSA_ADDVI_H(thiscolsum0_r * const_3 + lastcolsum0_r, 8);
      dst00_l = MSA_ADDVI_H(thiscolsum0_l * const_3 + lastcolsum0_l, 8);
      dst01_r = MSA_ADDVI_H(thiscolsum0_r * const_3 + nextcolsum0_r, 7);
      dst01_l = MSA_ADDVI_H(thiscolsum0_l * const_3 + nextcolsum0_l, 7);
      dst00_r = MSA_SRAI_H(dst00_r, 4);
      dst00_l = MSA_SRAI_H(dst00_l, 4);
      dst01_r = MSA_SRAI_H(dst01_r, 4);
      dst01_l = MSA_SRAI_H(dst01_l, 4);

      /* outptr1 */
      dst10_r = MSA_ADDVI_H(thiscolsum1_r * const_3 + lastcolsum1_r, 8);
      dst10_l = MSA_ADDVI_H(thiscolsum1_l * const_3 + lastcolsum1_l, 8);
      dst11_r = MSA_ADDVI_H(thiscolsum1_r * const_3 + nextcolsum1_r, 7);
      dst11_l = MSA_ADDVI_H(thiscolsum1_l * const_3 + nextcolsum1_l, 7);
      dst10_r = MSA_SRAI_H(dst10_r, 4);
      dst10_l = MSA_SRAI_H(dst10_l, 4);
      dst11_r = MSA_SRAI_H(dst11_r, 4);
      dst11_l = MSA_SRAI_H(dst11_l, 4);

      /* Pack */
      PCKEV_B2_SB(dst00_l, dst00_r, dst01_l, dst01_r, dst00, dst01);
      PCKEV_B2_SB(dst10_l, dst10_r, dst11_l, dst11_r, dst10, dst11);

      /* Interleave and Store */
      ILVRL_B2_SB(dst01, dst00, out00, out01);
      ILVRL_B2_SB(dst11, dst10, out10, out11);

      ST_SB2(out00, out01, (outptr0 + 2 * col), 16);
      ST_SB2(out10, out11, (outptr1 + 2 * col), 16);
    }

    /* Special case for last column */
    outptr0[2 * width - 1] = (uint8_t)((((int)(inptr0[width - 1]) * 3 +
                             (int)(inptr1[width - 1])) * 4 + 7) >> 4);
    outptr1[2 * width - 1] = (uint8_t)((((int)(inptr0[width - 1]) * 3 +
                             (int)(inptr2[width - 1])) * 4 + 7) >> 4);

    /* Update input and output pointers */
    inrow++;
    outrow += 2;
  }
}

GLOBAL(void)
jsimd_h2v1_upsample_msa(int max_v_samp_factor,
                        JDIMENSION output_width,
                        JSAMPARRAY input_data,
                        JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr, inptr1, outptr, outptr1, out32end, out64end;
  JDIMENSION inrow, out_width = (output_width + 31) & (~31);
  JSAMPARRAY dst = *output_data_ptr;
  v16u8 src0, src1, src2, src3;
  v16u8 dst0_l, dst0_r, dst1_l, dst1_r, dst2_l, dst2_r, dst3_l, dst3_r;

  inrow = 0;

  /* Height multiple of 2 */
  for (; inrow < (max_v_samp_factor & ~1); inrow += 2) {
    inptr = input_data[inrow];
    inptr1 = input_data[inrow + 1];
    outptr = dst[inrow];
    outptr1 = dst[inrow + 1];
    out32end = outptr + out_width;
    out64end = outptr + ((out_width >> 6) << 6);

    src0 = LD_UB(inptr);
    src2 = LD_UB(inptr + output_width);

    while (outptr < out64end) {
      src1 = LD_UB(inptr + 16);
      src3 = LD_UB(inptr1 + 16);
      ILVRL_B2_UB(src0, src0, dst0_r, dst0_l);
      ILVRL_B2_UB(src1, src1, dst1_r, dst1_l);
      ILVRL_B2_UB(src2, src2, dst2_r, dst2_l);
      ILVRL_B2_UB(src3, src3, dst3_r, dst3_l);
      ST_UB4(dst0_r, dst0_l, dst1_r, dst1_l, outptr, 16);
      ST_UB4(dst2_r, dst2_l, dst3_r, dst3_l, outptr1, 16);
      inptr += 32;
      inptr1 += 32;
      outptr += 64;
      outptr1 += 64;

      src0 = LD_UB(inptr);
      src2 = LD_UB(inptr1);
    }

    if (out32end > out64end) {
      ILVRL_B2_UB(src0, src0, dst0_r, dst0_l);
      ILVRL_B2_UB(src2, src2, dst2_r, dst2_l);
      ST_UB2(dst0_r, dst0_l, outptr, 16);
      ST_UB2(dst2_r, dst2_l, outptr1, 16);
    }
  }

  /* Remaining row */
  if (inrow < max_v_samp_factor) {
    inptr = input_data[inrow];
    outptr = dst[inrow];
    out32end = outptr + out_width;
    out64end = outptr + ((out_width >> 6) << 6);

    src0 = LD_UB(inptr);

    while (outptr < out64end) {
      src1 = LD_UB(inptr + 16);
      ILVRL_B2_UB(src0, src0, dst0_r, dst0_l);
      ILVRL_B2_UB(src1, src1, dst1_r, dst1_l);
      ST_UB4(dst0_r, dst0_l, dst1_r, dst1_l, outptr, 16);
      inptr += 32;
      outptr += 64;

      src0 = LD_UB(inptr);
    }

    if (out32end > out64end) {
      ILVRL_B2_UB(src0, src0, dst0_r, dst0_l);
      ST_UB2(dst0_r, dst0_l, outptr, 16);
    }
  }
}

GLOBAL(void)
jsimd_h2v2_upsample_msa(int max_v_samp_factor,
                        JDIMENSION output_width,
                        JSAMPARRAY input_data,
                        JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr, outptr;
  JDIMENSION out_width = (output_width + 31) & (~31);
  JSAMPARRAY dst = *output_data_ptr;
  JSAMPROW out32end, out64end;
  int inrow, outrow;
  v16u8 src0, src1, dst0_l, dst0_r, dst1_l, dst1_r;

  inrow = outrow = 0;

  while (outrow < max_v_samp_factor) {
    inptr = input_data[inrow];
    outptr = dst[outrow];
    out32end = outptr + out_width;
    out64end = outptr + ((out_width >> 6) << 6);

    src0 = LD_UB(inptr);

    while (outptr < out64end) {
      src1 = LD_UB(inptr + 16);
      ILVRL_B2_UB(src0, src0, dst0_r, dst0_l);
      ILVRL_B2_UB(src1, src1, dst1_r, dst1_l);
      ST_UB4(dst0_r, dst0_l, dst1_r, dst1_l, outptr, 16);
      ST_UB4(dst0_r, dst0_l, dst1_r, dst1_l, outptr + out_width, 16);
      inptr += 32;
      outptr += 64;

      src0 = LD_UB(inptr);
    }

    if (out32end > out64end) {
      ILVRL_B2_UB(src0, src0, dst0_r, dst0_l);
      ST_UB2(dst0_r, dst0_l, outptr, 16);
      ST_UB2(dst0_r, dst0_l, outptr + out_width, 16);
      inptr += 16;
      outptr += 32;
    }

    inrow++;
    outrow += 2;
  }
}
