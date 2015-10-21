;
; jidctint.asm - accurate integer IDCT (64-bit AVX2)
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
; This file contains a slow-but-accurate integer implementation of the
; inverse DCT (Discrete Cosine Transform). The following code is based
; directly on the IJG's original jidctint.c; see the jidctint.c for
; more details.
;
; [TAB8]

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------

%define CONST_BITS      13
%define PASS1_BITS      2

%define DESCALE_P1      (CONST_BITS-PASS1_BITS)
%define DESCALE_P2      (CONST_BITS+PASS1_BITS+3)

%if CONST_BITS == 13
F_0_298 equ      2446           ; FIX(0.298631336)
F_0_390 equ      3196           ; FIX(0.390180644)
F_0_541 equ      4433           ; FIX(0.541196100)
F_0_765 equ      6270           ; FIX(0.765366865)
F_0_899 equ      7373           ; FIX(0.899976223)
F_1_175 equ      9633           ; FIX(1.175875602)
F_1_501 equ     12299           ; FIX(1.501321110)
F_1_847 equ     15137           ; FIX(1.847759065)
F_1_961 equ     16069           ; FIX(1.961570560)
F_2_053 equ     16819           ; FIX(2.053119869)
F_2_562 equ     20995           ; FIX(2.562915447)
F_3_072 equ     25172           ; FIX(3.072711026)
%else
; NASM cannot do compile-time arithmetic on floating-point constants.
%define DESCALE(x,n)  (((x)+(1<<((n)-1)))>>(n))
F_0_298 equ     DESCALE( 320652955,30-CONST_BITS)       ; FIX(0.298631336)
F_0_390 equ     DESCALE( 418953276,30-CONST_BITS)       ; FIX(0.390180644)
F_0_541 equ     DESCALE( 581104887,30-CONST_BITS)       ; FIX(0.541196100)
F_0_765 equ     DESCALE( 821806413,30-CONST_BITS)       ; FIX(0.765366865)
F_0_899 equ     DESCALE( 966342111,30-CONST_BITS)       ; FIX(0.899976223)
F_1_175 equ     DESCALE(1262586813,30-CONST_BITS)       ; FIX(1.175875602)
F_1_501 equ     DESCALE(1612031267,30-CONST_BITS)       ; FIX(1.501321110)
F_1_847 equ     DESCALE(1984016188,30-CONST_BITS)       ; FIX(1.847759065)
F_1_961 equ     DESCALE(2106220350,30-CONST_BITS)       ; FIX(1.961570560)
F_2_053 equ     DESCALE(2204520673,30-CONST_BITS)       ; FIX(2.053119869)
F_2_562 equ     DESCALE(2751909506,30-CONST_BITS)       ; FIX(2.562915447)
F_3_072 equ     DESCALE(3299298341,30-CONST_BITS)       ; FIX(3.072711026)
%endif

; --------------------------------------------------------------------------
        SECTION SEG_CONST

        alignz  32
        global  EXTN(jconst_idct_islow_avx2)

EXTN(jconst_idct_islow_avx2):

PW_F130_F054    times 8 dw  (F_0_541+F_0_765), F_0_541
PW_F054_MF130   times 8 dw  F_0_541, (F_0_541-F_1_847)
PW_MF078_F117   times 8 dw  (F_1_175-F_1_961), F_1_175
PW_F117_F078    times 8 dw  F_1_175, (F_1_175-F_0_390)
PW_MF060_MF089  times 8 dw  (F_0_298-F_0_899),-F_0_899
PW_MF089_F060   times 8 dw -F_0_899, (F_1_501-F_0_899)
PW_MF050_MF256  times 8 dw  (F_2_053-F_2_562),-F_2_562
PW_MF256_F050   times 8 dw -F_2_562, (F_3_072-F_2_562)
PD_DESCALE_P1   times 8 dd  1 << (DESCALE_P1-1)
PD_DESCALE_P2   times 8 dd  1 << (DESCALE_P2-1)
PB_CENTERJSAMP  times 32 db CENTERJSAMPLE

        alignz  32

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Perform dequantization and inverse DCT on one block of coefficients.
;
; GLOBAL(void)
; jsimd_idct_islow_avx2 (void * dct_table, JCOEFPTR coef_block,
;                        JSAMPARRAY output_buf, JDIMENSION output_col)
;

