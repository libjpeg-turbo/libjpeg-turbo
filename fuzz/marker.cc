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
 * This fuzzer targets marker processing in the libjpeg API.
 *
 * Specifically targets:
 * - jpeg_save_markers() for APP0-APP15 and COM markers
 * - jpeg_set_marker_processor() for custom marker handling
 * - Marker list traversal and data access
 * - Large marker data handling
 * - ICC profile reading via jpeg_read_icc_profile()
 *
 * Sanitizer support:
 * - MemorySanitizer: All marker and ICC data bytes are touched
 * - AddressSanitizer: Standard heap/stack checking
 * - UndefinedBehaviorSanitizer: Integer overflow, null pointer, etc.
 *
 * Coverage: Multiple length limits test truncation and boundary handling.
 * Custom marker processor tests callback invocation and data consumption.
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


#define NUMTESTS 4


/* Global counter for custom marker processor - touched for MSan */
static int64_t g_custom_marker_sum = 0;


/*
 * Custom marker processor for APP3 markers.
 * Exercises jpeg_set_marker_processor() callback mechanism.
 * Returns TRUE to indicate the marker was handled.
 */
static boolean custom_marker_processor(j_decompress_ptr cinfo)
{
  struct jpeg_source_mgr *src = cinfo->src;
  INT32 length;

  /* Read the 2-byte length field */
  if (src->bytes_in_buffer < 2)
    return FALSE;

  length = ((INT32)src->next_input_byte[0] << 8) +
           (INT32)src->next_input_byte[1];

  /* Consume the length field */
  src->next_input_byte += 2;
  src->bytes_in_buffer -= 2;
  length -= 2;

  if (length < 0)
    return FALSE;

  /* Consume and touch all marker data bytes */
  while (length > 0) {
    if (src->bytes_in_buffer == 0) {
      if (!(*src->fill_input_buffer)(cinfo))
        return FALSE;
    }

    size_t available = (size_t)length < src->bytes_in_buffer ?
                       (size_t)length : src->bytes_in_buffer;

    for (size_t i = 0; i < available; i++)
      g_custom_marker_sum += src->next_input_byte[i];

    src->next_input_byte += available;
    src->bytes_in_buffer -= available;
    length -= (INT32)available;
  }

  return TRUE;
}


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


/* Test configuration for different marker save settings */
struct test_config {
  /* Length limit for APP markers (0 = don't save, 0xFFFF = max) */
  unsigned int app_length_limit;
  /* Length limit for COM markers */
  unsigned int com_length_limit;
  /* Whether to try reading ICC profile */
  boolean try_icc;
  /* Whether to use custom marker processor for APP3 */
  boolean use_custom_processor;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int row_stride;
  int ti;

  /* Test configurations */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: Save all markers with max length, read ICC */
    { 0xFFFF, 0xFFFF, TRUE, FALSE },
    /* Test 1: Save only small markers (256 bytes), no ICC */
    { 256, 256, FALSE, FALSE },
    /* Test 2: Save medium markers (4096 bytes), read ICC */
    { 4096, 4096, TRUE, FALSE },
    /* Test 3: Custom marker processor for APP3, save others */
    { 0xFFFF, 0xFFFF, TRUE, TRUE }
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
    JOCTET *icc_data = NULL;
    unsigned int icc_data_len = 0;

    if (setjmp(jerr.setjmp_buffer)) {
      free(icc_data);
      jpeg_destroy_decompress(&cinfo);
      continue;
    }

    jpeg_create_decompress(&cinfo);

    /* Save APP markers (APP0-APP15) with configured length limit */
    for (int m = JPEG_APP0; m <= JPEG_APP0 + 15; m++) {
      /* If using custom processor, don't save APP3 - handle it specially */
      if (tests[ti].use_custom_processor && m == JPEG_APP0 + 3)
        continue;
      jpeg_save_markers(&cinfo, m, tests[ti].app_length_limit);
    }

    /* Install custom marker processor for APP3 if configured */
    if (tests[ti].use_custom_processor) {
      jpeg_set_marker_processor(&cinfo, JPEG_APP0 + 3, custom_marker_processor);
    }

    /* Save COM markers with configured length limit */
    jpeg_save_markers(&cinfo, JPEG_COM, tests[ti].com_length_limit);

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

    /* Traverse the saved marker list and touch all data */
    for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL;
         marker = marker->next) {
      /* Touch marker metadata */
      sum += marker->marker;
      sum += marker->original_length;
      sum += marker->data_length;

      /* Touch all marker data to catch uninitialized reads */
      for (unsigned int i = 0; i < marker->data_length; i++)
        sum += marker->data[i];
    }

    /* Try to read ICC profile if configured (tests the ICC profile assembly
     * code which handles multi-chunk ICC profiles across APP2 markers) */
    if (tests[ti].try_icc) {
      if (jpeg_read_icc_profile(&cinfo, &icc_data, &icc_data_len)) {
        /* Touch ICC data */
        for (unsigned int i = 0; i < icc_data_len; i++)
          sum += icc_data[i];
        free(icc_data);
        icc_data = NULL;
      }
    }

    /* Now decompress the image */
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

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* Prevent the code above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    PREVENT_OPTIMIZATION(sum);
    if (sum > (int64_t)255 * 1048576 * 4 + 0xFFFFFFFFLL)
      return 1;
  }

  /* Include custom marker processor sum */
  PREVENT_OPTIMIZATION(g_custom_marker_sum);
  if (g_custom_marker_sum > (int64_t)255 * 1048576)
    return 1;

  return 0;
}
