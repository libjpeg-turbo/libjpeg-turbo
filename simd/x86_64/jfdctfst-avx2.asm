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
    SECTION     SEG_CONST

; PRE_MULTIPLY_SCALE_BITS <= 2 (to avoid overflow)
; CONST_BITS + CONST_SHIFT + PRE_MULTIPLY_SCALE_BITS == 16 (for pmulhw)

%define PRE_MULTIPLY_SCALE_BITS  2
%define CONST_SHIFT              (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)

    alignz      32
    global      EXTN(jconst_fdct_ifast_avx2)

EXTN(jconst_fdct_ifast_avx2):

PW_F0707 times 8 dw F_0_707 << CONST_SHIFT
PW_F0382 times 8 dw F_0_382 << CONST_SHIFT
PW_F0541 times 8 dw F_0_541 << CONST_SHIFT
PW_F1306 times 8 dw F_1_306 << CONST_SHIFT

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

    mov         rdx, r10                ; (DCTELEM *)

    movdqa      xmm0, XMMWORD [XMMBLOCK(0,0,rdx,SIZEOF_DCTELEM)]
    movdqa      xmm1, XMMWORD [XMMBLOCK(1,0,rdx,SIZEOF_DCTELEM)]
    movdqa      xmm2, XMMWORD [XMMBLOCK(2,0,rdx,SIZEOF_DCTELEM)]
    movdqa      xmm3, XMMWORD [XMMBLOCK(3,0,rdx,SIZEOF_DCTELEM)]

    ; xmm0=(00 01 02 03 04 05 06 07), xmm2=(20 21 22 23 24 25 26 27)
    ; xmm1=(10 11 12 13 14 15 16 17), xmm3=(30 31 32 33 34 35 36 37)

    vpunpckhwd  xmm4, xmm0, xmm1        ; xmm4=(04 14 05 15 06 16 07 17)
    vpunpcklwd  xmm0, xmm0, xmm1        ; xmm0=(00 10 01 11 02 12 03 13)
    vpunpckhwd  xmm8, xmm2, xmm3        ; xmm8=(24 34 25 35 26 36 27 37)
    vpunpcklwd  xmm9, xmm2, xmm3        ; xmm9=(20 30 21 31 22 32 23 33)

    movdqa      xmm6, XMMWORD [XMMBLOCK(4,0,rdx,SIZEOF_DCTELEM)]
    movdqa      xmm7, XMMWORD [XMMBLOCK(5,0,rdx,SIZEOF_DCTELEM)]
    movdqa      xmm1, XMMWORD [XMMBLOCK(6,0,rdx,SIZEOF_DCTELEM)]
    movdqa      xmm3, XMMWORD [XMMBLOCK(7,0,rdx,SIZEOF_DCTELEM)]

    ; xmm6=(40 41 42 43 44 45 46 47), xmm1=(60 61 62 63 64 65 66 67)
    ; xmm7=(50 51 52 53 54 55 56 57), xmm3=(70 71 72 73 74 75 76 77)

    vpunpckhwd  xmm2, xmm6, xmm7        ; xmm2=(44 54 45 55 46 56 47 57)
    vpunpcklwd  xmm6, xmm6, xmm7        ; xmm6=(40 50 41 51 42 52 43 53)
    vpunpckhwd  xmm5, xmm1, xmm3        ; xmm5=(64 74 65 75 66 76 67 77)
    vpunpcklwd  xmm1, xmm1, xmm3        ; xmm1=(60 70 61 71 62 72 63 73)

    vpunpckhdq  xmm10, xmm6, xmm1       ; xmm10=(42 52 62 72 43 53 63 73)
    vpunpckldq  xmm6, xmm6, xmm1        ; xmm6=(40 50 60 70 41 51 61 71)
    vpunpckhdq  xmm3, xmm2, xmm5        ; xmm3=(46 56 66 76 47 57 67 77)
    vpunpckldq  xmm11, xmm2, xmm5       ; xmm11=(44 54 64 74 45 55 65 75)

    vpunpckhdq  xmm7, xmm0, xmm9        ; xmm7=(02 12 22 32 03 13 23 33)
    vpunpckldq  xmm0, xmm0, xmm9        ; xmm0=(00 10 20 30 01 11 21 31)
    vpunpckhdq  xmm2, xmm4, xmm8        ; xmm2=(06 16 26 36 07 17 27 37)
    vpunpckldq  xmm4, xmm4, xmm8        ; xmm4=(04 14 24 34 05 15 25 35)

    vpunpckhqdq xmm1, xmm0, xmm6        ; xmm1=(01 11 21 31 41 51 61 71)=data1
    vpunpcklqdq xmm0, xmm0, xmm6        ; xmm0=(00 10 20 30 40 50 60 70)=data0
    vpunpckhqdq xmm5, xmm2, xmm3        ; xmm5=(07 17 27 37 47 57 67 77)=data7
    vpunpcklqdq xmm2, xmm2, xmm3        ; xmm2=(06 16 26 36 46 56 66 76)=data6

    vpaddw      xmm6, xmm1, xmm2        ; xmm6=data1+data6=tmp1
    vpaddw      xmm3, xmm0, xmm5        ; xmm3=data0+data7=tmp0
    vpsubw      xmm8, xmm1, xmm2        ; xmm8=data1-data6=tmp6
    vpsubw      xmm9, xmm0, xmm5        ; xmm9=data0-data7=tmp7

    vpunpckhqdq xmm1, xmm7, xmm10       ; xmm1=(03 13 23 33 43 53 63 73)=data3
    vpunpcklqdq xmm7, xmm7, xmm10       ; xmm7=(02 12 22 32 42 52 62 72)=data2
    vpunpckhqdq xmm0, xmm4, xmm11       ; xmm0=(05 15 25 35 45 55 65 75)=data5
    vpunpcklqdq xmm4, xmm4, xmm11       ; xmm4=(04 14 24 34 44 54 64 74)=data4

    vpsubw      xmm2, xmm1, xmm4        ; xmm2=data3-data4=tmp4
    vpsubw      xmm5, xmm7, xmm0        ; xmm5=data2-data5=tmp5
    vpaddw      xmm1, xmm1, xmm4        ; xmm1=data3+data4=tmp3
    vpaddw      xmm7, xmm7, xmm0        ; xmm7=data2+data5=tmp2

    ; -- Even part

    vpaddw      xmm4, xmm3, xmm1        ; xmm4=tmp10
    vpaddw      xmm0, xmm6, xmm7        ; xmm0=tmp11
    vpsubw      xmm3, xmm3, xmm1        ; xmm3=tmp13
    vpsubw      xmm6, xmm6, xmm7        ; xmm6=tmp12

    vpaddw      xmm6, xmm6, xmm3
    vpsllw      xmm6, xmm6, PRE_MULTIPLY_SCALE_BITS
    vpmulhw     xmm6, xmm6, [rel PW_F0707]  ; xmm6=z1

    vpaddw      xmm1, xmm4, xmm0        ; xmm1=data0
    vpaddw      xmm7, xmm3, xmm6        ; xmm7=data2
    vpsubw      xmm10, xmm4, xmm0       ; xmm10=data4
    vpsubw      xmm11, xmm3, xmm6       ; xmm11=data6

    ; -- Odd part

    vpaddw      xmm2, xmm2, xmm5        ; xmm2=tmp10
    vpaddw      xmm5, xmm5, xmm8        ; xmm5=tmp11
    vpaddw      xmm0, xmm8, xmm9        ; xmm0=tmp12, xmm9=tmp7

    vpsllw      xmm2, xmm2, PRE_MULTIPLY_SCALE_BITS
    vpsllw      xmm0, xmm0, PRE_MULTIPLY_SCALE_BITS

    vpsllw      xmm5, xmm5, PRE_MULTIPLY_SCALE_BITS
    vpmulhw     xmm5, xmm5, [rel PW_F0707]  ; xmm5=z3

    vpmulhw     xmm4, xmm2, [rel PW_F0541]  ; xmm4=MULTIPLY(tmp10,FIX_0_541196)
    vpsubw      xmm2, xmm2, xmm0
    vpmulhw     xmm2, xmm2, [rel PW_F0382]  ; xmm2=z5
    vpmulhw     xmm0, xmm0, [rel PW_F1306]  ; xmm0=MULTIPLY(tmp12,FIX_1_306562)
    vpaddw      xmm4, xmm4, xmm2        ; xmm4=z2
    vpaddw      xmm0, xmm0, xmm2        ; xmm0=z4

    vpsubw      xmm6, xmm9, xmm5        ; xmm6=z13
    vpaddw      xmm3, xmm9, xmm5        ; xmm3=z11

    vpaddw      xmm2, xmm6, xmm4        ; xmm2=data5
    vpaddw      xmm5, xmm3, xmm0        ; xmm5=data1
    vpsubw      xmm6, xmm6, xmm4        ; xmm6=data3
    vpsubw      xmm3, xmm3, xmm0        ; xmm3=data7

    ; ---- Pass 2: process columns.

    ; xmm1=(00 10 20 30 40 50 60 70), xmm7=(02 12 22 32 42 52 62 72)
    ; xmm5=(01 11 21 31 41 51 61 71), xmm6=(03 13 23 33 43 53 63 73)

    vpunpckhwd  xmm4, xmm1, xmm5        ; xmm4=(40 41 50 51 60 61 70 71)
    vpunpcklwd  xmm1, xmm1, xmm5        ; xmm1=(00 01 10 11 20 21 30 31)
    vpunpckhwd  xmm9, xmm7, xmm6        ; xmm9=(42 43 52 53 62 63 72 73)
    vpunpcklwd  xmm8, xmm7, xmm6        ; xmm8=(02 03 12 13 22 23 32 33)

    ; xmm5=(04 14 24 34 44 54 64 74), xmm6=(06 16 26 36 46 56 66 76)
    ; xmm2=(05 15 25 35 45 55 65 75), xmm3=(07 17 27 37 47 57 67 77)

    vpunpcklwd  xmm5, xmm10, xmm2       ; xmm5=(04 05 14 15 24 25 34 35)
    vpunpckhwd  xmm7, xmm10, xmm2       ; xmm7=(44 45 54 55 64 65 74 75)
    vpunpcklwd  xmm6, xmm11, xmm3       ; xmm6=(06 07 16 17 26 27 36 37)
    vpunpckhwd  xmm0, xmm11, xmm3       ; xmm0=(46 47 56 57 66 67 76 77)

    vpunpckhdq  xmm10, xmm5, xmm6       ; xmm10=(24 25 26 27 34 35 36 37)
    vpunpckldq  xmm5, xmm5, xmm6        ; xmm5=(04 05 06 07 14 15 16 17)
    vpunpckhdq  xmm3, xmm7, xmm0        ; xmm3=(64 65 66 67 74 75 76 77)
    vpunpckldq  xmm11, xmm7, xmm0       ; xmm11=(44 45 46 47 54 55 56 57)

    vpunpckhdq  xmm2, xmm1, xmm8        ; xmm2=(20 21 22 23 30 31 32 33)
    vpunpckldq  xmm1, xmm1, xmm8        ; xmm1=(00 01 02 03 10 11 12 13)
    vpunpckhdq  xmm7, xmm4, xmm9        ; xmm7=(60 61 62 63 70 71 72 73)
    vpunpckldq  xmm4, xmm4, xmm9        ; xmm4=(40 41 42 43 50 51 52 53)

    vpunpckhqdq xmm6, xmm1, xmm5        ; xmm6=(10 11 12 13 14 15 16 17)=data1
    vpunpcklqdq xmm1, xmm1, xmm5        ; xmm1=(00 01 02 03 04 05 06 07)=data0
    vpunpckhqdq xmm0, xmm7, xmm3        ; xmm0=(70 71 72 73 74 75 76 77)=data7
    vpunpcklqdq xmm7, xmm7, xmm3        ; xmm7=(60 61 62 63 64 65 66 67)=data6

    vpaddw      xmm5, xmm6, xmm7        ; xmm5=data1+data6=tmp1
    vpaddw      xmm3, xmm1, xmm0        ; xmm3=data0+data7=tmp0
    vpsubw      xmm8, xmm6, xmm7        ; xmm6=data1-data6=tmp6
    vpsubw      xmm9, xmm1, xmm0        ; xmm1=data0-data7=tmp7

    vpunpckhqdq xmm6, xmm2, xmm10       ; xmm6=(30 31 32 33 34 35 36 37)=data3
    vpunpcklqdq xmm2, xmm2, xmm10       ; xmm2=(20 21 22 23 24 25 26 27)=data2
    vpunpckhqdq xmm1, xmm4, xmm11       ; xmm1=(50 51 52 53 54 55 56 57)=data5
    vpunpcklqdq xmm4, xmm4, xmm11       ; xmm4=(40 41 42 43 44 45 46 47)=data4

    vpsubw      xmm7, xmm6, xmm4        ; xmm7=data3-data4=tmp4
    vpsubw      xmm0, xmm2, xmm1        ; xmm0=data2-data5=tmp5
    vpaddw      xmm6, xmm6, xmm4        ; xmm6=data3+data4=tmp3
    vpaddw      xmm2, xmm2, xmm1        ; xmm2=data2+data5=tmp2

    ; -- Even part

    vpaddw      xmm4, xmm3, xmm6        ; xmm4=tmp10
    vpaddw      xmm1, xmm5, xmm2        ; xmm1=tmp11
    vpsubw      xmm3, xmm3, xmm6        ; xmm3=tmp13
    vpsubw      xmm5, xmm5, xmm2        ; xmm5=tmp12

    vpaddw      xmm5, xmm5, xmm3
    vpsllw      xmm5, xmm5, PRE_MULTIPLY_SCALE_BITS
    vpmulhw     xmm5, xmm5, [rel PW_F0707]  ; xmm5=z1

    vpaddw      xmm6, xmm4, xmm1        ; xmm6=data0
    vpaddw      xmm2, xmm3, xmm5        ; xmm2=data2
    vpsubw      xmm4, xmm4, xmm1        ; xmm4=data4
    vpsubw      xmm3, xmm3, xmm5        ; xmm3=data6

    movdqa      XMMWORD [XMMBLOCK(4,0,rdx,SIZEOF_DCTELEM)], xmm4
    movdqa      XMMWORD [XMMBLOCK(6,0,rdx,SIZEOF_DCTELEM)], xmm3
    movdqa      XMMWORD [XMMBLOCK(0,0,rdx,SIZEOF_DCTELEM)], xmm6
    movdqa      XMMWORD [XMMBLOCK(2,0,rdx,SIZEOF_DCTELEM)], xmm2

    ; -- Odd part

    vpaddw      xmm7, xmm7, xmm0        ; xmm7=tmp10
    vpaddw      xmm0, xmm0, xmm8        ; xmm0=tmp11
    vpaddw      xmm1, xmm8, xmm9        ; xmm1=tmp12, xmm5=tmp7

    vpsllw      xmm7, xmm7, PRE_MULTIPLY_SCALE_BITS
    vpsllw      xmm1, xmm1, PRE_MULTIPLY_SCALE_BITS

    vpsllw      xmm0, xmm0, PRE_MULTIPLY_SCALE_BITS
    vpmulhw     xmm0, xmm0, [rel PW_F0707]  ; xmm0=z3

    vpmulhw     xmm4, xmm7, [rel PW_F0541]  ; xmm4=MULTIPLY(tmp10,FIX_0_541196)
    vpsubw      xmm7, xmm7, xmm1
    vpmulhw     xmm7, xmm7, [rel PW_F0382]  ; xmm7=z5
    vpmulhw     xmm1, xmm1, [rel PW_F1306]  ; xmm1=MULTIPLY(tmp12,FIX_1_306562)
    vpaddw      xmm4, xmm4, xmm7        ; xmm4=z2
    vpaddw      xmm1, xmm1, xmm7        ; xmm1=z4

    vpsubw      xmm5, xmm9, xmm0        ; xmm5=z13
    vpaddw      xmm3, xmm9, xmm0        ; xmm3=z11

    vpaddw      xmm6, xmm5, xmm4        ; xmm6=data5
    vpaddw      xmm2, xmm3, xmm1        ; xmm2=data1
    vpsubw      xmm5, xmm5, xmm4        ; xmm5=data3
    vpsubw      xmm3, xmm3, xmm1        ; xmm3=data7

    movdqa      XMMWORD [XMMBLOCK(3,0,rdx,SIZEOF_DCTELEM)], xmm5
    movdqa      XMMWORD [XMMBLOCK(7,0,rdx,SIZEOF_DCTELEM)], xmm3
    movdqa      XMMWORD [XMMBLOCK(5,0,rdx,SIZEOF_DCTELEM)], xmm6
    movdqa      XMMWORD [XMMBLOCK(1,0,rdx,SIZEOF_DCTELEM)], xmm2

    uncollect_args 1
    pop         rbp
    ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
