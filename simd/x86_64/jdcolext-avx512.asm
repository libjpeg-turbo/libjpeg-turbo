;
; jdcolext.asm - colorspace conversion (64-bit AVX512)
;
; Copyright 2009, 2012 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright (C) 2009, 2012, 2016, D. R. Commander.
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

;TODO
;Need check -tile (for small image)
;May improve perforance by remove some code, fg 'collect_args'

%include "jcolsamp.inc"

; --------------------------------------------------------------------------
;
; Convert some rows of samples to the output colorspace.
;
; GLOBAL(void)
; jsimd_ycc_rgb_convert_avx512 (JDIMENSION out_width,
;                             JSAMPIMAGE input_buf, JDIMENSION input_row,
;                             JSAMPARRAY output_buf, int num_rows)
;

; r10 = JDIMENSION out_width
; r11 = JSAMPIMAGE input_buf
; r12 = JDIMENSION input_row
; r13 = JSAMPARRAY output_buf
; r14 = int num_rows

%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_ZMMWORD ; zmmword wk[WK_NUM]
%define WK_NUM          2

        align   64
        global  EXTN(jsimd_ycc_rgb_convert_avx512)

EXTN(jsimd_ycc_rgb_convert_avx512):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_ZMMWORD)     ; align to 512 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        collect_args 5
        push    rbx

        mov     rcx, r10        ; num_cols
        test    rcx,rcx
        jz      near .return

        push    rcx

        mov     rdi, r11
        mov     rcx, r12
        mov     rsi, JSAMPARRAY [rdi+0*SIZEOF_JSAMPARRAY]
        mov     rbx, JSAMPARRAY [rdi+1*SIZEOF_JSAMPARRAY]
        mov     rdx, JSAMPARRAY [rdi+2*SIZEOF_JSAMPARRAY]
        lea     rsi, [rsi+rcx*SIZEOF_JSAMPROW]
        lea     rbx, [rbx+rcx*SIZEOF_JSAMPROW]
        lea     rdx, [rdx+rcx*SIZEOF_JSAMPROW]

        pop     rcx

        mov     rdi, r13
        mov     eax, r14d
        test    rax,rax
        jle     near .return
.rowloop:
        push    rax
        push    rdi
        push    rdx
        push    rbx
        push    rsi
        push    rcx                         ; col

        mov     rsi, JSAMPROW [rsi]         ; inptr0, Y
        mov     rbx, JSAMPROW [rbx]         ; inptr1, Cb
        mov     rdx, JSAMPROW [rdx]         ; inptr2, Cr
        mov     rdi, JSAMPROW [rdi]         ; outptr
