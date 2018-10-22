;
; jchuff-sse2.asm - Huffman entropy encoding (SSE2)
;
; Copyright (C) 2009-2011, 2014-2017, D. R. Commander.
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

    GLOBAL_DATA(jconst_huff_encode_one_block)
EXTN(jconst_huff_encode_one_block):

    alignz      32

jpeg_mask_bits  dq 0x0000, 0x0001, 0x0003, 0x0007
                dq 0x000f, 0x001f, 0x003f, 0x007f
                dq 0x00ff, 0x01ff, 0x03ff, 0x07ff
                dq 0x0fff, 0x1fff, 0x3fff, 0x7fff

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

%ifdef PIC
%define NBITS_(x) nbits_base + x
%else
%define NBITS_(x) jpeg_nbits_table + x
%endif
%define MASK_BITS(x) NBITS_((x) * 8) + (jpeg_mask_bits - jpeg_nbits_table)

; --------------------------------------------------------------------------
    SECTION     SEG_TEXT
    BITS        32

%define put_buffer     mm0
%define all_0xff       mm1
%define mmtemp         mm2
%define code_bits      mm3
%define code           mm4
%define overflow_bits  mm5
%define save_nbits     mm6
%define save_buffer    mm7

; Insert %%code into put_buffer, flush put_buffer and put the overflowing
; bits of %%code into the now empty put_buffer.
%macro EMIT_QWORD 9 ; temp, temph, tempb, insn1, insn2, insn3, insn4, insn5, insn6
%define %%temp     %1
%define %%temph    %2
%define %%tempb    %3
%define %%insn1    %4
%define %%insn2    %5
%define %%insn3    %6
%define %%insn4    %7
%define %%insn5    %8
%define %%insn6    %9
    add         nbits, free_bits            ; nbits += free_bits
    neg         free_bits                   ; free_bits = -free_bits
    movq        mmtemp, code                ; mmtemp = code
    movd        code_bits, nbits            ; code_bits = nbits
    movd        overflow_bits, free_bits    ; (b) #bits in put_buffer after flush
    neg         free_bits                   ; free_bits = -free_bits
    psllq       put_buffer, code_bits       ; put_buffer <<= code_bits
    psrlq       mmtemp, overflow_bits       ; mmtemp >>= (b)
    add         free_bits, 64               ; free_bits += 64
    por         mmtemp, put_buffer          ; (c) code' |= put_buffer
%ifidn %%temp, nbits_base
    movd        save_nbits, nbits_base      ; save nbits_base
%endif
    movq        code_bits, mmtemp           ; c'' = mmtemp
    movq        put_buffer, code            ; put_buffer = code
    pcmpeqb     mmtemp, all_0xff            ; i = 0..7: (mmtemp[i] = (mmtemp[i] == 0xff ? -1 : 0))...
    movq        code, code_bits             ; code = c''
    psrlq       code_bits, 32               ; mmtemp >>= 32
    pmovmskb    nbits, mmtemp               ; i = 0..7: size = ((sign(mmtemp[i]) << i) | ...)
    movd        %%temp, code_bits           ; temp = c''
    bswap       %%temp                      ; temp = htonl(temp)
    test        nbits, nbits                ; size == 0 => (no 0xff bytes to store)
    jnz         %%.SLOW                     ; jmp if != 0
    mov         dword [buffer], %%temp      ; *(uint32_t)buffer = temp
%ifidn %%temp, nbits_base
    movd        nbits_base, save_nbits      ; restore nbits_base
%endif
    %%insn1
    movd        nbits, code                 ; size = (uint32_t)(c)
    %%insn2
    bswap       nbits                       ; size = htonl(size)
    mov         dword [buffer + 4], nbits   ; *(uint32_t)(buffer + 4) = size
    lea         buffer, [buffer + 8]        ; buffer += 8;
    %%insn3
    %%insn4
    %%insn5
    %%insn6                                 ; return
