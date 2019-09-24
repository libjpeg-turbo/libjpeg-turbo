/*
 * jsimd_msa.c
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2009-2011, 2014 D. R. Commander
 * Copyright 2015 Matthieu Darbois
 *
 * This file contains the interface between the "normal" portions
 * of the library and the MSA implementations when running on a
 * MIPS architecture.
 */

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static unsigned int simd_support = ~0;

#if defined(__linux__)

LOCAL(int)
parse_proc_cpuinfo(void)
{
  const char *file_name = "/proc/cpuinfo";
  char cpuinfo_line[256];
  FILE *f = NULL;
  simd_support = 0;

  if ((f = fopen(file_name, "r")) != NULL) {
    while (fgets(cpuinfo_line, sizeof(cpuinfo_line), f) != NULL) {
      if (strstr(cpuinfo_line, "msa") != NULL) {
        fclose(f);
        simd_support |= JSIMD_MSA;
        return 1;
      }
    }
    fclose(f);
  }

  /* Did not find string in the proc file, or not Linux ELF. */
  return 0;
}

#endif

/*
 * Check what SIMD accelerations are supported.
 *
 * FIXME: This code is racy under a multi-threaded environment.
 */
LOCAL(void)
init_simd(void)
{
  if (simd_support != ~0U)
    return;

  simd_support = 0;

#if defined(__MIPSEL__) && defined(__mips_msa) && (__mips_isa_rev >= 5)
  simd_support |= JSIMD_MSA;
#elif defined(__linux__)
  /* We still have a chance to use MSA regardless of globally used
   * -mmsa option passed to gcc by performing runtime detection via
   * /proc/cpuinfo parsing on linux */
  parse_proc_cpuinfo();
  return;
#endif
}

