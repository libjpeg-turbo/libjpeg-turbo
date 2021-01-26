/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2021, Ilya Kurdyukov <jpegqs@gmail.com> for BaseALT, Ltd
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

/* Function declarations */

#define CONVERT_DECL(name) \
EXTERN(void) jsimd_##name##_ycc_convert_e2k \
  (JDIMENSION img_width, JSAMPARRAY input_buf, JSAMPIMAGE output_buf, \
   JDIMENSION output_row, int num_rows); \
EXTERN(void) jsimd_##name##_gray_convert_e2k \
  (JDIMENSION img_width, JSAMPARRAY input_buf, JSAMPIMAGE output_buf, \
   JDIMENSION output_row, int num_rows); \
EXTERN(void) jsimd_ycc_##name##_convert_e2k \
  (JDIMENSION out_width, JSAMPIMAGE input_buf, JDIMENSION input_row, \
   JSAMPARRAY output_buf, int num_rows);

CONVERT_DECL(rgb)
CONVERT_DECL(extrgb)
CONVERT_DECL(extrgbx)
CONVERT_DECL(extbgr)
CONVERT_DECL(extbgrx)
CONVERT_DECL(extxbgr)
CONVERT_DECL(extxrgb)

EXTERN(void) jsimd_h2v1_downsample_e2k
  (JDIMENSION image_width, int max_v_samp_factor, JDIMENSION v_samp_factor,
   JDIMENSION width_in_blocks, JSAMPARRAY input_data, JSAMPARRAY output_data);
EXTERN(void) jsimd_h2v2_downsample_e2k
  (JDIMENSION image_width, int max_v_samp_factor, JDIMENSION v_samp_factor,
   JDIMENSION width_in_blocks, JSAMPARRAY input_data, JSAMPARRAY output_data);

#define UPSAMPLE_DECL(name) \
EXTERN(void) jsimd_##name##_upsample_e2k \
  (int max_v_samp_factor, JDIMENSION output_width, JSAMPARRAY input_data, \
   JSAMPARRAY *output_data_ptr);

UPSAMPLE_DECL(h2v1)
UPSAMPLE_DECL(h2v2)
UPSAMPLE_DECL(h2v1_fancy)
UPSAMPLE_DECL(h2v2_fancy)

#define MERGED_DECL(name) \
EXTERN(void) jsimd_##name##_merged_upsample_e2k \
  (JDIMENSION output_width, JSAMPIMAGE input_buf, \
  JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf);

MERGED_DECL(h2v1)
MERGED_DECL(h2v1_extrgb)
MERGED_DECL(h2v1_extrgbx)
MERGED_DECL(h2v1_extbgr)
MERGED_DECL(h2v1_extbgrx)
MERGED_DECL(h2v1_extxbgr)
MERGED_DECL(h2v1_extxrgb)

MERGED_DECL(h2v2)
MERGED_DECL(h2v2_extrgb)
MERGED_DECL(h2v2_extrgbx)
MERGED_DECL(h2v2_extbgr)
MERGED_DECL(h2v2_extbgrx)
MERGED_DECL(h2v2_extxbgr)
MERGED_DECL(h2v2_extxrgb)

EXTERN(void) jsimd_convsamp_e2k
  (JSAMPARRAY sample_data, JDIMENSION start_col, DCTELEM *workspace);
EXTERN(void) jsimd_convsamp_float_e2k
  (JSAMPARRAY sample_data, JDIMENSION start_col, FAST_FLOAT *workspace);

EXTERN(void) jsimd_fdct_islow_e2k(DCTELEM *data);
EXTERN(void) jsimd_fdct_ifast_e2k(DCTELEM *data);
EXTERN(void) jsimd_fdct_float_e2k(FAST_FLOAT *data);
EXTERN(void) jsimd_quantize_e2k
  (JCOEFPTR coef_block, DCTELEM *divisors, DCTELEM *workspace);
EXTERN(void) jsimd_quantize_float_e2k
  (JCOEFPTR coef_block, FAST_FLOAT *divisors, FAST_FLOAT *workspace);
EXTERN(void) jsimd_idct_islow_e2k
  (void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
   JDIMENSION output_col);
EXTERN(void) jsimd_idct_ifast_e2k
  (void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
   JDIMENSION output_col);
EXTERN(void) jsimd_idct_float_e2k
  (void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf,
   JDIMENSION output_col);

extern int jsimd_use_eml;

