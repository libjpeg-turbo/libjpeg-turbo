/*
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
 * This fuzzer targets partial decoding features of the libjpeg API.
 *
 * Specifically targets:
 * - jpeg_skip_scanlines() for skipping rows
 * - jpeg_crop_scanline() for cropping columns
 * - Boundary conditions in partial decode
 * - Interaction between skip and crop operations
 *
 * Sanitizer support:
 * - MemorySanitizer: All decoded pixels are touched
 * - AddressSanitizer: Standard heap/stack checking
 * - UndefinedBehaviorSanitizer: Integer overflow, null pointer, etc.
 *
 * Coverage: Multiple skip/crop configurations test boundary conditions.
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


#define NUMTESTS 4


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
  /* Fraction of height to skip at start (0-3 maps to 0, 1/4, 1/2, 3/4) */
  int skip_fraction;
  /* Fraction of width to crop (0-3 maps to full, 3/4, 1/2, 1/4) */
  int crop_fraction;
  /* Whether to alternate between skip and read */
  boolean alternate_skip;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int ti;

  /* Various test configurations */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: Skip first quarter, read rest, full width */
    { 1, 0, FALSE },
    /* Test 1: Skip first half, read rest, crop to 3/4 width */
    { 2, 1, FALSE },
    /* Test 2: Skip 3/4, read rest, crop to half width */
    { 3, 2, FALSE },
    /* Test 3: Alternate skip and read, crop to quarter width */
    { 0, 3, TRUE }
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
    JDIMENSION skip_lines, crop_x, crop_width;
    int row_stride;

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

    /* Need minimum dimensions for meaningful partial decode testing */
    if (cinfo.image_width < 32 || cinfo.image_height < 32) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    if (!jpeg_start_decompress(&cinfo)) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    /* Calculate crop parameters based on test configuration */
    switch (tests[ti].crop_fraction) {
      case 1:  /* 3/4 width */
        crop_width = (cinfo.output_width * 3) / 4;
        crop_x = cinfo.output_width / 8;
        break;
      case 2:  /* 1/2 width */
        crop_width = cinfo.output_width / 2;
        crop_x = cinfo.output_width / 4;
        break;
      case 3:  /* 1/4 width */
        crop_width = cinfo.output_width / 4;
        crop_x = cinfo.output_width / 8;
        break;
      default: /* full width */
        crop_width = cinfo.output_width;
        crop_x = 0;
        break;
    }

    /* Apply cropping if specified */
    if (tests[ti].crop_fraction > 0 && crop_width > 0) {
      jpeg_crop_scanline(&cinfo, &crop_x, &crop_width);
    }

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
      ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    /* Calculate skip lines based on test configuration */
    skip_lines = (cinfo.output_height * tests[ti].skip_fraction) / 4;

    if (tests[ti].alternate_skip) {
      /* Alternate between skipping and reading groups of 8 lines */
      JDIMENSION group_size = 8;
      boolean do_skip = TRUE;

      while (cinfo.output_scanline < cinfo.output_height) {
        JDIMENSION remaining = cinfo.output_height - cinfo.output_scanline;
        JDIMENSION to_process = (remaining < group_size) ? remaining : group_size;

        if (do_skip && to_process > 0) {
          jpeg_skip_scanlines(&cinfo, to_process);
        } else {
          while (to_process > 0 &&
                 cinfo.output_scanline < cinfo.output_height) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            for (int i = 0; i < row_stride; i++)
              sum += buffer[0][i];
            to_process--;
          }
        }
        do_skip = !do_skip;
      }
    } else {
      /* Skip initial lines */
      if (skip_lines > 0 && skip_lines < cinfo.output_height) {
        jpeg_skip_scanlines(&cinfo, skip_lines);
      }

      /* Read remaining lines */
      while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        for (int i = 0; i < row_stride; i++)
          sum += buffer[0][i];
      }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* Prevent the code above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > (int64_t)255 * 1048576 * 4)
      return 1;
  }

  return 0;
}
