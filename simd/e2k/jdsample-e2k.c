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

/* CHROMA UPSAMPLING */

#include "jsimd_e2k.h"


void jsimd_h2v1_fancy_upsample_e2k(int max_v_samp_factor,
                                   JDIMENSION downsampled_width,
                                   JSAMPARRAY input_data,
                                   JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr, outptr;
  int inrow, incol;

  __m128i pb_zero = _mm_setzero_si128();
  __m128i this0, last0, p_last0, next0 = pb_zero, p_next0, out;
  __m128i this0l, this0h, last0l, last0h,
    next0l, next0h, outle, outhe, outlo, outho;

  /* Constants */
  __m128i pw_three = _mm_set1_epi16(3),
    next_index_lastcol = _mm_setr_epi8(
       1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15),
    pw_one = _mm_set1_epi16(1), pw_two = _mm_set1_epi16(2);

  if (downsampled_width > 0)
  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data[inrow];

    if (downsampled_width & 15)
      inptr[downsampled_width] = inptr[downsampled_width - 1];

    this0 = VEC_LD(inptr);
    last0 = _mm_bslli_si128(this0, 15);

    for (incol = downsampled_width; incol > 0;
         incol -= 16, inptr += 16, outptr += 32) {

      p_last0 = _mm_alignr_epi8(this0, last0, 15);
      last0 = this0;

      if (__builtin_expect(incol <= 16, 0))
        p_next0 = _mm_shuffle_epi8(this0, next_index_lastcol);
      else {
        next0 = VEC_LD(inptr + 16);
        p_next0 = _mm_alignr_epi8(next0, this0, 1);
      }

      this0l = _mm_mullo_epi16(_mm_unpacklo_epi8(this0, pb_zero), pw_three);
      last0l = _mm_unpacklo_epi8(p_last0, pb_zero);
      next0l = _mm_unpacklo_epi8(p_next0, pb_zero);
      last0l = _mm_add_epi16(last0l, pw_one);
      next0l = _mm_add_epi16(next0l, pw_two);

      outle = _mm_add_epi16(this0l, last0l);
      outlo = _mm_add_epi16(this0l, next0l);
      outle = _mm_srli_epi16(outle, 2);
      outlo = _mm_srli_epi16(outlo, 2);

      out = _mm_or_si128(outle, _mm_slli_epi16(outlo, 8));
      VEC_ST(outptr, out);

      if (__builtin_expect(incol <= 8, 0)) break;

      this0h = _mm_mullo_epi16(_mm_unpackhi_epi8(this0, pb_zero), pw_three);
      last0h = _mm_unpackhi_epi8(p_last0, pb_zero);
      next0h = _mm_unpackhi_epi8(p_next0, pb_zero);
      last0h = _mm_add_epi16(last0h, pw_one);
      next0h = _mm_add_epi16(next0h, pw_two);

      outhe = _mm_add_epi16(this0h, last0h);
      outho = _mm_add_epi16(this0h, next0h);
      outhe = _mm_srli_epi16(outhe, 2);
      outho = _mm_srli_epi16(outho, 2);

      out = _mm_or_si128(outhe, _mm_slli_epi16(outho, 8));
      VEC_ST(outptr + 16, out);

      this0 = next0;
    }
  }
}


