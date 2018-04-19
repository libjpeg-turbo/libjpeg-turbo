;
; jcgryext.asm - grayscale colorspace conversion (64-bit AVX512)
;
; Copyright (C) 2011, 2016, D. R. Commander.
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

%include "jcolsamp.inc"

; --------------------------------------------------------------------------
;
; Convert some rows of samples to the output colorspace.
;
; GLOBAL(void)
; jsimd_rgb_gray_convert_avx512 (JDIMENSION img_width,
;                              JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
;                              JDIMENSION output_row, int num_rows);
;

; r10 = JDIMENSION img_width
; r11 = JSAMPARRAY input_buf
; r12 = JSAMPIMAGE output_buf
; r13 = JDIMENSION output_row
; r14 = int num_rows

%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_ZMMWORD ; zmmword wk[WK_NUM]
%define WK_NUM          2

        align   64

        global  EXTN(jsimd_rgb_gray_convert_avx512)

EXTN(jsimd_rgb_gray_convert_avx512):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_ZMMWORD)     ; align to 512 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        collect_args 5
        push    rbx

        mov     rcx, r10
        test    rcx,rcx
        jz      near .return

        push    rcx

        mov rsi, r12
        mov rcx, r13
        mov     rdi, JSAMPARRAY [rsi+0*SIZEOF_JSAMPARRAY]
        lea     rdi, [rdi+rcx*SIZEOF_JSAMPROW]

        pop     rcx

        mov rsi, r11
        mov     eax, r14d
        test    rax,rax
        jle     near .return
.rowloop:
        push    rdi
        push    rsi
        push    rcx                     ; col

        mov     rsi, JSAMPROW [rsi]     ; inptr
        mov     rdi, JSAMPROW [rdi]     ; outptr0

        cmp     rcx, byte SIZEOF_ZMMWORD
        jae     near .columnloop

%if RGB_PIXELSIZE == 3
.column_ld1:
        push    rax
        push    rdx
        lea     rcx,[rcx+rcx*2]         ; imul ecx,RGB_PIXELSIZE
        test    cl, SIZEOF_BYTE
        jz      short .column_ld2
        sub     rcx, byte SIZEOF_BYTE
        movzx   rax, BYTE [rsi+rcx]
.column_ld2:
        test    cl, SIZEOF_WORD
        jz      short .column_ld4
        sub     rcx, byte SIZEOF_WORD
        movzx   rdx, WORD [rsi+rcx]
        shl     rax, WORD_BIT
        or      rax,rdx
.column_ld4:
        vmovd    xmmA,eax
        pop     rdx
        pop     rax
        test    cl, SIZEOF_DWORD
        jz      short .column_ld8
        sub     rcx, byte SIZEOF_DWORD
        vmovd    xmmF, XMM_DWORD [rsi+rcx]
        vpslldq  xmmA,xmmA, SIZEOF_DWORD
        vpor     xmmA,xmmA,xmmF
.column_ld8:
        test    cl, SIZEOF_MMWORD
        jz      short .column_ld16
        sub     rcx, byte SIZEOF_MMWORD
        vmovq    xmmB, XMM_MMWORD [rsi+rcx]
        vpslldq  xmmA,xmmA, SIZEOF_MMWORD
        vpor     xmmA,xmmA,xmmB
.column_ld16:
        test    cl, SIZEOF_XMMWORD
        jz      short .column_ld32
        sub     rcx, byte SIZEOF_XMMWORD
        vmovdqu    xmmB, XMM_MMWORD [rsi+rcx]
        vperm2i128 ymmA,ymmA,ymmA,1
        vpor     ymmA,ymmB
.column_ld32:
        test    cl, SIZEOF_YMMWORD
        jz      short .column_ld64
        sub     rcx, byte SIZEOF_YMMWORD
        vmovdqu  ymmB, YMMWORD [rsi+rcx]
        vshufi32x4 zmmA, zmmB, zmmA, 0x44
