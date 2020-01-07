/*
 * jcphuff-neon.c - progressive Huffman encoding (Arm Neon)
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
 * Data preparation for encode_mcu_AC_first().
 *
 * The equivalent scalar C function 'encode_mcu_AC_first_prepare' can be found
 * in jcphuff.c.
 */

void jsimd_encode_mcu_AC_first_prepare_neon(const JCOEF *block,
                                    const int *jpeg_natural_order_start,
                                    int Sl,
                                    int Al,
                                    JCOEF *values,
                                    size_t *zerobits)
{
  JCOEF *values_ptr = values;
  JCOEF *diff_values_ptr = values + DCTSIZE2;

  /* Rows of coefficients to zero (since they haven't been processed.) */
  int rows_to_zero = 8;

  for (int i = 0; i < Sl / 16; i++) {
    int16x8_t coefs1 = vld1q_dup_s16(block + jpeg_natural_order_start[0]);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[1], coefs1, 1);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[2], coefs1, 2);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[3], coefs1, 3);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[4], coefs1, 4);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[5], coefs1, 5);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[6], coefs1, 6);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[7], coefs1, 7);
    int16x8_t coefs2 = vld1q_dup_s16(block + jpeg_natural_order_start[8]);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[9], coefs2, 1);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[10], coefs2, 2);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[11], coefs2, 3);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[12], coefs2, 4);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[13], coefs2, 5);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[14], coefs2, 6);
    coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[15], coefs2, 7);

    /* Isolate sign of coefficients. */
    int16x8_t sign_coefs1 = vshrq_n_s16(coefs1, 15);
    int16x8_t sign_coefs2 = vshrq_n_s16(coefs2, 15);
    /* Compute absolute value of coefficients and apply point transform Al. */
    int16x8_t abs_coefs1 = vabsq_s16(coefs1);
    int16x8_t abs_coefs2 = vabsq_s16(coefs2);
    coefs1 = vshlq_s16(abs_coefs1, vdupq_n_s16(-Al));
    coefs2 = vshlq_s16(abs_coefs2, vdupq_n_s16(-Al));

    /* Compute diff values. */
    int16x8_t diff1 = veorq_s16(coefs1, sign_coefs1);
    int16x8_t diff2 = veorq_s16(coefs2, sign_coefs2);

    /* Store transformed coefficients and diff values. */
    vst1q_s16(values_ptr, coefs1);
    vst1q_s16(values_ptr + DCTSIZE, coefs2);
    vst1q_s16(diff_values_ptr, diff1);
    vst1q_s16(diff_values_ptr + DCTSIZE, diff2);
    values_ptr += 16;
    diff_values_ptr += 16;
    jpeg_natural_order_start += 16;
    rows_to_zero -= 2;
  }

  /* Same operation but for remaining partial vector. */
  int remaining_coefs = Sl % 16;
  if (remaining_coefs > 8) {
    int16x8_t coefs1 = vld1q_dup_s16(block + jpeg_natural_order_start[0]);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[1], coefs1, 1);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[2], coefs1, 2);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[3], coefs1, 3);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[4], coefs1, 4);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[5], coefs1, 5);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[6], coefs1, 6);
    coefs1 = vld1q_lane_s16(block + jpeg_natural_order_start[7], coefs1, 7);
    int16x8_t coefs2 = vdupq_n_s16(0);
    switch (remaining_coefs) {
    case 15 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[14], coefs2, 6);
    case 14 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[13], coefs2, 5);
    case 13 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[12], coefs2, 4);
    case 12 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[11], coefs2, 3);
    case 11 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[10], coefs2, 2);
    case 10 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[9], coefs2, 1);
    case 9 :
      coefs2 = vld1q_lane_s16(block + jpeg_natural_order_start[8], coefs2, 0);
    default:
      break;
    }

    /* Isolate sign of coefficients. */
    int16x8_t sign_coefs1 = vshrq_n_s16(coefs1, 15);
    int16x8_t sign_coefs2 = vshrq_n_s16(coefs2, 15);
    /* Compute absolute value of coefficients and apply point transform Al. */
    int16x8_t abs_coefs1 = vabsq_s16(coefs1);
    int16x8_t abs_coefs2 = vabsq_s16(coefs2);
    coefs1 = vshlq_s16(abs_coefs1, vdupq_n_s16(-Al));
    coefs2 = vshlq_s16(abs_coefs2, vdupq_n_s16(-Al));

    /* Compute diff values. */
    int16x8_t diff1 = veorq_s16(coefs1, sign_coefs1);
    int16x8_t diff2 = veorq_s16(coefs2, sign_coefs2);

    /* Store transformed coefficients and diff values. */
    vst1q_s16(values_ptr, coefs1);
    vst1q_s16(values_ptr + DCTSIZE, coefs2);
    vst1q_s16(diff_values_ptr, diff1);
    vst1q_s16(diff_values_ptr + DCTSIZE, diff2);
    values_ptr += 16;
    diff_values_ptr += 16;
    rows_to_zero -= 2;

  } else if (remaining_coefs > 0) {
    int16x8_t coefs = vdupq_n_s16(0);

    switch (remaining_coefs) {
    case 8 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[7], coefs, 7);
    case 7 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[6], coefs, 6);
    case 6 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[5], coefs, 5);
    case 5 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[4], coefs, 4);
    case 4 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[3], coefs, 3);
    case 3 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[2], coefs, 2);
    case 2 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[1], coefs, 1);
    case 1 :
      coefs = vld1q_lane_s16(block + jpeg_natural_order_start[0], coefs, 0);
    default:
      break;
    }

    /* Isolate sign of coefficients. */
    int16x8_t sign_coefs = vshrq_n_s16(coefs, 15);
    /* Compute absolute value of coefficients and apply point transform Al. */
    int16x8_t abs_coefs = vabsq_s16(coefs);
    coefs = vshlq_s16(abs_coefs, vdupq_n_s16(-Al));

    /* Compute diff values. */
    int16x8_t diff = veorq_s16(coefs, sign_coefs);

    /* Store transformed coefficients and diff values. */
    vst1q_s16(values_ptr, coefs);
    vst1q_s16(diff_values_ptr, diff);
    values_ptr += 8;
    diff_values_ptr += 8;
    rows_to_zero--;
  }

  /* Zero remaining memory in the values and diff_values blocks. */
  for (int i = 0; i < rows_to_zero; i++) {
    vst1q_s16(values_ptr, vdupq_n_s16(0));
    vst1q_s16(diff_values_ptr, vdupq_n_s16(0));
    values_ptr += 8;
    diff_values_ptr += 8;
  }

  /* Construct zerobits bitmap. */
  /* A set bit denotes the corresponding coefficient != 0. */
  int16x8_t row0 = vld1q_s16(values + 0 * DCTSIZE);
  int16x8_t row1 = vld1q_s16(values + 1 * DCTSIZE);
  int16x8_t row2 = vld1q_s16(values + 2 * DCTSIZE);
  int16x8_t row3 = vld1q_s16(values + 3 * DCTSIZE);
  int16x8_t row4 = vld1q_s16(values + 4 * DCTSIZE);
  int16x8_t row5 = vld1q_s16(values + 5 * DCTSIZE);
  int16x8_t row6 = vld1q_s16(values + 6 * DCTSIZE);
  int16x8_t row7 = vld1q_s16(values + 7 * DCTSIZE);

  uint8x8_t row0_eq0 = vmovn_u16(vceqq_s16(row0, vdupq_n_s16(0)));
  uint8x8_t row1_eq0 = vmovn_u16(vceqq_s16(row1, vdupq_n_s16(0)));
  uint8x8_t row2_eq0 = vmovn_u16(vceqq_s16(row2, vdupq_n_s16(0)));
  uint8x8_t row3_eq0 = vmovn_u16(vceqq_s16(row3, vdupq_n_s16(0)));
  uint8x8_t row4_eq0 = vmovn_u16(vceqq_s16(row4, vdupq_n_s16(0)));
  uint8x8_t row5_eq0 = vmovn_u16(vceqq_s16(row5, vdupq_n_s16(0)));
  uint8x8_t row6_eq0 = vmovn_u16(vceqq_s16(row6, vdupq_n_s16(0)));
  uint8x8_t row7_eq0 = vmovn_u16(vceqq_s16(row7, vdupq_n_s16(0)));

  const uint8x8_t bitmap_mask = { 0x01, 0x02, 0x04, 0x08,
                                  0x10, 0x20, 0x40, 0x80
                                };

  row0_eq0 = vand_u8(row0_eq0, bitmap_mask);
  row1_eq0 = vand_u8(row1_eq0, bitmap_mask);
  row2_eq0 = vand_u8(row2_eq0, bitmap_mask);
  row3_eq0 = vand_u8(row3_eq0, bitmap_mask);
  row4_eq0 = vand_u8(row4_eq0, bitmap_mask);
  row5_eq0 = vand_u8(row5_eq0, bitmap_mask);
  row6_eq0 = vand_u8(row6_eq0, bitmap_mask);
  row7_eq0 = vand_u8(row7_eq0, bitmap_mask);

  uint8x8_t bitmap_rows_01 = vpadd_u8(row0_eq0, row1_eq0);
  uint8x8_t bitmap_rows_23 = vpadd_u8(row2_eq0, row3_eq0);
  uint8x8_t bitmap_rows_45 = vpadd_u8(row4_eq0, row5_eq0);
  uint8x8_t bitmap_rows_67 = vpadd_u8(row6_eq0, row7_eq0);
  uint8x8_t bitmap_rows_0123 = vpadd_u8(bitmap_rows_01, bitmap_rows_23);
  uint8x8_t bitmap_rows_4567 = vpadd_u8(bitmap_rows_45, bitmap_rows_67);
  uint8x8_t bitmap_all = vpadd_u8(bitmap_rows_0123, bitmap_rows_4567);

#if defined(__aarch64__)
  /* Move bitmap to 64-bit scalar register. */
  uint64_t bitmap = vget_lane_u64(vreinterpret_u64_u8(bitmap_all), 0);
  /* Store zerobits bitmap. */
  *zerobits = ~bitmap;
#else
  /* Move bitmap to two 32-bit scalar registers. */
  uint32_t bitmap0 = vget_lane_u32(vreinterpret_u32_u8(bitmap_all), 0);
  uint32_t bitmap1 = vget_lane_u32(vreinterpret_u32_u8(bitmap_all), 1);
  /* Store zerobits bitmap. */
  zerobits[0] = ~bitmap0;
  zerobits[1] = ~bitmap1;
#endif
}
