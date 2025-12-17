/*
 * Copyright (C)2021-2024 D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2025 Leslie P. Polzer.  All Rights Reserved.
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
 * This fuzzer targets the raw libjpeg API (as opposed to TurboJPEG API)
 * to exercise code paths that might not be covered by the TurboJPEG fuzzer.
 *
 * Specifically targets:
 * - DHT (Huffman table) marker processing
 * - Slow-path Huffman decode in jpeg_huff_decode() (codes > 8 bits)
 * - Edge cases in valoffset calculations
 * - Multiple output colorspaces including JCS_UNKNOWN
 * - DCT scaling factors
 * - Various decompression options (fancy upsampling, block smoothing, etc.)
 *
 * Sanitizer support:
 * - MemorySanitizer: All output bytes are touched to detect uninitialized reads
 * - AddressSanitizer: Standard heap/stack checking
 * - UndefinedBehaviorSanitizer: Integer overflow, null pointer, etc.
 *
 * Coverage: Multiple test configurations maximize code path coverage.
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


/* Test configuration structure */
struct test_config {
  J_COLOR_SPACE out_color_space;
  int scale_num;
  int scale_denom;
  boolean do_fancy_upsampling;
  boolean do_block_smoothing;
  J_DCT_METHOD dct_method;
  J_DITHER_MODE dither_mode;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int row_stride;
  int ti;

  /* Various test configurations to exercise different code paths */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: RGB output with 1/1 scale, fast DCT */
    { JCS_RGB, 1, 1, TRUE, TRUE, JDCT_IFAST, JDITHER_NONE },
    /* Test 1: Grayscale output with 1/2 scale, integer DCT */
    { JCS_GRAYSCALE, 1, 2, FALSE, FALSE, JDCT_ISLOW, JDITHER_ORDERED },
    /* Test 2: RGB output with 1/4 scale, float DCT */
    { JCS_RGB, 1, 4, TRUE, FALSE, JDCT_FLOAT, JDITHER_FS },
    /* Test 3: CMYK output (for CMYK JPEGs), 1/8 scale */
    { JCS_CMYK, 1, 8, FALSE, TRUE, JDCT_ISLOW, JDITHER_NONE },
    /* Test 4: YCbCr output (no color conversion), 3/4 scale */
    { JCS_YCbCr, 3, 4, TRUE, TRUE, JDCT_IFAST, JDITHER_NONE },
    /* Test 5: Unknown colorspace (pass-through), 1/1 scale */
    { JCS_UNKNOWN, 1, 1, FALSE, FALSE, JDCT_ISLOW, JDITHER_NONE }
  };

  /* Reject too-small input */
  if (size < 2)
    return 0;

  /* Set up error handling */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  for (ti = 0; ti < NUMTESTS; ti++) {
    int64_t sum = 0;

    if (setjmp(jerr.setjmp_buffer)) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, (unsigned long)size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    /* Sanity check on dimensions to avoid OOM.  Casting width to (uint64_t)
       prevents integer overflow if width * height > INT_MAX. */
    if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
        (uint64_t)cinfo.image_width * cinfo.image_height > 1048576) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    /* Apply test configuration */
    cinfo.out_color_space = tests[ti].out_color_space;
    cinfo.scale_num = tests[ti].scale_num;
    cinfo.scale_denom = tests[ti].scale_denom;
    cinfo.do_fancy_upsampling = tests[ti].do_fancy_upsampling;
    cinfo.do_block_smoothing = tests[ti].do_block_smoothing;
    cinfo.dct_method = tests[ti].dct_method;
    cinfo.dither_mode = tests[ti].dither_mode;

    /* Handle colorspace compatibility:
     * - CMYK output only works with CMYK/YCCK input
     * - YCbCr output requires YCbCr input
     * - Unknown passes through without conversion
     */
    if (tests[ti].out_color_space == JCS_CMYK &&
        cinfo.jpeg_color_space != JCS_CMYK &&
        cinfo.jpeg_color_space != JCS_YCCK) {
      /* Fall back to RGB for non-CMYK images */
      cinfo.out_color_space = JCS_RGB;
    }
    if (tests[ti].out_color_space == JCS_YCbCr &&
        cinfo.jpeg_color_space != JCS_YCbCr) {
      /* Fall back to RGB for non-YCbCr images */
      cinfo.out_color_space = JCS_RGB;
    }
    if (tests[ti].out_color_space == JCS_UNKNOWN) {
      /* For unknown, match the input components */
      cinfo.out_color_components = cinfo.num_components;
    }

    if (!jpeg_start_decompress(&cinfo)) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
      ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    /* Read all scanlines - this exercises Huffman decoding including
     * the slow-path jpeg_huff_decode() where OOB reads can occur */
    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      /* Touch all of the output pixels in order to catch uninitialized reads
         when using MemorySanitizer. */
      for (int i = 0; i < row_stride; i++)
        sum += buffer[0][i];
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* This test should never be true, but it provides a side effect that
       prevents the sum calculation from being optimized out. */
    if (sum > (int64_t)255 * 1048576 * 4)
      return 1;
  }

  return 0;
}
