;
; jchuff-sse2.asm - Huffman entropy encoding (64-bit SSE2)
;
; Copyright (C) 2009-2011, 2014-2016, D. R. Commander.
; Copyright (C) 2015, Matthieu Darbois.
; Copyright (C) 2018, Matthias Räncker.
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
; This file contains an SSE2 implementation for Huffman coding of one block.
; The following code is based on jchuff.c; see jchuff.c for more details.
;
; [TAB8]

%include "jsimdext.inc"

struc working_state
.next_output_byte:   resp    1   ; => next byte to write in buffer
.free_in_buffer:     resp    1   ; # of byte spaces remaining in buffer
.cur.put.buffer_simd resq    1   ; current bit-accumulation buffer
.cur.free_bits       resd    1   ; # of free bits now in it
.cur.last_dc_val     resd    4   ; last DC coef for each component
.cinfo:              resp    1   ; dump_buffer needs access to this
endstruc

struc c_derived_tbl
.ehufco:             resd    256 ; code for each symbol
.ehufsi:             resb    256 ; length of code for each symbol
; If no code has been allocated for a symbol S, ehufsi[S] contains 0
endstruc

; --------------------------------------------------------------------------
    SECTION     SEG_CONST

    alignz      32

    GLOBAL_DATA(jconst_huff_encode_one_block)
EXTN(jconst_huff_encode_one_block):

jpeg_mask_bits  dd 0x0000, 0x0001, 0x0003, 0x0007
                dd 0x000f, 0x001f, 0x003f, 0x007f
                dd 0x00ff, 0x01ff, 0x03ff, 0x07ff
                dd 0x0fff, 0x1fff, 0x3fff, 0x7fff

    alignz      32

       times 1 << 14 db 15
       times 1 << 13 db 14
       times 1 << 12 db 13
       times 1 << 11 db 12
       times 1 << 10 db 11
       times 1 <<  9 db 10
       times 1 <<  8 db  9
       times 1 <<  7 db  8
       times 1 <<  6 db  7
       times 1 <<  5 db  6
       times 1 <<  4 db  5
       times 1 <<  3 db  4
       times 1 <<  2 db  3
       times 1 <<  1 db  2
       times 1 <<  0 db  1
       times 1       db  0
jpeg_nbits_table:
       times 1       db  0
       times 1 <<  0 db  1
       times 1 <<  1 db  2
       times 1 <<  2 db  3
       times 1 <<  3 db  4
       times 1 <<  4 db  5
       times 1 <<  5 db  6
       times 1 <<  6 db  7
       times 1 <<  7 db  8
       times 1 <<  8 db  9
       times 1 <<  9 db 10
       times 1 << 10 db 11
       times 1 << 11 db 12
       times 1 << 12 db 13
       times 1 << 13 db 14
       times 1 << 14 db 15

    alignz      32

%define NBITS(x) nbits_base + x
%define MASK_BITS(x) NBITS((x) * 4) + (jpeg_mask_bits - jpeg_nbits_table)

; --------------------------------------------------------------------------
    SECTION     SEG_TEXT
    BITS        64

; Insert %%code into put_buffer, flush put_buffer and put the overflowing
; bits of %%code into the now empty put_buffer.
; uses:
; %%code - contains the bits to shift into put_buffer, LSB aligned
; %%jump_to - the label to jump to when leaving the function
; put_buffer - (partially) filled buffer that will be full after inserting %%code
; free_bits - the number of free bits in put_buffer after inserting %%code
;             directly, is either 0 or negative
; clobbers temp and %%code

