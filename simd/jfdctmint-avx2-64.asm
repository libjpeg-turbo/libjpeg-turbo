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

%define CONST_BITS      13
%define PASS1_BITS      2

%define DESCALE_P1      (CONST_BITS-PASS1_BITS)
%define DESCALE_P2      (CONST_BITS+PASS1_BITS)

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

        SECTION SEG_CONST

%define PRE_MULTIPLY_SCALE_BITS   2
%define CONST_SHIFT     (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS)
        alignz  32
        alignz  32
        global  EXTN(jconst_fdct_merged_islow_avx2)

EXTN(jconst_fdct_merged_islow_avx2):

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
PW_DESCALE_P2X  times 16 dw  1 << (PASS1_BITS-1)


; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64

; --------------------------------------------------------------------------
        align   32
        global  EXTN(jsimd_fdct_merged_islow_avx2)

EXTN(jsimd_fdct_merged_islow_avx2):
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
        mov     rdx, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]       

        vmovdqa    xmm0, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm0, ymm0, 0x10
        vmovdqa    xmm1, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm1, ymm1, 0x10

        mov     rbx, JSAMPROW [rsi+2*SIZEOF_JSAMPROW]   
        mov     rdx, JSAMPROW [rsi+3*SIZEOF_JSAMPROW]   

        vmovdqa    xmm2, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm2, ymm2, 0x10
        vmovdqa    xmm3, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm3, ymm3, 0x10

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

        vmovdqa    xmm8, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm8, ymm8, 0x10
        vmovdqa    xmm9, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm9, ymm9, 0x10

        mov     rbx, JSAMPROW [rsi+6*SIZEOF_JSAMPROW]   
        mov     rdx, JSAMPROW [rsi+7*SIZEOF_JSAMPROW]   

        vmovdqa    xmm10, XMM_MMWORD [rbx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm10, ymm10, 0x10
        vmovdqa    xmm11, XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE]       
        vpermq     ymm11, ymm11, 0x10

        vpunpcklbw ymm8,ymm8,ymm14             
        vpunpcklbw ymm9,ymm9,ymm14             
        vpaddw     ymm4,ymm8,ymm15
        vpaddw     ymm5,ymm9,ymm15
        vpunpcklbw ymm10,ymm10,ymm14             
        vpunpcklbw ymm11,ymm11,ymm14             
        vpaddw     ymm6,ymm10,ymm15
        vpaddw     ymm7,ymm11,ymm15

        
        

.dct:

        mov     rcx, 2

.dctloop:
            dec     rcx
        
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
        test     rcx,rcx
        jnz     near .row_data04
        vpaddw   ymm0,ymm0,[rel PW_DESCALE_P2X]
        vpaddw   ymm4,ymm4,[rel PW_DESCALE_P2X]
        vpsraw   ymm0,ymm0,PASS1_BITS         
        vpsraw   ymm4,ymm4,PASS1_BITS         
        jmp near .data04
.row_data04:
        vpsllw ymm0,ymm0,PASS1_BITS
        vpsllw ymm4,ymm4,PASS1_BITS
.data04:
        vpunpcklwd ymm14,ymm13,ymm12
        vpunpckhwd ymm15,ymm13,ymm12
        vpmaddwd   ymm10,ymm14,[rel PW_F130_F054]     
        vpmaddwd   ymm11,ymm15,[rel PW_F130_F054]     
        vpmaddwd   ymm12,ymm14,[rel PW_F054_MF130]    
        vpmaddwd   ymm13,ymm15,[rel PW_F054_MF130]    
        test rcx,rcx
        jnz near .row_data26
        vpaddd   ymm10,ymm10,[rel PD_DESCALE_P2]
        vpaddd   ymm11,ymm11,[rel PD_DESCALE_P2]
        vpaddd   ymm12,ymm12,[rel PD_DESCALE_P2]
        vpaddd   ymm13,ymm13,[rel PD_DESCALE_P2]
        vpsrad   ymm10,ymm10,DESCALE_P2
        vpsrad   ymm11,ymm11,DESCALE_P2
        vpsrad   ymm12,ymm12,DESCALE_P2
        vpsrad   ymm13,ymm13,DESCALE_P2
        jmp near .data26