.columnloop:

        vmovdqu64  zmm5, ZMMWORD [rbx]      ; zmm5=Cb(0123456789ABCDEF)
        vmovdqu64  zmm1, ZMMWORD [rdx]      ; zmm1=Cr(0123456789ABCDEF)

        vpcmpeqw   ymm0,ymm0
        vshufi32x4 zmm0, zmm0, zmm0, 0x44   ; zmm0={0xFF 0xFF 0xFF 0xFF ..}
        vmovdqa64  zmm7, zmm0
        vpsrlw     zmm0,zmm0,BYTE_BIT       ; zmm0={0xFF 0x00 0xFF 0x00 ..}
        vpsllw     zmm7,zmm7,7              ; zmm7={0xFF80 0xFF80 0xFF80 ..}

        vpandd     zmm4,zmm0,zmm5           ; zmm4=Cb(02468ACE}=CbEven
        vpsrlw     zmm5,zmm5,BYTE_BIT       ; zmm5=Cb(13579BDF}=CBOdd
        vpandd     zmm0,zmm0,zmm1           ; zmm0=Cr(02468ACE)=CrEven
        vpsrlw     zmm1,zmm1,BYTE_BIT       ; zmm1=Cr(13579BDF)=CrOdd

        ; (Original)
        ; R = Y                + 1.40200 * Cr
        ; G = Y - 0.34414 * Cb - 0.71414 * Cr
        ; B = Y + 1.77200 * Cb
        ;
        ; (This implementation)
        ; R = Y                + 0.40200 * Cr + Cr
        ; G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr
        ; B = Y - 0.22800 * Cb + Cb + Cb

        vpaddw     zmm2,zmm4,zmm7          ; zmm2=CbE
        vpaddw     zmm3,zmm5,zmm7          ; zmm3=CbO
        vpaddw     zmm6,zmm0,zmm7          ; zmm6=CrE
        vpaddw     zmm7,zmm1,zmm7          ; zmm7=CrO

        vpaddw     zmm4,zmm2,zmm2          ; zmm4=2*CbE
        vpaddw     zmm5,zmm3,zmm3          ; zmm5=2*CbO
        vpaddw     zmm0,zmm6,zmm6          ; zmm0=2*CrE
        vpaddw     zmm1,zmm7,zmm7          ; zmm1=2*CrO  

        vpmulhw    zmm4,zmm4,[rel PW_MF0228]    
        vpmulhw    zmm5,zmm5,[rel PW_MF0228]    
        vpmulhw    zmm0,zmm0,[rel PW_F0402]     
        vpmulhw    zmm1,zmm1,[rel PW_F0402]     

        vpaddw     zmm4,zmm4,[rel PW_ONE]
        vpaddw     zmm5,zmm5,[rel PW_ONE]
        vpsraw     zmm4,zmm4,1                  
        vpsraw     zmm5,zmm5,1                  
        vpaddw     zmm0,zmm0,[rel PW_ONE]
        vpaddw     zmm1,zmm1,[rel PW_ONE]
        vpsraw     zmm0,zmm0,1                  
        vpsraw     zmm1,zmm1,1                  

        vpaddw     zmm4,zmm4,zmm2
        vpaddw     zmm5,zmm5,zmm3
        vpaddw     zmm4,zmm4,zmm2               
        vpaddw     zmm5,zmm5,zmm3               
        vpaddw     zmm0,zmm0,zmm6               
        vpaddw     zmm1,zmm1,zmm7               

        vmovdqa64  ZMMWORD [wk(0)], zmm4   
        vmovdqa64  ZMMWORD [wk(1)], zmm5   

        vpunpckhwd zmm4,zmm2,zmm6     				
        vpunpcklwd zmm2,zmm2,zmm6
        vpmaddwd   zmm2,zmm2,[rel PW_MF0344_F0285]
        vpmaddwd   zmm4,zmm4,[rel PW_MF0344_F0285]
        vpunpckhwd zmm5,zmm3,zmm7
        vpunpcklwd zmm3,zmm3,zmm7
        vpmaddwd   zmm3,zmm3,[rel PW_MF0344_F0285]
        vpmaddwd   zmm5,zmm5,[rel PW_MF0344_F0285]

        vpaddd     zmm2,zmm2,[rel PD_ONEHALF]
        vpaddd     zmm4,zmm4,[rel PD_ONEHALF]
        vpsrad     zmm2,zmm2,SCALEBITS
        vpsrad     zmm4,zmm4,SCALEBITS
        vpaddd     zmm3,zmm3,[rel PD_ONEHALF]
        vpaddd     zmm5,zmm5,[rel PD_ONEHALF]
        vpsrad     zmm3,zmm3,SCALEBITS
        vpsrad     zmm5,zmm5,SCALEBITS

        vpackssdw  zmm2,zmm2,zmm4     
        vpackssdw  zmm3,zmm3,zmm5     
        vpsubw     zmm2,zmm2,zmm6     
        vpsubw     zmm3,zmm3,zmm7     

        vmovdqu64  zmm5, ZMMWORD [rsi]   

        vpcmpeqw   ymm4,ymm4,ymm4
        vshufi32x4 zmm4,zmm4,zmm4, 0x44   ; zmm4={0xFF 0xFF 0xFF 0xFF ..}
        vpsrlw     zmm4,zmm4,BYTE_BIT     ; zmm4={0xFF 0x00 0xFF 0x00 ..} 
        vpandd     zmm4,zmm4,zmm5             
        vpsrlw     zmm5,zmm5,BYTE_BIT         

        vpaddw     zmm0,zmm0,zmm4             
        vpaddw     zmm1,zmm1,zmm5             
        vpackuswb  zmm0,zmm0,zmm0             
        vpackuswb  zmm1,zmm1,zmm1             

        vpaddw     zmm2,zmm2,zmm4             
        vpaddw     zmm3,zmm3,zmm5             
        vpackuswb  zmm2,zmm2,zmm2             
        vpackuswb  zmm3,zmm3,zmm3             

        vpaddw     zmm4,zmm4, ZMMWORD [wk(0)] 
        vpaddw     zmm5,zmm5, ZMMWORD [wk(1)] 
        vpackuswb  zmm4,zmm4,zmm4             
        vpackuswb  zmm5,zmm5,zmm5             

