/*
 * Copyright (C) 2018 Arm Limited. All Rights Reserved.
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

#include "jsimd_neon.h"

// The diagram below shows a row of samples in an H2V1-fancy-downsampled image.
//
//                 s0        s1        s2
//            +---------+---------+---------+
//            |         |         |         |
//            | p0   p1 | p2   p3 | p4   p5 |
//            |         |         |         |
//            +---------+---------+---------+
//
// Each sample contains two pixel centres in the original image. These pixels
// are centred at positions p0, p1, p2, p3, p4 and p5 above. To compute the
// pixel values of the original image, we proportionally blend the adjacent
// samples in each row.
//
// There are three cases to consider:
//
// 1) The first pixel in the original image.
//    Pixel 'p0' contains only a component from s0, so we set p0 = s0.
// 2) The last pixel in the original image.
//    Pixel 'p5' contains only a component from sample s2, so we set p5 = s2.
// 3) General case (all other pixels in the row).
//    Apart from the first and last pixels, every other pixel in the row is
//    computed by blending the containing sample and the nearest neigbouring
//    sample in the ratio 3:1.
//    For example, the pixel centred at 'p1' would be computed as follows:
//        3/4 * s0 + 1/4 * s1
//    while the pixel centred at 'p2' would be:
//        3/4 * s1 + 1/4 * s0

void jsimd_h2v1_fancy_upsample_neon(int max_v_samp_factor,
                                    JDIMENSION downsampled_width,
                                    JSAMPARRAY input_data,
                                    JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr, outptr;
  int colctr;
  int inrow;
  // Setup constants.
  uint16x8_t one_u16 = vdupq_n_u16(1);
  uint8x8_t three_u8 = vdup_n_u8(3);

  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data[inrow];
    // Case 1: first pixel in this row of the original image.
    *outptr++ = (JSAMPLE) GETJSAMPLE(*inptr++);

    colctr = downsampled_width - 1;
    for (; colctr >= 16; colctr -= 16) {
      // General case:
      //    3/4 * containing sample + 1/4 * nearest neighbouring sample
      // For p1: containing sample = s0, nearest neighbouring sample = s1.
      // For p2: containing sample = s1, nearest neighbouring sample = s0.
      uint8x16_t s0 = vld1q_u8(inptr-1);
      uint8x16_t s1 = vld1q_u8(inptr);
      // Multiplication makes vectors twice as wide: '_l' and '_h' suffixes
      // denote low half and high half respectively.
      uint16x8_t s1_add_3s0_l = vmlal_u8(vmovl_u8(vget_low_u8(s1)),
                                         vget_low_u8(s0), three_u8);
      uint16x8_t s1_add_3s0_h = vmlal_u8(vmovl_u8(vget_high_u8(s1)),
                                         vget_high_u8(s0), three_u8);
      uint16x8_t s0_add_3s1_l = vmlal_u8(vmovl_u8(vget_low_u8(s0)),
                                         vget_low_u8(s1), three_u8);
      uint16x8_t s0_add_3s1_h = vmlal_u8(vmovl_u8(vget_high_u8(s0)),
                                         vget_high_u8(s1), three_u8);
      s0_add_3s1_l = vaddq_u16(s0_add_3s1_l, one_u16);
      s0_add_3s1_h = vaddq_u16(s0_add_3s1_h, one_u16);
      // Right-shift by 2 (divide by 4), narrow to uint8_t and combine.
      uint8x16_t p1 = vcombine_u8(vrshrn_n_u16(s1_add_3s0_l, 2),
                                  vrshrn_n_u16(s1_add_3s0_h, 2));
      uint8x16_t p2 = vcombine_u8(vshrn_n_u16(s0_add_3s1_l, 2),
                                  vshrn_n_u16(s0_add_3s1_h, 2));
      uint8x16x2_t output_pixels = {p1, p2};
      // Store pixel values and increment pointers.
      vst2q_u8(outptr, output_pixels);
      inptr += 16;
      outptr += 32;
    }

    // Handle the tail elements - still the general case.
    if (colctr > 0) {
      int shift = 16 - colctr;
      uint8x16_t s0 = vld1q_u8((inptr - shift) - 1);
      uint8x16_t s1 = vld1q_u8(inptr - shift);
      // Multiplication makes vectors twice as wide: '_l' and '_h' suffixes
      // denote low half and high half respectively.
      uint16x8_t s1_add_3s0_l = vmlal_u8(vmovl_u8(vget_low_u8(s1)),
                                         vget_low_u8(s0), three_u8);
      uint16x8_t s1_add_3s0_h = vmlal_u8(vmovl_u8(vget_high_u8(s1)),
                                         vget_high_u8(s0), three_u8);
      uint16x8_t s0_add_3s1_l = vmlal_u8(vmovl_u8(vget_low_u8(s0)),
                                         vget_low_u8(s1), three_u8);
      uint16x8_t s0_add_3s1_h = vmlal_u8(vmovl_u8(vget_high_u8(s0)),
                                         vget_high_u8(s1), three_u8);
      s0_add_3s1_l = vaddq_u16(s0_add_3s1_l, one_u16);
      s0_add_3s1_h = vaddq_u16(s0_add_3s1_h, one_u16);
      // Right-shift by 2 (divide by 4), narrow to uint8_t and combine.
      uint8x16_t p1 = vcombine_u8(vrshrn_n_u16(s1_add_3s0_l, 2),
                                  vrshrn_n_u16(s1_add_3s0_h, 2));
      uint8x16_t p2 = vcombine_u8(vshrn_n_u16(s0_add_3s1_l, 2),
                                  vshrn_n_u16(s0_add_3s1_h, 2));
      uint8x8x2_t output_pixels_l = {vget_low_u8(p1), vget_low_u8(p2)};
      uint8x8x2_t output_pixels_h = {vget_high_u8(p1), vget_high_u8(p2)};

      // Store pixel values and increment pointers.
      switch (colctr) {
      case 15:
        vst2_lane_u8(outptr, output_pixels_l, 1);
        outptr += 2;
      case 14:
        vst2_lane_u8(outptr, output_pixels_l, 2);
        outptr += 2;
      case 13:
        vst2_lane_u8(outptr, output_pixels_l, 3);
        outptr += 2;
      case 12:
        vst2_lane_u8(outptr, output_pixels_l, 4);
        outptr += 2;
      case 11:
        vst2_lane_u8(outptr, output_pixels_l, 5);
        outptr += 2;
      case 10:
        vst2_lane_u8(outptr, output_pixels_l, 6);
        outptr += 2;
      case 9:
        vst2_lane_u8(outptr, output_pixels_l, 7);
        outptr += 2;
      case 8:
        vst2_u8(outptr, output_pixels_h);
        outptr += 16;
        break;
      case 7:
        vst2_lane_u8(outptr, output_pixels_h, 1);
        outptr += 2;
      case 6:
        vst2_lane_u8(outptr, output_pixels_h, 2);
        outptr += 2;
      case 5:
        vst2_lane_u8(outptr, output_pixels_h, 3);
        outptr += 2;
      case 4:
        vst2_lane_u8(outptr, output_pixels_h, 4);
        outptr += 2;
      case 3:
        vst2_lane_u8(outptr, output_pixels_h, 5);
        outptr += 2;
      case 2:
        vst2_lane_u8(outptr, output_pixels_h, 6);
        outptr += 2;
      case 1:
        vst2_lane_u8(outptr, output_pixels_h, 7);
        outptr += 2;
      default:
        break;
      }
      inptr += colctr;
    }

    // Case 2: last pixel in this row of the original image.
    *outptr++ = GETJSAMPLE(inptr[-1]);
  }
}
