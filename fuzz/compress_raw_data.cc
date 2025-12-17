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
 * This fuzzer targets jpeg_write_raw_data() for raw YUV compression.
 *
 * Specifically targets:
 * - jpeg_write_raw_data() for raw downsampled data input
 * - Component plane handling during compression
 * - MCU row buffer management
 * - Various subsampling configurations
 *
 * Sanitizer support:
 * - MemorySanitizer: All compressed output bytes are touched
 * - AddressSanitizer: Buffer bounds in raw data handling
 * - UndefinedBehaviorSanitizer: Integer operations
 *
 * Coverage: Tests raw data compression path not covered by scanline API.
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


#define NUMTESTS 3


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


/* Test configuration for different subsampling */
struct test_config {
  int h_samp[3];  /* Horizontal sampling factors */
  int v_samp[3];  /* Vertical sampling factors */
  int quality;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_compress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  unsigned char *outbuffer = NULL;
  unsigned long outsize = 0;
  JSAMPARRAY plane_buffers[3];
  JSAMPIMAGE raw_buffers;
  int64_t sum = 0;
  int ti;

  /* Test configurations for different subsampling */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: 4:4:4 (no subsampling) */
    { {1, 1, 1}, {1, 1, 1}, 90 },
    /* Test 1: 4:2:0 (common subsampling) */
    { {2, 1, 1}, {2, 1, 1}, 75 },
    /* Test 2: 4:2:2 (horizontal subsampling only) */
    { {2, 1, 1}, {1, 1, 1}, 85 }
  };

  /* Need minimum data for image dimensions and raw YUV data */
  if (size < 64)
    return 0;

  /* Derive dimensions from input - must be multiples of 16 for MCU alignment */
  unsigned int width = ((data[0] % 8) + 1) * 16;   /* 16-128, multiple of 16 */
  unsigned int height = ((data[1] % 8) + 1) * 16;  /* 16-128, multiple of 16 */

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  for (ti = 0; ti < NUMTESTS; ti++) {
    int max_h_samp = tests[ti].h_samp[0];
    int max_v_samp = tests[ti].v_samp[0];
    JDIMENSION rows_per_iMCU;
    size_t data_offset;
    int ci;

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
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr;

    jpeg_set_defaults(&cinfo);
    jpeg_set_colorspace(&cinfo, JCS_YCbCr);
    jpeg_set_quality(&cinfo, tests[ti].quality, TRUE);

    /* Set sampling factors */
    for (ci = 0; ci < 3; ci++) {
      cinfo.comp_info[ci].h_samp_factor = tests[ti].h_samp[ci];
      cinfo.comp_info[ci].v_samp_factor = tests[ti].v_samp[ci];
    }

    /* Enable raw data input */
    cinfo.raw_data_in = TRUE;

    jpeg_start_compress(&cinfo, TRUE);

    rows_per_iMCU = max_v_samp * DCTSIZE;

    /* Allocate raw data buffers for each component */
    for (ci = 0; ci < 3; ci++) {
      int comp_v_samp = tests[ti].v_samp[ci];
      int comp_h_samp = tests[ti].h_samp[ci];
      JDIMENSION comp_rows = comp_v_samp * DCTSIZE;
      JDIMENSION comp_width = (width * comp_h_samp) / max_h_samp;

      plane_buffers[ci] = (JSAMPARRAY)(*cinfo.mem->alloc_small)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, comp_rows * sizeof(JSAMPROW));

      for (JDIMENSION row = 0; row < comp_rows; row++) {
        plane_buffers[ci][row] = (JSAMPROW)(*cinfo.mem->alloc_large)
          ((j_common_ptr)&cinfo, JPOOL_IMAGE, comp_width * sizeof(JSAMPLE));
      }
    }

    raw_buffers = plane_buffers;
    data_offset = 4;  /* Skip header bytes */

    /* Write raw data one iMCU row at a time */
    while (cinfo.next_scanline < cinfo.image_height) {
      /* Fill buffers with fuzz data */
      for (ci = 0; ci < 3; ci++) {
        int comp_v_samp = tests[ti].v_samp[ci];
        int comp_h_samp = tests[ti].h_samp[ci];
        JDIMENSION comp_rows = comp_v_samp * DCTSIZE;
        JDIMENSION comp_width = (width * comp_h_samp) / max_h_samp;

        for (JDIMENSION row = 0; row < comp_rows; row++) {
          for (JDIMENSION col = 0; col < comp_width; col++) {
            if (data_offset < size) {
              plane_buffers[ci][row][col] = data[data_offset++];
            } else {
              plane_buffers[ci][row][col] = 128;  /* Neutral gray */
            }
          }
        }
      }

      jpeg_write_raw_data(&cinfo, raw_buffers, rows_per_iMCU);
    }

    jpeg_finish_compress(&cinfo);

    /* Touch all output bytes for MSan */
    for (unsigned long i = 0; i < outsize; i++)
      sum += outbuffer[i];

    free(outbuffer);
    outbuffer = NULL;

    jpeg_destroy_compress(&cinfo);
  }

  if (sum > (int64_t)255 * 1048576)
    return 1;

  return 0;
}
