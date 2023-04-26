/*
 * LOONGARCH LASX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2021 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Jin Bo (jinbo@loongson.cn)
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

#ifndef __JMACROS_LASX_H__
#define __JMACROS_LASX_H__

#include <stdint.h>
#include "generic_macros_lasx.h"

#define LASX_UNPCKL_HU_BU(_in, _out) { \
  _out = __lasx_vext2xv_hu_bu(_in); \
}

#define LASX_UNPCKL_H_B(_in, _out) { \
  _out = __lasx_vext2xv_h_b(_in); \
}

#define LASX_UNPCKLH_HU_BU(_in, _out_h, _out_l) { \
  __m256i _tmp_h; \
  _tmp_h = __lasx_xvpermi_q(_in, _in, 0x33); \
  _out_l = __lasx_vext2xv_hu_bu(_in); \
  _out_h = __lasx_vext2xv_hu_bu(_tmp_h); \
}

#define LASX_UNPCKLH_H_B(_in, _out_h, _out_l) { \
  __m256i _tmp_h; \
  _tmp_h = __lasx_xvpermi_q(_in, _in, 0x33); \
  _out_l = __lasx_vext2xv_h_b(_in); \
  _out_h = __lasx_vext2xv_h_b(_tmp_h); \
}

#define LASX_UNPCKL_WU_HU(_in, _out) { \
  _out = __lasx_vext2xv_wu_hu(_in); \
}

#define LASX_UNPCKL_W_H(_in, _out) { \
  _out = __lasx_vext2xv_w_h(_in); \
}

#define LASX_UNPCKLH_WU_HU(_in, _out_h, _out_l) { \
  __m256i _tmp_h; \
  _tmp_h = __lasx_xvpermi_q(_in, _in, 0x33); \
  _out_l = __lasx_vext2xv_wu_hu(_in); \
  _out_h = __lasx_vext2xv_wu_hu(_tmp_h); \
}

#define LASX_UNPCKLH_W_H(_in, _out_h, _out_l) { \
  __m256i _tmp_h; \
  _tmp_h = __lasx_xvpermi_q(_in, _in, 0x33); \
  _out_l = __lasx_vext2xv_w_h(_in); \
  _out_h = __lasx_vext2xv_w_h(_tmp_h); \
}

#define LASX_UNPCK_WU_BU_4(_in, _out0, _out1, _out2, _out3) {\
\
  _out0 = __lasx_vext2xv_wu_bu(_in); \
  _out1 = __lasx_xvpermi_d(_in, 0xe1); \
  _out1 = __lasx_vext2xv_wu_bu(_out1); \
  _out2 = __lasx_xvpermi_d(_in, 0xc6); \
  _out2 = __lasx_vext2xv_wu_bu(_out2); \
  _out3 = __lasx_xvpermi_d(_in, 0x27); \
  _out3 = __lasx_vext2xv_wu_bu(_out3); \
}

#define LASX_SRARI_W_4(_in0, _in1, _in2, _in3, shift_value) { \
  _in0 = __lasx_xvsrari_w(_in0, shift_value); \
  _in1 = __lasx_xvsrari_w(_in1, shift_value); \
  _in2 = __lasx_xvsrari_w(_in2, shift_value); \
  _in3 = __lasx_xvsrari_w(_in3, shift_value); \
}

/*
 * 1,2,3,4,5,6,7,8     1 1 1 1 5 5 5 5
 * 1,2,3,4,5,6,7,8 --> 2 2 2 2 6 6 6 6
 * 1,2,3,4,5,6,7,8     3 3 3 3 7 7 7 7
 * 1,2,3,4,5,6,7,8     4 4 4 4 8 8 8 8
 */
#define LASX_TRANSPOSE4x4_W_128SV(_in0, _in1, _in2, _in3, \
                                  _out0, _out1, _out2, _out3) \
{ \
  __m256i _tmp0,_tmp1,_tmp2,_tmp3; \
  LASX_ILVLH_W_128SV(_in1, _in0, _tmp1, _tmp0); \
  LASX_ILVLH_W_128SV(_in3, _in2, _tmp3, _tmp2); \
  LASX_PCKEV_D_128SV(_tmp2, _tmp0, _out0); \
  LASX_PCKOD_D_128SV(_tmp2, _tmp0, _out1); \
  LASX_PCKEV_D_128SV(_tmp3, _tmp1, _out2); \
  LASX_PCKOD_D_128SV(_tmp3, _tmp1, _out3); \
}

#endif /* __JMACROS_LASX_H__ */
