;
; jcsample.asm - downsampling (64-bit AVX512) ;
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2009, 2016, D. R. Commander.
; Copyright (C) 2015, 2018, Intel Corporation.
;
; Based on the x86 SIMD extension for IJG JPEG library
; Copyright (C) 1999-2006, MIYASAKA Masaru.
; For conditions of distribution and use, see copyright notice in jsimdext.inc
;
; This file should be aavxmbled with NASM (Netwide Assembler),
; can *not* be assembled with Microsoft's MASM or any compatible
; assembler (including Borland's Turbo Assembler).
; NASM is available from http://nasm.sourceforge.net/ or
; http://sourceforge.net/project/showfiles.php?group_id=6208
;
; [TAB8]

%include "jsimdext.inc"

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Downsample pixel values of a single component.
; This version handles the common case of 2:1 horizontal and 1:1 vertical,
; without smoothing.
;
; GLOBAL(void)
; jsimd_h2v1_downsample_avx512 (JDIMENSION image_width, int max_v_samp_factor,
;                             JDIMENSION v_samp_factor, JDIMENSION width_blocks,
;                             JSAMPARRAY input_data, JSAMPARRAY output_data);
;

; r10 = JDIMENSION image_width
; r11 = int max_v_samp_factor
; r12 = JDIMENSION v_samp_factor
; r13 = JDIMENSION width_blocks
; r14 = JSAMPARRAY input_data
; r15 = JSAMPARRAY output_data

        align   64
        global  EXTN(jsimd_h2v1_downsample_avx512)

EXTN(jsimd_h2v1_downsample_avx512):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args 6

        mov rcx, r13
        shl     rcx,3                   ; imul rcx,DCTSIZE (rcx = output_cols)
        jz      near .return
        mov rdx, r10

        ; -- expand_right_edge

        push    rcx
        shl     rcx,1                           ; output_cols * 2
        sub     rcx,rdx
        jle     short .expand_end

        mov     rax, r11
        test    rax,rax
        jle     short .expand_end

        cld
        mov     rsi, r14        ; input_data
.expandloop:
        push    rax
        push    rcx

        mov     rdi, JSAMPROW [rsi]
        add     rdi,rdx
        mov     al, JSAMPLE [rdi-1]

        rep stosb

        pop     rcx
        pop     rax

        add     rsi, byte SIZEOF_JSAMPROW
        dec     rax
        jg      short .expandloop

.expand_end:
        pop     rcx                    

        ; -- h2v1_downsample

        mov     rax, r12        ; rowctr
        test    eax,eax
        jle     near .return

        mov     rdx, 0x00010000         ; bias pattern
        vmovd    xmm7,edx
        vpshufd  xmm7,xmm7,0x00         ; xmm7={0, 1, 0, 1, 0, 1, 0, 1}
        vperm2i128 ymm7,ymm7,ymm7,0     ; ymm7={xmm7,xmm7}
        vshufi32x4 zmm7, zmm7, zmm7, 0b01000100

        vpcmpeqw ymm6,ymm6,ymm6
        vpsrlw   ymm6,ymm6,BYTE_BIT     ; ymm6={0xFF 0x00 0xFF 0x00 ..}
        vshufi32x4 zmm6, zmm6, zmm6, 0x44

        mov     rsi, r14        ; input_data
        mov     rdi, r15        ; output_data
.rowloop:
        push    rcx
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]             ; inptr
        mov rdi, JSAMPROW [rdi]         ; outptr

        cmp     rcx, byte SIZEOF_ZMMWORD
        jae     .columnloop

        ;rcx can possibly be 8,16,24,32,40,48,56
.columnloop_r56:
        cmp rcx, 56 
        jne   .columnloop_r48
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu  ymm1, YMMWORD [rsi+1*SIZEOF_ZMMWORD]
        vmovdqu  xmm2, XMMWORD [rsi+3*SIZEOF_YMMWORD]
        vshufi32x4 zmm1, zmm1, zmm2, 0b01000100
        mov     rcx, SIZEOF_ZMMWORD
        jmp     .downsample
