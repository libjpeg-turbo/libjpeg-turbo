/*
 * jsimd_loongson.c
 *
 * Copyright 2016 Chen Zhu <zhuchen@loongson.cn>
 * 
 * Based on the LOONGSON SIMD extension for IJG JPEG library,
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * loongson cpu.
 */

#define JPEG_INTERNALS
#include "../jinclude.h"
#include "../jpeglib.h"
#include "../jsimd.h"
#include "../jdct.h"
#include "../jsimddct.h"
#include "jsimd.h"

/*
 * In the PIC cases, we have no guarantee that constants will keep
 * their alignment. This macro allows us to verify it at runtime.
 */
#define IS_ALIGNED(ptr, order) (((unsigned)ptr & ((1 << order) - 1)) == 0)

static unsigned int simd_support = ~0;

/*
 * Check what SIMD accelerations are supported.
 *
 * FIXME: This code is racy under a multi-threaded environment.
 */
LOCAL(void)
init_simd (void)
{
  if (simd_support != ~0U)
    return;
   
  simd_support &= JSIMD_LOONGSON;
}

GLOBAL(int)
jsimd_can_rgb_ycc (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_rgb_gray (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(void)
jsimd_rgb_ycc_convert (j_compress_ptr cinfo,
                       JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                       JDIMENSION output_row, int num_rows)
{
void (*mmifct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);      
switch(cinfo->in_color_space)
{
    case JCS_EXT_RGB:
        mmifct=jsimd_extrgb_ycc_convert_mmi;
        break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      mmifct=jsimd_extrgbx_ycc_convert_mmi;
      break;
    case JCS_EXT_BGR:
      mmifct=jsimd_extbgr_ycc_convert_mmi;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      mmifct=jsimd_extbgrx_ycc_convert_mmi;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      mmifct=jsimd_extxbgr_ycc_convert_mmi;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      mmifct=jsimd_extxrgb_ycc_convert_mmi;
      break;
    default:
      mmifct=jsimd_rgb_ycc_convert_mmi;
      break;
  }
    mmifct(cinfo->image_width, input_buf,
        output_buf, output_row, num_rows);

}

GLOBAL(void)
jsimd_rgb_gray_convert (j_compress_ptr cinfo,
                        JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                        JDIMENSION output_row, int num_rows)
{
//TODO
}

GLOBAL(void)
jsimd_ycc_rgb_convert (j_decompress_ptr cinfo,
                       JSAMPIMAGE input_buf, JDIMENSION input_row,
                       JSAMPARRAY output_buf, int num_rows)
{
  void (*mmifct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

  switch(cinfo->out_color_space)
  {
    case JCS_EXT_RGB:
      mmifct=jsimd_ycc_extrgb_convert_mmi;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      mmifct=jsimd_ycc_extrgbx_convert_mmi;
      break;
    case JCS_EXT_BGR:
      mmifct=jsimd_ycc_extbgr_convert_mmi;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      mmifct=jsimd_ycc_extbgrx_convert_mmi;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      mmifct=jsimd_ycc_extxbgr_convert_mmi;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      mmifct=jsimd_ycc_extxrgb_convert_mmi;
      break;
    default:
      mmifct=jsimd_ycc_rgb_convert_mmi;
      break;
  }


    mmifct(cinfo->output_width, input_buf,
        input_row, output_buf, num_rows);

}

GLOBAL(int)
jsimd_can_h2v2_downsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_downsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_h2v2_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
    jsimd_h2v2_downsample_mmi(cinfo->image_width, cinfo->max_v_samp_factor,
        compptr->v_samp_factor, compptr->width_in_blocks,
        input_data, output_data);
}

GLOBAL(void)
jsimd_h2v1_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
//TODO
}

GLOBAL(int)
jsimd_can_h2v2_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_h2v2_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info * compptr, 
                     JSAMPARRAY input_data,
                     JSAMPARRAY * output_data_ptr)
{
//TODO
}

GLOBAL(void)
jsimd_h2v1_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info * compptr, 
                     JSAMPARRAY input_data,
                     JSAMPARRAY * output_data_ptr)
{
//TODO
}