%%.SLOW:
    mov         byte [buffer], %%tempb      ; buffer[0] = temp[0]
    cmp         %%tempb, 0xff               ; temp[0] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (temp[0] - 0xff < 0) (unsigned compare)
    mov         byte [buffer], %%temph      ; buffer[0] = temp[1]
    cmp         %%temph, 0xff               ; temp[1] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (temp[1] - 0xff < 0) (unsigned compare)
    shr         %%temp, 16                  ; temp >>= 16 (unsigned arithmetic)
    mov         byte [buffer], %%tempb      ; buffer[0] = temp[0]
    cmp         %%tempb, 0xff               ; temp[0] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (temp[0] - 0xff < 0) (unsigned compare)
    mov         byte [buffer], %%temph      ; buffer[0] = temp[1]
    cmp         %%temph, 0xff               ; temp[1] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (temp[1] - 0xff < 0) (unsigned compare)
    movd        nbits, code                 ; size = (uint32_t)(c)
%ifidn %%temp, nbits_base
    movd        nbits_base, save_nbits      ; restore nbits_base
%endif
    bswap       nbits                       ; size = htonl(size)
    mov         byte [buffer], nbitsb       ; buffer[0] = size[0]
    cmp         nbitsb, 0xff                ; size[0] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (size[0] - 0xff < 0) (unsigned compare)
    mov         byte [buffer], nbitsh       ; buffer[0] = size[1]
    cmp         nbitsh, 0xff                ; size[1] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (size[1] - 0xff < 0) (unsigned compare)
    shr         nbits, 16                   ; size >>= 16 (unsigned arithmetic)
    mov         byte [buffer], nbitsb       ; buffer[0] = size[0]
    cmp         nbitsb, 0xff                ; size[0] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (size[0] - 0xff < 0) (unsigned compare)
    mov         byte [buffer], nbitsh       ; buffer[0] = size[1]
    %%insn1
    cmp         nbitsh, 0xff                ; size[1] - 0xff ?
    mov         byte [buffer+1], 0          ; buffer[1] = 0
    sbb         buffer, -2                  ; buffer -= -2 + (size[1] - 0xff < 0) (unsigned compare)
    %%insn2
    %%insn3
    %%insn4
    %%insn5
    %%insn6                                 ; return
%endmacro

%macro PUSH 1
    push        %1
%ifidn frame, esp
%assign stack_offset stack_offset + 4
%endif
%endmacro

%macro POP 1
    pop         %1
%ifidn frame, esp
%assign stack_offset stack_offset - 4
%endif
%endmacro

; Macro to load the address of a symbol defined in this file into a register if PIC is defined.
; Equivalent to
; get_GOT %1
; lea %1, [GOTOFF(%1, %2)]
; without using the GOT.
; Optional arguments may be multi line macros to execute between call/ret/jmp.
; If PIC is not defined, only execute extra instructions.
%macro GET_SYM 2-4 ; reg, sym, insn1, insn2
%ifdef PIC
    call        %%.geteip
%%.ref:
    %4
    add         %1, %2 - %%.ref
    jmp         short %%.done
align 32
%%.geteip:
    %3          4  ; indicate stack pointer adjustment due to call
    mov         %1, POINTER [esp]
    ret
align 32
%%.done:
%else
    %3          0
    %4
%endif
%endmacro

;
; Encode a single block's worth of coefficients.
;
; GLOBAL(JOCTET *)
; jsimd_huff_encode_one_block_sse2(working_state *state, JOCTET *buffer,
;                                  JCOEFPTR block, int last_dc_val,
;                                  c_derived_tbl *dctbl, c_derived_tbl *actbl)
;
; stack layout:
;
; function args
; return address
; saved ebx
; saved ebp
; saved esi
; saved edi   <- esp_save
; ...
; esp_save
; t_ 64*2 bytes  aligned at 128 bytes
; esp is used (as t) to point into t_ (data in lower indexes
; is not used once esp passes over them, so this is signal safe).
; Realigning to 128 bytes allows us to find the rest of the data again.

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

%assign stack_offset     0
%define arg_actbl       24 + stack_offset
%define arg_dctbl       20 + stack_offset
%define arg_last_dc_val 16 + stack_offset
%define arg_block       12 + stack_offset
%define arg_buffer       8 + stack_offset
%define arg_state        4 + stack_offset

    align       32
    GLOBAL_FUNCTION(jsimd_huff_encode_one_block_sse2)

EXTN(jsimd_huff_encode_one_block_sse2):

; initial register allocation
; eax - frame -> buffer
; ebx - nbits_base (PIC) / emit_temp
; ecx - dctbl -> size
; edx - block -> nbits
; esi - code_temp -> actbl
; edi - index_temp -> free_bits
; ebp - index
; esp - t

