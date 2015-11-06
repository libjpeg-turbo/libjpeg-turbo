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

        alignz  32
; --------------------------------------------------------------------------

%define RECIPROCAL(m,n,b) YMMBLOCK(DCTSIZE*0+(m),(n),(b),2*SIZEOF_DCTELEM)
%define CORRECTION(m,n,b) YMMBLOCK(DCTSIZE*1+(m),(n),(b),2*SIZEOF_DCTELEM)
%define SCALE(m,n,b)      YMMBLOCK(DCTSIZE*2+(m),(n),(b),2*SIZEOF_DCTELEM)

; --------------------------------------------------------------------------

%define CONST_BITS      8       ; 14 is also OK.

%if CONST_BITS == 8
F_0_382 equ      98             ; FIX(0.382683433)
F_0_541 equ     139             ; FIX(0.541196100)
F_0_707 equ     181             ; FIX(0.707106781)
F_1_306 equ     334             ; FIX(1.306562965)
%else
; NASM cannot do compile-time arithmetic on floating-point constants.
%define DESCALE(x,n)  (((x)+(1<<((n)-1)))>>(n))
F_0_382 equ     DESCALE( 410903207,30-CONST_BITS)       ; FIX(0.382683433)
F_0_541 equ     DESCALE( 581104887,30-CONST_BITS)       ; FIX(0.541196100)
F_0_707 equ     DESCALE( 759250124,30-CONST_BITS)       ; FIX(0.707106781)
F_1_306 equ     DESCALE(1402911301,30-CONST_BITS)       ; FIX(1.306562965)
%endif
        SECTION SEG_CONST

%define PRE_MULTIPLY_SCALE_BITS   2
%define CONST_SHIFT     (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)
        alignz  32
        global  EXTN(jconst_conv_fdct_quan_ifast_avx2)

EXTN(jconst_fdct_merged_ifast_avx2):

PW_F0707        times 16 dw  F_0_707 << CONST_SHIFT
PW_F0382        times 16 dw  F_0_382 << CONST_SHIFT
PW_F0541        times 16 dw  F_0_541 << CONST_SHIFT
PW_F1306        times 16 dw  F_1_306 << CONST_SHIFT

;
        SECTION SEG_TEXT
        BITS    64

; --------------------------------------------------------------------------
        align   32
        global  EXTN(jsimd_fdct_merged_ifast_avx2)

EXTN(jsimd_fdct_merged_ifast_avx2):
        push    rbp
        mov     rbp,rsp

		push rbx
        collect_args

.convsamp:
		mov rsi, r10
        mov rax, r11

        vpxor    ymm14,ymm14,ymm14               
        vpcmpeqw ymm15,ymm15,ymm15
        vpsllw   ymm15,ymm15,7                  
.convloop:
        mov     rbx, JSAMPROW [rsi+0*SIZEOF_JSAMPROW]   
        mov rdx, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]       

        vmovdqu    xmm0, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm0, ymm0, 0x10
        vmovdqu    xmm1, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm1, ymm1, 0x10

        mov     rbx, JSAMPROW [rsi+2*SIZEOF_JSAMPROW]   
        mov     rdx, JSAMPROW [rsi+3*SIZEOF_JSAMPROW]   

        vmovdqu    xmm2, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm2, ymm2, 0x10
        vmovdqu    xmm3, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm3, ymm3, 0x10

        vpunpcklbw ymm0,ymm0,ymm14             
        vpunpcklbw ymm1,ymm1,ymm14             
        vpaddw     ymm0,ymm0,ymm15
        vpaddw     ymm1,ymm1,ymm15
        vpunpcklbw ymm2,ymm2,ymm14             
        vpunpcklbw ymm3,ymm3,ymm14             
        vpaddw     ymm2,ymm2,ymm15
        vpaddw     ymm3,ymm3,ymm15

        mov     rbx, JSAMPROW [rsi+4*SIZEOF_JSAMPROW]   
        mov     rdx, JSAMPROW [rsi+5*SIZEOF_JSAMPROW]       

        vmovdqu    xmm8, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm8, ymm8, 0x10
        vmovdqu    xmm9, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm9, ymm9, 0x10

        mov     rbx, JSAMPROW [rsi+6*SIZEOF_JSAMPROW]   
        mov     rdx, JSAMPROW [rsi+7*SIZEOF_JSAMPROW]   

        vmovdqu    xmm10, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm10, ymm10, 0x10
        vmovdqu    xmm11, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
		vpermq	   ymm11, ymm11, 0x10

        vpunpcklbw ymm8,ymm8,ymm14             
        vpunpcklbw ymm9,ymm9,ymm14             
        vpaddw     ymm4,ymm8,ymm15
        vpaddw     ymm5,ymm9,ymm15
        vpunpcklbw ymm10,ymm10,ymm14             
        vpunpcklbw ymm11,ymm11,ymm14             
        vpaddw     ymm6,ymm10,ymm15
        vpaddw     ymm7,ymm11,ymm15

