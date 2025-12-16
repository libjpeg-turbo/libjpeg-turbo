/*
 * Copyright (C) 2024
 *
 * This fuzzer targets the raw libjpeg API (as opposed to TurboJPEG API)
 * to exercise code paths that might not be covered by the TurboJPEG fuzzer.
 *
 * Specifically targets:
 * - DHT (Huffman table) marker processing
 * - Slow-path Huffman decode in jpeg_huff_decode()
 * - Edge cases in valoffset calculations
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
  /* Suppress error messages during fuzzing */
  /* (*cinfo->err->output_message)(cinfo); */
  longjmp(myerr->setjmp_buffer, 1);
}

static void fuzzer_emit_message(j_common_ptr cinfo, int msg_level)
{
  /* Suppress all messages during fuzzing */
  (void)cinfo;
  (void)msg_level;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int row_stride;
  int64_t sum = 0;  /* Prevent optimization */

  /* Reject empty input */
  if (size < 2)
    return 0;

  /* Set up error handling */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  if (setjmp(jerr.setjmp_buffer)) {
    /* Error occurred - cleanup and return */
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }

  /* Initialize decompression */
  jpeg_create_decompress(&cinfo);

  /* Feed fuzz data directly from memory */
  jpeg_mem_src(&cinfo, data, (unsigned long)size);

  /* Read header - this parses DHT, DQT, SOF markers */
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    goto bailout;

  /* Sanity check on dimensions to avoid OOM */
  if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
      (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
    goto bailout;

  /* Start decompression - this sets up Huffman tables via make_d_derived_tbl */
  if (!jpeg_start_decompress(&cinfo))
    goto bailout;

  /* Allocate output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Read all scanlines - this exercises Huffman decoding including
   * the slow-path jpeg_huff_decode() where the OOB read can occur */
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Touch output to catch uninitialized reads */
    for (int i = 0; i < row_stride; i++)
      sum += buffer[0][i];
  }

  jpeg_finish_decompress(&cinfo);

bailout:
  jpeg_destroy_decompress(&cinfo);

  /* Prevent sum from being optimized out */
  if (sum > (int64_t)255 * 1048576 * 4)
    return 1;

  return 0;
}