.row_data26:
        vpaddd   ymm10,ymm10,[rel PD_DESCALE_P1]
        vpaddd   ymm11,ymm11,[rel PD_DESCALE_P1]
        vpaddd   ymm12,ymm12,[rel PD_DESCALE_P1]
        vpaddd   ymm13,ymm13,[rel PD_DESCALE_P1]
        vpsrad   ymm10,ymm10,DESCALE_P1
        vpsrad   ymm11,ymm11,DESCALE_P1
        vpsrad   ymm12,ymm12,DESCALE_P1
        vpsrad   ymm13,ymm13,DESCALE_P1
.data26:
        vpackssdw  ymm2,ymm10,ymm11             ; ymm2=data2
        vpackssdw  ymm6,ymm12,ymm13             ; ymm6=data6

.ck_data26:
        ; -- Odd part

        vpaddw ymm12, ymm9, ymm8 ;z3
        vpaddw ymm13, ymm5, ymm7 ;z4

        ; (Original)
        ; z5 = (z3 + z4) * 1.175875602;
        ; z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
        ; z3 += z5;  z4 += z5;
        ;
        ; (This implementation)
        ; z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
        ; z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);

        vpunpcklwd ymm10, ymm12,ymm13
        vpunpckhwd ymm11, ymm12,ymm13
        vpmaddwd   ymm12,ymm10,[rel PW_MF078_F117]      ; ymm12=z3L
        vpmaddwd   ymm13,ymm11,[rel PW_MF078_F117]      ; ymm13=z3H
        vpmaddwd   ymm10,ymm10,[rel PW_F117_F078]       ; ymm10=z4L
        vpmaddwd   ymm11,ymm11,[rel PW_F117_F078]       ; ymm11=z4H

        ; (Original)
        ; z1 = tmp4 + tmp7;  z2 = tmp5 + tmp6;
        ; tmp4 = tmp4 * 0.298631336;  tmp5 = tmp5 * 2.053119869;
        ; tmp6 = tmp6 * 3.072711026;  tmp7 = tmp7 * 1.501321110;
        ; z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
        ; data7 = tmp4 + z1 + z3;  data5 = tmp5 + z2 + z4;
        ; data3 = tmp6 + z2 + z3;  data1 = tmp7 + z1 + z4;
        ;
        ; (This implementation)
        ; tmp4 = tmp4 * (0.298631336 - 0.899976223) + tmp7 * -0.899976223;
        ; tmp5 = tmp5 * (2.053119869 - 2.562915447) + tmp6 * -2.562915447;
        ; tmp6 = tmp5 * -2.562915447 + tmp6 * (3.072711026 - 2.562915447);
        ; tmp7 = tmp4 * -0.899976223 + tmp7 * (1.501321110 - 0.899976223);
        ; data7 = tmp4 + z3;  data5 = tmp5 + z4;
        ; data3 = tmp6 + z3;  data1 = tmp7 + z4;

        vpunpcklwd ymm1,ymm9,ymm7 ; ymm7 = tmp7, ymm9 = tmp4
        vpunpckhwd ymm3,ymm9,ymm7
        vpmaddwd   ymm15,ymm1,[rel PW_MF060_MF089]     ; ymm15=tmp4L
        vpmaddwd   ymm7,ymm3,[rel PW_MF060_MF089]     ; ymm7=tmp4H
        vpmaddwd   ymm1,ymm1,[rel PW_MF089_F060]      ; ymm1=tmp7L
        vpmaddwd   ymm3,ymm3,[rel PW_MF089_F060]      ; ymm3=tmp7H

        vpaddd   ymm15,ymm15, ymm12   ; ymm15=data7L
        vpaddd   ymm7,ymm7, ymm13   ; ymm7=data7H
        vpaddd   ymm1,ymm1,ymm10    ; ymm1=data1L
        vpaddd   ymm3,ymm3,ymm11    ; ymm3=data1H

        test rcx,rcx
        jnz near .row_data17
        vpaddd   ymm15,ymm15,[rel PD_DESCALE_P2]
        vpaddd   ymm7,ymm7,[rel PD_DESCALE_P2]
        vpsrad   ymm15,ymm15,DESCALE_P2
        vpsrad   ymm7,ymm7,DESCALE_P2
        vpaddd   ymm1,ymm1,[rel PD_DESCALE_P2]
        vpaddd   ymm3,ymm3,[rel PD_DESCALE_P2]
        vpsrad   ymm1,ymm1,DESCALE_P2
        vpsrad   ymm3,ymm3,DESCALE_P2
        jmp near .data17