.dct:

        mov     rcx, DCTSIZE/4
.dctloop:
		vpunpcklwd ymm12,ymm2,ymm6
		vpunpckhwd ymm13,ymm2,ymm6
		vpunpcklwd ymm8,ymm0,ymm4
		vpunpckhwd ymm9,ymm0,ymm4
		vpunpcklwd ymm10,ymm1,ymm5
		vpunpckhwd ymm11,ymm1,ymm5
		vpunpcklwd ymm14,ymm3,ymm7
		vpunpckhwd ymm15,ymm3,ymm7

		vpunpcklwd ymm0,ymm8,ymm12
		vpunpckhwd ymm1,ymm8,ymm12
		vpunpcklwd ymm2,ymm9,ymm13
		vpunpckhwd ymm3,ymm9,ymm13
		vpunpcklwd ymm4,ymm10,ymm14
		vpunpckhwd ymm5,ymm10,ymm14
		vpunpcklwd ymm6,ymm11,ymm15
		vpunpckhwd ymm7,ymm11,ymm15

		vpunpcklwd ymm8,ymm0,ymm4
		vpunpckhwd ymm9,ymm0,ymm4
		vpunpcklwd ymm10,ymm1,ymm5
		vpunpckhwd ymm11,ymm1,ymm5
		vpunpcklwd ymm12,ymm2,ymm6
		vpunpckhwd ymm13,ymm2,ymm6
		vpunpcklwd ymm14,ymm3,ymm7
		vpunpckhwd ymm15,ymm3,ymm7

		;
		vpaddw ymm0,ymm8,ymm15
		vpsubw ymm7,ymm8,ymm15
		vpaddw ymm1,ymm9,ymm14
		vpsubw ymm8,ymm9,ymm14
		vpaddw ymm2,ymm10,ymm13
		vpsubw ymm5,ymm10,ymm13
		vpaddw ymm3,ymm11,ymm12
		vpsubw ymm9,ymm11,ymm12

		vpaddw ymm10, ymm0,ymm3
		vpsubw ymm13, ymm0,ymm3
		vpaddw ymm11, ymm1,ymm2
		vpsubw ymm12, ymm1,ymm2

		vpaddw ymm0, ymm10,ymm11 
		vpsubw ymm4, ymm10,ymm11 

		vpaddw ymm10,ymm12,ymm13
		vpsllw  ymm10,ymm10,PRE_MULTIPLY_SCALE_BITS
		vpmulhw ymm12, ymm10,[rel PW_F0707]
		vpaddw ymm2, ymm13,ymm12
		vpsubw ymm6, ymm13,ymm12

		;/*Odd part*/
		vpaddw ymm12, ymm9, ymm5
		vpaddw ymm13, ymm5, ymm8
		vpaddw ymm14, ymm8, ymm7
		
		vpsubw ymm15, ymm12, ymm14

		vpsllw  ymm15,ymm15,PRE_MULTIPLY_SCALE_BITS
		vpsllw  ymm12,ymm12,PRE_MULTIPLY_SCALE_BITS
		vpsllw  ymm14,ymm14,PRE_MULTIPLY_SCALE_BITS

		vpmulhw ymm8, ymm15,[rel PW_F0382]
		vpmulhw ymm12, ymm12,[rel PW_F0541]
		vpaddw ymm12,ymm12,ymm8
		vpmulhw ymm14, ymm14,[rel PW_F1306]
		vpaddw ymm14,ymm14,ymm8

		vpsllw  ymm13,ymm13,PRE_MULTIPLY_SCALE_BITS
		vpmulhw ymm15, ymm13,[rel PW_F0707]

		vpaddw ymm8,ymm7,ymm15 
		vpsubw ymm1,ymm7,ymm15 
		
		vpaddw ymm5,ymm1,ymm12 
		vpsubw ymm3,ymm1,ymm12 
		vpaddw ymm1,ymm8,ymm14 
		vpsubw ymm7,ymm8,ymm14 

	 	dec     rcx
        jnz     near .dctloop


