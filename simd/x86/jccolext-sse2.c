/*
 * Colorspace conversion (64-bit SSE2)
 *
 * Copyright (C) 2009, 2016, 2024-2025, D. R. Commander.
 * Copyright (C) 2020, Arm Limited.  All Rights Reserved.
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

/* This file is included by jccolor-sse2.c */

#include "../../src/jpeglib.h"


/* RGB -> YCbCr conversion is defined by the following equations:
 *    Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *    Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + 128
 *    Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + 128
 *
 * Avoid floating point arithmetic by using shifted integer constants:
 *    0.29899597 = 19595 * 2^-16
 *    0.58700561 = 38470 * 2^-16
 *    0.11399841 =  7471 * 2^-16
 *    0.16874695 = 11059 * 2^-16
 *    0.33125305 = 21709 * 2^-16
 *    0.50000000 = 32768 * 2^-16
 *    0.41868592 = 27439 * 2^-16
 *    0.08131409 =  5329 * 2^-16
 * These constants are defined in jccolor-sse2.c
 *
 * We add the fixed-point equivalent of 0.5 to Cb and Cr, which effectively
 * rounds up or down the result via integer truncation.
 */


INLINE LOCAL(void)
jsimd_rgb_ycc_convert_cols(JSAMPROW inptr, JSAMPROW outptr0, JSAMPROW outptr1,
                           JSAMPROW outptr2)
{
  __m128i PW_F0299_F0337 = _mm_set_epi16(F_0_337, F_0_299, F_0_337, F_0_299,
                                         F_0_337, F_0_299, F_0_337, F_0_299);
  __m128i PW_F0114_F0250 = _mm_set_epi16(F_0_250, F_0_114, F_0_250, F_0_114,
                                         F_0_250, F_0_114, F_0_250, F_0_114);
  __m128i PW_MF016_MF033 =
    _mm_set_epi16(-F_0_331, -F_0_168, -F_0_331, -F_0_168,
                  -F_0_331, -F_0_168, -F_0_331, -F_0_168);
  __m128i PW_MF008_MF041 =
    _mm_set_epi16(-F_0_418, -F_0_081, -F_0_418, -F_0_081,
                  -F_0_418, -F_0_081, -F_0_418, -F_0_081);
  __m128i PD_ONEHALFM1_CJ =
    _mm_set1_epi32((1 << (SCALEBITS - 1)) - 1 + (CENTERJSAMPLE << SCALEBITS));
  __m128i PD_ONEHALF = _mm_set1_epi32(1 << (SCALEBITS - 1));
  __m128i zero;
#if RGB_RED != EXT_XBGR_RED && RGB_RED != EXT_XRGB_RED
  __m128i aE, aO;
#endif
  __m128i bE, bO, cE, cO, aEbE;
  __m128i rel, reh, rol, roh;
  __m128i rel_gel, rel_gel_F0299_F0337, reh_geh, reh_geh_F0299_F0337;
  __m128i rol_gol, rol_gol_F0299_F0337, roh_goh, roh_goh_F0299_F0337;
  __m128i bel, beh, bol, boh;
  __m128i bel_gel, beh_geh, bol_gol, boh_goh;
  __m128i yel, yeh, ye, yol, yoh, yo, y;
  __m128i cbel, cbeh, cbe, cbol, cboh, cbo, cb;
  __m128i crel, creh, cre, crol, croh, cro, cr;

#if RGB_PIXELSIZE == 3

  __m128i abc0, abc1, abc2, abc0l, abc0h, abc1l, abc0l1h, abc0h2l, abc1l2h,
    abcEaO, bcOabE, bOcO, cEabcO, cEaO;

  /* NOTE: The values of RGB_RED, RGB_GREEN, and RGB_BLUE determine the mapping
   * of components A, B, and C to red, green, and blue.
   *
   * abc0 = (A0 B0 C0 A1 B1 C1 A2 B2 C2 A3 B3 C3 A4 B4 C4 A5)
   * abc1 = (B5 C5 A6 B6 C6 A7 B7 C7 A8 B8 C8 A9 B9 C9 Aa Ba)
   * abc2 = (Ca Ab Bb Cb Ac Bc Cc Ad Bd Cd Ae Be Ce Af Bf Cf)
   */
  abc0 = _mm_loadu_si128((__m128i *)inptr);
  abc1 = _mm_loadu_si128((__m128i *)inptr + 1);
  abc2 = _mm_loadu_si128((__m128i *)inptr + 2);

  /* abc0l = (-- -- -- -- -- -- -- -- A0 B0 C0 A1 B1 C1 A2 B2) */
  abc0l = _mm_slli_si128(abc0, 8);
  /* abc0h = (C2 A3 B3 C3 A4 B4 C4 A5 -- -- -- -- -- -- -- --) */
  abc0h = _mm_srli_si128(abc0, 8);

  /* abc0l1h = (A0 A8 B0 B8 C0 C8 A1 A9 B1 B9 C1 C9 A2 Aa B2 Ba) */
  abc0l1h = _mm_unpackhi_epi8(abc0l, abc1);
  /* abc1l = (-- -- -- -- -- -- -- -- B5 C5 A6 B6 C6 A7 B7 C7) */
  abc1l = _mm_slli_si128(abc1, 8);

  /* abc0h2l = (C2 Ca A3 Ab B3 Bb C3 Cb A4 Ac B4 Bc C4 Cc A5 Ad) */
  abc0h2l = _mm_unpacklo_epi8(abc0h, abc2);
  /* abc1l2h = (B5 Bd C5 Cd A6 Ae B6 Be C6 Ce A7 Af B7 Bf C7 Cf) */
  abc1l2h = _mm_unpackhi_epi8(abc1l, abc2);

  /* abcEaO = (-- -- -- -- -- -- -- -- A0 A8 B0 B8 C0 C8 A1 A9) */
  abcEaO = _mm_slli_si128(abc0l1h, 8);
  /* bcOabE = (B1 B9 C1 C9 A2 Aa B2 Ba -- -- -- -- -- -- -- --) */
  bcOabE = _mm_srli_si128(abc0l1h, 8);

  /* abcEaO = (A0 A4 A8 Ac B0 B4 B8 Bc C0 C4 C8 Cc A1 A5 A9 Ad) */
  abcEaO = _mm_unpackhi_epi8(abcEaO, abc0h2l);
  /* cEabcO = (-- -- -- -- -- -- -- -- C2 Ca A3 Ab B3 Bb C3 Cb) */
  cEabcO = _mm_slli_si128(abc0h2l, 8);

  /* bcOabE = (B1 B5 B9 Bd C1 C5 C9 Cd A2 A6 Aa Ae B2 B6 Ba Be) */
  bcOabE = _mm_unpacklo_epi8(bcOabE, abc1l2h);
  /* cEabcO = (C2 C6 Ca Ce A3 A7 Ab Af B3 B7 Bb Bf C3 C7 Cb Cf) */
  cEabcO = _mm_unpackhi_epi8(cEabcO, abc1l2h);

  /* aEbE = (-- -- -- -- -- -- -- -- A0 A4 A8 Ac B0 B4 B8 Bc) */
  aEbE = _mm_slli_si128(abcEaO, 8);
  /* cEaO = (C0 C4 C8 Cc A1 A5 A9 Ad -- -- -- -- -- -- -- --) */
  cEaO = _mm_srli_si128(abcEaO, 8);

  /* aEbE = (A0 A2 A4 A6 A8 Aa Ac Ae B0 B2 B4 B6 B8 Ba Bc Be) */
  aEbE = _mm_unpackhi_epi8(aEbE, bcOabE);
  /* bOcO = (-- -- -- -- -- -- -- -- B1 B5 B9 Bd C1 C5 C9 Cd) */
  bOcO = _mm_slli_si128(bcOabE, 8);

  /* cEaO = (C0 C2 C4 C6 C8 Ca Cc Ce A1 A3 A5 A7 A9 Ab Ad Af) */
  cEaO = _mm_unpacklo_epi8(cEaO, cEabcO);
  /* bOcO = (B1 B3 B5 B7 B9 Bb Bd Bf C1 C3 C5 C7 C9 Cb Cd Cf) */
  bOcO = _mm_unpackhi_epi8(bOcO, cEabcO);

  zero = _mm_setzero_si128();

  aE = _mm_unpacklo_epi8(aEbE, zero);  /* aE = (A0 A2 A4 A6 A8 Aa Ac Ae) */
  bE = _mm_unpackhi_epi8(aEbE, zero);  /* bE = (B0 B2 B4 B6 B8 Ba Bc Be) */

  cE = _mm_unpacklo_epi8(cEaO, zero);  /* cE = (C0 C2 C4 C6 C8 Ca Cc Ce) */
  aO = _mm_unpackhi_epi8(cEaO, zero);  /* aO = (A1 A3 A5 A7 A9 Ab Ad Af) */

  bO = _mm_unpacklo_epi8(bOcO, zero);  /* bO = (B1 B3 B5 B7 B9 Bb Bd Bf) */
  cO = _mm_unpackhi_epi8(bOcO, zero);  /* cO = (C1 C3 C5 C7 C9 Cb Cd Cf) */

#else  /* RGB_PIXELSIZE == 4 */

  __m128i abcd0, abcd1, abcd2, abcd3, abcd04_15, abcd26_37, abcd8c_9d,
    abcdae_bf, abcd04, abcd15, abcd26, abcd37, cEdE, aObO, cOdO;
#if RGB_RED == EXT_XBGR_RED || RGB_RED == EXT_XRGB_RED
  __m128i dE, dO;
#endif

  /* NOTE: The values of RGB_RED, RGB_GREEN, and RGB_BLUE determine the mapping
   * of components A, B, C, and D to red, green, and blue.
   *
   * abcd0 = (A0 B0 C0 D0 A1 B1 C1 D1 A2 B2 C2 D2 A3 B3 C3 D3)
   * abcd1 = (A4 B4 C4 D4 A5 B5 C5 D5 A6 B6 C6 D6 A7 B7 C7 D7)
   * abcd2 = (A8 B8 C8 D8 A9 B9 C9 D9 Aa Ba Ca Da Ab Bb Cb Db)
   * abcd3 = (Ac Bc Cc Dc Ad Bd Cd Dd Ae Be Ce De Af Bf Cf Df)
   */
  abcd0 = _mm_loadu_si128((__m128i *)inptr);
  abcd1 = _mm_loadu_si128((__m128i *)inptr + 1);
  abcd2 = _mm_loadu_si128((__m128i *)inptr + 2);
  abcd3 = _mm_loadu_si128((__m128i *)inptr + 3);

  /* abcd04_15 = (A0 A4 B0 B4 C0 C4 D0 D4 A1 A5 B1 B5 C1 C5 D1 D5) */
  abcd04_15 = _mm_unpacklo_epi8(abcd0, abcd1);
  /* abcd26_37 = (A2 A6 B2 B6 C2 C6 D2 D6 A3 A7 B3 B7 C3 C7 D3 D7) */
  abcd26_37 = _mm_unpackhi_epi8(abcd0, abcd1);

  /* abcd8c_9d = (A8 Ac B8 Bc C8 Cc D8 Dc A9 Ad B9 Bd C9 Cd D9 Dd) */
  abcd8c_9d = _mm_unpacklo_epi8(abcd2, abcd3);
  /* abcdae_bf = (Aa Ae Ba Be Ca Ce Da De Ab Af Bb Bf Cb Cf Db Df) */
  abcdae_bf = _mm_unpackhi_epi8(abcd2, abcd3);

  /* abcd04 = (A0 A4 A8 Ac B0 B4 B8 Bc C0 C4 C8 Cc D0 D4 D8 Dc) */
  abcd04 = _mm_unpacklo_epi16(abcd04_15, abcd8c_9d);
  /* abcd15 = (A1 A5 A9 Ad B1 B5 B9 Bd C1 C5 C9 Cd D1 D5 D9 Dd) */
  abcd15 = _mm_unpackhi_epi16(abcd04_15, abcd8c_9d);

  /* abcd26 = (A2 A6 Aa Ae B2 B6 Ba Be C2 C6 Ca Ce D2 D6 Da De) */
  abcd26 = _mm_unpacklo_epi16(abcd26_37, abcdae_bf);
  /* abcd37 = (A3 A7 Ab Af B3 B7 Bb Bf C3 C7 Cb Cf D3 D7 Db Df) */
  abcd37 = _mm_unpackhi_epi16(abcd26_37, abcdae_bf);

  /* aEbE = (A0 A2 A4 A6 A8 Aa Ac Ae B0 B2 B4 B6 B8 Ba Bc Be) */
  aEbE = _mm_unpacklo_epi8(abcd04, abcd26);
  /* cEdE = (C0 C2 C4 C6 C8 Ca Cc Ce D0 D2 D4 D6 D8 Da Dc De) */
  cEdE = _mm_unpackhi_epi8(abcd04, abcd26);

  /* aObO = (A1 A3 A5 A7 A9 Ab Ad Af B1 B3 B5 B7 B9 Bb Bd Bf) */
  aObO = _mm_unpacklo_epi8(abcd15, abcd37);
  /* cOdO = (C1 C3 C5 C7 C9 Cb Cd Cf D1 D3 D5 D7 D9 Db Dd Df) */
  cOdO = _mm_unpackhi_epi8(abcd15, abcd37);

  zero = _mm_setzero_si128();

#if RGB_RED != EXT_XBGR_RED && RGB_RED != EXT_XRGB_RED
  aE = _mm_unpacklo_epi8(aEbE, zero);  /* aE = (A0 A2 A4 A6 A8 Aa Ac Ae) */
#endif
  bE = _mm_unpackhi_epi8(aEbE, zero);  /* bE = (B0 B2 B4 B6 B8 Ba Bc Be) */

#if RGB_RED != EXT_XBGR_RED && RGB_RED != EXT_XRGB_RED
  aO = _mm_unpacklo_epi8(aObO, zero);  /* aO = (A1 A3 A5 A7 A9 Ab Ad Af) */
#endif
  bO = _mm_unpackhi_epi8(aObO, zero);  /* bO = (B1 B3 B5 B7 B9 Bb Bd Bf) */

  cE = _mm_unpacklo_epi8(cEdE, zero);  /* cE = (C0 C2 C4 C6 C8 Ca Cc Ce) */
#if RGB_RED == EXT_XBGR_RED || RGB_RED == EXT_XRGB_RED
  dE = _mm_unpackhi_epi8(cEdE, zero);  /* dE = (D0 D2 D4 D6 D8 Da Dc De) */
#endif

  cO = _mm_unpacklo_epi8(zero, cOdO);
#if RGB_RED == EXT_XBGR_RED || RGB_RED == EXT_XRGB_RED
  dO = _mm_unpackhi_epi8(cOdO, cOdO);
#endif
  cO = _mm_srli_epi16(cO, 8);          /* cO = (C1 C3 C5 C7 C9 Cb Cd Cf) */
#if RGB_RED == EXT_XBGR_RED || RGB_RED == EXT_XRGB_RED
  dO = _mm_srli_epi16(dO, 8);          /* dO = (D1 D3 D5 D7 D9 Db Dd Df) */
#endif

#endif  /* RGB_PIXELSIZE */

  /* RE = (R0 R2 R4 R6 R8 Ra Rc Re)
   * GE = (G0 G2 G4 G6 G8 Ga Gc Ge)
   * BE = (B0 B2 B4 B6 B8 Ba Bc Be)
   * RO = (R1 R3 R5 R7 R9 Rb Rd Rf)
   * GO = (G1 G3 G5 G7 G9 Gb Gd Gf)
   * BO = (B1 B3 B5 B7 B9 Bb Bd Bf)
   *
   * (Original)
   * Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
   * Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B + CENTERJSAMPLE
   * Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B + CENTERJSAMPLE
   *
   * (This implementation)
   * Y  =  0.29900 * R + 0.33700 * G + 0.11400 * B + 0.25000 * G
   * Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B + CENTERJSAMPLE
   * Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B + CENTERJSAMPLE
   */

  rol_gol = _mm_unpacklo_epi16(RO, GO);
  roh_goh = _mm_unpackhi_epi16(RO, GO);
  /* rol_gol_F0299_F0337 = ROL * FIX(0.299) + GOL * FIX(0.337) */
  rol_gol_F0299_F0337 = _mm_madd_epi16(rol_gol, PW_F0299_F0337);
  /* roh_goh_F0299_F0337 = ROH * FIX(0.299) + GOH * FIX(0.337) */
  roh_goh_F0299_F0337 = _mm_madd_epi16(roh_goh, PW_F0299_F0337);
  /* cbol = ROL * -FIX(0.168) + GOL * -FIX(0.331) */
  cbol = _mm_madd_epi16(rol_gol, PW_MF016_MF033);
  /* cboh = ROH * -FIX(0.168) + GOH * -FIX(0.331) */
  cboh = _mm_madd_epi16(roh_goh, PW_MF016_MF033);

  zero = _mm_setzero_si128();
  bol = _mm_unpacklo_epi16(zero, BO);
  boh = _mm_unpackhi_epi16(zero, BO);
  bol = _mm_srli_epi32(bol, 1);        /* BOL = BOL * FIX(0.500) */
  boh = _mm_srli_epi32(boh, 1);        /* BOH = BOH * FIX(0.500) */

  cbol = _mm_add_epi32(cbol, bol);
  cboh = _mm_add_epi32(cboh, boh);
  cbol = _mm_add_epi32(cbol, PD_ONEHALFM1_CJ);
  cboh = _mm_add_epi32(cboh, PD_ONEHALFM1_CJ);
  cbol = _mm_srli_epi32(cbol, SCALEBITS);
  cboh = _mm_srli_epi32(cboh, SCALEBITS);
  cbo = _mm_packs_epi32(cbol, cboh);

  rel_gel = _mm_unpacklo_epi16(RE, GE);
  reh_geh = _mm_unpackhi_epi16(RE, GE);
  /* rel_gel_F0299_F0337 = REL * FIX(0.299) + GEL * FIX(0.337) */
  rel_gel_F0299_F0337 = _mm_madd_epi16(rel_gel, PW_F0299_F0337);
  /* reh_geh_F0299_F0337 = REH * FIX(0.299) + GEH * FIX(0.337) */
  reh_geh_F0299_F0337 = _mm_madd_epi16(reh_geh, PW_F0299_F0337);
  /* cbel = REL * -FIX(0.168) + GEL * -FIX(0.331) */
  cbel = _mm_madd_epi16(rel_gel, PW_MF016_MF033);
  /* cbeh = REH * -FIX(0.168) + GEH * -FIX(0.331) */
  cbeh = _mm_madd_epi16(reh_geh, PW_MF016_MF033);

  zero = _mm_setzero_si128();
  bel = _mm_unpacklo_epi16(zero, BE);
  beh = _mm_unpackhi_epi16(zero, BE);
  bel = _mm_srli_epi32(bel, 1);        /* BEL = BEL * FIX(0.500) */
  beh = _mm_srli_epi32(beh, 1);        /* BEH = BEH * FIX(0.500) */

  cbel = _mm_add_epi32(cbel, bel);
  cbeh = _mm_add_epi32(cbeh, beh);
  cbel = _mm_add_epi32(cbel, PD_ONEHALFM1_CJ);
  cbeh = _mm_add_epi32(cbeh, PD_ONEHALFM1_CJ);
  cbel = _mm_srli_epi32(cbel, SCALEBITS);
  cbeh = _mm_srli_epi32(cbeh, SCALEBITS);
  cbe = _mm_packs_epi32(cbel, cbeh);

  cbo = _mm_slli_epi16(cbo, 8);
  cb = _mm_xor_si128(cbe, cbo);
  _mm_store_si128((__m128i *)outptr1, cb);

  bol_gol = _mm_unpacklo_epi16(BO, GO);
  boh_goh = _mm_unpackhi_epi16(BO, GO);
  /* yol = BOL * FIX(0.114) + GOL * FIX(0.250) */
  yol = _mm_madd_epi16(bol_gol, PW_F0114_F0250);
  /* yoh = BOH * FIX(0.114) + GOH * FIX(0.250) */
  yoh = _mm_madd_epi16(boh_goh, PW_F0114_F0250);
  /* crol = BOL * -FIX(0.081) + GOL * -FIX(0.418) */
  crol = _mm_madd_epi16(bol_gol, PW_MF008_MF041);
  /* croh = BOH * -FIX(0.081) + GOH * -FIX(0.418) */
  croh = _mm_madd_epi16(boh_goh, PW_MF008_MF041);

  yol = _mm_add_epi32(yol, rol_gol_F0299_F0337);
  yoh = _mm_add_epi32(yoh, roh_goh_F0299_F0337);
  yol = _mm_add_epi32(yol, PD_ONEHALF);
  yoh = _mm_add_epi32(yoh, PD_ONEHALF);
  yol = _mm_srli_epi32(yol, SCALEBITS);
  yoh = _mm_srli_epi32(yoh, SCALEBITS);
  yo = _mm_packs_epi32(yol, yoh);

  zero = _mm_setzero_si128();
  rol = _mm_unpacklo_epi16(zero, RO);
  roh = _mm_unpackhi_epi16(zero, RO);
  rol = _mm_srli_epi32(rol, 1);        /* ROL = ROL * FIX(0.500) */
  roh = _mm_srli_epi32(roh, 1);        /* ROH = ROH * FIX(0.500) */

  crol = _mm_add_epi32(crol, rol);
  croh = _mm_add_epi32(croh, roh);
  crol = _mm_add_epi32(crol, PD_ONEHALFM1_CJ);
  croh = _mm_add_epi32(croh, PD_ONEHALFM1_CJ);
  crol = _mm_srli_epi32(crol, SCALEBITS);
  croh = _mm_srli_epi32(croh, SCALEBITS);
  cro = _mm_packs_epi32(crol, croh);

  bel_gel = _mm_unpacklo_epi16(BE, GE);
  beh_geh = _mm_unpackhi_epi16(BE, GE);
  /* yel = BEL * FIX(0.114) + GEL * FIX(0.250) */
  yel = _mm_madd_epi16(bel_gel, PW_F0114_F0250);
  /* yeh = BEH * FIX(0.114) + GEH * FIX(0.250) */
  yeh = _mm_madd_epi16(beh_geh, PW_F0114_F0250);
  /* crel = BEL * -FIX(0.081) + GEL * -FIX(0.418) */
  crel = _mm_madd_epi16(bel_gel, PW_MF008_MF041);
  /* creh = BEH * -FIX(0.081) + GEH * -FIX(0.418) */
  creh = _mm_madd_epi16(beh_geh, PW_MF008_MF041);

  yel = _mm_add_epi32(yel, rel_gel_F0299_F0337);
  yeh = _mm_add_epi32(yeh, reh_geh_F0299_F0337);
  yel = _mm_add_epi32(yel, PD_ONEHALF);
  yeh = _mm_add_epi32(yeh, PD_ONEHALF);
  yel = _mm_srli_epi32(yel, SCALEBITS);
  yeh = _mm_srli_epi32(yeh, SCALEBITS);
  ye = _mm_packs_epi32(yel, yeh);

  yo = _mm_slli_epi16(yo, 8);
  y = _mm_xor_si128(ye, yo);
  _mm_store_si128((__m128i *)outptr0, y);

  zero = _mm_setzero_si128();
  rel = _mm_unpacklo_epi16(zero, RE);
  reh = _mm_unpackhi_epi16(zero, RE);
  rel = _mm_srli_epi32(rel, 1);        /* REL = REL * FIX(0.500) */
  reh = _mm_srli_epi32(reh, 1);        /* REH = REH * FIX(0.500) */

  crel = _mm_add_epi32(crel, rel);
  creh = _mm_add_epi32(creh, reh);
  crel = _mm_add_epi32(crel, PD_ONEHALFM1_CJ);
  creh = _mm_add_epi32(creh, PD_ONEHALFM1_CJ);
  crel = _mm_srli_epi32(crel, SCALEBITS);
  creh = _mm_srli_epi32(creh, SCALEBITS);
  cre = _mm_packs_epi32(crel, creh);

  cro = _mm_slli_epi16(cro, 8);
  cr = _mm_xor_si128(cre, cro);
  _mm_store_si128((__m128i *)outptr2, cr);
}