; step 1: arrange input data according to jpeg_natural_order
;  0                       d3 d2 b6 b5 a5 a4 a0 xx a b   d             a7 a6 a5 a4 a3 a2 a1 a0
;  8                    f1 d4 d1 b7 b4 a6 a3 a1    a b   d   f         b7 b6 b5 b4 b3 b2 b1 b0
; 16                 f2 f0 d5 d0 c0 b3 a7 a2       a b c d   f         c7 c6 c5 c4 c3 c2 c1 c0
; 24              g4 f3 e7 d6 c7 c1 b2 b0            b c d e f g   ==> d7 d6 d5 d4 d3 d2 d1 d0
; 32           g5 g3 f4 e6 d7 c6 c2 b1               b c d e f g       e7 e6 e5 e4 e3 e2 e1 e0
; 40        h3 g6 g2 f5 e5 e0 c5 c3                    c   e f g h     f7 f6 f5 f4 f3 f2 f1 f0
; 48     h4 h2 g7 g1 f6 e4 e1 c4                       c   e f g h     g7 g6 g5 g4 g3 g2 g1 g0
; 56  h6 h5 h1 h0 g0 f7 e3 e2                              e f g h     00 h6 h5 h4 h3 h2 h1 h0

%ifdef PIC
%define nbits_base     ebx
%endif
%define emit_temp      ebx
%define emit_temph     bh
%define emit_tempb     bl
%define dctbl          ecx
%define block          edx
%define code_temp      esi
%define index_temp     edi
%define index          ebp
                                                         ; i: 0..7
%define frame   esp
    mov         block, [frame + arg_block]
    PUSH        ebx
    PUSH        ebp
    movups      xmm3, XMMWORD [block +  0 * SIZEOF_WORD] ; w3: d3 d2 b6 b5 a5 a4 a0 xx
    PUSH        esi
    PUSH        edi
    movdqa      xmm0, xmm3                               ; w0: d3 d2 b6 b5 a5 a4 a0 xx
    mov         eax, esp
%define frame   eax
%define t       esp
%assign save_frame     DCTSIZE2 * SIZEOF_WORD
    lea         t, [frame - (save_frame + 4)]
    movups      xmm1, XMMWORD [block +  8 * SIZEOF_WORD] ; w1: f1 d4 d1 b7 b4 a6 a3 a1
    and         t, -DCTSIZE2 * SIZEOF_WORD                                             ; t = &t_[0]
    mov         [t + save_frame], frame
    pxor        xmm4, xmm4                               ; w4[i] = 0
    punpckldq   xmm0, xmm1                               ; w0: b4 a6 a5 a4 a3 a1 a0 xx
    pshuflw     xmm0, xmm0, 11001001b                    ; w0: b4 a6 a5 a4 a3 xx a1 a0
    pinsrw      xmm0, word [block + 16 * SIZEOF_WORD], 2 ; w0: b4 a6 a5 a4 a3 a2 a1 a0
    punpckhdq   xmm3, xmm1                               ; w3: f1 d4 d3 d2 d1 b7 b6 b5
    punpcklqdq  xmm1, xmm3                               ; w1: d1 b7 b6 b5 b4 a6 a3 a1
    pinsrw      xmm0, word [block + 17 * SIZEOF_WORD], 7 ; w0: a7 a6 a5 a4 a3 a2 a1 a0
    pcmpgtw     xmm4, xmm0                               ; w4[i] = -(w4[i] > w0[i])
    paddw       xmm0, xmm4                               ; w0[i] += w4[i]
    movaps      XMMWORD [t + 0 * SIZEOF_WORD], xmm0      ; t[i] = w0[i]
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
    movaps      XMMWORD [t + 8 * SIZEOF_WORD], xmm1      ; t[i+8] = w1[i]
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
    pinsrw      xmm2, word [block + 33 * SIZEOF_WORD], 2 ; w2: f6 e4 e1 c4 c7 c2 c1 c0
    pinsrw      xmm2, word [block + 40 * SIZEOF_WORD], 3 ; w2: f6 e4 e1 c4 c3 c2 c1 c0
    pinsrw      xmm2, word [block + 41 * SIZEOF_WORD], 5 ; w2: f6 e4 c5 c4 c3 c2 c1 c0
    pinsrw      xmm2, word [block + 34 * SIZEOF_WORD], 6 ; w2: f6 c6 c5 c4 c3 c2 c1 c0
    pinsrw      xmm2, word [block + 27 * SIZEOF_WORD], 7 ; w2: c7 c6 c5 c4 c3 c2 c1 c0
    pcmpgtw     xmm4, xmm2                               ; w4[i] = -(w4[i] > w2[i])
    paddw       xmm2, xmm4                               ; w2[i] += w4[i]
    movsx       code_temp, word [block]                                                ; code_temp = block[0];
