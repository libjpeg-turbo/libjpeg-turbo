;
; jdsample.asm - upsampling (64-bit AVX512)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2009, 2016, D. R. Commander.
; Copyright (C) 2015, 2018, Intel Corporation.
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
; [TAB8]

%include "jsimdext.inc"

; --------------------------------------------------------------------------
        SECTION SEG_CONST

        alignz  64
        global  EXTN(jconst_fancy_upsample_avx512)

EXTN(jconst_fancy_upsample_avx512):

PW_ONE          times 32 dw  1
PW_TWO          times 32 dw  2
PW_THREE        times 32 dw  3
PW_SEVEN        times 32 dw  7
PW_EIGHT        times 32 dw  8

        alignz  64

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Fancy processing for the common case of 2:1 horizontal and 1:1 vertical.
;
; The upsampling algorithm is linear interpolation between pixel centers,
; also known as a "triangle filter".  This is a good compromise between
; speed and visual quality.  The centers of the output pixels are 1/4 and 3/4
; of the way between input pixel centers.
;
; GLOBAL(void)
; jsimd_h2v1_fancy_upsample_avx512 (int max_v_samp_factor,
;                                 JDIMENSION downsampled_width,
;                                 JSAMPARRAY input_data,
;                                 JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11d = JDIMENSION downsampled_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

        align   64
        global  EXTN(jsimd_h2v1_fancy_upsample_avx512)

EXTN(jsimd_h2v1_fancy_upsample_avx512):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args 4

        mov     rax, r11  ; colctr
        test    rax,rax
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      near .return

        mov     rsi, r12        ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data

        vpxor   ymm0,ymm0,ymm0               ; ymm0=(all 0's)
        vpcmpeqb xmm14,xmm14,xmm14
        vpsrldq  xmm15,xmm14,(SIZEOF_XMMWORD-1); (ff -- -- -- ... -- --); LSB is ff

        vpslldq xmm14,xmm14,(SIZEOF_XMMWORD-1)
        vshufi32x4 zmm14, zmm14, zmm14, 0b00010101 ;(---- ---- ... ---- ---- ff) MSB is ff

.rowloop:
        push    rax                     ; colctr
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]     ; inptr
        mov     rdi, JSAMPROW [rdi]     ; outptr

        test    rax, SIZEOF_ZMMWORD-1
        jz      short .skip
        mov     dl, JSAMPLE [rsi+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rsi+rax*SIZEOF_JSAMPLE], dl    ; insert a dummy sample
.skip:
        vpandd  zmm7,zmm15, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]

        add     rax, byte SIZEOF_ZMMWORD-1
        and     rax, byte -SIZEOF_ZMMWORD
        cmp     rax, byte SIZEOF_ZMMWORD
        ja      short .columnloop

.columnloop_last:
        vpandd  zmm6,zmm14, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        jmp     short .upsample

.columnloop:
        vpxord  zmm6,zmm6,zmm6
        movdqa  xmm6, XMMWORD [rsi+1*SIZEOF_ZMMWORD]
        pslldq  xmm6,(SIZEOF_XMMWORD-1)
        vshufi32x4 zmm6, zmm6, zmm6, 0b00010101