.column_ld64:
        test    cl, SIZEOF_ZMMWORD
        jz      short .column_1d128
        sub     rcx, byte SIZEOF_ZMMWORD
        vmovdqa64 zmmF, zmmA
        vmovdqu64 zmmA, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
.column_1d128:
        test    cl, 2*SIZEOF_ZMMWORD
        mov     rcx, SIZEOF_ZMMWORD
        jz      short .rgb_gray_cnv
        vmovdqa64  zmmB,zmmA
        vmovdqu64  zmmA, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmmF, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]
        jmp     short .rgb_gray_cnv
       
.columnloop:
        vmovdqu64  zmmA, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmmF, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]
        vmovdqu64  zmmB, ZMMWORD [rsi+2*SIZEOF_ZMMWORD]

.rgb_gray_cnv:

        vmovdqu64 zmmD, zmmA             ; zmmD = 00 01 .. 10 11 .... 20 21 .. 30 31 ..
        vmovdqu64 zmmE, zmmF             ; zmmE = 40 41 .. 50 51 .... 60 61 .. 70 71 ..
        vmovdqu64 zmmG, zmmB             ; zmmG = 80 81 .. 90 91 .... a0 a1 .. b0 b1 ..
        vinserti64x4 zmmC, zmmF, ymmB, 0 ; zmmC = 80 81 .. 90 91 .... 60 61 .. 70 71 ..
        vshufi32x4 zmmA, zmmA, zmmC, 0x6c; zmmA = 00 01 .. 30 31 .... 60 61 .. 90 91 ..

        vinserti32x4 zmmC, zmmD, xmmE, 0 ; zmmC = 40 41 .. 10 11 ...........
        vshufi32x4 zmmH, zmmG, zmmE, 0xc8; zmmH = 80 81 .. a0 a1 .... 60 61 .. 70 71 ..
        vshufi32x4 zmmF, zmmC, zmmH, 0x71; zmmF = 10 11 .. 40 41 .... 70 71 .. a0 a1 ..
        
        vinserti64x4 zmmC, zmmD, ymmE, 0 ; zmmC = 40 41 .. 50 51 .... 20 21 .. 30 31 ..
        vshufi32x4 zmmB, zmmC, zmmG, 0xc6; zmmB = 20 21 .. 50 51 .... 80 81 .. b0 b1 ..

        vmovdqa64 zmmG,zmmA
        vpslldq zmmA,zmmA,8
        vpsrldq zmmG,zmmG,8

        vpunpckhbw zmmA,zmmA,zmmF
        vpslldq zmmF,zmmF,8
        vpunpcklbw zmmG,zmmG,zmmB
        vpunpckhbw zmmF, zmmF, zmmB
        vmovdqa64 zmmD,zmmA
        vpslldq zmmA,zmmA,8
        vpsrldq zmmD,zmmD,8

        vpunpckhbw zmmA,zmmA,zmmG
        vpslldq zmmG,zmmG,8
        vpunpcklbw zmmD,zmmD,zmmF
        vpunpckhbw zmmG,zmmG,zmmF
        vmovdqa64 zmmE, zmmA
        vpslldq zmmA,zmmA,8
        vpsrldq zmmE, zmmE, 8
        vpunpckhbw zmmA,zmmA,zmmD
        vpslldq zmmD,zmmD,8
        vpunpcklbw zmmE,zmmE,zmmG
        vpunpckhbw zmmD,zmmD,zmmG
        vpxord zmmH,zmmH,zmmH
        vmovdqa64 zmmC,zmmA
        vpunpcklbw zmmA,zmmA,zmmH
        vpunpckhbw zmmC,zmmC,zmmH
        vmovdqa64 zmmB,zmmE
        vpunpcklbw zmmE,zmmE,zmmH
        vpunpckhbw zmmB,zmmB,zmmH

        vmovdqa64 zmmF,zmmD
        vpunpcklbw zmmD,zmmD,zmmH
        vpunpckhbw zmmF,zmmF,zmmH

