;
; jidctfst.asm - fast integer IDCT (64-bit AVX2)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009 D. R. Commander
; Copyright (C) 2015, Intel Corporation
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
; This file contains a fast, not so accurate integer implementation of
; the inverse DCT (Discrete Cosine Transform). The following code is
; based directly on the IJG's original jidctfst.c; see the jidctfst.c
; for more details.
;
; [TAB8]

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------

%define CONST_BITS      8       ; 14 is also OK.
%define PASS1_BITS      2

%if IFAST_SCALE_BITS != PASS1_BITS
%error "'IFAST_SCALE_BITS' must be equal to 'PASS1_BITS'."
%endif

%if CONST_BITS == 8
F_1_082 equ     277             ; FIX(1.082392200)
F_1_414 equ     362             ; FIX(1.414213562)
F_1_847 equ     473             ; FIX(1.847759065)
F_2_613 equ     669             ; FIX(2.613125930)
F_1_613 equ     (F_2_613 - 256) ; FIX(2.613125930) - FIX(1)
%else
; NASM cannot do compile-time arithmetic on floating-point constants.
%define DESCALE(x,n)  (((x)+(1<<((n)-1)))>>(n))
F_1_082 equ     DESCALE(1162209775,30-CONST_BITS)       ; FIX(1.082392200)
F_1_414 equ     DESCALE(1518500249,30-CONST_BITS)       ; FIX(1.414213562)
F_1_847 equ     DESCALE(1984016188,30-CONST_BITS)       ; FIX(1.847759065)
F_2_613 equ     DESCALE(2805822602,30-CONST_BITS)       ; FIX(2.613125930)
F_1_613 equ     (F_2_613 - (1 << CONST_BITS))   ; FIX(2.613125930) - FIX(1)
%endif

; --------------------------------------------------------------------------
        SECTION SEG_CONST

; PRE_MULTIPLY_SCALE_BITS <= 2 (to avoid overflow)
; CONST_BITS + CONST_SHIFT + PRE_MULTIPLY_SCALE_BITS == 16 (for pmulhw)

%define PRE_MULTIPLY_SCALE_BITS   2
%define CONST_SHIFT     (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)

        alignz  32
        global  EXTN(jconst_idct_ifast_avx2)

EXTN(jconst_idct_ifast_avx2):

PW_F1414        times 16 dw  F_1_414 << CONST_SHIFT
PW_F1847        times 16 dw  F_1_847 << CONST_SHIFT
PW_MF1613       times 16 dw -F_1_613 << CONST_SHIFT
PW_F1082        times 16 dw  F_1_082 << CONST_SHIFT
PB_CENTERJSAMP  times 32 db CENTERJSAMPLE

        alignz  32

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Perform dequantization and inverse DCT on one block of coefficients.
;
; GLOBAL(void)
; jsimd_idct_ifast_avx2 (void * dct_table, JCOEFPTR coef_block,
;                       JSAMPARRAY output_buf, JDIMENSION output_col)
;

; r10 = jpeg_component_info * compptr
; r11 = JCOEFPTR coef_block
; r12 = JSAMPARRAY output_buf
; r13 = JDIMENSION output_col

%define original_rbp    rbp+0
%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_YMMWORD ; ymmword wk[WK_NUM]
%define WK_NUM          2

        align   32
        global  EXTN(jsimd_idct_ifast_avx2)

EXTN(jsimd_idct_ifast_avx2):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_YMMWORD)     ; align to 128 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        collect_args

        ; ---- Pass 1: process columns from input.

        mov     rdx, r10                ; quantptr
        mov     rsi, r11                ; inptr

