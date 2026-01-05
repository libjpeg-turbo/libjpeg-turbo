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
 * This fuzzer targets lossless transcoding via the raw libjpeg API.
 *
 * Specifically targets:
 * - jpeg_read_coefficients() for reading DCT coefficients
 * - jpeg_write_coefficients() for writing DCT coefficients
 * - jpeg_copy_critical_parameters() for copying parameters between objects
 * - Virtual coefficient array access during transcoding
 * - Huffman table optimization during write
 *
 * Sanitizer support:
 * - MemorySanitizer: All coefficient values are touched
 * - AddressSanitizer: Buffer bounds in coefficient access
 * - UndefinedBehaviorSanitizer: Integer operations on coefficients
 *
 * Coverage: Tests full transcode pipeline via libjpeg API.
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


/* Test configuration */
struct test_config {
  boolean optimize_coding;   /* Use optimized Huffman tables */
  boolean progressive_mode;  /* Output progressive JPEG */
  boolean arithmetic;        /* Use arithmetic coding */
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct srcinfo;
  struct jpeg_compress_struct dstinfo;
  struct fuzzer_error_mgr srcerr, dsterr;
  jvirt_barray_ptr *coef_arrays = NULL;
  unsigned char *outbuffer = NULL;
  unsigned long outsize = 0;
  int64_t sum = 0;
  int ti;

  /* Test configurations */
  struct test_config tests[NUMTESTS] = {
    /* Test 0: Basic transcode, optimized Huffman */
    { TRUE, FALSE, FALSE },
    /* Test 1: Progressive output */
    { TRUE, TRUE, FALSE },
    /* Test 2: Arithmetic coding */
    { FALSE, FALSE, TRUE }
  };

  if (size < 2)
    return 0;

  /* Set up source error handling */
  srcinfo.err = jpeg_std_error(&srcerr.pub);
  srcerr.pub.error_exit = fuzzer_error_exit;
  srcerr.pub.emit_message = fuzzer_emit_message;

  /* Set up destination error handling */
  dstinfo.err = jpeg_std_error(&dsterr.pub);
  dsterr.pub.error_exit = fuzzer_error_exit;
  dsterr.pub.emit_message = fuzzer_emit_message;

  for (ti = 0; ti < NUMTESTS; ti++) {
    if (setjmp(srcerr.setjmp_buffer)) {
      free(outbuffer);
      outbuffer = NULL;
      jpeg_destroy_compress(&dstinfo);
      jpeg_destroy_decompress(&srcinfo);
      continue;
    }

    if (setjmp(dsterr.setjmp_buffer)) {
      free(outbuffer);
      outbuffer = NULL;
      jpeg_destroy_compress(&dstinfo);
      jpeg_destroy_decompress(&srcinfo);
      continue;
    }

    jpeg_create_decompress(&srcinfo);
    jpeg_mem_src(&srcinfo, data, (unsigned long)size);

    if (jpeg_read_header(&srcinfo, TRUE) != JPEG_HEADER_OK) {
      jpeg_destroy_decompress(&srcinfo);
      continue;
    }

    /* Sanity check on dimensions */
    if (srcinfo.image_width < 1 || srcinfo.image_height < 1 ||
        (uint64_t)srcinfo.image_width * srcinfo.image_height > 1048576) {
      jpeg_destroy_decompress(&srcinfo);
      continue;
    }

    /* Read coefficients - this is the core of lossless transcoding */
    coef_arrays = jpeg_read_coefficients(&srcinfo);
    if (coef_arrays == NULL) {
      jpeg_destroy_decompress(&srcinfo);
      continue;
    }

    /* Touch all coefficient values for MSan */
    for (int ci = 0; ci < srcinfo.num_components; ci++) {
      jpeg_component_info *compptr = srcinfo.comp_info + ci;
      JDIMENSION block_rows = compptr->height_in_blocks;
      JDIMENSION block_cols = compptr->width_in_blocks;

      /* Limit to prevent excessive processing */
      if ((uint64_t)block_rows * block_cols > 65536)
        continue;

      for (JDIMENSION blk_y = 0; blk_y < block_rows; blk_y++) {
        JBLOCKARRAY buffer = (*srcinfo.mem->access_virt_barray)
          ((j_common_ptr)&srcinfo, coef_arrays[ci], blk_y, 1, FALSE);

        for (JDIMENSION blk_x = 0; blk_x < block_cols; blk_x++) {
          JCOEFPTR block = buffer[0][blk_x];
          for (int k = 0; k < DCTSIZE2; k++)
            sum += block[k];
        }
      }
    }

    /* Set up compression for transcoding */
    jpeg_create_compress(&dstinfo);
    outbuffer = NULL;
    outsize = 0;
    jpeg_mem_dest(&dstinfo, &outbuffer, &outsize);

    /* Copy critical parameters from source */
    jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

    /* Apply test configuration */
    dstinfo.optimize_coding = tests[ti].optimize_coding;
    if (tests[ti].progressive_mode)
      jpeg_simple_progression(&dstinfo);
    dstinfo.arith_code = tests[ti].arithmetic;

    /* Write coefficients - lossless transcode */
    jpeg_write_coefficients(&dstinfo, coef_arrays);

    /* Finish compression */
    jpeg_finish_compress(&dstinfo);

    /* Touch output for MSan */
    for (unsigned long i = 0; i < outsize; i++)
      sum += outbuffer[i];

    free(outbuffer);
    outbuffer = NULL;

    jpeg_destroy_compress(&dstinfo);

    /* Finish decompression */
    jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);
  }

  if (sum > (int64_t)32767 * 64 * 65536 + (int64_t)255 * 1048576)
    return 1;

  return 0;
}