.columnloop_r48:
        cmp rcx, 48 
        jne   .columnloop_r40
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu  ymm1, YMMWORD [rsi+1*SIZEOF_ZMMWORD]
        mov     rcx, SIZEOF_ZMMWORD
        jmp     .downsample
.columnloop_r40:
        cmp rcx, 40 
        jne   .columnloop_r32
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu  xmm1, XMMWORD [rsi+1*SIZEOF_ZMMWORD]
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample
.columnloop_r32:
        cmp rcx, 32 
        jne   .columnloop_r24
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vpxord zmm1,zmm1,zmm1
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample
.columnloop_r24:
        cmp rcx,  24
        jne   .columnloop_r16
        vmovdqu  ymm0, YMMWORD [rsi+0*SIZEOF_YMMWORD]
        vmovdqu  xmm1, XMMWORD [rsi+1*SIZEOF_YMMWORD]
        vshufi32x4 zmm0, zmm0, zmm1, 0b01000100
        vpxord zmm1,zmm1,zmm1
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample
.columnloop_r16:
        cmp rcx,  16
        jne   .columnloop_r8
        vmovdqu  ymm0, YMMWORD [rsi+0*SIZEOF_YMMWORD]
        vpxord zmm1,zmm1,zmm1
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample
.columnloop_r8:
        vmovdqu xmm0,XMMWORD[rsi+0*SIZEOF_YMMWORD]
        vpxord zmm1,zmm1,zmm1
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample

.columnloop:
        vmovdqu64  zmm0, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]

.downsample:
        vpsrlw   zmm2,zmm0,BYTE_BIT
        vpandd   zmm0,zmm0,zmm6
        vpsrlw   zmm3,zmm1,BYTE_BIT
        vpandd   zmm1,zmm1,zmm6

        vpaddw   zmm0,zmm0,zmm2
        vpaddw   zmm1,zmm1,zmm3
        vpaddw   zmm0,zmm0,zmm7
        vpaddw   zmm1,zmm1,zmm7
        vpsrlw   zmm0,zmm0,1
        vpsrlw   zmm1,zmm1,1

        vpackuswb zmm0,zmm0,zmm1
        vpermq zmm0,zmm0,0b11011000
        vshufi32x4 zmm0, zmm0, zmm0, 0b11011000

        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm0

        sub     rcx, byte SIZEOF_ZMMWORD        ; outcol
        add     rsi, 2*SIZEOF_ZMMWORD      ; inptr
        add     rdi, byte 1*SIZEOF_ZMMWORD      ; outptr
        cmp     rcx, byte SIZEOF_ZMMWORD
        jae     short .columnloop
        test    rcx,rcx
        jnz     .columnloop_r56

        pop     rsi
        pop     rdi
        pop     rcx

        add     rsi, byte SIZEOF_JSAMPROW       ; input_data
        add     rdi, byte SIZEOF_JSAMPROW       ; output_data
        dec     rax                             ; rowctr
        jg      near .rowloop

.return:
        uncollect_args 6
        pop     rbp
        ret

; --------------------------------------------------------------------------
;
; Downsample pixel values of a single component.
; This version handles the standard case of 2:1 horizontal and 2:1 vertical,
; without smoothing.
;
; GLOBAL(void)
; jsimd_h2v2_downsample_avx512 (JDIMENSION image_width, int max_v_samp_factor,
;                             JDIMENSION v_samp_factor, JDIMENSION width_blocks,
;                             JSAMPARRAY input_data, JSAMPARRAY output_data);
;

; r10 = JDIMENSION image_width
; r11 = int max_v_samp_factor
; r12 = JDIMENSION v_samp_factor
; r13 = JDIMENSION width_blocks
; r14 = JSAMPARRAY input_data
; r15 = JSAMPARRAY output_data

        align   64
        global  EXTN(jsimd_h2v2_downsample_avx512)

