/*
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2011, Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2009-2011, 2013-2014, 2016, 2018, D. R. Commander.
 * Copyright (C) 2015-2016, 2018, Matthieu Darbois.
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 */

#define JPEG_INTERNALS
#include "../../../jinclude.h"
#include "../../../jpeglib.h"
#include "../../../jsimd.h"
#include "../../../jdct.h"
#include "../../../jsimddct.h"
#include "../../jsimd.h"

#include <arm_neon.h>