%if RGB_PIXELSIZE == 3 ; ---------------

        ; zmmA=(00 02 04 06 08 0A 0C 0E **), zmmB=(01 03 05 07 09 0B 0D 0F **)
        ; zmmC=(10 12 14 16 18 1A 1C 1E **), zmmD=(11 13 15 17 19 1B 1D 1F **)
        ; zmmE=(20 22 24 26 28 2A 2C 2E **), zmmF=(21 23 25 27 29 2B 2D 2F **)
        ; zmmG=(** ** ** ** ** ** ** ** **), zmmH=(** ** ** ** ** ** ** ** **)

        vpunpcklbw zmmA,zmmA,zmmC       ; zmmA=(00 10 02 12 04 14 06 16 08 18 0A 1A 0C 1C 0E 1E ..)
        vpunpcklbw zmmE,zmmE,zmmB       ; zmmE=(20 01 22 03 24 05 26 07 28 09 2A 0B 2C 0D 2E 0F ..)
        vpunpcklbw zmmD,zmmD,zmmF       ; zmmD=(11 21 13 23 15 25 17 27 19 29 1B 2B 1D 2D 1F 2F ..)

        vpsrldq    zmmH,zmmA,2          ; zmmH=(02 12 04 14 06 16 08 18 0A 1A 0C 1C 0E 1E -- --)
        vpunpckhwd zmmG,zmmA,zmmE       ; zmmG=(08 18 28 09 0A 1A 2A 0B 0C 1C 2C 0D 0E 1E 2E 0F)
        vpunpcklwd zmmA,zmmA,zmmE       ; zmmA=(00 10 20 01 02 12 22 03 04 14 24 05 06 16 26 07)

        vpsrldq    zmmE,zmmE,2          ; zmmE=(22 03 24 05 26 07 28 09 2A 0B 2C 0D 2E 0F -- --)

        vmovdqa64  zmmC,zmmD            
        vpsrldq    zmmB,zmmD,2          ; zmmB=(13 23 15 25 17 27 19 29 1B 2B 1D 2D 1F 2F -- --)
        vpunpckhwd zmmC,zmmD,zmmH       ; zmmC=(19 29 0A 1A 1B 2B 0C 1C 1D 2D 0E 1E 1F 2F -- --)
        vpunpcklwd zmmD,zmmD,zmmH       ; zmmD=(11 21 02 12 13 23 04 14 15 25 06 16 17 27 08 18)

        vpunpckhwd zmmF,zmmE,zmmB       ; zmmF=(2A 0B 1B 2B 2C 0D 1D 2D 2E 0F 1F 2F -- -- -- --)
        vpunpcklwd zmmE,zmmE,zmmB       ; zmmE=(22 03 13 23 24 05 15 25 26 07 17 27 28 09 19 29)

        vpshufd    zmmH,zmmA,0x4E       ; zmmH=(04 14 24 05 06 16 26 07 00 10 20 01 02 12 22 03)
        vpunpckldq zmmA,zmmA,zmmD       ; zmmA=(00 10 20 01 11 21 02 12 02 12 22 03 13 23 04 14)
        vpunpckhdq zmmD,zmmD,zmmE       ; zmmD=(15 25 06 16 26 07 17 27 17 27 08 18 28 09 19 29)
        vpunpckldq zmmE,zmmE,zmmH       ; zmmE=(22 03 13 23 04 14 24 05 24 05 15 25 06 16 26 07)

        vpshufd    zmmH,zmmG,0x4E       ; zmmH=(0C 1C 2C 0D 0E 1E 2E 0F 08 18 28 09 0A 1A 2A 0B)
        vpunpckldq zmmG,zmmG,zmmC       ; zmmG=(08 18 28 09 19 29 0A 1A 0A 1A 2A 0B 1B 2B 0C 1C)
        vpunpckhdq zmmC,zmmC,zmmF       ; zmmC=(1D 2D 0E 1E 2E 0F 1F 2F 1F 2F -- -- -- -- -- --)
        vpunpckldq zmmF,zmmF,zmmH       ; zmmF=(2A 0B 1B 2B 0C 1C 2C 0D 2C 0D 1D 2D 0E 1E 2E 0F)

        vpunpcklqdq zmmH,zmmA,zmmE      ; zmmH=(00 01 .. 30 31 .... 60 61 .. 90 91 ..)
        vpunpcklqdq zmmG,zmmD,zmmG      ; zmmG=(10 11 .. 40 41 .... 70 71 .. a0 a1 ..)
        vpunpcklqdq zmmC,zmmF,zmmC      ; zmmC=(20 21 .. 50 51 .... 80 81 .. b0 b1 ..)

        vinserti32x4 zmmB, zmmH, xmmG, 1 ; zmmB=(00 01 .. 10 11 .... 60 61 .. 90 91 ..)
        vinserti32x4 zmmE, zmmH, xmmC, 0 ; zmmE=(20 21 .. 30 31 .... 60 61 .. 90 91 ..)
        vshufi32x4  zmmA, zmmB, zmmE, 0b01000100 ; zmmA=(00 01 .. 10 11 .... 20 21 .. 30 31 ..)

        vshufi32x4 zmmB, zmmG, zmmC, 0b11011101  ; zmmB=(40 41 .. a0 a1 .... 50 51 .. b0 b1 ..)
        vshufi32x4 zmmE, zmmH, zmmG, 0b11101110  ; zmmE=(60 61 .. 90 91 .... 70 71 .. a0 a1 ..)
        vshufi32x4 zmmD, zmmB, zmmE, 0b10001000  ; zmmD=(40 41 .. 50 51 .... 60 61 .. 70 71 ..)

        vshufi32x4 zmmE, zmmC, zmmH, 0b00110010  ; zmmE=(80 81 .. 20 21 .... 90 91 ...........)
        vshufi32x4 zmmF, zmmE, zmmB, 0b11011000  ; zmmF=(80 81 .. 90 91 .... a0 a1 .. b0 b1 ..)


        cmp     rcx, byte SIZEOF_ZMMWORD
        jb      short .column_st128

        test    rdi, SIZEOF_ZMMWORD-1
        jnz     short .out1
        ; --(aligned)-------------------
        vmovntdq ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovntdq ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        vmovntdq ZMMWORD [rdi+2*SIZEOF_ZMMWORD], zmmF
        jmp     short .out0