%macro EMIT_QWORD 1-2
%define %%jump_to    %1
%define %%extra_insn %2
    add         nbitsb, free_bitsb      ; nbits += free_bits
    neg         free_bitsb              ; free_bits = -free_bits
    mov         tempd, code           ; temp = code;
    shl         put_buffer, nbitsb      ; put_buffer <<= nbits
    mov         nbitsb, free_bitsb      ; nbits = free_bits
    neg         free_bitsb              ; free_bits = -free_bits;
    shr         tempd, nbitsb           ; temp >>= nbits
    or          tempq, put_buffer       ; temp |= put_buffer;
    movq        xmm0, tempq             ; xmm0.u64 = { 0, temp };
    bswap       tempq                   ; temp = htonl(temp);
    mov         put_buffer, codeq     ; put_buffer = code
    pcmpeqb     xmm0, xmm1              ; i = 0...15: xmm0.u8[i] = xmm0.u8[i] == 0xff ? 0xff : 0...;
    %%extra_insn
    pmovmskb    code, xmm0              ; i = 0...15: code = (((xmm0.u8[i] >> 7) << i) | ...);
    mov         qword [buffer], tempq   ; memcpy(buffer, &temp, 8);
    add         free_bitsb, 64          ; free_bits += 64;
    add         bufferp, 8              ; buffer += 8;
    test        code, code              ; if (code == 0)
    jz          %%jump_to               ;     return;
    cmp         tempb, 0xff             ; temp[0] - 0xff ?
    mov         byte [buffer-7], 0      ; buffer[-7] = 0;
    sbb         bufferp, 6              ; buffer -= 6 + (temp[0] - 0xff < 0);
    mov         byte [buffer], temph    ; buffer[0] = temp[1];
    cmp         temph, 0xff             ; temp[1] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    shr         tempq, 16               ; temp >>= 16;
    mov         byte [buffer], tempb    ; buffer[0] = temp[0];
    cmp         tempb, 0xff             ; temp[0] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    mov         byte [buffer], temph    ; buffer[0] = temp[1];
    cmp         temph, 0xff             ; temp[1] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    shr         tempq, 16               ; temp >>= 16;
    mov         byte [buffer], tempb    ; buffer[0] = temp[0];
    cmp         tempb, 0xff             ; temp[0] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    mov         byte [buffer], temph    ; buffer[0] = temp[1];
    cmp         temph, 0xff             ; temp[1] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    shr         tempd, 16               ; temp >>= 16;
    mov         byte [buffer], tempb    ; buffer[0] = temp[0];
    cmp         tempb, 0xff             ; temp[0] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    mov         byte [buffer], temph    ; buffer[0] = temp[1];
    cmp         temph, 0xff             ; temp[1] - 0xff ?
    mov         byte [buffer+1], 0      ; buffer[1] = 0;
    sbb         bufferp, -2             ; buffer -= -2 + (temp[1] - 0xff < 0);
    jmp         %%jump_to               ; return;
%endmacro

;
; Encode a single block's worth of coefficients.
;
; GLOBAL(JOCTET *)
; jsimd_huff_encode_one_block_sse2(working_state *state, JOCTET *buffer,
;                                  JCOEFPTR block, int last_dc_val,
;                                  c_derived_tbl *dctbl, c_derived_tbl *actbl)
;

    align       32
    GLOBAL_FUNCTION(jsimd_huff_encode_one_block_sse2)

; Notes:
; When shuffling data we try to avoid pinsrw as much as possible as it is slow on many processors.
; Its reciprocal throughput (issue latency) is 1 even on modern cpus, so chains
; even with different outputs can limit performance. pinsrw is a vectorpath instruction
; on amd K8 and requires 2 µops (with mem operand) on intel. In either case only one pinsrw
; instruction can be decoded per cycle (and nothing else if they are back to back) so out-of-order
; execution cannot be used to work around long pinsrw chains, though for Sandy Bridge and later
; this may be less of a problem if the code runs out of the µop cache.
;
; We use tzcnt instead of bsf without checking for support, the instruction is executed
; as bsf on processors that don't support tzcnt (encoding is equivalent to rep bsf).
; bsf (and tzcnt on some processors) carries an input dependency on its first operand
; (although formally undefinied, intel processors usually leave the destination
; unmodified, if source is zero). This can prevent out-of-order execution, therefore we
; clear dst before tzcnt.

; initial register allocation
; rax - buffer
; rbx - temp
; rcx - last_dc_val(unix)  -> nbits
; rdx - block              -> free_bits
; rsi - nbits_base
; rdi - t
; rbp - code
; r8  - dctbl              -> code_temp
; r9  - actbl
; r10 - state
; r11 - index
; r12 - put_buffer


