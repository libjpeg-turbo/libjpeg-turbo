/*
 * Elbrus optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014, D. R. Commander.  All Rights Reserved.
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

/* FAST INTEGER FORWARD DCT */

#include "jsimd_e2k.h"


#define F_0_382  98   /* FIX(0.382683433) */
#define F_0_541  139  /* FIX(0.541196100) */
#define F_0_707  181  /* FIX(0.707106781) */
#define F_1_306  334  /* FIX(1.306562965) */

#define CONST_BITS  8
#define PRE_MULTIPLY_SCALE_BITS  2
#define CONST_SHIFT  (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)


#define DO_FDCT() { \
  /* Even part */ \
  \
  tmp10 = _mm_add_epi16(tmp0, tmp3); \
  tmp13 = _mm_sub_epi16(tmp0, tmp3); \
  tmp11 = _mm_add_epi16(tmp1, tmp2); \
  tmp12 = _mm_sub_epi16(tmp1, tmp2); \
  \
  out0  = _mm_add_epi16(tmp10, tmp11); \
  out4  = _mm_sub_epi16(tmp10, tmp11); \
  \
  z1 = _mm_add_epi16(tmp12, tmp13); \
  z1 = _mm_slli_epi16(z1, PRE_MULTIPLY_SCALE_BITS); \
  z1 = _mm_mulhi_epi16(z1, pw_0707); \
  \
  out2 = _mm_add_epi16(tmp13, z1); \
  out6 = _mm_sub_epi16(tmp13, z1); \
  \
  /* Odd part */ \
  \
  tmp10 = _mm_add_epi16(tmp4, tmp5); \
  tmp11 = _mm_add_epi16(tmp5, tmp6); \
  tmp12 = _mm_add_epi16(tmp6, tmp7); \
  \
  tmp10 = _mm_slli_epi16(tmp10, PRE_MULTIPLY_SCALE_BITS); \
  tmp12 = _mm_slli_epi16(tmp12, PRE_MULTIPLY_SCALE_BITS); \
  z5 = _mm_sub_epi16(tmp10, tmp12); \
  z5 = _mm_mulhi_epi16(z5, pw_0382); \
  \
  z2 = _mm_add_epi16(_mm_mulhi_epi16(tmp10, pw_0541), z5); \
  z4 = _mm_add_epi16(_mm_mulhi_epi16(tmp12, pw_1306), z5); \
  \
  tmp11 = _mm_slli_epi16(tmp11, PRE_MULTIPLY_SCALE_BITS); \
  z3 = _mm_mulhi_epi16(tmp11, pw_0707); \
  \
  z11 = _mm_add_epi16(tmp7, z3); \
  z13 = _mm_sub_epi16(tmp7, z3); \
  \
  out5 = _mm_add_epi16(z13, z2); \
  out3 = _mm_sub_epi16(z13, z2); \
  out1 = _mm_add_epi16(z11, z4); \
  out7 = _mm_sub_epi16(z11, z4); \
}


void jsimd_fdct_ifast_e2k(DCTELEM *data)
{
  __m128i row0, row1, row2, row3, row4, row5, row6, row7,
    col0, col1, col2, col3, col4, col5, col6, col7,
    tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13,
    z1, z2, z3, z4, z5, z11, z13,
    out0, out1, out2, out3, out4, out5, out6, out7;

  /* Constants */
  __m128i pw_0382 = _mm_set1_epi16(F_0_382 << CONST_SHIFT),
    pw_0541 = _mm_set1_epi16(F_0_541 << CONST_SHIFT),
    pw_0707 = _mm_set1_epi16(F_0_707 << CONST_SHIFT),
    pw_1306 = _mm_set1_epi16(F_1_306 << CONST_SHIFT);

  /* Pass 1: process rows */

  row0 = VEC_LD(data + 0 * 8);
  row1 = VEC_LD(data + 1 * 8);
  row2 = VEC_LD(data + 2 * 8);
  row3 = VEC_LD(data + 3 * 8);
  row4 = VEC_LD(data + 4 * 8);
  row5 = VEC_LD(data + 5 * 8);
  row6 = VEC_LD(data + 6 * 8);
  row7 = VEC_LD(data + 7 * 8);

  TRANSPOSE(row, col);

  tmp0 = _mm_add_epi16(col0, col7);
  tmp7 = _mm_sub_epi16(col0, col7);
  tmp1 = _mm_add_epi16(col1, col6);
  tmp6 = _mm_sub_epi16(col1, col6);
  tmp2 = _mm_add_epi16(col2, col5);
  tmp5 = _mm_sub_epi16(col2, col5);
  tmp3 = _mm_add_epi16(col3, col4);
  tmp4 = _mm_sub_epi16(col3, col4);

  DO_FDCT();

  /* Pass 2: process columns */

  TRANSPOSE(out, row);

  tmp0 = _mm_add_epi16(row0, row7);
  tmp7 = _mm_sub_epi16(row0, row7);
  tmp1 = _mm_add_epi16(row1, row6);
  tmp6 = _mm_sub_epi16(row1, row6);
  tmp2 = _mm_add_epi16(row2, row5);
  tmp5 = _mm_sub_epi16(row2, row5);
  tmp3 = _mm_add_epi16(row3, row4);
  tmp4 = _mm_sub_epi16(row3, row4);

  DO_FDCT();

  VEC_ST(data + 0 * 8, out0);
  VEC_ST(data + 1 * 8, out1);
  VEC_ST(data + 2 * 8, out2);
  VEC_ST(data + 3 * 8, out3);
  VEC_ST(data + 4 * 8, out4);
  VEC_ST(data + 5 * 8, out5);
  VEC_ST(data + 6 * 8, out6);
  VEC_ST(data + 7 * 8, out7);
}