%else ; RGB_PIXELSIZE == 4 

.column_ld1:
        test    cl, SIZEOF_YMMWORD/32
        jz      short .column_ld2
        sub     rcx, byte SIZEOF_YMMWORD/32
        vmovd    xmmA, XMM_DWORD [rsi+rcx*RGB_PIXELSIZE]
.column_ld2:
        test    cl, SIZEOF_YMMWORD/16
        jz      short .column_ld4
        sub     rcx, byte SIZEOF_YMMWORD/16
        vmovq       xmmE, XMM_MMWORD [rsi+rcx*RGB_PIXELSIZE]
        vpslldq     xmmA,xmmA, SIZEOF_MMWORD
        vpor     xmmA,xmmA,xmmE
.column_ld4:
        test    cl, SIZEOF_YMMWORD/8
        jz      short .column_ld8
        sub     rcx, byte SIZEOF_YMMWORD/8
        vmovdqa  xmmE,xmmA
        vperm2i128 ymmE,ymmE,ymmE,1
        vmovdqu  xmmA, XMMWORD [rsi+rcx*RGB_PIXELSIZE]
        vpor     ymmA,ymmA,ymmE
.column_ld8:
        test    cl, SIZEOF_YMMWORD/4
        jz      short .column_ld16
        sub     rcx, byte SIZEOF_YMMWORD/4
        vmovdqa  ymmE,ymmA
        vmovdqu  ymmA, YMMWORD [rsi+rcx*RGB_PIXELSIZE]
        vshufi32x4 zmmA, zmmA, zmmE, 0x44
.column_ld16:
        test    cl, SIZEOF_YMMWORD/2
        jz      short .column_ld32
        sub     rcx, byte SIZEOF_YMMWORD/2
        vmovdqa64  zmmE,zmmA
        vmovdqu64  zmmA, ZMMWORD [rsi+rcx*RGB_PIXELSIZE]
.column_ld32:
        test    cl, SIZEOF_YMMWORD
        mov     rcx, SIZEOF_ZMMWORD
        jz      short .rgb_gray_cnv
        vmovdqa64  zmmF,zmmA
        vmovdqa64  zmmH,zmmE
        vmovdqu64  zmmA, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmmE, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]
        jmp     short .rgb_gray_cnv

.columnloop:
        vmovdqu64  zmmA, ZMMWORD [rsi+0*SIZEOF_ZMMWORD]
        vmovdqu64  zmmE, ZMMWORD [rsi+1*SIZEOF_ZMMWORD]
        vmovdqu64  zmmF, ZMMWORD [rsi+2*SIZEOF_ZMMWORD]
        vmovdqu64  zmmH, ZMMWORD [rsi+3*SIZEOF_ZMMWORD]

