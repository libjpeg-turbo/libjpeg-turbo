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

    xor         edi, edi                ; simd support flag

    ; Check for AVX2 instruction support
    lea         eax, [rdi+7]
    xor         ecx, ecx
    cpuid

    or          edi, JSIMD_SSE2|JSIMD_SSE
    test        bl, 1<<5               ; bit5:AVX2
    jz          short .return

    ; Check for AVX2 O/S support
    xor         ecx, ecx
    lea         eax, [rcx+1]
    cpuid
    mov		eax, ecx
    shr		eax, 27			; bring XSAVE bit to lsb
    test        al, 1
    jz          short .return           ; O/S does not support XSAVE
    test        al, 2
    jz          short .return           ; CPU does not support AVX2

    xor         ecx, ecx
    xgetbv
    test        al, 6			;O/S does not manage XMM/YMM state
    jz          short .return		; using XSAVE

    bts         edi, 7 			;JSIMD_AVX2=0x80

.return:
    mov         eax, edi

    pop         rdi
    pop         rbx
    ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