%define tempq           rbx
%define tempd           ebx
%define temph           bh
%define tempb           bl
%define nbits           ecx
%define nbitsq          rcx
%define nbitsb          cl
%define nbits_base      rsi
%define t               rdi
%define td              edi
%define code            ebp
%define codeq           rbp
%define index           r11
%define indexd          r11d
%define put_buffer      r12
%define put_bufferd     r12d

; step 1: arrange input data according to jpeg_natural_order
;  0                       d3 d2 b6 b5 a5 a4 a0 xx a b   d             a7 a6 a5 a4 a3 a2 a1 a0
;  8                    f1 d4 d1 b7 b4 a6 a3 a1    a b   d   f         b7 b6 b5 b4 b3 b2 b1 b0
; 16                 f2 f0 d5 d0 c0 b3 a7 a2       a b c d   f         c7 c6 c5 c4 c3 c2 c1 c0
; 24              g4 f3 e7 d6 c7 c1 b2 b0            b c d e f g   ==> d7 d6 d5 d4 d3 d2 d1 d0
; 32           g5 g3 f4 e6 d7 c6 c2 b1               b c d e f g       e7 e6 e5 e4 e3 e2 e1 e0
; 40        h3 g6 g2 f5 e5 e0 c5 c3                    c   e f g h     f7 f6 f5 f4 f3 f2 f1 f0
; 48     h4 h2 g7 g1 f6 e4 e1 c4                       c   e f g h     g7 g6 g5 g4 g3 g2 g1 g0
; 56  h6 h5 h1 h0 g0 f7 e3 e2                              e f g h     00 h6 h5 h4 h3 h2 h1 h0
                                                        ; i: 0..7
EXTN(jsimd_huff_encode_one_block_sse2):
%ifdef WIN64
%define state         rcx
%define buffer        rdx
%define block         r8
%define last_dc_val   r9d
    mov         rax, buffer
%define bufferp       rax
%define buffer        rax
    mov         rdx, block
%define block         rdx
    movups      xmm3, XMMWORD [block +  0 * SIZEOF_WORD] ; w3: d3 d2 b6 b5 a5 a4 a0 xx
    push        rbx
    push        rbp
    movdqa      xmm0, xmm3                               ; w0: d3 d2 b6 b5 a5 a4 a0 xx
    push        rsi
    push        rdi
    push        r12
    movups      xmm1, XMMWORD [block +  8 * SIZEOF_WORD] ; w1: f1 d4 d1 b7 b4 a6 a3 a1
%define dctbl         POINTER [rsp+6*8+4*8]
%define actbl         POINTER [rsp+6*8+5*8]
    mov         r10, state
%define state         r10
    movsx       code, word [block]                                                     ; code = block[0];
    pxor        xmm4, xmm4
    sub         code, last_dc_val                                                      ; code -= last_dc_val;
%undef last_dc_val
    mov         r8, dctbl
%define dctbl         r8
    mov         r9, actbl
    punpckldq   xmm0, xmm1                               ; w0: b4 a6 a5 a4 a3 a1 a0 xx
%define actbl         r9
    lea         nbits_base, [rel jpeg_nbits_table]
    add         rsp, -DCTSIZE2 * SIZEOF_WORD
    mov         t, rsp
%else
%define state         rdi
%define buffer        rsi
%define block         rdx
%define last_dc_val   rcx
%define dctbl         r8
%define actbl         r9
    movups      xmm3, XMMWORD [block +  0 * SIZEOF_WORD] ; w3: d3 d2 b6 b5 a5 a4 a0 xx
    push        rbx
    push        rbp
    movdqa      xmm0, xmm3                               ; w0: d3 d2 b6 b5 a5 a4 a0 xx
    push        r12
    mov         r10, state
%define state         r10
    mov         rax, buffer
%define bufferp       raxp
%define buffer        rax
    movups      xmm1, XMMWORD [block +  8 * SIZEOF_WORD] ; w1: f1 d4 d1 b7 b4 a6 a3 a1
    movsx       codeq, word [block]                                                    ; code = block[0];
    lea         nbits_base, [rel jpeg_nbits_table]
    pxor        xmm4, xmm4                               ; w4[i] = 0
    sub         codeq, last_dc_val                                                     ; code -= last_dc_val;
