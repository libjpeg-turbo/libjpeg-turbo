/*
 * Risc-V vector extension optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2025, Samsung. All Rights Reserved.
 *
 * Authors:  Filip Wasil     <f.wasil@samsung.com>
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
#include "jinclude.h"
#include "jpeglib.h"
#include "jsimd.h"
#include "jdct.h"
#include "jsimddct.h"
#include "../jsimd.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static unsigned int simd_support = ~0;

#if defined(__linux__)
#include <asm/hwcap.h>
#include <asm/hwprobe.h>
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_riscv_hwprobe
#define __NR_riscv_hwprobe 258
#endif

// This manual declaration is safe because:
// It matches the standard Linux ABI (long return, varargs)
// It's essentially documenting what the system already provides
// The actual implementation will be linked from glibc
#ifndef SYS_syscall
long syscall(long number, ...);
#endif

int
has_compliant_vsetvli (void);

static int
is_rvv_1_0_available ()
{
  struct riscv_hwprobe pair = {RISCV_HWPROBE_KEY_IMA_EXT_0, 0};

  if (syscall (__NR_riscv_hwprobe, &pair, 1, 0, 0, 0) < 0)
  {
    // At this point we already checked AT_HWCAP and we know we have
    // some version of RVV to our dispose. If this version of kernel
    // is failing on syscall __NR_riscv_hwprobe, we will check the RVV
    // version by looking at the vsetvli behaviour.
    return has_compliant_vsetvli ();
  }
  return (pair.value & RISCV_HWPROBE_IMA_V);
}

#endif

/* RVV SIMD support is always available during test. */
/*
 * Check what SIMD accelerations are supported.
 *
 * FIXME: This code is racy under a multi-threaded environment.
 */