.quantize:
        mov rdx, r13
        mov rdi, r12


        vpsraw   ymm8,ymm0,(WORD_BIT-1)
        vpsraw   ymm9,ymm1,(WORD_BIT-1)
        vpsraw   ymm10,ymm2,(WORD_BIT-1)
        vpsraw   ymm11,ymm3,(WORD_BIT-1)
        vpxor    ymm0,ymm0,ymm8
        vpxor    ymm1,ymm1,ymm9
        vpxor    ymm2,ymm2,ymm10
        vpxor    ymm3,ymm3,ymm11
        vpsubw   ymm0,ymm0,ymm8               
        vpsubw   ymm1,ymm1,ymm9               
        vpsubw   ymm2,ymm2,ymm10               
        vpsubw   ymm3,ymm3,ymm11               

        vpaddw   ymm0,ymm0, YMMWORD [CORRECTION(0,0,rdx)]  
        vpaddw   ymm1,ymm1, YMMWORD [CORRECTION(1,0,rdx)]
        vpaddw   ymm2,ymm2, YMMWORD [CORRECTION(2,0,rdx)]
        vpaddw   ymm3,ymm3, YMMWORD [CORRECTION(3,0,rdx)]
        vpmulhuw ymm0,ymm0, YMMWORD [RECIPROCAL(0,0,rdx)]  
        vpmulhuw ymm1,ymm1, YMMWORD [RECIPROCAL(1,0,rdx)]
        vpmulhuw ymm2,ymm2, YMMWORD [RECIPROCAL(2,0,rdx)]
        vpmulhuw ymm3,ymm3, YMMWORD [RECIPROCAL(3,0,rdx)]
        vpmulhuw ymm0,ymm0, YMMWORD [SCALE(0,0,rdx)]  
        vpmulhuw ymm1,ymm1, YMMWORD [SCALE(1,0,rdx)]
        vpmulhuw ymm2,ymm2, YMMWORD [SCALE(2,0,rdx)]
        vpmulhuw ymm3,ymm3, YMMWORD [SCALE(3,0,rdx)]

        vpxor    ymm0,ymm0,ymm8
        vpxor    ymm1,ymm1,ymm9
        vpxor    ymm2,ymm2,ymm10
        vpxor    ymm3,ymm3,ymm11
        vpsubw   ymm0,ymm0,ymm8
        vpsubw   ymm1,ymm1,ymm9
        vpsubw   ymm2,ymm2,ymm10
        vpsubw   ymm3,ymm3,ymm11

        vpsraw   ymm12,ymm4,(WORD_BIT-1)
        vpsraw   ymm13,ymm5,(WORD_BIT-1)
        vpsraw   ymm14,ymm6,(WORD_BIT-1)
        vpsraw   ymm15,ymm7,(WORD_BIT-1)
        vpxor    ymm8,ymm4,ymm12
        vpxor    ymm9,ymm5,ymm13
        vpxor    ymm10,ymm6,ymm14
        vpxor    ymm11,ymm7,ymm15
        vpsubw   ymm8,ymm8,ymm12 
        vpsubw   ymm9,ymm9,ymm13
        vpsubw   ymm10,ymm10,ymm14
        vpsubw   ymm11,ymm11,ymm15

        vpaddw   ymm8,ymm8, YMMWORD [CORRECTION(4,0,rdx)]  
        vpaddw   ymm9,ymm9, YMMWORD [CORRECTION(5,0,rdx)]
        vpaddw   ymm10,ymm10, YMMWORD [CORRECTION(6,0,rdx)]
        vpaddw   ymm11,ymm11, YMMWORD [CORRECTION(7,0,rdx)]
        vpmulhuw ymm8,ymm8, YMMWORD [RECIPROCAL(4,0,rdx)]  
        vpmulhuw ymm9,ymm9, YMMWORD [RECIPROCAL(5,0,rdx)]
        vpmulhuw ymm10,ymm10, YMMWORD [RECIPROCAL(6,0,rdx)]
        vpmulhuw ymm11,ymm11, YMMWORD [RECIPROCAL(7,0,rdx)]
        vpmulhuw ymm8,ymm8, YMMWORD [SCALE(4,0,rdx)]  
        vpmulhuw ymm9,ymm9, YMMWORD [SCALE(5,0,rdx)]
        vpmulhuw ymm10,ymm10, YMMWORD [SCALE(6,0,rdx)]
        vpmulhuw ymm11,ymm11, YMMWORD [SCALE(7,0,rdx)]

        vpxor    ymm8,ymm8,ymm12
        vpxor    ymm9,ymm9,ymm13
        vpxor    ymm10,ymm10,ymm14
        vpxor    ymm11,ymm11,ymm15
        vpsubw   ymm4,ymm8,ymm12
        vpsubw   ymm5,ymm9,ymm13
        vpsubw   ymm6,ymm10,ymm14
        vpsubw   ymm7,ymm11,ymm15

		vperm2i128 ymm8,ymm0,ymm1,0x20
		vperm2i128 ymm9,ymm2,ymm3,0x20
		vperm2i128 ymm10,ymm4,ymm5,0x20
		vperm2i128 ymm11,ymm6,ymm7,0x20

		vperm2i128 ymm0,ymm0,ymm1,0x31
		vperm2i128 ymm1,ymm2,ymm3,0x31
		vperm2i128 ymm2,ymm4,ymm5,0x31
		vperm2i128 ymm3,ymm6,ymm7,0x31

		vmovdqu  YMMWORD [YMMBLOCK(0,0,rdi,2*SIZEOF_DCTELEM)], ymm8
        vmovdqu  YMMWORD [YMMBLOCK(1,0,rdi,2*SIZEOF_DCTELEM)], ymm9
        vmovdqu  YMMWORD [YMMBLOCK(2,0,rdi,2*SIZEOF_DCTELEM)], ymm10
        vmovdqu  YMMWORD [YMMBLOCK(3,0,rdi,2*SIZEOF_DCTELEM)], ymm11
		vmovdqu  YMMWORD [YMMBLOCK(4,0,rdi,2*SIZEOF_DCTELEM)], ymm0
        vmovdqu  YMMWORD [YMMBLOCK(5,0,rdi,2*SIZEOF_DCTELEM)], ymm1
        vmovdqu  YMMWORD [YMMBLOCK(6,0,rdi,2*SIZEOF_DCTELEM)], ymm2
        vmovdqu  YMMWORD [YMMBLOCK(7,0,rdi,2*SIZEOF_DCTELEM)], ymm3

.exit:


		uncollect_args
		pop     rbx
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32