%undef last_dc_val
    punpckldq   xmm0, xmm1                               ; w0: b4 a6 a5 a4 a3 a1 a0 xx
    lea         t, [rsp - DCTSIZE2 * SIZEOF_WORD] ; use red zone for t_
%endif

    pshuflw     xmm0, xmm0, 11001001b                    ; w0: b4 a6 a5 a4 a3 xx a1 a0
    pinsrw      xmm0, word [block + 16 * SIZEOF_WORD], 2 ; w0: b4 a6 a5 a4 a3 a2 a1 a0
    punpckhdq   xmm3, xmm1                               ; w3: f1 d4 d3 d2 d1 b7 b6 b5
    punpcklqdq  xmm1, xmm3                               ; w1: d1 b7 b6 b5 b4 a6 a3 a1
    pinsrw      xmm0, word [block + 17 * SIZEOF_WORD], 7 ; w0: a7 a6 a5 a4 a3 a2 a1 a0
    pcmpgtw     xmm4, xmm0                               ; w4[i] = -(w4[i] > w0[i])
    paddw       xmm0, xmm4                               ; w0[i] += w4[i]
    movaps      XMMWORD [t + 0  * SIZEOF_WORD], xmm0     ; t[i] = w0[i]
    movq        xmm2, qword [block + 24 * SIZEOF_WORD]   ; w2: 00 00 00 00 c7 c1 b2 b0
    pshuflw     xmm2, xmm2, 11011000b                    ; w2: 00 00 00 00 c7 b2 c1 b0
    pslldq      xmm1, 1 * SIZEOF_WORD                    ; w1: b7 b6 b5 b4 a6 a3 a1 00
    movups      xmm5, XMMWORD [block + 48 * SIZEOF_WORD] ; w5: h4 h2 g7 g1 f6 e4 e1 c4
    movsd       xmm1, xmm2                               ; w1: b7 b6 b5 b4 c7 b2 c1 b0
    punpcklqdq  xmm2, xmm5                               ; w2: f6 e4 e1 c4 c7 b2 c1 b0
    pinsrw      xmm1, word [block + 32 * SIZEOF_WORD], 1 ; w1: b7 b6 b5 b4 c7 b2 b1 b0
    pxor        xmm4, xmm4                               ; w4[i] = 0
    psrldq      xmm3, 2 * SIZEOF_WORD                    ; w3: 00 00 f1 d4 d3 d2 d1 b7
    pcmpeqw     xmm0, xmm4                               ; w0[i] = -(w0[i] == w4[i])
    pinsrw      xmm1, word [block + 18 * SIZEOF_WORD], 3 ; w1: b7 b6 b5 b4 b3 b2 b1 b0
    pcmpgtw     xmm4, xmm1                               ; w4[i] = -(w4[i] > w1[i])
    paddw       xmm1, xmm4                               ; w1[i] += w4[i]
    movaps      XMMWORD [t + 8  * SIZEOF_WORD], xmm1     ; t[i+8] = w1[i]
    pxor        xmm4, xmm4                               ; w4[i] = 0
    pcmpeqw     xmm1, xmm4                               ; w1[i] = -(w1[i] == w4[i])
    packsswb    xmm0, xmm1                               ; b0[i] = w0[i], b0[i+8] = w1[i], sign saturated
    pinsrw      xmm3, word [block + 20 * SIZEOF_WORD], 0 ; w3: 00 00 f1 d4 d3 d2 d1 d0
    pinsrw      xmm3, word [block + 21 * SIZEOF_WORD], 5 ; w3: 00 00 d5 d4 d3 d2 d1 d0
    pinsrw      xmm3, word [block + 28 * SIZEOF_WORD], 6 ; w3: 00 f6 d5 d4 d3 d2 d1 d0
    pinsrw      xmm3, word [block + 35 * SIZEOF_WORD], 7 ; w3: d7 d6 d5 d4 d3 d2 d1 d0
    pcmpgtw     xmm4, xmm3                               ; w4[i] = -(w4[i] > w3[i])
    paddw       xmm3, xmm4                               ; w3[i] += w4[i]
    movaps      XMMWORD [t + 24 * SIZEOF_WORD], xmm3     ; t[i+24] = w3[i]
    pxor        xmm4, xmm4                               ; w4[i] = 0
    pcmpeqw     xmm3, xmm4                               ; w3[i] = -(w3[i] == w4[i])
    pinsrw      xmm2, word [block + 19 * SIZEOF_WORD], 0 ; w2: f6 e4 e1 c4 c7 b2 c1 c0
    cmp         code, 1 << 31                                                          ; code - (1u << 31) ?
    pinsrw      xmm2, word [block + 33 * SIZEOF_WORD], 2 ; w2: f6 e4 e1 c4 c7 c2 c1 c0
    pinsrw      xmm2, word [block + 40 * SIZEOF_WORD], 3 ; w2: f6 e4 e1 c4 c3 c2 c1 c0
    adc         code, -1                                                               ; code += -1 + (code < (1u << 31))
    pinsrw      xmm2, word [block + 41 * SIZEOF_WORD], 5 ; w2: f6 e4 c5 c4 c3 c2 c1 c0
    pinsrw      xmm2, word [block + 34 * SIZEOF_WORD], 6 ; w2: f6 c6 c5 c4 c3 c2 c1 c0
    movsxd      codeq, code                                                            ; sign extend
    pinsrw      xmm2, word [block + 27 * SIZEOF_WORD], 7 ; w2: c7 c6 c5 c4 c3 c2 c1 c0
    pcmpgtw     xmm4, xmm2                               ; w4[i] = -(w4[i] > w2[i])
    paddw       xmm2, xmm4                               ; w2[i] += w4[i]
    movaps      XMMWORD [t + 16 * SIZEOF_WORD], xmm2     ; t[i+16] = w2[i]
    pxor        xmm4, xmm4                               ; w4[i] = 0
    pcmpeqw     xmm2, xmm4                               ; w2[i] = -(w2[i] == w4[i])
    packsswb    xmm2, xmm3                               ; b2[i] = w2[i], b2[i+8] = w3[i], sign saturated
    movzx       nbitsq, byte [NBITS(codeq)]                                            ; nbits = JPEG_NBITS(code);
    movdqa      xmm3, xmm5                               ; w3: h4 h2 g7 g1 f6 e4 e1 c4
    pmovmskb    tempd, xmm2                                                            ; temp = (((b2[i] >> 7) << i) | ...);
    pmovmskb    put_bufferd, xmm0                                                      ; put_buffer = (((b0[i] >> 7) << i) | ...);
    movups      xmm0, XMMWORD [block + 56 * SIZEOF_WORD] ; w0: h6 h5 h1 h0 g0 f7 e3 e2
    punpckhdq   xmm3, xmm0                               ; w3: h6 h5 h4 h2 h1 h0 g7 g1
    shl         tempd, 16                                                              ; temp <<= 16;
    psrldq      xmm3, 1 * SIZEOF_WORD                    ; w3: 00 h6 h5 h4 h2 h1 h0 g7
    pxor        xmm2, xmm2                               ; w2[i] = 0
    or          put_bufferd, tempd                                                     ; put_buffer |= temp
    pshuflw     xmm3, xmm3, 00111001b                    ; w3: 00 h6 h5 h4 g7 h2 h1 h0
    movq        xmm1, qword [block + 44 * SIZEOF_WORD]   ; w1: 00 00 00 00 h3 g6 g2 f5
    unpcklps    xmm5, xmm0                               ; w5: g0 f7 f6 e4 e3 e2 e1 c4
    pxor        xmm0, xmm0                               ; w0[i] = 0
    pinsrw      xmm3, word [block + 47 * SIZEOF_WORD], 3 ; w3: 00 h6 h5 h4 h3 h2 h1 h0
    pcmpgtw     xmm2, xmm3                               ; w2[i] = -(w2[i] > w3[i])
    paddw       xmm3, xmm2                               ; w3[i] += w2[i]
    movaps      XMMWORD [t + 56 * SIZEOF_WORD], xmm3     ; t[i+56] = w3[i]
    movq        xmm4, qword [block + 36 * SIZEOF_WORD]   ; w4: 00 00 00 00 g5 g3 f4 e6
    pcmpeqw     xmm3, xmm0                               ; w3[i] = -(w3[i] == w[0])
    punpckldq   xmm4, xmm1                               ; w4: h3 g6 g5 g3 g2 f5 f4 e6
    mov         tempd, [dctbl + c_derived_tbl.ehufco + nbitsq * 4]                     ; temp = dctbl->ehufco[nbits];
    movdqa      xmm1, xmm4                               ; w1: h3 g6 g5 g3 g2 f5 f4 e6
    psrldq      xmm4, 1 * SIZEOF_WORD                    ; w4: 00 h3 g6 g5 g3 g2 f5 f4
    shufpd      xmm1, xmm5, 10b                          ; w1: g0 f7 f6 e4 g2 f5 f4 e6
    and         code, dword [MASK_BITS(nbitsq)]                                        ; code &= (1 << nbits) - 1;
    pshufhw     xmm4, xmm4, 11010011b                    ; w4: 00 g6 g5 00 g3 g2 f5 f4
    pslldq      xmm1, 1 * SIZEOF_WORD                    ; w1: f7 f6 e4 g2 f5 f4 e6 00
    shl         tempq, nbitsb                                                          ; temp <<= nbits;
    pinsrw      xmm4, word [block + 59 * SIZEOF_WORD], 0 ; w4: 00 g6 g5 00 g3 g2 f5 g0
    pshufd      xmm1, xmm1, 11011000b                    ; w1: f7 f6 f5 f4 e4 g2 e6 00
    pinsrw      xmm4, word [block + 52 * SIZEOF_WORD], 1 ; w4: 00 g6 g5 00 g3 g2 g1 g0
    or          code, tempd                                                            ; code |= temp;
    movlps      xmm1, qword [block + 20 * SIZEOF_WORD]   ; w1: f7 f6 f5 f4 f2 f0 d5 d0
    pinsrw      xmm4, word [block + 31 * SIZEOF_WORD], 4 ; w4: 00 g6 g5 g4 g3 g2 g1 g0
    pshuflw     xmm1, xmm1, 01110010b                    ; w1: f7 f6 f5 f4 d5 f2 d0 f0
    pinsrw      xmm4, word [block + 53 * SIZEOF_WORD], 7 ; w4: g7 g6 g5 g4 g3 g2 g1 g0
    pxor        xmm2, xmm2                               ; w2[i] = 0
    pcmpgtw     xmm0, xmm4                               ; w0[i] = -(w0[i] < w4[i])
    pinsrw      xmm1, word [block + 15 * SIZEOF_WORD], 1 ; w1: f7 f6 f5 f4 d5 f2 f1 f0
    paddw       xmm4, xmm0                               ; w4[i] += w0[i]
    movaps      XMMWORD [t + 48 * SIZEOF_WORD], xmm4     ; t[48+i] = w4[i]
    pinsrw      xmm1, word [block + 30 * SIZEOF_WORD], 3 ; w1: f7 f6 f5 f4 f3 f2 f1 f0
    pcmpeqw     xmm4, xmm2                               ; w4[i] = -(w4[i] == w2[i])
    pinsrw      xmm5, word [block + 42 * SIZEOF_WORD], 0 ; w5: g0 f7 f6 e4 e3 e2 e1 e0
    packsswb    xmm4, xmm3                               ; b4[i] = w4[i], b4[i+8] = w3[i], sign saturated
    pxor        xmm0, xmm0                               ; w0[i] = 0
    pinsrw      xmm5, word [block + 43 * SIZEOF_WORD], 5 ; w5: g0 f7 e5 e4 e3 e2 e1 e0
    pcmpgtw     xmm2, xmm1                               ; w2[i] = -(w2[i] < w1[i])
    pmovmskb    tempd, xmm4                                                            ; temp = (((b4[i] >> 7) << i) | ...);
    pinsrw      xmm5, word [block + 36 * SIZEOF_WORD], 6 ; w5: g0 e6 e5 e4 e3 e2 e1 e0
    paddw       xmm1, xmm2                               ; w1[i] += w2[i]
    movaps      XMMWORD [t + 40 * SIZEOF_WORD], xmm1     ; t[40+i] = w1[i]
    pinsrw      xmm5, word [block + 29 * SIZEOF_WORD], 7 ; w5: e7 e6 e5 e4 e3 e2 e1 e0
