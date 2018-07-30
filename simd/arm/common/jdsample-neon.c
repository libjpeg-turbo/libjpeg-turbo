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


// The diagram below shows a grid-window of samples in an
// H2V2-fancy-downsampled image.
//
//                  s0        s1
//             +---------+---------+
//             | p0   p1 | p2   p3 |
//     r1      |         |         |
//             | p4   p5 | p6   p7 |
//             +---------+---------+
//             | p8   p9 | p10  p11|
//     r0      |         |         |
//             | p12  p13| p14  p15|
//             +---------+---------+
//
// Every sample contains four pixel centres in the original image. The pixels
// are centred at positions p0, p1, p2,..., p15 above. For a given grid-window
// position, r0 is always used to denote the row of samples containing the
// centre of the pixels whose values we are computing. Likewise, r1 is always
// used to denote the row of nearest neighbouring samples. For the top row of
// pixels in r0 (p8-p11), the nearest neighbouring samples are in the row
// above. In the same way, for the bottom row of pixels in r0 (p12-p15), the
// nearest neighbouring samples are in the row below r0.
//
// To compute the pixel values of the original image, we proportionally blend
// the sample containing the pixel centre with the nearest neighbouring samples
// in each row, column and diagonal.
//
// There are three cases to consider:
//
// 1) The first pixel in this row of the original image.
//    Pixel 'p8' only contains components from sample column s0. Its value is
//    computed by blending samples s0r0 and s0r1 in the ratio 3:1.
// 2) The last pixel in this row of the original image.
//    Pixel 'p11' only contains components from sample column s1. Its value is
//    computed by blending samples s1r0 and s1r1 in the ratio 3:1.
// 3) General case (all other pixels in the row).
//    Apart from the first and last pixels, every other pixel in the row
//    contains components from samples in adjacent columns.
//
//    For example, the pixel centred at 'p9' would be computed as follows:
//        (9/16 * r0s0) + (3/16 * r1s0) + (3/16 * r0s1) + (1/16 * r1s1)
//
//    This can be broken down into two steps:
//    1) Blend samples vertically in columns s0 and s1 in the ratio 3:1:
//        s0colsum = 3/4 * s0r0 + 1/4 * s0r1
//        s1colsum = 3/4 * s1r0 + 1/4 * s1r1
//    2) Blend the already-blended columns in the ratio 3:1:
//        p9 = 3/4 * s0colsum + 1/4 * s1colsum