.rgb_gray_cnv:
        vmovdqa64 zmmB, zmmA                    ; zmmB = 00 01 .. 10 11 .... 20 21 .. 30 31 ..
        vmovdqa64 zmmC, zmmE                    ; zmmC = 40 41 .. 50 51 .... 60 61 .. 70 71 ..
        vmovdqa64 zmmD, zmmF                    ; zmmD = 80 81 .. 90 91 .... a0 a1 .. b0 b1 ..
        vmovdqa64 zmmG, zmmH                    ; zmmG = c0 c1 .. d0 d1 .... e0 e1 .. f0 f1 ..

        vshufi32x4 zmmI, zmmB, zmmC, 0b10001000 ; zmmI = 00 01 .. 20 21 .... 40 41 .. 60 61 .. 
        vshufi32x4 zmmJ, zmmD, zmmG, 0b10001000 ; zmmJ = 80 81 .. a0 a1 .... c0 c1 .. e0 e1 .. 
        vshufi32x4 zmmA, zmmI, zmmJ, 0b10001000 ; zmmA = 00 01 .. 40 41 .... 80 81 .. c0 c1 ..
        vshufi32x4 zmmF, zmmI, zmmJ, 0b11011101 ; zmmF = 20 21 .. 60 61 .... a0 a1 .. e0 e1 .. 

        vshufi32x4 zmmI, zmmB, zmmC, 0b11011101 ; zmmI = 10 11 .. 30 31 .... 50 51 .. 70 71 ..
        vshufi32x4 zmmJ, zmmD, zmmG, 0b11011101 ; zmmJ = 90 91 .. b0 b1 .... d0 d1 .. f0 f1 .. 
        vshufi32x4 zmmE, zmmI, zmmJ, 0b10001000 ; zmmE = 10 11 .. 50 51 .... 90 91 .. d0 d1 ..
        vshufi32x4 zmmH, zmmI, zmmJ, 0b11011101 ; zmmH = 30 31 .. 70 71 .... b0 b1 .. f0 f1 ..

        vmovdqa64  zmmD,zmmA 
        vpunpcklbw zmmA,zmmA,zmmE 
        vpunpckhbw zmmD,zmmD,zmmE 

        vmovdqa64  zmmC,zmmF 
        vpunpcklbw zmmF,zmmF,zmmH
        vpunpckhbw zmmC,zmmC,zmmH

        vmovdqa64  zmmB,zmmA 
        vpunpcklwd zmmA,zmmA,zmmF
        vpunpckhwd zmmB,zmmB,zmmF

        vmovdqa64  zmmG,zmmD 
        vpunpcklwd zmmD,zmmD,zmmC
        vpunpckhwd zmmG,zmmG,zmmC

        vmovdqa64  zmmE,zmmA 
        vpunpcklbw zmmA,zmmA,zmmD
        vpunpckhbw zmmE,zmmE,zmmD

        vmovdqa64  zmmH,zmmB 
        vpunpcklbw zmmB,zmmB,zmmG
        vpunpckhbw zmmH,zmmH,zmmG

        vpxord     zmmF,zmmF,zmmF 

        vmovdqa64  zmmC,zmmA 
        vpunpcklbw zmmA,zmmA,zmmF
        vpunpckhbw zmmC,zmmC,zmmF

        vmovdqa64  zmmD,zmmB 
        vpunpcklbw zmmB,zmmB,zmmF
        vpunpckhbw zmmD,zmmD,zmmF

        vmovdqa64  zmmG,zmmE 
        vpunpcklbw zmmE,zmmE,zmmF
        vpunpckhbw zmmG,zmmG,zmmF

        vpunpcklbw zmmF,zmmF,zmmH 
        vpunpckhbw zmmH,zmmH,zmmH 
        vpsrlw     zmmF,zmmF,8 
        vpsrlw     zmmH,zmmH,8 