%undef block
%define free_bitsb      dl
%define free_bitsd      edx
%define free_bitsq      rdx
    pcmpeqw     xmm1, xmm0                               ; w1[i] = -(w1[i] == w0[i])
    shl         tempq, 48                                                              ; temp <<= 48;
    pxor        xmm2, xmm2                               ; w2[i] = 0
    pcmpgtw     xmm0, xmm5                               ; w5[i] = -(w5[i] < w0[i])
    paddw       xmm5, xmm0                               ; w5[i] += w0[i]
    or          tempq, put_buffer                                                      ; temp |= put_buffer;
    movaps      XMMWORD [t + 32 * SIZEOF_WORD], xmm5     ; t[32+i] = w5[i]
    lea         t, [dword t - 2]                                                       ; t = &t[-1];
    pcmpeqw     xmm5, xmm2                               ; w5[i] = -(w5[i] == w2[i])
    packsswb    xmm5, xmm1                               ; b5[i] = w5[i], b5[i+8] = w1[i], sign saturated
    add         nbitsb, byte [dctbl + c_derived_tbl.ehufsi + nbitsq]                   ; nbits += dctbl->ehufsi[nbits];
%undef dctbl
%define code_temp r8d
    pmovmskb    indexd, xmm5                                                           ; index = (((b5[i] >> 7) << i) | ...);
    mov         free_bitsd, [state+working_state.cur.free_bits]                        ; free_bits = state->cur.free_bits;
    pcmpeqw     xmm1, xmm1                                ; b1[i] = 0xff
    shl         index, 32                                                              ; index <<= 32;
    mov         put_buffer, [state+working_state.cur.put.buffer_simd]                  ; put_buffer = state->cur.put.buffer_simd;
    or          index, tempq                                                           ; index |= temp;
    not         index                                                                  ; index = ~index;
    sub         free_bitsb, nbitsb                                                     ; if ((free_bits -= nbits) >= 0)
    jnl         .ENTRY_SKIP_EMIT_CODE                                                  ;     goto .ENTRY_SKIP_EMIT_CODE;
    align       16