GLOBAL(int)
jsimd_can_h2v2_fancy_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_fancy_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info * compptr, 
                           JSAMPARRAY input_data,
                           JSAMPARRAY * output_data_ptr)
{
jsimd_h2v2_fancy_upsample_mmi(cinfo->max_v_samp_factor,
        compptr->downsampled_width, input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info * compptr, 
                           JSAMPARRAY input_data,
                           JSAMPARRAY * output_data_ptr)
{
//TODO
}

GLOBAL(int)
jsimd_can_h2v2_merged_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_merged_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf)
{
//TODO
}

GLOBAL(void)
jsimd_h2v1_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf)
{
//TODO
}

GLOBAL(int)
jsimd_can_convsamp (void)
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

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_convsamp_float (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_convsamp (JSAMPARRAY sample_data, JDIMENSION start_col,
                DCTELEM * workspace)
{
//TODO
}

GLOBAL(void)
jsimd_convsamp_float (JSAMPARRAY sample_data, JDIMENSION start_col,
                      FAST_FLOAT * workspace)
{
//TODO
}

GLOBAL(int)
jsimd_can_fdct_islow (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_ifast (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_float (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_fdct_islow (DCTELEM * data)
{
    jsimd_fdct_islow_mmi(data);
}

GLOBAL(void)
jsimd_fdct_ifast (DCTELEM * data)
{
//TODO
}

GLOBAL(void)
jsimd_fdct_float (FAST_FLOAT * data)
{
//TODO
}

GLOBAL(int)
jsimd_can_quantize (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_quantize_float (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_quantize (JCOEFPTR coef_block, DCTELEM * divisors,
                DCTELEM * workspace)
{
    jsimd_quantize_mmi(coef_block, divisors, workspace);
}

GLOBAL(void)
jsimd_quantize_float (JCOEFPTR coef_block, FAST_FLOAT * divisors,
                      FAST_FLOAT * workspace)
{
//TODO
}

GLOBAL(int)
jsimd_can_idct_2x2 (void)
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

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_idct_4x4 (void)
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

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_idct_2x2 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
//TODO
}

GLOBAL(void)
jsimd_idct_4x4 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
//TODO
}

GLOBAL(int)
jsimd_can_idct_islow (void)
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

  if (simd_support & JSIMD_LOONGSON)
    return 1; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_idct_ifast (void)
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

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(int)
jsimd_can_idct_float (void)
{
  init_simd();

  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;
  if (sizeof(FLOAT_MULT_TYPE) != 4)
    return 0;

  if (simd_support & JSIMD_LOONGSON)
    return 0; //TODO

  return 0;
}

GLOBAL(void)
jsimd_idct_islow (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
    jsimd_idct_islow_mmi(compptr->dct_table, coef_block, output_buf, output_col);
}

GLOBAL(void)
jsimd_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
//TODO
}

GLOBAL(void)
jsimd_idct_float (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
//TODO
}

GLOBAL(int)
jsimd_can_ycc_rgb565 (void)
{
  return 0;
}


GLOBAL(void)
jsimd_ycc_rgb565_convert (j_decompress_ptr cinfo,
                          JSAMPIMAGE input_buf, JDIMENSION input_row,
                          JSAMPARRAY output_buf, int num_rows)
{
}

GLOBAL(int)
jsimd_can_huff_encode_one_block (void)
{
  return 0;
}

GLOBAL(JOCTET*)
jsimd_huff_encode_one_block (void *state, JOCTET *buffer, JCOEFPTR block,
                             int last_dc_val, c_derived_tbl *dctbl,
                             c_derived_tbl *actbl)
{
  return NULL;
}

GLOBAL(int)
jsimd_c_can_null_convert (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_h2v2_smooth_downsample (void)
{
  return 0;
}


GLOBAL(int)
jsimd_can_idct_6x6 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_12x12 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_int_upsample (void)
{
  return 0;
}

GLOBAL(void)
jsimd_c_null_convert (j_compress_ptr cinfo,
                      JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                      JDIMENSION output_row, int num_rows)
{
}

GLOBAL(void)
jsimd_idct_6x6 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_int_upsample (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                      JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(void)
jsimd_h2v2_smooth_downsample (j_compress_ptr cinfo,
                              jpeg_component_info *compptr,
                              JSAMPARRAY input_data, JSAMPARRAY output_data)
{
}

GLOBAL(void)
jsimd_idct_12x12 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
}