%macro GET_SYM_INSN1 1
    movaps      XMMWORD [t + 16 * SIZEOF_WORD + %1], xmm2; t[i+16] = w2[i]
    pxor        xmm4, xmm4                               ; w4[i] = 0
    pcmpeqw     xmm2, xmm4                               ; w2[i] = -(w2[i] == w4[i])
    sub         code_temp, [frame + arg_last_dc_val]                                   ; code_temp -= last_dc_val;
    packsswb    xmm2, xmm3                               ; b2[i] = w2[i], b2[i+8] = w3[i], sign saturated
    movdqa      xmm3, xmm5                               ; w3: h4 h2 g7 g1 f6 e4 e1 c4
    pmovmskb    index_temp, xmm2                                                       ; index_temp = (((b2[i] >> 7) << i) | ...);
    pmovmskb    index, xmm0                                                            ; index = (((b0[i] >> 7) << i) | ...);
    movups      xmm0, XMMWORD [block + 56 * SIZEOF_WORD] ; w0: h6 h5 h1 h0 g0 f7 e3 e2
    punpckhdq   xmm3, xmm0                               ; w3: h6 h5 h4 h2 h1 h0 g7 g1
    shl         index_temp, 16                                                         ; index_temp <<= 16;
    psrldq      xmm3, 1 * SIZEOF_WORD                    ; w3: 00 h6 h5 h4 h2 h1 h0 g7
    pxor        xmm2, xmm2                               ; w2[i] = 0
    pshuflw     xmm3, xmm3, 00111001b                    ; w3: 00 h6 h5 h4 g7 h2 h1 h0
    or          index, index_temp                                                      ; index |= index_temp
; index_temp -> free_bits
%xdefine free_bits index_temp
%undef index_temp
%endmacro

%macro GET_SYM_INSN2 0
    movq        xmm1, qword [block + 44 * SIZEOF_WORD]   ; w1: 00 00 00 00 h3 g6 g2 f5
    unpcklps    xmm5, xmm0                               ; w5: g0 f7 f6 e4 e3 e2 e1 c4
    pxor        xmm0, xmm0                               ; w0[i] = 0
    not         index                                                                  ; index = ~index;
    pinsrw      xmm3, word [block + 47 * SIZEOF_WORD], 3 ; w3: 00 h6 h5 h4 h3 h2 h1 h0
    pcmpgtw     xmm2, xmm3                               ; w2[i] = -(w2[i] > w3[i])
    mov         dctbl, [frame + arg_dctbl]
    paddw       xmm3, xmm2                               ; w3[i] += w2[i]
    movaps      XMMWORD [t + 56 * SIZEOF_WORD], xmm3     ; t[i+56] = w3[i]
    movq        xmm4, qword [block + 36 * SIZEOF_WORD]   ; w4: 00 00 00 00 g5 g3 f4 e6
    pcmpeqw     xmm3, xmm0                               ; w3[i] = -(w3[i] == w[0])
    punpckldq   xmm4, xmm1                               ; w4: h3 g6 g5 g3 g2 f5 f4 e6
    movdqa      xmm1, xmm4                               ; w1: h3 g6 g5 g3 g2 f5 f4 e6
    pcmpeqw     all_0xff, all_0xff                       ; all_0xff[i] = 0xff