LOCAL(void)
init_simd(void)
{
#ifndef NO_GETENV
  char *env = NULL;
#endif

  if (simd_support != ~0U)
    return;

  simd_support = 0;
#if defined(__linux__) && __riscv_v >= 1000000 && __riscv_v < 2000000
  simd_support = (getauxval (AT_HWCAP) & COMPAT_HWCAP_ISA_V
    && is_rvv_1_0_available ()) ? JSIMD_RVV : 0;
#endif

#ifndef NO_GETENV
  /* Force different settings through environment variables */
  env = getenv("JSIMD_FORCERVV");
  if ((env != NULL) && (strcmp(env, "1") == 0))
    simd_support = JSIMD_RVV;
  env = getenv("JSIMD_FORCENONE");
  if ((env != NULL) && (strcmp(env, "1") == 0))
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb565(void)
{
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
  void (*rvvfct) (JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch (cinfo->in_color_space) {
  case JCS_EXT_RGB:
    rvvfct = jsimd_extrgb_ycc_convert_rvv;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    rvvfct = jsimd_extrgbx_ycc_convert_rvv;
    break;
  case JCS_EXT_BGR:
    rvvfct = jsimd_extbgr_ycc_convert_rvv;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    rvvfct = jsimd_extbgrx_ycc_convert_rvv;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    rvvfct = jsimd_extxbgr_ycc_convert_rvv;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    rvvfct = jsimd_extxrgb_ycc_convert_rvv;
    break;
  default:
    rvvfct = jsimd_rgb_ycc_convert_rvv;
    break;
  }

  rvvfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_rgb_gray_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                       JSAMPIMAGE output_buf, JDIMENSION output_row,
                       int num_rows)
{
  void (*rvvfct) (JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch (cinfo->in_color_space) {
  case JCS_EXT_RGB:
    rvvfct = jsimd_extrgb_gray_convert_rvv;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    rvvfct = jsimd_extrgbx_gray_convert_rvv;
    break;
  case JCS_EXT_BGR:
    rvvfct = jsimd_extbgr_gray_convert_rvv;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    rvvfct = jsimd_extbgrx_gray_convert_rvv;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    rvvfct = jsimd_extxbgr_gray_convert_rvv;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    rvvfct = jsimd_extxrgb_gray_convert_rvv;
    break;
  default:
    rvvfct = jsimd_rgb_gray_convert_rvv;
    break;
  }

  rvvfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb_convert(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                      JDIMENSION input_row, JSAMPARRAY output_buf,
                      int num_rows)
{
  void (*rvvfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    rvvfct = jsimd_ycc_extrgb_convert_rvv;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    rvvfct = jsimd_ycc_extrgbx_convert_rvv;
    break;
  case JCS_EXT_BGR:
    rvvfct = jsimd_ycc_extbgr_convert_rvv;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    rvvfct = jsimd_ycc_extbgrx_convert_rvv;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    rvvfct = jsimd_ycc_extxbgr_convert_rvv;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    rvvfct = jsimd_ycc_extxrgb_convert_rvv;
    break;
  default:
    rvvfct = jsimd_ycc_rgb_convert_rvv;
    break;
  }

  rvvfct(cinfo->output_width, input_buf, input_row, output_buf, num_rows);
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_downsample(j_compress_ptr cinfo, jpeg_component_info *compptr,
                      JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  jsimd_h2v2_downsample_rvv(cinfo->image_width, cinfo->max_v_samp_factor,
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
  jsimd_h2v1_downsample_rvv(cinfo->image_width, cinfo->max_v_samp_factor,
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
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
  jsimd_h2v2_upsample_rvv(cinfo->max_v_samp_factor,
                          cinfo->output_width, input_data,
                          output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                    JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v1_upsample_rvv(cinfo->max_v_samp_factor,
                          cinfo->output_width, input_data,
                          output_data_ptr);
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                          JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v2_fancy_upsample_rvv(cinfo->max_v_samp_factor,
                                compptr->downsampled_width, input_data,
                                output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_fancy_upsample(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                          JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v1_fancy_upsample_rvv(cinfo->max_v_samp_factor,
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  void (*rvvfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    rvvfct = jsimd_h2v2_extrgb_merged_upsample_rvv;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    rvvfct = jsimd_h2v2_extrgbx_merged_upsample_rvv;
    break;
  case JCS_EXT_BGR:
    rvvfct = jsimd_h2v2_extbgr_merged_upsample_rvv;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    rvvfct = jsimd_h2v2_extbgrx_merged_upsample_rvv;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    rvvfct = jsimd_h2v2_extxbgr_merged_upsample_rvv;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    rvvfct = jsimd_h2v2_extxrgb_merged_upsample_rvv;
    break;
  default:
    rvvfct = jsimd_h2v2_merged_upsample_rvv;
    break;
  }

  rvvfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(void)
jsimd_h2v1_merged_upsample(j_decompress_ptr cinfo, JSAMPIMAGE input_buf,
                           JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  void (*rvvfct) (JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch (cinfo->out_color_space) {
  case JCS_EXT_RGB:
    rvvfct = jsimd_h2v1_extrgb_merged_upsample_rvv;
    break;
  case JCS_EXT_RGBX:
  case JCS_EXT_RGBA:
    rvvfct = jsimd_h2v1_extrgbx_merged_upsample_rvv;
    break;
  case JCS_EXT_BGR:
    rvvfct = jsimd_h2v1_extbgr_merged_upsample_rvv;
    break;
  case JCS_EXT_BGRX:
  case JCS_EXT_BGRA:
    rvvfct = jsimd_h2v1_extbgrx_merged_upsample_rvv;
    break;
  case JCS_EXT_XBGR:
  case JCS_EXT_ABGR:
    rvvfct = jsimd_h2v1_extxbgr_merged_upsample_rvv;
    break;
  case JCS_EXT_XRGB:
  case JCS_EXT_ARGB:
    rvvfct = jsimd_h2v1_extxrgb_merged_upsample_rvv;
    break;
  default:
    rvvfct = jsimd_h2v1_merged_upsample_rvv;
    break;
  }

  rvvfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(int)
jsimd_can_convsamp(void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_RVV)
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
  jsimd_convsamp_rvv(sample_data, start_col, workspace);
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
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
  jsimd_fdct_islow_rvv(data);
}

GLOBAL(void)
jsimd_fdct_ifast(DCTELEM *data)
{
  jsimd_fdct_ifast_rvv(data);
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

  if (simd_support & JSIMD_RVV)
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
  jsimd_quantize_rvv(coef_block, divisors, workspace);
}

GLOBAL(void)
jsimd_quantize_float(JCOEFPTR coef_block, FAST_FLOAT *divisors,
                     FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_idct_2x2(void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_4x4(void)
{
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
}

GLOBAL(void)
jsimd_idct_4x4(j_decompress_ptr cinfo, jpeg_component_info *compptr,
               JCOEFPTR coef_block, JSAMPARRAY output_buf,
               JDIMENSION output_col)
{
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

  if (simd_support & JSIMD_RVV)
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

  if (simd_support & JSIMD_RVV)
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
  jsimd_idct_islow_rvv(compptr->dct_table, coef_block, output_buf, output_col);
}

GLOBAL(void)
jsimd_idct_ifast(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  jsimd_idct_ifast_rvv(compptr->dct_table, coef_block, output_buf, output_col);
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
