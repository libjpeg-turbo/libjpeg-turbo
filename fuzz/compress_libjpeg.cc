/*
 * Copyright (C)2021-2025 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This fuzzer targets the raw libjpeg compression API.
 *
 * The fuzz input is treated as raw pixel data to compress, exercising:
 * - jpeg_set_defaults() / jpeg_set_quality() / jpeg_set_colorspace()
 * - jpeg_start_compress() / jpeg_write_scanlines() / jpeg_finish_compress()
 * - jpeg_write_marker() for custom markers
 * - jpeg_write_icc_profile() for ICC profile embedding
 * - Huffman table generation from input data
 * - Various subsampling factors and quality levels
 *
 * Sanitizer support:
 * - MemorySanitizer: All compressed output bytes are touched
 * - AddressSanitizer: Buffer bounds in compression
 * - UndefinedBehaviorSanitizer: Integer operations
 *
 * Coverage: Tests compression paths not covered by TurboJPEG API.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

extern "C" {
#include "jpeglib.h"
#include "jerror.h"
}


/* Ensure sum variable is not optimized out - visible to sanitizers */
#if defined(__clang__) || defined(__GNUC__)
#define PREVENT_OPTIMIZATION(x) __asm__ volatile("" : "+r"(x))
#else
#define PREVENT_OPTIMIZATION(x) ((void)(x))
#endif


#define NUMTESTS 6


/* Error handler that uses longjmp to return control to fuzzer */
struct fuzzer_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};


static void fuzzer_error_exit(j_common_ptr cinfo)
{
  fuzzer_error_mgr *myerr = (fuzzer_error_mgr *)cinfo->err;
  longjmp(myerr->setjmp_buffer, 1);
}


static void fuzzer_emit_message(j_common_ptr cinfo, int msg_level)
{
  (void)cinfo;
  (void)msg_level;
}


/* Test configuration */
struct test_config {
  int quality;
  J_COLOR_SPACE colorspace;
  int h_samp;  /* Horizontal sampling factor for component 0 */
  int v_samp;  /* Vertical sampling factor for component 0 */
  boolean optimize_coding;
  boolean progressive;
  boolean arithmetic;
  boolean write_marker;
  boolean write_icc;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_compress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  unsigned char *outbuffer = NULL;
  unsigned long outsize = 0;
  int64_t sum = 0;
  int ti;

  /* Test configurations */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: RGB, 4:4:4, quality 90, optimized Huffman */
    { 90, JCS_RGB, 1, 1, TRUE, FALSE, FALSE, FALSE, FALSE },
    /* Test 1: YCbCr, 4:2:0, quality 75, progressive */
    { 75, JCS_YCbCr, 2, 2, TRUE, TRUE, FALSE, FALSE, FALSE },
    /* Test 2: Grayscale, quality 50 */
    { 50, JCS_GRAYSCALE, 1, 1, FALSE, FALSE, FALSE, FALSE, FALSE },
    /* Test 3: YCbCr, 4:2:2, quality 85, with custom marker */
    { 85, JCS_YCbCr, 2, 1, TRUE, FALSE, FALSE, TRUE, FALSE },
    /* Test 4: RGB, quality 95, with ICC profile */
    { 95, JCS_RGB, 1, 1, TRUE, FALSE, FALSE, FALSE, TRUE },
    /* Test 5: YCbCr, arithmetic coding */
    { 80, JCS_YCbCr, 2, 2, FALSE, FALSE, TRUE, FALSE, FALSE }
  };

  /* Need at least enough data for a minimal image */
  if (size < 16)
    return 0;

  /* Derive image dimensions from input size.
   * Use first 4 bytes to influence width/height. */
  unsigned int width = (data[0] % 64) + 8;   /* 8-71 */
  unsigned int height = (data[1] % 64) + 8;  /* 8-71 */
  int num_components = (data[2] % 2) + 1;    /* 1 or 3 (grayscale or RGB) */
  if (num_components == 1)
    num_components = 1;
  else
    num_components = 3;

  /* Calculate required input size */
  size_t required_size = 4 + (size_t)width * height * num_components;
  if (size < required_size) {
    /* Not enough data - use smaller dimensions */
    width = 8;
    height = 8;
    required_size = 4 + (size_t)width * height * num_components;
    if (size < required_size)
      return 0;
  }

  /* Skip header bytes */
  const uint8_t *pixel_data = data + 4;
  size_t pixel_size = size - 4;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  for (ti = 0; ti < NUMTESTS; ti++) {
    /* Skip grayscale test if we have RGB data and vice versa */
    if (tests[ti].colorspace == JCS_GRAYSCALE && num_components != 1)
      continue;
    if (tests[ti].colorspace != JCS_GRAYSCALE && num_components != 3)
      continue;

    if (setjmp(jerr.setjmp_buffer)) {
      free(outbuffer);
      outbuffer = NULL;
      jpeg_destroy_compress(&cinfo);
      continue;
    }

    jpeg_create_compress(&cinfo);

    outbuffer = NULL;
    outsize = 0;
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = num_components;
    cinfo.in_color_space = (num_components == 1) ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, tests[ti].quality, TRUE);

    /* Set colorspace (may differ from input for testing conversion) */
    if (tests[ti].colorspace != JCS_GRAYSCALE || num_components == 1)
      jpeg_set_colorspace(&cinfo, tests[ti].colorspace);

    /* Set sampling factors */
    if (cinfo.num_components > 0) {
      cinfo.comp_info[0].h_samp_factor = tests[ti].h_samp;
      cinfo.comp_info[0].v_samp_factor = tests[ti].v_samp;
    }

    cinfo.optimize_coding = tests[ti].optimize_coding;
    cinfo.arith_code = tests[ti].arithmetic;

    if (tests[ti].progressive)
      jpeg_simple_progression(&cinfo);

    jpeg_start_compress(&cinfo, TRUE);

    /* Write custom APP marker if configured */
    if (tests[ti].write_marker && pixel_size >= 16) {
      /* Use some of the input data as marker content */
      jpeg_write_marker(&cinfo, JPEG_APP0 + 1, pixel_data, 16);
    }

    /* Write ICC profile if configured */
    if (tests[ti].write_icc && pixel_size >= 32) {
      /* Use some input data as fake ICC profile */
      jpeg_write_icc_profile(&cinfo, pixel_data, 32);
    }

    /* Write scanlines */
    JSAMPROW row_pointer[1];
    int row_stride = width * num_components;

    while (cinfo.next_scanline < cinfo.image_height) {
      size_t offset = cinfo.next_scanline * row_stride;
      if (offset + row_stride > pixel_size)
        break;
      row_pointer[0] = (JSAMPROW)(pixel_data + offset);
      jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    /* Touch all output bytes for MSan */
    for (unsigned long i = 0; i < outsize; i++)
      sum += outbuffer[i];

    free(outbuffer);
    outbuffer = NULL;

    jpeg_destroy_compress(&cinfo);
  }

  PREVENT_OPTIMIZATION(sum);
  if (sum > (int64_t)255 * 1048576)
    return 1;

  return 0;
}