GLOBAL(int)
jsimd_can_rgb_ycc(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_rgb_gray(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb565(void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_c_can_null_convert(void)
{
  return 0;
}

GLOBAL(void)
jsimd_rgb_ycc_convert(j_compress_ptr cinfo,
                      JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                      JDIMENSION output_row, int num_rows)
{
  void (*msafct) (JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch (cinfo->in_color_space) {
    case JCS_EXT_RGB:
      msafct = jsimd_extrgb_ycc_convert_msa;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      msafct = jsimd_extrgbx_ycc_convert_msa;
      break;
    case JCS_EXT_BGR:
      msafct = jsimd_extbgr_ycc_convert_msa;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      msafct = jsimd_extbgrx_ycc_convert_msa;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      msafct = jsimd_extxbgr_ycc_convert_msa;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      msafct = jsimd_extxrgb_ycc_convert_msa;
      break;
    default:
      msafct = jsimd_extrgb_ycc_convert_msa;
      break;
  }

  msafct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_rgb_gray_convert(j_compress_ptr cinfo,
                       JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                       JDIMENSION output_row, int num_rows)
{
  void (*msafct) (JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch (cinfo->in_color_space) {
    case JCS_EXT_RGB:
      msafct = jsimd_extrgb_gray_convert_msa;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      msafct = jsimd_extrgbx_gray_convert_msa;
      break;
    case JCS_EXT_BGR:
      msafct = jsimd_extbgr_gray_convert_msa;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      msafct = jsimd_extbgrx_gray_convert_msa;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      msafct = jsimd_extxbgr_gray_convert_msa;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      msafct = jsimd_extxrgb_gray_convert_msa;
      break;
    default:
      msafct = jsimd_rgb_gray_convert_msa;
      break;
  }

  msafct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb_convert(j_decompress_ptr cinfo,
                      JSAMPIMAGE input_buf, JDIMENSION input_row,
                      JSAMPARRAY output_buf, int num_rows)
{
  void (*mipsoptfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

  switch (cinfo->out_color_space) {
    case JCS_EXT_RGB:
      mipsoptfct = jsimd_ycc_extrgb_convert_msa;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      mipsoptfct = jsimd_ycc_extrgbx_convert_msa;
      break;
    case JCS_EXT_BGR:
      mipsoptfct = jsimd_ycc_extbgr_convert_msa;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      mipsoptfct = jsimd_ycc_extbgrx_convert_msa;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      mipsoptfct = jsimd_ycc_extxbgr_convert_msa;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      mipsoptfct = jsimd_ycc_extxrgb_convert_msa;
      break;
    default:
      mipsoptfct = jsimd_ycc_extrgb_convert_msa;
      break;
  }

  mipsoptfct(cinfo->output_width, input_buf, input_row, output_buf,
             num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb565_convert(j_decompress_ptr cinfo,
                         JSAMPIMAGE input_buf, JDIMENSION input_row,
                         JSAMPARRAY output_buf, int num_rows)
{
  jsimd_ycc_rgb565_convert_msa(cinfo->output_width, input_buf, input_row,
                               output_buf, num_rows);
}

GLOBAL(void)
jsimd_c_null_convert(j_compress_ptr cinfo,
                     JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                     JDIMENSION output_row, int num_rows)
{
}

GLOBAL(int)
jsimd_can_h2v2_downsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v2_smooth_downsample(void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_downsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_downsample(j_compress_ptr cinfo, jpeg_component_info *compptr,
                      JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  jsimd_h2v2_downsample_msa(cinfo->image_width, cinfo->max_v_samp_factor,
                            compptr->v_samp_factor, compptr->width_in_blocks,
                            input_data, output_data);
}

GLOBAL(void)
jsimd_h2v2_smooth_downsample(j_compress_ptr cinfo,
                             jpeg_component_info *compptr,
                             JSAMPARRAY input_data, JSAMPARRAY output_data)
{
}

GLOBAL(void)
jsimd_h2v1_downsample(j_compress_ptr cinfo, jpeg_component_info *compptr,
                      JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  jsimd_h2v1_downsample_msa(cinfo->image_width, cinfo->max_v_samp_factor,
                            compptr->v_samp_factor, compptr->width_in_blocks,
                            input_data, output_data);
}

GLOBAL(int)
jsimd_can_h2v2_upsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_upsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_int_upsample(void)
{
  return 0;
}

GLOBAL(void)
jsimd_h2v2_upsample(j_decompress_ptr cinfo,
                    jpeg_component_info *compptr,
                    JSAMPARRAY input_data,
                    JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v2_upsample_msa(cinfo->max_v_samp_factor, cinfo->output_width,
                          input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_upsample(j_decompress_ptr cinfo,
                    jpeg_component_info *compptr,
                    JSAMPARRAY input_data,
                    JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v1_upsample_msa(cinfo->max_v_samp_factor, cinfo->output_width,
                          input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_int_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                   JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(int)
jsimd_can_h2v2_fancy_upsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_fancy_upsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample(j_decompress_ptr cinfo,
                          jpeg_component_info *compptr,
                          JSAMPARRAY input_data,
                          JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v2_fancy_upsample_msa(cinfo->max_v_samp_factor,
                                compptr->downsampled_width,
                                input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_fancy_upsample(j_decompress_ptr cinfo,
                          jpeg_component_info *compptr,
                          JSAMPARRAY input_data,
                          JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v1_fancy_upsample_msa(cinfo->max_v_samp_factor,
                                compptr->downsampled_width,
                                input_data, output_data_ptr);
}

GLOBAL(int)
jsimd_can_h2v2_merged_upsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_merged_upsample(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample(j_decompress_ptr cinfo,
                           JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr,
                           JSAMPARRAY output_buf)
{
  void (*msafct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch (cinfo->out_color_space) {
    case JCS_EXT_RGB:
      msafct = jsimd_h2v2_extrgb_merged_upsample_msa;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      msafct = jsimd_h2v2_extrgbx_merged_upsample_msa;
      break;
    case JCS_EXT_BGR:
      msafct = jsimd_h2v2_extbgr_merged_upsample_msa;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      msafct = jsimd_h2v2_extbgrx_merged_upsample_msa;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      msafct = jsimd_h2v2_extxbgr_merged_upsample_msa;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      msafct = jsimd_h2v2_extxrgb_merged_upsample_msa;
      break;
    default:
      msafct = jsimd_h2v2_merged_upsample_msa;
      break;
  }

  msafct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(void)
jsimd_h2v1_merged_upsample(j_decompress_ptr cinfo,
                           JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr,
                           JSAMPARRAY output_buf)
{
  void (*msafct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch (cinfo->out_color_space) {
    case JCS_EXT_RGB:
      msafct = jsimd_h2v1_extrgb_merged_upsample_msa;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      msafct = jsimd_h2v1_extrgbx_merged_upsample_msa;
      break;
    case JCS_EXT_BGR:
      msafct = jsimd_h2v1_extbgr_merged_upsample_msa;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      msafct = jsimd_h2v1_extbgrx_merged_upsample_msa;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      msafct = jsimd_h2v1_extxbgr_merged_upsample_msa;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      msafct = jsimd_h2v1_extxrgb_merged_upsample_msa;
      break;
    default:
      msafct = jsimd_h2v1_merged_upsample_msa;
      break;
  }

  msafct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(int)
jsimd_can_convsamp(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_convsamp_float(void)
{
  return 0;
}

GLOBAL(void)
jsimd_convsamp(JSAMPARRAY sample_data, JDIMENSION start_col,
               DCTELEM *workspace)
{
  jsimd_convsamp_msa(sample_data, start_col, workspace);
}

GLOBAL(void)
jsimd_convsamp_float(JSAMPARRAY sample_data, JDIMENSION start_col,
                     FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_fdct_islow(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_ifast(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_float(void)
{
  return 0;
}

GLOBAL(void)
jsimd_fdct_islow(DCTELEM *data)
{
  jsimd_fdct_islow_msa(data);
}

GLOBAL(void)
jsimd_fdct_ifast(DCTELEM *data)
{
  jsimd_fdct_ifast_msa(data);
}

GLOBAL(void)
jsimd_fdct_float(FAST_FLOAT *data)
{
}

GLOBAL(int)
jsimd_can_quantize(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_quantize_float(void)
{
  return 0;
}

GLOBAL(void)
jsimd_quantize(JCOEFPTR coef_block, DCTELEM *divisors,
               DCTELEM *workspace)
{
  jsimd_quantize_msa(coef_block, divisors, workspace);
}

GLOBAL(void)
jsimd_quantize_float(JCOEFPTR coef_block, FAST_FLOAT *divisors,
                     FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_idct_2x2(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(ISLOW_MULT_TYPE) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_4x4(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(ISLOW_MULT_TYPE) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_6x6(void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_12x12(void)
{
  return 0;
}

GLOBAL(void)
jsimd_idct_2x2(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
  unsigned char *output[2] = {
    output_buf[0] + output_col,
    output_buf[1] + output_col,
  };

  jsimd_idct_2x2_msa(cinfo, compptr, coef_block, output, output_col);
}

GLOBAL(void)
jsimd_idct_4x4(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
  unsigned char *output[4] = {
    output_buf[0] + output_col,
    output_buf[1] + output_col,
    output_buf[2] + output_col,
    output_buf[3] + output_col,
  };

  jsimd_idct_4x4_msa(cinfo, compptr, coef_block, output, output_col);
}

GLOBAL(void)
jsimd_idct_6x6(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_12x12(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block,
                 JSAMPARRAY output_buf, JDIMENSION output_col)
{
}

GLOBAL(int)
jsimd_can_idct_islow(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(ISLOW_MULT_TYPE) != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_ifast(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(IFAST_MULT_TYPE) != 2)
    return 0;
  if (IFAST_SCALE_BITS != 2)
    return 0;

  if (simd_support & JSIMD_MSA)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_float(void)
{
  return 0;
}

GLOBAL(void)
jsimd_idct_islow(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  unsigned char *output[8] = {
    output_buf[0] + output_col,
    output_buf[1] + output_col,
    output_buf[2] + output_col,
    output_buf[3] + output_col,
    output_buf[4] + output_col,
    output_buf[5] + output_col,
    output_buf[6] + output_col,
    output_buf[7] + output_col,
  };

  jsimd_idct_islow_msa(cinfo, compptr, coef_block, output, output_col);
}

GLOBAL(void)
jsimd_idct_ifast(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  unsigned char *output[8] = {
    output_buf[0] + output_col,
    output_buf[1] + output_col,
    output_buf[2] + output_col,
    output_buf[3] + output_col,
    output_buf[4] + output_col,
    output_buf[5] + output_col,
    output_buf[6] + output_col,
    output_buf[7] + output_col,
  };

  jsimd_idct_ifast_msa(cinfo, compptr, coef_block, output, output_col);
}

GLOBAL(void)
jsimd_idct_float(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
}

GLOBAL(int)
jsimd_can_huff_encode_one_block(void)
{
  return 0;
}

GLOBAL(JOCTET *)
jsimd_huff_encode_one_block(void *state, JOCTET *buffer, JCOEFPTR block,
                            int last_dc_val, c_derived_tbl *dctbl,
                            c_derived_tbl *actbl)
{
  return NULL;
}

GLOBAL(int)
jsimd_can_encode_mcu_AC_first_prepare(void)
{
  return 0;
}

GLOBAL(void)
jsimd_encode_mcu_AC_first_prepare(const JCOEF *block,
                                  const int *jpeg_natural_order_start, int Sl,
                                  int Al, JCOEF *values, size_t *zerobits)
{
}

GLOBAL(int)
jsimd_can_encode_mcu_AC_refine_prepare(void)
{
  return 0;
}

GLOBAL(int)
jsimd_encode_mcu_AC_refine_prepare(const JCOEF *block,
                                   const int *jpeg_natural_order_start, int Sl,
                                   int Al, JCOEF *absvalues, size_t *bits)
{
  return 0;
}
