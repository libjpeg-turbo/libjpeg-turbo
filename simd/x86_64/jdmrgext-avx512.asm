;
; jdmrgext.asm - merged upsampling/color conversion (64-bit AVX512)
;
; Copyright 2009, 2012 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009, 2012 D. R. Commander
; Copyright 2015, 2018, Intel Corporation
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
; Upsample and color convert for the case of 2:1 horizontal and 1:1 vertical.
;
; GLOBAL(void)
; jsimd_h2v1_merged_upsample_avx512 (JDIMENSION output_width,
;                                  JSAMPIMAGE input_buf,
;                                  JDIMENSION in_row_group_ctr,
;                                  JSAMPARRAY output_buf);
;

; r10 = JDIMENSION output_width
; r11 = JSAMPIMAGE input_buf
; r12 = JDIMENSION in_row_group_ctr
; r13 = JSAMPARRAY output_buf

%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_ZMMWORD ; zmmword wk[WK_NUM]
%define WK_NUM          3

        align   64
        global  EXTN(jsimd_h2v1_merged_upsample_avx512)


EXTN(jsimd_h2v1_merged_upsample_avx512):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_ZMMWORD)     ; align to 512 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        collect_args 4
        push    rbx

        mov     rcx, r10        ; col
        test    rcx,rcx
        jz      near .return

        push    rcx

        mov     rdi, r11
        mov     rcx, r12
        mov     rsi, JSAMPARRAY [rdi+0*SIZEOF_JSAMPARRAY]
        mov     rbx, JSAMPARRAY [rdi+1*SIZEOF_JSAMPARRAY]
        mov     rdx, JSAMPARRAY [rdi+2*SIZEOF_JSAMPARRAY]
        mov     rdi, r13
        mov     rsi, JSAMPROW [rsi+rcx*SIZEOF_JSAMPROW]         ; inptr0
        mov     rbx, JSAMPROW [rbx+rcx*SIZEOF_JSAMPROW]         ; inptr1
        mov     rdx, JSAMPROW [rdx+rcx*SIZEOF_JSAMPROW]         ; inptr2
        mov     rdi, JSAMPROW [rdi]                             ; outptr

        pop     rcx                     ; col