.upsample:
        vmovdqu64  zmm1, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqa64  zmm2,zmm1
        vmovdqa64  zmm3,zmm1
        vmovdqa64  zmm4,zmm1

        vshufi32x4 zmm8, zmm2, zmm2, 0b10010011
        pxor xmm8, xmm8
        vpalignr zmm2,zmm2,zmm8,15

        pxor xmm4, xmm4
        vshufi32x4 zmm8, zmm4, zmm4, 0b00111001
        vpalignr zmm3,zmm8,zmm3,1

        vpord     zmm2,zmm2,zmm7
        vpord     zmm3,zmm3,zmm6

        vshufi32x4 zmm8, zmm1, zmm0, 0b00000011
        vshufi32x4 zmm8, zmm8, zmm8, 0b11111100
        vpsrldq  zmm7,zmm8,(SIZEOF_XMMWORD-1)

        vpunpckhbw zmm4,zmm1,zmm0
        vpunpcklbw zmm8,zmm1,zmm0
        vshufi32x4 zmm9, zmm4, zmm4, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm1, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm4, zmm10, zmm10, 0b01110010

        vpunpckhbw zmm5,zmm2,zmm0             
        vpunpcklbw zmm8,zmm2,zmm0             
        vshufi32x4 zmm9, zmm5, zmm5, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm2, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm5, zmm10, zmm10, 0b01110010

        vpunpckhbw zmm6,zmm3,zmm0             
        vpunpcklbw zmm8,zmm3,zmm0             
        vshufi32x4 zmm9, zmm6, zmm6, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm3, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm6, zmm10, zmm10, 0b01110010

        vpmullw  zmm1,zmm1,[rel PW_THREE]
        vpmullw  zmm4,zmm4,[rel PW_THREE]
        vpaddw   zmm2,zmm2,[rel PW_ONE]
        vpaddw   zmm5,zmm5,[rel PW_ONE]
        vpaddw   zmm3,zmm3,[rel PW_TWO]
        vpaddw   zmm6,zmm6,[rel PW_TWO]

        vpaddw   zmm2,zmm2,zmm1
        vpaddw   zmm5,zmm5,zmm4
        vpsrlw   zmm2,zmm2,2                  
        vpsrlw   zmm5,zmm5,2                  
        vpaddw   zmm3,zmm3,zmm1
        vpaddw   zmm6,zmm6,zmm4
        vpsrlw   zmm3,zmm3,2                  
        vpsrlw   zmm6,zmm6,2                  

        vpsllw   zmm3,zmm3,BYTE_BIT
        vpsllw   zmm6,zmm6,BYTE_BIT
        vpord    zmm2,zmm2,zmm3               
        vpord    zmm5,zmm5,zmm6               

        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm2
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmm5

        sub     rax, byte SIZEOF_ZMMWORD
        add     rsi, 1*SIZEOF_ZMMWORD      ; inptr
        add     rdi, 2*SIZEOF_ZMMWORD      ; outptr
        cmp     rax, byte SIZEOF_ZMMWORD
        ja      near .columnloop
        test    eax,eax
        jnz     near .columnloop_last

        pop     rsi
        pop     rdi
        pop     rax

        add     rsi, byte SIZEOF_JSAMPROW       ; input_data
        add     rdi, byte SIZEOF_JSAMPROW       ; output_data
        dec     rcx                             ; rowctr
        jg      near .rowloop

.return:
        uncollect_args 4
        pop     rbp
        ret

; --------------------------------------------------------------------------
;
; Fancy processing for the common case of 2:1 horizontal and 2:1 vertical.
; Again a triangle filter; see comments for h2v1 case, above.
;
; GLOBAL(void)
; jsimd_h2v2_fancy_upsample_avx512 (int max_v_samp_factor,
;                                 JDIMENSION downsampled_width,
;                                 JSAMPARRAY input_data,
;                                 JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION downsampled_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_ZMMWORD ; zmmword wk[WK_NUM]
%define WK_NUM          4

        align   64
        global  EXTN(jsimd_h2v2_fancy_upsample_avx512)

EXTN(jsimd_h2v2_fancy_upsample_avx512):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_ZMMWORD)     ; align to 512 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        push_xmm    3
        collect_args 4
        push    rbx

        mov     rax, r11  ; colctr
        test    rax,rax
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      near .return

        mov     rsi, r12        ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data
.rowloop:
        push    rax                                     ; colctr
        push    rcx
        push    rdi
        push    rsi

        mov     rcx, JSAMPROW [rsi-1*SIZEOF_JSAMPROW]   ; inptr1(above)
        mov     rbx, JSAMPROW [rsi+0*SIZEOF_JSAMPROW]   ; inptr0
        mov     rsi, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]   ; inptr1(below)
        mov     rdx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]   ; outptr0
        mov     rdi, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]   ; outptr1
        vpcmpeqb ymm12, ymm12, ymm12
        pslldq xmm12, 2
        vshufi32x4 zmm12, zmm12, zmm12, 0b01010100
        vpxord   zmm13,zmm13,zmm13
        vpxord   zmm14,zmm14,zmm14
        vpcmpeqb xmm14,xmm14,xmm14
        vpsrldq  xmm15,xmm14,(SIZEOF_XMMWORD-2); (ffff ---- ---- ... ---- ----) LSB is ffff
        vpslldq  xmm14,xmm14,(SIZEOF_XMMWORD-2)
        vshufi32x4 zmm14, zmm14, zmm14, 0b00010101 ; (---- ---- ... ---- ---- ffff) MSB is ffff

        test    rax, SIZEOF_ZMMWORD-1
        jz      short .skip
        push    rdx
        mov     dl, JSAMPLE [rcx+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rcx+rax*SIZEOF_JSAMPLE], dl
        mov     dl, JSAMPLE [rbx+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rbx+rax*SIZEOF_JSAMPLE], dl
        mov     dl, JSAMPLE [rsi+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rsi+rax*SIZEOF_JSAMPLE], dl    ; insert a dummy sample
        pop     rdx
