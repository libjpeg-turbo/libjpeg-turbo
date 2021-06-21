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

/* YCC --> RGB CONVERSION */

#include "jsimd_e2k.h"
#ifdef HAVE_EML
#include "eml.h"
#endif


#define F_0_344  22554              /* FIX(0.34414) */
#define F_0_714  46802              /* FIX(0.71414) */
#define F_1_402  91881              /* FIX(1.40200) */
#define F_1_772  116130             /* FIX(1.77200) */
#define F_0_402  (F_1_402 - 65536)  /* FIX(1.40200) - FIX(1) */
#define F_0_285  (65536 - F_0_714)  /* FIX(1) - FIX(0.71414) */
#define F_0_228  (131072 - F_1_772) /* FIX(2) - FIX(1.77200) */

#define SCALEBITS  16
#define ONE_HALF  (1 << (SCALEBITS - 1))

#define RGB_INDEX  0, 1, 2,  4, 5, 6,  8, 9, 10,  12, 13, 14

#include "jdcolext-e2k.c"

#define RGB_RED  EXT_RGB_RED
#define RGB_GREEN  EXT_RGB_GREEN
#define RGB_BLUE  EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_e2k  jsimd_ycc_extrgb_convert_e2k
#include "jdcolext-e2k.c"

#define RGB_RED  EXT_RGBX_RED
#define RGB_GREEN  EXT_RGBX_GREEN
#define RGB_BLUE  EXT_RGBX_BLUE
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define jsimd_ycc_rgb_convert_e2k  jsimd_ycc_extrgbx_convert_e2k
#include "jdcolext-e2k.c"

#define RGB_RED  EXT_BGR_RED
#define RGB_GREEN  EXT_BGR_GREEN
#define RGB_BLUE  EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_e2k  jsimd_ycc_extbgr_convert_e2k
#include "jdcolext-e2k.c"

#define RGB_RED  EXT_BGRX_RED
#define RGB_GREEN  EXT_BGRX_GREEN
#define RGB_BLUE  EXT_BGRX_BLUE
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define jsimd_ycc_rgb_convert_e2k  jsimd_ycc_extbgrx_convert_e2k
#include "jdcolext-e2k.c"

#define RGB_RED  EXT_XBGR_RED
#define RGB_GREEN  EXT_XBGR_GREEN
#define RGB_BLUE  EXT_XBGR_BLUE
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_e2k  jsimd_ycc_extxbgr_convert_e2k
#include "jdcolext-e2k.c"

#define RGB_RED  EXT_XRGB_RED
#define RGB_GREEN  EXT_XRGB_GREEN
#define RGB_BLUE  EXT_XRGB_BLUE
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_e2k  jsimd_ycc_extxrgb_convert_e2k
#include "jdcolext-e2k.c"

