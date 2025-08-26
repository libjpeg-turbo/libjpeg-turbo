/*
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2009-2011, 2014, 2018, 2020, 2025, D. R. Commander.
 * Copyright (C) 2013-2014, MIPS Technologies, Inc., California.
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
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * MIPS architecture.
 */

#define JPEG_INTERNALS
#include "../src/jsimd.h"
#include "../src/jsimddct.h"
#include "../src/jdsample.h"
#include "../jsimd.h"

#if !(defined(__mips_dsp) && (__mips_dsp_rev >= 2)) && defined(__linux__)

LOCAL(void)
parse_proc_cpuinfo(const char *search_string, unsigned int *simd_support)
{
  char cpuinfo_line[256];
  FILE *f = NULL;

  *simd_support = 0;

  if ((f = fopen("/proc/cpuinfo", "r")) != NULL) {
    while (fgets(cpuinfo_line, sizeof(cpuinfo_line), f) != NULL) {
      if (strstr(cpuinfo_line, search_string) != NULL) {
        fclose(f);
        *simd_support |= JSIMD_DSPR2;
        return;
      }
    }
    fclose(f);
  }
  /* Did not find string in the proc file, or not Linux ELF. */
}

#endif


HIDDEN unsigned int
jpeg_simd_cpu_support(void)
{
  unsigned int simd_support = 0;

#if defined(__mips_dsp) && (__mips_dsp_rev >= 2)
  simd_support |= JSIMD_DSPR2;
#elif defined(__linux__)
  /* We still have a chance to use MIPS DSPR2 regardless of globally used
   * -mdspr2 options passed to gcc by performing runtime detection via
   * /proc/cpuinfo parsing on linux */
  parse_proc_cpuinfo("MIPS 74K", &simd_support);
#endif

  return simd_support;
}


static const int mips_idct_ifast_coefs[4] = {
  0x45404540,           /* FIX( 1.082392200 / 2) =  17734 = 0x4546 */
  0x5A805A80,           /* FIX( 1.414213562 / 2) =  23170 = 0x5A82 */
  0x76407640,           /* FIX( 1.847759065 / 2) =  30274 = 0x7642 */
  0xAC60AC60            /* FIX(-2.613125930 / 4) = -21407 = 0xAC61 */
};


HIDDEN void
jsimd_c_null_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                     JSAMPIMAGE output_buf, JDIMENSION output_row,
                     int num_rows)
{
  jsimd_c_null_convert_dspr2(cinfo->image_width, input_buf, output_buf,
                             output_row, num_rows, cinfo->num_components);
}


HIDDEN void
jsimd_h2v2_smooth_downsample(j_compress_ptr cinfo,
                             jpeg_component_info *compptr,
                             JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  jsimd_h2v2_smooth_downsample_dspr2(input_data, output_data,
                                     compptr->v_samp_factor,
                                     cinfo->max_v_samp_factor,
                                     cinfo->smoothing_factor,
                                     compptr->width_in_blocks,
                                     cinfo->image_width);
}


HIDDEN void
jsimd_int_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                   JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  my_upsample_ptr upsample = (my_upsample_ptr)cinfo->upsample;

  jsimd_int_upsample_dspr2(upsample->h_expand[compptr->component_index],
                           upsample->v_expand[compptr->component_index],
                           input_data, output_data_ptr, cinfo->output_width,
                           cinfo->max_v_samp_factor);
}


HIDDEN void
jsimd_h2v1_merged_upsample(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  void (*dspr2fct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, JSAMPLE *);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    dspr2fct = jsimd_h2v1_extrgb_merged_upsample_dspr2;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    dspr2fct = jsimd_h2v1_extrgbx_merged_upsample_dspr2;
    break;
  case JCS_EXT_BGR:
    dspr2fct = jsimd_h2v1_extbgr_merged_upsample_dspr2;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    dspr2fct = jsimd_h2v1_extbgrx_merged_upsample_dspr2;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    dspr2fct = jsimd_h2v1_extxbgr_merged_upsample_dspr2;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    dspr2fct = jsimd_h2v1_extxrgb_merged_upsample_dspr2;
    break;
  default:
    dspr2fct = jsimd_h2v1_extrgb_merged_upsample_dspr2;
    break;
  }

  dspr2fct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf,
           cinfo->sample_range_limit);
}


