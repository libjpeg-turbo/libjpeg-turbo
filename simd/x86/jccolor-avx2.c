/*
 * Colorspace conversion (64-bit AVX2)
 *
 * Copyright (C) 2009, 2011, 2016, 2024-2025, D. R. Commander.
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

#include "../jsimdint.h"
#include "../align.h"

#include <immintrin.h>


/* RGB -> YCbCr conversion constants */

#define SCALEBITS  16

#define F_0_081  5329   /* FIX(0.08131) */
#define F_0_114  7471   /* FIX(0.11400) */
#define F_0_168  11059  /* FIX(0.16874) */
#define F_0_250  16384  /* FIX(0.25000) */
#define F_0_299  19595  /* FIX(0.29900) */
#define F_0_331  21709  /* FIX(0.33126) */
#define F_0_418  27439  /* FIX(0.41869) */
#define F_0_587  38470  /* FIX(0.58700) */
#define F_0_337  (F_0_587 - F_0_250)  /* FIX(0.58700) - FIX(0.25000) */


/* Include inline routines for colorspace extensions. */

#if RGB_RED == 0
#define RE  aE
#define RO  aO
#elif RGB_RED == 1
#define RE  bE
#define RO  bO
#elif RGB_RED == 2
#define RE  cE
#define RO  cO
#else
#define RE  dE
#define RO  dO
#endif

#if RGB_GREEN == 0
#define GE  aE
#define GO  aO
#elif RGB_GREEN == 1
#define GE  bE
#define GO  bO
#elif RGB_GREEN == 2
#define GE  cE
#define GO  cO
#else
#define GE  dE
#define GO  dO
#endif

#if RGB_BLUE == 0
#define BE  aE
#define BO  aO
#elif RGB_BLUE == 1
#define BE  bE
#define BO  bO
#elif RGB_BLUE == 2
#define BE  cE
#define BO  cO
#else
#define BE  dE
#define BO  dO
#endif

#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED  EXT_RGB_RED
#define RGB_GREEN  EXT_RGB_GREEN
#define RGB_BLUE  EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define RE  aE
#define GE  bE
#define BE  cE
#define RO  aO
#define GO  bO
#define BO  cO
#define jsimd_rgb_ycc_convert_avx2  jsimd_extrgb_ycc_convert_avx2
#define jsimd_rgb_ycc_convert_cols  jsimd_extrgb_ycc_convert_cols
#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef RE
#undef GE
#undef BE
#undef RO
#undef GO
#undef BO
#undef jsimd_rgb_ycc_convert_avx2
#undef jsimd_rgb_ycc_convert_cols

#define RGB_RED  EXT_RGBX_RED
#define RGB_GREEN  EXT_RGBX_GREEN
#define RGB_BLUE  EXT_RGBX_BLUE
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define RE  aE
#define GE  bE
#define BE  cE
#define RO  aO
#define GO  bO
#define BO  cO
#define jsimd_rgb_ycc_convert_avx2  jsimd_extrgbx_ycc_convert_avx2
#define jsimd_rgb_ycc_convert_cols  jsimd_extrgbx_ycc_convert_cols
#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef RE
#undef GE
#undef BE
#undef RO
#undef GO
#undef BO
#undef jsimd_rgb_ycc_convert_avx2
#undef jsimd_rgb_ycc_convert_cols

#define RGB_RED  EXT_BGR_RED
#define RGB_GREEN  EXT_BGR_GREEN
#define RGB_BLUE  EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define RE  cE
#define GE  bE
#define BE  aE
#define RO  cO
#define GO  bO
#define BO  aO
#define jsimd_rgb_ycc_convert_avx2  jsimd_extbgr_ycc_convert_avx2
#define jsimd_rgb_ycc_convert_cols  jsimd_extbgr_ycc_convert_cols
#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef RE
#undef GE
#undef BE
#undef RO
#undef GO
#undef BO
#undef jsimd_rgb_ycc_convert_avx2
#undef jsimd_rgb_ycc_convert_cols

#define RGB_RED  EXT_BGRX_RED
#define RGB_GREEN  EXT_BGRX_GREEN
#define RGB_BLUE  EXT_BGRX_BLUE
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define RE  cE
#define GE  bE
#define BE  aE
#define RO  cO
#define GO  bO
#define BO  aO
#define jsimd_rgb_ycc_convert_avx2  jsimd_extbgrx_ycc_convert_avx2
#define jsimd_rgb_ycc_convert_cols  jsimd_extbgrx_ycc_convert_cols
#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef RE
#undef GE
#undef BE
#undef RO
#undef GO
#undef BO
#undef jsimd_rgb_ycc_convert_avx2
#undef jsimd_rgb_ycc_convert_cols

#define RGB_RED  EXT_XBGR_RED
#define RGB_GREEN  EXT_XBGR_GREEN
#define RGB_BLUE  EXT_XBGR_BLUE
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define RE  dE
#define GE  cE
#define BE  bE
#define RO  dO
#define GO  cO
#define BO  bO
#define jsimd_rgb_ycc_convert_avx2  jsimd_extxbgr_ycc_convert_avx2
#define jsimd_rgb_ycc_convert_cols  jsimd_extxbgr_ycc_convert_cols
#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef RE
#undef GE
#undef BE
#undef RO
#undef GO
#undef BO
#undef jsimd_rgb_ycc_convert_avx2
#undef jsimd_rgb_ycc_convert_cols

#define RGB_RED  EXT_XRGB_RED
#define RGB_GREEN  EXT_XRGB_GREEN
#define RGB_BLUE  EXT_XRGB_BLUE
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define RE  bE
#define GE  cE
#define BE  dE
#define RO  bO
#define GO  cO
#define BO  dO
#define jsimd_rgb_ycc_convert_avx2  jsimd_extxrgb_ycc_convert_avx2
#define jsimd_rgb_ycc_convert_cols  jsimd_extxrgb_ycc_convert_cols
#include "jccolext-avx2.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef RE
#undef GE
#undef BE
#undef RO
#undef GO
#undef BO
#undef jsimd_rgb_ycc_convert_avx2
#undef jsimd_rgb_ycc_convert_cols