void jsimd_h2v2_fancy_upsample_neon(int max_v_samp_factor,
                                    JDIMENSION downsampled_width,
                                    JSAMPARRAY input_data,
                                    JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr0, inptr1, outptr;
  int colctr;
  int inrow, outrow, v;
  // Setup constants.
  uint16x8_t seven_u16 = vdupq_n_u16(7);
  uint8x8_t three_u8 = vdup_n_u8(3);
  uint16x8_t three_u16 = vdupq_n_u16(3);

  inrow = outrow = 0;
  while (outrow < max_v_samp_factor) {
    for (v = 0; v < 2; v++) {
      // inptr0 points to row 'r0', inptr1 points to row 'r1'.
      inptr0 = input_data[inrow];
      if (v == 0)               // Nearest neighbouring row is row above.
        inptr1 = input_data[inrow-1];
      else                      // Nearest neighbouring row is row below.
        inptr1 = input_data[inrow+1];
      outptr = output_data[outrow++];

      // Case 1: first pixel in this row of original image.
      int s0colsum = GETJSAMPLE(*inptr0++) * 3 + GETJSAMPLE(*inptr1++);
      *outptr++ = (JSAMPLE) ((s0colsum * 4 + 8) >> 4);

      // General case as described above.
      colctr = downsampled_width - 1;
      for (; colctr >= 16; colctr -= 16) {
        // Step 1: Blend samples vertically in columns s0 and s1.
        // Leave the divide by 4 to the end when it can be done for both
        // dimensions at once, right-shifting by 4.

        // Load and compute 's0colsum'.
        uint8x16_t s0r0 = vld1q_u8(inptr0-1);
        uint8x16_t s0r1 = vld1q_u8(inptr1-1);
        // Multiplication makes vectors twice as wide: '_l' and '_h' suffixes
        // denote low half and high half respectively.
        uint16x8_t s0colsum_l = vmlal_u8(vmovl_u8(vget_low_u8(s0r1)),
                                         vget_low_u8(s0r0), three_u8);
        uint16x8_t s0colsum_h = vmlal_u8(vmovl_u8(vget_high_u8(s0r1)),
                                         vget_high_u8(s0r0), three_u8);
        // Load and compute 's1colsum'.
        uint8x16_t s1r0 = vld1q_u8(inptr0);
        uint8x16_t s1r1 = vld1q_u8(inptr1);
        uint16x8_t s1colsum_l = vmlal_u8(vmovl_u8(vget_low_u8(s1r1)),
                                         vget_low_u8(s1r0), three_u8);
        uint16x8_t s1colsum_h = vmlal_u8(vmovl_u8(vget_high_u8(s1r1)),
                                         vget_high_u8(s1r0), three_u8);
        // Step 2: Blend the already-blended columns.
        uint16x8_t output0_l = vmlaq_u16(s1colsum_l, s0colsum_l, three_u16);
        uint16x8_t output0_h = vmlaq_u16(s1colsum_h, s0colsum_h, three_u16);
        uint16x8_t output1_l = vmlaq_u16(s0colsum_l, s1colsum_l, three_u16);
        uint16x8_t output1_h = vmlaq_u16(s0colsum_h, s1colsum_h, three_u16);
        output0_l = vaddq_u16(output0_l, seven_u16);
        output0_h = vaddq_u16(output0_h, seven_u16);
        // Right-shift by 4 (divide by 16), narrow to uint8_t and combine.
        uint8x16x2_t output_pixels = {vcombine_u8(vshrn_n_u16(output0_l, 4),
                                                  vshrn_n_u16(output0_h, 4)),
                                      vcombine_u8(vrshrn_n_u16(output1_l, 4),
                                                  vrshrn_n_u16(output1_h, 4))};
        // Store upsampled pixels and increment pointers.
        vst2q_u8(outptr, output_pixels);
        inptr0 += 16;
        inptr1 += 16;
        outptr += 32;
      }

      // Handle the tail elements - still the general case.
      if (colctr > 0) {
        int shift = 16 - colctr;
        // Load and compute 's0colsum'.
        uint8x16_t s0r0 = vld1q_u8((inptr0-shift)-1);
        uint8x16_t s0r1 = vld1q_u8((inptr1-shift)-1);
        // Multiplication makes vectors twice as wide: '_l' and '_h' suffixes
        // denote low half and high half respectively.
        uint16x8_t s0colsum_l = vmlal_u8(vmovl_u8(vget_low_u8(s0r1)),
                                         vget_low_u8(s0r0), three_u8);
        uint16x8_t s0colsum_h = vmlal_u8(vmovl_u8(vget_high_u8(s0r1)),
                                         vget_high_u8(s0r0), three_u8);
        // Load and compute 's1colsum'.
        uint8x16_t s1r0 = vld1q_u8(inptr0-shift);
        uint8x16_t s1r1 = vld1q_u8(inptr1-shift);
        uint16x8_t s1colsum_l = vmlal_u8(vmovl_u8(vget_low_u8(s1r1)),
                                         vget_low_u8(s1r0), three_u8);
        uint16x8_t s1colsum_h = vmlal_u8(vmovl_u8(vget_high_u8(s1r1)),
                                         vget_high_u8(s1r0), three_u8);
        // Step 2: Blend the already-blended columns.
        uint16x8_t output0_l = vmlaq_u16(s1colsum_l, s0colsum_l, three_u16);
        uint16x8_t output0_h = vmlaq_u16(s1colsum_h, s0colsum_h, three_u16);
        uint16x8_t output1_l = vmlaq_u16(s0colsum_l, s1colsum_l, three_u16);
        uint16x8_t output1_h = vmlaq_u16(s0colsum_h, s1colsum_h, three_u16);
        output0_l = vaddq_u16(output0_l, seven_u16);
        output0_h = vaddq_u16(output0_h, seven_u16);
        // Right-shift by 4 (divide by 16), narrow to uint8_t and combine.
        uint8x8x2_t output_pixels_l = {vshrn_n_u16(output0_l, 4),
                                       vrshrn_n_u16(output1_l, 4)};
        uint8x8x2_t output_pixels_h = {vshrn_n_u16(output0_h, 4),
                                       vrshrn_n_u16(output1_h, 4)};
        // Store upsampled pixels and increment pointers.
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
        inptr0 += colctr;
        inptr1 += colctr;
      }

      // Case 2: last pixel in this row of the original image.
      int s1colsum = GETJSAMPLE(*(inptr0-1)) * 3 + GETJSAMPLE(*(inptr1-1));
      *outptr++ = (JSAMPLE) ((s1colsum * 4 + 7) >> 4);
    }
    inrow++;
  }
}