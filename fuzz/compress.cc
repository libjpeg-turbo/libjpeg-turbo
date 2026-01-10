/*
 * Copyright (C)2021, 2023-2026 D. R. Commander.  All Rights Reserved.
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

#include "../turbojpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

extern "C" unsigned char *
_tj3LoadImageFromFileHandle8(tjhandle handle, FILE *file, int *width,
                             int align, int *height, int *pixelFormat);


#define NUMTESTS  7


struct test {
  enum TJPF pf;
  enum TJSAMP subsamp;
  int quality;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  tjhandle handle = NULL;
  unsigned char *srcBuf = NULL, *dstBuf = NULL;
  int width = 0, height = 0, ti;
  FILE *file = NULL;
  struct test tests[NUMTESTS] = {
    { TJPF_RGB, TJSAMP_444, 100 },
    { TJPF_BGR, TJSAMP_422, 90 },
    { TJPF_RGBX, TJSAMP_420, 80 },
    { TJPF_BGRA, TJSAMP_411, 70 },
    { TJPF_XRGB, TJSAMP_GRAY, 60 },
    { TJPF_GRAY, TJSAMP_GRAY, 50 },
    { TJPF_CMYK, TJSAMP_440, 40 }
  };

  if ((file = fmemopen((void *)data, size, "r")) == NULL)
    goto bailout;

  if ((handle = tj3Init(TJINIT_COMPRESS)) == NULL)
    goto bailout;

  for (ti = 0; ti < NUMTESTS; ti++) {
    int pf = tests[ti].pf;
    size_t dstSize = 0, maxBufSize, i, sum = 0;

    /* Test non-default compression options on specific iterations. */
    tj3Set(handle, TJPARAM_BOTTOMUP, ti == 0);
    tj3Set(handle, TJPARAM_FASTDCT, ti == 1);
    tj3Set(handle, TJPARAM_OPTIMIZE, ti == 6);
    tj3Set(handle, TJPARAM_PROGRESSIVE, ti == 1 || ti == 3);
    tj3Set(handle, TJPARAM_ARITHMETIC, ti == 2 || ti == 3);
    tj3Set(handle, TJPARAM_NOREALLOC, ti != 2);
    tj3Set(handle, TJPARAM_RESTARTROWS, ti == 1 || ti == 2 ? 2 : 0);

    tj3Set(handle, TJPARAM_MAXPIXELS, 1048576);
    /* tj3LoadImage8() will refuse to load images larger than 1 Megapixel, so
       we don't need to check the width and height here. */
    fseek(file, 0, SEEK_SET);
    if ((srcBuf = _tj3LoadImageFromFileHandle8(handle, file, &width, 1,
                                               &height, &pf)) == NULL)
      continue;

    maxBufSize = tj3JPEGBufSize(width, height, tests[ti].subsamp);
    if (tj3Get(handle, TJPARAM_NOREALLOC)) {
      if ((dstBuf = (unsigned char *)tj3Alloc(maxBufSize)) == NULL)
        goto bailout;
    } else
      dstBuf = NULL;

    tj3Set(handle, TJPARAM_SUBSAMP, tests[ti].subsamp);
    tj3Set(handle, TJPARAM_QUALITY, tests[ti].quality);
    if (tj3Compress8(handle, srcBuf, width, 0, height, pf, &dstBuf,
                     &dstSize) == 0) {
      /* Touch all of the output data in order to catch uninitialized reads
         when using MemorySanitizer. */
      for (i = 0; i < dstSize; i++)
        sum += dstBuf[i];
    }

    tj3Free(dstBuf);
    dstBuf = NULL;
    tj3Free(srcBuf);
    srcBuf = NULL;

    /* Prevent the sum above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > 255 * maxBufSize)
      goto bailout;
  }

bailout:
  tj3Free(dstBuf);
  tj3Free(srcBuf);
  if (file) fclose(file);
  tj3Destroy(handle);
  return 0;
}