%endmacro

    GET_SYM     nbits_base, jpeg_nbits_table, GET_SYM_INSN1, GET_SYM_INSN2

    psrldq      xmm4, 1 * SIZEOF_WORD                    ; w4: 00 h3 g6 g5 g3 g2 f5 f4
    shufpd      xmm1, xmm5, 10b                          ; w1: g0 f7 f6 e4 g2 f5 f4 e6
    pshufhw     xmm4, xmm4, 11010011b                    ; w4: 00 g6 g5 00 g3 g2 f5 f4
    pslldq      xmm1, 1 * SIZEOF_WORD                    ; w1: f7 f6 e4 g2 f5 f4 e6 00
    pinsrw      xmm4, word [block + 59 * SIZEOF_WORD], 0 ; w4: 00 g6 g5 00 g3 g2 f5 g0
    pshufd      xmm1, xmm1, 11011000b                    ; w1: f7 f6 f5 f4 e4 g2 e6 00
    cmp         code_temp, 1 << 31                                                     ; code_temp - (1u << 31) ?
    pinsrw      xmm4, word [block + 52 * SIZEOF_WORD], 1 ; w4: 00 g6 g5 00 g3 g2 g1 g0
    movlps      xmm1, qword [block + 20 * SIZEOF_WORD]   ; w1: f7 f6 f5 f4 f2 f0 d5 d0
    pinsrw      xmm4, word [block + 31 * SIZEOF_WORD], 4 ; w4: 00 g6 g5 g4 g3 g2 g1 g0
    pshuflw     xmm1, xmm1, 01110010b                    ; w1: f7 f6 f5 f4 d5 f2 d0 f0
    pinsrw      xmm4, word [block + 53 * SIZEOF_WORD], 7 ; w4: g7 g6 g5 g4 g3 g2 g1 g0
    adc         code_temp, -1                                                          ; code_temp += -1 + (code_temp >= 0);
    pxor        xmm2, xmm2                               ; w2[i] = 0
    pcmpgtw     xmm0, xmm4                               ; w0[i] = -(w0[i] < w4[i])
    pinsrw      xmm1, word [block + 15 * SIZEOF_WORD], 1 ; w1: f7 f6 f5 f4 d5 f2 f1 f0
    paddw       xmm4, xmm0                               ; w4[i] += w0[i]
    movaps      XMMWORD [t + 48 * SIZEOF_WORD], xmm4     ; t[48+i] = w4[i]
    movd        mmtemp, code_temp                                                      ; mmtemp = code_temp
    pinsrw      xmm1, word [block + 30 * SIZEOF_WORD], 3 ; w1: f7 f6 f5 f4 f3 f2 f1 f0
    pcmpeqw     xmm4, xmm2                               ; w4[i] = -(w4[i] == w2[i])
    packsswb    xmm4, xmm3                               ; b4[i] = w4[i], b4[i+8] = w3[i], sign saturated
    lea         t, [t - SIZEOF_WORD]                                                   ; t = &t[-1]
    pxor        xmm0, xmm0                               ; w0[i] = 0
    pcmpgtw     xmm2, xmm1                               ; w2[i] = -(w2[i] < w1[i])
    paddw       xmm1, xmm2                               ; w1[i] += w2[i]
    movaps      XMMWORD [t + (40+1) * SIZEOF_WORD], xmm1 ; t[40+i] = w1[i]
    pcmpeqw     xmm1, xmm0                               ; w1[i] = -(w1[i] == w0[i])
    pinsrw      xmm5, word [block + 42 * SIZEOF_WORD], 0 ; w5: g0 f7 f6 e4 e3 e2 e1 e0
    pinsrw      xmm5, word [block + 43 * SIZEOF_WORD], 5 ; w5: g0 f7 e5 e4 e3 e2 e1 e0
    pinsrw      xmm5, word [block + 36 * SIZEOF_WORD], 6 ; w5: g0 e6 e5 e4 e3 e2 e1 e0
    pinsrw      xmm5, word [block + 29 * SIZEOF_WORD], 7 ; w5: e7 e6 e5 e4 e3 e2 e1 e0
; block -> nbits
%xdefine nbits  block
%define nbitsh dh
%define nbitsb dl
%undef block
    movzx       nbits, byte [NBITS_(code_temp)]                                        ; nbits = JPEG_NBITS_(code_temp);
%xdefine actbl code_temp
%undef code_temp
    pxor        xmm2, xmm2                               ; w2[i] = 0
    mov         actbl, [frame + arg_state]
    movd        code_bits, nbits                                                       ; code_bits = nbits;
    pcmpgtw     xmm0, xmm5                               ; w5[i] = -(w5[i] < w0[i])
    movd        code, dword [dctbl + c_derived_tbl.ehufco + nbits * 4]                 ; code = dctbl->ehufco[nbits];
