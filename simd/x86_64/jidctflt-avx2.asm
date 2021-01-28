;
; jidctflt.asm - floating-point IDCT (64-bit AVX & AVX2)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2009, 2016, D. R. Commander.
; Copyright (C) 2021, Ilya Kurdyukov <jpegqs@gmail.com>
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
; This file contains a floating-point implementation of the inverse DCT
; (Discrete Cosine Transform). The following code is based directly on
; the IJG's original jidctflt.c; see the jidctflt.c for more details.

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------

%macro quant_mul 4
    movups      xmm%1, XMMWORD [XMMBLOCK(%1,0,rsi,SIZEOF_JCOEF)]
    movups      xmm%2, XMMWORD [XMMBLOCK(%2,0,rsi,SIZEOF_JCOEF)]
    movups      xmm%3, XMMWORD [XMMBLOCK(%3,0,rsi,SIZEOF_JCOEF)]
    movups      xmm%4, XMMWORD [XMMBLOCK(%4,0,rsi,SIZEOF_JCOEF)]

    vpmovsxwd   ymm%1, xmm%1
    vpmovsxwd   ymm%2, xmm%2
    vpmovsxwd   ymm%3, xmm%3
    vpmovsxwd   ymm%4, xmm%4

    vcvtdq2ps   ymm%1, ymm%1
    vcvtdq2ps   ymm%2, ymm%2
    vcvtdq2ps   ymm%3, ymm%3
    vcvtdq2ps   ymm%4, ymm%4

    vmulps      ymm%1, YMMWORD [YMMBLOCK(%1,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
    vmulps      ymm%2, YMMWORD [YMMBLOCK(%2,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
    vmulps      ymm%3, YMMWORD [YMMBLOCK(%3,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
    vmulps      ymm%4, YMMWORD [YMMBLOCK(%4,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]
%endmacro

%macro even_part 0
    vmovaps     ymm10, [rel PD_1_414]

    ; -- Even part
    vsubps      ymm8, ymm0, ymm4        ; ymm3=tmp11
    vsubps      ymm9, ymm2, ymm6
    vaddps      ymm0, ymm0, ymm4        ; ymm0=tmp10
    vaddps      ymm2, ymm2, ymm6        ; ymm2=tmp13

    vmulps      ymm9, ymm10
    vsubps      ymm9, ymm2              ; ymm9=tmp12

    vsubps      ymm4, ymm0, ymm2        ; ymm4=tmp3
    vsubps      ymm6, ymm8, ymm9        ; ymm6=tmp2
    vaddps      ymm0, ymm0, ymm2        ; ymm0=tmp0
    vaddps      ymm8, ymm8, ymm9        ; ymm8=tmp1
%endmacro

%macro odd_part 0
    ; -- Odd part
    vaddps      ymm9, ymm1, ymm7        ; ymm9=z11
    vaddps      ymm2, ymm5, ymm3        ; ymm2=z13
    vsubps      ymm1, ymm1, ymm7        ; ymm1=z12
    vsubps      ymm5, ymm5, ymm3        ; ymm5=z10

    vsubps      ymm7, ymm9, ymm2
    vaddps      ymm9, ymm9, ymm2        ; ymm9=tmp7
    vaddps      ymm3, ymm5, ymm1

    vmulps      ymm7, ymm10             ; ymm7=tmp11
    vmulps      ymm3, [rel PD_1_847]    ; ymm3=z5
    vmulps      ymm5, [rel PD_M2_613]   ; ymm5=(z10 * -2.613125930)
    vmulps      ymm1, [rel PD_1_082]    ; ymm1=(z12 * 1.082392200)
    vaddps      ymm5, ymm3              ; ymm5=tmp12
    vsubps      ymm1, ymm3              ; ymm1=tmp10
%endmacro

%macro final_part 1
    vsubps      ymm5, ymm9              ; ymm5=tmp6
    vsubps      ymm2, ymm0, ymm9        ; ymm2=data7=(07 17 27 37)
    vsubps      ymm3, ymm8, ymm5        ; ymm3=data6=(06 16 26 36)
    vsubps      ymm7, ymm5              ; ymm7=tmp5
    vaddps      ymm9, ymm0              ; ymm9=data0=(00 10 20 30)
    vaddps      ymm8, ymm5              ; ymm8=data1=(01 11 21 31)
    vaddps      ymm1, ymm7              ; ymm1=tmp4
%if %1
    vmovaps     ymm0, [rel PD_RNDINT_MAGIC]
%endif
    vaddps      ymm10, ymm6, ymm7       ; ymm10=data2=(20 21 22 23)
    vaddps      ymm11, ymm4, ymm1       ; ymm11=data4=(40 41 42 43)
    vsubps      ymm6, ymm7              ; ymm6 =data5=(50 51 52 53)
    vsubps      ymm4, ymm1              ; ymm4 =data3=(30 31 32 33)
%endmacro

; --------------------------------------------------------------------------
    SECTION     SEG_CONST

    alignz      32
    GLOBAL_DATA(jconst_idct_float_avx2)

EXTN(jconst_idct_float_avx2):

PD_1_414        times 8  dd  1.414213562373095048801689
PD_1_847        times 8  dd  1.847759065022573512256366
PD_1_082        times 8  dd  1.082392200292393968799446
PD_M2_613       times 8  dd -2.613125929752753055713286
; (float)((0x00C00000 + CENTERJSAMPLE) << 3)
PD_RNDINT_MAGIC times 8  dd  0x4cc00000 + CENTERJSAMPLE

    alignz      32

; --------------------------------------------------------------------------
    SECTION     SEG_TEXT
    BITS        64
;
; Perform dequantization and inverse DCT on one block of coefficients.
;
; GLOBAL(void)
; jsimd_idct_float_avx2(void *dct_table, JCOEFPTR coef_block,
;                       JSAMPARRAY output_buf, JDIMENSION output_col)
;

; r10 = void *dct_table
; r11 = JCOEFPTR coef_block
; r12 = JSAMPARRAY output_buf
; r13d = JDIMENSION output_col

    align       32
    GLOBAL_FUNCTION(jsimd_idct_float_avx2)

EXTN(jsimd_idct_float_avx2):
    push        rbp
    mov         rax, rsp                     ; rax = original rbp
    push        rcx
    and         rsp, byte (-SIZEOF_XMMWORD)  ; align to 128 bits
    mov         [rsp], rax
    mov         rbp, rsp                     ; rbp = aligned rbp
    push_xmm    4
    collect_args 4

    ; ---- Pass 1: process columns from input, store into work array.

    mov         rdx, r10                ; quantptr
    mov         rsi, r11                ; inptr

%ifndef NO_ZERO_COLUMN_TEST_FLOAT_AVX2
    mov         eax, dword [DWBLOCK(1,0,rsi,SIZEOF_JCOEF)]
    or          eax, dword [DWBLOCK(2,0,rsi,SIZEOF_JCOEF)]
    jnz         near .columnDCT

    movdqa      xmm0, XMMWORD [XMMBLOCK(1,0,rsi,SIZEOF_JCOEF)]
    movdqa      xmm1, XMMWORD [XMMBLOCK(2,0,rsi,SIZEOF_JCOEF)]
    por         xmm0, XMMWORD [XMMBLOCK(3,0,rsi,SIZEOF_JCOEF)]
    por         xmm1, XMMWORD [XMMBLOCK(4,0,rsi,SIZEOF_JCOEF)]
    por         xmm0, XMMWORD [XMMBLOCK(5,0,rsi,SIZEOF_JCOEF)]
    por         xmm1, XMMWORD [XMMBLOCK(6,0,rsi,SIZEOF_JCOEF)]
    por         xmm0, XMMWORD [XMMBLOCK(7,0,rsi,SIZEOF_JCOEF)]
    por         xmm1, xmm0
    packsswb    xmm1, xmm1
    movq        rax, xmm1
    test        rax, rax
    jnz         short .columnDCT

    ; -- AC terms all zero

    movups      xmm0, XMMWORD [XMMBLOCK(0,0,rsi,SIZEOF_JCOEF)]

    vpmovsxwd   ymm0, xmm0
    vcvtdq2ps   ymm0, ymm0                  ; xmm0=in0=(00 01 02 03)

    vmulps      ymm0, YMMWORD [YMMBLOCK(0,0,rdx,SIZEOF_FLOAT_MULT_TYPE)]

    vshufps     ymm4, ymm0, ymm0, 0x00      ; ymm0=(00 00 00 00)
    vshufps     ymm5, ymm0, ymm0, 0x55      ; ymm1=(01 01 01 01)
    vshufps     ymm6, ymm0, ymm0, 0xAA      ; ymm2=(02 02 02 02)
    vshufps     ymm7, ymm0, ymm0, 0xFF      ; xmm3=(03 03 03 03)

    vpermq      ymm0, ymm4, 0x44
    vpermq      ymm1, ymm5, 0x44
    vpermq      ymm2, ymm6, 0x44
    vpermq      ymm3, ymm7, 0x44
    vpermq      ymm4, ymm4, 0xEE
    vpermq      ymm5, ymm5, 0xEE
    vpermq      ymm6, ymm6, 0xEE
    vpermq      ymm7, ymm7, 0xEE
    jmp         near .column_end
.columnDCT:
%endif

    quant_mul 0, 2, 4, 6
    even_part
    quant_mul 1, 3, 5, 7
    odd_part
    final_part 0

                                        ; transpose coefficients(phase 1)
    vunpcklps   ymm5, ymm9, ymm10       ; ymm5=(00 20 01 21)
    vunpckhps   ymm9, ymm9, ymm10       ; ymm9=(02 22 03 23)
    vunpcklps   ymm1, ymm8, ymm4        ; ymm1=(10 30 11 31)
    vunpckhps   ymm8, ymm8, ymm4        ; ymm8=(12 32 13 33)
    vunpcklps   ymm10, ymm11, ymm3      ; ymm10=(40 60 41 61)
    vunpckhps   ymm11, ymm11, ymm3      ; ymm11=(42 62 43 63)
    vunpcklps   ymm3, ymm6, ymm2        ; ymm3=(50 70 51 71)
    vunpckhps   ymm6, ymm6, ymm2        ; ymm6=(52 72 53 73)

                                        ; transpose coefficients(phase 2)
    vunpcklps   ymm4, ymm5, ymm1        ; ymm4=(00 10 20 30)
    vunpckhps   ymm5, ymm5, ymm1        ; ymm5=(01 11 21 31)
    vunpcklps   ymm7, ymm9, ymm8        ; ymm7=(02 12 22 32)
    vunpckhps   ymm9, ymm9, ymm8        ; ymm9=(03 13 23 33)

    vunpcklps   ymm1, ymm10, ymm3       ; ymm1=(40 50 60 70)
    vunpckhps   ymm10, ymm10, ymm3      ; ymm10=(41 51 61 71)
    vunpcklps   ymm8, ymm11, ymm6       ; ymm8=(42 52 62 72)
    vunpckhps   ymm11, ymm11, ymm6      ; ymm11=(43 53 63 73)

    ; transpose coefficients(phase 3)
    vperm2f128  ymm0, ymm4, ymm1, 0x20
    vperm2f128  ymm4, ymm4, ymm1, 0x31
    vperm2f128  ymm1, ymm5, ymm10, 0x20
    vperm2f128  ymm5, ymm5, ymm10, 0x31
    vperm2f128  ymm2, ymm7, ymm8, 0x20
    vperm2f128  ymm6, ymm7, ymm8, 0x31
    vperm2f128  ymm3, ymm9, ymm11, 0x20
    vperm2f128  ymm7, ymm9, ymm11, 0x31

.column_end:

    ; -- Prefetch the next coefficient block

    prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 0*32]
    prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 1*32]
    prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 2*32]
    prefetchnta [rsi + DCTSIZE2*SIZEOF_JCOEF + 3*32]

    ; ---- Pass 2: process rows from work array, store into output array.

    mov         rdi, r12                ; (JSAMPROW *)
    mov         eax, r13d

    even_part
    odd_part
    final_part 1

    vaddps      ymm9, ymm0              ; ymm9 =roundint(data0/8)=(00 ** 10 ** 20 ** 30 **)
    vaddps      ymm8, ymm0              ; ymm8 =roundint(data1/8)=(01 ** 11 ** 21 ** 31 **)
    vaddps      ymm10, ymm0             ; ymm10=roundint(data2/8)=(02 ** 12 ** 22 ** 32 **)
    vaddps      ymm4, ymm0              ; ymm4 =roundint(data3/8)=(03 ** 13 ** 23 ** 33 **)
    vaddps      ymm11, ymm0             ; ymm11=roundint(data4/8)=(04 ** 14 ** 24 ** 34 **)
    vaddps      ymm6, ymm0              ; ymm6 =roundint(data5/8)=(05 ** 15 ** 25 ** 35 **)
    vaddps      ymm3, ymm0              ; ymm3 =roundint(data6/8)=(06 ** 16 ** 26 ** 36 **)
    vaddps      ymm2, ymm0              ; ymm2 =roundint(data7/8)=(07 ** 17 ** 27 ** 37 **)

    vpslld      ymm8, WORD_BIT          ; ymm8=(-- 01 -- 11 -- 21 -- 31)
    vpslld      ymm4, WORD_BIT          ; ymm4=(-- 03 -- 13 -- 23 -- 33)
    vpslld      ymm6, WORD_BIT          ; ymm6=(-- 05 -- 15 -- 25 -- 35)
    vpslld      ymm2, WORD_BIT          ; ymm2=(-- 07 -- 17 -- 27 -- 37)
    vpblendw    ymm9, ymm9, ymm8, 0xaa  ; ymm9=(00 01 10 11 20 21 30 31)
    vpblendw    ymm0, ymm10, ymm4, 0xaa ; ymm0=(02 03 12 13 22 23 32 33)
    vpblendw    ymm5, ymm11, ymm6, 0xaa ; ymm5=(04 05 14 15 24 25 34 35)
    vpblendw    ymm3, ymm3, ymm2, 0xaa  ; ymm3=(06 07 16 17 26 27 36 37)

                                  ; transpose coefficients(phase 1)
    vpackuswb   ymm9, ymm5        ; ymm9=(00 01 10 11 20 21 30 31 04 05 14 15 24 25 34 35)
    vpackuswb   ymm0, ymm3        ; ymm0=(02 03 12 13 22 23 32 33 06 07 16 17 26 27 36 37)

                                  ; transpose coefficients(phase 2)
    vpunpcklwd  ymm1, ymm9, ymm0  ; ymm1=(00 01 02 03 10 11 12 13 20 21 22 23 30 31 32 33)
    vpunpckhwd  ymm9, ymm9, ymm0  ; ymm9=(04 05 06 07 14 15 16 17 24 25 26 27 34 35 36 37)

                                  ; transpose coefficients(phase 3)
    vpunpckldq  ymm8, ymm1, ymm9  ; ymm9=(00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17)
    vpunpckhdq  ymm1, ymm1, ymm9  ; ymm8=(20 21 22 23 24 25 26 27 30 31 32 33 34 35 36 37)

    vpshufd     ymm2, ymm8, 0x4E  ; ymm2=(10 11 12 13 14 15 16 17 00 01 02 03 04 05 06 07)
    vpshufd     ymm5, ymm1, 0x4E  ; ymm5=(30 31 32 33 34 35 36 37 20 21 22 23 24 25 26 27)

    vextracti128 xmm9, ymm8, 1
    vextracti128 xmm0, ymm1, 1
    vextracti128 xmm3, ymm2, 1
    vextracti128 xmm4, ymm5, 1

    mov         rdx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]
    mov         rcx, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]
    movq        XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm8
    movq        XMM_MMWORD [rcx+rax*SIZEOF_JSAMPLE], xmm2
    mov         rdx, JSAMPROW [rdi+2*SIZEOF_JSAMPROW]
    mov         rcx, JSAMPROW [rdi+3*SIZEOF_JSAMPROW]
    movq        XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm1
    movq        XMM_MMWORD [rcx+rax*SIZEOF_JSAMPLE], xmm5
    mov         rdx, JSAMPROW [rdi+4*SIZEOF_JSAMPROW]
    mov         rcx, JSAMPROW [rdi+5*SIZEOF_JSAMPROW]
    movq        XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm9
    movq        XMM_MMWORD [rcx+rax*SIZEOF_JSAMPLE], xmm3
    mov         rdx, JSAMPROW [rdi+6*SIZEOF_JSAMPROW]
    mov         rcx, JSAMPROW [rdi+7*SIZEOF_JSAMPROW]
    movq        XMM_MMWORD [rdx+rax*SIZEOF_JSAMPLE], xmm0
    movq        XMM_MMWORD [rcx+rax*SIZEOF_JSAMPLE], xmm4

    uncollect_args 4
    pop_xmm     4
    mov         rsp, [rbp]              ; rsp <- original rbp
    pop         rbp
    ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
