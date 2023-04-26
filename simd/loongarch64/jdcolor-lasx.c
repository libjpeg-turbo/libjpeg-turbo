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

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jsimd.h"
#include "jconfigint.h"
#include "jmacros_lasx.h"


#define FIX_140200  91881
#define FIX_34414   22554
#define FIX_71414   46802
#define FIX_177200  116130
#define FIX_28586   18734

#define DESCALE(x,n)  (((x)+(1<<((n)-1)))>>(n))

#define LASX_DOTP2_W_H(_in0, _in1, _out0) \
{ \
  __m256i _tmp0, _tmp1, _tmp2, _tmp3; \
  _tmp0 = __lasx_xvsrai_h(_in0, 15); \
  _tmp1 = __lasx_xvsrai_h(_in1, 15); \
  _tmp2 = __lasx_xvpackev_h(_tmp0, _in0); \
  _tmp3 = __lasx_xvpackev_h(_tmp1, _in1); \
  _out0 = __lasx_xvmul_w(_tmp2, _tmp3); \
  _tmp2 = __lasx_xvpackod_h(_tmp0, _in0); \
  _tmp3 = __lasx_xvpackod_h(_tmp1, _in1); \
  _tmp0 = __lasx_xvmul_w(_tmp2, _tmp3); \
  _out0 = __lasx_xvadd_w(_out0, _tmp0); \
}

static INLINE unsigned char clip_pixel(int val)
{
  return ((val & ~0xFF) ? ((-val) >> 31) & 0xFF : val);
}

#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED        EXT_RGB_RED
#define RGB_GREEN      EXT_RGB_GREEN
#define RGB_BLUE       EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_lasx  jsimd_ycc_extrgb_convert_lasx
#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_lasx

#define RGB_RED        EXT_BGR_RED
#define RGB_GREEN      EXT_BGR_GREEN
#define RGB_BLUE       EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_lasx  jsimd_ycc_extbgr_convert_lasx
#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_lasx

#define RGB_RED        EXT_RGBX_RED
#define RGB_GREEN      EXT_RGBX_GREEN
#define RGB_BLUE       EXT_RGBX_BLUE
#define RGB_ALPHA      3
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define jsimd_ycc_rgb_convert_lasx  jsimd_ycc_extrgbx_convert_lasx
#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_lasx

#define RGB_RED        EXT_BGRX_RED
#define RGB_GREEN      EXT_BGRX_GREEN
#define RGB_BLUE       EXT_BGRX_BLUE
#define RGB_ALPHA      3
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define jsimd_ycc_rgb_convert_lasx  jsimd_ycc_extbgrx_convert_lasx
#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_lasx

#define RGB_RED        EXT_XRGB_RED
#define RGB_GREEN      EXT_XRGB_GREEN
#define RGB_BLUE       EXT_XRGB_BLUE
#define RGB_ALPHA      0
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_lasx  jsimd_ycc_extxrgb_convert_lasx
#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_lasx

#define RGB_RED        EXT_XBGR_RED
#define RGB_GREEN      EXT_XBGR_GREEN
#define RGB_BLUE       EXT_XBGR_BLUE
#define RGB_ALPHA      0
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_lasx  jsimd_ycc_extxbgr_convert_lasx
#include "jdcolext-lasx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_lasx