.EMIT_CODE:                                                                            ; .EMIT_CODE:
    EMIT_QWORD        .BLOOP_COND                                                      ;     insert code, flush buffer, goto .BLOOP_COND
;-------------------------------------------
    align       16
.BRLOOP:                                                                    ; .BRLOOP: do {
    lea         code_temp, [nbitsq - 16]                                    ;     code_temp = nbits - 16;
    movzx       nbits, byte [actbl + c_derived_tbl.ehufsi + 0xf0]           ;     nbits -= actbl->ehufsi[0xf0];
    mov         code, [actbl + c_derived_tbl.ehufco + 0xf0 * 4]             ;     code = actbl->ehufco[0xf0];
    sub         free_bitsb, nbitsb                                          ;     if ((free_bits - nbits) <= 0)
    jle         .EMIT_BRLOOP_CODE                                           ;         goto .EMIT_BRLOOP_CODE
    shl         put_buffer, nbitsb                                          ;     put_buffer <<= nbits;
    mov         nbits, code_temp                                            ;     nbits = code_temp;
    or          put_buffer, codeq                                           ;     put_buffer |= code;
    cmp         nbits, 16                                                   ;     if (nbits <= 16)
    jle         .ERLOOP                                                     ;         goto .ERLOOP;
    jmp         .BRLOOP                                                     ; } while(1);