.out1:  ; --(unaligned)-----------------
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        vmovdqu64  ZMMWORD [rdi+2*SIZEOF_ZMMWORD], zmmF
.out0:
        add     rdi, RGB_PIXELSIZE*SIZEOF_ZMMWORD  ; outptr
        sub     rcx, byte SIZEOF_ZMMWORD
        jz      near .nextrow

        add     rsi, byte SIZEOF_ZMMWORD        ; inptr0
        add     rbx, byte SIZEOF_ZMMWORD        ; inptr1
        add     rdx, byte SIZEOF_ZMMWORD        ; inptr2
        jmp     near .columnloop

.column_st128:
        lea     rcx, [rcx+rcx*2]                ; imul ecx, RGB_PIXELSIZE
        cmp     rcx,  2*SIZEOF_ZMMWORD
        jb      short .column_st64
        vmovdqu64 ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovdqu64 ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        add     rdi, 2*SIZEOF_ZMMWORD      ; outptr
        vmovdqa64 zmmA,zmmF
        sub     rcx, 2*SIZEOF_ZMMWORD
        jmp     short .column_st63

.column_st64:
        cmp     rcx, byte SIZEOF_ZMMWORD
        jb      short .column_st63
        vmovdqu64 ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        add     rdi, byte SIZEOF_ZMMWORD      ; outptr
        vmovdqa64 zmmA,zmmD
        sub     rcx, byte SIZEOF_ZMMWORD

.column_st63:
        cmp     rcx, byte SIZEOF_YMMWORD
        jb      short .column_st31
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymmA
        add     rdi, byte SIZEOF_YMMWORD      ; outptr
        vshufi32x4 zmmA, zmmA, zmmA, 0b00101110
        sub     rcx, byte SIZEOF_YMMWORD
