/*
 * Loongson MMI optimizations for libjpeg-turbo
 *
 * Copyright (C) 2018, Loongson Technology Corporation Limited, BeiJing.
 *                     All Rights Reserved.
 * Authors:  ZhangLixia  <zhanglixia-hf@loongson.cn>
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

/* This file is included by jdmerge-mmi.c */


#if RGB_RED == 0
#define mmA  mm0
#define mmB  mm1
#elif RGB_GREEN == 0
#define mmA  mm2
#define mmB  mm3
#elif RGB_BLUE == 0
#define mmA  mm4
#define mmB  mm5
#else
#define mmA  mm6
#define mmB  mm7
#endif

#if RGB_RED == 1
#define mmC  mm0
#define mmD  mm1
#elif RGB_GREEN == 1
#define mmC  mm2
#define mmD  mm3
#elif RGB_BLUE == 1
#define mmC  mm4
#define mmD  mm5
#else
#define mmC  mm6
#define mmD  mm7
#endif

#if RGB_RED == 2
#define mmE  mm0
#define mmF  mm1
#elif RGB_GREEN == 2
#define mmE  mm2
#define mmF  mm3
#elif RGB_BLUE == 2
#define mmE  mm4
#define mmF  mm5
#else
#define mmE  mm6
#define mmF  mm7
#endif

#if RGB_RED == 3
#define mmG  mm0
#define mmH  mm1
#elif RGB_GREEN == 3
#define mmG  mm2
#define mmH  mm3
#elif RGB_BLUE == 3
#define mmG  mm4
#define mmH  mm5
#else
#define mmG  mm6
#define mmH  mm7
#endif

