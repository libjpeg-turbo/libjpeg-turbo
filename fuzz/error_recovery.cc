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
 * This fuzzer targets error handling and recovery paths in libjpeg.
 *
 * Specifically targets:
 * - jpeg_abort() and jpeg_abort_decompress() cleanup paths
 * - jpeg_resync_to_restart() marker resynchronization
 * - Error recovery after partial decode
 * - Multiple decode attempts with same decompressor object
 * - Memory cleanup after errors (use-after-free, double-free detection)
 * - State machine transitions on error
 *
 * Sanitizer support:
 * - MemorySanitizer: Detects use of uninitialized memory after errors
 * - AddressSanitizer: Detects use-after-free in error cleanup
 * - UndefinedBehaviorSanitizer: Detects undefined state after errors
 *
 * Coverage: Tests error paths that normal fuzzers skip via longjmp.
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


/* Error handler that counts errors and optionally continues */
struct fuzzer_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
  int error_count;
  boolean continue_on_error;
};


static void fuzzer_error_exit(j_common_ptr cinfo)
{
  fuzzer_error_mgr *myerr = (fuzzer_error_mgr *)cinfo->err;
  myerr->error_count++;

  if (myerr->continue_on_error && myerr->error_count < 10) {
    /* Try to continue - exercises error recovery paths */
    return;
  }

  longjmp(myerr->setjmp_buffer, 1);
}


static void fuzzer_emit_message(j_common_ptr cinfo, int msg_level)
{
  (void)cinfo;
  (void)msg_level;
}


/* Custom output message that doesn't print anything */
static void fuzzer_output_message(j_common_ptr cinfo)
{
  (void)cinfo;
}


/*
 * Test 1: Abort during header reading
 * Exercises jpeg_abort_decompress() cleanup when header is partially parsed.
 */
static int test_abort_during_header(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  int64_t sum = 0;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;
  jerr.pub.output_message = fuzzer_output_message;
  jerr.error_count = 0;
  jerr.continue_on_error = FALSE;

  if (setjmp(jerr.setjmp_buffer)) {
    /* Error during header - test abort cleanup */
    jpeg_abort_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, (unsigned long)size);

  /* Read header - may fail on malformed input */
  int header_result = jpeg_read_header(&cinfo, FALSE);

  /* Abort regardless of result to test cleanup path */
  jpeg_abort_decompress(&cinfo);

  /* Try to reuse the object after abort */
  jpeg_mem_src(&cinfo, data, (unsigned long)size);
  if (jpeg_read_header(&cinfo, FALSE) == JPEG_HEADER_OK) {
    sum += cinfo.image_width + cinfo.image_height;
  }

  jpeg_destroy_decompress(&cinfo);

  PREVENT_OPTIMIZATION(sum);
  return (sum > 0xFFFFFFFF) ? 1 : 0;
}


/*
 * Test 2: Abort during decompression
 * Exercises cleanup when scan data is partially decoded.
 */
static int test_abort_during_decompress(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int64_t sum = 0;
  int row_stride;
  JDIMENSION target;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;
  jerr.pub.output_message = fuzzer_output_message;
  jerr.error_count = 0;
  jerr.continue_on_error = FALSE;

  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_abort_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, (unsigned long)size);

  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    goto bailout;

  if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
      (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
    goto bailout;

  if (!jpeg_start_decompress(&cinfo))
    goto bailout;

  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Read only half the scanlines, then abort */
  target = cinfo.output_height / 2;
  while (cinfo.output_scanline < target) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    for (int i = 0; i < row_stride; i++)
      sum += buffer[0][i];
  }

  /* Abort mid-decode - tests partial cleanup */
  jpeg_abort_decompress(&cinfo);

  /* Try to reuse after abort */
  jpeg_mem_src(&cinfo, data, (unsigned long)size);
  if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
    if (jpeg_start_decompress(&cinfo)) {
      while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        for (int i = 0; i < row_stride; i++)
          sum += buffer[0][i];
      }
      jpeg_finish_decompress(&cinfo);
    }
  }

bailout:
  jpeg_destroy_decompress(&cinfo);

  PREVENT_OPTIMIZATION(sum);
  return (sum > (int64_t)255 * 1048576 * 4) ? 1 : 0;
}


