;
; jfdctfst.asm - fast integer FDCT (64-bit AVX2)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2009, 2016, 2018, D. R. Commander.
;
; Based on the x86 SIMD extension for IJG JPEG library
; Copyright (C) 1999-2006, MIYASAKA Masaru.
; For conditions of distribution and use, see copyright notice in jsimdext.inc
;
; This file should be assembled with NASM (Netwide Assembler),
; can *not* be assembled with Microsoft's MASM or any compatible
; assembler (including Borland's Turbo Assembler).
; NASM is available from http://nasm.sourceforge.net/ or
; http://sourceforge.net/project/showfiles.php?group_id=6208
;
; This file contains a fast, not so accurate integer implementation of
; the forward DCT (Discrete Cosine Transform). The following code is
; based directly on the IJG's original jfdctfst.c; see the jfdctfst.c
; for more details.
;
; [TAB8]

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------

%define CONST_BITS  8  ; 14 is also OK.

%if CONST_BITS == 8
F_0_382 equ  98  ; FIX(0.382683433)
F_0_541 equ 139  ; FIX(0.541196100)
F_0_707 equ 181  ; FIX(0.707106781)
F_1_306 equ 334  ; FIX(1.306562965)
%else
; NASM cannot do compile-time arithmetic on floating-point constants.
%define DESCALE(x,n)  (((x)+(1<<((n)-1)))>>(n))
F_0_382 equ DESCALE( 410903207, 30-CONST_BITS)  ; FIX(0.382683433)
F_0_541 equ DESCALE( 581104887, 30-CONST_BITS)  ; FIX(0.541196100)
F_0_707 equ DESCALE( 759250124, 30-CONST_BITS)  ; FIX(0.707106781)
F_1_306 equ DESCALE(1402911301, 30-CONST_BITS)  ; FIX(1.306562965)
%endif

; --------------------------------------------------------------------------
; In-place 8x8x16-bit matrix transpose using AVX2 instructions
; %1-%4: Input/output registers
; %5-%8: Temp registers

%macro ymmtranspose 8
    ; %1=(00 01 02 03 04 05 06 07  40 41 42 43 44 45 46 47)
    ; %2=(10 11 12 13 14 15 16 17  50 51 52 53 54 55 56 57)
    ; %3=(20 21 22 23 24 25 26 27  60 61 62 63 64 65 66 67)
    ; %4=(30 31 32 33 34 35 36 37  70 71 72 73 74 75 76 77)

    vpunpcklwd  %5, %1, %2
    vpunpckhwd  %6, %1, %2
    vpunpcklwd  %7, %3, %4
    vpunpckhwd  %8, %3, %4
    ; transpose coefficients(phase 1)
    ; %5=(00 10 01 11 02 12 03 13  40 50 41 51 42 52 43 53)
    ; %6=(04 14 05 15 06 16 07 17  44 54 45 55 46 56 47 57)
    ; %7=(20 30 21 31 22 32 23 33  60 70 61 71 62 72 63 73)
    ; %8=(24 34 25 35 26 36 27 37  64 74 65 75 66 76 67 77)

    vpunpckldq  %1, %5, %7
    vpunpckhdq  %2, %5, %7
    vpunpckldq  %3, %6, %8
    vpunpckhdq  %4, %6, %8
    ; transpose coefficients(phase 2)
    ; %1=(00 10 20 30 01 11 21 31  40 50 60 70 41 51 61 71)
    ; %2=(02 12 22 32 03 13 23 33  42 52 62 72 43 53 63 73)
    ; %3=(04 14 24 34 05 15 25 35  44 54 64 74 45 55 65 75)
    ; %4=(06 16 26 36 07 17 27 37  46 56 66 76 47 57 67 77)

    vpermq      %1, %1, 0xD8
    vpermq      %2, %2, 0x8D
    vpermq      %3, %3, 0xD8
    vpermq      %4, %4, 0x8D
    ; transpose coefficients(phase 3)
    ; %1=(00 10 20 30 40 50 60 70  01 11 21 31 41 51 61 71)
    ; %2=(03 13 23 33 43 53 63 73  02 12 22 32 42 52 62 72)
    ; %3=(04 14 24 34 44 54 64 74  05 15 25 35 45 55 65 75)
    ; %4=(07 17 27 37 47 57 67 77  06 16 26 36 46 56 66 76)
%endmacro

; --------------------------------------------------------------------------
; In-place 8x8x16-bit fast integer forward DCT using AVX2 instructions
; %1-%4: Input/output registers
; %5-%8: Temp registers

%macro dodct 8
    vpsubw      %5, %1, %4              ; %5=data0_1-data7_6=tmp7_6
    vpaddw      %6, %1, %4              ; %6=data0_1+data7_6=tmp0_1
    vpaddw      %7, %2, %3              ; %7=data3_2+data4_5=tmp3_2
    vpsubw      %8, %2, %3              ; %8=data3_2-data4_5=tmp4_5

    ; -- Even part

    vpaddw      %1, %6, %7              ; %1=tmp0_1+tmp3_2=tmp10_11
    vpsubw      %6, %6, %7              ; %6=tmp0_1-tmp3_2=tmp13_12

    vperm2i128  %7, %1, %1, 0x01        ; %7=tmp11_10
    vpsignw     %1, %1, [rel PW_1_NEG1]  ; %1=tmp10_neg11
    vpaddw      %1, %7, %1              ; %1=data0_4

    vperm2i128  %7, %6, %6, 0x01        ; %7=tmp12_13
    vpaddw      %7, %7, %6              ; %7=(tmp12+13)_(tmp12+13)
    vpsllw      %7, %7, PRE_MULTIPLY_SCALE_BITS
    vpmulhw     %7, %7, [rel PW_F0707]  ; %7=z1_z1
    vpsignw     %7, %7, [rel PW_1_NEG1]  ; %7=z1_negz1

    vperm2i128  %6, %6, %6, 0x00        ; %6=tmp13_13
    vpaddw      %3, %6, %7              ; %3=data2_6

    ; -- Odd part

    vperm2i128  %6, %8, %5, 0x30        ; %6=tmp4_6
    vperm2i128  %7, %8, %5, 0x21        ; %7=tmp5_7
    vpaddw      %6, %6, %7              ; %6=tmp10_12

    vpsllw      %6, %6, PRE_MULTIPLY_SCALE_BITS

    vperm2i128  %7, %6, %6, 0x00        ; %7=tmp10_10
    vperm2i128  %2, %6, %6, 0x11        ; %2=tmp12_12
    vpsubw      %7, %7, %2
    vpmulhw     %7, %7, [rel PW_F0382]  ; %7=z5_z5

    vpmulhw     %6, %6, [rel PW_F0541_F1306]  ; %6=MULTIPLY(tmp10,FIX_0_541196)_MULTIPLY(tmp12,FIX_1_306562)
    vpaddw      %6, %6, %7              ; %6=z2_z4

    vperm2i128  %7, %8, %5, 0x31        ; %7=tmp5_6
    vperm2i128  %2, %5, %8, 0x31        ; %2=tmp6_5
    vpaddw      %7, %7, %2              ; %7=(tmp5+6)_(tmp5+6)
    vpsllw      %7, %7, PRE_MULTIPLY_SCALE_BITS
    vpmulhw     %7, %7, [rel PW_F0707]  ; %7=z3_z3
    vpsignw     %7, %7, [rel PW_NEG1_1]  ; %7=negz3_z3

    vperm2i128  %2, %5, %5, 0x00        ; %2=tmp7_7
    vpaddw      %2, %2, %7              ; %2=z13_11

    vpsubw      %4, %2, %6              ; %4=z13_11-z2_4=data3_7
    vpaddw      %2, %2, %6              ; %2=z13_11+z2_4=data5_1
%endmacro

; --------------------------------------------------------------------------
    SECTION     SEG_CONST

; PRE_MULTIPLY_SCALE_BITS <= 2 (to avoid overflow)
; CONST_BITS + CONST_SHIFT + PRE_MULTIPLY_SCALE_BITS == 16 (for pmulhw)

%define PRE_MULTIPLY_SCALE_BITS  2
%define CONST_SHIFT              (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)

    alignz      32
    global      EXTN(jconst_fdct_ifast_avx2)

EXTN(jconst_fdct_ifast_avx2):

PW_F0707       times 16 dw  F_0_707 << CONST_SHIFT
PW_F0382       times 16 dw  F_0_382 << CONST_SHIFT
PW_F0541_F1306 times 8  dw  F_0_541 << CONST_SHIFT
               times 8  dw  F_1_306 << CONST_SHIFT
PW_1_NEG1      times 8  dw  1
               times 8  dw -1
PW_NEG1_1      times 8  dw -1
               times 8  dw  1

    alignz      32

; --------------------------------------------------------------------------
    SECTION     SEG_TEXT
    BITS        64
;
; Perform the forward DCT on one block of samples.
;
; GLOBAL(void)
; jsimd_fdct_ifast_avx2 (DCTELEM *data)
;

; r10 = DCTELEM *data

    align       32
    global      EXTN(jsimd_fdct_ifast_avx2)

EXTN(jsimd_fdct_ifast_avx2):
    push        rbp
    mov         rax, rsp
    mov         rbp, rsp
    collect_args 1

    ; ---- Pass 1: process rows.

    vmovdqu     ymm4, YMMWORD [YMMBLOCK(0,0,r10,SIZEOF_DCTELEM)]
    vmovdqu     ymm5, YMMWORD [YMMBLOCK(2,0,r10,SIZEOF_DCTELEM)]
    vmovdqu     ymm6, YMMWORD [YMMBLOCK(4,0,r10,SIZEOF_DCTELEM)]
    vmovdqu     ymm7, YMMWORD [YMMBLOCK(6,0,r10,SIZEOF_DCTELEM)]
    ; ymm4=(00 01 02 03 04 05 06 07  10 11 12 13 14 15 16 17)
    ; ymm5=(20 21 22 23 24 25 26 27  30 31 32 33 34 35 36 37)
    ; ymm6=(40 41 42 43 44 45 46 47  50 51 52 53 54 55 56 57)
    ; ymm7=(60 61 62 63 64 65 66 67  70 71 72 73 74 75 76 77)

    vperm2i128  ymm0, ymm4, ymm6, 0x20
    vperm2i128  ymm1, ymm4, ymm6, 0x31
    vperm2i128  ymm2, ymm5, ymm7, 0x20
    vperm2i128  ymm3, ymm5, ymm7, 0x31
    ; ymm0=(00 01 02 03 04 05 06 07  40 41 42 43 44 45 46 47)
    ; ymm1=(10 11 12 13 14 15 16 17  50 51 52 53 54 55 56 57)
    ; ymm2=(20 21 22 23 24 25 26 27  60 61 62 63 64 65 66 67)
    ; ymm3=(30 31 32 33 34 35 36 37  70 71 72 73 74 75 76 77)

    ymmtranspose ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7

    dodct       ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7
    ; ymm0=data0_4, ymm1=data5_1, ymm2=data2_6, ymm3=data3_7

    ; ---- Pass 2: process columns.

    vperm2i128  ymm1, ymm1, ymm1, 0x01  ; ymm1=data1_5

    ymmtranspose ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7

    dodct       ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7
    ; ymm0=data0_4, ymm1=data5_1, ymm2=data2_6, ymm3=data3_7

    vperm2i128 ymm4, ymm0, ymm1, 0x30   ; ymm4=data0_1
    vperm2i128 ymm5, ymm2, ymm3, 0x20   ; ymm5=data2_3
    vperm2i128 ymm6, ymm0, ymm1, 0x21   ; ymm6=data4_5
    vperm2i128 ymm7, ymm2, ymm3, 0x31   ; ymm7=data6_7

    vmovdqu     YMMWORD [YMMBLOCK(0,0,r10,SIZEOF_DCTELEM)], ymm4
    vmovdqu     YMMWORD [YMMBLOCK(2,0,r10,SIZEOF_DCTELEM)], ymm5
    vmovdqu     YMMWORD [YMMBLOCK(4,0,r10,SIZEOF_DCTELEM)], ymm6
    vmovdqu     YMMWORD [YMMBLOCK(6,0,r10,SIZEOF_DCTELEM)], ymm7

    vzeroupper
    uncollect_args 1
    pop         rbp
    ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