.column_st31:
        cmp     rcx, byte SIZEOF_XMMWORD
        jb      short .column_st15
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmmA
        add     rdi, byte SIZEOF_XMMWORD        ; outptr
        vperm2i128 ymmA,ymmA,ymmA,1
        sub     rcx, byte SIZEOF_XMMWORD
.column_st15:
        ; Store the lower 8 bytes of xmmA to the output when it has enough
        ; space.
        cmp     rcx, byte SIZEOF_MMWORD
        jb      short .column_st7
        vmovq    XMM_MMWORD [rdi], xmmA
        add     rdi, byte SIZEOF_MMWORD
        sub     rcx, byte SIZEOF_MMWORD
        vpsrldq  xmmA,xmmA, SIZEOF_MMWORD
.column_st7:
        ; Store the lower 4 bytes of xmmA to the output when it has enough
        ; space.
        cmp     rcx, byte SIZEOF_DWORD
        jb      short .column_st3
        vmovd    XMM_DWORD [rdi], xmmA
        add     rdi, byte SIZEOF_DWORD
        sub     rcx, byte SIZEOF_DWORD
        vpsrldq xmmA, xmmA, SIZEOF_DWORD
.column_st3:
        ; Store the lower 2 bytes of rax to the output when it has enough
        ; space.
        vmovd    eax, xmmA
        cmp     rcx, byte SIZEOF_WORD
        jb      short .column_st1
        mov     WORD [rdi], ax
        add     rdi, byte SIZEOF_WORD
        sub     rcx, byte SIZEOF_WORD
        shr     rax, 16
.column_st1:
        ; Store the lower 1 byte of rax to the output when it has enough
        ; space.
        test    rcx, rcx
        jz      short .nextrow
        mov     BYTE [rdi], al

%else ; RGB_PIXELSIZE == 4 ; -----------

%ifdef RGBX_FILLER_0XFF
        vpcmpeqb   ymm6,ymm6,ymm6             
        vshufi32x4 zmm6, zmm6, zmm6, 0x44   ; zmm6={0xFF 0xFF 0xFF 0xFF ..}
        vmovdqa64  zmm7, zmm6
%else
        vpxord     zmm6,zmm6,zmm6             
        vpxord     zmm7,zmm7,zmm7             
%endif

        vpunpcklbw zmmA,zmmA,zmmC     
        vpunpcklbw zmmE,zmmE,zmmG     
        vpunpcklbw zmmB,zmmB,zmmD     
        vpunpcklbw zmmF,zmmF,zmmH     

        vpunpckhwd zmmC,zmmA,zmmE     
        vpunpcklwd zmmA,zmmA,zmmE     
        vpunpckhwd zmmG,zmmB,zmmF     
        vpunpcklwd zmmB,zmmB,zmmF     

        vpunpckhdq zmmE,zmmA,zmmB                 ; zmmE=(10 11 .. 50 51 .... 90 91 .. d0 d1 ..)
        vpunpckldq zmmB,zmmA,zmmB                 ; zmmB=(00 01 .. 40 41 .... 80 81 .. c0 c1 ..)
        vpunpckhdq zmmF,zmmC,zmmG                 ; zmmF=(30 31 .. 70 71 .... b0 b1 .. f0 f1 ..)
        vpunpckldq zmmG,zmmC,zmmG                 ; zmmG=(20 21 .. 60 61 .... a0 a1 .. e0 e1 ..)

        vshufi32x4 zmmJ, zmmB, zmmE, 0b01000100   ; zmmJ=(00 01 .. 40 41 .... 10 11 .. 50 51 ..)
        vshufi32x4 zmmI, zmmG, zmmF, 0b01000100   ; zmmI=(20 21 .. 60 61 .... 30 31 .. 70 71 ..)
        vshufi32x4 zmmA, zmmJ, zmmI, 0b10001000   ; zmmA=(00 01 .. 10 11 .... 20 21 .. 30 31 ..)
        vshufi32x4 zmmD, zmmJ, zmmI, 0b11011101   ; zmmA=(40 41 .. 50 51 .... 60 61 .. 70 71 ..)

        vshufi32x4 zmmJ, zmmB, zmmE, 0b11101110   ; zmmJ=(80 81 .. c0 c1 .... 90 91 .. d0 d1 ..)
        vshufi32x4 zmmI, zmmG, zmmF, 0b11101110   ; zmmI=(a0 a1 .. e0 e1 .... b0 b1 .. f0 f1 ..)
        vshufi32x4 zmmC, zmmJ, zmmI, 0b10001000   ; zmmC=(80 81 .. 90 91 .... a0 a1 .. b0 b1 ..)
        vshufi32x4 zmmH, zmmJ, zmmI, 0b11011101   ; zmmA=(c0 c1 .. d0 d1 .... e0 e1 .. f0 f1 ..)

        cmp     rcx, byte SIZEOF_ZMMWORD
        jb      short .column_st128

        test    rdi, SIZEOF_ZMMWORD-1
        jnz     short .out1
        ; --(aligned)-------------------
        vmovntdq ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovntdq ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        vmovntdq ZMMWORD [rdi+2*SIZEOF_ZMMWORD], zmmC
        vmovntdq ZMMWORD [rdi+3*SIZEOF_ZMMWORD], zmmH
        jmp     short .out0