; r10 = jpeg_component_info * compptr
; r11 = JCOEFPTR coef_block
; r12 = JSAMPARRAY output_buf
; r13 = JDIMENSION output_col

%define original_rbp    rbp+0
%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_YMMWORD 
%define WK_NUM          12

        align   32
        global  EXTN(jsimd_idct_islow_avx2)

EXTN(jsimd_idct_islow_avx2):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_YMMWORD)     ; align to 256 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        collect_args

        ; ---- Pass 1: process columns from input.

        mov     rdx, r10                ; quantptr
        mov     rsi, r11                ; inptr

%ifndef NO_ZERO_COLUMN_TEST_ISLOW_AVX2
        mov     eax, DWORD [DWBLOCK(1,0,rsi,SIZEOF_JCOEF)]
        or      eax, DWORD [DWBLOCK(2,0,rsi,SIZEOF_JCOEF)]
        ;next MCUBUffer is located at DWBLOCK(8,0,rsi,SIZEOF_JCOEF)
        ; or DWBLOCK(4,0,rsi,2*SIZEOF_JCOEF)
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
        vpmullw  ymm5, ymm0,ymm1

        vpsllw   ymm5,ymm5,PASS1_BITS

        vpunpckhwd ymm4,ymm5,ymm5             
        vpunpcklwd ymm5,ymm5,ymm5             

        vpshufd  ymm7,ymm5,0x00          
        vpshufd  ymm6,ymm5,0x55          
        vpshufd  ymm1,ymm5,0xAA          
        vpshufd  ymm5,ymm5,0xFF          
        vpshufd  ymm0,ymm4,0x00          
        vpshufd  ymm3,ymm4,0x55          
        vpshufd  ymm2,ymm4,0xAA          
        vpshufd  ymm4,ymm4,0xFF          

        vmovdqa  YMMWORD [wk(8)], ymm6   
        vmovdqa  YMMWORD [wk(9)], ymm5   
        vmovdqa  YMMWORD [wk(10)], ymm3  
        vmovdqa  YMMWORD [wk(11)], ymm4  
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
        ; (Original)
        ; z1 = (z2 + z3) * 0.541196100;
        ; tmp2 = z1 + z3 * -1.847759065;
        ; tmp3 = z1 + z2 * 0.765366865;
        ;
        ; (This implementation)
        ; tmp2 = z2 * 0.541196100 + z3 * (0.541196100 - 1.847759065);
        ; tmp3 = z2 * (0.541196100 + 0.765366865) + z3 * 0.541196100;

        vpunpcklwd ymm4,ymm1,ymm3             
        vpunpckhwd ymm5,ymm1,ymm3
        vpmaddwd   ymm1,ymm4,[rel PW_F054_MF130]      
        vpmaddwd   ymm3,ymm5,[rel PW_F054_MF130]      
        vpmaddwd   ymm4,ymm4,[rel PW_F130_F054]       
        vpmaddwd   ymm5,ymm5,[rel PW_F130_F054]       

        vpsubw     ymm6,ymm0,ymm2             
        vpaddw     ymm0,ymm0,ymm2             

        vpxor      ymm2,ymm2,ymm2
        vpunpcklwd ymm7,ymm2,ymm0             
        vpunpckhwd ymm2,ymm2,ymm0             
        vpsrad     ymm7,ymm7,(16-CONST_BITS)  
        vpsrad     ymm2,ymm2,(16-CONST_BITS)  

        vpsubd   ymm0,ymm7,ymm4               
        vpaddd   ymm7,ymm7,ymm4               
        vpsubd   ymm4,ymm2,ymm5               
        vpaddd   ymm2,ymm2,ymm5               

        vmovdqa  YMMWORD [wk(0)], ymm7   
        vmovdqa  YMMWORD [wk(1)], ymm2   
        vmovdqa  YMMWORD [wk(2)], ymm0   
        vmovdqa  YMMWORD [wk(3)], ymm4   

        vpxor      ymm7,ymm7
        vpunpcklwd ymm5,ymm7,ymm6             
        vpunpckhwd ymm7,ymm7,ymm6             
        vpsrad     ymm5,ymm5,(16-CONST_BITS)  
        vpsrad     ymm7,ymm7,(16-CONST_BITS)  

        vpsubd   ymm2,ymm5,ymm1               
        vpaddd   ymm5,ymm5,ymm1               
        vpsubd   ymm0,ymm7,ymm3               
        vpaddd   ymm7,ymm7,ymm3               

        vmovdqa  YMMWORD [wk(4)], ymm5   
        vmovdqa  YMMWORD [wk(5)], ymm7   
        vmovdqa  YMMWORD [wk(6)], ymm2   
        vmovdqa  YMMWORD [wk(7)], ymm0   

        ; -- Odd part

        vpmullw ymm4, ymm8,ymm12
        vpmullw ymm6, ymm9,ymm13
        vpmullw ymm1, ymm10,ymm14
        vpmullw ymm3, ymm11,ymm15
        vpaddw   ymm5,ymm6,ymm3               
        vpaddw   ymm7,ymm4,ymm1               

        ; (Original)
        ; z5 = (z3 + z4) * 1.175875602;
        ; z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
        ; z3 += z5;  z4 += z5;
        ;
        ; (This implementation)
        ; z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
        ; z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);

        vpunpcklwd ymm2,ymm5,ymm7
        vpunpckhwd ymm0,ymm5,ymm7
        vpmaddwd   ymm5,ymm2,[rel PW_F117_F078]       
        vpmaddwd   ymm7,ymm0,[rel PW_F117_F078]       
        vpmaddwd   ymm2,ymm2,[rel PW_MF078_F117]      
        vpmaddwd   ymm0,ymm0,[rel PW_MF078_F117]      
        vmovdqa  YMMWORD [wk(10)], ymm2  
        vmovdqa  YMMWORD [wk(11)], ymm0  

        ; (Original)
        ; z1 = tmp0 + tmp3;  z2 = tmp1 + tmp2;
        ; tmp0 = tmp0 * 0.298631336;  tmp1 = tmp1 * 2.053119869;
        ; tmp2 = tmp2 * 3.072711026;  tmp3 = tmp3 * 1.501321110;
        ; z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
        ; tmp0 += z1 + z3;  tmp1 += z2 + z4;
        ; tmp2 += z2 + z3;  tmp3 += z1 + z4;
        ;
        ; (This implementation)
        ; tmp0 = tmp0 * (0.298631336 - 0.899976223) + tmp3 * -0.899976223;
        ; tmp1 = tmp1 * (2.053119869 - 2.562915447) + tmp2 * -2.562915447;
        ; tmp2 = tmp1 * -2.562915447 + tmp2 * (3.072711026 - 2.562915447);
        ; tmp3 = tmp0 * -0.899976223 + tmp3 * (1.501321110 - 0.899976223);
        ; tmp0 += z3;  tmp1 += z4;
        ; tmp2 += z3;  tmp3 += z4;

        vpunpcklwd ymm2,ymm3,ymm4
        vpunpckhwd ymm0,ymm3,ymm4
        vpmaddwd   ymm3,ymm2,[rel PW_MF089_F060]      
        vpmaddwd   ymm4,ymm0,[rel PW_MF089_F060]      
        vpmaddwd   ymm2,ymm2,[rel PW_MF060_MF089]     
        vpmaddwd   ymm0,ymm0,[rel PW_MF060_MF089]     

        vpaddd   ymm2,ymm2, YMMWORD [wk(10)]  
        vpaddd   ymm0,ymm0, YMMWORD [wk(11)]  
        vpaddd   ymm3,ymm3,ymm5               
        vpaddd   ymm4,ymm4,ymm7               

        vmovdqa  YMMWORD [wk(8)], ymm2   
        vmovdqa  YMMWORD [wk(9)], ymm0   

        vpunpcklwd ymm2,ymm1,ymm6
        vpunpckhwd ymm0,ymm1,ymm6
        vpmaddwd   ymm1,ymm2,[rel PW_MF256_F050]      
        vpmaddwd   ymm6,ymm0,[rel PW_MF256_F050]      
        vpmaddwd   ymm2,ymm2,[rel PW_MF050_MF256]     
        vpmaddwd   ymm0,ymm0,[rel PW_MF050_MF256]     
        
        vpaddd   ymm2,ymm2,ymm5               
        vpaddd   ymm0,ymm0,ymm7               
        vpaddd   ymm1,ymm1, YMMWORD [wk(10)]  
        vpaddd   ymm6,ymm6, YMMWORD [wk(11)]  

        vmovdqa  YMMWORD [wk(10)], ymm2  
        vmovdqa  YMMWORD [wk(11)], ymm0  

        ; -- Final output stage

        vmovdqa  ymm5, YMMWORD [wk(0)]   
        vmovdqa  ymm7, YMMWORD [wk(1)]   

        vpsubd   ymm2,ymm5,ymm3               
        vpsubd   ymm0,ymm7,ymm4               
        vpaddd   ymm5,ymm5,ymm3               
        vpaddd   ymm7,ymm7,ymm4               

        vmovdqa  ymm3,[rel PD_DESCALE_P1]        

        vpaddd   ymm5,ymm5,ymm3
        vpaddd   ymm7,ymm7,ymm3
        vpsrad   ymm5,ymm5,DESCALE_P1
        vpsrad   ymm7,ymm7,DESCALE_P1
        vpaddd   ymm2,ymm2,ymm3
        vpaddd   ymm0,ymm0,ymm3
        vpsrad   ymm2,ymm2,DESCALE_P1
        vpsrad   ymm0,ymm0,DESCALE_P1

        vpackssdw  ymm5,ymm5,ymm7             
        vpackssdw  ymm2,ymm2,ymm0             

        vmovdqa  ymm4, YMMWORD [wk(4)]   
        vmovdqa  ymm3, YMMWORD [wk(5)]   

        vpsubd   ymm7,ymm4,ymm1               
        vpsubd   ymm0,ymm3,ymm6               
        vpaddd   ymm4,ymm4,ymm1               
        vpaddd   ymm3,ymm3,ymm6               

        vmovdqa  ymm1,[rel PD_DESCALE_P1]        

        vpaddd   ymm4,ymm4,ymm1
        vpaddd   ymm3,ymm3,ymm1
        vpsrad   ymm4,ymm4,DESCALE_P1
        vpsrad   ymm3,ymm3,DESCALE_P1
        vpaddd   ymm7,ymm7,ymm1
        vpaddd   ymm0,ymm0,ymm1
        vpsrad   ymm7,ymm7,DESCALE_P1
        vpsrad   ymm0,ymm0,DESCALE_P1

        vpackssdw  ymm4,ymm4,ymm3             
        vpackssdw  ymm7,ymm7,ymm0             

        vpunpckhwd ymm6,ymm5,ymm4             
        vpunpcklwd ymm5,ymm5,ymm4             
        vpunpckhwd ymm1,ymm7,ymm2             
        vpunpcklwd ymm7,ymm7,ymm2             

        vmovdqa  ymm3, YMMWORD [wk(6)]   
        vmovdqa  ymm0, YMMWORD [wk(7)]   
        vmovdqa  ymm4, YMMWORD [wk(10)]  
        vmovdqa  ymm2, YMMWORD [wk(11)]  

        vmovdqa  YMMWORD [wk(0)], ymm5   
        vmovdqa  YMMWORD [wk(1)], ymm6   
        vmovdqa  YMMWORD [wk(4)], ymm7   
        vmovdqa  YMMWORD [wk(5)], ymm1   

        vpsubd   ymm5,ymm3,ymm4               
        vpsubd   ymm6,ymm0,ymm2               
        vpaddd   ymm3,ymm3,ymm4               
        vpaddd   ymm0,ymm0,ymm2               

        vmovdqa  ymm7,[rel PD_DESCALE_P1]        

        vpaddd   ymm3,ymm3,ymm7
        vpaddd   ymm0,ymm0,ymm7
        vpsrad   ymm3,ymm3,DESCALE_P1
        vpsrad   ymm0,ymm0,DESCALE_P1
        vpaddd   ymm5,ymm5,ymm7
        vpaddd   ymm6,ymm6,ymm7
        vpsrad   ymm5,ymm5,DESCALE_P1
        vpsrad   ymm6,ymm6,DESCALE_P1

        vpackssdw  ymm3,ymm3,ymm0             
        vpackssdw  ymm5,ymm5,ymm6             

        vmovdqa  ymm1, YMMWORD [wk(2)]   
        vmovdqa  ymm4, YMMWORD [wk(3)]   
        vmovdqa  ymm2, YMMWORD [wk(8)]   
        vmovdqa  ymm7, YMMWORD [wk(9)]   

        vpsubd   ymm0,ymm1,ymm2               
        vpsubd   ymm6,ymm4,ymm7               
        vpaddd   ymm1,ymm1,ymm2               
        vpaddd   ymm4,ymm4,ymm7               

        vmovdqa  ymm2,[rel PD_DESCALE_P1]        

        vpaddd   ymm1,ymm1,ymm2
        vpaddd   ymm4,ymm4,ymm2
        vpsrad   ymm1,ymm1,DESCALE_P1
        vpsrad   ymm4,ymm4,DESCALE_P1
        vpaddd   ymm0,ymm0,ymm2
        vpaddd   ymm6,ymm6,ymm2
        vpsrad   ymm0,ymm0,DESCALE_P1
        vpsrad   ymm6,ymm6,DESCALE_P1

        vpackssdw  ymm1,ymm1,ymm4             
        vpackssdw  ymm0,ymm0,ymm6             

        vmovdqa  ymm7, YMMWORD [wk(0)]   
        vmovdqa  ymm2, YMMWORD [wk(1)]   

        vpunpckhwd ymm4,ymm3,ymm1             
        vpunpcklwd ymm3,ymm3,ymm1             
        vpunpckhwd ymm6,ymm0,ymm5             
        vpunpcklwd ymm0,ymm0,ymm5             

        vpunpckhdq ymm1,ymm7,ymm3             
        vpunpckldq ymm7,ymm7,ymm3             
        vpunpckhdq ymm5,ymm2,ymm4             
        vpunpckldq ymm2,ymm2,ymm4             

        vmovdqa  ymm3, YMMWORD [wk(4)]   
        vmovdqa  ymm4, YMMWORD [wk(5)]   

        vmovdqa  YMMWORD [wk(6)], ymm2   
        vmovdqa  YMMWORD [wk(7)], ymm5   

        vpunpckhdq ymm2,ymm0,ymm3             
        vpunpckldq ymm0,ymm0,ymm3             
        vpunpckhdq ymm5,ymm6,ymm4             
        vpunpckldq ymm6,ymm6,ymm4             

        vpunpckhqdq ymm3,ymm7,ymm0            
        vpunpcklqdq ymm7,ymm7,ymm0            
        vpunpckhqdq ymm4,ymm1,ymm2            
        vpunpcklqdq ymm1,ymm1,ymm2            

        vmovdqa  ymm0, YMMWORD [wk(6)]   
        vmovdqa  ymm2, YMMWORD [wk(7)]   

        vmovdqa  YMMWORD [wk(8)], ymm3   
        vmovdqa  YMMWORD [wk(9)], ymm4   

        vpunpckhqdq ymm3,ymm0,ymm6            
        vpunpcklqdq ymm0,ymm0,ymm6            
        vpunpckhqdq ymm4,ymm2,ymm5            
        vpunpcklqdq ymm2,ymm2,ymm5            

        vmovdqa  YMMWORD [wk(10)], ymm3  
        vmovdqa  YMMWORD [wk(11)], ymm4  

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

        ; ymm7=col0, ymm1=col2, ymm0=col4, xmm2=col6

        ; (Original)
        ; z1 = (z2 + z3) * 0.541196100;
        ; tmp2 = z1 + z3 * -1.847759065;
        ; tmp3 = z1 + z2 * 0.765366865;
        ;
        ; (This implementation)
        ; tmp2 = z2 * 0.541196100 + z3 * (0.541196100 - 1.847759065);
        ; tmp3 = z2 * (0.541196100 + 0.765366865) + z3 * 0.541196100;

        vpunpckhwd ymm5,ymm1,ymm2
        vpunpcklwd ymm6,ymm1,ymm2             
        vpmaddwd   ymm1,ymm6,[rel PW_F054_MF130]      
        vpmaddwd   ymm2,ymm5,[rel PW_F054_MF130]      
        vpmaddwd   ymm6,ymm6,[rel PW_F130_F054]       
        vpmaddwd   ymm5,ymm5,[rel PW_F130_F054]       

        vpsubw     ymm3,ymm7,ymm0             
        vpaddw     ymm7,ymm7,ymm0             

        vpxor      ymm0,ymm0,ymm0
        vpunpcklwd ymm4,ymm0,ymm7             
        vpunpckhwd ymm0,ymm0,ymm7             
        vpsrad     ymm4,ymm4,(16-CONST_BITS)  
        vpsrad     ymm0,ymm0,(16-CONST_BITS)  

        vpsubd   ymm7,ymm4,ymm6               
        vpaddd   ymm4,ymm4,ymm6               
        vpsubd   ymm6,ymm0,ymm5               
        vpaddd   ymm0,ymm0,ymm5               

        vmovdqa  YMMWORD [wk(0)], ymm4   
        vmovdqa  YMMWORD [wk(1)], ymm0   
        vmovdqa  YMMWORD [wk(2)], ymm7   
        vmovdqa  YMMWORD [wk(3)], ymm6   

        vpxor      ymm4,ymm4,ymm4
        vpunpcklwd ymm5,ymm4,ymm3             
        vpunpckhwd ymm4,ymm4,ymm3             
        vpsrad     ymm5,ymm5,(16-CONST_BITS)  
        vpsrad     ymm4,ymm4,(16-CONST_BITS)  

        vpsubd   ymm0,ymm5,ymm1               
        vpaddd   ymm5,ymm5,ymm1               
        vpsubd   ymm7,ymm4,ymm2               
        vpaddd   ymm4,ymm4,ymm2               

        vmovdqa  YMMWORD [wk(4)], ymm5   
        vmovdqa  YMMWORD [wk(5)], ymm4   
        vmovdqa  YMMWORD [wk(6)], ymm0   
        vmovdqa  YMMWORD [wk(7)], ymm7   

        ; -- Odd part

        vmovdqa  ymm6, YMMWORD [wk(9)]   
        vmovdqa  ymm3, YMMWORD [wk(8)]   
        vmovdqa  ymm1, YMMWORD [wk(11)]  
        vmovdqa  ymm2, YMMWORD [wk(10)]  

        vpaddw   ymm5,ymm6,ymm1               
        vpaddw   ymm4,ymm3,ymm2               

        ; (Original)
        ; z5 = (z3 + z4) * 1.175875602;
        ; z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
        ; z3 += z5;  z4 += z5;
        ;
        ; (This implementation)
        ; z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
        ; z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);

        vpunpckhwd ymm7,ymm5,ymm4
        vpunpcklwd ymm0,ymm5,ymm4
        vpmaddwd   ymm5,ymm0,[rel PW_F117_F078]       
        vpmaddwd   ymm4,ymm7,[rel PW_F117_F078]       
        vpmaddwd   ymm0,ymm0,[rel PW_MF078_F117]      
        vpmaddwd   ymm7,ymm7,[rel PW_MF078_F117]      

        vmovdqa  YMMWORD [wk(10)], ymm0  
        vmovdqa  YMMWORD [wk(11)], ymm7  

        ; (Original)
        ; z1 = tmp0 + tmp3;  z2 = tmp1 + tmp2;
        ; tmp0 = tmp0 * 0.298631336;  tmp1 = tmp1 * 2.053119869;
        ; tmp2 = tmp2 * 3.072711026;  tmp3 = tmp3 * 1.501321110;
        ; z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
        ; tmp0 += z1 + z3;  tmp1 += z2 + z4;
        ; tmp2 += z2 + z3;  tmp3 += z1 + z4;
        ;
        ; (This implementation)
        ; tmp0 = tmp0 * (0.298631336 - 0.899976223) + tmp3 * -0.899976223;
        ; tmp1 = tmp1 * (2.053119869 - 2.562915447) + tmp2 * -2.562915447;
        ; tmp2 = tmp1 * -2.562915447 + tmp2 * (3.072711026 - 2.562915447);
        ; tmp3 = tmp0 * -0.899976223 + tmp3 * (1.501321110 - 0.899976223);
        ; tmp0 += z3;  tmp1 += z4;
        ; tmp2 += z3;  tmp3 += z4;

        vpunpcklwd ymm0,ymm1,ymm3
        vpunpckhwd ymm7,ymm1,ymm3
        vpmaddwd   ymm1,ymm0,[rel PW_MF089_F060]      
        vpmaddwd   ymm3,ymm7,[rel PW_MF089_F060]      
        vpmaddwd   ymm0,ymm0,[rel PW_MF060_MF089]     
        vpmaddwd   ymm7,ymm7,[rel PW_MF060_MF089]     

        vpaddd   ymm0,ymm0, YMMWORD [wk(10)]  
        vpaddd   ymm7,ymm7, YMMWORD [wk(11)]  
        vpaddd   ymm1,ymm1,ymm5               
        vpaddd   ymm3,ymm3,ymm4               

        vmovdqa  YMMWORD [wk(8)], ymm0   
        vmovdqa  YMMWORD [wk(9)], ymm7   

        vpunpcklwd ymm0,ymm2,ymm6
        vpunpckhwd ymm7,ymm2,ymm6
        vpmaddwd   ymm2,ymm0,[rel PW_MF256_F050]      
        vpmaddwd   ymm6,ymm7,[rel PW_MF256_F050]      
        vpmaddwd   ymm0,ymm0,[rel PW_MF050_MF256]     
        vpmaddwd   ymm7,ymm7,[rel PW_MF050_MF256]     
        
        vpaddd   ymm0,ymm0,ymm5               
        vpaddd   ymm7,ymm7,ymm4               
        vpaddd   ymm2,ymm2, YMMWORD [wk(10)]  
        vpaddd   ymm6,ymm6, YMMWORD [wk(11)]  

        vmovdqa  YMMWORD [wk(10)], ymm0  
        vmovdqa  YMMWORD [wk(11)], ymm7  

        ; -- Final output stage

        vmovdqa  ymm5, YMMWORD [wk(0)]   
        vmovdqa  ymm4, YMMWORD [wk(1)]   

        vpsubd   ymm0,ymm5,ymm1               
        vpsubd   ymm7,ymm4,ymm3               
        vpaddd   ymm5,ymm5,ymm1               
        vpaddd   ymm4,ymm4,ymm3               

        vmovdqa  ymm1,[rel PD_DESCALE_P2]        

        vpaddd   ymm5,ymm5,ymm1
        vpaddd   ymm4,ymm4,ymm1
        vpsrad   ymm5,ymm5,DESCALE_P2
        vpsrad   ymm4,ymm4,DESCALE_P2
        vpaddd   ymm0,ymm0,ymm1
        vpaddd   ymm7,ymm7,ymm1
        vpsrad   ymm0,ymm0,DESCALE_P2
        vpsrad   ymm7,ymm7,DESCALE_P2

        vpackssdw  ymm5,ymm5,ymm4             
        vpackssdw  ymm0,ymm0,ymm7             
        vmovdqa  ymm3, YMMWORD [wk(4)]   
        vmovdqa  ymm1, YMMWORD [wk(5)]   

        vpsubd   ymm4,ymm3,ymm2               
        vpsubd   ymm7,ymm1,ymm6               
        vpaddd   ymm3,ymm3,ymm2               
        vpaddd   ymm1,ymm1,ymm6               
        
        vmovdqa  ymm2,[rel PD_DESCALE_P2]        

        vpaddd   ymm3,ymm3,ymm2
        vpaddd   ymm1,ymm1,ymm2
        vpsrad   ymm3,ymm3,DESCALE_P2
        vpsrad   ymm1,ymm1,DESCALE_P2
        vpaddd   ymm4,ymm4,ymm2
        vpaddd   ymm7,ymm7,ymm2
        vpsrad   ymm4,ymm4,DESCALE_P2
        vpsrad   ymm7,ymm7,DESCALE_P2

        vpackssdw  ymm3,ymm3,ymm1             
        vpackssdw  ymm4,ymm4,ymm7             

        vpacksswb  ymm5,ymm5,ymm4             
        vpacksswb  ymm3,ymm3,ymm0             

        vmovdqa  ymm6, YMMWORD [wk(6)]   
        vmovdqa  ymm2, YMMWORD [wk(7)]   
        vmovdqa  ymm1, YMMWORD [wk(10)]  
        vmovdqa  ymm7, YMMWORD [wk(11)]  

        vmovdqa  YMMWORD [wk(0)], ymm5   
        vmovdqa  YMMWORD [wk(1)], ymm3   

        vpsubd   ymm4,ymm6,ymm1               
        vpsubd   ymm0,ymm2,ymm7               
        vpaddd   ymm6,ymm6,ymm1               
        vpaddd   ymm2,ymm2,ymm7               
        
        vmovdqa  ymm5,[rel PD_DESCALE_P2]        

        vpaddd   ymm6,ymm6,ymm5
        vpaddd   ymm2,ymm2,ymm5
        vpsrad   ymm6,ymm6,DESCALE_P2
        vpsrad   ymm2,ymm2,DESCALE_P2
        vpaddd   ymm4,ymm4,ymm5
        vpaddd   ymm0,ymm0,ymm5
        vpsrad   ymm4,ymm4,DESCALE_P2
        vpsrad   ymm0,ymm0,DESCALE_P2

        vpackssdw  ymm6,ymm6,ymm2             
        vpackssdw  ymm4,ymm4,ymm0             

        vmovdqa  ymm3, YMMWORD [wk(2)]   
        vmovdqa  ymm1, YMMWORD [wk(3)]   
        vmovdqa  ymm7, YMMWORD [wk(8)]   
        vmovdqa  ymm5, YMMWORD [wk(9)]   

        vpsubd   ymm2,ymm3,ymm7               
        vpsubd   ymm0,ymm1,ymm5               
        vpaddd   ymm3,ymm3,ymm7               
        vpaddd   ymm1,ymm1,ymm5               
        
        vmovdqa  ymm7,[rel PD_DESCALE_P2]        

        vpaddd   ymm3,ymm3,ymm7
        vpaddd   ymm1,ymm1,ymm7
        vpsrad   ymm3,ymm3,DESCALE_P2
        vpsrad   ymm1,ymm1,DESCALE_P2
        vpaddd   ymm2,ymm2,ymm7
        vpaddd   ymm0,ymm0,ymm7
        vpsrad   ymm2,ymm2,DESCALE_P2
        vpsrad   ymm0,ymm0,DESCALE_P2

        vmovdqa    ymm5,[rel PB_CENTERJSAMP]     

        vpackssdw  ymm3,ymm3,ymm1             
        vpackssdw  ymm2,ymm2,ymm0             

        vmovdqa    ymm7, YMMWORD [wk(0)] 
        vmovdqa    ymm1, YMMWORD [wk(1)] 

        vpacksswb  ymm6,ymm6,ymm2             
        vpacksswb  ymm3,ymm3,ymm4             

        vpaddb     ymm7,ymm7,ymm5
        vpaddb     ymm1,ymm1,ymm5
        vpaddb     ymm6,ymm6,ymm5
        vpaddb     ymm3,ymm3,ymm5

        vpunpckhbw ymm0,ymm7,ymm1     
        vpunpcklbw ymm7,ymm7,ymm1     
        vpunpckhbw ymm2,ymm6,ymm3     
        vpunpcklbw ymm6,ymm6,ymm3     

        vpunpckhwd ymm4,ymm7,ymm6     
        vpunpcklwd ymm7,ymm7,ymm6     
        vpunpckhwd ymm5,ymm2,ymm0     
        vpunpcklwd ymm2,ymm2,ymm0     

        vpunpckhdq ymm1,ymm7,ymm2     
        vpunpckldq ymm7,ymm7,ymm2     
        vpunpckhdq ymm3,ymm4,ymm5     
        vpunpckldq ymm4,ymm4,ymm5     

        vpermq  ymm7,ymm7,0xd8      
        vperm2i128 ymm2,ymm7,ymm7,1 
        vpermq  ymm1,ymm1,0xd8      
        vperm2i128 ymm0,ymm1,ymm1,1 
        vpermq  ymm4,ymm4,0xd8      
        vperm2i128 ymm6,ymm4,ymm4,1 
        vpermq  ymm3,ymm3,0xd8      
        vperm2i128 ymm5,ymm3,ymm3,1 

        mov     rdx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+2*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm7
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm1
        mov     rdx, JSAMPROW [rdi+4*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+6*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm4
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm3

        mov     rdx, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+3*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm2
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm0
        mov     rdx, JSAMPROW [rdi+5*SIZEOF_JSAMPROW]
        mov     rsi, JSAMPROW [rdi+7*SIZEOF_JSAMPROW]
        vmovdqu    XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm6
        vmovdqu    XMM_MMWORD [rsi+rax*SIZEOF_JSAMPLE], xmm5

        uncollect_args
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32
