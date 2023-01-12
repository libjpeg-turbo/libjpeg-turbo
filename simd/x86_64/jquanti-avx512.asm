;
; jquanti.asm - sample data conversion and quantization (64-bit AVX512)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2009, 2016, 2018, D. R. Commander.
; Copyright (C) 2016, Matthieu Darbois.
; Copyright (C) 2018, Matthias Räncker.
; Copyright (C) 2023, Stephan Vedder.
; Copyright (C) 2023, Robert Clausecker.
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

%include "jsimdext.inc"
%include "jdct.inc"

; --------------------------------------------------------------------------
    SECTION     SEG_TEXT
    BITS        64
;
; Load data into workspace, applying unsigned->signed conversion
;
; GLOBAL(void)
; jsimd_convsamp_avx512(JSAMPARRAY sample_data, JDIMENSION start_col,
;                       DCTELEM *workspace);
;

; r10 = JSAMPARRAY sample_data
; r11d = JDIMENSION start_col
; r12 = DCTELEM *workspace

    align       32
    GLOBAL_FUNCTION(jsimd_convsamp_avx512)

EXTN(jsimd_convsamp_avx512):
    push        rbp
    mov         rax, rsp
    mov         rbp, rsp
    collect_args 3

    vmovdqu64    ymm18, [r10]
    vmovdqu64    ymm19, [r10+32]
    kxnorb       k1, k1, k1
    kxnorb       k2, k2, k2
    mov          r11d, r11d
    vpgatherqq   ymm16{k1}, [r11 + ymm18*1]
    vpgatherqq   ymm17{k2}, [r11 + ymm19*1]

    vpmovzxbw    zmm16, ymm16
    vpmovzxbw    zmm17, ymm17

    vpternlogd   zmm18, zmm18, zmm18, 0xff
    vpsllw       zmm18, zmm18, 7            ; 0xff80
    vpaddw       zmm16, zmm16, zmm18
    vpaddw       zmm17, zmm17, zmm18

    vmovdqu16    [ZMMBLOCK(0,0,r12,SIZEOF_DCTELEM)], zmm16
    vmovdqu16    [ZMMBLOCK(4,0,r12,SIZEOF_DCTELEM)], zmm17

    uncollect_args 3
    pop         rbp
    ret

; --------------------------------------------------------------------------
;
; Quantize/descale the coefficients, and store into coef_block
;
; This implementation is based on an algorithm described in
;   "How to optimize for the Pentium family of microprocessors"
;   (http://www.agner.org/assem/).
;
; GLOBAL(void)
; jsimd_quantize_avx512(JCOEFPTR coef_block, DCTELEM *divisors,
;                       DCTELEM *workspace);
;

%define RECIPROCAL(m, n, b) \
  ZMMBLOCK(DCTSIZE * 0 + (m), (n), (b), SIZEOF_DCTELEM)
%define CORRECTION(m, n, b) \
  ZMMBLOCK(DCTSIZE * 1 + (m), (n), (b), SIZEOF_DCTELEM)
%define SCALE(m, n, b) \
  ZMMBLOCK(DCTSIZE * 2 + (m), (n), (b), SIZEOF_DCTELEM)

; r10 = JCOEFPTR coef_block
; r11 = DCTELEM *divisors
; r12 = DCTELEM *workspace

    align       32
    GLOBAL_FUNCTION(jsimd_quantize_avx512)

EXTN(jsimd_quantize_avx512):
    push        rbp
    mov         rax, rsp
    mov         rbp, rsp
    collect_args 3

    vmovdqu32   zmm18, [ZMMBLOCK(0,0,r12,SIZEOF_DCTELEM)]
    vmovdqu32   zmm19, [ZMMBLOCK(4,0,r12,SIZEOF_DCTELEM)]
    vpabsw      zmm16, zmm18
    vpabsw      zmm17, zmm19

    vpmovw2m      k1, zmm18
    vpmovw2m      k2, zmm19

    vpaddw      zmm16, ZMMWORD [CORRECTION(0,0,r11)]  ; correction + roundfactor
    vpaddw      zmm17, ZMMWORD [CORRECTION(4,0,r11)]
    vpmulhuw    zmm16, ZMMWORD [RECIPROCAL(0,0,r11)]  ; reciprocal
    vpmulhuw    zmm17, ZMMWORD [RECIPROCAL(4,0,r11)]
    vpmulhuw    zmm16, ZMMWORD [SCALE(0,0,r11)]       ; scale
    vpmulhuw    zmm17, ZMMWORD [SCALE(4,0,r11)]

    vpxorq        zmm20, zmm20, zmm20
    vpsubw        zmm16{k1}, zmm20, zmm16
    vpsubw        zmm17{k2}, zmm20, zmm17

    vmovdqu32     [ZMMBLOCK(0,0,r10,SIZEOF_DCTELEM)], zmm16
    vmovdqu32     [ZMMBLOCK(4,0,r10,SIZEOF_DCTELEM)], zmm17

    uncollect_args 3
    pop         rbp
    ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