/*
 * Test 3: Continue after errors
 * Exercises error recovery paths when errors don't immediately abort.
 */
static int test_continue_after_error(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int64_t sum = 0;
  int row_stride;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;
  jerr.pub.output_message = fuzzer_output_message;
  jerr.error_count = 0;
  jerr.continue_on_error = TRUE;  /* Try to continue on errors */

  if (setjmp(jerr.setjmp_buffer)) {
    /* Too many errors - final bailout */
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, (unsigned long)size);

  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    goto bailout;

  if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
      (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
    goto bailout;

  if (!jpeg_start_decompress(&cinfo))
    goto bailout;

  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Read all scanlines, continuing even if errors occur */
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Touch all output pixels for MSan */
    for (int i = 0; i < row_stride; i++)
      sum += buffer[0][i];
  }

  jpeg_finish_decompress(&cinfo);

bailout:
  jpeg_destroy_decompress(&cinfo);

  PREVENT_OPTIMIZATION(sum);
  return (sum > (int64_t)255 * 1048576 * 4) ? 1 : 0;
}


/*
 * Test 4: Multiple decode cycles
 * Reuses decompressor object to detect state leaks between decodes.
 */
static int test_multiple_decodes(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int64_t sum = 0;
  int cycle;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;
  jerr.pub.output_message = fuzzer_output_message;
  jerr.error_count = 0;
  jerr.continue_on_error = FALSE;

  jpeg_create_decompress(&cinfo);

  /* Run multiple decode cycles with same object */
  for (cycle = 0; cycle < 3; cycle++) {
    jerr.error_count = 0;

    if (setjmp(jerr.setjmp_buffer)) {
      /* Error in this cycle - abort and try next */
      jpeg_abort_decompress(&cinfo);
      continue;
    }

    jpeg_mem_src(&cinfo, data, (unsigned long)size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
      continue;

    if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
        (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
      continue;

    /* Vary parameters between cycles to exercise different paths */
    cinfo.out_color_space = (cycle == 0) ? JCS_RGB :
                            (cycle == 1) ? JCS_GRAYSCALE : JCS_RGB;
    cinfo.scale_num = 1;
    cinfo.scale_denom = (cycle == 0) ? 1 : (cycle == 1) ? 2 : 4;

    if (!jpeg_start_decompress(&cinfo))
      continue;

    int row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
      ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      for (int i = 0; i < row_stride; i++)
        sum += buffer[0][i];
    }

    jpeg_finish_decompress(&cinfo);
  }

  jpeg_destroy_decompress(&cinfo);

  PREVENT_OPTIMIZATION(sum);
  return (sum > (int64_t)255 * 1048576 * 4 * 3) ? 1 : 0;
}


/*
 * Test 5: Generic jpeg_abort() function
 * Tests the generic abort that works on both compress and decompress.
 */
static int test_generic_abort(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  int64_t sum = 0;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;
  jerr.pub.output_message = fuzzer_output_message;
  jerr.error_count = 0;
  jerr.continue_on_error = FALSE;

  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_abort((j_common_ptr)&cinfo);
    jpeg_destroy((j_common_ptr)&cinfo);
    return 0;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, (unsigned long)size);

  /* Use generic abort at various stages */
  if (jpeg_read_header(&cinfo, FALSE) == JPEG_HEADER_OK) {
    sum += cinfo.image_width;

    /* Generic abort */
    jpeg_abort((j_common_ptr)&cinfo);

    /* Reuse */
    jpeg_mem_src(&cinfo, data, (unsigned long)size);
    if (jpeg_read_header(&cinfo, FALSE) == JPEG_HEADER_OK) {
      sum += cinfo.image_height;
    }
  }

  /* Generic destroy */
  jpeg_destroy((j_common_ptr)&cinfo);

  PREVENT_OPTIMIZATION(sum);
  return (sum > 0xFFFFFFFF) ? 1 : 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < 2)
    return 0;

  /* Run all error recovery tests */
  test_abort_during_header(data, size);
  test_abort_during_decompress(data, size);
  test_continue_after_error(data, size);
  test_multiple_decodes(data, size);
  test_generic_abort(data, size);

  return 0;
}
