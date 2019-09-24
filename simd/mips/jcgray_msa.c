/*
 * MIPS MSA optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * All rights reserved.
 * Authors: Vikram Dattu (vikram.dattu@imgtec.com)
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
#include "jmacros_msa.h"


#define SCALE  16
#define ONE_HALF  (1 << (SCALE - 1))
#define CBCR_OFFSET  (128 << SCALE)
#define GETSAMPLE(value)  ((int)(value))
#define RGB_CONVERT_COEFF  { 19595, 16384, 22086, 7471, 0, 0, 0, 0 }
#define BGR_CONVERT_COEFF  { 7471, 22086, 16384, 19595, 0, 0, 0, 0 }

#define COEFF_CONST  RGB_CONVERT_COEFF
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST

#define RGB_RED        EXT_RGB_RED
#define RGB_GREEN      EXT_RGB_GREEN
#define RGB_BLUE       EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define COEFF_CONST    RGB_CONVERT_COEFF
#define jsimd_rgb_gray_convert_msa  jsimd_extrgb_gray_convert_msa
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST
#undef jsimd_rgb_gray_convert_msa

#define RGB_RED        EXT_BGR_RED
#define RGB_GREEN      EXT_BGR_GREEN
#define RGB_BLUE       EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define COEFF_CONST    BGR_CONVERT_COEFF
#define jsimd_rgb_gray_convert_msa  jsimd_extbgr_gray_convert_msa
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST
#undef jsimd_rgb_gray_convert_msa

#define RGB_RED        EXT_RGBX_RED
#define RGB_GREEN      EXT_RGBX_GREEN
#define RGB_BLUE       EXT_RGBX_BLUE
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define COEFF_CONST    RGB_CONVERT_COEFF
#define jsimd_rgb_gray_convert_msa  jsimd_extrgbx_gray_convert_msa
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST
#undef jsimd_rgb_gray_convert_msa

#define RGB_RED        EXT_BGRX_RED
#define RGB_GREEN      EXT_BGRX_GREEN
#define RGB_BLUE       EXT_BGRX_BLUE
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define COEFF_CONST    BGR_CONVERT_COEFF
#define jsimd_rgb_gray_convert_msa  jsimd_extbgrx_gray_convert_msa
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST
#undef jsimd_rgb_gray_convert_msa

#define RGB_RED        EXT_XRGB_RED
#define RGB_GREEN      EXT_XRGB_GREEN
#define RGB_BLUE       EXT_XRGB_BLUE
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define COEFF_CONST    RGB_CONVERT_COEFF
#define jsimd_rgb_gray_convert_msa  jsimd_extxrgb_gray_convert_msa
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST
#undef jsimd_rgb_gray_convert_msa

#define RGB_RED        EXT_XBGR_RED
#define RGB_GREEN      EXT_XBGR_GREEN
#define RGB_BLUE       EXT_XBGR_BLUE
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define COEFF_CONST    BGR_CONVERT_COEFF
#define jsimd_rgb_gray_convert_msa  jsimd_extxbgr_gray_convert_msa
#include "jcgryext_msa.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef COEFF_CONST
#undef jsimd_rgb_gray_convert_msa