%endif ; RGB_PIXELSIZE 

        ; (Original)
        ; Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
        ;
        ; (This implementation)
        ; Y  =  0.29900 * R + 0.33700 * G + 0.11400 * B + 0.25000 * G

        vmovdqa64  zmm6,zmm1
        vpunpcklwd zmm1,zmm1,zmm3
        vpunpckhwd zmm6,zmm6,zmm3
        vpmaddwd   zmm1,zmm1,[rel PW_F0299_F0337] ; zmm1=ROL*FIX(0.299)+GOL*FIX(0.337)
        vpmaddwd   zmm6,zmm6,[rel PW_F0299_F0337] ; zmm6=ROH*FIX(0.299)+GOH*FIX(0.337)

        vmovdqa64  zmm7, zmm6    ; zmm7=ROH*FIX(0.299)+GOH*FIX(0.337)

        vmovdqa64  zmm6,zmm0
        vpunpcklwd zmm0,zmm0,zmm2
        vpunpckhwd zmm6,zmm6,zmm2
        vpmaddwd   zmm0,zmm0,[rel PW_F0299_F0337] ; zmm0=REL*FIX(0.299)+GEL*FIX(0.337)
        vpmaddwd   zmm6,zmm6,[rel PW_F0299_F0337] ; zmm6=REH*FIX(0.299)+GEH*FIX(0.337)

        vmovdqa64  ZMMWORD [wk(0)], zmm0 ; wk(0)=REL*FIX(0.299)+GEL*FIX(0.337)
        vmovdqa64  ZMMWORD [wk(1)], zmm6 ; wk(1)=REH*FIX(0.299)+GEH*FIX(0.337)

        vmovdqa64  zmm0, zmm5    ; zmm0=BO
        vmovdqa64  zmm6, zmm4    ; zmm6=BE

        vmovdqa64  zmm4,zmm0
        vpunpcklwd zmm0,zmm0,zmm3
        vpunpckhwd zmm4,zmm4,zmm3
        vpmaddwd   zmm0,zmm0,[rel PW_F0114_F0250] ; zmm0=BOL*FIX(0.114)+GOL*FIX(0.250)
        vpmaddwd   zmm4,zmm4,[rel PW_F0114_F0250] ; zmm4=BOH*FIX(0.114)+GOH*FIX(0.250)

        vmovdqa64  zmm3,[rel PD_ONEHALF] ; zmm3=[PD_ONEHALF]

        vpaddd     zmm0,zmm0, zmm1
        vpaddd     zmm4,zmm4, zmm7
        vpaddd     zmm0,zmm0,zmm3
        vpaddd     zmm4,zmm4,zmm3
        vpsrld     zmm0,zmm0,SCALEBITS        ; zmm0=YOL
        vpsrld     zmm4,zmm4,SCALEBITS        ; zmm4=YOH
        vpackssdw  zmm0,zmm0,zmm4             ; zmm0=YO

        vmovdqa64  zmm4,zmm6
        vpunpcklwd zmm6,zmm6,zmm2
        vpunpckhwd zmm4,zmm4,zmm2
        vpmaddwd   zmm6,zmm6,[rel PW_F0114_F0250] ; zmm6=BEL*FIX(0.114)+GEL*FIX(0.250)
        vpmaddwd   zmm4,zmm4,[rel PW_F0114_F0250] ; zmm4=BEH*FIX(0.114)+GEH*FIX(0.250)

        vmovdqa64  zmm2,[rel PD_ONEHALF] ; zmm2=[PD_ONEHALF]

        vpaddd     zmm6,zmm6, ZMMWORD [wk(0)]
        vpaddd     zmm4,zmm4, ZMMWORD [wk(1)]
        vpaddd     zmm6,zmm6,zmm2
        vpaddd     zmm4,zmm4,zmm2
        vpsrld     zmm6,zmm6,SCALEBITS        ; zmm6=YEL
        vpsrld     zmm4,zmm4,SCALEBITS        ; zmm4=YEH
        vpackssdw  zmm6,zmm6,zmm4             ; zmm6=YE

        vpsllw     zmm0,zmm0,BYTE_BIT
        vpord      zmm6,zmm6,zmm0             ; zmm6=Y
        vmovdqu64  ZMMWORD [rdi], zmm6   ; Save Y

        sub     rcx, byte SIZEOF_ZMMWORD
        add     rsi, RGB_PIXELSIZE*SIZEOF_ZMMWORD  ; inptr
        add     rdi, byte SIZEOF_ZMMWORD                ; outptr0
        cmp     rcx, byte SIZEOF_ZMMWORD
        jae     near .columnloop
        test    rcx,rcx
        jnz     near .column_ld1

        pop     rcx                     ; col
        pop     rsi
        pop     rdi

        add     rsi, byte SIZEOF_JSAMPROW       ; input_buf
        add     rdi, byte SIZEOF_JSAMPROW
        dec     rax                             ; num_rows
        jg      near .rowloop

.return:
        pop     rbx
        uncollect_args 5
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   64
