/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2015, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2021, Ilya Kurdyukov <jpegqs@gmail.com> for BaseALT, Ltd
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* CHROMA DOWNSAMPLING */

#include "jsimd_e2k.h"
#include "jcsample.h"


void jsimd_h2v1_downsample_e2k(JDIMENSION image_width,
                               int max_v_samp_factor,
                               JDIMENSION v_samp_factor,
                               JDIMENSION width_in_blocks,
                               JSAMPARRAY input_data,
                               JSAMPARRAY output_data)
{
  int outrow, outcol;
  JDIMENSION output_cols = width_in_blocks * DCTSIZE;
  JSAMPROW inptr, outptr;

  __m128i this0, next0, out;
  __m128i this0e, this0o, next0e, next0o, outl, outh;

  /* Constants */
  __m128i pw_bias = _mm_set1_epi32(1 << 16),
    even_mask = _mm_set1_epi16(255),
    pb_zero = _mm_setzero_si128();

  expand_right_edge(input_data, max_v_samp_factor, image_width,
                    output_cols * 2);

  for (outrow = 0; (JDIMENSION)outrow < v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr = input_data[outrow];

    for (outcol = output_cols; outcol > 0;
         outcol -= 16, inptr += 32, outptr += 16) {

      this0 = VEC_LD(inptr);
      this0e = _mm_and_si128(this0, even_mask);
      this0o = _mm_srli_epi16(this0, 8);
      outl = _mm_add_epi16(this0e, this0o);
      outl = _mm_srli_epi16(_mm_add_epi16(outl, pw_bias), 1);

      if (outcol > 8) {
        next0 = VEC_LD(inptr + 16);
        next0e = _mm_and_si128(next0, even_mask);
        next0o = _mm_srli_epi16(next0, 8);
        outh = _mm_add_epi16(next0e, next0o);
        outh = _mm_srli_epi16(_mm_add_epi16(outh, pw_bias), 1);
      } else
        outh = pb_zero;

      out = _mm_packus_epi16(outl, outh);
      VEC_ST(outptr, out);
    }
  }
}


void
jsimd_h2v2_downsample_e2k(JDIMENSION image_width, int max_v_samp_factor,
                              JDIMENSION v_samp_factor,
                              JDIMENSION width_in_blocks,
                              JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow, outrow, outcol;
  JDIMENSION output_cols = width_in_blocks * DCTSIZE;
  JSAMPROW inptr0, inptr1, outptr;

  __m128i this0, next0, this1, next1, out;
  __m128i this0e, this0o, next0e, next0o, this1e, this1o,
    next1e, next1o, out0l, out0h, out1l, out1h, outl, outh;

  /* Constants */
  __m128i pw_bias = _mm_set1_epi32(1 | 2 << 16),
    even_mask = _mm_set1_epi16(255),
    pb_zero = _mm_setzero_si128();

  expand_right_edge(input_data, max_v_samp_factor, image_width,
                    output_cols * 2);

  for (inrow = 0, outrow = 0; (JDIMENSION)outrow < v_samp_factor;
       inrow += 2, outrow++) {

    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];
    outptr = output_data[outrow];

    for (outcol = output_cols; outcol > 0;
         outcol -= 16, inptr0 += 32, inptr1 += 32, outptr += 16) {

      this0 = VEC_LD(inptr0);
      this0e = _mm_and_si128(this0, even_mask);
      this0o = _mm_srli_epi16(this0, 8);
      out0l = _mm_add_epi16(this0e, this0o);

      this1 = VEC_LD(inptr1);
      this1e = _mm_and_si128(this1, even_mask);
      this1o = _mm_srli_epi16(this1, 8);
      out1l = _mm_add_epi16(this1e, this1o);

      outl = _mm_add_epi16(out0l, out1l);
      outl = _mm_srli_epi16(_mm_add_epi16(outl, pw_bias), 2);

      if (outcol > 8) {
        next0 = VEC_LD(inptr0 + 16);
        next0e = _mm_and_si128(next0, even_mask);
        next0o = _mm_srli_epi16(next0, 8);
        out0h = _mm_add_epi16(next0e, next0o);

        next1 = VEC_LD(inptr1 + 16);
        next1e = _mm_and_si128(next1, even_mask);
        next1o = _mm_srli_epi16(next1, 8);
        out1h = _mm_add_epi16(next1e, next1o);

        outh = _mm_add_epi16(out0h, out1h);
        outh = _mm_srli_epi16(_mm_add_epi16(outh, pw_bias), 2);
      } else
        outh = pb_zero;

      out = _mm_packus_epi16(outl, outh);
      VEC_ST(outptr, out);
    }
  }
}