.out1:  ; --(unaligned)-----------------
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        vmovdqu64  ZMMWORD [rdi+2*SIZEOF_ZMMWORD], zmmC
        vmovdqu64  ZMMWORD [rdi+3*SIZEOF_ZMMWORD], zmmH
.out0:
        add     rdi, RGB_PIXELSIZE*SIZEOF_ZMMWORD  ; outptr
        sub     rcx, byte SIZEOF_ZMMWORD
        jz      near .nextrow

        add     rsi, byte SIZEOF_ZMMWORD        ; inptr0
        add     rbx, byte SIZEOF_ZMMWORD        ; inptr1
        add     rdx, byte SIZEOF_ZMMWORD        ; inptr2
        jmp     near .columnloop

.column_st128:
        cmp     rcx, byte SIZEOF_ZMMWORD/2
        jb      short .column_st64
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        add     rdi, 2*SIZEOF_ZMMWORD      ; outptr
        vmovdqa64  zmmA,zmmC
        vmovdqa64  zmmD,zmmH
        sub     rcx, byte SIZEOF_ZMMWORD/2
.column_st64:
        cmp     rcx, byte SIZEOF_YMMWORD/2
        jb      short .column_st32
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        add     rdi, byte 2*SIZEOF_YMMWORD      ; outptr
        vmovdqa64 zmmA,zmmD
        sub     rcx, byte SIZEOF_YMMWORD/2
.column_st32:
    	cmp     rcx, byte SIZEOF_YMMWORD/4
        jb      short .column_st16
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymmA
        add     rdi, byte SIZEOF_YMMWORD      ; outptr
        vshufi32x4 zmmA, zmmA, zmmA, 0b00101110
        sub     rcx, byte SIZEOF_YMMWORD/4
.column_st16:
        cmp     rcx, byte SIZEOF_YMMWORD/8
        jb      short .column_st15
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmmA
        vperm2i128 ymmA,ymmA,ymmA,1
        add     rdi, byte SIZEOF_XMMWORD        ; outptr
        sub     rcx, byte SIZEOF_YMMWORD/8
.column_st15:
        ; Store two pixels (8 bytes) of ymmA to the output when it has enough
        ; space.
        cmp     rcx, byte SIZEOF_YMMWORD/16
        jb      short .column_st7
        vmovq    MMWORD [rdi], xmmA
        add     rdi, byte 8 
        sub     rcx, byte 2
        vpsrldq  xmmA, 8
.column_st7:
        ; Store one pixel (4 bytes) of ymmA to the output when it has enough
        ; space.
        test    rcx, rcx
        jz      short .nextrow
        vmovd    XMM_DWORD [rdi], xmmA

%endif ; RGB_PIXELSIZE ; ---------------

.nextrow:
        pop     rcx
        pop     rsi
        pop     rbx
        pop     rdx
        pop     rdi
        pop     rax

        add     rsi, byte SIZEOF_JSAMPROW
        add     rbx, byte SIZEOF_JSAMPROW
        add     rdx, byte SIZEOF_JSAMPROW
        add     rdi, byte SIZEOF_JSAMPROW       ; output_buf
        dec     rax                             ; num_rows
        jg      near .rowloop

        sfence          ; flush the write buffer

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