.row_data17:
        vpaddd   ymm15,ymm15,[rel PD_DESCALE_P1]
        vpaddd   ymm7,ymm7,[rel PD_DESCALE_P1]
        vpsrad   ymm15,ymm15,DESCALE_P1
        vpsrad   ymm7,ymm7,DESCALE_P1
        vpaddd   ymm1,ymm1,[rel PD_DESCALE_P1]
        vpaddd   ymm3,ymm3,[rel PD_DESCALE_P1]
        vpsrad   ymm1,ymm1,DESCALE_P1
        vpsrad   ymm3,ymm3,DESCALE_P1
.data17:
        vpackssdw  ymm7,ymm15,ymm7             ; ymm7=data7
        vpackssdw  ymm1,ymm1,ymm3             ; ymm1=data1
.ck_data17:

        vpunpcklwd ymm3,ymm5,ymm8
        vpunpckhwd ymm5,ymm5,ymm8

        vpmaddwd   ymm14,ymm3,[rel PW_MF050_MF256]     ; ymm14=tmp5L
        vpmaddwd   ymm15,ymm5,[rel PW_MF050_MF256]     ; ymm15=tmp5H
        vpmaddwd   ymm3,ymm3,[rel PW_MF256_F050]      ; ymm3=tmp6L
        vpmaddwd   ymm5,ymm5,[rel PW_MF256_F050]      ; ymm5=tmp6H

        vpaddd   ymm14,ymm14,ymm10               ; ymm1=data5L
        vpaddd   ymm15,ymm15,ymm11               ; ymm5=data5H
        vpaddd   ymm3,ymm3, ymm12   ; ymm5=data3L
        vpaddd   ymm5,ymm5, ymm13   ; ymm3=data3H
        test rcx,rcx
        jnz near .row_data35
        vpaddd   ymm14,ymm14,[rel PD_DESCALE_P2]
        vpaddd   ymm15,ymm15,[rel PD_DESCALE_P2]
        vpsrad   ymm14,ymm14,DESCALE_P2
        vpsrad   ymm15,ymm15,DESCALE_P2
        vpaddd   ymm10,ymm3,[rel PD_DESCALE_P2]
        vpaddd   ymm11,ymm5,[rel PD_DESCALE_P2]
        vpsrad   ymm10,ymm10,DESCALE_P2
        vpsrad   ymm11,ymm11,DESCALE_P2
        jmp .data35
.row_data35:
        vpaddd   ymm14,ymm14,[rel PD_DESCALE_P1]
        vpaddd   ymm15,ymm15,[rel PD_DESCALE_P1]
        vpsrad   ymm14,ymm14,DESCALE_P1
        vpsrad   ymm15,ymm15,DESCALE_P1
        vpaddd   ymm10,ymm3,[rel PD_DESCALE_P1]
        vpaddd   ymm11,ymm5,[rel PD_DESCALE_P1]
        vpsrad   ymm10,ymm10,DESCALE_P1
        vpsrad   ymm11,ymm11,DESCALE_P1
.data35:
        vpackssdw  ymm5,ymm14,ymm15             ; ymm5=data5
        vpackssdw  ymm3,ymm10,ymm11             ; ymm3=data3