void jsimd_h2v2_fancy_upsample_e2k(int max_v_samp_factor,
                                   JDIMENSION downsampled_width,
                                   JSAMPARRAY input_data,
                                   JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr_1, inptr0, inptr1, outptr0, outptr1;
  int inrow, outrow, incol;

  __m128i pb_zero = _mm_setzero_si128();
  __m128i this_1, this0, this1, out;
  __m128i this_1l, this_1h, this0l, this0h, this1l, this1h,
    lastcolsum_1h, lastcolsum1h,
    p_lastcolsum_1l, p_lastcolsum_1h, p_lastcolsum1l, p_lastcolsum1h,
    thiscolsum_1l, thiscolsum_1h, thiscolsum1l, thiscolsum1h,
    nextcolsum_1l = pb_zero, nextcolsum_1h = pb_zero,
    nextcolsum1l = pb_zero, nextcolsum1h = pb_zero,
    p_nextcolsum_1l, p_nextcolsum_1h, p_nextcolsum1l, p_nextcolsum1h,
    tmpl, tmph, outle, outhe, outlo, outho;

  /* Constants */
  __m128i pw_three = _mm_set1_epi16(3),
    pw_seven = _mm_set1_epi16(7), pw_eight = _mm_set1_epi16(8),
    next_index_lastcol = _mm_setr_epi8(
       2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 14, 15);

  if (downsampled_width > 0)
  for (inrow = 0, outrow = 0; outrow < max_v_samp_factor; inrow++) {

    inptr_1 = input_data[inrow - 1];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];
    outptr0 = output_data[outrow++];
    outptr1 = output_data[outrow++];

    if (downsampled_width & 15) {
      inptr_1[downsampled_width] = inptr_1[downsampled_width - 1];
      inptr0[downsampled_width] = inptr0[downsampled_width - 1];
      inptr1[downsampled_width] = inptr1[downsampled_width - 1];
    }

    this0 = VEC_LD(inptr0);
    this0l = _mm_unpacklo_epi8(this0, pb_zero);
    this0h = _mm_unpackhi_epi8(this0, pb_zero);
    this0l = _mm_mullo_epi16(this0l, pw_three);
    this0h = _mm_mullo_epi16(this0h, pw_three);

    this_1 = VEC_LD(inptr_1);
    this_1l = _mm_unpacklo_epi8(this_1, pb_zero);
    this_1h = _mm_unpackhi_epi8(this_1, pb_zero);
    thiscolsum_1l = _mm_add_epi16(this0l, this_1l);
    thiscolsum_1h = _mm_add_epi16(this0h, this_1h);
    lastcolsum_1h = _mm_bslli_si128(thiscolsum_1l, 14);;

    this1 = VEC_LD(inptr1);
    this1l = _mm_unpacklo_epi8(this1, pb_zero);
    this1h = _mm_unpackhi_epi8(this1, pb_zero);
    thiscolsum1l = _mm_add_epi16(this0l, this1l);
    thiscolsum1h = _mm_add_epi16(this0h, this1h);
    lastcolsum1h = _mm_bslli_si128(thiscolsum1l, 14);

    for (incol = downsampled_width; incol > 0;
         incol -= 16, inptr_1 += 16, inptr0 += 16, inptr1 += 16,
         outptr0 += 32, outptr1 += 32) {

      p_lastcolsum_1l = _mm_alignr_epi8(thiscolsum_1l, lastcolsum_1h, 14);
      p_lastcolsum_1h = _mm_alignr_epi8(thiscolsum_1h, thiscolsum_1l, 14);
      p_lastcolsum1l = _mm_alignr_epi8(thiscolsum1l, lastcolsum1h, 14);
      p_lastcolsum1h = _mm_alignr_epi8(thiscolsum1h, thiscolsum1l, 14);
      lastcolsum_1h = thiscolsum_1h;
      lastcolsum1h = thiscolsum1h;

      if (__builtin_expect(incol <= 16, 0)) {
        p_nextcolsum_1l = _mm_alignr_epi8(thiscolsum_1h, thiscolsum_1l, 2);
        p_nextcolsum_1h = _mm_shuffle_epi8(thiscolsum_1h, next_index_lastcol);
        p_nextcolsum1l = _mm_alignr_epi8(thiscolsum1h, thiscolsum1l, 2);
        p_nextcolsum1h = _mm_shuffle_epi8(thiscolsum1h, next_index_lastcol);
      } else {
        this0 = VEC_LD(inptr0 + 16);
        this0l = _mm_unpacklo_epi8(this0, pb_zero);
        this0h = _mm_unpackhi_epi8(this0, pb_zero);
        this0l = _mm_mullo_epi16(this0l, pw_three);
        this0h = _mm_mullo_epi16(this0h, pw_three);

        this_1 = VEC_LD(inptr_1 + 16);
        this_1l = _mm_unpacklo_epi8(this_1, pb_zero);
        this_1h = _mm_unpackhi_epi8(this_1, pb_zero);
        nextcolsum_1l = _mm_add_epi16(this0l, this_1l);
        nextcolsum_1h = _mm_add_epi16(this0h, this_1h);
        p_nextcolsum_1l = _mm_alignr_epi8(thiscolsum_1h, thiscolsum_1l, 2);
        p_nextcolsum_1h = _mm_alignr_epi8(nextcolsum_1l, thiscolsum_1h, 2);

        this1 = VEC_LD(inptr1 + 16);
        this1l = _mm_unpacklo_epi8(this1, pb_zero);
        this1h = _mm_unpackhi_epi8(this1, pb_zero);
        nextcolsum1l = _mm_add_epi16(this0l, this1l);
        nextcolsum1h = _mm_add_epi16(this0h, this1h);
        p_nextcolsum1l = _mm_alignr_epi8(thiscolsum1h, thiscolsum1l, 2);
        p_nextcolsum1h = _mm_alignr_epi8(nextcolsum1l, thiscolsum1h, 2);
      }

      /* Process the upper row */

      tmpl = _mm_mullo_epi16(thiscolsum_1l, pw_three);
      outle = _mm_add_epi16(tmpl, p_lastcolsum_1l);
      outle = _mm_add_epi16(outle, pw_eight);
      outle = _mm_srli_epi16(outle, 4);

      outlo = _mm_add_epi16(tmpl, p_nextcolsum_1l);
      outlo = _mm_add_epi16(outlo, pw_seven);
      outlo = _mm_srli_epi16(outlo, 4);

      out = _mm_or_si128(outle, _mm_slli_epi16(outlo, 8));
      VEC_ST(outptr0, out);

      /* Process the lower row */

      tmpl = _mm_mullo_epi16(thiscolsum1l, pw_three);
      outle = _mm_add_epi16(tmpl, p_lastcolsum1l);
      outle = _mm_add_epi16(outle, pw_eight);
      outle = _mm_srli_epi16(outle, 4);

      outlo = _mm_add_epi16(tmpl, p_nextcolsum1l);
      outlo = _mm_add_epi16(outlo, pw_seven);
      outlo = _mm_srli_epi16(outlo, 4);

      out = _mm_or_si128(outle, _mm_slli_epi16(outlo, 8));
      VEC_ST(outptr1, out);

      if (__builtin_expect(incol <= 8, 0)) break;

      tmph = _mm_mullo_epi16(thiscolsum_1h, pw_three);
      outhe = _mm_add_epi16(tmph, p_lastcolsum_1h);
      outhe = _mm_add_epi16(outhe, pw_eight);
      outhe = _mm_srli_epi16(outhe, 4);

      outho = _mm_add_epi16(tmph, p_nextcolsum_1h);
      outho = _mm_add_epi16(outho, pw_seven);
      outho = _mm_srli_epi16(outho, 4);

      out = _mm_or_si128(outhe, _mm_slli_epi16(outho, 8));
      VEC_ST(outptr0 + 16, out);

      tmph = _mm_mullo_epi16(thiscolsum1h, pw_three);
      outhe = _mm_add_epi16(tmph, p_lastcolsum1h);
      outhe = _mm_add_epi16(outhe, pw_eight);
      outhe = _mm_srli_epi16(outhe, 4);

      outho = _mm_add_epi16(tmph, p_nextcolsum1h);
      outho = _mm_add_epi16(outho, pw_seven);
      outho = _mm_srli_epi16(outho, 4);

      out = _mm_or_si128(outhe, _mm_slli_epi16(outho, 8));
      VEC_ST(outptr1 + 16, out);

      thiscolsum_1l = nextcolsum_1l;  thiscolsum_1h = nextcolsum_1h;
      thiscolsum1l = nextcolsum1l;  thiscolsum1h = nextcolsum1h;
    }
  }
}


