/*
 * LOONGARCH LSX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2023 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Song Ding (songding@loongson.cn)
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

#ifndef __JMACROS_LSX_H__
#define __JMACROS_LSX_H__

#include <stdint.h>
#include "generic_macros_lsx.h"

#define LSX_UNPCKL_HU_BU(_in, _out)   \
{                                     \
  _out = __lsx_vsllwil_hu_bu(_in, 0); \
}

#define LSX_UNPCKL_H_B(_in, _out)     \
{                                     \
  _out = __lsx_vsllwil_h_b(_in, 0);   \
}

#define LSX_UNPCKLH_HU_BU(_in, _out_h, _out_l) \
{                                              \
  _out_l = __lsx_vsllwil_hu_bu(_in, 0);        \
  _out_h = __lsx_vexth_hu_bu(_in);             \
}

#define LSX_UNPCKLH_H_B(_in, _out_h, _out_l)   \
{                                              \
  _out_l = __lsx_vsllwil_h_b(_in, 0);          \
  _out_h = __lsx_vexth_h_b(_in);               \
}

#define LSX_UNPCKL_WU_HU(_in, _out)            \
{                                              \
  _out = __lsx_vsllwil_wu_hu(_in, 0);          \
}

#define LSX_UNPCKL_W_H(_in, _out)              \
{                                              \
  _out = __lsx_vsllwil_w_h(_in, 0);            \
}

#define LSX_UNPCKL_W_H_2(_in0, _in1, _out0, _out1)  \
{                                                   \
  LSX_UNPCKL_W_H(_in0, _out0);                      \
  LSX_UNPCKL_W_H(_in1, _out1);                      \
}

#define LSX_UNPCKLH_WU_HU(_in, _out_h, _out_l) \
{                                              \
  _out_l = __lsx_vsllwil_wu_hu(_in, 0);        \
  _out_h = __lsx_vexth_wu_hu(_in);             \
}

#define LSX_UNPCKLH_W_H(_in, _out_h, _out_l)   \
{                                              \
  _out_l = __lsx_vsllwil_w_h(_in, 0);          \
  _out_h = __lsx_vexth_w_h(_in);               \
}

#define LSX_UNPCKLH_W_H_2(_in0, _in1, _out0_h, _out0_l, _out1_h, _out1_l)    \
{                                                                            \
  LSX_UNPCKLH_W_H(_in0, _out0_h, _out0_l);                                   \
  LSX_UNPCKLH_W_H(_in1, _out1_h, _out1_l);                                   \
}

#define LSX_UNPCKLH_W_H_4(_in0, _in1, _in2, _in3, _out0_h, _out0_l, _out1_h, \
                          _out1_l, _out2_h, _out2_l, _out3_h, _out3_l)       \
{                                                                            \
  LSX_UNPCKLH_W_H_2(_in0, _in1, _out0_h, _out0_l, _out1_h, _out1_l);         \
  LSX_UNPCKLH_W_H_2(_in2, _in3, _out2_h, _out2_l, _out3_h, _out3_l);         \
}

#define LSX_UNPCK_WU_BU_4(_in, _out0, _out1, _out2, _out3)                   \
{                                                                            \
  __m128i zero = __lsx_vldi(0);                                              \
  v16u8 mask0 = {0,16,16,16,1,16,16,16,2,16,16,16,3,16,16,16};               \
  v16u8 mask1 = {4,16,16,16,5,16,16,16,6,16,16,16,7,16,16,16};               \
  v16u8 mask2 = {8,16,16,16,9,16,16,16,10,16,16,16,11,16,16,16};             \
  v16u8 mask3 = {12,16,16,16,13,16,16,16,14,16,16,16,15,16,16,16};           \
  _out0 = __lsx_vshuf_b(zero, _in, (__m128i)mask0);                          \
  _out1 = __lsx_vshuf_b(zero, _in, (__m128i)mask1);                          \
  _out2 = __lsx_vshuf_b(zero, _in, (__m128i)mask2);                          \
  _out3 = __lsx_vshuf_b(zero, _in, (__m128i)mask3);                          \
}

#define LSX_SRARI_W_4(_in0, _in1, _in2, _in3, shift_value) {                 \
  _in0 = __lsx_vsrari_w(_in0, shift_value);                                  \
  _in1 = __lsx_vsrari_w(_in1, shift_value);                                  \
  _in2 = __lsx_vsrari_w(_in2, shift_value);                                  \
  _in3 = __lsx_vsrari_w(_in3, shift_value);                                  \
}

#endif /* __JMACROS_LSX_H__ */