.ck_data35:
            test rcx,rcx
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
        vpsubw   ymm0,ymm0,ymm8               ; if (ymm0 < 0) ymm0 = -ymm0;
        vpsubw   ymm1,ymm1,ymm9               ; if (ymm1 < 0) ymm1 = -ymm1;
        vpsubw   ymm2,ymm2,ymm10               ; if (ymm2 < 0) ymm2 = -ymm2;
        vpsubw   ymm3,ymm3,ymm11               ; if (ymm3 < 0) ymm3 = -ymm3;

        vpaddw   ymm0,ymm0, YMMWORD [CORRECTION(0,0,rdx)]  ; correction + roundfactor
        vpaddw   ymm1,ymm1, YMMWORD [CORRECTION(1,0,rdx)]
        vpaddw   ymm2,ymm2, YMMWORD [CORRECTION(2,0,rdx)]
        vpaddw   ymm3,ymm3, YMMWORD [CORRECTION(3,0,rdx)]
        vpmulhuw ymm0,ymm0, YMMWORD [RECIPROCAL(0,0,rdx)]  ; reciprocal
        vpmulhuw ymm1,ymm1, YMMWORD [RECIPROCAL(1,0,rdx)]
        vpmulhuw ymm2,ymm2, YMMWORD [RECIPROCAL(2,0,rdx)]
        vpmulhuw ymm3,ymm3, YMMWORD [RECIPROCAL(3,0,rdx)]
        vpmulhuw ymm0,ymm0, YMMWORD [SCALE(0,0,rdx)]  ; scale
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

; next half

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

        vpaddw   ymm8,ymm8, YMMWORD [CORRECTION(4,0,rdx)]  ; correction + roundfactor
        vpaddw   ymm9,ymm9, YMMWORD [CORRECTION(5,0,rdx)]
        vpaddw   ymm10,ymm10, YMMWORD [CORRECTION(6,0,rdx)]
        vpaddw   ymm11,ymm11, YMMWORD [CORRECTION(7,0,rdx)]
        vpmulhuw ymm8,ymm8, YMMWORD [RECIPROCAL(4,0,rdx)]  ; reciprocal
        vpmulhuw ymm9,ymm9, YMMWORD [RECIPROCAL(5,0,rdx)]
        vpmulhuw ymm10,ymm10, YMMWORD [RECIPROCAL(6,0,rdx)]
        vpmulhuw ymm11,ymm11, YMMWORD [RECIPROCAL(7,0,rdx)]
        vpmulhuw ymm8,ymm8, YMMWORD [SCALE(4,0,rdx)]  ; scale
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

        ;ymm0 = [blk0 low128][blk1 low128]
        ;ymmN = [blk0 128<<N][blk1 128<<N]
        ;vperm2i128 bit1:0 = 00b,bit5:4=10b
        ;src2[127:0] -> dest[255:128] ; src1[127:0] -> dest[127:0]
        vperm2i128 ymm8,ymm0,ymm1,0x20
        vperm2i128 ymm9,ymm2,ymm3,0x20
        vperm2i128 ymm10,ymm4,ymm5,0x20
        vperm2i128 ymm11,ymm6,ymm7,0x20
        ;vperm2i128 bit1:0 = 01b,bit5:4=11b
        ;src2[255:128] -> dest[255:128] ; src1[255:128] -> dest[127:0]
        vperm2i128 ymm0,ymm0,ymm1,0x31
        vperm2i128 ymm1,ymm2,ymm3,0x31
        vperm2i128 ymm2,ymm4,ymm5,0x31
        vperm2i128 ymm3,ymm6,ymm7,0x31

        vmovdqa  YMMWORD [YMMBLOCK(0,0,rdi,2*SIZEOF_DCTELEM)], ymm8
        vmovdqa  YMMWORD [YMMBLOCK(1,0,rdi,2*SIZEOF_DCTELEM)], ymm9
        vmovdqa  YMMWORD [YMMBLOCK(2,0,rdi,2*SIZEOF_DCTELEM)], ymm10
        vmovdqa  YMMWORD [YMMBLOCK(3,0,rdi,2*SIZEOF_DCTELEM)], ymm11
        vmovdqa  YMMWORD [YMMBLOCK(4,0,rdi,2*SIZEOF_DCTELEM)], ymm0
        vmovdqa  YMMWORD [YMMBLOCK(5,0,rdi,2*SIZEOF_DCTELEM)], ymm1
        vmovdqa  YMMWORD [YMMBLOCK(6,0,rdi,2*SIZEOF_DCTELEM)], ymm2
        vmovdqa  YMMWORD [YMMBLOCK(7,0,rdi,2*SIZEOF_DCTELEM)], ymm3

.exit:


        uncollect_args
        pop     rbx
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32



