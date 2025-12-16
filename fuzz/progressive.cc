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
 * This fuzzer targets the buffered-image mode for progressive JPEG decoding.
 *
 * Specifically targets:
 * - jpeg_has_multiple_scans()
 * - jpeg_start_output() / jpeg_finish_output()
 * - jpeg_consume_input() for incremental decoding
 * - Multi-scan progressive JPEG processing
 * - Coefficient buffer management
 *
 * Sanitizer support:
 * - MemorySanitizer: All output bytes are touched to detect uninitialized reads
 * - AddressSanitizer: Standard heap/stack checking
 * - UndefinedBehaviorSanitizer: Integer overflow, null pointer, etc.
 *
 * Coverage: Tests both progressive and non-progressive code paths.
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


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int row_stride;
  int64_t sum = 0;

  /* Reject too-small input */
  if (size < 2)
    return 0;

  /* Set up error handling */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, (unsigned long)size);

  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    goto bailout;

  /* Sanity check on dimensions to avoid OOM.  Casting width to (uint64_t)
     prevents integer overflow if width * height > INT_MAX. */
  if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
      (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
    goto bailout;

  /* Enable buffered-image mode for progressive decoding.  This allows us to
   * access image data while the image is still being decoded. */
  cinfo.buffered_image = TRUE;

  /* Check if the image actually has multiple scans */
  if (!jpeg_has_multiple_scans(&cinfo)) {
    /* For non-progressive images, just do a regular decode */
    cinfo.buffered_image = FALSE;
    if (!jpeg_start_decompress(&cinfo))
      goto bailout;

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
      ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      for (int i = 0; i < row_stride; i++)
        sum += buffer[0][i];
    }

    jpeg_finish_decompress(&cinfo);
    goto bailout;
  }

  /* Start decompression in buffered-image mode */
  if (!jpeg_start_decompress(&cinfo))
    goto bailout;

  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Process all scans of the progressive JPEG */
  while (!jpeg_input_complete(&cinfo)) {
    int retval;

    /* Consume input data until we have a complete scan or reach end of input */
    do {
      retval = jpeg_consume_input(&cinfo);
    } while (retval != JPEG_SUSPENDED && retval != JPEG_REACHED_SOS &&
             retval != JPEG_REACHED_EOI);

    if (retval == JPEG_REACHED_EOI)
      break;

    /* Start outputting the current scan */
    if (!jpeg_start_output(&cinfo, cinfo.input_scan_number))
      goto bailout;

    /* Read all scanlines from this scan */
    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      /* Touch output to catch uninitialized reads with MemorySanitizer */
      for (int i = 0; i < row_stride; i++)
        sum += buffer[0][i];
    }

    /* Finish this output pass */
    if (!jpeg_finish_output(&cinfo))
      goto bailout;
  }

  /* Final output pass after all scans have been read */
  if (jpeg_start_output(&cinfo, cinfo.input_scan_number)) {
    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      for (int i = 0; i < row_stride; i++)
        sum += buffer[0][i];
    }
    jpeg_finish_output(&cinfo);
  }

  jpeg_finish_decompress(&cinfo);

bailout:
  jpeg_destroy_decompress(&cinfo);

  /* Prevent the code above from being optimized out.  This test should never
     be true, but the compiler doesn't know that. */
  PREVENT_OPTIMIZATION(sum);
  if (sum > (int64_t)255 * 1048576 * 4)
    return 1;

  return 0;
}
