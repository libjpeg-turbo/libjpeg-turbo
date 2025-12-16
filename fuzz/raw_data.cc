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
 * This fuzzer targets the raw data (downsampled YUV) API in libjpeg.
 *
 * Specifically targets:
 * - jpeg_read_raw_data() for raw YUV plane access
 * - Downsampling/upsampling buffer calculations
 * - Component plane separation
 * - MCU row handling with various subsampling factors
 *
 * Sanitizer support:
 * - MemorySanitizer: All raw YUV plane bytes are touched
 * - AddressSanitizer: Buffer boundary checking for plane access
 * - UndefinedBehaviorSanitizer: Integer overflow in buffer calculations
 *
 * Coverage: Tests raw data path not covered by normal scanline API.
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
  JSAMPARRAY plane_buffers[MAX_COMPONENTS];
  JSAMPIMAGE raw_buffers;
  int64_t sum = 0;
  int ci;

  if (size < 2)
    return 0;

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

  /* Sanity check on dimensions */
  if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
      (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
    goto bailout;

  /* Raw data output requires specific setup */
  cinfo.raw_data_out = TRUE;
  cinfo.do_fancy_upsampling = FALSE;

  if (!jpeg_start_decompress(&cinfo))
    goto bailout;

  /* Calculate buffer sizes based on component info.
   * For raw data, we need to allocate buffers for each component plane
   * based on the component's sampling factors. */
  {
    int max_v_samp_factor = cinfo.max_v_samp_factor;
    JDIMENSION rows_per_iMCU = max_v_samp_factor * DCTSIZE;

    /* Allocate buffers for each component */
    for (ci = 0; ci < cinfo.num_components; ci++) {
      jpeg_component_info *compptr = cinfo.comp_info + ci;
      int v_samp = compptr->v_samp_factor;
      JDIMENSION comp_rows = v_samp * DCTSIZE;
      JDIMENSION comp_width = compptr->width_in_blocks * DCTSIZE;

      /* Allocate row pointers */
      plane_buffers[ci] = (JSAMPARRAY)(*cinfo.mem->alloc_small)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE,
         comp_rows * sizeof(JSAMPROW));

      /* Allocate each row */
      for (JDIMENSION row = 0; row < comp_rows; row++) {
        plane_buffers[ci][row] = (JSAMPROW)(*cinfo.mem->alloc_large)
          ((j_common_ptr)&cinfo, JPOOL_IMAGE, comp_width * sizeof(JSAMPLE));
      }
    }

    raw_buffers = plane_buffers;

    /* Read raw data one iMCU row at a time */
    while (cinfo.output_scanline < cinfo.output_height) {
      JDIMENSION rows_read = jpeg_read_raw_data(&cinfo, raw_buffers,
                                                 rows_per_iMCU);

      if (rows_read == 0)
        break;

      /* Touch all raw data bytes for MemorySanitizer */
      for (ci = 0; ci < cinfo.num_components; ci++) {
        jpeg_component_info *compptr = cinfo.comp_info + ci;
        int v_samp = compptr->v_samp_factor;
        JDIMENSION comp_rows = v_samp * DCTSIZE;
        JDIMENSION comp_width = compptr->width_in_blocks * DCTSIZE;

        /* Limit rows to what was actually read */
        JDIMENSION actual_rows = (rows_read * v_samp) / cinfo.max_v_samp_factor;
        if (actual_rows > comp_rows)
          actual_rows = comp_rows;

        for (JDIMENSION row = 0; row < actual_rows; row++) {
          for (JDIMENSION col = 0; col < comp_width; col++) {
            sum += plane_buffers[ci][row][col];
          }
        }
      }
    }
  }

  jpeg_finish_decompress(&cinfo);

bailout:
  jpeg_destroy_decompress(&cinfo);

  PREVENT_OPTIMIZATION(sum);
  if (sum > (int64_t)255 * 1048576 * 3)
    return 1;

  return 0;
}