;-------------------------------------------
    align       16
    times 5 nop
.ENTRY_SKIP_EMIT_CODE:                                                                 ; .ENTRY_SKIP_EMIT_CODE:
    shl         put_buffer, nbitsb                                                     ;     put_buffer <<= nbits;
    or          put_buffer, codeq                                                      ;     put_buffer |= code;
.BLOOP_COND:                                                                           ; .BLOOP_COND:
    test        index, index                                                           ;     index & index ?
    jz          .ELOOP                                                                 ;     if (index == 0) goto .ELOOP
.BLOOP:                                                                                ; do {
    xor         nbits, nbits ; kill tzcnt input dependency                             ;     nbits = 0;
    tzcnt       nbitsq, index                                                          ;     nbits = #trailing 0-bits in index
    inc         nbits                                                                  ;     ++nbits;
    lea         t, [t + nbitsq * 2]                                                    ;     t = &t[nbits];
    shr         index, nbitsb                                                          ;     index >>= nbits;
.EMIT_BRLOOP_CODE_END:                                                                 ; .EMIT_BRLOOP_CODE_END:
    cmp         nbits, 16                                                              ;     if (nbits > 16)
    jg          .BRLOOP                                                                ;         goto .BRLOOP;
.ERLOOP:                                                                               ; .ERLOOP:
    movsx       codeq, word [t]                                                        ;     code = *t;
    lea         tempd, [nbitsq * 2]                                                    ;     temp = nbits * 2;
    movzx       nbits, byte [NBITS(codeq)]                                             ;     nbits = JPEG_NBITS(code);
    lea         tempd, [nbitsq + tempq * 8]                                            ;     temp = temp * 8 + nbits;
    mov         code_temp, [actbl + c_derived_tbl.ehufco + (tempq - 16) * 4]           ;     code_temp = actbl->ehufco[temp-16];
    shl         code_temp, nbitsb                                                      ;     code_temp <<= nbits;
    and         code, dword [MASK_BITS(nbitsq)]                                        ;     code &= (1 << nbits) - 1;
    add         nbitsb, [actbl + c_derived_tbl.ehufsi + (tempq - 16)]                  ;     free_bits -= actbl->ehufsi[temp-16];
    or          code, code_temp                                                        ;     code |= code_temp;
    sub         free_bitsb, nbitsb                                                     ;     if ((free_bits -= nbits) <= 0)
    jle         .EMIT_CODE                                                             ;         goto .EMIT_CODE
    shl         put_buffer, nbitsb                                                     ;     put_buffer <<= nbits;
    or          put_buffer, codeq                                                      ;     put_buffer |= code;
    test        index, index                                                           ;     index & index ?
    jnz         .BLOOP                                                                 ; } while (index != 0);