%xdefine size dctbl
%define sizeh ch
%define sizeb cl
    paddw       xmm5, xmm0                               ; w5[i] += w0[i]
    movaps      XMMWORD [t + (32+1) * SIZEOF_WORD], xmm5 ; t[i] = w0[i]
    movzx       size, byte [dctbl + c_derived_tbl.ehufsi + nbits]                      ; size = dctbl->ehufsi[nbits];
%undef dctbl
    pcmpeqw     xmm5, xmm2                               ; w5[i] = -(w5[i] == w2[i])
    packsswb    xmm5, xmm1                               ; b5[i] = w5[i], b5[i+8] = w1[i], sign saturated
    movq        put_buffer, [actbl + working_state.cur.put.buffer_simd]                ; put_buffer = state->cur.put.buffer;
    mov         free_bits, [actbl + working_state.cur.free_bits]                       ; free_bits -= state->cur.free_bits;
    mov         actbl, [frame + arg_actbl]
%define buffer eax
    mov         buffer, [frame + arg_buffer]
%undef frame
    jmp        .BEGIN
    align       16
; size <= 32, so not really a loop
.BRLOOP1:                                                             ; .BRLOOP1:
    movzx       nbits, byte [actbl + c_derived_tbl.ehufsi + 0xf0]     ;     nbits = actbl->ehufsi[0xf0];
    movd        code, dword [actbl + c_derived_tbl.ehufco + 0xf0 * 4] ;     code = actbl->ehufco[0xf0];
    and         index, 0x7ffffff ; clear index if size == 32          ;     index &= 0x7fffffff; // fix case size == 32
    sub         size, 16                                              ;     size -= 16;
    sub         free_bits, nbits                                      ;     if ((free_bits -= nbits) <= 0)
    jle         .EMIT_BRLOOP1                                         ;         insert code and flush put_buffer
    movd        code_bits, nbits                                      ;     else { code_bits = nbits;
    psllq       put_buffer, code_bits                                 ;         put_buffer <<= code_bits;
    por         put_buffer, code                                      ;         put_buffer |= code;
    jmp        .ERLOOP1                                               ;     goto .ERLOOP1;
    align       16
%ifdef PIC
    times 6 nop
%else
    times 2 nop
%endif
.BLOOP1:                                                                                       ; do { // size == #zero bits/elements to skip
; if size == 32 index remains unchanged, correct in BRLOOP
    shr         index, sizeb                                                                   ;    index >>= size;
    lea         t, [t + size * SIZEOF_WORD]                                                    ;    t += size;
    cmp         size, 16                                                                       ;    if (size > 16)
    jg          .BRLOOP1                                                                       ;        goto .BRLOOP1;
.ERLOOP1:                                                                                      ; .ERLOOP1:
    movsx       nbits, word [t]                                                                ;     nbits = *t;
%ifdef PIC
    add         size, size                                                                     ;     size += size;
%else
    lea         size, [size * 2]                                                               ;     size += size;
%endif
    movd        mmtemp, nbits                                                                  ;     mmtemp = nbits;
    movzx       nbits, byte [NBITS_(nbits)]                                                    ;     nbits = JPEG_NBITS_(nbits);
    lea         size, [size * 8 + nbits]                                                       ;     size = size * 8 + nbits;
    movd        code_bits, nbits                                                               ;     code_bits = nbits;
    movd        code, dword [actbl + c_derived_tbl.ehufco + (size - 16) * 4]                   ;     code = actbl->ehufco[size-16];
    movzx       size, byte [actbl + c_derived_tbl.ehufsi + (size - 16)]                        ;     size = actbl->ehufsi[size-16];
.BEGIN:                                                                                        ; .BEGIN:
    pand        mmtemp, [MASK_BITS(nbits)]                                                     ;     mmtemp &= (1 << nbits) - 1;
    psllq       code, code_bits                                                                ;     code <<= code_bits;
    add         nbits, size                                                                    ;     nbits += size;
    por         code, mmtemp                                                                   ;     code |= mmtemp;
    sub         free_bits, nbits                                                               ;     if ((free_bits -= nbits) <= 0)
    jle         .EMIT_ERLOOP1                                                                  ;         insert code,flush put_buffer, init size, goto BLOOP
    xor         size, size ; kill tzcnt input dependency                                       ;     size = 0;
    tzcnt       size, index                                                                    ;     size = #trailing 0-bits in index
    movd        code_bits, nbits                                                               ;     code_bits = nbits;
    psllq       put_buffer, code_bits                                                          ;     put_buffer <<= code_bits;
    inc         size                                                                           ;     ++size;
    por         put_buffer, code                                                               ;     put_buffer |= code;
    test        index, index                                                                   ;     index & index ?
    jnz         .BLOOP1                                                                        ; } while (index != 0);
; round 2
; t points to the last used word, possibly below t_ if previous index had 32 zero bits
.ELOOP1:
    pmovmskb    size, xmm4                                                                     ; size = (((b4[i] >> 7) << i) | ...);
    pmovmskb    index, xmm5                                                                    ; index = (((b5[i] >> 7) << i) | ...);
    shl         size, 16                                                                       ; size <<= 16;
    or          index, size                                                                    ; index |= size;
    not         index                                                                          ; index = ~index;
    lea         nbits, [t + (1 + DCTSIZE2) * SIZEOF_WORD]                                      ; nbits = t + 1 + 64
    and         nbits, -DCTSIZE2 * SIZEOF_WORD                                                 ; nbits &= -128 // now points at &t_[64]
    sub         nbits, t                                                                       ; nbits -= t;
    shr         nbits, 1 ; #leading-zero-bits_old_index + 33                                   ; nbits >>= 1;
    tzcnt       size, index                                                                    ; size = #trailing 0-bits in index
    inc         size                                                                           ; ++size;
    test        index, index                                                                   ; index & index ?
    jz          .ELOOP2                                                                        ; if (index == 0) goto .ELOOP2;
; note: size == 32 cannot happen, since the last element is always 0
    shr         index, sizeb                                                                   ; index >>= size;
    lea         size, [size + nbits - 33]                                                      ; size = size + nbits - 33;
    lea         t, [t + size * SIZEOF_WORD]                                                    ; t += size;
    cmp         size, 16                                                                       ; if (size <= 16)
    jle         .ERLOOP2                                                                       ;     goto .ERLOOP2;
.BRLOOP2:                                                                                      ; .BRLOOP2: do {
    movzx       nbits, byte [actbl + c_derived_tbl.ehufsi + 0xf0]                              ;    nbits = actbl->ehufsi[0xf0];
    sub         size, 16                                                                       ;    size -= 16;
    movd        code, dword [actbl + c_derived_tbl.ehufco + 0xf0 * 4]                          ;    code = actbl->ehufco[0xf0];
    sub         free_bits, nbits                                                               ;    if ((free_bits -= nbits) <= 0)
    jle         .EMIT_BRLOOP2                                                                  ;        insert code and flush put_buffer
    movd        code_bits, nbits                                                               ;    else { code_bits = nbits;
    psllq       put_buffer, code_bits                                                          ;        put_buffer <<= code_bits;
    por         put_buffer, code                                                               ;        put_buffer |= code;
    cmp         size, 16                                                                       ;    if (size <= 16)
    jle        .ERLOOP2                                                                        ;        goto .ERLOOP2;
    jmp        .BRLOOP2                                                                        ; } while(1);

    align      16
.BLOOP2:                                                                                       ; .BLOOP2: do {
    shr         index, sizeb                                                                   ;     index >>= size;
    lea         t, [t + size * SIZEOF_WORD]                                                    ;     t += size;
    cmp         size, 16                                                                       ;     if (size > 16)
    jg          .BRLOOP2                                                                       ;         goto .BRLOOP2;
.ERLOOP2:                                                                                      ; .ERLOOP2:
    movsx       nbits, word [t]                                                                ;     nbits = *t;
    add         size, size                                                                     ;     size *= 2;
    movd        mmtemp, nbits                                                                  ;     mmtemp = nbits;
    movzx       nbits, byte [NBITS_(nbits)]                                                    ;     nbits = JPEG_NBITS_(nbits);
    movd        code_bits, nbits                                                               ;     code_bits = nbits;
    lea         size, [size * 8 + nbits]                                                       ;     size = size * 8 + nbits;
    movd        code, dword [actbl + c_derived_tbl.ehufco + (size - 16) * 4]                   ;     code = actbl->ehufco[size-16];
    movzx       size, byte [actbl + c_derived_tbl.ehufsi + (size - 16)]                        ;     size = actbl->ehufsi[size-16];
    psllq       code, code_bits                                                                ;     code <<= code_bits;
    pand        mmtemp, [MASK_BITS(nbits)]                                                     ;     mmtemp &= (1 << nbits) - 1;
    lea         nbits, [nbits + size]                                                          ;     nbits += size;
    por         code, mmtemp                                                                   ;     code |= mmtemp;
    xor         size, size ; kill tzcnt input dependency                                       ;     size = 0;
    sub         free_bits, nbits                                                               ;     if ((free_bits -= nbits) <= 0)
    jle         .EMIT_ERLOOP2                                                                  ;         insert code, flush put_buffer, init size, goto BLOOP2
    tzcnt       size, index                                                                    ;     size = #trailing 0-bits in index
    movd        code_bits, nbits                                                               ;     code_bits = nbits;
    psllq       put_buffer, code_bits                                                          ;     put_buffer <<= code_bits;
    inc         size                                                                           ;     ++size;
    por         put_buffer, code                                                               ;     put_buffer |= code;
    test        index, index                                                                   ;     index & index ?
    jnz         .BLOOP2                                                                        ; } while (index != 0);
.ELOOP2:                                                                                       ; .ELOOP2:
    mov         nbits, t                                                                       ; nbits = t;
    lea         t, [t + SIZEOF_WORD]                                                              ; t = &t[1];
    and         nbits, DCTSIZE2 * SIZEOF_WORD - 1                                                 ; nbits &= 127;
    and         t, -DCTSIZE2 * SIZEOF_WORD                                                        ; t &= -128; // &t_[0]
    cmp         nbits, (DCTSIZE2 - 2) * SIZEOF_WORD                                               ; if (nbits == 62 * 2)
    je          .EFN                                                                           ;     goto .EFN;
    movd        code, dword [actbl + c_derived_tbl.ehufco + 0]                                 ; code = actbl->ehufco[0];
    movzx       nbits, byte [actbl + c_derived_tbl.ehufsi + 0]                                 ; size = actbl->ehufsi[0];
    sub         free_bits, nbits                                                               ; free_bits -= nbits;
    jg         .EMIT_EFN_SKIP                                                                  ; if (free_bits <= 0)
    EMIT_QWORD  size, sizeh, sizeb, , , , , , jmp .EFN                                         ;     insert code, flush put_buffer
    align      16
.EMIT_EFN_SKIP:                                                                                ; else {
    movd        code_bits, nbits                                                               ;     code_bits = free_bits;
    psllq       put_buffer, code_bits                                                          ;     put_buffer <<= code_bits;
    por         put_buffer, code                                                               ;     put_buffer |= code;
.EFN:                                                                                          ; }
    mov         esp, [t + save_frame]
%define frame   esp
    ; Save put_buffer & put_bits
    mov         size, [frame + arg_state]                                                      ; size = state;
    movq        [size + working_state.cur.put.buffer_simd], put_buffer                         ; state->cur.put.buffer_size = put_buffer;
    emms
    mov         [size + working_state.cur.free_bits], free_bits                                ; state->cur.put_bits = free_bits;
    POP         edi
    POP         esi
    POP         ebp
    POP         ebx
    ret

    align      16
.EMIT_BRLOOP1:
    EMIT_QWORD        emit_temp, emit_temph, emit_tempb, , , , , , \
      {jmp         .ERLOOP1}

    align      16
.EMIT_ERLOOP1:
    EMIT_QWORD        size, sizeh, sizeb, \
      {xor         size, size}, \
      {tzcnt       size, index}, \
      {inc         size}, \
      {test        index, index}, \
      {jnz         .BLOOP1}, \
      {jmp         .ELOOP1}

    align      16
.EMIT_BRLOOP2:
    EMIT_QWORD        emit_temp, emit_temph, emit_tempb, , , , \
      {cmp         size, 16}, \
      {jle        .ERLOOP2}, \
      {jmp        .BRLOOP2}

    align      16
.EMIT_ERLOOP2:
    EMIT_QWORD        size, sizeh, sizeb, \
      {xor         size, size}, \
      {tzcnt       size, index}, \
      {inc         size}, \
      {test        index, index}, \
      {jnz         .BLOOP2}, \
      {jmp         .ELOOP2}

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
    align       32