.skip:
        ; -- process the first column block

        vmovdqu64  zmm0, ZMMWORD [rbx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, ZMMWORD [rcx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm2, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]

        vpunpckhbw zmm4,zmm0,zmm13
        vpunpcklbw zmm8,zmm0,zmm13
        vshufi32x4 zmm9, zmm4, zmm4, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm0, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm4, zmm10, zmm10, 0b01110010

        vpunpckhbw zmm5,zmm1,zmm13
        vpunpcklbw zmm8,zmm1,zmm13
        vshufi32x4 zmm9, zmm5, zmm5, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm1, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm5, zmm10, zmm10, 0b01110010

        vpunpckhbw zmm6,zmm2,zmm13
        vpunpcklbw zmm8,zmm2,zmm13
        vshufi32x4 zmm9, zmm6, zmm6, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm2, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm6, zmm10, zmm10, 0b01110010

        vpmullw  zmm0,zmm0,[rel PW_THREE]
        vpmullw  zmm4,zmm4,[rel PW_THREE]

        vpaddw   zmm1,zmm1,zmm0
        vpaddw   zmm5,zmm5,zmm4
        vpaddw   zmm2,zmm2,zmm0
        vpaddw   zmm6,zmm6,zmm4

        vmovdqu64  ZMMWORD [rdx+0*SIZEOF_ZMMWORD], zmm1
        vmovdqu64  ZMMWORD [rdx+1*SIZEOF_ZMMWORD], zmm5
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm2
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmm6

        vpandd    zmm1,zmm1,zmm15
        vpandd    zmm2,zmm2,zmm15

        vmovdqa64  ZMMWORD [wk(0)], zmm1
        vmovdqa64  ZMMWORD [wk(1)], zmm2

        add     rax, byte SIZEOF_ZMMWORD-1
        and     rax, byte -SIZEOF_ZMMWORD
        cmp     rax, byte SIZEOF_ZMMWORD
        ja      short .columnloop

.columnloop_last:
        ; -- process the last column block
        vpandd    zmm1,zmm14, ZMMWORD [rdx+1*SIZEOF_ZMMWORD]
        vpandd    zmm2,zmm14, ZMMWORD [rdi+1*SIZEOF_ZMMWORD]

        vmovdqa64  ZMMWORD [wk(2)], zmm1
        vmovdqa64  ZMMWORD [wk(3)], zmm2

        jmp     near .upsample

.columnloop:
        ; -- process the next column block

        jmp     .upsample
        vmovdqu64  zmm0, ZMMWORD [rbx+1*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, ZMMWORD [rcx+1*SIZEOF_ZMMWORD]
        vmovdqu64  zmm2, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]

        vpunpckhbw zmm4,zmm0,zmm13
        vpunpcklbw zmm8,zmm0,zmm13
        vshufi32x4 zmm9, zmm4, zmm4, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm0, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm4, zmm10, zmm10, 0b01110010

        vpunpckhbw zmm5,zmm1,zmm13
        vpunpcklbw zmm8,zmm1,zmm13
        vshufi32x4 zmm9, zmm5, zmm5, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm1, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm5, zmm10, zmm10, 0b01110010

        vpunpckhbw zmm6,zmm2,zmm13
        vpunpcklbw zmm8,zmm2,zmm13
        vshufi32x4 zmm9, zmm6, zmm6, 0b01001110
        vshufi32x4 zmm10, zmm8, zmm9, 0b11100100
        vshufi32x4 zmm2, zmm10, zmm10, 0b11011000
        vshufi32x4 zmm10, zmm9, zmm8, 0b11100100
        vshufi32x4 zmm6, zmm10, zmm10, 0b01110010
        vpmullw  zmm0,zmm0,[rel PW_THREE]
        vpmullw  zmm4,zmm4,[rel PW_THREE]

        vpaddw   zmm1,zmm1,zmm0
        vpaddw   zmm5,zmm5,zmm4
        vpaddw   zmm2,zmm2,zmm0
        vpaddw   zmm6,zmm6,zmm4

        vmovdqu64  ZMMWORD [rdx+2*SIZEOF_ZMMWORD], zmm1
        vmovdqu64  ZMMWORD [rdx+3*SIZEOF_ZMMWORD], zmm5
        vmovdqu64  ZMMWORD [rdi+2*SIZEOF_ZMMWORD], zmm2
        vmovdqu64  ZMMWORD [rdi+3*SIZEOF_ZMMWORD], zmm6

        vshufi32x4 zmm1, zmm13, zmm1, 0b00000000
        vshufi32x4 zmm1, zmm1, zmm1, 0b11000000
        vpslldq  zmm1,zmm1,14
        vshufi32x4 zmm1, zmm13, zmm2, 0b00000000
        vshufi32x4 zmm2, zmm2, zmm2, 0b11000000
        vpslldq  zmm2,zmm2,14

        vmovdqa64  ZMMWORD [wk(2)], zmm1
        vmovdqa64  ZMMWORD [wk(3)], zmm2

.upsample:
        ; -- process the upper row

        vmovdqu64 zmm7, ZMMWORD [rdx+0*SIZEOF_ZMMWORD]
        vmovdqu64 zmm3, ZMMWORD [rdx+1*SIZEOF_ZMMWORD]

        vmovdqa64 zmm0,zmm7 ; zmm7=Int0L=( 0  1  2  3  4  5  6  7 .. 30 31)
        vmovdqa64 zmm4,zmm3 ; zmm3=Int0H=( 32 33 34 35 .. 62 63)
        vmovdqa64 zmm9,zmm0
        pxor      xmm9, xmm9
        vshufi32x4 zmm8, zmm9, zmm9, 0b00111001
        vpalignr  zmm0,zmm8,zmm0,2 ; zmm0=( 1  2  3  4  5  6  7 ..   29 30 --)

        vshufi32x4 zmm8, zmm13, zmm4, 0b00000000
        vshufi32x4 zmm8, zmm8, zmm8, 0b11000000
        vpslldq    zmm4,zmm8,14 ; zmm4=(-- -- -- -- -- -- --  32)

        vmovdqa64  zmm5,zmm7
        vmovdqa64  zmm6,zmm3

        vshufi32x4 zmm5, zmm5, zmm13, 0b00000011
        vshufi32x4 zmm5, zmm5, zmm5, 0b11111100
        vpsrldq    zmm5,zmm5,14  ; zmm5=( 31 -- -- -- -- -- -- --)

        vmovdqa64  zmm8,zmm6
        vshufi32x4 zmm8, zmm8, zmm8, 0b10010011
        pxor       xmm8, xmm8
        vpalignr zmm6,zmm6,zmm8,14 ; zmm6=(--  32 33 34 .. 62)

        vpord    zmm0,zmm0,zmm4 ; zmm0=( 1  2  3  4  5  6  7  8 .. 31 32)
        vpord    zmm5,zmm5,zmm6 ; zmm5=( 31 32 33 ... 61 62)

        vmovdqa64  zmm1,zmm7
        vmovdqa64  zmm2,zmm3

        vmovdqa64  zmm8,zmm1
        vshufi32x4 zmm8, zmm8, zmm8, 0b10010011
        pxor       xmm8, xmm8
        vpalignr zmm1,zmm1,zmm8,14 ; zmm1=(--  0 1 2 3 .. 29 30)

        vmovdqa64 zmm8, zmm2
        pxor       xmm8, xmm8
        vshufi32x4 zmm8, zmm8, zmm8, 0b00111001
        vpalignr zmm2,zmm8,zmm2,2 ; zmm2=( 33 34 .. 62 63 --)

        vmovdqa64  zmm4,zmm3
        vshufi32x4 zmm4, zmm4, zmm13, 0b00000011
        vshufi32x4 zmm4, zmm4, zmm4, 0b11111100
        vpsrldq    zmm4,zmm4,14  ; zmm4=(63 -- -- -- -- -- -- --)

        vpord     zmm1,zmm1, ZMMWORD [wk(0)]  ; zmm1=(-1  0  1  2  3  ..  29 30)
        vpord     zmm2,zmm2, ZMMWORD [wk(2)]  ; zmm2=(33 34 .. 63 64)

        vmovdqa64 ZMMWORD [wk(0)], zmm4

        vpmullw  zmm7,zmm7,[rel PW_THREE]
        vpmullw  zmm3,zmm3,[rel PW_THREE]
        vpaddw   zmm1,zmm1,[rel PW_EIGHT]
        vpaddw   zmm5,zmm5,[rel PW_EIGHT]
        vpaddw   zmm0,zmm0,[rel PW_SEVEN]
        vpaddw   zmm2,zmm2,[rel PW_SEVEN]

        vpaddw   zmm1,zmm1,zmm7
        vpaddw   zmm5,zmm5,zmm3

        vpsrlw   zmm1,zmm1,4
        vpsrlw   zmm5,zmm5,4
        vpaddw   zmm0,zmm0,zmm7
        vpaddw   zmm2,zmm2,zmm3
        vpsrlw   zmm0,zmm0,4
        vpsrlw   zmm2,zmm2,4

        vpsllw   zmm0,zmm0,BYTE_BIT
        vpsllw   zmm2,zmm2,BYTE_BIT
        vpord    zmm1,zmm1,zmm0
        vpord    zmm5,zmm5,zmm2

        vmovdqu64 ZMMWORD [rdx+0*SIZEOF_ZMMWORD], zmm1
        vmovdqu64 ZMMWORD [rdx+1*SIZEOF_ZMMWORD], zmm5

        ; -- process the lower row
        vmovdqu64 zmm6, ZMMWORD [rdi+0*SIZEOF_ZMMWORD]
        vmovdqu64 zmm4, ZMMWORD [rdi+1*SIZEOF_ZMMWORD]

        vmovdqa64 zmm7,zmm6
        vmovdqa64 zmm3,zmm4
        vmovdqa64 zmm9,zmm6
        pxor      xmm9, xmm9
        vshufi32x4 zmm8, zmm9, zmm9, 0b00111001
        vpalignr zmm7,zmm8,zmm7,2 ;zmm7=( 1  2  3  4  5  6  .. 30 31 --)


        vshufi32x4 zmm8, zmm13, zmm3, 0b00000000
        vshufi32x4 zmm8, zmm8, zmm8, 0b11000000
        vpslldq    zmm3,zmm8,14 ;  zmm3=(-- -- -- -- -- -- --  32)

        vmovdqa64  zmm0,zmm6
        vmovdqa64  zmm2,zmm4

        vshufi32x4 zmm0, zmm0, zmm13, 0b00000011
        vshufi32x4 zmm0, zmm0, zmm0, 0b11111100
        vpsrldq    zmm0,zmm0,14 ; zmm0=( 31 -- -- -- -- -- -- --)


        vmovdqa64  zmm8,zmm2
        vshufi32x4 zmm8, zmm8, zmm8, 0b10010011
        pxor       xmm8, xmm8
        vpalignr   zmm2,zmm2,zmm8,14 ; zmm2=(--  31 32  .. 61 62)

        vpord     zmm7,zmm7,zmm3  ; zmm7=( 1  2  3  4  .. 31 32)
        vpord     zmm0,zmm0,zmm2  ; zmm0=( 31 32 .. 61 62)

        vmovdqa64  zmm1,zmm6
        vmovdqa64  zmm5,zmm4

        vmovdqa64  zmm8,zmm1
        vshufi32x4 zmm8, zmm8, zmm8, 0b10010011
        pxor       xmm8, xmm8
        vpalignr   zmm1, zmm1,zmm8,14 ; zmm1=(--  0  1  2  3  4  5  6 .. 29 30)

        vmovdqa64 zmm8, zmm5
        pxor       xmm8, xmm8
        vshufi32x4 zmm8, zmm8, zmm8, 0b00111001
        vpalignr zmm5,zmm8,zmm5,2 ; zmm5=( 33 34 .. 62 63 --)

        vmovdqa64  zmm3,zmm4
        vshufi32x4 zmm3, zmm3, zmm13, 0b00000011
        vshufi32x4 zmm3, zmm3, zmm3, 0b11111100
        vpsrldq    zmm3,zmm3,14 ; zmm3=(63 -- -- -- -- -- -- --)


        vpord     zmm1,zmm1, ZMMWORD [wk(1)]
        vpord     zmm5,zmm5, ZMMWORD [wk(3)]

        vmovdqa64 ZMMWORD [wk(1)], zmm3

        vpmullw  zmm6,zmm6,[rel PW_THREE]
        vpmullw  zmm4,zmm4,[rel PW_THREE]
        vpaddw   zmm1,zmm1,[rel PW_EIGHT]
        vpaddw   zmm0,zmm0,[rel PW_EIGHT]
        vpaddw   zmm7,zmm7,[rel PW_SEVEN]
        vpaddw   zmm5,zmm5,[rel PW_SEVEN]

        vpaddw   zmm1,zmm1,zmm6
        vpaddw   zmm0,zmm0,zmm4
        vpsrlw   zmm1,zmm1,4
        vpsrlw   zmm0,zmm0,4
        vpaddw   zmm7,zmm7,zmm6
        vpaddw   zmm5,zmm5,zmm4
        vpsrlw   zmm7,zmm7,4
        vpsrlw   zmm5,zmm5,4

        vpsllw   zmm7,zmm7,BYTE_BIT
        vpsllw   zmm5,zmm5,BYTE_BIT
        vpord    zmm1,zmm1,zmm7
        vpord    zmm0,zmm0,zmm5

        vmovdqu64 ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm1
        vmovdqu64 ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmm0

        sub     rax, byte SIZEOF_ZMMWORD
        add     rcx, byte 1*SIZEOF_ZMMWORD
        add     rbx, byte 1*SIZEOF_ZMMWORD
        add     rsi, byte 1*SIZEOF_ZMMWORD
        add     rdx, 2*SIZEOF_ZMMWORD
        add     rdi, 2*SIZEOF_ZMMWORD
        cmp     rax, byte SIZEOF_ZMMWORD
        ja      near .columnloop
        test    rax,rax
        jnz     near .columnloop_last

        pop     rsi
        pop     rdi
        pop     rcx
        pop     rax

        add     rsi, byte 1*SIZEOF_JSAMPROW     ; input_data
        add     rdi, byte 2*SIZEOF_JSAMPROW     ; output_data
        sub     rcx, byte 2                     ; rowctr
        jg      near .rowloop

.return:
        pop     rbx
        vzeroupper
        uncollect_args 4
        pop_xmm     3
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret


        ; --------------------------------------------------------------------------
;
; Fast processing for the common case of 2:1 horizontal and 1:1 vertical.
; It's still a box filter.
;
; GLOBAL(void)
; jsimd_h2v1_upsample_avx512 (int max_v_samp_factor,
;                           JDIMENSION output_width,
;                           JSAMPARRAY input_data,
;                           JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION output_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

        align   64
        global  EXTN(jsimd_h2v1_upsample_avx512)

EXTN(jsimd_h2v1_upsample_avx512):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args 4

        mov     rdx, r11
        add     rdx, byte (SIZEOF_YMMWORD-1)
        and     rdx, -(SIZEOF_YMMWORD)
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      .return

        mov     rsi, r12 ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data
.rowloop:
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]             ; inptr
        mov     rdi, JSAMPROW [rdi]             ; outptr

        mov     rax,rdx                         ; colctr

.columnloop:

        cmp   rax, byte SIZEOF_ZMMWORD
        ja    near .above_32

        cmp   rax, byte SIZEOF_YMMWORD
        ja    near .above_16

        vmovdqu  xmm0, XMMWORD [rsi+0*SIZEOF_YMMWORD]
        vpunpckhbw xmm1,xmm0,xmm0
        vpunpcklbw xmm0,xmm0,xmm0
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmm0
        vmovdqu  XMMWORD [rdi+1*SIZEOF_XMMWORD], xmm1
        jmp   short .nextrow

.above_16:
        vmovdqu  ymm0, YMMWORD [rsi+0*SIZEOF_YMMWORD]

        vpermq     ymm0,ymm0,0xd8
        vpunpckhbw ymm1,ymm0,ymm0
        vpunpcklbw ymm0,ymm0,ymm0

        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm0
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm1
        jmp      short .nextrow

.above_32:
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vshufi32x4 zmm0, zmm0, zmm0, 0b11011000
        vpermq     zmm0, zmm0, 0xd8
        vpunpckhbw zmm1, zmm0, zmm0
        vpunpcklbw zmm0, zmm0, zmm0
        
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm0
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmm1

        sub     rax, 2*SIZEOF_ZMMWORD
        jz      short .nextrow

        add     rsi, byte SIZEOF_ZMMWORD      ; inptr
        add     rdi, 2*SIZEOF_ZMMWORD      ; outptr
        jmp     .columnloop

.nextrow:
        pop     rsi
        pop     rdi

        add     rsi, byte SIZEOF_JSAMPROW       ; input_data
        add     rdi, byte SIZEOF_JSAMPROW       ; output_data
        dec     rcx                             ; rowctr
        jg      .rowloop

.return:
        vzeroupper
        uncollect_args 4
        pop     rbp
        ret

; --------------------------------------------------------------------------
;
; Fast processing for the common case of 2:1 horizontal and 2:1 vertical.
; It's still a box filter.
;
; GLOBAL(void)
; jsimd_h2v2_upsample_avx512 (nt max_v_samp_factor,
;                           JDIMENSION output_width,
;                           JSAMPARRAY input_data,
;                           JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION output_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

        align   64
        global  EXTN(jsimd_h2v2_upsample_avx512)

EXTN(jsimd_h2v2_upsample_avx512):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args 4
        push    rbx

        mov     rdx, r11
        add     rdx, byte (SIZEOF_YMMWORD-1)
        and     rdx, -SIZEOF_YMMWORD
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      near .return

        mov     rsi, r12        ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data
.rowloop:
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]                     ; inptr
        mov     rbx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]   ; outptr0
        mov     rdi, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]   ; outptr1
        mov     rax,rdx                                 ; colctr

