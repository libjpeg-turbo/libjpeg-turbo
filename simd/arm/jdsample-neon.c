/*
 * jdsample-neon.c - upsampling (Arm Neon)
 *
 * Copyright (C) 2020 Arm Limited. All Rights Reserved.
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
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"

#include <arm_neon.h>

/*
 * The diagram below shows a row of samples (luma or chroma) produced by h2v1
 * downsampling.
 *
 *                 s0        s1        s2
 *            +---------+---------+---------+
 *            |         |         |         |
 *            | p0   p1 | p2   p3 | p4   p5 |
 *            |         |         |         |
 *            +---------+---------+---------+
 *
 * Each sample contains two of the original pixel channel values. These pixel
 * channel values are centred at positions p0, p1, p2, p3, p4 and p5 above. To
 * compute the channel values of the original image, we proportionally blend
 * the adjacent samples in each row.
 *
 * There are three cases to consider:
 *
 * 1) The first pixel in the original image.
 *    Pixel channel value p0 contains only a component from sample s0, so we
 *    set p0 = s0.
 * 2) The last pixel in the original image.
 *    Pixel channel value p5 contains only a component from sample s2, so we
 *    set p5 = s2.
 * 3) General case (all other pixels in the row).
 *    Apart from the first and last pixels, every other pixel channel value is
 *    computed by blending the containing sample and the nearest neigbouring
 *    sample in the ratio 3:1.
 *    For example, the pixel channel value centred at p1 would be computed as
 *    follows:
 *        3/4 * s0 + 1/4 * s1
 *    while the pixel channel value centred at p2 would be:
 *        3/4 * s1 + 1/4 * s0
 */

void jsimd_h2v1_fancy_upsample_neon(int max_v_samp_factor,
                                    JDIMENSION downsampled_width,
                                    JSAMPARRAY input_data,
                                    JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr, outptr;
  /* Setup constants. */
  const uint16x8_t one_u16 = vdupq_n_u16(1);
  const uint8x8_t three_u8 = vdup_n_u8(3);

  for (int inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data[inrow];
    /* Case 1: first pixel channel value in this row of the original image. */
    *outptr = (JSAMPLE)GETJSAMPLE(*inptr);

    /* General case: */
    /*    3/4 * containing sample + 1/4 * nearest neighboring sample */
    /* For p1: containing sample = s0, nearest neighboring sample = s1. */
    /* For p2: containing sample = s1, nearest neighboring sample = s0. */
    uint8x16_t s0 = vld1q_u8(inptr);
    uint8x16_t s1 = vld1q_u8(inptr + 1);
    /* Multiplication makes vectors twice as wide: '_l' and '_h' suffixes */
    /* denote low half and high half respectively. */
    uint16x8_t s1_add_3s0_l = vmlal_u8(vmovl_u8(vget_low_u8(s1)),
                                       vget_low_u8(s0), three_u8);
    uint16x8_t s1_add_3s0_h = vmlal_u8(vmovl_u8(vget_high_u8(s1)),
                                       vget_high_u8(s0), three_u8);
    uint16x8_t s0_add_3s1_l = vmlal_u8(vmovl_u8(vget_low_u8(s0)),
                                       vget_low_u8(s1), three_u8);
    uint16x8_t s0_add_3s1_h = vmlal_u8(vmovl_u8(vget_high_u8(s0)),
                                       vget_high_u8(s1), three_u8);
    /* Add ordered dithering bias to odd pixel values. */
    s0_add_3s1_l = vaddq_u16(s0_add_3s1_l, one_u16);
    s0_add_3s1_h = vaddq_u16(s0_add_3s1_h, one_u16);

    /* Initially 1 - due to having already stored the first pixel of the */
    /* image. However, in subsequent iterations of the SIMD loop this offset */
    /* is (2 * colctr - 1) to stay within the bounds of the sample buffers */
    /* without having to resort to a slow scalar tail case for the last */
    /* (downsampled_width % 16) samples. See "Creation of 2-D sample arrays" */
    /* in jmemmgr.c for details. */
    unsigned outptr_offset = 1;
    uint8x16x2_t output_pixels;

    /* We use software pipelining to maximise performance. The code indented */
    /* an extra 6 spaces begins the next iteration of the loop. */
    for (unsigned colctr = 16; colctr < downsampled_width; colctr += 16) {
            s0 = vld1q_u8(inptr + colctr - 1);
            s1 = vld1q_u8(inptr + colctr);
      /* Right-shift by 2 (divide by 4), narrow to 8-bit and combine. */
      output_pixels.val[0] = vcombine_u8(vrshrn_n_u16(s1_add_3s0_l, 2),
                                         vrshrn_n_u16(s1_add_3s0_h, 2));
      output_pixels.val[1] = vcombine_u8(vshrn_n_u16(s0_add_3s1_l, 2),
                                         vshrn_n_u16(s0_add_3s1_h, 2));
            /* Multiplication makes vectors twice as wide: '_l' and '_h' */
            /* suffixes denote low half and high half respectively. */
            s1_add_3s0_l = vmlal_u8(vmovl_u8(vget_low_u8(s1)),
                                             vget_low_u8(s0), three_u8);
            s1_add_3s0_h = vmlal_u8(vmovl_u8(vget_high_u8(s1)),
                                             vget_high_u8(s0), three_u8);
            s0_add_3s1_l = vmlal_u8(vmovl_u8(vget_low_u8(s0)),
                                             vget_low_u8(s1), three_u8);
            s0_add_3s1_h = vmlal_u8(vmovl_u8(vget_high_u8(s0)),
                                             vget_high_u8(s1), three_u8);
            /* Add ordered dithering bias to odd pixel values. */
            s0_add_3s1_l = vaddq_u16(s0_add_3s1_l, one_u16);
            s0_add_3s1_h = vaddq_u16(s0_add_3s1_h, one_u16);
      /* Store pixel channel values to memory. */
      vst2q_u8(outptr + outptr_offset, output_pixels);
      outptr_offset = 2 * colctr - 1;
    }

    /* Complete the last iteration of the loop. */
    /* Right-shift by 2 (divide by 4), narrow to 8-bit and combine. */
    output_pixels.val[0] = vcombine_u8(vrshrn_n_u16(s1_add_3s0_l, 2),
                                       vrshrn_n_u16(s1_add_3s0_h, 2));
    output_pixels.val[1] = vcombine_u8(vshrn_n_u16(s0_add_3s1_l, 2),
                                       vshrn_n_u16(s0_add_3s1_h, 2));
    /* Store pixel channel values to memory. */
    vst2q_u8(outptr + outptr_offset, output_pixels);

    /* Case 2: last pixel channel value in this row of the original image. */
    outptr[2 * downsampled_width - 1] =
          GETJSAMPLE(inptr[downsampled_width - 1]);
  }
}