HIDDEN void
jsimd_h2v2_merged_upsample(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  void (*dspr2fct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, JSAMPLE *);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    dspr2fct = jsimd_h2v2_extrgb_merged_upsample_dspr2;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    dspr2fct = jsimd_h2v2_extrgbx_merged_upsample_dspr2;
    break;
  case JCS_EXT_BGR:
    dspr2fct = jsimd_h2v2_extbgr_merged_upsample_dspr2;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    dspr2fct = jsimd_h2v2_extbgrx_merged_upsample_dspr2;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    dspr2fct = jsimd_h2v2_extxbgr_merged_upsample_dspr2;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    dspr2fct = jsimd_h2v2_extxrgb_merged_upsample_dspr2;
    break;
  default:
    dspr2fct = jsimd_h2v2_extrgb_merged_upsample_dspr2;
    break;
  }

  dspr2fct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf,
           cinfo->sample_range_limit);
}


HIDDEN void
jsimd_idct_islow(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  int output[8];

  output[0] = (int)(output_buf[0] + output_col);
  output[1] = (int)(output_buf[1] + output_col);
  output[2] = (int)(output_buf[2] + output_col);
  output[3] = (int)(output_buf[3] + output_col);
  output[4] = (int)(output_buf[4] + output_col);
  output[5] = (int)(output_buf[5] + output_col);
  output[6] = (int)(output_buf[6] + output_col);
  output[7] = (int)(output_buf[7] + output_col);

  jsimd_idct_islow_dspr2(coef_block, compptr->dct_table, output,
                         IDCT_range_limit(cinfo));
}


HIDDEN void
jsimd_idct_ifast(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  JCOEFPTR inptr;
  IFAST_MULT_TYPE *quantptr;
  DCTELEM workspace[DCTSIZE2];  /* buffers data between passes */

  /* Pass 1: process columns from input, store into work array. */

  inptr = coef_block;
  quantptr = (IFAST_MULT_TYPE *)compptr->dct_table;

  jsimd_idct_ifast_cols_dspr2(inptr, quantptr, workspace,
                              mips_idct_ifast_coefs);

  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

  jsimd_idct_ifast_rows_dspr2(workspace, output_buf, output_col,
                              mips_idct_ifast_coefs);
}


HIDDEN void
jsimd_idct_4x4(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
  int workspace[DCTSIZE * 4]; /* buffers data between passes */

  jsimd_idct_4x4_dspr2(compptr->dct_table, coef_block, output_buf, output_col,
                       workspace);
}


HIDDEN void
jsimd_idct_6x6(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
  jsimd_idct_6x6_dspr2(compptr->dct_table, coef_block, output_buf, output_col);
}


HIDDEN void
jsimd_idct_12x12(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  int workspace[96];
  int output[12];

  output[0] = (int)(output_buf[0] + output_col);
  output[1] = (int)(output_buf[1] + output_col);
  output[2] = (int)(output_buf[2] + output_col);
  output[3] = (int)(output_buf[3] + output_col);
  output[4] = (int)(output_buf[4] + output_col);
  output[5] = (int)(output_buf[5] + output_col);
  output[6] = (int)(output_buf[6] + output_col);
  output[7] = (int)(output_buf[7] + output_col);
  output[8] = (int)(output_buf[8] + output_col);
  output[9] = (int)(output_buf[9] + output_col);
  output[10] = (int)(output_buf[10] + output_col);
  output[11] = (int)(output_buf[11] + output_col);

  jsimd_idct_12x12_pass1_dspr2(coef_block, compptr->dct_table, workspace);
  jsimd_idct_12x12_pass2_dspr2(workspace, output);
}