.columnloop:

        cmp   rax, byte SIZEOF_ZMMWORD
        ja    near .above_32

        cmp rax, byte SIZEOF_YMMWORD
        ja    short .above_16

        vmovdqu  xmm0, XMMWORD [rsi+0*SIZEOF_XMMWORD]
        vpunpckhbw xmm1,xmm0,xmm0
        vpunpcklbw xmm0,xmm0,xmm0
        vmovdqu  XMMWORD [rbx+0*SIZEOF_XMMWORD], xmm0
        vmovdqu  XMMWORD [rbx+1*SIZEOF_XMMWORD], xmm1
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmm0
        vmovdqu  XMMWORD [rdi+1*SIZEOF_XMMWORD], xmm1

        jmp   near .nextrow

.above_16:
        vmovdqu  ymm0, YMMWORD [rsi+0*SIZEOF_YMMWORD]
    
        vpermq     ymm0,ymm0,0xd8
        vpunpckhbw ymm1,ymm0,ymm0 
        vpunpcklbw ymm0,ymm0,ymm0 

        vmovdqu  YMMWORD [rbx+0*SIZEOF_YMMWORD], ymm0
        vmovdqu  YMMWORD [rbx+1*SIZEOF_YMMWORD], ymm1
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm0
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm1

        jmp      short .nextrow

.above_32:
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vshufi32x4 zmm0, zmm0, zmm0, 0b11011000
        vpermq     zmm0, zmm0, 0xd8
        vpunpckhbw zmm1, zmm0, zmm0
        vpunpcklbw zmm0, zmm0, zmm0

        vmovdqu64  ZMMWORD [rbx+0*SIZEOF_ZMMWORD], zmm0
        vmovdqu64  ZMMWORD [rbx+1*SIZEOF_ZMMWORD], zmm1
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm0
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmm1

        add     rsi, byte SIZEOF_ZMMWORD      ; inptr
        add     rbx, 2*SIZEOF_ZMMWORD      ; outptr0
        add     rdi, 2*SIZEOF_ZMMWORD      ; outptr1
        jmp     .columnloop

.nextrow:
        pop     rsi
        pop     rdi

        add     rsi, byte 1*SIZEOF_JSAMPROW     ; input_data
        add     rdi, byte 2*SIZEOF_JSAMPROW     ; output_data
        sub     rcx, byte 2                     ; rowctr
        jg      near .rowloop

.return:
        pop     rbx
        vzeroupper
        uncollect_args 4
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   64
