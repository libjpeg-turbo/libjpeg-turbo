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

/* FLOAT FORWARD DCT */

#include "jsimd_e2k.h"

#define DO_FDCT(in, out) { \
  tmp0 = _mm_add_ps(in##0, in##7); \
  tmp7 = _mm_sub_ps(in##0, in##7); \
  tmp1 = _mm_add_ps(in##1, in##6); \
  tmp6 = _mm_sub_ps(in##1, in##6); \
  tmp2 = _mm_add_ps(in##2, in##5); \
  tmp5 = _mm_sub_ps(in##2, in##5); \
  tmp3 = _mm_add_ps(in##3, in##4); \
  tmp4 = _mm_sub_ps(in##3, in##4); \
  \
  /* Even part */ \
  \
  tmp10 = _mm_add_ps(tmp0, tmp3); \
  tmp13 = _mm_sub_ps(tmp0, tmp3); \
  tmp11 = _mm_add_ps(tmp1, tmp2); \
  tmp12 = _mm_sub_ps(tmp1, tmp2); \
  \
  out##0 = _mm_add_ps(tmp10, tmp11); \
  out##4 = _mm_sub_ps(tmp10, tmp11); \
  \
  z1 = _mm_mul_ps(_mm_add_ps(tmp12, tmp13), pd_f0707); \
  out##2 = _mm_add_ps(tmp13, z1); \
  out##6 = _mm_sub_ps(tmp13, z1); \
  \
  /* Odd part */ \
  \
  tmp10 = _mm_add_ps(tmp4, tmp5); \
  tmp11 = _mm_add_ps(tmp5, tmp6); \
  tmp12 = _mm_add_ps(tmp6, tmp7); \
  \
  z5 = _mm_mul_ps(_mm_sub_ps(tmp10, tmp12), pd_f0382); \
  z2 = _mm_add_ps(_mm_mul_ps(tmp10, pd_f0541), z5); \
  z4 = _mm_add_ps(_mm_mul_ps(tmp12, pd_f1306), z5); \
  z3 = _mm_mul_ps(tmp11, pd_f0707); \
  \
  z11 = _mm_add_ps(tmp7, z3); \
  z13 = _mm_sub_ps(tmp7, z3); \
  \
  out##5 = _mm_add_ps(z13, z2); \
  out##3 = _mm_sub_ps(z13, z2); \
  out##1 = _mm_add_ps(z11, z4); \
  out##7 = _mm_sub_ps(z11, z4); \
}

#define LOAD_DATA(a, b, c, d, l, i) \
  l##a = _mm_loadu_ps(data + a * 8 + i); \
  l##b = _mm_loadu_ps(data + b * 8 + i); \
  l##c = _mm_loadu_ps(data + c * 8 + i); \
  l##d = _mm_loadu_ps(data + d * 8 + i);

#define STORE_DATA(a, b, c, d, l, i) \
  _mm_storeu_ps(data + a * 8 + i, l##a); \
  _mm_storeu_ps(data + b * 8 + i, l##b); \
  _mm_storeu_ps(data + c * 8 + i, l##c); \
  _mm_storeu_ps(data + d * 8 + i, l##d);


void jsimd_fdct_float_e2k(FAST_FLOAT *data)
{
  __m128 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
    tmp10, tmp11, tmp12, tmp13, z1, z2, z3, z4, z5, z11, z13;
  __m128 l0, l1, l2, l3, l4, l5, l6, l7;
  __m128 h0, h1, h2, h3, h4, h5, h6, h7;
  __m128 x0, x1, x2, x3, x4, x5, x6, x7;
  __m128 y0, y1, y2, y3, y4, y5, y6, y7;

  /* Constants */
  __m128 pd_f0382 = _mm_set1_ps(0.382683433f),
    pd_f0541 = _mm_set1_ps(0.541196100f),
    pd_f0707 = _mm_set1_ps(0.707106781f),
    pd_f1306 = _mm_set1_ps(1.306562965f);

  /* Pass 1: process columns */

  LOAD_DATA(0, 1, 2, 3, x, 0)
  LOAD_DATA(0, 1, 2, 3, y, 4)
  TRANSPOSE_FLOAT(x0, x1, x2, x3, l0, l1, l2, l3)
  TRANSPOSE_FLOAT(y0, y1, y2, y3, l4, l5, l6, l7)
  DO_FDCT(l, l);

  LOAD_DATA(4, 5, 6, 7, x, 0)
  LOAD_DATA(4, 5, 6, 7, y, 4)
  TRANSPOSE_FLOAT(x4, x5, x6, x7, h0, h1, h2, h3)
  TRANSPOSE_FLOAT(y4, y5, y6, y7, h4, h5, h6, h7)
  DO_FDCT(h, h);

  /* Pass 2: process rows */

  TRANSPOSE_FLOAT(l0, l1, l2, l3, x0, x1, x2, x3)
  TRANSPOSE_FLOAT(h0, h1, h2, h3, x4, x5, x6, x7)
  DO_FDCT(x, x);
  STORE_DATA(0, 1, 2, 3, x, 0)
  STORE_DATA(4, 5, 6, 7, x, 0)

  TRANSPOSE_FLOAT(l4, l5, l6, l7, y0, y1, y2, y3)
  TRANSPOSE_FLOAT(h4, h5, h6, h7, y4, y5, y6, y7)
  DO_FDCT(y, y);
  STORE_DATA(0, 1, 2, 3, y, 4)
  STORE_DATA(4, 5, 6, 7, y, 4)
}
