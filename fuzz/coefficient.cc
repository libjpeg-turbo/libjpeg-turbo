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
 * This fuzzer targets DCT coefficient access in the libjpeg API.
 *
 * Specifically targets:
 * - jpeg_read_coefficients() for direct coefficient access
 * - Virtual block array management
 * - Coefficient buffer access patterns
 * - Lossless transcoding paths
 *
 * Sanitizer support:
 * - MemorySanitizer: All 64 DCT coefficients per block are touched
 * - AddressSanitizer: Standard heap/stack checking
 * - UndefinedBehaviorSanitizer: Integer overflow, null pointer, etc.
 *
 * Coverage: Exercises coefficient access for all image components.
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
  struct jpeg_decompress_struct srcinfo;
  struct fuzzer_error_mgr jerr;
  jvirt_barray_ptr *coef_arrays = NULL;
  int64_t sum = 0;

  /* Reject too-small input */
  if (size < 2)
    return 0;

  /* Set up error handling */
  srcinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&srcinfo);
    return 0;
  }

  jpeg_create_decompress(&srcinfo);
  jpeg_mem_src(&srcinfo, data, (unsigned long)size);

  if (jpeg_read_header(&srcinfo, TRUE) != JPEG_HEADER_OK)
    goto bailout;

  /* Sanity check on dimensions to avoid OOM.  Casting width to (uint64_t)
     prevents integer overflow if width * height > INT_MAX. */
  if (srcinfo.image_width < 1 || srcinfo.image_height < 1 ||
      (uint64_t)srcinfo.image_width * srcinfo.image_height > 1048576)
    goto bailout;

  /* Read DCT coefficients directly - this is the path used by jpegtran
   * for lossless transcoding */
  coef_arrays = jpeg_read_coefficients(&srcinfo);
  if (coef_arrays == NULL)
    goto bailout;

  /* Access coefficient data for each component */
  for (int ci = 0; ci < srcinfo.num_components; ci++) {
    jpeg_component_info *compptr = srcinfo.comp_info + ci;
    JDIMENSION block_rows = compptr->height_in_blocks;
    JDIMENSION block_cols = compptr->width_in_blocks;

    /* Limit block access to prevent excessive processing time */
    if ((uint64_t)block_rows * block_cols > 65536)
      continue;

    /* Access all blocks in this component */
    for (JDIMENSION blk_y = 0; blk_y < block_rows; blk_y++) {
      JBLOCKARRAY buffer = (*srcinfo.mem->access_virt_barray)
        ((j_common_ptr)&srcinfo, coef_arrays[ci], blk_y, 1, FALSE);

      for (JDIMENSION blk_x = 0; blk_x < block_cols; blk_x++) {
        JCOEFPTR block = buffer[0][blk_x];

        /* Touch all 64 coefficients in the DCT block to catch
         * uninitialized reads with MemorySanitizer */
        for (int k = 0; k < DCTSIZE2; k++)
          sum += block[k];
      }
    }
  }

  jpeg_finish_decompress(&srcinfo);

bailout:
  jpeg_destroy_decompress(&srcinfo);

  /* Prevent the code above from being optimized out.  This test should never
     be true, but the compiler doesn't know that. */
  if (sum > (int64_t)32767 * 64 * 65536)
    return 1;

  return 0;
}
