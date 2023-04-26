/*
 * jsimd_loongarch64.c
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2009-2011, 2014, 2016, 2018, D. R. Commander.
 * Copyright (C) 2013-2014, MIPS Technologies, Inc., California.
 * Copyright (C) 2015, 2018, Matthieu Darbois.
 * Copyright (C) 2021, 2023, Loongson Technology Corporation Limited, BeiJing.
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * 64-bit LOONGARCH architecture.
 */

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "../../jdct.h"
#include "../../jsimddct.h"
#include "../jsimd.h"

#include <ctype.h>
#include <sys/auxv.h>
#include <asm/hwcap.h>

static THREAD_LOCAL unsigned int simd_support = ~0;

LOCAL(void)
check_loongarch_feature(void)
{
  simd_support = 0;

  uint64_t hwcaps = getauxval(AT_HWCAP);

  if (hwcaps & HWCAP_LOONGARCH_LSX)
      simd_support |= JSIMD_LSX;
  if (hwcaps & HWCAP_LOONGARCH_LASX)
      simd_support |= JSIMD_LASX;
}

/*
 * Check what SIMD accelerations are supported.
 */
LOCAL(void)
init_simd(void)
{
#ifndef NO_GETENV
  char env[2] = { 0 };
#endif

  if (simd_support != ~0U)
    return;

  check_loongarch_feature();

#ifndef NO_GETENV
  /* Force different settings through environment variables */
  if (!GETENV_S(env, 2, "JSIMD_FORCELSX") && !strcmp(env, "1"))
    simd_support &= JSIMD_LSX;
  if (!GETENV_S(env, 2, "JSIMD_FORCELASX") && !strcmp(env, "1"))
    simd_support &= JSIMD_LASX;
  if (!GETENV_S(env, 2, "JSIMD_FORCENONE") && !strcmp(env, "1"))
    simd_support = 0;
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb565(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  return 0;
}

GLOBAL(int)
jsimd_c_can_null_convert(void)
{
  return 0;
}

GLOBAL(void)
jsimd_rgb_ycc_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                      JSAMPIMAGE output_buf, JDIMENSION output_row,
                      int num_rows)
{
  void (*lasxfct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);
  void (*lsxfct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch (cinfo->in_color_space) {
  case JCS_EXT_RGB:
    lasxfct = jsimd_extrgb_ycc_convert_lasx;
    lsxfct = jsimd_extrgb_ycc_convert_lsx;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    lasxfct = jsimd_extrgbx_ycc_convert_lasx;
    lsxfct = jsimd_extrgbx_ycc_convert_lsx;
    break;
  case JCS_EXT_BGR:
    lasxfct = jsimd_extbgr_ycc_convert_lasx;
    lsxfct = jsimd_extbgr_ycc_convert_lsx;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    lasxfct = jsimd_extbgrx_ycc_convert_lasx;
    lsxfct = jsimd_extbgrx_ycc_convert_lsx;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    lasxfct = jsimd_extxbgr_ycc_convert_lasx;
    lsxfct = jsimd_extxbgr_ycc_convert_lsx;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    lasxfct = jsimd_extxrgb_ycc_convert_lasx;
    lsxfct = jsimd_extxrgb_ycc_convert_lsx;
    break;
  default:
    lasxfct = jsimd_rgb_ycc_convert_lasx;
    lsxfct = jsimd_rgb_ycc_convert_lsx;
    break;
  }

  if (simd_support & JSIMD_LASX)
    lasxfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
  else if (simd_support & JSIMD_LSX)
    lsxfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_rgb_gray_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                       JSAMPIMAGE output_buf, JDIMENSION output_row,
                       int num_rows)
{
  void (*lasxfct) (JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);
  void (*lsxfct) (JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch (cinfo->in_color_space) {
  case JCS_EXT_RGB:
    lasxfct = jsimd_extrgb_gray_convert_lasx;
    lsxfct = jsimd_extrgb_gray_convert_lsx;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    lasxfct = jsimd_extrgbx_gray_convert_lasx;
    lsxfct = jsimd_extrgbx_gray_convert_lsx;
    break;
  case JCS_EXT_BGR:
    lasxfct = jsimd_extbgr_gray_convert_lasx;
    lsxfct = jsimd_extbgr_gray_convert_lsx;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    lasxfct = jsimd_extbgrx_gray_convert_lasx;
    lsxfct = jsimd_extbgrx_gray_convert_lsx;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    lasxfct = jsimd_extxbgr_gray_convert_lasx;
    lsxfct = jsimd_extxbgr_gray_convert_lsx;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    lasxfct = jsimd_extxrgb_gray_convert_lasx;
    lsxfct = jsimd_extxrgb_gray_convert_lsx;
    break;
  default:
    lasxfct = jsimd_rgb_gray_convert_lasx;
    lsxfct = jsimd_rgb_gray_convert_lsx;
    break;
  }

  if (simd_support & JSIMD_LASX)
    lasxfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
  else if (simd_support & JSIMD_LSX)
    lsxfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb_convert(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                      JDIMENSION input_row, JSAMPARRAY output_buf,
                      int num_rows)
{
  void (*lasxfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);
  void (*lsxfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    lasxfct = jsimd_ycc_extrgb_convert_lasx;
    lsxfct = jsimd_ycc_extrgb_convert_lsx;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    lasxfct = jsimd_ycc_extrgbx_convert_lasx;
    lsxfct = jsimd_ycc_extrgbx_convert_lsx;
    break;
  case JCS_EXT_BGR:
    lasxfct = jsimd_ycc_extbgr_convert_lasx;
    lsxfct = jsimd_ycc_extbgr_convert_lsx;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    lasxfct = jsimd_ycc_extbgrx_convert_lasx;
    lsxfct = jsimd_ycc_extbgrx_convert_lsx;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    lasxfct = jsimd_ycc_extxbgr_convert_lasx;
    lsxfct = jsimd_ycc_extxbgr_convert_lsx;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    lasxfct = jsimd_ycc_extxrgb_convert_lasx;
    lsxfct = jsimd_ycc_extxrgb_convert_lsx;
    break;
  default:
    lasxfct = jsimd_ycc_rgb_convert_lasx;
    lsxfct = jsimd_ycc_rgb_convert_lsx;
    break;
  }

  if (simd_support & JSIMD_LASX)
    lasxfct(cinfo->output_width, input_buf, input_row, output_buf, num_rows);
  else if (simd_support & JSIMD_LSX)
    lsxfct(cinfo->output_width, input_buf, input_row, output_buf, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb565_convert(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                         JDIMENSION input_row, JSAMPARRAY output_buf,
                         int num_rows)
{
}

GLOBAL(void)
jsimd_c_null_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                     JSAMPIMAGE output_buf, JDIMENSION output_row,
                     int num_rows)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_downsample(j_compress_ptr cinfo, jpeg_component_info *compptr,
                      JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  if (simd_support & JSIMD_LASX)
    jsimd_h2v2_downsample_lasx(cinfo->image_width, cinfo->max_v_samp_factor,
                               compptr->v_samp_factor, compptr->width_in_blocks,
                               input_data, output_data);
  else if (simd_support & JSIMD_LSX)
    jsimd_h2v2_downsample_lsx(cinfo->image_width, cinfo->max_v_samp_factor,
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
  if (simd_support & JSIMD_LASX)
    jsimd_h2v1_downsample_lasx(cinfo->image_width, cinfo->max_v_samp_factor,
                               compptr->v_samp_factor, compptr->width_in_blocks,
                               input_data, output_data);
  else if (simd_support & JSIMD_LSX)
    jsimd_h2v1_downsample_lsx(cinfo->image_width, cinfo->max_v_samp_factor,
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_int_upsample(void)
{
  return 0;
}

GLOBAL(void)
jsimd_h2v2_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                    JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  if (simd_support & JSIMD_LASX)
    jsimd_h2v2_upsample_lasx(cinfo->max_v_samp_factor, cinfo->output_width,
                             input_data, output_data_ptr);
  else if (simd_support & JSIMD_LSX)
    jsimd_h2v2_upsample_lsx(cinfo->max_v_samp_factor, cinfo->output_width,
                            input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                    JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  if (simd_support & JSIMD_LASX)
    jsimd_h2v1_upsample_lasx(cinfo->max_v_samp_factor, cinfo->output_width,
                             input_data, output_data_ptr);
  else if (simd_support & JSIMD_LSX)
    jsimd_h2v1_upsample_lsx(cinfo->max_v_samp_factor, cinfo->output_width,
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                          JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  if (simd_support & JSIMD_LASX)
    jsimd_h2v2_fancy_upsample_lasx(cinfo->max_v_samp_factor,
                                   compptr->downsampled_width, input_data,
                                   output_data_ptr);
  else if (simd_support & JSIMD_LSX)
    jsimd_h2v2_fancy_upsample_lsx(cinfo->max_v_samp_factor,
                                  compptr->downsampled_width, input_data,
                                  output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_fancy_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                          JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  if (simd_support & JSIMD_LASX)
    jsimd_h2v1_fancy_upsample_lasx(cinfo->max_v_samp_factor,
                                   compptr->downsampled_width, input_data,
                                   output_data_ptr);
  else if (simd_support & JSIMD_LSX)
    jsimd_h2v1_fancy_upsample_lsx(cinfo->max_v_samp_factor,
                                  compptr->downsampled_width, input_data,
                                  output_data_ptr);
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  void (*lasxfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
  void (*lsxfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    lasxfct = jsimd_h2v2_extrgb_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_extrgb_merged_upsample_lsx;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    lasxfct = jsimd_h2v2_extrgbx_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_extrgbx_merged_upsample_lsx;
    break;
  case JCS_EXT_BGR:
    lasxfct = jsimd_h2v2_extbgr_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_extbgr_merged_upsample_lsx;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    lasxfct = jsimd_h2v2_extbgrx_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_extbgrx_merged_upsample_lsx;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    lasxfct = jsimd_h2v2_extxbgr_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_extxbgr_merged_upsample_lsx;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    lasxfct = jsimd_h2v2_extxrgb_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_extxrgb_merged_upsample_lsx;
    break;
  default:
    lasxfct = jsimd_h2v2_merged_upsample_lasx;
    lsxfct = jsimd_h2v2_merged_upsample_lsx;
    break;
  }

  if (simd_support & JSIMD_LASX)
    lasxfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
  else if (simd_support & JSIMD_LSX)
    lsxfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(void)
jsimd_h2v1_merged_upsample(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  void (*lasxfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
  void (*lsxfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    lasxfct = jsimd_h2v1_extrgb_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_extrgb_merged_upsample_lsx;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    lasxfct = jsimd_h2v1_extrgbx_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_extrgbx_merged_upsample_lsx;
    break;
  case JCS_EXT_BGR:
    lasxfct = jsimd_h2v1_extbgr_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_extbgr_merged_upsample_lsx;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    lasxfct = jsimd_h2v1_extbgrx_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_extbgrx_merged_upsample_lsx;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    lasxfct = jsimd_h2v1_extxbgr_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_extxbgr_merged_upsample_lsx;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    lasxfct = jsimd_h2v1_extxrgb_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_extxrgb_merged_upsample_lsx;
    break;
  default:
    lasxfct = jsimd_h2v1_merged_upsample_lasx;
    lsxfct = jsimd_h2v1_merged_upsample_lsx;
    break;
  }

  if (simd_support & JSIMD_LASX)
    lasxfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
  else if (simd_support & JSIMD_LSX)
    lsxfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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
  if (simd_support & JSIMD_LASX)
    jsimd_convsamp_lasx(sample_data, start_col, workspace);
  else if (simd_support & JSIMD_LSX)
    jsimd_convsamp_lsx(sample_data, start_col, workspace);
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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
  if (simd_support & JSIMD_LASX)
    jsimd_fdct_islow_lasx(data);
  else if (simd_support & JSIMD_LSX)
    jsimd_fdct_islow_lsx(data);
}

GLOBAL(void)
jsimd_fdct_ifast(DCTELEM *data)
{
  if (simd_support & JSIMD_LASX)
    jsimd_fdct_ifast_lasx(data);
  else if (simd_support & JSIMD_LSX)
    jsimd_fdct_ifast_lsx(data);
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_quantize_float(void)
{
  return 0;
}

GLOBAL(void)
jsimd_quantize(JCOEFPTR coef_block, DCTELEM *divisors, DCTELEM *workspace)
{
  if (simd_support & JSIMD_LASX)
    jsimd_quantize_lasx(coef_block, divisors, workspace);
  else if (simd_support & JSIMD_LSX)
    jsimd_quantize_lsx(coef_block, divisors, workspace);
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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
  if (simd_support & JSIMD_LASX)
    jsimd_idct_2x2_lasx(compptr->dct_table, coef_block, output_buf, output_col);
  else if (simd_support & JSIMD_LSX)
    jsimd_idct_2x2_lsx(compptr->dct_table, coef_block, output_buf, output_col);
}

GLOBAL(void)
jsimd_idct_4x4(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
  if (simd_support & JSIMD_LASX)
    jsimd_idct_4x4_lasx(compptr->dct_table, coef_block, output_buf, output_col);
  else if (simd_support & JSIMD_LSX)
    jsimd_idct_4x4_lsx(compptr->dct_table, coef_block, output_buf, output_col);
}

GLOBAL(void)
jsimd_idct_6x6(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_12x12(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    return 1;
  if (simd_support & JSIMD_LSX)
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

  if (simd_support & JSIMD_LASX)
    jsimd_idct_islow_lasx(cinfo, compptr, coef_block, output, output_col);
  else if (simd_support & JSIMD_LSX)
    jsimd_idct_islow_lsx(cinfo, compptr, coef_block, output, output_col);
}

GLOBAL(void)
jsimd_idct_ifast(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  if (simd_support & JSIMD_LASX)
    jsimd_idct_ifast_lasx(compptr->dct_table, coef_block, output_buf,
                          output_col);
  else if (simd_support & JSIMD_LSX)
    jsimd_idct_ifast_lsx(compptr->dct_table, coef_block, output_buf,
                         output_col);
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
                                  int Al, UJCOEF *values, size_t *zerobits)
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
                                   int Al, UJCOEF *absvalues, size_t *bits)
{
  return 0;
}
