;
; jquantf.asm - sample data conversion and quantization (64-bit AVX & AVX2) ;
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
; [TAB8]

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Load data into workspace, applying unsigned->signed conversion
;
; GLOBAL(void)
; jsimd_convsamp_float_avx2 (JSAMPARRAY sample_data, JDIMENSION start_col,
;                            FAST_FLOAT * workspace);
;

; r10 = JSAMPARRAY sample_data
; r11 = JDIMENSION start_col
; r12 = FAST_FLOAT * workspace

        align   32
        global  EXTN(jsimd_convsamp_float_avx2)

EXTN(jsimd_convsamp_float_avx2):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args
        push    rbx


        vpcmpeqw  ymm15,ymm15,ymm15
        vpsllw    ymm15,ymm15,7
        vpacksswb ymm15,ymm15,ymm15              

        mov rsi, r10
        mov     rax, r11
        mov rdi, r12
.convloop:
        mov     rbx, JSAMPROW [rsi+0*SIZEOF_JSAMPROW]   
        mov rdx, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]       

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

        mov     rbx, JSAMPROW [rsi+2*SIZEOF_JSAMPROW]   
        mov rdx, JSAMPROW [rsi+3*SIZEOF_JSAMPROW]       

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

        mov     rbx, JSAMPROW [rsi+4*SIZEOF_JSAMPROW]   
        mov rdx, JSAMPROW [rsi+5*SIZEOF_JSAMPROW]       

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

        mov     rbx, JSAMPROW [rsi+6*SIZEOF_JSAMPROW]   
        mov rdx, JSAMPROW [rsi+7*SIZEOF_JSAMPROW]       

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


        vmovaps  YMMWORD [YMMBLOCK(0,0,rdi,SIZEOF_FAST_FLOAT)], ymm0
        vmovaps  YMMWORD [YMMBLOCK(1,0,rdi,SIZEOF_FAST_FLOAT)], ymm1

        vmovaps  YMMWORD [YMMBLOCK(2,0,rdi,SIZEOF_FAST_FLOAT)], ymm2
        vmovaps  YMMWORD [YMMBLOCK(3,0,rdi,SIZEOF_FAST_FLOAT)], ymm3

        vmovaps  YMMWORD [YMMBLOCK(4,0,rdi,SIZEOF_FAST_FLOAT)], ymm4
        vmovaps  YMMWORD [YMMBLOCK(5,0,rdi,SIZEOF_FAST_FLOAT)], ymm5

        vmovaps  YMMWORD [YMMBLOCK(6,0,rdi,SIZEOF_FAST_FLOAT)], ymm6
        vmovaps  YMMWORD [YMMBLOCK(7,0,rdi,SIZEOF_FAST_FLOAT)], ymm7

        pop     rbx
        uncollect_args
        pop     rbp
        ret


; --------------------------------------------------------------------------
;
; Quantize/descale the coefficients, and store into coef_block
;
; GLOBAL(void)
; jsimd_quantize_float_avx2 (JCOEFPTR coef_block, FAST_FLOAT * divisors,
;                         FAST_FLOAT * workspace);
;

; r10 = JCOEFPTR coef_block
; r11 = FAST_FLOAT * divisors
; r12 = FAST_FLOAT * workspace

        align   32
        global  EXTN(jsimd_quantize_float_avx2)

EXTN(jsimd_quantize_float_avx2):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args

        mov rsi, r12
        mov rdx, r11
        mov rdi, r10
.quantloop:
            vmovaps  ymm0, YMMWORD [YMMBLOCK(0,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm0,ymm0, YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm1, YMMWORD [YMMBLOCK(1,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm1,ymm1, YMMWORD [YMMBLOCK(1,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm0,ymm0
        vcvtps2dq ymm1,ymm1

        vpackssdw ymm1,ymm0,ymm1
        vpermq ymm0,ymm1,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(0,0,rdi,2*SIZEOF_JCOEF)], ymm0
        vmovaps  ymm2, YMMWORD [YMMBLOCK(2,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm2,ymm2, YMMWORD [YMMBLOCK(2,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm3, YMMWORD [YMMBLOCK(3,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm3,ymm3, YMMWORD [YMMBLOCK(3,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm2,ymm2
        vcvtps2dq ymm3,ymm3

        vpackssdw ymm3,ymm2,ymm3
        vpermq ymm2,ymm3,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(1,0,rdi,2*SIZEOF_JCOEF)], ymm2


        vmovaps  ymm4, YMMWORD [YMMBLOCK(4,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm4,ymm4, YMMWORD [YMMBLOCK(4,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm5, YMMWORD [YMMBLOCK(5,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm5,ymm5, YMMWORD [YMMBLOCK(5,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm4,ymm4
        vcvtps2dq ymm5,ymm5

        vpackssdw ymm5,ymm4,ymm5
        vpermq ymm4,ymm5,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(2,0,rdi,2*SIZEOF_JCOEF)], ymm4
        vmovaps  ymm6, YMMWORD [YMMBLOCK(6,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm6,ymm6, YMMWORD [YMMBLOCK(6,0,rdx,SIZEOF_FAST_FLOAT)]
        vmovaps  ymm7, YMMWORD [YMMBLOCK(7,0,rsi,SIZEOF_FAST_FLOAT)]
        vmulps   ymm7,ymm7, YMMWORD [YMMBLOCK(7,0,rdx,SIZEOF_FAST_FLOAT)]

        vcvtps2dq ymm6,ymm6
        vcvtps2dq ymm7,ymm7

        vpackssdw ymm7,ymm6,ymm7
        vpermq ymm6,ymm7,0xD8

        vmovdqa  YMMWORD [YMMBLOCK(3,0,rdi,2*SIZEOF_JCOEF)], ymm6

        uncollect_args
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32
