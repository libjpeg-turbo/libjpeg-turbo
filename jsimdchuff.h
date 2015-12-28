/*
 * jsimdchuff.h
 *
 * Copyright 2015 Matthieu Darbois
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 */

EXTERN(int)
jsimd_can_chuff_encode_one_block (void);


EXTERN(JOCTET*)
jsimd_chuff_encode_one_block (/*working_state*/void * state, JOCTET *buffer,
                              JCOEFPTR block, int last_dc_val,
                              c_derived_tbl *dctbl, c_derived_tbl *actbl);
