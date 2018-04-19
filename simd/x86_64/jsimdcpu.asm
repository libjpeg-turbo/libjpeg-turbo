;
; jsimdcpu.asm - SIMD instruction support check
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2016, D. R. Commander.
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

; --------------------------------------------------------------------------
    SECTION     SEG_TEXT
    BITS        64
;
; Check if the CPU supports SIMD instructions
;
; GLOBAL(unsigned int)
; jpeg_simd_cpu_support(void)
;

    align       32
    GLOBAL_FUNCTION(jpeg_simd_cpu_support)

EXTN(jpeg_simd_cpu_support):
    push        rbx
    push        rdi

    xor         rdi, rdi                ; simd support flag

    ; Check for AVX2 instruction support
    mov         rax, 7
    xor         rcx, rcx
    cpuid
    mov         rax, rbx                ; rax = Extended feature flags

    or          rdi, JSIMD_SSE2
    or          rdi, JSIMD_SSE
    test        rax, 1<<5               ; bit5:AVX2
    jz          short .return

    ; Check for AVX2 O/S support
    mov         rax, 1
    xor         rcx, rcx
    cpuid
    test        rcx, 1<<27
    jz          short .return           ; O/S does not support XSAVE
    test        rcx, 1<<28
    jz          short .return           ; CPU does not support AVX2

    xor         rcx, rcx
    xgetbv
    test        rax, 6                  ; O/S does not manage XMM/YMM state
                                        ; using XSAVE
    jz          short .return

    or          rdi, JSIMD_AVX2

    mov         rax, 1
    mov         rcx, 0
    cpuid
    and         rcx, 0x8000000
    test        rcx, 0x8000000
    jz          short .return           ; CPU does not support AVX-512
    mov         rcx, 0
    xgetbv
    and         rax, 0xe6
    test        rax, 0xe6

    jz          short .return
    or          rdi,  JSIMD_AVX512

.return:
    mov         rax, rdi

    pop         rdi
    pop         rbx
    ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
