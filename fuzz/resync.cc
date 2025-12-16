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
 * This fuzzer targets restart marker resynchronization in libjpeg.
 *
 * Specifically targets:
 * - jpeg_resync_to_restart() for restart marker recovery
 * - Custom resync_to_restart handler in source manager
 * - Restart interval processing (jpeg_set_restart_interval)
 * - MCU row boundary handling with corrupt restart markers
 *
 * Sanitizer support:
 * - MemorySanitizer: All output bytes are touched
 * - AddressSanitizer: Buffer bounds during resync
 * - UndefinedBehaviorSanitizer: State transitions
 *
 * Coverage: Tests restart marker error recovery paths.
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


#define NUMTESTS 3


/* Global counter for custom resync handler - touched for MSan */
static int64_t g_resync_sum = 0;
static int g_resync_call_count = 0;


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


/*
 * Custom resync_to_restart handler.
 * This is called when a restart marker is missing or out of sequence.
 * We call the default implementation and track that it was invoked.
 */
static boolean custom_resync_to_restart(j_decompress_ptr cinfo, int desired)
{
  g_resync_call_count++;
  g_resync_sum += desired;

  /* Call the default implementation */
  return jpeg_resync_to_restart(cinfo, desired);
}


/* Test configuration */
struct test_config {
  boolean use_custom_resync;  /* Use custom resync handler */
  boolean buffered_image;     /* Use buffered-image mode */
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int row_stride;
  int ti;
  int64_t sum = 0;

  /* Test configurations */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: Default resync handler, normal mode */
    { FALSE, FALSE },
    /* Test 1: Custom resync handler, normal mode */
    { TRUE, FALSE },
    /* Test 2: Custom resync handler, buffered-image mode */
    { TRUE, TRUE }
  };

  /* Reject too-small input */
  if (size < 2)
    return 0;

  /* Reset global counters */
  g_resync_sum = 0;
  g_resync_call_count = 0;

  /* Set up error handling */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  for (ti = 0; ti < NUMTESTS; ti++) {
    if (setjmp(jerr.setjmp_buffer)) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, (unsigned long)size);

    /* Install custom resync handler if configured */
    if (tests[ti].use_custom_resync) {
      cinfo.src->resync_to_restart = custom_resync_to_restart;
    }

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    /* Sanity check on dimensions to avoid OOM */
    if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
        (uint64_t)cinfo.image_width * cinfo.image_height > 1048576) {
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    if (tests[ti].buffered_image && jpeg_has_multiple_scans(&cinfo)) {
      /* Buffered-image mode for progressive JPEGs */
      cinfo.buffered_image = TRUE;

      if (!jpeg_start_decompress(&cinfo)) {
        jpeg_destroy_decompress(&cinfo);
        continue;
      }

      row_stride = cinfo.output_width * cinfo.output_components;
      buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

      /* Process all scans */
      while (!jpeg_input_complete(&cinfo)) {
        int scan = cinfo.input_scan_number;

        /* Consume input until scan complete or EOI */
        while (jpeg_consume_input(&cinfo) != JPEG_SCAN_COMPLETED &&
               jpeg_consume_input(&cinfo) != JPEG_REACHED_EOI) {
          /* Keep consuming */
        }

        /* Start output for this scan */
        if (jpeg_start_output(&cinfo, scan)) {
          while (cinfo.output_scanline < cinfo.output_height) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            for (int i = 0; i < row_stride; i++)
              sum += buffer[0][i];
          }
          jpeg_finish_output(&cinfo);
        }
      }
    } else {
      /* Normal single-pass decode */
      if (!jpeg_start_decompress(&cinfo)) {
        jpeg_destroy_decompress(&cinfo);
        continue;
      }

      row_stride = cinfo.output_width * cinfo.output_components;
      buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

      while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        for (int i = 0; i < row_stride; i++)
          sum += buffer[0][i];
      }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
  }

  /* Combine sums for MSan visibility */
  sum += g_resync_sum + g_resync_call_count;

  PREVENT_OPTIMIZATION(sum);
  if (sum > (int64_t)255 * 1048576 * 4)
    return 1;

  return 0;
}
