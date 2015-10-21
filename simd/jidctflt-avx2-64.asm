;
; jidctflt.asm - floating-point IDCT (64-bit AVX & AVX2)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009 D. R. Commander
; Copyright 2015 Intel Corporation.
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
; This file contains a floating-point implementation of the inverse DCT
; (Discrete Cosine Transform). The following code is based directly on
; the IJG's original jidctflt.c; see the jidctflt.c for more details.
;
; [TAB8]

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------

%macro  vunpcklps2 3     ; %1=(0 1 2 3) / %2=(4 5 6 7) => %1=(0 1 4 5)
        vshufps  %1,%2,%3,0x44
%endmacro

%macro  unpckhps2 3     ; %1=(0 1 2 3) / %2=(4 5 6 7) => %1=(2 3 6 7)
        vshufps  %1,%2,%3,0xEE
%endmacro

; --------------------------------------------------------------------------
        SECTION SEG_CONST

        alignz  32
        global  EXTN(jconst_idct_float_avx2)

EXTN(jconst_idct_float_avx2):

PD_1_414        times 8 dd  1.414213562373095048801689
PD_1_847        times 8 dd  1.847759065022573512256366
PD_1_082        times 8 dd  1.082392200292393968799446
PD_M2_613       times 8 dd -2.613125929752753055713286
PD_RNDINT_MAGIC times 8 dd  100663296.0 ; (float)(0x00C00000 << 3)
PB_CENTERJSAMP  times 32 db CENTERJSAMPLE

        alignz  32

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Perform dequantization and inverse DCT on one block of coefficients.
;
; GLOBAL(void)
; jsimd_idct_float_avx2 (void * dct_table, JCOEFPTR coef_block,
;                        JSAMPARRAY output_buf, JDIMENSION output_col)
;

; r10 = void * dct_table
; r11 = JCOEFPTR coef_block
; r12 = JSAMPARRAY output_buf
; r13 = JDIMENSION output_col

%define original_rbp    rbp+0
%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_YMMWORD ; ymmword wk[WK_NUM]
%define WK_NUM          2
%define workspace       wk(0)-DCTSIZE2*SIZEOF_FAST_FLOAT
                                        ; FAST_FLOAT workspace[DCTSIZE2]

        align   32
        global  EXTN(jsimd_idct_float_avx2)

EXTN(jsimd_idct_float_avx2):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_YMMWORD)     ; align to 128 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [workspace]
        collect_args
        push    rbx

        ; ---- Pass 1: process columns from input, store into work array.

        mov     rdx, r10                ; quantptr
        mov     rsi, r11                ; inptr
        lea     rdi, [workspace]                        ; FAST_FLOAT * wsptr
