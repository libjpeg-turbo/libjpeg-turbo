/*
 * jquanti-neon.c - sample quantization (Arm NEON)
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

/* Load sample data into DCT workspace, applying unsigned -> signed
 * conversion, subtracting CENTREJSAMPLE and widening to 16-bit.
 */

void jsimd_convsamp_neon(JSAMPARRAY sample_data,
                         JDIMENSION start_col,
                         DCTELEM *workspace)
{
  uint8x8_t samp_row0 = vld1_u8(sample_data[0] + start_col);
  uint8x8_t samp_row1 = vld1_u8(sample_data[1] + start_col);
  uint8x8_t samp_row2 = vld1_u8(sample_data[2] + start_col);
  uint8x8_t samp_row3 = vld1_u8(sample_data[3] + start_col);
  uint8x8_t samp_row4 = vld1_u8(sample_data[4] + start_col);
  uint8x8_t samp_row5 = vld1_u8(sample_data[5] + start_col);
  uint8x8_t samp_row6 = vld1_u8(sample_data[6] + start_col);
  uint8x8_t samp_row7 = vld1_u8(sample_data[7] + start_col);

  int16x8_t row0 = vreinterpretq_s16_u16(vsubl_u8(samp_row0,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row1 = vreinterpretq_s16_u16(vsubl_u8(samp_row1,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row2 = vreinterpretq_s16_u16(vsubl_u8(samp_row2,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row3 = vreinterpretq_s16_u16(vsubl_u8(samp_row3,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row4 = vreinterpretq_s16_u16(vsubl_u8(samp_row4,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row5 = vreinterpretq_s16_u16(vsubl_u8(samp_row5,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row6 = vreinterpretq_s16_u16(vsubl_u8(samp_row6,
                                                  vdup_n_u8(CENTERJSAMPLE)));
  int16x8_t row7 = vreinterpretq_s16_u16(vsubl_u8(samp_row7,
                                                  vdup_n_u8(CENTERJSAMPLE)));

  vst1q_s16(workspace + 0 * DCTSIZE, row0);
  vst1q_s16(workspace + 1 * DCTSIZE, row1);
  vst1q_s16(workspace + 2 * DCTSIZE, row2);
  vst1q_s16(workspace + 3 * DCTSIZE, row3);
  vst1q_s16(workspace + 4 * DCTSIZE, row4);
  vst1q_s16(workspace + 5 * DCTSIZE, row5);
  vst1q_s16(workspace + 6 * DCTSIZE, row6);
  vst1q_s16(workspace + 7 * DCTSIZE, row7);
}