.ELOOP:                                                                                ; .ELOOP:
    sub         td, esp                                                                ; t -= (WIN64: &t_[0]  UNIX: &t_[64]);
%ifdef WIN64
    cmp         td, (DCTSIZE2 - 2) * SIZEOF_WORD                                       ; if (t == 62)
%else
    cmp         td, -2 * SIZEOF_WORD                                                   ; if (t == -2)
%endif
    je          .EFN                                                                   ;     goto .EFN;
    movzx       nbits, byte [actbl + c_derived_tbl.ehufsi + 0]                         ; nbits = actbl->ehufsi[0];
    mov         code, [actbl + c_derived_tbl.ehufco + 0]                               ; code = actbl->ehufco[0];
    sub         free_bitsb, nbitsb                                                     ; if ((free_bits -= nbits) > 0)
    jg          .EFN_SKIP_EMIT_CODE                                                    ;     goto .EFN_SKIP_EMIT_CODE
    EMIT_QWORD  .EFN                                                                   ; insert code, flush put_buffer, goto .EFN
    align       16
.EFN_SKIP_EMIT_CODE:                                                                   ; .EFN_SKIP_EMIT_CODE:
    shl         put_buffer, nbitsb                                                     ;     put_buffer <<= nbits;
    or          put_buffer, codeq                                                      ;     put_buffer |= code;
.EFN:                                                                                  ; .EFN:
    mov         [state + working_state.cur.put.buffer_simd], put_buffer                ; state->cur.put.buffer_simd = put_buffer;
    mov         byte [state + working_state.cur.free_bits], free_bitsb                 ; state->cur.free_bits = free_bits;
%ifdef WIN64
    sub         rsp, -DCTSIZE2 * SIZEOF_WORD
    pop         r12
    pop         rdi
    pop         rsi
    pop         rbp
    pop         rbx
%else
    pop         r12
    pop         rbp
    pop         rbx
%endif
    ret

    align       16
.EMIT_BRLOOP_CODE:                                                                     ; insert code, flush buffer,
    EMIT_QWORD        .EMIT_BRLOOP_CODE_END, {mov nbits, code_temp}                    ; nbits = code_temp, goto .EMIT_BRLOOP_CODE_END

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
