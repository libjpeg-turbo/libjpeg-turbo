/*
 * jsimd.h
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2011, 2014, 2022, 2025, D. R. Commander.
 * Copyright (C) 2015-2016, 2018, 2022, Matthieu Darbois.
 * Copyright (C) 2020, Arm Limited.
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 */

#include "jinclude.h"
#include "jpeglib.h"

#ifdef WITH_SIMD

#include "jchuff.h"
#include "../simd/jsimdconst.h"

/* Colorspace Conversion */
EXTERN(unsigned int) jsimd_set_rgb_ycc(j_compress_ptr cinfo);

EXTERN(unsigned int) jsimd_set_rgb_gray(j_compress_ptr cinfo);

EXTERN(void) jsimd_color_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                                 JSAMPIMAGE output_buf, JDIMENSION output_row,
                                 int num_rows);

#if SIMD_ARCHITECTURE == MIPS
EXTERN(unsigned int) jsimd_set_c_null_convert(j_compress_ptr cinfo);
EXTERN(void) jsimd_c_null_convert(j_compress_ptr cinfo, JSAMPARRAY input_buf,
                                  JSAMPIMAGE output_buf, JDIMENSION output_row,
                                  int num_rows);
#endif

EXTERN(unsigned int) jsimd_set_ycc_rgb(j_decompress_ptr cinfo);

EXTERN(unsigned int) jsimd_set_ycc_rgb565(j_decompress_ptr cinfo);

EXTERN(void) jsimd_color_deconvert(j_decompress_ptr cinfo,
                                   JSAMPIMAGE input_buf, JDIMENSION input_row,
                                   JSAMPARRAY output_buf, int num_rows);

/* Downsampling */
EXTERN(unsigned int) jsimd_set_h2v1_downsample(j_compress_ptr cinfo);
EXTERN(void) jsimd_h2v1_downsample(j_compress_ptr cinfo,
                                   jpeg_component_info *compptr,
                                   JSAMPARRAY input_data,
                                   JSAMPARRAY output_data);

EXTERN(unsigned int) jsimd_set_h2v2_downsample(j_compress_ptr cinfo);
EXTERN(void) jsimd_h2v2_downsample(j_compress_ptr cinfo,
                                   jpeg_component_info *compptr,
                                   JSAMPARRAY input_data,
                                   JSAMPARRAY output_data);

/* Smooth Downsampling */
#if SIMD_ARCHITECTURE == MIPS
EXTERN(unsigned int) jsimd_set_h2v2_smooth_downsample(j_compress_ptr cinfo);
EXTERN(void) jsimd_h2v2_smooth_downsample(j_compress_ptr cinfo,
                                          jpeg_component_info *compptr,
                                          JSAMPARRAY input_data,
                                          JSAMPARRAY output_data);
#endif

/* Upsampling */
EXTERN(unsigned int) jsimd_set_h2v1_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h2v1_upsample(j_decompress_ptr cinfo,
                                 jpeg_component_info *compptr,
                                 JSAMPARRAY input_data,
                                 JSAMPARRAY *output_data_ptr);

EXTERN(unsigned int) jsimd_set_h2v2_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h2v2_upsample(j_decompress_ptr cinfo,
                                 jpeg_component_info *compptr,
                                 JSAMPARRAY input_data,
                                 JSAMPARRAY *output_data_ptr);

#if SIMD_ARCHITECTURE == MIPS
EXTERN(unsigned int) jsimd_set_int_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_int_upsample(j_decompress_ptr cinfo,
                                jpeg_component_info *compptr,
                                JSAMPARRAY input_data,
                                JSAMPARRAY *output_data_ptr);
#endif

/* Fancy (Smooth) Upsampling */
EXTERN(unsigned int) jsimd_set_h2v1_fancy_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h2v1_fancy_upsample(j_decompress_ptr cinfo,
                                       jpeg_component_info *compptr,
                                       JSAMPARRAY input_data,
                                       JSAMPARRAY *output_data_ptr);

EXTERN(unsigned int) jsimd_set_h2v2_fancy_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h2v2_fancy_upsample(j_decompress_ptr cinfo,
                                       jpeg_component_info *compptr,
                                       JSAMPARRAY input_data,
                                       JSAMPARRAY *output_data_ptr);

#if SIMD_ARCHITECTURE == ARM64 || SIMD_ARCHITECTURE == ARM
EXTERN(unsigned int) jsimd_set_h1v2_fancy_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h1v2_fancy_upsample(j_decompress_ptr cinfo,
                                       jpeg_component_info *compptr,
                                       JSAMPARRAY input_data,
                                       JSAMPARRAY *output_data_ptr);
#endif

/* Merged Upsampling/Color Conversion */
EXTERN(unsigned int) jsimd_set_h2v1_merged_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h2v1_merged_upsample(j_decompress_ptr cinfo,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf);

EXTERN(unsigned int) jsimd_set_h2v2_merged_upsample(j_decompress_ptr cinfo);
EXTERN(void) jsimd_h2v2_merged_upsample(j_decompress_ptr cinfo,
                                        JSAMPIMAGE input_buf,
                                        JDIMENSION in_row_group_ctr,
                                        JSAMPARRAY output_buf);

/* Huffman Encoding */
EXTERN(unsigned int) jsimd_set_huff_encode_one_block(j_compress_ptr cinfo);

/* Progressive Huffman Encoding */
EXTERN(unsigned int) jsimd_set_encode_mcu_AC_first_prepare
  (j_compress_ptr cinfo,
   void (**method) (const JCOEF *, const int *, int, int, UJCOEF *, size_t *));

EXTERN(unsigned int) jsimd_set_encode_mcu_AC_refine_prepare
  (j_compress_ptr cinfo,
   int (**method) (const JCOEF *, const int *, int, int, UJCOEF *, size_t *));

#endif /* WITH_SIMD */