.columnloop:
%ifndef NO_ZERO_COLUMN_TEST_FLOAT_AVX
        mov     eax, DWORD [DWBLOCK(1,0,rsi,SIZEOF_JCOEF)]
        or      eax, DWORD [DWBLOCK(2,0,rsi,SIZEOF_JCOEF)]
        jnz     near .columnDCT

        vmovdqa  xmm0, XMMWORD [XMMBLOCK(1,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm0, YMMWORD [YMMBLOCK(2,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm0, YMMWORD [YMMBLOCK(4,0,rsi,SIZEOF_JCOEF)]
        vpor     ymm0, YMMWORD [YMMBLOCK(6,0,rsi,SIZEOF_JCOEF)]

        vpacksswb ymm0,ymm0,ymm0
        vpermq  ymm0,ymm0,0xD8 
        vpacksswb xmm0,xmm0,xmm0
        vpacksswb xmm0,xmm0,xmm0

        vmovd    eax,xmm0
        test    rax,rax
        jnz     short .columnDCT

        ; -- AC terms all zero

        vmovdqa     xmm0, XMM_MMWORD [MMBLOCK(0,0,rsi,SIZEOF_JCOEF)]
        vpermq      ymm0,ymm0,0x50

        vpunpcklwd ymm0,ymm0,ymm0             
        vpsrad     ymm0,(DWORD_BIT-WORD_BIT)     
        vcvtdq2ps  ymm0,ymm0                     

        vmulps   ymm0,ymm0, YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]

        vperm2f128 ymm14,ymm0,ymm0,0
        vperm2f128 ymm15,ymm0,ymm0,0x11

        vshufps  ymm0,ymm14,ymm14,0x00                  
        vshufps  ymm8,ymm14,ymm14,0x55                  
        vshufps  ymm1,ymm14,ymm14,0xAA                  
        vshufps  ymm9,ymm14,ymm14,0xFF                  

        vshufps  ymm2,ymm15,ymm15,0x00                  
        vshufps  ymm10,ymm15,ymm15,0x55                  
        vshufps  ymm3,ymm15,ymm15,0xAA       
        vshufps  ymm11,ymm15,ymm15,0xFF                 

        jmp     near .nextcolumn
%endif

.columnDCT:

        ; -- Even part

        vmovdqa      ymm8, YMM_MMWORD [YMMBLOCK(0,0,rsi,SIZEOF_JCOEF)] 
        vpermq       ymm0,ymm8,0x10                                   
        vmovdqa      ymm9, YMM_MMWORD [YMMBLOCK(2,0,rsi,SIZEOF_JCOEF)] 
        vpermq       ymm1,ymm9,0x10
        vmovdqa      ymm10, YMM_MMWORD [YMMBLOCK(4,0,rsi,SIZEOF_JCOEF)]
        vpermq       ymm2,ymm10,0x10
        vmovdqa      ymm11, YMM_MMWORD [YMMBLOCK(6,0,rsi,SIZEOF_JCOEF)]
        vpermq       ymm3,ymm11,0x10

        vpunpcklwd ymm0,ymm0,ymm0             
        vpunpcklwd ymm1,ymm1,ymm1             
        vpsrad     ymm0,ymm0,(DWORD_BIT-WORD_BIT)     
        vpsrad     ymm1,ymm1,(DWORD_BIT-WORD_BIT)     
        vcvtdq2ps  ymm0,ymm0                     
        vcvtdq2ps  ymm1,ymm1                     

        vpunpcklwd ymm2,ymm2,ymm2             
        vpunpcklwd ymm3,ymm3,ymm3             
        vpsrad     ymm2,ymm2,(DWORD_BIT-WORD_BIT)     
        vpsrad     ymm3,ymm3,(DWORD_BIT-WORD_BIT)     
        vcvtdq2ps  ymm2,ymm2                     
        vcvtdq2ps  ymm3,ymm3                     

        vmulps     ymm0,ymm0, YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
        vmulps     ymm1,ymm1, YMMWORD [YMMBLOCK(2,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
        vmulps     ymm2,ymm2, YMMWORD [YMMBLOCK(4,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
        vmulps     ymm3,ymm3, YMMWORD [YMMBLOCK(6,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]

        vaddps   ymm4,ymm0,ymm2               
        vaddps   ymm5,ymm1,ymm3               
        vsubps   ymm0,ymm0,ymm2               
        vsubps   ymm1,ymm1,ymm3

        vmulps   ymm1,ymm1,[rel PD_1_414]
        vsubps   ymm1,ymm1,ymm5               

        vaddps   ymm6,ymm4,ymm5               
        vaddps   ymm7,ymm0,ymm1               
        vsubps   ymm4,ymm4,ymm5               
        vsubps   ymm0,ymm0,ymm1               

        vmovaps  YMMWORD [wk(1)], ymm4   
        vmovaps  YMMWORD [wk(0)], ymm0   

        ; -- Odd part
        vpermq       ymm2,ymm8,0x32
        vpermq       ymm3,ymm9,0x32
        vpermq       ymm5,ymm10,0x32
        vpermq       ymm1,ymm11,0x32

        vpunpcklwd ymm2,ymm2,ymm2             
        vpunpcklwd ymm3,ymm3,ymm3             
        vpsrad     ymm2,ymm2,(DWORD_BIT-WORD_BIT)     
        vpsrad     ymm3,ymm3,(DWORD_BIT-WORD_BIT)     
        vcvtdq2ps  ymm2,ymm2                     
        vcvtdq2ps  ymm3,ymm3                     

        vpunpcklwd ymm5,ymm5,ymm5             
        vpunpcklwd ymm1,ymm1,ymm1             
        vpsrad     ymm5,ymm5,(DWORD_BIT-WORD_BIT)     
        vpsrad     ymm1,ymm1,(DWORD_BIT-WORD_BIT)     
        vcvtdq2ps  ymm5,ymm5                     
        vcvtdq2ps  ymm1,ymm1                     

        vmulps     ymm2, YMMWORD [YMMBLOCK(1,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
        vmulps     ymm3, YMMWORD [YMMBLOCK(3,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
        vmulps     ymm5, YMMWORD [YMMBLOCK(5,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
        vmulps     ymm1, YMMWORD [YMMBLOCK(7,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]

        vsubps   ymm4,ymm2,ymm1               
        vsubps   ymm0,ymm5,ymm3               
        vaddps   ymm2,ymm2,ymm1               
        vaddps   ymm5,ymm5,ymm3               

        vaddps   ymm1,ymm2,ymm5               
        vsubps   ymm2,ymm2,ymm5

        vmulps   ymm2,[rel PD_1_414]     

        vmulps   ymm3,ymm0,[rel PD_M2_613]    
        vaddps   ymm0,ymm0,ymm4
        vmulps   ymm0,[rel PD_1_847]     
        vmulps   ymm4,[rel PD_1_082]     
        vaddps   ymm3,ymm3,ymm0               
        vsubps   ymm4,ymm4,ymm0               

        ; -- Final output stage

        vsubps   ymm3,ymm3,ymm1               
        vaddps   ymm8,ymm6,ymm1               
        vaddps   ymm9,ymm7,ymm3               
        vsubps   ymm15,ymm6,ymm1               
        vsubps   ymm14,ymm7,ymm3               
        vsubps   ymm2,ymm2,ymm3               

        vmovaps  ymm7, YMMWORD [wk(0)]   
        vmovaps  ymm5, YMMWORD [wk(1)]   

        vaddps   ymm4,ymm4,ymm2               
        vaddps   ymm10,ymm7,ymm2               
        vaddps   ymm12,ymm5,ymm4               
        vsubps   ymm13,ymm7,ymm2               
        vsubps   ymm11,ymm5,ymm4               

        vunpcklps ymm2,ymm10,ymm11
        vunpckhps ymm3,ymm10,ymm11
        vunpcklps ymm0,ymm8,ymm9
        vunpckhps ymm1,ymm8,ymm9
        vunpcklpd ymm8,ymm0,ymm2
        vunpckhpd ymm10,ymm0,ymm2
        vunpcklpd ymm9,ymm1,ymm3
        vunpckhpd ymm11,ymm1,ymm3
        ;   

        vunpcklps ymm6,ymm14,ymm15
        vunpckhps ymm7,ymm14,ymm15
        vunpcklps ymm4,ymm12,ymm13
        vunpckhps ymm5,ymm12,ymm13
        vunpcklpd ymm12,ymm4,ymm6
        vunpckhpd ymm14,ymm4,ymm6
        vunpcklpd ymm13,ymm5,ymm7
        vunpckhpd ymm15,ymm5,ymm7

        vperm2f128 ymm0,ymm8,ymm12,0x20  
        vperm2f128 ymm2,ymm8,ymm12,0x31  
        vperm2f128 ymm1,ymm9,ymm13,0x20  
        vperm2f128 ymm3,ymm9,ymm13,0x31  
        vperm2f128 ymm8,ymm10,ymm14,0x20 
        vperm2f128 ymm10,ymm10,ymm14,0x31
        vperm2f128 ymm9,ymm11,ymm15,0x20 
        vperm2f128 ymm11,ymm11,ymm15,0x31
;
.nextcolumn:

        ; -- Prefetch the next coefficient block
        prefetchnta [rsi + (DCTSIZE2-8)*SIZEOF_JCOEF + 0*32]
        prefetchnta [rsi + (DCTSIZE2-8)*SIZEOF_JCOEF + 1*32]
        prefetchnta [rsi + (DCTSIZE2-8)*SIZEOF_JCOEF + 2*32]
        prefetchnta [rsi + (DCTSIZE2-8)*SIZEOF_JCOEF + 3*32]


        ; ---- Pass 2: process rows from work array, store into output array.

        mov     rax, [original_rbp]
        lea     rsi, [workspace]                        ; FAST_FLOAT * wsptr
        mov     rdi, r12        ; (JSAMPROW *)
        mov     rax, r13

.rowloop:

        ; -- Even part

        vaddps   ymm4,ymm0,ymm2               
        vaddps   ymm5,ymm1,ymm3               
        vsubps   ymm0,ymm0,ymm2               
        vsubps   ymm1,ymm1,ymm3

        vmulps   ymm1,ymm1,[rel PD_1_414]
        vsubps   ymm1,ymm1,ymm5               

        vaddps   ymm6,ymm4,ymm5               
        vaddps   ymm7,ymm0,ymm1               
        vsubps   ymm4,ymm4,ymm5               
        vsubps   ymm0,ymm0,ymm1               

        vmovaps  YMMWORD [wk(1)], ymm4   
        vmovaps  YMMWORD [wk(0)], ymm0   

        ; -- Odd part

        vmovaps ymm2,ymm8
        vmovaps ymm3,ymm9
        vmovaps ymm5,ymm10
        vmovaps ymm1,ymm11

        vsubps   ymm4,ymm2,ymm1               
        vsubps   ymm0,ymm5,ymm3               
        vaddps   ymm2,ymm2,ymm1               
        vaddps   ymm5,ymm5,ymm3               

        vaddps   ymm1,ymm2,ymm5               
        vsubps   ymm2,ymm2,ymm5

        vmulps   ymm2,ymm2,[rel PD_1_414]     

        vmulps   ymm3,ymm0,[rel PD_M2_613]    
        vaddps   ymm0,ymm0,ymm4
        vmulps   ymm0,ymm0,[rel PD_1_847]     
        vmulps   ymm4,ymm4,[rel PD_1_082]     
        vaddps   ymm3,ymm3,ymm0               
        vsubps   ymm4,ymm4,ymm0               

        ; -- Final output stage

        vsubps   ymm3,ymm3,ymm1               
        vsubps   ymm5,ymm6,ymm1               
        vsubps   ymm0,ymm7,ymm3               
        vaddps   ymm6,ymm6,ymm1               
        vaddps   ymm7,ymm7,ymm3               
        vsubps   ymm2,ymm2,ymm3               

        vmovaps  ymm1,[rel PD_RNDINT_MAGIC]      
        vpcmpeqd ymm3,ymm3,ymm3
        vpsrld   ymm3,ymm3,WORD_BIT           

        vaddps   ymm6,ymm6,ymm1       
        vaddps   ymm7,ymm7,ymm1       
        vaddps   ymm0,ymm0,ymm1       
        vaddps   ymm5,ymm5,ymm1       

        vpand    ymm6,ymm6,ymm3               
        vpslld   ymm7,ymm7,WORD_BIT           
        vpand    ymm0,ymm0,ymm3               
        vpslld   ymm5,ymm5,WORD_BIT           
        vpor     ymm6,ymm6,ymm7               
        vpor     ymm0,ymm0,ymm5               

        vmovaps  ymm1, YMMWORD [wk(0)]   
        vmovaps  ymm3, YMMWORD [wk(1)]   

        vaddps   ymm4,ymm4,ymm2               
        vsubps   ymm7,ymm1,ymm2             
        vsubps   ymm5,ymm3,ymm4            
        vaddps   ymm1,ymm1,ymm2               
        vaddps   ymm3,ymm3,ymm4              

        vmovaps  ymm2,[rel PD_RNDINT_MAGIC]      
        vpcmpeqd ymm4,ymm4,ymm4
        vpsrld   ymm4,ymm4,WORD_BIT           

        vaddps   ymm3,ymm3,ymm2      
        vaddps   ymm7,ymm7,ymm2     
        vaddps   ymm1,ymm1,ymm2    
        vaddps   ymm5,ymm5,ymm2   

        vpand    ymm3,ymm3,ymm4          
        vpslld   ymm7,ymm7,WORD_BIT     
        vpand    ymm1,ymm1,ymm4        
        vpslld   ymm5,ymm5,WORD_BIT   
        vpor     ymm3,ymm3,ymm7               
        vpor     ymm1,ymm1,ymm5               

        vmovdqa    ymm2,[rel PB_CENTERJSAMP]     

        vpacksswb  ymm6,ymm6,ymm3     
        vpacksswb  ymm1,ymm1,ymm0     
        vpaddb     ymm6,ymm6,ymm2
        vpaddb     ymm1,ymm1,ymm2

        vmovdqa    ymm4,ymm6     
        vpunpcklwd ymm6,ymm6,ymm1     
        vpunpckhwd ymm4,ymm4,ymm1     

        vmovdqa    ymm7,ymm6     
        vpunpckldq ymm6,ymm6,ymm4     
        vpunpckhdq ymm7,ymm7,ymm4     

        mov     rdx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]
        mov     rbx, JSAMPROW [rdi+2*SIZEOF_JSAMPROW]
        vmovq    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm6;
        vmovq    XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE], xmm7;

        vpermq  ymm5,ymm6,0x01
        vpermq  ymm3,ymm7,0x01
        mov     rdx, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]
        mov     rbx, JSAMPROW [rdi+3*SIZEOF_JSAMPROW]
        vmovq   XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm5
        vmovq   XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE], xmm3

        vpermq  ymm5,ymm6,0x02
        vpermq  ymm3,ymm7,0x02
        mov     rdx, JSAMPROW [rdi+4*SIZEOF_JSAMPROW]
        mov     rbx, JSAMPROW [rdi+6*SIZEOF_JSAMPROW]
        vmovq   XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm5
        vmovq   XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE], xmm3

        vpermq  ymm5,ymm6,0x03
        vpermq  ymm3,ymm7,0x03
        mov     rdx, JSAMPROW [rdi+5*SIZEOF_JSAMPROW]
        mov     rbx, JSAMPROW [rdi+7*SIZEOF_JSAMPROW]
        vmovq   XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm5
        vmovq   XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE], xmm3

.exit:

        pop     rbx
        uncollect_args
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32