HIDDEN void
jsimd_rgb_ycc_convert_sse2(JDIMENSION image_width, JSAMPARRAY input_buf,
                           JSAMPIMAGE output_buf, JDIMENSION output_row,
                           int num_rows)
{
  /* Pointer to RGB(X/A) input data */
  JSAMPROW inptr;
  /* Pointers to Y, Cb, and Cr output data */
  JSAMPROW outptr0, outptr1, outptr2;

  while (--num_rows >= 0) {
    int cols_remaining = image_width;

    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;

    for (; cols_remaining >= 16; cols_remaining -= 16,
         inptr += RGB_PIXELSIZE * 16,
         outptr0 += 16, outptr1 += 16, outptr2 += 16)
      jsimd_rgb_ycc_convert_cols(inptr, outptr0, outptr1, outptr2);

    if (cols_remaining) {
      ALIGN(16) unsigned char inbuf[RGB_PIXELSIZE * 16];
      ALIGN(16) unsigned char outbuf0[16], outbuf1[16], outbuf2[16];

      memset(inbuf, 0, RGB_PIXELSIZE * 16);
      memcpy(inbuf, inptr, cols_remaining * RGB_PIXELSIZE);
      jsimd_rgb_ycc_convert_cols(inbuf, outbuf0, outbuf1, outbuf2);
      memcpy(outptr0, outbuf0, cols_remaining);
      memcpy(outptr1, outbuf1, cols_remaining);
      memcpy(outptr2, outbuf2, cols_remaining);
    }
  }
}
