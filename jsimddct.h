/*
 * jsimddct.h
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 */

EXTERN(int) jsimd_can_convsamp JPP((void));
EXTERN(int) jsimd_can_convsamp_float JPP((void));

EXTERN(void) jsimd_convsamp JPP((JSAMPARRAY sample_data,
                                 JDIMENSION start_col,
                                 DCTELEM * workspace));
EXTERN(void) jsimd_convsamp_float JPP((JSAMPARRAY sample_data,
                                       JDIMENSION start_col,
                                       FAST_FLOAT * workspace));

EXTERN(int) jsimd_can_fdct_islow JPP((void));
EXTERN(int) jsimd_can_fdct_ifast JPP((void));
EXTERN(int) jsimd_can_fdct_float JPP((void));

EXTERN(void) jsimd_fdct_islow JPP((DCTELEM * data));
EXTERN(void) jsimd_fdct_ifast JPP((DCTELEM * data));
EXTERN(void) jsimd_fdct_float JPP((FAST_FLOAT * data));

EXTERN(int) jsimd_can_quantize JPP((void));
EXTERN(int) jsimd_can_quantize_float JPP((void));

EXTERN(void) jsimd_quantize JPP((JCOEFPTR coef_block,
                                 DCTELEM * divisors,
                                 DCTELEM * workspace));
EXTERN(void) jsimd_quantize_float JPP((JCOEFPTR coef_block,
                                       FAST_FLOAT * divisors,
                                       FAST_FLOAT * workspace));

EXTERN(int) jsimd_can_idct_2x2 JPP((void));
EXTERN(int) jsimd_can_idct_4x4 JPP((void));
EXTERN(int) jsimd_can_idct_6x6 JPP((void));
EXTERN(int) jsimd_can_idct_12x12 JPP((void));

EXTERN(void) jsimd_idct_2x2 JPP((j_decompress_ptr cinfo,
                                 jpeg_component_info * compptr,
                                 JCOEFPTR coef_block,
                                 JSAMPARRAY output_buf,
                                 JDIMENSION output_col));
EXTERN(void) jsimd_idct_4x4 JPP((j_decompress_ptr cinfo,
                                 jpeg_component_info * compptr,
                                 JCOEFPTR coef_block,
                                 JSAMPARRAY output_buf,
                                 JDIMENSION output_col));
EXTERN(void) jsimd_idct_6x6 JPP((j_decompress_ptr cinfo,
                                 jpeg_component_info * compptr,
                                 JCOEFPTR coef_block,
                                 JSAMPARRAY output_buf,
                                 JDIMENSION output_col));
EXTERN(void) jsimd_idct_12x12 JPP((j_decompress_ptr cinfo,
                                   jpeg_component_info * compptr,
                                   JCOEFPTR coef_block,
                                   JSAMPARRAY output_buf,
                                   JDIMENSION output_col));

EXTERN(int) jsimd_can_idct_islow JPP((void));
EXTERN(int) jsimd_can_idct_ifast JPP((void));
EXTERN(int) jsimd_can_idct_float JPP((void));

EXTERN(void) jsimd_idct_islow JPP((j_decompress_ptr cinfo,
                                   jpeg_component_info * compptr,
                                   JCOEFPTR coef_block,
                                   JSAMPARRAY output_buf,
                                   JDIMENSION output_col));
EXTERN(void) jsimd_idct_ifast JPP((j_decompress_ptr cinfo,
                                   jpeg_component_info * compptr,
                                   JCOEFPTR coef_block,
                                   JSAMPARRAY output_buf,
                                   JDIMENSION output_col));
EXTERN(void) jsimd_idct_float JPP((j_decompress_ptr cinfo,
                                   jpeg_component_info * compptr,
                                   JCOEFPTR coef_block,
                                   JSAMPARRAY output_buf,
                                   JDIMENSION output_col));