/* These are rarely used (mainly just for decompressing YCCK images) */

void jsimd_h2v1_upsample_e2k(int max_v_samp_factor,
                             JDIMENSION output_width,
                             JSAMPARRAY input_data,
                             JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr, outptr;
  int inrow, incol;

  __m128i in, inl, inh;

  if (output_width > 0)
  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data[inrow];

    for (incol = output_width; incol > 0;
         incol -= 32, inptr += 16, outptr += 32) {

      in = VEC_LD(inptr);
      inl = _mm_unpacklo_epi8(in, in);
      inh = _mm_unpackhi_epi8(in, in);

      VEC_ST(outptr, inl);
      VEC_ST(outptr + 16, inh);
    }
  }
}


void jsimd_h2v2_upsample_e2k(int max_v_samp_factor,
                             JDIMENSION output_width,
                             JSAMPARRAY input_data,
                             JSAMPARRAY *output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  JSAMPROW inptr, outptr0, outptr1;
  int inrow, outrow, incol;

  __m128i in, inl, inh;

  if (output_width > 0)
  for (inrow = 0, outrow = 0; outrow < max_v_samp_factor; inrow++) {

    inptr = input_data[inrow];
    outptr0 = output_data[outrow++];
    outptr1 = output_data[outrow++];

    for (incol = output_width; incol > 0;
         incol -= 32, inptr += 16, outptr0 += 32, outptr1 += 32) {

      in = VEC_LD(inptr);
      inl = _mm_unpacklo_epi8(in, in);
      inh = _mm_unpackhi_epi8(in, in);

      VEC_ST(outptr0, inl);
      VEC_ST(outptr1, inl);
      VEC_ST(outptr0 + 16, inh);
      VEC_ST(outptr1 + 16, inh);
    }
  }
}