EXTN(jsimd_h2v2_downsample_avx512):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args 6

        mov     rcx, r13
        shl     rcx,3                   ; imul rcx,DCTSIZE (rcx = output_cols)
        jz      near .return

        mov     rdx, r10

        ; -- expand_right_edge

        push    rcx
        shl     rcx,1                           ; output_cols * 2
        sub     rcx,rdx
        jle     short .expand_end

        mov     rax, r11
        test    rax,rax
        jle     short .expand_end

        cld
        mov     rsi, r14        ; input_data
.expandloop:
        push    rax
        push    rcx

        mov     rdi, JSAMPROW [rsi]
        add     rdi,rdx
        mov     al, JSAMPLE [rdi-1]

        rep stosb

        pop     rcx
        pop     rax

        add     rsi, byte SIZEOF_JSAMPROW
        dec     rax
        jg      short .expandloop

.expand_end:
        pop     rcx                             ; output_cols = width*8

        ; -- h2v2_downsample

        mov     rax, r12        ; rowctr
        test    rax,rax
        jle     near .return

        mov     rdx, 0x00020001         ; bias pattern
        vmovd    xmm7,edx
        vpcmpeqw ymm6,ymm6,ymm6
        vpshufd  xmm7,xmm7,0x00          ; ymm7={1, 2, 1, 2, 1, 2, 1, 2}
        vperm2i128 ymm7,ymm7,ymm7,0
        vshufi32x4 zmm7, zmm7, zmm7, 0b01000100
        vpsrlw   ymm6,ymm6,BYTE_BIT           ; ymm6={0xFF 0x00 0xFF 0x00 ..}
        vshufi32x4 zmm6, zmm6, zmm6, 0x44

        mov     rsi, r14        ; input_data
        mov     rdi, r15        ; output_data
.rowloop:
        push    rcx
        push    rdi
        push    rsi

        mov     rdx, JSAMPROW [rsi+0*SIZEOF_JSAMPROW]   ; inptr0
        mov     rsi, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]   ; inptr1
        mov     rdi, JSAMPROW [rdi]                     ; outptr

        cmp     rcx, byte SIZEOF_ZMMWORD
        jae     .columnloop