void jsimd_h2v1_merged_upsample_mmi(JDIMENSION output_width, JSAMPIMAGE input_buf,
                                    JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  JSAMPROW outptr, inptr0, inptr1, inptr2;
  int num_cols, col;
  __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
  __m64 mm8, mm9, wk[2];

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];

  for (num_cols = output_width >> 1; num_cols > 0; num_cols -= 8,
       inptr0 += 16, inptr1 += 8, inptr2 += 8) {

    mm5 = _mm_load_si64((__m64 *)inptr1);
    mm1 = _mm_load_si64((__m64 *)inptr2);
    mm8 = _mm_load_si64((__m64 *)inptr0);
    mm9 = _mm_load_si64((__m64 *)inptr0 + 1);
    mm4 = 0;
    mm7 = 0;
    mm3 = _mm_xor_si64(mm4, mm4);
    mm7 = _mm_cmpeq_pi16(mm7, mm7);
    mm7 = _mm_slli_pi16(mm7, 7);      /* mm7={0xFF80 0xFF80 0xFF80 0xFF80} */

    mm4 = _mm_unpacklo_pi8(mm5, mm3);       /* mm4=Cb(0123)=CbL */
    mm5 = _mm_unpackhi_pi8(mm5, mm3);       /* mm5=Cb(4567)=CbH */
    mm0 = _mm_unpacklo_pi8(mm1, mm3);       /* mm0=Cr(0123)=CrL */
    mm1 = _mm_unpackhi_pi8(mm1, mm3);       /* mm1=Cr(4567)=CrH */
    mm4 = _mm_add_pi16(mm4, mm7);
    mm5 = _mm_add_pi16(mm5, mm7);
    mm0 = _mm_add_pi16(mm0, mm7);
    mm1 = _mm_add_pi16(mm1, mm7);

    /* (Original)
     * R = Y                + 1.40200 * Cr
     * G = Y - 0.34414 * Cb - 0.71414 * Cr
     * B = Y + 1.77200 * Cb
     *
     * (This implementation)
     * R = Y                + 0.40200 * Cr + Cr
     * G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr
     * B = Y - 0.22800 * Cb + Cb + Cb
     */

    mm2 = mm4;                              /* mm2 = CbL */
    mm3 = mm5;                              /* mm3 = CbH */
    mm4 = _mm_add_pi16(mm4, mm4);           /* mm4 = 2*CbL */
    mm5 = _mm_add_pi16(mm5, mm5);           /* mm5 = 2*CbH */
    mm6 = mm0;                              /* mm6 = CrL */
    mm7 = mm1;                              /* mm7 = CrH */
    mm0 = _mm_add_pi16(mm0, mm0);           /* mm0 = 2*CrL */
    mm1 = _mm_add_pi16(mm1, mm1);           /* mm1 = 2*CrH */

    mm4 = _mm_mulhi_pi16(mm4, PW_MF0228);   /* mm4=(2*CbL * -FIX(0.22800) */
    mm5 = _mm_mulhi_pi16(mm5, PW_MF0228);   /* mm5=(2*CbH * -FIX(0.22800) */
    mm0 = _mm_mulhi_pi16(mm0, PW_F0402);    /* mm0=(2*CrL * FIX(0.40200)) */
    mm1 = _mm_mulhi_pi16(mm1, PW_F0402);    /* mm1=(2*CrH * FIX(0.40200)) */

    mm4 = _mm_add_pi16(mm4, PW_ONE);
    mm5 = _mm_add_pi16(mm5, PW_ONE);
    mm4 = _mm_srai_pi16(mm4, 1);            /* mm4=(CbL * -FIX(0.22800)) */
    mm5 = _mm_srai_pi16(mm5, 1);            /* mm5=(CbH * -FIX(0.22800)) */
    mm0 = _mm_add_pi16(mm0, PW_ONE);
    mm1 = _mm_add_pi16(mm1, PW_ONE);
    mm0 = _mm_srai_pi16(mm0, 1);            /* mm0=(CrL * FIX(0.40200)) */
    mm1 = _mm_srai_pi16(mm1, 1);            /* mm1=(CrH * FIX(0.40200)) */

    mm4 = _mm_add_pi16(mm4, mm2);
    mm5 = _mm_add_pi16(mm5, mm3);
    mm4 = _mm_add_pi16(mm4, mm2);       /* mm4=(CbL * FIX(1.77200))=(B-Y)L = (cblue)L */
    mm5 = _mm_add_pi16(mm5, mm3);       /* mm5=(CbH * FIX(1.77200))=(B-Y)H = (cblue)H*/
    mm0 = _mm_add_pi16(mm0, mm6);       /* mm0=(CrL * FIX(1.40200))=(R-Y)L = (cred)L*/
    mm1 = _mm_add_pi16(mm1, mm7);       /* mm1=(CrH * FIX(1.40200))=(R-Y)H = (cred)H*/

    wk[0] = mm4;                            /* wk(0)=(cblue)L */
    wk[1] = mm5;                            /* wk(1)=(cblue)H */

    mm4 = mm2;
    mm5 = mm3;
    mm2 = _mm_unpacklo_pi16(mm2, mm6);
    mm4 = _mm_unpackhi_pi16(mm4, mm6);
    mm2 = _mm_madd_pi16(mm2, PW_MF0344_F0285);
    mm4 = _mm_madd_pi16(mm4, PW_MF0344_F0285);
    mm3 = _mm_unpacklo_pi16(mm3, mm7);
    mm5 = _mm_unpackhi_pi16(mm5, mm7);
    mm3 = _mm_madd_pi16(mm3, PW_MF0344_F0285);
    mm5 = _mm_madd_pi16(mm5, PW_MF0344_F0285);

    mm2 = _mm_add_pi32(mm2, PD_ONEHALF);
    mm4 = _mm_add_pi32(mm4, PD_ONEHALF);
    mm2 = _mm_srai_pi32(mm2, SCALEBITS);
    mm4 = _mm_srai_pi32(mm4, SCALEBITS);
    mm3 = _mm_add_pi32(mm3, PD_ONEHALF);
    mm5 = _mm_add_pi32(mm5, PD_ONEHALF);
    mm3 = _mm_srai_pi32(mm3, SCALEBITS);
    mm5 = _mm_srai_pi32(mm5, SCALEBITS);

    mm2 = _mm_packs_pi32(mm2, mm4);  /* mm2=CbL*-FIX(0.344)+CrL*FIX(0.285) */
    mm3 = _mm_packs_pi32(mm3, mm5);  /* mm3=CbH*-FIX(0.344)+CrH*FIX(0.285) */
    mm2 = _mm_sub_pi16(mm2, mm6);  /* mm2=CbL*-FIX(0.344)+CrL*-FIX(0.714)=(G-Y)L=(cgreen)L */
    mm3 = _mm_sub_pi16(mm3, mm7);  /* mm3=CbH*-FIX(0.344)+CrH*-FIX(0.714)=(G-Y)H=(cgreen)H */

    mm5 = mm8;                              /* mm5=Y(01234567) */
    mm7 = mm9;

    mm6 = _mm_cmpeq_pi16(mm4, mm4);
    mm6 = _mm_srli_pi16(mm6, BYTE_BIT);    /* mm6={0xFF 0x00 0xFF 0x00 ..} */
    mm4 = _mm_and_si64(mm6, mm5);          /* mm4=Y(0246)=YE */
    mm5 = _mm_srli_pi16(mm5, BYTE_BIT);    /* mm5=Y(1357)=YO */
    mm6 = _mm_and_si64(mm6, mm7);          /* mm6=Y(8ACE)=YEH */
    mm7 = _mm_srli_pi16(mm7, BYTE_BIT);    /* mm7=Y(9BDF)=YOH */

    mm8 = _mm_add_pi16(mm0, mm4);      /* mm8=((cred)L+YE)=RE=(R0 R2 R4 R6) */
    mm9 = _mm_add_pi16(mm0, mm5);      /* mm9=((cred)L+YO)=RO=(R1 R3 R5 R7) */
    mm0 = _mm_add_pi16(mm1, mm6);      /* mm0=((cred)H+YEH)=REH=(R8 RA RC RE) */
    mm1 = _mm_add_pi16(mm1, mm7);      /* mm1=((cred)H+YOH)=ROH=(R9 RB RD RF) */
    mm0 = _mm_packs_pu16(mm8, mm0);    /* mm0=(R0 R2 R4 R6 R8 RA RC RE) */
    mm1 = _mm_packs_pu16(mm9, mm1);    /* mm1=(R1 R3 R5 R7 R9 RB RD RF) */

    mm8 = _mm_add_pi16(mm2, mm4);      /* mm8=((cgreen)L+YE)=GE=(G0 G2 G4 G6) */
    mm9 = _mm_add_pi16(mm2, mm5);      /* mm9=((cgreen)L+YO)=GO=(G1 G3 G5 G7) */
    mm2 = _mm_add_pi16(mm3, mm6);      /* mm2=((cgreen)H+YEH)=GEH=(G8 GA GC GE) */
    mm3 = _mm_add_pi16(mm3, mm7);      /* mm3=((cgreen)H+YOH)=GOH=(G9 GB GD GF) */
    mm2 = _mm_packs_pu16(mm8, mm2);    /* mm2=(G0 G2 G4 G6 G8 GA GC GE) */
    mm3 = _mm_packs_pu16(mm9, mm3);    /* mm3=(G1 G3 G5 G7 G9 GB GD GF) */

    mm4 = _mm_add_pi16(mm4, wk[0]);    /* mm4=(YE+(cblue)L)=BE=(B0 B2 B4 B6) */
    mm5 = _mm_add_pi16(mm5, wk[0]);    /* mm5=(YO+(cblue)L)=BO=(B1 B3 B5 B7) */
    mm8 = _mm_add_pi16(mm6, wk[1]);    /* mm8=(YEH+(cblue)H)=BEH=(B8 BA BC BE) */
    mm9 = _mm_add_pi16(mm7, wk[1]);    /* mm9=(YOH+(cblue)H)=BOH=(B9 BB BD BF) */
    mm4 = _mm_packs_pu16(mm4, mm8);    /* mm4=(B0 B2 B4 B6 B8 BA BC BE) */
    mm5 = _mm_packs_pu16(mm5, mm9);    /* mm5=(B1 B3 B5 B7 B9 BB BD BF) */

#if RGB_PIXELSIZE == 3

    /* mmA=(00 02 04 06 08 0A 0C 0E), mmB=(01 03 05 07 09 0B 0D 0F) */
    /* mmC=(10 12 14 16 18 1A 1C 1E), mmD=(11 13 15 17 19 1B 1D 1F) */
    /* mmE=(20 22 24 26 28 2A 2C 2E), mmF=(21 23 25 27 29 2B 2D 2F) */
    mmG = _mm_unpacklo_pi8(mmA, mmC);     /* mmG=(00 10 02 12 04 14 06 16) */
    mmA = _mm_unpackhi_pi8(mmA, mmC);     /* mmA=(08 18 0A 1A 0C 1C 0E 1E) */
    mmH = _mm_unpacklo_pi8(mmE, mmB);     /* mmH=(20 01 22 03 24 05 26 07) */
    mmE = _mm_unpackhi_pi8(mmE, mmB);     /* mmE=(28 09 2A 0B 2C 0D 2E 0F) */
    mmC = _mm_unpacklo_pi8(mmD, mmF);     /* mmC=(11 21 13 23 15 25 17 27) */
    mmD = _mm_unpackhi_pi8(mmD, mmF);     /* mmD=(19 29 1B 2B 1D 2D 1F 2F) */

    mmB = _mm_unpacklo_pi16(mmG, mmA);     /* mmB=(00 10 08 18 02 12 0A 1A)*/
    mmA = _mm_unpackhi_pi16(mmG, mmA);     /* mmA=(04 14 0C 1C 06 16 0E 1E)*/
    mmF = _mm_unpacklo_pi16(mmH, mmE);     /* mmF=(20 01 28 09 22 03 2A 0B)*/
    mmE = _mm_unpackhi_pi16(mmH, mmE);     /* mmE=(24 05 2C 0D 26 07 2E 0F)*/
    mmH = _mm_unpacklo_pi16(mmC, mmD);     /* mmH=(11 21 19 29 13 23 1B 2B)*/
    mmG = _mm_unpackhi_pi16(mmC, mmD);     /* mmG=(15 25 1D 2D 17 27 1F 2F)*/

    mmC = _mm_unpacklo_pi16(mmB, mmF);     /* mmC=(00 10 20 01 08 18 28 09)*/
    mmB = _mm_srli_si64(mmB, 4 * BYTE_BIT);
    mmB = _mm_unpacklo_pi16(mmH, mmB);     /* mmB=(11 21 02 12 19 29 0A 1A)*/
    mmD = _mm_unpackhi_pi16(mmF, mmH);     /* mmD=(22 03 13 23 2A 0B 1B 2B)*/
    mmF = _mm_unpacklo_pi16(mmA, mmE);     /* mmF=(04 14 24 05 0C 1C 2C 0D)*/
    mmA = _mm_srli_si64(mmA, 4 * BYTE_BIT);
    mmH = _mm_unpacklo_pi16(mmG, mmA);     /* mmH=(15 25 06 16 1D 2D 0E 1E)*/
    mmG = _mm_unpackhi_pi16(mmE, mmG);     /* mmG=(26 07 17 27 2E 0F 1F 2F)*/

    mmA = _mm_unpacklo_pi32(mmC, mmB);     /* mmA=(00 10 20 01 11 21 02 12)*/
    mmE = _mm_unpackhi_pi32(mmC, mmB);     /* mmE=(08 18 28 09 19 29 0A 1A)*/
    mmB = _mm_unpacklo_pi32(mmD, mmF);     /* mmB=(22 03 13 23 04 14 24 05)*/
    mmF = _mm_unpackhi_pi32(mmD, mmF);     /* mmF=(2A 0B 1B 2B 0C 1C 2C 0D)*/
    mmC = _mm_unpacklo_pi32(mmH, mmG);     /* mmC=(15 25 06 16 26 07 17 27)*/
    mmG = _mm_unpackhi_pi32(mmH, mmG);     /* mmG=(1D 2D 0E 1E 2E 0F 1F 2F)*/

    if (num_cols >= 8) {
      if (!(((long)outptr) & 7)) {
        _mm_store_si64((__m64 *)outptr, mmA);
        _mm_store_si64((__m64 *)(outptr + 8), mmB);
        _mm_store_si64((__m64 *)(outptr + 16), mmC);
        _mm_store_si64((__m64 *)(outptr + 24), mmE);
        _mm_store_si64((__m64 *)(outptr + 32), mmF);
        _mm_store_si64((__m64 *)(outptr + 40), mmG);
      } else {
        _mm_storeu_si64((__m64 *)outptr, mmA);
        _mm_storeu_si64((__m64 *)(outptr + 8), mmB);
        _mm_storeu_si64((__m64 *)(outptr + 16), mmC);
        _mm_storeu_si64((__m64 *)(outptr + 24), mmE);
        _mm_storeu_si64((__m64 *)(outptr + 32), mmF);
        _mm_storeu_si64((__m64 *)(outptr + 40), mmG);
      }
      outptr += RGB_PIXELSIZE * 16;
    } else {
      if(output_width & 1)
        col = num_cols * 6 + 3;
      else
        col = num_cols * 6;

      asm(".set noreorder\r\n"             /* st24 */

          "li       $8, 24\r\n"
          "move     $9, %7\r\n"
          "mov.s    $f4, %1\r\n"
          "mov.s    $f6, %2\r\n"
          "mov.s    $f8, %3\r\n"
          "move     $10, %8\r\n"
          "bltu     $9, $8, 1f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f6, 7+8($10)\r\n"
          "gssdrc1  $f6, 8($10)\r\n"
          "gssdlc1  $f8, 7+16($10)\r\n"
          "gssdrc1  $f8, 16($10)\r\n"
          "mov.s    $f4, %4\r\n"
          "mov.s    $f6, %5\r\n"
          "mov.s    $f8, %6\r\n"
          "subu     $9, $9, 24\r\n"
          PTR_ADDU  "$10, $10, 24\r\n"

          "1:       \r\n"
          "li       $8, 16\r\n"               /* st16 */
          "bltu     $9, $8, 2f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f6, 7+8($10)\r\n"
          "gssdrc1  $f6, 8($10)\r\n"
          "mov.s    $f4, $f8\r\n"
          "subu     $9, $9, 16\r\n"
          PTR_ADDU  "$10, $10, 16\r\n"

          "2:       \r\n"
          "li       $8,  8\r\n"               /* st8 */
          "bltu     $9, $8, 3f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "mov.s    $f4, $f6\r\n"
          "subu     $9, $9, 8\r\n"
          PTR_ADDU  "$10, $10, 8\r\n"

          "3:       \r\n"
          "li       $8,  4\r\n"               /* st4 */
          "mfc1     $11, $f4\r\n"
          "bltu     $9, $8, 4f\r\n"
          "nop      \r\n"
          "swl      $11, 3($10)\r\n"
          "swr      $11, 0($10)\r\n"
          "li       $8, 32\r\n"
          "mtc1     $8, $f6\r\n"
          "dsrl     $f4, $f4, $f6\r\n"
          "mfc1     $11, $f4\r\n"
          "subu     $9, $9, 4\r\n"
          PTR_ADDU  "$10, $10, 4\r\n"

          "4:       \r\n"
          "li       $8, 2\r\n"               /* st2 */
          "bltu     $9, $8, 5f\r\n"
          "nop      \r\n"
          "ush      $11, 0($10)\r\n"
          "srl      $11, 16\r\n"
          "subu     $9, $9, 2\r\n"
          PTR_ADDU  "$10, $10, 2\r\n"

          "5:       \r\n"
          "li       $8, 1\r\n"               /* st1 */
          "bltu     $9, $8, 6f\r\n"
          "nop      \r\n"
          "sb       $11, 0($10)\r\n"

          "6:       \r\n"
          "nop      \r\n"                    /* end */
          : "=m" (*outptr)
          : "f" (mmA), "f" (mmB), "f" (mmC), "f" (mmE), "f" (mmF),
            "f" (mmG), "r" (col), "r" (outptr)
          : "$f4", "$f6", "$f8", "$8", "$9", "$10", "$11", "memory"
         );
    }

#else  /* RGB_PIXELSIZE == 4 */

#ifdef RGBX_FILLER_0XFF
    mm6 = _mm_cmpeq_pi8(mm6, mm6);
    mm7 = _mm_cmpeq_pi8(mm7, mm7);
#else
    mm6 = _mm_xor_si64(mm6, mm6);
    mm7 = _mm_xor_si64(mm7, mm7);
#endif
    /* mmA=(00 02 04 06 08 0A 0C 0E), mmB=(01 03 05 07 09 0B 0D 0F) */
    /* mmC=(10 12 14 16 18 1A 1C 1E), mmD=(11 13 15 17 19 1B 1D 1F) */
    /* mmE=(20 22 24 26 28 2A 2C 2E), mmF=(21 23 25 27 29 2B 2D 2F) */
    /* mmG=(30 32 34 36 38 3A 3C 3E), mmH=(31 33 35 37 39 3B 3D 3F) */

    mm8 = _mm_unpacklo_pi8(mmA, mmC);     /* mm8=(00 10 02 12 04 14 06 16) */
    mm9 = _mm_unpackhi_pi8(mmA, mmC);     /* mm9=(08 18 0A 1A 0C 1C 0E 1E) */
    mmA = _mm_unpacklo_pi8(mmE, mmG);     /* mmA=(20 30 22 32 24 34 26 36) */
    mmE = _mm_unpackhi_pi8(mmE, mmG);     /* mmE=(28 38 2A 3A 2C 3C 2E 3E) */
 
    mmG = _mm_unpacklo_pi8(mmB, mmD);     /* mmG=(01 11 03 13 05 15 07 17) */
    mmB = _mm_unpackhi_pi8(mmB, mmD);     /* mmB=(09 19 0B 1B 0D 1D 0F 1F) */
    mmD = _mm_unpacklo_pi8(mmF, mmH);     /* mmD=(21 31 23 33 25 35 27 37) */
    mmF = _mm_unpackhi_pi8(mmF, mmH);     /* mmF=(29 39 2B 3B 2D 3D 2F 3F) */

    mmH = _mm_unpacklo_pi16(mm8, mmA);    /* mmH=(00 10 20 30 02 12 22 32) */
    mm8 = _mm_unpackhi_pi16(mm8, mmA);    /* mm8=(04 14 24 34 06 16 26 36) */
    mmA = _mm_unpacklo_pi16(mmG, mmD);    /* mmA=(01 11 21 31 03 13 23 33) */
    mmD = _mm_unpackhi_pi16(mmG, mmD);    /* mmD=(05 15 25 35 07 17 27 37) */

    mmG = _mm_unpackhi_pi16(mm9, mmE);    /* mmG=(0C 1C 2C 3C 0E 1E 2E 3E) */
    mm9 = _mm_unpacklo_pi16(mm9, mmE);    /* mm9=(08 18 28 38 0A 1A 2A 3A) */
    mmE = _mm_unpacklo_pi16(mmB, mmF);    /* mmE=(09 19 29 39 0B 1B 2B 3B) */
    mmF = _mm_unpackhi_pi16(mmB, mmF);    /* mmF=(0D 1D 2D 3D 0F 1F 2F 3F) */

    mmB = _mm_unpackhi_pi32(mmH, mmA);    /* mmB=(02 12 22 32 03 13 23 33) */
    mmA = _mm_unpacklo_pi32(mmH, mmA);    /* mmA=(00 10 20 30 01 11 21 31) */
    mmC = _mm_unpacklo_pi32(mm8, mmD);    /* mmC=(04 14 24 34 05 15 25 35) */
    mmD = _mm_unpackhi_pi32(mm8, mmD);    /* mmD=(06 16 26 36 07 17 27 37) */

    mmH = _mm_unpackhi_pi32(mmG, mmF);    /* mmH=(0E 1E 2E 3E 0F 1F 2F 3F) */
    mmG = _mm_unpacklo_pi32(mmG, mmF);    /* mmG=(0C 1C 2C 3C 0D 1D 2D 3D) */
    mmF = _mm_unpackhi_pi32(mm9, mmE);    /* mmF=(0A 1A 2A 3A 0B 1B 2B 3B) */
    mmE = _mm_unpacklo_pi32(mm9, mmE);    /* mmE=(08 18 28 38 09 19 29 39) */

    if (num_cols >= 8) {
      if (!(((long)outptr) & 7)) {
        _mm_store_si64((__m64 *)outptr, mmA);
        _mm_store_si64((__m64 *)(outptr + 8), mmB);
        _mm_store_si64((__m64 *)(outptr + 16), mmC);
        _mm_store_si64((__m64 *)(outptr + 24), mmD);
        _mm_store_si64((__m64 *)(outptr + 32), mmE);
        _mm_store_si64((__m64 *)(outptr + 40), mmF);
        _mm_store_si64((__m64 *)(outptr + 48), mmG);
        _mm_store_si64((__m64 *)(outptr + 56), mmH);
      } else {
        _mm_storeu_si64((__m64 *)outptr, mmA);
        _mm_storeu_si64((__m64 *)(outptr + 8), mmB);
        _mm_storeu_si64((__m64 *)(outptr + 16), mmC);
        _mm_storeu_si64((__m64 *)(outptr + 24), mmD);
        _mm_storeu_si64((__m64 *)(outptr + 32), mmE);
        _mm_storeu_si64((__m64 *)(outptr + 40), mmF);
        _mm_storeu_si64((__m64 *)(outptr + 48), mmG);
        _mm_storeu_si64((__m64 *)(outptr + 56), mmH);
      }
      outptr += RGB_PIXELSIZE * 16;
    } else {
      if(output_width & 1)
        col = num_cols * 2 + 1;
      else
        col = num_cols * 2;
      asm(".set noreorder\r\n"              /* st32 */

          "li       $8, 8\r\n"
          "move     $9, %10\r\n"
          "move     $10, %11\r\n"
          "mov.s    $f4, %2\r\n"
          "mov.s    $f6, %3\r\n"
          "mov.s    $f8, %4\r\n"
          "mov.s    $f10, %5\r\n"
          "bltu     $9, $8, 1f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f6, 7+8($10)\r\n"
          "gssdrc1  $f6, 8($10)\r\n"
          "gssdlc1  $f8, 7+16($10)\r\n"
          "gssdrc1  $f8, 16($10)\r\n"
          "gssdlc1  $f10, 7+24($10)\r\n"
          "gssdrc1  $f10, 24($10)\r\n"
          "mov.s    $f4, %6\r\n"
          "mov.s    $f6, %7\r\n"
          "mov.s    $f8, %8\r\n"
          "mov.s    $f10, %9\r\n"
          "subu     $9, $9, 8\r\n"
          PTR_ADDU  "$10, $10, 32\r\n"

          "1:       \r\n"
          "li       $8, 4\r\n"               /* st16 */
          "bltu     $9, $8, 2f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f6, 7+8($10)\r\n"
          "gssdrc1  $f6, 8($10)\r\n"
          "mov.s    $f4, $f8\r\n"
          "mov.s    $f6, $f10\r\n"
          "subu     $9, $9, 4\r\n"
          PTR_ADDU  "$10, $10, 16\r\n"

          "2:       \r\n"
          "li       $8, 2\r\n"               /* st8 */
          "bltu     $9, $8, 3f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "mov.s    $f4, $f6\r\n"
          "subu     $9, $9, 2\r\n"
          PTR_ADDU  "$10, $10, 8\r\n"

          "3:       \r\n"
          "li       $8, 1\r\n"               /* st4 */
          "bltu     $9, $8, 4f\r\n"
          "nop      \r\n"
          "gsswlc1  $f4, 3($10)\r\n"
          "gsswrc1  $f4, 0($10)\r\n"

          "4:       \r\n"
          "li       %1, 0\r\n"               /* end */
          : "=m" (*outptr), "=r" (col)
          : "f" (mmA), "f" (mmB), "f" (mmC), "f" (mmD), "f" (mmE), "f" (mmF),
            "f" (mmG), "f" (mmH), "r" (col), "r" (outptr)
          : "$f4", "$f6", "$f8", "$f10", "$8", "$9", "$10", "memory"
         );
    }

#endif

  }

  if(!((output_width >> 1) & 7)) {
    if(output_width & 1) {
      mm5 = _mm_load_si64((__m64 *)inptr1);
      mm1 = _mm_load_si64((__m64 *)inptr2);
      mm8 = _mm_load_si64((__m64 *)inptr0);
      mm4 = 0;
      mm7 = 0;
      mm3 = _mm_xor_si64(mm4, mm4);
      mm7 = _mm_cmpeq_pi16(mm7, mm7);
      mm7 = _mm_slli_pi16(mm7, 7);      /* mm7={0xFF80 0xFF80 0xFF80 0xFF80} */
      mm4 = _mm_unpacklo_pi8(mm5, mm3);       /* mm4=Cb(0123)=CbL */
      mm0 = _mm_unpacklo_pi8(mm1, mm3);       /* mm0=Cr(0123)=CrL */
      mm4 = _mm_add_pi16(mm4, mm7);
      mm0 = _mm_add_pi16(mm0, mm7);

      mm2 = mm4;                              /* mm2 = CbL */
      mm4 = _mm_add_pi16(mm4, mm4);           /* mm4 = 2*CbL */
      mm6 = mm0;                              /* mm6 = CrL */
      mm0 = _mm_add_pi16(mm0, mm0);           /* mm0 = 2*CrL */
      mm4 = _mm_mulhi_pi16(mm4, PW_MF0228);   /* mm4=(2*CbL * -FIX(0.22800) */
      mm0 = _mm_mulhi_pi16(mm0, PW_F0402);    /* mm0=(2*CrL * FIX(0.40200)) */

      mm4 = _mm_add_pi16(mm4, PW_ONE);
      mm4 = _mm_srai_pi16(mm4, 1);            /* mm4=(CbL * -FIX(0.22800)) */
      mm0 = _mm_add_pi16(mm0, PW_ONE);
      mm0 = _mm_srai_pi16(mm0, 1);            /* mm0=(CrL * FIX(0.40200)) */

      mm4 = _mm_add_pi16(mm4, mm2);
      mm4 = _mm_add_pi16(mm4, mm2);       /* mm4=(CbL * FIX(1.77200))=(B-Y)L = (cblue)L */
      mm0 = _mm_add_pi16(mm0, mm6);       /* mm0=(CrL * FIX(1.40200))=(R-Y)L = (cred)L*/

      wk[0] = mm4;                            /* wk(0)=(cblue)L */
      mm4 = mm2;

      mm2 = _mm_unpacklo_pi16(mm2, mm6);
      mm2 = _mm_madd_pi16(mm2, PW_MF0344_F0285);
      mm2 = _mm_add_pi32(mm2, PD_ONEHALF);
      mm2 = _mm_srai_pi32(mm2, SCALEBITS);
      mm2 = _mm_packs_pi32(mm2, mm3);  /* mm2=CbL*-FIX(0.344)+CrL*FIX(0.285) */
      mm2 = _mm_sub_pi16(mm2, mm6);  /* mm2=CbL*-FIX(0.344)+CrL*-FIX(0.714)=(G-Y)L=(cgreen)L */

      mm5 = mm8;
      mm5 = _mm_unpacklo_pi8(mm5, mm3);       /* mm5=Y(0123)=YL */
      mm0 = _mm_add_pi16(mm0, mm5);      /* mm0=((cred)L+YL)=RL=(R0 R1 R2 R3) */
      mm2 = _mm_add_pi16(mm2, mm5);      /* mm2=((cgreen)L+YL)=GE=(G0 G1 G2 G3) */
      mm4 = _mm_add_pi16(mm5, wk[0]);    /* mm4=(YL+(cblue)L)=BL=(B0 B1 B2 B3) */
      mm0 = _mm_packs_pu16(mm0,mm0);
      mm2 = _mm_packs_pu16(mm2,mm2);
      mm4 = _mm_packs_pu16(mm4,mm4);
#if RGB_PIXELSIZE == 3
      mmA = _mm_unpacklo_pi8(mmA, mmC);
      mmA = _mm_unpacklo_pi16(mmA, mmE);
      asm(".set noreorder\r\n"

          "move    $8, %2\r\n"
          "mov.s   $f4, %1\r\n"
          "mfc1    $9, $f4\r\n"
          "ush     $9, 0($8)\r\n"
          "srl     $9, 16\r\n"
          "sb      $9, 2($8)\r\n"
          : "=m" (*outptr)
          : "f" (mmA), "r" (outptr)
          : "$f4", "$8", "$9", "memory"
         );
#else  /* RGB_PIXELSIZE == 4 */

#ifdef RGBX_FILLER_0XFF
      mm6 = _mm_cmpeq_pi8(mm6, mm6);
#else
      mm6 = _mm_xor_si64(mm6, mm6);
#endif
      mmA = _mm_unpacklo_pi8(mmA, mmC);
      mmE = _mm_unpacklo_pi8(mmE, mmG);
      mmA = _mm_unpacklo_pi16(mmA, mmE);
      asm(".set noreorder\r\n"

          "move    $8, %2\r\n"
          "mov.s   $f4, %1\r\n"
          "gsswlc1 $f4, 3($8)\r\n"
          "gsswrc1 $f4, 0($8)\r\n"
          : "=m" (*outptr)
          : "f" (mmA), "r" (outptr)
          : "$f4", "$8", "memory"
         );
#endif
    }
  }
}