%ifndef NO_ZERO_COLUMN_TEST_IFAST_AVX2
        mov     eax, DWORD [DWBLOCK(1,0,rsi,SIZEOF_JCOEF)]
        or      eax, DWORD [DWBLOCK(2,0,rsi,SIZEOF_JCOEF)]
        or      eax, DWORD [DWBLOCK(9,0,rsi,SIZEOF_JCOEF)]
        or      eax, DWORD [DWBLOCK(10,0,rsi,SIZEOF_JCOEF)]
        jnz     near .columnDCT

        vmovdqa  xmm0, XMMWORD [XMMBLOCK(1,0,rsi,SIZEOF_JCOEF)]
        vmovdqa  xmm1, XMMWORD [XMMBLOCK(9,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm0, YMMWORD [YMMBLOCK(2,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm1, YMMWORD [YMMBLOCK(10,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm0, YMMWORD [YMMBLOCK(4,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm1, YMMWORD [YMMBLOCK(12,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm0, YMMWORD [YMMBLOCK(6,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm1, YMMWORD [YMMBLOCK(14,0,rsi,SIZEOF_JCOEF)]

        vpor     ymm1,ymm0
        vpacksswb ymm1,ymm1,ymm1
        vpermq  ymm1,ymm1,0xD8 
        vpacksswb xmm1,xmm1,xmm1
        vpacksswb xmm1,xmm1,xmm1

        vmovd    eax,xmm1
        test    rax,rax
        jnz     short .columnDCT

        ; -- AC terms all zero

        vmovdqa  xmm0, XMMWORD [XMMBLOCK(0,0,rsi,SIZEOF_JCOEF)]
        vmovdqa  xmm1, XMMWORD [XMMBLOCK(8,0,rsi,SIZEOF_JCOEF)]
        vperm2i128  ymm0,ymm0,ymm1,0x20

        vmovdqa xmm1, XMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_IFAST_MULT_TYPE)]
        vperm2i128  ymm1,ymm1,ymm1,0
        vpmullw  ymm0, ymm0,ymm1

        vpunpckhwd ymm7,ymm0,ymm0             
        vpunpcklwd ymm0,ymm0,ymm0             

        vpshufd  ymm6,ymm0,0x00          
        vpshufd  ymm2,ymm0,0x55          
        vpshufd  ymm5,ymm0,0xAA          
        vpshufd  ymm0,ymm0,0xFF          
        vpshufd  ymm1,ymm7,0x00          
        vpshufd  ymm4,ymm7,0x55          
        vpshufd  ymm3,ymm7,0xAA          
        vpshufd  ymm7,ymm7,0xFF          

        vmovdqa  YMMWORD [wk(0)], ymm2   
        vmovdqa  YMMWORD [wk(1)], ymm0   
        jmp     near .column_end
%endif
.columnDCT:

        ; -- Even part
        vmovdqa  ymm14, YMMWORD [YMMBLOCK(0,0,rsi,SIZEOF_JCOEF)]
        vmovdqa  ymm15, YMMWORD [YMMBLOCK(8,0,rsi,SIZEOF_JCOEF)]
        vperm2i128  ymm0,ymm14,ymm15, 0x20
        vperm2i128  ymm8,ymm14,ymm15, 0x31
        vmovdqa  ymm14, [YMMBLOCK(0,0,rdx,SIZEOF_IFAST_MULT_TYPE)]
        vperm2i128  ymm12,ymm14,ymm14,0x11
        vperm2i128  ymm14,ymm14,ymm14,0
        vpmullw ymm0,ymm0,ymm14

        vmovdqa  ymm14, YMMWORD [YMMBLOCK(2,0,rsi,SIZEOF_JCOEF)]
        vmovdqa  ymm15, YMMWORD [YMMBLOCK(10,0,rsi,SIZEOF_JCOEF)]
        vperm2i128  ymm1,ymm14,ymm15, 0x20
        vperm2i128  ymm9,ymm14,ymm15, 0x31
        vmovdqa  ymm14, [YMMBLOCK(2,0,rdx,SIZEOF_IFAST_MULT_TYPE)]
        vperm2i128  ymm13,ymm14,ymm14,0x11
        vperm2i128  ymm14,ymm14,ymm14,0
        vpmullw ymm1,ymm1,ymm14

        vmovdqa  ymm14, YMMWORD [YMMBLOCK(4,0,rsi,SIZEOF_JCOEF)]
        vmovdqa  ymm15, YMMWORD [YMMBLOCK(12,0,rsi,SIZEOF_JCOEF)]
        vperm2i128  ymm2,ymm14,ymm15, 0x20
        vperm2i128  ymm10,ymm14,ymm15, 0x31
        vmovdqa  ymm6, [YMMBLOCK(4,0,rdx,SIZEOF_IFAST_MULT_TYPE)]
        vperm2i128  ymm14,ymm6,ymm6,0
        vpmullw ymm2,ymm2,ymm14
        vperm2i128  ymm14,ymm6,ymm6,0x11

        vmovdqa  ymm6, YMMWORD [YMMBLOCK(6,0,rsi,SIZEOF_JCOEF)]
        vmovdqa  ymm7, YMMWORD [YMMBLOCK(14,0,rsi,SIZEOF_JCOEF)]
        vperm2i128  ymm3,ymm6,ymm7, 0x20
        vperm2i128  ymm11,ymm6,ymm7, 0x31
        vmovdqa  ymm6, [YMMBLOCK(6,0,rdx,SIZEOF_IFAST_MULT_TYPE)]
        vperm2i128  ymm15,ymm6,ymm6,0
        vpmullw ymm3,ymm3,ymm15
        vperm2i128  ymm15,ymm6,ymm6,0x11

        vpaddw   ymm4,ymm0,ymm2               
        vpaddw   ymm5,ymm1,ymm3               
        vpsubw   ymm0,ymm0,ymm2               
        vpsubw   ymm1,ymm1,ymm3

        vpsllw   ymm1,ymm1,PRE_MULTIPLY_SCALE_BITS
        vpmulhw  ymm1,ymm1,[rel PW_F1414]
        vpsubw   ymm1,ymm1,ymm5               

        vpaddw   ymm6,ymm4,ymm5               
        vpaddw   ymm7,ymm0,ymm1               
        vpsubw   ymm4,ymm4,ymm5               
        vpsubw   ymm0,ymm0,ymm1               

        vmovdqa  YMMWORD [wk(1)], ymm4   
        vmovdqa  YMMWORD [wk(0)], ymm0   

        ; -- Odd part

        vpmullw ymm2, ymm8,ymm12
        vpmullw ymm3, ymm9,ymm13
        vpmullw ymm5, ymm10,ymm14
        vpmullw ymm1, ymm11,ymm15

        vpaddw   ymm4,ymm2,ymm1               
        vpaddw   ymm0,ymm5,ymm3               
        vpsubw   ymm2,ymm2,ymm1               
        vpsubw   ymm5,ymm5,ymm3               

        vmovdqa  ymm1,ymm5               
        vpsllw   ymm2,ymm2,PRE_MULTIPLY_SCALE_BITS
        vpsllw   ymm5,ymm5,PRE_MULTIPLY_SCALE_BITS

        vpaddw   ymm3,ymm4,ymm0               
        vpsubw   ymm4,ymm4,ymm0

        vpsllw   ymm4,ymm4,PRE_MULTIPLY_SCALE_BITS
        vpmulhw  ymm4,ymm4,[rel PW_F1414]     

        ; To avoid overflow...
        ;
        ; (Original)
        ; tmp12 = -2.613125930 * z10 + z5;
        ;
        ; (This implementation)
        ; tmp12 = (-1.613125930 - 1) * z10 + z5;
        ;       = -1.613125930 * z10 - z10 + z5;

        vpmulhw  ymm0,ymm5,[rel PW_MF1613]
        vpaddw   ymm5,ymm5,ymm2
        vpmulhw  ymm5,ymm5,[rel PW_F1847]     
        vpmulhw  ymm2,ymm2,[rel PW_F1082]
        vpsubw   ymm0,ymm0,ymm1
        vpsubw   ymm2,ymm2,ymm5               
        vpaddw   ymm0,ymm0,ymm5               

        ; -- Final output stage

        vpsubw   ymm0,ymm0,ymm3               
        vpsubw   ymm1,ymm6,ymm3               
        vpsubw   ymm5,ymm7,ymm0               
        vpaddw   ymm6,ymm6,ymm3               
        vpaddw   ymm7,ymm7,ymm0               
        vpsubw   ymm4,ymm4,ymm0               
        vpunpckhwd ymm3,ymm6,ymm7             
        vpunpcklwd ymm6,ymm6,ymm7             
        vpunpckhwd ymm0,ymm5,ymm1             
        vpunpcklwd ymm5,ymm5,ymm1             

        vmovdqa  ymm7, YMMWORD [wk(0)]   
        vmovdqa  ymm1, YMMWORD [wk(1)]   

        vmovdqa  YMMWORD [wk(0)], ymm5   
        vmovdqa  YMMWORD [wk(1)], ymm0   

        vpaddw   ymm2,ymm2,ymm4               
        vpsubw   ymm5,ymm7,ymm4               
        vpaddw   ymm7,ymm7,ymm4               
        vpsubw   ymm0,ymm1,ymm2               
        vpaddw   ymm1,ymm1,ymm2               

        vpunpckhwd ymm4,ymm7,ymm0             
        vpunpcklwd ymm7,ymm7,ymm0             
        vpunpckhwd ymm2,ymm1,ymm5             
        vpunpcklwd ymm1,ymm1,ymm5             

        vpunpckhdq ymm0,ymm3,ymm4             
        vpunpckldq ymm3,ymm3,ymm4             
        vpunpckhdq ymm5,ymm6,ymm7             
        vpunpckldq ymm6,ymm6,ymm7             

        vmovdqa  ymm4, YMMWORD [wk(0)]   
        vmovdqa  ymm7, YMMWORD [wk(1)]   

        vmovdqa  YMMWORD [wk(0)], ymm3   
        vmovdqa  YMMWORD [wk(1)], ymm0   

        vpunpckhdq ymm3,ymm1,ymm4             
        vpunpckldq ymm1,ymm1,ymm4             
        vpunpckhdq ymm0,ymm2,ymm7             
        vpunpckldq ymm2,ymm2,ymm7             

        vpunpckhqdq ymm4,ymm6,ymm1            
        vpunpcklqdq ymm6,ymm6,ymm1            
        vpunpckhqdq ymm7,ymm5,ymm3            
        vpunpcklqdq ymm5,ymm5,ymm3            

        vmovdqa  ymm1, YMMWORD [wk(0)]   
        vmovdqa  ymm3, YMMWORD [wk(1)]   

        vmovdqa  YMMWORD [wk(0)], ymm4   
        vmovdqa  YMMWORD [wk(1)], ymm7   

        vpunpckhqdq ymm4,ymm1,ymm2            
        vpunpcklqdq ymm1,ymm1,ymm2            
        vpunpckhqdq ymm7,ymm3,ymm0            
        vpunpcklqdq ymm3,ymm3,ymm0            

.column_end:

        ; -- Prefetch the next coefficient block

        prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 0*32]
        prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 1*32]
        prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 2*32]
        prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 3*32]

        ; ---- Pass 2: process rows from work array, store into output array.

        mov     rax, [original_rbp]
        mov     rdi, r12        ; (JSAMPROW *)
        mov     rax, r13

        ; -- Even part

        vpaddw   ymm2,ymm6,ymm1               
        vpaddw   ymm0,ymm5,ymm3               
        vpsubw   ymm6,ymm6,ymm1               
        vpsubw   ymm5,ymm5,ymm3

        vpsllw   ymm5,ymm5,PRE_MULTIPLY_SCALE_BITS
        vpmulhw  ymm5,ymm5,[rel PW_F1414]
        vpsubw   ymm5,ymm5,ymm0               

        vpaddw   ymm1,ymm2,ymm0               
        vpaddw   ymm3,ymm6,ymm5               
        vpsubw   ymm2,ymm2,ymm0               
        vpsubw   ymm6,ymm6,ymm5               
  
        vmovdqa  ymm0, YMMWORD [wk(0)]   
        vmovdqa  ymm5, YMMWORD [wk(1)]   

        vmovdqa  YMMWORD [wk(0)], ymm2   
        vmovdqa  YMMWORD [wk(1)], ymm6   

        ; -- Odd part

        vpaddw   ymm2,ymm0,ymm7               
        vpaddw   ymm6,ymm4,ymm5               
        vpsubw   ymm0,ymm0,ymm7               
        vpsubw   ymm4,ymm4,ymm5               

        vmovdqa  ymm7,ymm4               
        vpsllw   ymm0,ymm0,PRE_MULTIPLY_SCALE_BITS
        vpsllw   ymm4,ymm4,PRE_MULTIPLY_SCALE_BITS

        vmovdqa  ymm5,ymm2
        vpsubw   ymm2,ymm2,ymm6
        vpaddw   ymm5,ymm5,ymm6               

        vpsllw   ymm2,ymm2,PRE_MULTIPLY_SCALE_BITS
        vpmulhw  ymm2,ymm2,[rel PW_F1414]     
        ; To avoid overflow...
        ;
        ; (Original)
        ; tmp12 = -2.613125930 * z10 + z5;
        ;
        ; (This implementation)
        ; tmp12 = (-1.613125930 - 1) * z10 + z5;
        ;       = -1.613125930 * z10 - z10 + z5;

        vpmulhw  ymm6,ymm4,[rel PW_MF1613]
        vpaddw   ymm4,ymm4,ymm0
        vpmulhw  ymm4,ymm4,[rel PW_F1847]     
        vpmulhw  ymm0,ymm0,[rel PW_F1082]
        vpsubw   ymm6,ymm6,ymm7
        vpsubw   ymm0,ymm0,ymm4               
        vpaddw   ymm6,ymm6,ymm4               

        ; -- Final output stage

        vpsubw   ymm6,ymm6,ymm5               
        vpsubw   ymm7,ymm1,ymm5               
        vpsubw   ymm4,ymm3,ymm6               
        vpaddw   ymm1,ymm1,ymm5               
        vpaddw   ymm3,ymm3,ymm6               
        vpsraw   ymm1,ymm1,(PASS1_BITS+3)     
        vpsraw   ymm3,ymm3,(PASS1_BITS+3)     
        vpsraw   ymm7,ymm7,(PASS1_BITS+3)     
        vpsraw   ymm4,ymm4,(PASS1_BITS+3)     
        vpsubw   ymm2,ymm2,ymm6               

        vpacksswb  ymm1,ymm1,ymm4     
        vpacksswb  ymm3,ymm3,ymm7     

        vmovdqa  ymm5, YMMWORD [wk(1)]   
        vmovdqa  ymm6, YMMWORD [wk(0)]   

        vpaddw   ymm0,ymm0,ymm2               
        vpsubw   ymm7,ymm6,ymm0               
        vpsubw   ymm4,ymm5,ymm2               
        vpaddw   ymm5,ymm5,ymm2               
        vpaddw   ymm6,ymm6,ymm0               
        vpsraw   ymm5,ymm5,(PASS1_BITS+3)     
        vpsraw   ymm6,ymm6,(PASS1_BITS+3)     
        vpsraw   ymm4,ymm4,(PASS1_BITS+3)     
        vpsraw   ymm7,ymm7,(PASS1_BITS+3)     

        vmovdqa    ymm2,[rel PB_CENTERJSAMP]     

        vpacksswb  ymm5,ymm5,ymm6     
        vpacksswb  ymm7,ymm7,ymm4     

        vpaddb     ymm1,ymm1,ymm2
        vpaddb     ymm3,ymm3,ymm2
        vpaddb     ymm5,ymm5,ymm2
        vpaddb     ymm7,ymm7,ymm2

        vpunpckhbw ymm0,ymm1,ymm3     
        vpunpcklbw ymm1,ymm1,ymm3     
        vpunpckhbw ymm6,ymm5,ymm7     
        vpunpcklbw ymm5,ymm5,ymm7     

        vpunpckhwd ymm4,ymm1,ymm5     
        vpunpcklwd ymm1,ymm1,ymm5     
        vpunpckhwd ymm2,ymm6,ymm0     
        vpunpcklwd ymm6,ymm6,ymm0     

        vpunpckhdq ymm3,ymm1,ymm6     
        vpunpckldq ymm1,ymm1,ymm6     
        vpunpckhdq ymm7,ymm4,ymm2     
        vpunpckldq ymm4,ymm4,ymm2     

        vpermq  ymm5,ymm1,0xd8      
        vperm2i128 ymm1,ymm5,ymm5,1 
        vpermq  ymm0,ymm3,0xd8      
        vperm2i128 ymm3,ymm0,ymm0,1 
        vpermq  ymm6,ymm4,0xd8      
        vperm2i128 ymm4,ymm6,ymm6,1 
        vpermq  ymm2,ymm7,0xd8      
        vperm2i128 ymm7,ymm2,ymm2,1 

        mov     rdx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+2*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm5
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm0
        mov     rdx, JSAMPROW [rdi+4*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+6*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm6
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm2

        mov     rdx, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+3*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm1
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm3
        mov     rdx, JSAMPROW [rdi+5*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+7*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm4
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm7

        uncollect_args
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32