.columnloop:

        vmovdqu64  zmm6, ZMMWORD [rbx]   ; zmm6=Cb(0123456789ABCDEF)
        vmovdqu64  zmm7, ZMMWORD [rdx]   ; zmm7=Cr(0123456789ABCDEF)

        vpxord     zmm1,zmm1,zmm1             ; zmm1=(all 0's)
        vpcmpeqw   ymm3,ymm3,ymm3
        vshufi32x4 zmm3, zmm3, zmm3, 0x44
        vpsllw     zmm3,zmm3,7                ; zmm3={0xFF80 0xFF80 0xFF80 0xFF80 ..}

        vmovaps    zmm10, [rel PW_PERM_ZMM]
        vpermq     zmm6,zmm10,zmm6
        vpermq     zmm7,zmm10,zmm7
        vpunpcklbw zmm4,zmm6,zmm1             ; zmm4=Cb(01234567)=CbL
        vpunpckhbw zmm6,zmm6,zmm1             ; zmm6=Cb(89ABCDEF)=CbH
        vpunpcklbw zmm0,zmm7,zmm1             ; zmm0=Cr(01234567)=CrL
        vpunpckhbw zmm7,zmm7,zmm1             ; zmm7=Cr(89ABCDEF)=CrH

        vpaddw     zmm5,zmm6,zmm3
        vpaddw     zmm2,zmm4,zmm3
        vpaddw     zmm1,zmm7,zmm3
        vpaddw     zmm3,zmm0,zmm3

        ; (Original)
        ; R = Y                + 1.40200 * Cr
        ; G = Y - 0.34414 * Cb - 0.71414 * Cr
        ; B = Y + 1.77200 * Cb
        ;
        ; (This implementation)
        ; R = Y                + 0.40200 * Cr + Cr
        ; G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr
        ; B = Y - 0.22800 * Cb + Cb + Cb

        vpaddw   zmm6,zmm5,zmm5               ; zmm6=2*CbH
        vpaddw   zmm4,zmm2,zmm2               ; zmm4=2*CbL
        vpaddw   zmm7,zmm1,zmm1               ; zmm7=2*CrH
        vpaddw   zmm0,zmm3,zmm3               ; zmm0=2*CrL

        vpmulhw  zmm6,zmm6,[rel PW_MF0228]    ; zmm6=(2*CbH * -FIX(0.22800))
        vpmulhw  zmm4,zmm4,[rel PW_MF0228]    ; zmm4=(2*CbL * -FIX(0.22800))
        vpmulhw  zmm7,zmm7,[rel PW_F0402]     ; zmm7=(2*CrH * FIX(0.40200))
        vpmulhw  zmm0,zmm0,[rel PW_F0402]     ; zmm0=(2*CrL * FIX(0.40200))

        vpaddw   zmm6,zmm6,[rel PW_ONE]
        vpaddw   zmm4,zmm4,[rel PW_ONE]
        vpsraw   zmm6,zmm6,1                  ; zmm6=(CbH * -FIX(0.22800))
        vpsraw   zmm4,zmm4,1                  ; zmm4=(CbL * -FIX(0.22800))
        vpaddw   zmm7,zmm7,[rel PW_ONE]
        vpaddw   zmm0,zmm0,[rel PW_ONE]
        vpsraw   zmm7,zmm7,1                  ; zmm7=(CrH * FIX(0.40200))
        vpsraw   zmm0,zmm0,1                  ; zmm0=(CrL * FIX(0.40200))

        vpaddw   zmm6,zmm6,zmm5
        vpaddw   zmm4,zmm4,zmm2
        vpaddw   zmm6,zmm6,zmm5               ; zmm6=(CbH * FIX(1.77200))=(B-Y)H
        vpaddw   zmm4,zmm4,zmm2               ; zmm4=(CbL * FIX(1.77200))=(B-Y)L
        vpaddw   zmm7,zmm7,zmm1               ; zmm7=(CrH * FIX(1.40200))=(R-Y)H
        vpaddw   zmm0,zmm0,zmm3               ; zmm0=(CrL * FIX(1.40200))=(R-Y)L

        vmovdqa64 ZMMWORD [wk(0)], zmm6   ; wk(0)=(B-Y)H
        vmovdqa64 ZMMWORD [wk(1)], zmm7   ; wk(1)=(R-Y)H

        vpunpckhwd zmm6,zmm5,zmm1
        vpunpcklwd zmm5,zmm5,zmm1
        vpmaddwd   zmm5,zmm5,[rel PW_MF0344_F0285]
        vpmaddwd   zmm6,zmm6,[rel PW_MF0344_F0285]
        vpunpckhwd zmm7,zmm2,zmm3
        vpunpcklwd zmm2,zmm2,zmm3
        vpmaddwd   zmm2,zmm2,[rel PW_MF0344_F0285]
        vpmaddwd   zmm7,zmm7,[rel PW_MF0344_F0285]

        vpaddd     zmm5,zmm5,[rel PD_ONEHALF]
        vpaddd     zmm6,zmm6,[rel PD_ONEHALF]
        vpsrad     zmm5,zmm5,SCALEBITS
        vpsrad     zmm6,zmm6,SCALEBITS
        vpaddd     zmm2,zmm2,[rel PD_ONEHALF]
        vpaddd     zmm7,zmm7,[rel PD_ONEHALF]
        vpsrad     zmm2,zmm2,SCALEBITS
        vpsrad     zmm7,zmm7,SCALEBITS

        vpackssdw  zmm5,zmm5,zmm6     ; zmm5=CbH*-FIX(0.344)+CrH*FIX(0.285)
        vpackssdw  zmm2,zmm2,zmm7     ; zmm2=CbL*-FIX(0.344)+CrL*FIX(0.285)
        vpsubw     zmm5,zmm5,zmm1     ; zmm5=CbH*-FIX(0.344)+CrH*-FIX(0.714)=(G-Y)H
        vpsubw     zmm2,zmm2,zmm3     ; zmm2=CbL*-FIX(0.344)+CrL*-FIX(0.714)=(G-Y)L

        vmovdqa64  ZMMWORD [wk(2)], zmm5   ; wk(2)=(G-Y)H

        mov     al,2                    ; Yctr
        jmp     short .Yloop_1st

.Yloop_2nd:
        vmovdqa64  zmm0, ZMMWORD [wk(1)]   ; zmm0=(R-Y)H
        vmovdqa64  zmm2, ZMMWORD [wk(2)]   ; zmm2=(G-Y)H
        vmovdqa64  zmm4, ZMMWORD [wk(0)]   ; zmm4=(B-Y)H

.Yloop_1st:
        vmovdqu64  zmm7, ZMMWORD [rsi]     ; ymm7=Y(0123456789ABCDEF)

        vpcmpeqw   ymm6,ymm6,ymm6
        vshufi32x4 zmm6, zmm6, zmm6, 0x44
        vpsrlw     zmm6,zmm6,BYTE_BIT           ; zmm6={0xFF 0x00 0xFF 0x00 ..}
        vpandd     zmm6,zmm6,zmm7               ; zmm6=Y(02468ACE)=YE
        vpsrlw     zmm7,zmm7,BYTE_BIT           ; zmm7=Y(13579BDF)=YO

        vmovdqa64  zmm1,zmm0               ; zmm1=zmm0=(R-Y)(L/H)
        vmovdqa64  zmm3,zmm2               ; zmm3=zmm2=(G-Y)(L/H)
        vmovdqa64  zmm5,zmm4               ; zmm5=zmm4=(B-Y)(L/H)

        vpaddw     zmm0,zmm0,zmm6             ; zmm0=((R-Y)+YE)=RE=R(02468ACE)
        vpaddw     zmm1,zmm1,zmm7             ; zmm1=((R-Y)+YO)=RO=R(13579BDF)
        vpackuswb  zmm0,zmm0,zmm0             ; zmm0=R(02468ACE********)
        vpackuswb  zmm1,zmm1,zmm1             ; zmm1=R(13579BDF********)

        vpaddw     zmm2,zmm2,zmm6             ; zmm2=((G-Y)+YE)=GE=G(02468ACE)
        vpaddw     zmm3,zmm3,zmm7             ; zmm3=((G-Y)+YO)=GO=G(13579BDF)
        vpackuswb  zmm2,zmm2,zmm2             ; zmm2=G(02468ACE********)
        vpackuswb  zmm3,zmm3,zmm3             ; zmm3=G(13579BDF********)

        vpaddw     zmm4,zmm4,zmm6             ; zmm4=((B-Y)+YE)=BE=B(02468ACE)
        vpaddw     zmm5,zmm5,zmm7             ; zmm5=((B-Y)+YO)=BO=B(13579BDF)
        vpackuswb  zmm4,zmm4,zmm4             ; zmm4=B(02468ACE********)
        vpackuswb  zmm5,zmm5,zmm5             ; zmm5=B(13579BDF********)

%if RGB_PIXELSIZE == 3 ; ---------------

        vpunpcklbw zmmA,zmmA,zmmC     
        vpunpcklbw zmmE,zmmE,zmmB     
        vpunpcklbw zmmD,zmmD,zmmF     

        vmovdqa64  zmmG,zmmA
        vmovdqa64  zmmH,zmmA
        vpunpcklwd zmmA,zmmA,zmmE     
        vpunpckhwd zmmG,zmmG,zmmE     

        vpsrldq    zmmH,zmmH,2        
        vpsrldq    zmmE,zmmE,2        

        vmovdqa64  zmmC,zmmD
        vmovdqa64  zmmB,zmmD
        vpunpcklwd zmmD,zmmD,zmmH     
        vpunpckhwd zmmC,zmmC,zmmH     

        vpsrldq    zmmB,zmmB,2        

        vmovdqa64  zmmF,zmmE
        vpunpcklwd zmmE,zmmE,zmmB     
        vpunpckhwd zmmF,zmmF,zmmB     

        vpshufd    zmmH,zmmA,0x4E
        vmovdqa64  zmmB,zmmE
        vpunpckldq zmmA,zmmA,zmmD     
        vpunpckldq zmmE,zmmE,zmmH     
        vpunpckhdq zmmD,zmmD,zmmB     

        vpshufd    zmmH,zmmG,0x4E
        vmovdqa64  zmmB,zmmF
        vpunpckldq zmmG,zmmG,zmmC     
        vpunpckldq zmmF,zmmF,zmmH     
        vpunpckhdq zmmC,zmmC,zmmB     

        vpunpcklqdq zmmH,zmmA,zmmE    
        vpunpcklqdq zmmG,zmmD,zmmG    
        vpunpcklqdq zmmC,zmmF,zmmC    
    
        vperm2i128 ymmB,ymmH,ymmG,0x20
        vperm2i128 ymmE, ymmH, ymmC, 0b00010010
        vshufi32x4 zmmA, zmmB, zmmE, 0b01000100

        vperm2i128 ymmB,ymmG,ymmC,0b00110001
        vshufi32x4 zmmE, zmmH, zmmH, 0b00000010
        vshufi32x4 zmmE, zmmE, zmmG, 0b00100000
        vshufi32x4 zmmD, zmmB, zmmE, 0b10000100

        vshufi32x4 zmmB, zmmC, zmmH, 0b00110010
        vshufi32x4 zmmE, zmmG, zmmC, 0b00110011
        vshufi32x4 zmmF, zmmB, zmmE, 0b10001000

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
        jz      near .endcolumn

        add     rsi, byte SIZEOF_ZMMWORD        ; inptr0
        dec     al                      ; Yctr
        jnz     near .Yloop_2nd

        add     rbx, byte SIZEOF_ZMMWORD        ; inptr1
        add     rdx, byte SIZEOF_ZMMWORD        ; inptr2
        jmp     near .columnloop

.column_st128:
        lea     rcx, [rcx+rcx*2]                ; imul ecx, RGB_PIXELSIZE
        cmp     rcx, 2*SIZEOF_ZMMWORD
        jb      .column_st64
        vmovdqu64  ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        vmovdqu64  ZMMWORD [rdi+1*SIZEOF_ZMMWORD], zmmD
        add     rdi, 2*SIZEOF_ZMMWORD      ; outptr
        vmovdqa64  zmmA,zmmF
        sub     rcx, 2*SIZEOF_ZMMWORD
        jmp     short .column_st63
.column_st64:
        cmp     rcx, byte SIZEOF_ZMMWORD
        jb      short .column_st63
        vmovdqu64 ZMMWORD [rdi+0*SIZEOF_ZMMWORD], zmmA
        add     rdi, byte SIZEOF_ZMMWORD      ; outptr
        vmovdqa64 zmmA, zmmD
        sub     rcx, byte SIZEOF_ZMMWORD
.column_st63:
        cmp     rcx, byte SIZEOF_YMMWORD
        jb      short .column_st31
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymmA
        add     rdi, byte SIZEOF_YMMWORD      ; outptr
        vshufi32x4 zmmA, zmmA, zmmA, 0b00001110
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
        jz      short .endcolumn
        mov     BYTE [rdi], al

%else ; RGB_PIXELSIZE == 4 ; -----------

%ifdef RGBX_FILLER_0XFF
        vpcmpeqb   ymm6,ymm6,ymm6             
        vshufi32x4 zmm6, zmm6, zmm6,0
        vpcmpeqb   ymm7,ymm7,ymm7             
        vshufi32x4 zmm7, zmm7, zmm7,0
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

        vpunpckhdq zmmE,zmmA,zmmB     
        vpunpckldq zmmB,zmmA,zmmB     
        vpunpckhdq zmmF,zmmC,zmmG     
        vpunpckldq zmmG,zmmC,zmmG    

        vshufi32x4 zmm10, zmmB, zmmE, 0b01000100
        vshufi32x4 zmm11, zmmG, zmmF, 0b01000100
        vshufi32x4 zmmA, zmm10, zmm11, 0b10001000
        vshufi32x4 zmmD, zmm10, zmm11, 0b11011101

        vshufi32x4 zmm10, zmmB, zmmE, 0b11101110
        vshufi32x4 zmm11, zmmG, zmmF, 0b11101110
        vshufi32x4 zmmC, zmm10, zmm11, 0b10001000
        vshufi32x4 zmmH, zmm10, zmm11, 0b11011101

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
        jz      near .endcolumn

        add     rsi, byte SIZEOF_ZMMWORD        ; inptr0
        dec   al
        jnz   near .Yloop_2nd

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
        add     rdi, byte SIZEOF_ZMMWORD      ; outptr
        vmovdqu64 zmmA, zmmD
        sub     rcx, byte SIZEOF_YMMWORD/2
.column_st32:
        cmp     rcx, byte SIZEOF_YMMWORD/4
        jb      short .column_st16
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymmA
        add     rdi, byte SIZEOF_YMMWORD      ; outptr
        vshufi32x4 zmmA, zmmA, zmmA, 0b00001110
        sub     rcx, byte SIZEOF_YMMWORD/4
.column_st16:
        cmp     rcx, byte SIZEOF_YMMWORD/8
        jb      short .column_st15
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmmA
        add     rdi, byte SIZEOF_XMMWORD        ; outptr
        vperm2i128 ymmA,ymmA,ymmA,1
        sub     rcx, byte SIZEOF_YMMWORD/8
.column_st15:
        ; Store two pixels (8 bytes) of ymmA to the output when it has enough
        ; space.
        cmp     rcx, byte SIZEOF_YMMWORD/16
        jb      short .column_st7
        vmovq   XMM_MMWORD [rdi], xmmA
        add     rdi, byte 2*4
        sub     rcx, byte 2
        vpsrldq  xmmA, 2*4
.column_st7:
        ; Store one pixel (4 bytes) of ymmA to the output when it has enough
        ; space.
        test    rcx, rcx
        jz      short .endcolumn
        vmovd    XMM_DWORD [rdi], xmmA

%endif ; RGB_PIXELSIZE ; ---------------

.endcolumn:
        sfence          ; flush the write buffer

.return:
        pop     rbx
        uncollect_args 4
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret

; --------------------------------------------------------------------------
;
; Upsample and color convert for the case of 2:1 horizontal and 2:1 vertical.
;
; GLOBAL(void)
; jsimd_h2v2_merged_upsample_avx512 (JDIMENSION output_width,
;                                  JSAMPIMAGE input_buf,
;                                  JDIMENSION in_row_group_ctr,
;                                  JSAMPARRAY output_buf);
;

; r10 = JDIMENSION output_width
; r11 = JSAMPIMAGE input_buf
; r12 = JDIMENSION in_row_group_ctr
; r13 = JSAMPARRAY output_buf

        align   64
        global  EXTN(jsimd_h2v2_merged_upsample_avx512)

EXTN(jsimd_h2v2_merged_upsample_avx512):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args 4
        push    rbx

        mov     rax, r10

        mov     rdi, r11
        mov     rcx, r12
        mov     rsi, JSAMPARRAY [rdi+0*SIZEOF_JSAMPARRAY]
        mov     rbx, JSAMPARRAY [rdi+1*SIZEOF_JSAMPARRAY]
        mov     rdx, JSAMPARRAY [rdi+2*SIZEOF_JSAMPARRAY]
        mov     rdi, r13
        lea     rsi, [rsi+rcx*SIZEOF_JSAMPROW]

        push    rdx                     ; inptr2
        push    rbx                     ; inptr1
        push    rsi                     ; inptr00
        mov     rbx,rsp

        push    rdi
        push    rcx
        push    rax

        %ifdef WIN64
        mov r8, rcx
        mov r9, rdi
        mov rcx, rax
        mov rdx, rbx
        %else
        mov rdx, rcx
        mov rcx, rdi
        mov     rdi, rax
        mov rsi, rbx
        %endif

        call    EXTN(jsimd_h2v1_merged_upsample_avx512)

        pop rax
        pop rcx
        pop rdi
        pop rsi
        pop rbx
        pop rdx

        add     rdi, byte SIZEOF_JSAMPROW       ; outptr1
        add     rsi, byte SIZEOF_JSAMPROW       ; inptr01

        push    rdx                     ; inptr2
        push    rbx                     ; inptr1
        push    rsi                     ; inptr00
        mov     rbx,rsp

        push    rdi
        push    rcx
        push    rax

        %ifdef WIN64
        mov r8, rcx
        mov r9, rdi
        mov rcx, rax
        mov rdx, rbx
        %else
        mov rdx, rcx
        mov rcx, rdi
        mov     rdi, rax
        mov rsi, rbx
        %endif

        call    EXTN(jsimd_h2v1_merged_upsample_avx512)

        pop rax
        pop rcx
        pop rdi
        pop rsi
        pop rbx
        pop rdx

        pop     rbx
        uncollect_args 4
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   64