void jsimd_h2v2_merged_upsample_mmi(JDIMENSION output_width, JSAMPIMAGE input_buf,
                                    JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf)
{
  JSAMPROW outptr0, outptr1, inptr00, inptr01, inptr1, inptr2;
  int num_cols, col;
  __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
  __m64 mm8, mm9, wk[1];

  inptr00 = input_buf[0][in_row_group_ctr * 2];
  inptr01 = input_buf[0][in_row_group_ctr * 2 + 1];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr0 = output_buf[0];
  outptr1 = output_buf[1];

  for (num_cols = output_width >> 1; num_cols > 0; num_cols -= 4,
       inptr00 += 8, inptr01 += 8, inptr1 += 4, inptr2 += 4) {

    if (!(((long)inptr1) & 7))
      mm5 = _mm_load_si64((__m64 *)inptr1);
    else
      mm5 = _mm_loadu_si64((__m64 *)inptr1);
    if (!(((long)inptr2) & 7))
      mm1 = _mm_load_si64((__m64 *)inptr2);
    else
      mm1 = _mm_loadu_si64((__m64 *)inptr2);
    if (!(((long)inptr00) & 7)) {
      mm8 = _mm_load_si64((__m64 *)inptr00);
      mm9 = _mm_load_si64((__m64 *)inptr01);
    } else {
      mm8 = _mm_loadu_si64((__m64 *)inptr00);
      mm9 = _mm_loadu_si64((__m64 *)inptr01);
    }
    mm4 = 0;
    mm7 = 0;
    mm3 = _mm_xor_si64(mm4, mm4);
    mm7 = _mm_cmpeq_pi16(mm7, mm7);
    mm7 = _mm_slli_pi16(mm7, 7);      /* mm7={0xFF80 0xFF80 0xFF80 0xFF80} */

    mm4 = _mm_unpacklo_pi8(mm5, mm3);       /* mm4=Cb(0123)=CbL */
    mm1 = _mm_unpacklo_pi8(mm1, mm3);       /* mm1=Cr(0123)=CrL */
    mm4 = _mm_add_pi16(mm4, mm7);
    mm1 = _mm_add_pi16(mm1, mm7);

    /* (Original)
     * R = Y                + 1.40200 * Cr
     * G = Y - 0.34414 * Cb - 0.71414 * Cr
     * B = Y + 1.77200 * Cb
     *
     * (This implementation)
     * R = Y                + 0.40200 * Cr + Cr
     * G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr
     * B = Y - 0.22800 * Cb + Cb + Cb
    */

    mm3 = mm4;                              /* mm3 = CbL */
    mm4 = _mm_add_pi16(mm4, mm4);           /* mm4 = 2*CbL */
    mm6 = mm1;                              /* mm6 = CrL */
    mm1 = _mm_add_pi16(mm1, mm1);           /* mm1 = 2*CrL */

    mm4 = _mm_mulhi_pi16(mm4, PW_MF0228);   /* mm4=(2*CbL * -FIX(0.22800) */
    mm1 = _mm_mulhi_pi16(mm1, PW_F0402);    /* mm1=(2*CrL * FIX(0.40200)) */

    mm4 = _mm_add_pi16(mm4, PW_ONE);
    mm4 = _mm_srai_pi16(mm4, 1);            /* mm4=(CbL * -FIX(0.22800)) */
    mm1 = _mm_add_pi16(mm1, PW_ONE);
    mm1 = _mm_srai_pi16(mm1, 1);            /* mm1=(CrL * FIX(0.40200)) */

    mm4 = _mm_add_pi16(mm4, mm3);
    mm4 = _mm_add_pi16(mm4, mm3);       /* mm4=(CbL * FIX(1.77200))=(B-Y)L = (cblue)L */
    mm1 = _mm_add_pi16(mm1, mm6);       /* mm1=(CrL * FIX(1.40200))=(R-Y)L = (cred)L*/

    wk[0] = mm4;                            /* wk(0)=(cblue)L */

    mm4 = mm3;
    mm3 = _mm_unpacklo_pi16(mm3, mm6);
    mm4 = _mm_unpackhi_pi16(mm4, mm6);
    mm3 = _mm_madd_pi16(mm3, PW_MF0344_F0285);
    mm4 = _mm_madd_pi16(mm4, PW_MF0344_F0285);

    mm3 = _mm_add_pi32(mm3, PD_ONEHALF);
    mm4 = _mm_add_pi32(mm4, PD_ONEHALF);
    mm3 = _mm_srai_pi32(mm3, SCALEBITS);
    mm4 = _mm_srai_pi32(mm4, SCALEBITS);

    mm3 = _mm_packs_pi32(mm3, mm4);  /* mm3=CbL*-FIX(0.344)+CrL*FIX(0.285) */
    mm3 = _mm_sub_pi16(mm3, mm6);  /* mm3=CbL*-FIX(0.344)+CrL*-FIX(0.714)=(G-Y)L=(cgreen)L */

    mm5 = mm8;                              /* mm5=Y0(01234567) */
    mm7 = mm9;                              /* mm7=Y1(01234567) */

    mm6 = _mm_cmpeq_pi16(mm4, mm4);
    mm6 = _mm_srli_pi16(mm6, BYTE_BIT);    /* mm6={0xFF 0x00 0xFF 0x00 ..} */
    mm4 = _mm_and_si64(mm6, mm5);          /* mm4=Y0(0246)=Y0E */
    mm5 = _mm_srli_pi16(mm5, BYTE_BIT);    /* mm5=Y0(1357)=Y0O */
    mm6 = _mm_and_si64(mm6, mm7);          /* mm6=Y1(0246)=Y1E */
    mm7 = _mm_srli_pi16(mm7, BYTE_BIT);    /* mm7=Y1(1257)=Y1O */

    mm8 = _mm_add_pi16(mm1, mm4);      /* mm8=((cred)L+Y0E)=R0E=(R00 R02 R04 R06) */
    mm9 = _mm_add_pi16(mm1, mm5);      /* mm9=((cred)L+Y0O)=R0O=(R01 R03 R05 R07) */
    mm0 = _mm_add_pi16(mm1, mm6);      /* mm0=((cred)L+Y1E)=R1E=(R10 R12 R14 R16) */
    mm1 = _mm_add_pi16(mm1, mm7);      /* mm1=((cred)L+Y1O)=R1O=(R11 R13 R15 R17) */
    mm0 = _mm_packs_pu16(mm8, mm0);    /* mm0=(R00 R02 R04 R06 R10 R12 R14 R16) */
    mm1 = _mm_packs_pu16(mm9, mm1);    /* mm1=(R01 R03 R05 R07 R11 R13 R15 R17) */

    mm8 = _mm_add_pi16(mm3, mm4);      /* mm8=((cgreen)L+Y0E)=G0E=(G00 G02 G04 G06) */
    mm9 = _mm_add_pi16(mm3, mm5);      /* mm9=((cgreen)L+Y0O)=G0O=(G01 G03 G05 G07) */
    mm2 = _mm_add_pi16(mm3, mm6);      /* mm2=((cgreen)L+Y1E)=G1E=(G10 G12 G14 G16) */
    mm3 = _mm_add_pi16(mm3, mm7);      /* mm3=((cgreen)L+Y1O)=G1O=(G11 G13 G15 G17) */
    mm2 = _mm_packs_pu16(mm8, mm2);    /* mm2=(G00 G02 G04 G06 G10 G12 G14 G16) */
    mm3 = _mm_packs_pu16(mm9, mm3);    /* mm3=(G01 G03 G05 G07 G11 G13 G15 G17) */

    mm4 = _mm_add_pi16(mm4, wk[0]);    /* mm4=(Y0E+(cblue)L)=B0E=(B00 B02 B04 B06) */
    mm5 = _mm_add_pi16(mm5, wk[0]);    /* mm5=(Y0O+(cblue)L)=B0O=(B01 B03 B05 B07) */
    mm8 = _mm_add_pi16(mm6, wk[0]);    /* mm8=(Y1E+(cblue)L)=B1E=(B10 B12 B14 B16) */
    mm9 = _mm_add_pi16(mm7, wk[0]);    /* mm9=(Y1O+(cblue)L)=B1O=(B11 B13 B15 B17) */
    mm4 = _mm_packs_pu16(mm4, mm8);    /* mm4=(B00 B02 B04 B06 B10 B12 B14 B16) */
    mm5 = _mm_packs_pu16(mm5, mm9);    /* mm5=(B01 B03 B05 B07 B11 B13 B15 B17) */

#if RGB_PIXELSIZE == 3

    /* mmA=(000 002 004 006 010 012 014 016), mmB=(001 003 005 007 011 013 015 017) */
    /* mmC=(100 102 104 106 110 112 114 116), mmD=(101 103 105 107 111 113 115 117) */
    /* mmE=(200 202 204 206 210 212 214 216), mmF=(201 203 205 207 211 213 215 217) */
    mmG = _mm_unpacklo_pi8(mmA, mmC);     /* mmG=(000 100 002 102 004 104 006 106) */
    mmA = _mm_unpackhi_pi8(mmA, mmC);     /* mmA=(010 110 012 112 014 114 016 116) */
    mmH = _mm_unpacklo_pi8(mmE, mmB);     /* mmH=(200 001 202 003 204 005 206 007) */
    mmE = _mm_unpackhi_pi8(mmE, mmB);     /* mmE=(210 011 212 013 214 015 216 017) */
    mmC = _mm_unpacklo_pi8(mmD, mmF);     /* mmC=(101 201 103 203 105 205 107 207) */
    mmD = _mm_unpackhi_pi8(mmD, mmF);     /* mmD=(111 211 113 213 115 215 117 217) */

    mmB = _mm_unpacklo_pi16(mmG, mmA);     /* mmB=(000 100 010 110 002 102 012 112)*/
    mmA = _mm_unpackhi_pi16(mmG, mmA);     /* mmA=(004 104 014 114 006 106 016 116)*/
    mmF = _mm_unpacklo_pi16(mmH, mmE);     /* mmF=(200 001 210 011 202 003 212 013)*/
    mmE = _mm_unpackhi_pi16(mmH, mmE);     /* mmE=(204 005 214 015 206 007 216 017)*/
    mmH = _mm_unpacklo_pi16(mmC, mmD);     /* mmH=(101 201 111 211 103 203 113 213)*/
    mmG = _mm_unpackhi_pi16(mmC, mmD);     /* mmG=(105 205 115 215 107 207 117 217)*/

    mmC = _mm_unpacklo_pi16(mmB, mmF);     /* mmC=(000 100 200 001 010 110 210 011)*/
    mmB = _mm_srli_si64(mmB, 4 * BYTE_BIT);
    mmB = _mm_unpacklo_pi16(mmH, mmB);     /* mmB=(101 201 002 102 111 211 012 112)*/
    mmD = _mm_unpackhi_pi16(mmF, mmH);     /* mmD=(202 003 103 203 212 013 113 213)*/
    mmF = _mm_unpacklo_pi16(mmA, mmE);     /* mmF=(004 104 204 005 014 114 214 015)*/
    mmA = _mm_srli_si64(mmA, 4 * BYTE_BIT);
    mmH = _mm_unpacklo_pi16(mmG, mmA);     /* mmH=(105 205 006 106 115 215 016 116)*/
    mmG = _mm_unpackhi_pi16(mmE, mmG);     /* mmG=(206 007 107 207 216 017 117 217)*/

    mmA = _mm_unpacklo_pi32(mmC, mmB);     /* mmA=(000 100 200 001 101 201 002 102)*/
    mmE = _mm_unpackhi_pi32(mmC, mmB);     /* mmE=(010 110 210 011 111 211 012 112)*/
    mmB = _mm_unpacklo_pi32(mmD, mmF);     /* mmB=(202 003 103 203 004 104 204 005)*/
    mmF = _mm_unpackhi_pi32(mmD, mmF);     /* mmF=(212 013 113 213 014 114 214 015)*/
    mmC = _mm_unpacklo_pi32(mmH, mmG);     /* mmC=(105 205 006 106 206 007 107 207)*/
    mmG = _mm_unpackhi_pi32(mmH, mmG);     /* mmG=(115 215 016 116 216 017 117 217)*/

    if (num_cols >= 4) {

      if (!(((long)outptr0) & 7)) {
        _mm_store_si64((__m64 *)outptr0, mmA);
        _mm_store_si64((__m64 *)(outptr0 + 8), mmB);
        _mm_store_si64((__m64 *)(outptr0 + 16), mmC);
      } else {
        _mm_storeu_si64((__m64 *)outptr0, mmA);
        _mm_storeu_si64((__m64 *)(outptr0 + 8), mmB);
        _mm_storeu_si64((__m64 *)(outptr0 + 16), mmC);
      }

      if (!(((long)outptr1) & 7)) {
        _mm_store_si64((__m64 *)outptr1, mmE);
        _mm_store_si64((__m64 *)(outptr1 + 8), mmF);
        _mm_store_si64((__m64 *)(outptr1 + 16), mmG);
      } else {
        _mm_storeu_si64((__m64 *)outptr1, mmE);
        _mm_storeu_si64((__m64 *)(outptr1 + 8), mmF);
        _mm_storeu_si64((__m64 *)(outptr1 + 16), mmG);
      }

      outptr0 += RGB_PIXELSIZE * 8;
      outptr1 += RGB_PIXELSIZE * 8;
    } else {
      if(output_width & 1)
        col = num_cols * 6 + 3;
      else
        col = num_cols * 6;

      asm(".set noreorder\r\n"

          "li       $8, 16\r\n"               /* st16 */
          "move     $9, %8\r\n"
          "mov.s    $f4, %2\r\n"
          "mov.s    $f6, %3\r\n"
          "mov.s    $f8, %5\r\n"
          "mov.s    $f10, %6\r\n"
          "move     $10, %9\r\n"
          "move     $11, %10\r\n"
          "bltu     $9, $8, 1f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f6, 7+8($10)\r\n"
          "gssdrc1  $f6, 8($10)\r\n"
          "gssdlc1  $f8, 7($11)\r\n"
          "gssdrc1  $f8, 0($11)\r\n"
          "gssdlc1  $f10, 7+8($11)\r\n"
          "gssdrc1  $f10, 8($11)\r\n"
          "mov.s    $f4, %4\r\n"
          "mov.s    $f8, %7\r\n"
          "subu     $9, $9, 16\r\n"
          PTR_ADDU  "$10, $10, 16\r\n"
          PTR_ADDU  "$11, $11, 16\r\n"

          "1:       \r\n"
          "li       $8,  8\r\n"               /* st8 */
          "bltu     $9, $8, 2f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f8, 7($11)\r\n"
          "gssdrc1  $f8, 0($11)\r\n"
          "mov.s    $f4, $f6\r\n"
          "mov.s    $f8, $f10\r\n"
          "subu     $9, $9, 8\r\n"
          PTR_ADDU  "$10, $10, 8\r\n"
          PTR_ADDU  "$11, $11, 8\r\n"

          "2:       \r\n"
          "li       $8,  4\r\n"               /* st4 */
          "mfc1     $12, $f4\r\n"
          "mfc1     $13, $f8\r\n"
          "bltu     $9, $8, 3f\r\n"
          "nop      \r\n"
          "swl      $12, 3($10)\r\n"
          "swr      $12, 0($10)\r\n"
          "swl      $13, 3($11)\r\n"
          "swr      $13, 0($11)\r\n"
          "li       $8, 32\r\n"
          "mtc1     $8, $f6\r\n"
          "dsrl     $f4, $f4, $f6\r\n"
          "dsrl     $f8, $f8, $f6\r\n"
          "mfc1     $12, $f4\r\n"
          "mfc1     $13, $f8\r\n"
          "subu     $9, $9, 4\r\n"
          PTR_ADDU  "$10, $10, 4\r\n"
          PTR_ADDU  "$11, $11, 4\r\n"

          "3:       \r\n"
          "li       $8, 2\r\n"               /* st2 */
          "bltu     $9, $8, 4f\r\n"
          "nop      \r\n"
          "ush      $12, 0($10)\r\n"
          "ush      $13, 0($11)\r\n"
          "srl      $12, 16\r\n"
          "srl      $13, 16\r\n"
          "subu     $9, $9, 2\r\n"
          PTR_ADDU  "$10, $10, 2\r\n"
          PTR_ADDU  "$11, $11, 2\r\n"

          "4:       \r\n"
          "li       $8, 1\r\n"               /* st1 */
          "bltu     $9, $8, 5f\r\n"
          "nop      \r\n"
          "sb       $12, 0($10)\r\n"
          "sb       $13, 0($11)\r\n"

          "5:       \r\n"
          "nop      \r\n"                    /* end */
          : "=m" (*outptr0), "=m" (*outptr1)
          : "f" (mmA), "f" (mmB), "f" (mmC), "f" (mmE), "f" (mmF),
            "f" (mmG), "r" (col), "r" (outptr0), "r" (outptr1)
          : "$f4", "$f6", "$f8", "$f10", "$8", "$9", "$10", "$11", "$12", "$13", "memory"
         );
    }

#else  /* RGB_PIXELSIZE == 4 */

#ifdef RGBX_FILLER_0XFF
    mm6 = _mm_cmpeq_pi8(mm6, mm6);
    mm7 = _mm_cmpeq_pi8(mm7, mm7);
#else
    mm6 = _mm_xor_si64(mm6, mm6);
    mm7 = _mm_xor_si64(mm7, mm7);
#endif
    /* mmA=(000 002 004 006 010 012 014 016), mmB=(001 003 005 007 011 013 015 017) */
    /* mmC=(100 102 104 106 110 112 114 116), mmD=(101 103 105 107 111 113 115 117) */
    /* mmE=(200 202 204 206 210 212 214 216), mmF=(201 203 205 207 211 213 215 217) */
    /* mmG=(300 302 304 306 310 312 314 316), mmH=(301 303 305 307 311 313 315 317) */

    mm8 = _mm_unpacklo_pi8(mmA, mmC);     /* mm8=(000 100 002 102 004 104 006 106) */
    mm9 = _mm_unpackhi_pi8(mmA, mmC);     /* mm9=(010 110 012 112 014 114 016 116) */
    mmA = _mm_unpacklo_pi8(mmE, mmG);     /* mmA=(200 300 202 302 204 304 206 306) */
    mmE = _mm_unpackhi_pi8(mmE, mmG);     /* mmE=(210 310 212 312 214 314 216 316) */

    mmG = _mm_unpacklo_pi8(mmB, mmD);     /* mmG=(001 101 003 103 005 105 007 107) */
    mmB = _mm_unpackhi_pi8(mmB, mmD);     /* mmB=(011 111 013 113 015 115 017 117) */
    mmD = _mm_unpacklo_pi8(mmF, mmH);     /* mmD=(201 301 203 303 205 305 207 307) */
    mmF = _mm_unpackhi_pi8(mmF, mmH);     /* mmF=(211 311 213 313 215 315 217 317) */

    mmH = _mm_unpacklo_pi16(mm8, mmA);    /* mmH=(000 100 200 300 002 102 202 302) */
    mm8 = _mm_unpackhi_pi16(mm8, mmA);    /* mm8=(004 104 204 304 006 106 206 306) */
    mmA = _mm_unpacklo_pi16(mmG, mmD);    /* mmA=(001 101 201 301 003 103 203 303) */
    mmD = _mm_unpackhi_pi16(mmG, mmD);    /* mmD=(005 105 205 305 007 107 207 307) */

    mmG = _mm_unpackhi_pi16(mm9, mmE);    /* mmG=(014 114 214 314 016 116 216 316) */
    mm9 = _mm_unpacklo_pi16(mm9, mmE);    /* mm9=(010 110 210 310 012 112 212 312) */
    mmE = _mm_unpacklo_pi16(mmB, mmF);    /* mmE=(011 111 211 311 013 113 213 313) */
    mmF = _mm_unpackhi_pi16(mmB, mmF);    /* mmF=(015 115 215 315 017 117 217 317) */

    mmB = _mm_unpackhi_pi32(mmH, mmA);    /* mmB=(002 102 202 302 003 103 203 303) */
    mmA = _mm_unpacklo_pi32(mmH, mmA);    /* mmA=(000 100 200 300 001 101 201 301) */
    mmC = _mm_unpacklo_pi32(mm8, mmD);    /* mmC=(004 104 204 304 005 105 205 305) */
    mmD = _mm_unpackhi_pi32(mm8, mmD);    /* mmD=(006 106 206 306 007 107 207 307) */

    mmH = _mm_unpackhi_pi32(mmG, mmF);    /* mmH=(016 116 216 316 017 117 217 317) */
    mmG = _mm_unpacklo_pi32(mmG, mmF);    /* mmG=(014 114 214 314 015 115 215 315) */
    mmF = _mm_unpackhi_pi32(mm9, mmE);    /* mmF=(012 112 212 312 013 113 213 313) */
    mmE = _mm_unpacklo_pi32(mm9, mmE);    /* mmE=(010 110 210 310 011 111 211 311) */

    if (num_cols >= 4) {
      if (!(((long)outptr0) & 7)) {
        _mm_store_si64((__m64 *)outptr0, mmA);
        _mm_store_si64((__m64 *)(outptr0 + 8), mmB);
        _mm_store_si64((__m64 *)(outptr0 + 16), mmC);
        _mm_store_si64((__m64 *)(outptr0 + 24), mmD);
      } else {
        _mm_storeu_si64((__m64 *)outptr0, mmA);
        _mm_storeu_si64((__m64 *)(outptr0 + 8), mmB);
        _mm_storeu_si64((__m64 *)(outptr0 + 16), mmC);
        _mm_storeu_si64((__m64 *)(outptr0 + 24), mmD);
      }

      if (!(((long)outptr1) & 7)) {
        _mm_store_si64((__m64 *)outptr1, mmE);
        _mm_store_si64((__m64 *)(outptr1 + 8), mmF);
        _mm_store_si64((__m64 *)(outptr1 + 16), mmG);
        _mm_store_si64((__m64 *)(outptr1 + 24), mmH);
      } else {
        _mm_storeu_si64((__m64 *)outptr1, mmE);
        _mm_storeu_si64((__m64 *)(outptr1 + 8), mmF);
        _mm_storeu_si64((__m64 *)(outptr1 + 16), mmG);
        _mm_storeu_si64((__m64 *)(outptr1 + 24), mmH);
      }
      outptr0 += RGB_PIXELSIZE * 8;
      outptr1 += RGB_PIXELSIZE * 8;
    } else {
      if(output_width & 1)
        col = num_cols * 2 + 1;
      else
        col = num_cols * 2;
      asm(".set noreorder\r\n"

          "li       $8, 4\r\n"               /* st16 */
          "move     $9, %11\r\n"
          "move     $10, %12\r\n"
          "move     $11, %13\r\n"
          "mov.s    $f4, %3\r\n"
          "mov.s    $f6, %4\r\n"
          "mov.s    $f8, %7\r\n"
          "mov.s    $f10, %8\r\n"
          "bltu     $9, $8, 1f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f6, 7+8($10)\r\n"
          "gssdrc1  $f6, 8($10)\r\n"
          "gssdlc1  $f8, 7($11)\r\n"
          "gssdrc1  $f8, 0($11)\r\n"
          "gssdlc1  $f10, 7+8($11)\r\n"
          "gssdrc1  $f10, 8($11)\r\n"
          "mov.s    $f4, %5\r\n"
          "mov.s    $f6, %6\r\n"
          "mov.s    $f8, %9\r\n"
          "mov.s    $f10, %10\r\n"
          "subu     $9, $9, 4\r\n"
          PTR_ADDU  "$10, $10, 16\r\n"
          PTR_ADDU  "$11, $11, 16\r\n"

          "1:       \r\n"
          "li       $8, 2\r\n"               /* st8 */
          "bltu     $9, $8, 2f\r\n"
          "nop      \r\n"
          "gssdlc1  $f4, 7($10)\r\n"
          "gssdrc1  $f4, 0($10)\r\n"
          "gssdlc1  $f8, 7($11)\r\n"
          "gssdrc1  $f8, 0($11)\r\n"
          "mov.s    $f4, $f6\r\n"
          "mov.s    $f8, $f10\r\n"
          "subu     $9, $9, 2\r\n"
          PTR_ADDU  "$10, $10, 8\r\n"
          PTR_ADDU  "$11, $11, 8\r\n"

          "2:       \r\n"
          "li       $8, 1\r\n"               /* st4 */
          "bltu     $9, $8, 3f\r\n"
          "nop      \r\n"
          "gsswlc1  $f4, 3($10)\r\n"
          "gsswrc1  $f4, 0($10)\r\n"
          "gsswlc1  $f8, 3($11)\r\n"
          "gsswrc1  $f8, 0($11)\r\n"

          "3:       \r\n"
          "li       %2, 0\r\n"               /* end */
          : "=m" (*outptr0), "=m" (*outptr1), "=r" (col)
          : "f" (mmA), "f" (mmB), "f" (mmC), "f" (mmD), "f" (mmE), "f" (mmF),
            "f" (mmG), "f" (mmH), "r" (col), "r" (outptr0), "r" (outptr1)
          : "$f4", "$f6", "$f8", "$f10", "$8", "$9", "$10", "memory"
         );
    }

#endif
  }

  if(!((output_width >> 1) & 3)) {
    if(output_width & 1) {
      mm5 = _mm_load_si64((__m64 *)inptr1);
      mm1 = _mm_load_si64((__m64 *)inptr2);
      mm8 = _mm_load_si64((__m64 *)inptr00);
      mm9 = _mm_load_si64((__m64 *)inptr01);
      mm4 = 0;
      mm7 = 0;
      mm3 = _mm_xor_si64(mm4, mm4);
      mm7 = _mm_cmpeq_pi16(mm7, mm7);
      mm7 = _mm_slli_pi16(mm7, 7);      /* mm7={0xFF80 0xFF80 0xFF80 0xFF80} */
      mm4 = _mm_unpacklo_pi8(mm5, mm3);       /* mm4=Cb(0123)=CbL */
      mm4 = _mm_unpacklo_pi16(mm4, mm4);      /* mm4=Cb(0011)=Cb */
      mm0 = _mm_unpacklo_pi8(mm1, mm3);       /* mm0=Cr(0123)=CrL */
      mm0 = _mm_unpacklo_pi16(mm0, mm0);       /* mm0=Cr(0011)=Cr */
      mm4 = _mm_add_pi16(mm4, mm7);
      mm0 = _mm_add_pi16(mm0, mm7);

      mm2 = mm4;                              /* mm2 = Cb */
      mm4 = _mm_add_pi16(mm4, mm4);           /* mm4 = 2*Cb */
      mm6 = mm0;                              /* mm6 = Cr */
      mm0 = _mm_add_pi16(mm0, mm0);           /* mm0 = 2*Cr */
      mm4 = _mm_mulhi_pi16(mm4, PW_MF0228);   /* mm4=(2*Cb * -FIX(0.22800) */
      mm0 = _mm_mulhi_pi16(mm0, PW_F0402);    /* mm0=(2*Cr * FIX(0.40200)) */

      mm4 = _mm_add_pi16(mm4, PW_ONE);
      mm4 = _mm_srai_pi16(mm4, 1);            /* mm4=(Cb * -FIX(0.22800)) */
      mm0 = _mm_add_pi16(mm0, PW_ONE);
      mm0 = _mm_srai_pi16(mm0, 1);            /* mm0=(Cr * FIX(0.40200)) */

      mm4 = _mm_add_pi16(mm4, mm2);
      mm4 = _mm_add_pi16(mm4, mm2);       /* mm4=(Cb * FIX(1.77200))=(B-Y) = (cblue) */
      mm0 = _mm_add_pi16(mm0, mm6);       /* mm0=(Cr * FIX(1.40200))=(R-Y) = (cred)*/

      wk[0] = mm4;                            /* wk(0)=(cblue) */
      mm4 = mm2;

      mm2 = _mm_unpacklo_pi16(mm2, mm6);
      mm2 = _mm_madd_pi16(mm2, PW_MF0344_F0285);
      mm2 = _mm_add_pi32(mm2, PD_ONEHALF);
      mm2 = _mm_srai_pi32(mm2, SCALEBITS);
      mm2 = _mm_packs_pi32(mm2, mm3);  /* mm2=Cb*-FIX(0.344)+Cr*FIX(0.285) */
      mm2 = _mm_sub_pi16(mm2, mm6);  /* mm2=Cb*-FIX(0.344)+Cr*-FIX(0.714)=(G-Y)=(cgreen) */

      mm5 = mm8;
      mm7 = mm9;
      mm5 = _mm_unpacklo_pi8(mm5, mm7);
      mm5 = _mm_unpacklo_pi8(mm5, mm3);       /* mm5=(Y00 Y10 Y01 Y11) */
      mm0 = _mm_add_pi16(mm0, mm5);      /* mm0=((cred)L+YL)=RL=(R00 R01 R10 R11) */
      mm2 = _mm_add_pi16(mm2, mm5);      /* mm2=((cgreen)L+YL)=GE=(G00 G01 - -) */
      mm4 = _mm_add_pi16(mm5, wk[0]);    /* mm4=(YL+(cblue)L)=BL=(B00 B01 B10 B11) */
      mm0 = _mm_packs_pu16(mm0,mm0);
      mm2 = _mm_packs_pu16(mm2,mm2);
      mm4 = _mm_packs_pu16(mm4,mm4);
#if RGB_PIXELSIZE == 3
      mmA = _mm_unpacklo_pi8(mmA, mmC);
      mmB = _mm_unpacklo_pi16(mmA, mmE);
      mmA = _mm_srli_si64(mmA, 2 * BYTE_BIT);
      mmE = _mm_srli_si64(mmE, BYTE_BIT);
      mmC = _mm_unpacklo_pi16(mmA, mmE);
      asm(".set noreorder\r\n"

          "move    $8, %4\r\n"
          "move    $9, %5\r\n"
          "mov.s   $f4, %2\r\n"
          "mov.s   $f6, %3\r\n"
          "mfc1    $10, $f4\r\n"
          "mfc1    $11, $f6\r\n"
          "ush     $10, 0($8)\r\n"
          "ush     $11, 0($9)\r\n"
          "srl     $10, 16\r\n"
          "srl     $11, 16\r\n"
          "sb      $10, 2($8)\r\n"
          "sb      $11, 2($9)\r\n"
          : "=m" (*outptr0), "=m" (*outptr1)
          : "f" (mmB), "f" (mmC), "r" (outptr0), "r" (outptr1)
          : "$f4", "$f6", "$8", "$9", "$10", "$11", "memory"
         );
#else  /* RGB_PIXELSIZE == 4 */

#ifdef RGBX_FILLER_0XFF
      mm6 = _mm_cmpeq_pi8(mm6, mm6);
#else
      mm6 = _mm_xor_si64(mm6, mm6);
#endif
      mmA = _mm_unpacklo_pi8(mmA, mmC);
      mmE = _mm_unpacklo_pi8(mmE, mmG);
      mmB = _mm_unpacklo_pi16(mmA, mmE);
      mmC = _mm_srli_si64(mmB, 4 * BYTE_BIT);;
      asm(".set noreorder\r\n"

          "move    $8, %4\r\n"
          "move    $9, %5\r\n"
          "mov.s   $f4, %2\r\n"
          "mov.s   $f6, %3\r\n"
          "gsswlc1 $f4, 3($8)\r\n"
          "gsswrc1 $f4, 0($8)\r\n"
          "gsswlc1 $f6, 3($9)\r\n"
          "gsswrc1 $f6, 0($9)\r\n"
          : "=m" (*outptr0), "=m" (*outptr1)
          : "f" (mmB), "f" (mmC), "r" (outptr0), "r" (outptr1)
          : "$f4", "$f6", "$8", "$9", "memory"
         );
#endif
    }
  }
}

#undef mmA
#undef mmB
#undef mmC
#undef mmD
#undef mmE
#undef mmF
#undef mmG
#undef mmH