.columnloop_r56:
        cmp rcx,56
        jne   .columnloop_r48
        vmovdqu64  zmm0, ZMMWORD [rdx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, YMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu  ymm2, YMMWORD [rdx+1*SIZEOF_ZMMWORD]
        vmovdqu  ymm3, YMMWORD [rsi+1*SIZEOF_ZMMWORD]
        vmovdqu  xmm4, XMMWORD [rdx+3*SIZEOF_YMMWORD]
        vshufi32x4 zmm2, zmm2, zmm4, 0b01000100
        vmovdqu  xmm4, XMMWORD [rsi+3*SIZEOF_YMMWORD]
        vshufi32x4 zmm3, zmm3, zmm4, 0b01000100
        mov     rcx, SIZEOF_ZMMWORD
        jmp     .downsample
.columnloop_r48:
        cmp rcx,48
        jne   .columnloop_r40
        vmovdqu64  zmm0, ZMMWORD [rdx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, YMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu  ymm2, YMMWORD [rdx+1*SIZEOF_ZMMWORD]
        vmovdqu  ymm3, YMMWORD [rsi+1*SIZEOF_ZMMWORD]
        mov     rcx, SIZEOF_ZMMWORD
        jmp     .downsample
.columnloop_r40:
        cmp rcx,40
        jne   .columnloop_r32
        vmovdqu64  zmm0, ZMMWORD [rdx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, YMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu  xmm2, XMMWORD [rdx+1*SIZEOF_ZMMWORD]
        vmovdqu  xmm3, XMMWORD [rsi+1*SIZEOF_ZMMWORD]
        mov     rcx, SIZEOF_ZMMWORD
        jmp     .downsample
.columnloop_r32:
        cmp rcx,32
        jne   .columnloop_r24
        vmovdqu64  zmm0, ZMMWORD [rdx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, YMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vpxord    zmm2,zmm2,zmm2
        vpxord    zmm3,zmm3,zmm3
        mov     rcx, SIZEOF_ZMMWORD
        jmp     .downsample
.columnloop_r24:
        cmp rcx,24
        jne   .columnloop_r16
        vmovdqu  ymm0, YMMWORD [rdx+0*SIZEOF_YMMWORD]
        vmovdqu  xmm4, XMMWORD [rdx+1*SIZEOF_YMMWORD]
        vshufi32x4 zmm0, zmm0, zmm4, 0b01000100
        vmovdqu  ymm1, YMMWORD [rsi+0*SIZEOF_YMMWORD]
        vmovdqu  xmm4, XMMWORD [rsi+1*SIZEOF_YMMWORD]
        vshufi32x4 zmm1, zmm1, zmm4, 0b01000100
        vpxord    zmm2,zmm2,zmm2
        vpxord    zmm3,zmm3,zmm3
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample
.columnloop_r16:
        cmp rcx,16
        jne   .columnloop_r8
        vmovdqu  ymm0, YMMWORD [rdx+0*SIZEOF_YMMWORD]
        vmovdqu  ymm1, YMMWORD [rsi+0*SIZEOF_YMMWORD]
        vpxord    zmm2,zmm2,zmm2
        vpxord    zmm3,zmm3,zmm3
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample
.columnloop_r8:
        vmovdqu  xmm0, XMMWORD [rdx+0*SIZEOF_XMMWORD]
        vmovdqu  xmm1, XMMWORD [rsi+0*SIZEOF_XMMWORD]
        vpxord    zmm2,zmm2,zmm2
        vpxord    zmm3,zmm3,zmm3
        mov     rcx, SIZEOF_ZMMWORD
        jmp     short .downsample

.columnloop:
        vmovdqu64  zmm0, ZMMWORD [rdx+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm1, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmm2, ZMMWORD [rdx+1*SIZEOF_ZMMWORD]
        vmovdqu64  zmm3, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]

.downsample:
        vpandd   zmm4,zmm0,zmm6
        vpsrlw   zmm0,zmm0,BYTE_BIT
        vpandd   zmm5,zmm1,zmm6
        vpsrlw   zmm1,zmm1,BYTE_BIT
        vpaddw   zmm0,zmm0,zmm4
        vpaddw   zmm1,zmm1,zmm5

        vpandd   zmm4,zmm2,zmm6
        vpsrlw   zmm2,zmm2,BYTE_BIT
        vpandd   zmm5,zmm3,zmm6
        vpsrlw   zmm3,zmm3,BYTE_BIT
        vpaddw   zmm2,zmm2,zmm4
        vpaddw   zmm3,zmm3,zmm5

        vpaddw   zmm0,zmm0,zmm1
        vpaddw   zmm2,zmm2,zmm3
        vpaddw   zmm0,zmm0,zmm7
        vpaddw   zmm2,zmm2,zmm7
        vpsrlw   zmm0,zmm0,2
        vpsrlw   zmm2,zmm2,2

        vpackuswb zmm0,zmm0,zmm2
        vpermq zmm0,zmm0,0b11011000
        vshufi32x4 zmm0, zmm0, zmm0, 0b11011000

        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmm0

        sub     rcx, byte SIZEOF_ZMMWORD        ; outcol
        add     rdx, 2*SIZEOF_ZMMWORD      ; inptr0
        add     rsi, 2*SIZEOF_ZMMWORD      ; inptr1
        add     rdi, byte 1*SIZEOF_ZMMWORD      ; outptr
        cmp     rcx, byte SIZEOF_ZMMWORD
        jae     near .columnloop
        test    rcx,rcx
        jnz     near .columnloop_r56

        pop     rsi
        pop     rdi
        pop     rcx

        add     rsi, byte 2*SIZEOF_JSAMPROW     ; input_data
        add     rdi, byte 1*SIZEOF_JSAMPROW     ; output_data
        dec     rax                             ; rowctr
        jg      near .rowloop

.return:
        uncollect_args 6
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   64
