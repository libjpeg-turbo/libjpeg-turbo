/*
 * jsimddct.h
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2025, D. R. Commander.
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

#ifdef WITH_SIMD

#include "jdct.h"
#include "../simd/jsimdconst.h"

/* Sample Conversion */
EXTERN(unsigned int) jsimd_set_convsamp(j_compress_ptr cinfo,
                                        convsamp_method_ptr *method);

EXTERN(unsigned int) jsimd_set_convsamp_float
  (j_compress_ptr cinfo, float_convsamp_method_ptr *method);

/* Forward DCT */
EXTERN(unsigned int) jsimd_set_fdct_islow(j_compress_ptr cinfo,
                                          forward_DCT_method_ptr *method);

EXTERN(unsigned int) jsimd_set_fdct_ifast(j_compress_ptr cinfo,
                                          forward_DCT_method_ptr *method);

EXTERN(unsigned int) jsimd_set_fdct_float(j_compress_ptr cinfo,
                                          float_DCT_method_ptr *method);

/* Quantization */
EXTERN(unsigned int) jsimd_set_quantize(j_compress_ptr cinfo,
                                        quantize_method_ptr *method);

EXTERN(unsigned int) jsimd_set_quantize_float
  (j_compress_ptr cinfo, float_quantize_method_ptr *method);

/* Inverse DCT */
EXTERN(unsigned int) jsimd_set_idct_islow(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_islow(j_decompress_ptr cinfo,
                              jpeg_component_info *compptr,
                              JCOEFPTR coef_block, JSAMPARRAY output_buf,
                              JDIMENSION output_col);

EXTERN(unsigned int) jsimd_set_idct_ifast(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_ifast(j_decompress_ptr cinfo,
                              jpeg_component_info *compptr,
                              JCOEFPTR coef_block, JSAMPARRAY output_buf,
                              JDIMENSION output_col);

EXTERN(unsigned int) jsimd_set_idct_float(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_float(j_decompress_ptr cinfo,
                              jpeg_component_info *compptr,
                              JCOEFPTR coef_block, JSAMPARRAY output_buf,
                              JDIMENSION output_col);

/* Scaled Inverse DCT */
EXTERN(unsigned int) jsimd_set_idct_2x2(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_2x2(j_decompress_ptr cinfo,
                            jpeg_component_info *compptr, JCOEFPTR coef_block,
                            JSAMPARRAY output_buf, JDIMENSION output_col);

EXTERN(unsigned int) jsimd_set_idct_4x4(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_4x4(j_decompress_ptr cinfo,
                            jpeg_component_info *compptr, JCOEFPTR coef_block,
                            JSAMPARRAY output_buf, JDIMENSION output_col);

#if SIMD_ARCHITECTURE == MIPS
EXTERN(unsigned int) jsimd_set_idct_6x6(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_6x6(j_decompress_ptr cinfo,
                            jpeg_component_info *compptr, JCOEFPTR coef_block,
                            JSAMPARRAY output_buf, JDIMENSION output_col);

EXTERN(unsigned int) jsimd_set_idct_12x12(j_decompress_ptr cinfo);
EXTERN(void) jsimd_idct_12x12(j_decompress_ptr cinfo,
                              jpeg_component_info *compptr,
                              JCOEFPTR coef_block, JSAMPARRAY output_buf,
                              JDIMENSION output_col);
#endif

#endif /* WITH_SIMD */
