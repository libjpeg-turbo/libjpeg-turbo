;
; jfdctflt.asm - floating-point FDCT (64-bit SSE)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009 D. R. Commander
; Copyright 2015 Intel Corporation
;
; Based on
; x86 SIMD extension for IJG JPEG library
; Copyright (C) 1999-2006, MIYASAKA Masaru.
; For conditions of distribution and use, see copyright notice in jsimdext.inc
;
; This file should be assembled with NASM (Netwide Assembler),
; can *not* be assembled with Microsoft's MASM or any compatible
; assembler (including Borland's Turbo Assembler).
; NASM is available from http://nasm.sourceforge.net/ or
; http://sourceforge.net/project/showfiles.php?group_id=6208
;
; This file contains a floating-point implementation of the forward DCT
; (Discrete Cosine Transform). The following code is based directly on
; the IJG's original jfdctflt.c; see the jfdctflt.c for more details.
;
; [TAB8]

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------
        SECTION SEG_CONST

        alignz  32
        global  EXTN(jconst_fdct_float_avx2)

EXTN(jconst_fdct_float_avx2):

PD_0_382        times 8 dd  0.382683432365089771728460
PD_0_707        times 8 dd  0.707106781186547524400844
PD_0_541        times 8 dd  0.541196100146196984399723
PD_1_306        times 8 dd  1.306562964876376527856643

        alignz  32

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Perform the forward DCT on one block of samples.
;
; GLOBAL(void)
; jsimd_fdct_float_avx2 (FAST_FLOAT * data)
;

; r10 = FAST_FLOAT * data

        align   32
        global  EXTN(jsimd_fdct_float_avx2)

EXTN(jsimd_fdct_float_avx2):

        ; ---- Pass 1: process rows.

        mov     rdx, rdi        ; (FAST_FLOAT *)
        mov     rcx, DCTSIZE/4

        vmovaps  ymm0, YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm1, YMMWORD [YMMBLOCK(1,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm2, YMMWORD [YMMBLOCK(2,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm3, YMMWORD [YMMBLOCK(3,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm4, YMMWORD [YMMBLOCK(4,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm5, YMMWORD [YMMBLOCK(5,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm6, YMMWORD [YMMBLOCK(6,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm7, YMMWORD [YMMBLOCK(7,0,rdx,SIZEOF_FAST_FLOAT)]

        jmp near .rowloop
        nop

.columnloop:
.rowloop:

        vunpcklps ymm10,ymm2,ymm3
        vunpckhps ymm11,ymm2,ymm3
        vunpcklps ymm8,ymm0,ymm1
        vunpckhps ymm9,ymm0,ymm1
        vunpcklpd ymm0,ymm8,ymm10
        vunpckhpd ymm2,ymm8,ymm10
        vunpcklpd ymm1,ymm9,ymm11
        vunpckhpd ymm3,ymm9,ymm11

        vunpcklps ymm14,ymm6,ymm7
        vunpckhps ymm15,ymm6,ymm7
        vunpcklps ymm12,ymm4,ymm5
        vunpckhps ymm13,ymm4,ymm5
        vunpcklpd ymm4,ymm12,ymm14
        vunpckhpd ymm6,ymm12,ymm14
        vunpcklpd ymm5,ymm13,ymm15
        vunpckhpd ymm7,ymm13,ymm15
        vperm2f128 ymm8,ymm0,ymm4,0x20 
        vperm2f128 ymm12,ymm0,ymm4,0x31
        vperm2f128 ymm10,ymm1,ymm5,0x20
        vperm2f128 ymm14,ymm1,ymm5,0x31
        vperm2f128 ymm9,ymm2,ymm6,0x20
        vperm2f128 ymm13,ymm2,ymm6,0x31
        vperm2f128 ymm11,ymm3,ymm7,0x20
        vperm2f128 ymm15,ymm3,ymm7,0x31

        vaddps ymm0,ymm8,ymm15
        vsubps ymm7,ymm8,ymm15
        vaddps ymm1,ymm9,ymm14
        vsubps ymm8,ymm9,ymm14
        vaddps ymm2,ymm10,ymm13
        vsubps ymm5,ymm10,ymm13
        vaddps ymm3,ymm11,ymm12
        vsubps ymm9,ymm11,ymm12

        vaddps ymm10, ymm0,ymm3
        vsubps ymm13, ymm0,ymm3
        vaddps ymm11, ymm1,ymm2
        vsubps ymm12, ymm1,ymm2

        vaddps ymm0, ymm10,ymm11 
        vsubps ymm4, ymm10,ymm11 

        vaddps ymm10,ymm12,ymm13
        vmulps ymm12,ymm10,[rel PD_0_707]
        vaddps ymm2, ymm13,ymm12
        vsubps ymm6, ymm13,ymm12

        vaddps ymm12, ymm9, ymm5
        vaddps ymm13, ymm5, ymm8
        vaddps ymm14, ymm8, ymm7
        
        vsubps ymm15, ymm12, ymm14
        vmulps ymm8, ymm15, [rel PD_0_382] 
        vmulps ymm12,ymm12,[rel PD_0_541]
        vaddps ymm12,ymm12,ymm8
        vmulps ymm14,ymm14,[rel PD_1_306]
        vaddps ymm14,ymm14,ymm8


        vmulps ymm15,ymm13,[rel PD_0_707]

        vaddps ymm8,ymm7,ymm15 
        vsubps ymm1,ymm7,ymm15 
        
        vaddps ymm5,ymm1,ymm12 
        vsubps ymm3,ymm1,ymm12
        vaddps ymm1,ymm8,ymm14
        vsubps ymm7,ymm8,ymm14

        ; ---- Pass 2: process columns.

        dec     rcx
        jnz     near .columnloop

        vmovaps   YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FAST_FLOAT)],ymm0
        vmovaps   YMMWORD [YMMBLOCK(1,0,rdx,SIZEOF_FAST_FLOAT)],ymm1
        vmovaps   YMMWORD [YMMBLOCK(2,0,rdx,SIZEOF_FAST_FLOAT)],ymm2
        vmovaps   YMMWORD [YMMBLOCK(3,0,rdx,SIZEOF_FAST_FLOAT)],ymm3
        vmovaps   YMMWORD [YMMBLOCK(4,0,rdx,SIZEOF_FAST_FLOAT)],ymm4
        vmovaps   YMMWORD [YMMBLOCK(5,0,rdx,SIZEOF_FAST_FLOAT)],ymm5
        vmovaps   YMMWORD [YMMBLOCK(6,0,rdx,SIZEOF_FAST_FLOAT)],ymm6
        vmovaps   YMMWORD [YMMBLOCK(7,0,rdx,SIZEOF_FAST_FLOAT)],ymm7
        
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32
