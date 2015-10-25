;
; jfdctflt.asm - floating-point FDCT (64-bit AVX2)
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
        SECTION SEG_TEXT
        BITS    64

        alignz  32
        global  EXTN(jconst_fdct_merged_float_avx2)

EXTN(jconst_fdct_merged_float_avx2):

PD_0_382        times 8 dd  0.382683432365089771728460
PD_0_707        times 8 dd  0.707106781186547524400844
PD_0_541        times 8 dd  0.541196100146196984399723
PD_1_306        times 8 dd  1.306562964876376527856643

;
        align   32
        global  EXTN(jsimd_fdct_merged_float_avx2)

EXTN(jsimd_fdct_merged_float_avx2):
        push    rbp
        mov     rbp,rsp

        push rbx
        collect_args

.convsamp:
        mov rsi, r10
        mov rax, r11
        vpcmpeqw  ymm15,ymm15,ymm15
        vpsllw    ymm15,ymm15,7
        vpacksswb ymm15,ymm15,ymm15

        mov     rbx, JSAMPROW [rsi+0*SIZEOF_JSAMPROW]   ; (JSAMPLE *)
        mov rdx, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]       ; (JSAMPLE *)
        vmovq    xmm0, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]
        vmovq    xmm1, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]

        vperm2i128 ymm9, ymm0,ymm1,0x20
        vpsubb ymm0,ymm9,ymm15

        vpunpcklbw ymm1,ymm0,ymm0 
        vpermq ymm8,ymm1,0xd8
        vpunpcklwd ymm0,ymm1,ymm8 
        vpunpckhwd ymm1,ymm9,ymm8 
        
        vpsrad ymm8,ymm0,(DWORD_BIT-BYTE_BIT) 
        vpsrad ymm9,ymm1,(DWORD_BIT-BYTE_BIT)
        vcvtdq2ps ymm0,ymm8
        vcvtdq2ps ymm1,ymm9

        mov     rbx, JSAMPROW [rsi+2*SIZEOF_JSAMPROW]   ; (JSAMPLE *)
        mov rdx, JSAMPROW [rsi+3*SIZEOF_JSAMPROW]       ; (JSAMPLE *)

        vmovq    xmm2, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]
        vmovq    xmm3, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]

        vperm2i128 ymm11, ymm2,ymm3,0x20
        vpsubb ymm2,ymm11,ymm15

        vpunpcklbw ymm3,ymm2,ymm2
        vpermq ymm10,ymm3,0xd8
        vpunpcklwd ymm2,ymm3,ymm10 
        vpunpckhwd ymm3,ymm11,ymm10 
        
        vpsrad ymm10,ymm2,(DWORD_BIT-BYTE_BIT) 
        vpsrad ymm11,ymm3,(DWORD_BIT-BYTE_BIT)
        vcvtdq2ps ymm2,ymm10
        vcvtdq2ps ymm3,ymm11

        mov     rbx, JSAMPROW [rsi+4*SIZEOF_JSAMPROW]   ; (JSAMPLE *)
        mov rdx, JSAMPROW [rsi+5*SIZEOF_JSAMPROW]       ; (JSAMPLE *)

        vmovq    xmm4, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]
        vmovq    xmm5, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]

        vperm2i128 ymm13, ymm4,ymm5,0x20
        vpsubb ymm4,ymm13,ymm15

        vpunpcklbw ymm5,ymm4,ymm4
        vpermq ymm12,ymm5,0xd8
        vpunpcklwd ymm4,ymm5,ymm12 
        vpunpckhwd ymm5,ymm13,ymm12 
        
        vpsrad ymm12,ymm4,(DWORD_BIT-BYTE_BIT) 
        vpsrad ymm13,ymm5,(DWORD_BIT-BYTE_BIT)
        vcvtdq2ps ymm4,ymm12
        vcvtdq2ps ymm5,ymm13

        mov     rbx, JSAMPROW [rsi+6*SIZEOF_JSAMPROW]   ; (JSAMPLE *)
        mov     rdx, JSAMPROW [rsi+7*SIZEOF_JSAMPROW]       ; (JSAMPLE *)

        vmovq    xmm6, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]
        vmovq    xmm7, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]

        vperm2i128 ymm9, ymm6,ymm7,0x20
        vpsubb ymm6,ymm9,ymm15

        vpunpcklbw ymm7,ymm6,ymm6 
        vpermq ymm14,ymm7,0xd8
        vpunpcklwd ymm6,ymm7,ymm14 
        vpunpckhwd ymm7,ymm9,ymm14 
        
        vpsrad ymm14,ymm6,(DWORD_BIT-BYTE_BIT) 
        vpsrad ymm9,ymm7,(DWORD_BIT-BYTE_BIT)
        vcvtdq2ps ymm6,ymm14
        vcvtdq2ps ymm7,ymm9

.dct:

        mov     rcx, DCTSIZE/4
.dctloop:
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

        ;/*Odd part*/
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
        jnz     near .dctloop
.quantize:
        mov rdx, r13
        mov rdi, r12

        vmulps   ymm0,ymm0, YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FAST_FLOAT)]
        vmulps   ymm1,ymm1, YMMWORD [YMMBLOCK(1,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm0,ymm0
        vcvtps2dq ymm1,ymm1

        vpackssdw ymm1,ymm0,ymm1
        vpermq ymm0,ymm1,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(0,0,rdi,2*SIZEOF_JCOEF)], ymm0
        vmulps   ymm2,ymm2, YMMWORD [YMMBLOCK(2,0,rdx,SIZEOF_FAST_FLOAT)]
        vmulps   ymm3,ymm3, YMMWORD [YMMBLOCK(3,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm2,ymm2
        vcvtps2dq ymm3,ymm3

        vpackssdw ymm3,ymm2,ymm3
        vpermq ymm2,ymm3,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(1,0,rdi,2*SIZEOF_JCOEF)], ymm2
        vmulps   ymm4,ymm4, YMMWORD [YMMBLOCK(4,0,rdx,SIZEOF_FAST_FLOAT)]
        vmulps   ymm5,ymm5, YMMWORD [YMMBLOCK(5,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm4,ymm4
        vcvtps2dq ymm5,ymm5

        vpackssdw ymm5,ymm4,ymm5
        vpermq ymm4,ymm5,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(2,0,rdi,2*SIZEOF_JCOEF)], ymm4

        vmulps   ymm6,ymm6, YMMWORD [YMMBLOCK(6,0,rdx,SIZEOF_FAST_FLOAT)]
        vmulps   ymm7,ymm7, YMMWORD [YMMBLOCK(7,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm6,ymm6
        vcvtps2dq ymm7,ymm7

        vpackssdw ymm7,ymm6,ymm7
        vpermq ymm6,ymm7,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(3,0,rdi,2*SIZEOF_JCOEF)], ymm6

        uncollect_args
        pop     rbx
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32



