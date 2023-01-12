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

