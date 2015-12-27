;
; jchuff-avx2.asm - Huffman entropy encoding routines.
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009-2011, 2014-2015 D. R. Commander.
; Copyright 2015 Matthieu Darbois
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
; This file contains an AVX2 implementation for huffman coding of one block.
; The following code is based directly on jchuff.c; see the jchuff.c for more details.
;
; [TAB8]

%include "jsimdext.inc"

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    32

; These macros perform the same task as the emit_bits() function in the
; original libjpeg code.  In addition to reducing overhead by explicitly
; inlining the code, additional performance is achieved by taking into
; account the size of the bit buffer and waiting until it is almost full
; before emptying it.  This mostly benefits 64-bit platforms, since 6
; bytes can be stored in a 64-bit bit buffer before it has to be emptied.

%macro EMIT_BYTE 0
        sub put_bits, 8 ; put_bits -= 8;
        mov edx, put_buffer
        mov ecx, put_bits
        shr edx, cl ; c = (JOCTET)GETJOCTET(put_buffer >> put_bits);
        mov byte [esi], dl ; *buffer++ = c;
        add esi, 1
        cmp dl, 0xFF ; need to stuff a zero byte?
        jne %%.EMIT_BYTE_END
        mov byte [esi], 0 ; *buffer++ = 0;
        add esi, 1
%%.EMIT_BYTE_END:
%endmacro

%macro PUT_BITS 1
        add put_bits, ecx ;put_bits += size;
        shl put_buffer, cl ; put_buffer = (put_buffer << size);
        or  put_buffer, %1
%endmacro

%macro CHECKBUF15 0
        cmp put_bits, 16 ; if (put_bits > 31) {
        jl %%.CHECKBUF15_END
        push esi
        push edx
        mov esi, POINTER [esp+buffer+8]
        EMIT_BYTE
        EMIT_BYTE
        mov POINTER [esp+buffer+8], esi
        pop edx
        pop esi
%%.CHECKBUF15_END:
%endmacro

%macro EMIT_BITS 1
        PUT_BITS %1
        CHECKBUF15
%endmacro

%macro kloop_prepare2 37 ;(ko, jno0, ..., jno31, xmm0, xmm1, xmm2, xmm3)
    pxor xmm4, xmm4 ; __m128i neg = _mm_setzero_si128();
    pxor xmm5, xmm5 ; __m128i neg = _mm_setzero_si128();
    pxor xmm6, xmm6 ; __m128i neg = _mm_setzero_si128();
    pxor xmm7, xmm7 ; __m128i neg = _mm_setzero_si128();
    pinsrw %34, word [esi+%2*SIZEOF_WORD], 0 ; xmm_shadow[0] = block[jno0];
    pinsrw %35, word [esi+%10*SIZEOF_WORD], 0 ; xmm_shadow[8] = block[jno8];
    pinsrw %36, word [esi+%18*SIZEOF_WORD], 0 ; xmm_shadow[16] = block[jno16];
    pinsrw %37, word [esi+%26*SIZEOF_WORD], 0 ; xmm_shadow[24] = block[jno24];
    pinsrw %34, word [esi+%3*SIZEOF_WORD], 1 ; xmm_shadow[1] = block[jno1];
    pinsrw %35, word [esi+%11*SIZEOF_WORD], 1 ; xmm_shadow[9] = block[jno9];
    pinsrw %36, word [esi+%19*SIZEOF_WORD], 1 ; xmm_shadow[17] = block[jno17];
    pinsrw %37, word [esi+%27*SIZEOF_WORD], 1 ; xmm_shadow[25] = block[jno25];
    pinsrw %34, word [esi+%4*SIZEOF_WORD], 2 ; xmm_shadow[2] = block[jno2];
    pinsrw %35, word [esi+%12*SIZEOF_WORD], 2 ; xmm_shadow[10] = block[jno10];
    pinsrw %36, word [esi+%20*SIZEOF_WORD], 2 ; xmm_shadow[18] = block[jno18];
    pinsrw %37, word [esi+%28*SIZEOF_WORD], 2 ; xmm_shadow[26] = block[jno26];
    pinsrw %34, word [esi+%5*SIZEOF_WORD], 3 ; xmm_shadow[3] = block[jno3];
    pinsrw %35, word [esi+%13*SIZEOF_WORD], 3 ; xmm_shadow[11] = block[jno11];
    pinsrw %36, word [esi+%21*SIZEOF_WORD], 3 ; xmm_shadow[19] = block[jno19];
    pinsrw %37, word [esi+%29*SIZEOF_WORD], 3 ; xmm_shadow[27] = block[jno27];
    pinsrw %34, word [esi+%6*SIZEOF_WORD], 4 ; xmm_shadow[4] = block[jno4];
    pinsrw %35, word [esi+%14*SIZEOF_WORD], 4 ; xmm_shadow[12] = block[jno12];
    pinsrw %36, word [esi+%22*SIZEOF_WORD], 4 ; xmm_shadow[20] = block[jno20];
    pinsrw %37, word [esi+%30*SIZEOF_WORD], 4 ; xmm_shadow[28] = block[jno28];
    pinsrw %34, word [esi+%7*SIZEOF_WORD], 5 ; xmm_shadow[5] = block[jno5];
    pinsrw %35, word [esi+%15*SIZEOF_WORD], 5 ; xmm_shadow[13] = block[jno13];
    pinsrw %36, word [esi+%23*SIZEOF_WORD], 5 ; xmm_shadow[21] = block[jno21];
    pinsrw %37, word [esi+%31*SIZEOF_WORD], 5 ; xmm_shadow[29] = block[jno29];
    pinsrw %34, word [esi+%8*SIZEOF_WORD], 6 ; xmm_shadow[6] = block[jno6];
    pinsrw %35, word [esi+%16*SIZEOF_WORD], 6 ; xmm_shadow[14] = block[jno14];
    pinsrw %36, word [esi+%24*SIZEOF_WORD], 6 ; xmm_shadow[22] = block[jno22];
    pinsrw %37, word [esi+%32*SIZEOF_WORD], 6 ; xmm_shadow[30] = block[jno30];
    pinsrw %34, word [esi+%9*SIZEOF_WORD], 7 ; xmm_shadow[7] = block[jno7];
    pinsrw %35, word [esi+%17*SIZEOF_WORD], 7 ; xmm_shadow[15] = block[jno15];
    pinsrw %36, word [esi+%25*SIZEOF_WORD], 7 ; xmm_shadow[23] = block[jno23];
%if %1 != 32 
    pinsrw %37, word [esi+%33*SIZEOF_WORD], 7 ; xmm_shadow[31] = block[jno31];
%else
    pinsrw %37, ecx, 7 ; xmm_shadow[31] = block[jno31];
%endif
    pcmpgtw xmm4, %34 ; neg = _mm_cmpgt_epi16(neg, x1);
    pcmpgtw xmm5, %35 ; neg = _mm_cmpgt_epi16(neg, x1);
    pcmpgtw xmm6, %36 ; neg = _mm_cmpgt_epi16(neg, x1);
    pcmpgtw xmm7, %37 ; neg = _mm_cmpgt_epi16(neg, x1);
    pabsw %34, %34   ; x1 = _mm_abs_epi16(x1);
    pabsw %35, %35   ; x1 = _mm_abs_epi16(x1);
    pabsw %36, %36   ; x1 = _mm_abs_epi16(x1);
    pabsw %37, %37   ; x1 = _mm_abs_epi16(x1);
    pxor xmm4, %34   ; neg = _mm_xor_si128(neg, x1);
    pxor xmm5, %35   ; neg = _mm_xor_si128(neg, x1);
    pxor xmm6, %36   ; neg = _mm_xor_si128(neg, x1);
    pxor xmm7, %37   ; neg = _mm_xor_si128(neg, x1);
    movdqa XMMWORD [esp+t1+%1*SIZEOF_WORD], %34 ; _mm_storeu_si128((__m128i*)(t1+ko), x1);
    movdqa XMMWORD [esp+t1+(%1+8)*SIZEOF_WORD], %35 ; _mm_storeu_si128((__m128i*)(t1+ko+8), x1);
    movdqa XMMWORD [esp+t1+(%1+16)*SIZEOF_WORD], %36 ; _mm_storeu_si128((__m128i*)(t1+ko+16), x1);
    movdqa XMMWORD [esp+t1+(%1+24)*SIZEOF_WORD], %37 ; _mm_storeu_si128((__m128i*)(t1+ko+24), x1);
    movdqa XMMWORD [esp+t2+%1*SIZEOF_WORD], xmm4 ; _mm_storeu_si128((__m128i*)(t2+ko), neg);
    movdqa XMMWORD [esp+t2+(%1+8)*SIZEOF_WORD], xmm5 ; _mm_storeu_si128((__m128i*)(t2+ko+8), neg);
    movdqa XMMWORD [esp+t2+(%1+16)*SIZEOF_WORD], xmm6 ; _mm_storeu_si128((__m128i*)(t2+ko+16), neg);
    movdqa XMMWORD [esp+t2+(%1+24)*SIZEOF_WORD], xmm7 ; _mm_storeu_si128((__m128i*)(t2+ko+24), neg);
%endmacro

%macro kloop_prepare 37 ;(ko, jno0, ..., jno31, xmm0, xmm1, xmm2, xmm3)
    vpinsrw %34, %34, word [esi+%2*SIZEOF_WORD], 0 ; xmm_shadow[0] = block[jno0];
    vpinsrw %35, %35, word [esi+%10*SIZEOF_WORD], 0 ; xmm_shadow[8] = block[jno8];
    vpinsrw %36, %36, word [esi+%18*SIZEOF_WORD], 0 ; xmm_shadow[16] = block[jno16];
    vpinsrw %37, %37, word [esi+%26*SIZEOF_WORD], 0 ; xmm_shadow[24] = block[jno24];
    vpinsrw %34, %34, word [esi+%3*SIZEOF_WORD], 1 ; xmm_shadow[1] = block[jno1];
    vpinsrw %35, %35, word [esi+%11*SIZEOF_WORD], 1 ; xmm_shadow[9] = block[jno9];
    vpinsrw %36, %36, word [esi+%19*SIZEOF_WORD], 1 ; xmm_shadow[17] = block[jno17];
    vpinsrw %37, %37, word [esi+%27*SIZEOF_WORD], 1 ; xmm_shadow[25] = block[jno25];
    vpinsrw %34, %34, word [esi+%4*SIZEOF_WORD], 2 ; xmm_shadow[2] = block[jno2];
    vpinsrw %35, %35, word [esi+%12*SIZEOF_WORD], 2 ; xmm_shadow[10] = block[jno10];
    vpinsrw %36, %36, word [esi+%20*SIZEOF_WORD], 2 ; xmm_shadow[18] = block[jno18];
    vpinsrw %37, %37, word [esi+%28*SIZEOF_WORD], 2 ; xmm_shadow[26] = block[jno26];
    vpinsrw %34, %34, word [esi+%5*SIZEOF_WORD], 3 ; xmm_shadow[3] = block[jno3];
    vpinsrw %35, %35, word [esi+%13*SIZEOF_WORD], 3 ; xmm_shadow[11] = block[jno11];
    vpinsrw %36, %36, word [esi+%21*SIZEOF_WORD], 3 ; xmm_shadow[19] = block[jno19];
    vpinsrw %37, %37, word [esi+%29*SIZEOF_WORD], 3 ; xmm_shadow[27] = block[jno27];
    vpinsrw %34, %34, word [esi+%6*SIZEOF_WORD], 4 ; xmm_shadow[4] = block[jno4];
    vpinsrw %35, %35, word [esi+%14*SIZEOF_WORD], 4 ; xmm_shadow[12] = block[jno12];
    vpinsrw %36, %36, word [esi+%22*SIZEOF_WORD], 4 ; xmm_shadow[20] = block[jno20];
    vpinsrw %37, %37, word [esi+%30*SIZEOF_WORD], 4 ; xmm_shadow[28] = block[jno28];
    vpinsrw %34, %34, word [esi+%7*SIZEOF_WORD], 5 ; xmm_shadow[5] = block[jno5];
    vpinsrw %35, %35, word [esi+%15*SIZEOF_WORD], 5 ; xmm_shadow[13] = block[jno13];
    vpinsrw %36, %36, word [esi+%23*SIZEOF_WORD], 5 ; xmm_shadow[21] = block[jno21];
    vpinsrw %37, %37, word [esi+%31*SIZEOF_WORD], 5 ; xmm_shadow[29] = block[jno29];
    vpinsrw %34, %34, word [esi+%8*SIZEOF_WORD], 6 ; xmm_shadow[6] = block[jno6];
    vpinsrw %35, %35, word [esi+%16*SIZEOF_WORD], 6 ; xmm_shadow[14] = block[jno14];
    vpinsrw %36, %36, word [esi+%24*SIZEOF_WORD], 6 ; xmm_shadow[22] = block[jno22];
    vpinsrw %37, %37, word [esi+%32*SIZEOF_WORD], 6 ; xmm_shadow[30] = block[jno30];
    vpinsrw %34, %34, word [esi+%9*SIZEOF_WORD], 7 ; xmm_shadow[7] = block[jno7];
    vpinsrw %35, %35, word [esi+%17*SIZEOF_WORD], 7 ; xmm_shadow[15] = block[jno15];
    vpinsrw %36, %36, word [esi+%25*SIZEOF_WORD], 7 ; xmm_shadow[23] = block[jno23];
%if %1 != 32 
    vpinsrw %37, %37, word [esi+%33*SIZEOF_WORD], 7 ; xmm_shadow[31] = block[jno31];
%else
    vpinsrw %37, %37, ecx, 7 ; xmm_shadow[31] = block[jno31];
%endif
%endmacro

;
; Encode a single block's worth of coefficients.
;
; GLOBAL(void)
; jsimd_encode_one_block_ssse3 (DCTELEM * data)
;

; eax + 8 = working_state* state
; eax + 12 = JOCTET *buffer
; eax + 16 = JCOEFPTR block
; eax + 20 = int last_dc_val
; eax + 24 = c_derived_tbl *dctbl
; eax + 28 = c_derived_tbl *actbl

%define pushsize        6*SIZEOF_DWORD ; Align to 16 bytes
%define t1              pushsize
%define t2              t1+(DCTSIZE2*SIZEOF_WORD)
%define block           t2+(DCTSIZE2*SIZEOF_WORD)
%define actbl           block+SIZEOF_DWORD   
%define buffer          actbl+SIZEOF_DWORD
%define temp            buffer+SIZEOF_DWORD
%define temp2           temp+SIZEOF_DWORD
%define temp3           temp2+SIZEOF_DWORD
%define temp4           temp3+SIZEOF_DWORD
%define temp5           temp4+SIZEOF_DWORD 
%define put_buffer      ebx
%define put_bits        edi

        align   16
        global  EXTN(jsimd_encode_one_block_avx2)

EXTN(jsimd_encode_one_block_avx2):
        push    ebp
        mov     eax,esp                         ; eax = original ebp
        sub     esp, byte 4
        and     esp, byte (-SIZEOF_XMMWORD)     ; align to 128 bits
        mov     [esp],eax
        mov     ebp,esp                         ; ebp = aligned ebp
        sub     esp, temp5+9*SIZEOF_DWORD-pushsize
        push    ebx
        push    ecx
;       push    edx             ; need not be preserved
        push    esi
        push    edi
        push    ebp
        
        ; Load put_buffer & put_bits
        mov esi, POINTER [eax+8]        ; (working_state* state)
        mov put_buffer,  DWORD [esi+8]  ;put_buffer = state->cur.put_buffer;
        mov put_bits,    DWORD [esi+12] ;put_bits = state->cur.put_bits;
        push esi ; r10 is now scratch
        
        mov ecx, POINTER [eax+28]
        mov edx, POINTER [eax+16]
        mov esi, POINTER [eax+12]
        mov POINTER [esp+actbl],  ecx
        mov POINTER [esp+block],  edx
        mov POINTER [esp+buffer], esi
        
        ; Encode the DC coefficient difference per section F.1.2.1
        mov esi, POINTER [esp+block]        ; block
        movsx ecx, word [esi]  ; temp = temp2 = block[0] - last_dc_val;
        sub   ecx, DWORD [eax+20]
        mov   esi, ecx

        ; This is a well-known technique for obtaining the absolute value without a
        ; branch.  It is derived from an assembly language technique presented in
        ; "How to Optimize for the Pentium Processors", Copyright (c) 1996, 1997 by
        ; Agner Fog.
        mov edx, ecx
        sar edx, 31   ; temp3 = temp >> (CHAR_BIT * sizeof(int) - 1);
        xor ecx, edx ; temp ^= temp3;
        sub ecx, edx ; temp -= temp3;

        ; For a negative input, want temp2 = bitwise complement of abs(input)
        ; This code assumes we are on a two's complement machine
        add esi, edx ; temp2 += temp3;
        mov DWORD [esp+temp], esi ; backup in temp

        ; Find the number of bits needed for the magnitude of the coefficient
        bsr edx, ecx ; nbits = JPEG_NBITS(temp);
        jz .nbits_zero
        inc edx
        jmp .nbits_end
.nbits_zero:
        xor edx, edx
.nbits_end:
        mov DWORD [esp+temp2], edx ; backup in temp2

        ; Emit the Huffman-coded symbol for the number of bits
        mov    ebp, POINTER [eax+24] ; After this point, arguments are not accessible anymore
        mov    eax,  INT [ebp + edx * 4 ] ; code = dctbl->ehufco[nbits];
        movzx  ecx, byte [ebp + edx + 1024]  ; size = dctbl->ehufsi[nbits];
        
        EMIT_BITS eax ; EMIT_BITS(code, size)

        mov ecx, DWORD [esp+temp2] ; restore nbits
        ; Mask off any extra bits in code
        mov eax, 1
        shl eax, cl
        dec eax
        and eax, DWORD [esp+temp] ; temp2 &= (((JLONG) 1)<<nbits) - 1;

        ; Emit that number of bits of the value, if positive,
        ; or the complement of its magnitude, if negative.
        EMIT_BITS eax ; EMIT_BITS(temp2, nbits)
        
        ; Prepare data
        xor ecx, ecx
        mov esi, POINTER [esp+block]
        kloop_prepare  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28, 35, xmm0, xmm4, xmm1, xmm5
        kloop_prepare 32, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63, 63, xmm2, xmm6, xmm3, xmm7
        vinserti128 ymm0, ymm0, xmm4, 1
        vinserti128 ymm1, ymm1, xmm5, 1
        vinserti128 ymm2, ymm2, xmm6, 1
        vinserti128 ymm3, ymm3, xmm7, 1
        vpxor ymm4, ymm4, ymm4 ; __m128i neg = _mm_setzero_si128();
        vpxor ymm5, ymm5, ymm5 ; __m128i neg = _mm_setzero_si128();
        vpxor ymm6, ymm6, ymm6 ; __m128i neg = _mm_setzero_si128();
        vpxor ymm7, ymm7, ymm7 ; __m128i neg = _mm_setzero_si128();
        vpcmpgtw ymm4, ymm4, ymm0 ; neg = _mm_cmpgt_epi16(neg, x1);
        vpcmpgtw ymm5, ymm5, ymm1 ; neg = _mm_cmpgt_epi16(neg, x1);
        vpcmpgtw ymm6, ymm6, ymm2 ; neg = _mm_cmpgt_epi16(neg, x1);
        vpcmpgtw ymm7, ymm7, ymm3 ; neg = _mm_cmpgt_epi16(neg, x1);
        vpabsw ymm0, ymm0   ; x1 = _mm_abs_epi16(x1);
        vpabsw ymm1, ymm1   ; x1 = _mm_abs_epi16(x1);
        vpabsw ymm2, ymm2   ; x1 = _mm_abs_epi16(x1);
        vpabsw ymm3, ymm3   ; x1 = _mm_abs_epi16(x1);
        vpxor ymm4, ymm0    ; neg = _mm_xor_si128(neg, x1);
        vpxor ymm5, ymm1    ; neg = _mm_xor_si128(neg, x1);
        vpxor ymm6, ymm2   ; neg = _mm_xor_si128(neg, x1);
        vpxor ymm7, ymm3   ; neg = _mm_xor_si128(neg, x1);
        vmovdqu XMMWORD [esp+t1+0*SIZEOF_WORD], ymm0 ; _mm_storeu_si128((__m128i*)(t1+ko), x1);
        vmovdqu XMMWORD [esp+t1+16*SIZEOF_WORD], ymm1 ; _mm_storeu_si128((__m128i*)(t1+ko+8), x1);
        vmovdqu XMMWORD [esp+t1+32*SIZEOF_WORD], ymm2 ; _mm_storeu_si128((__m128i*)(t1+ko+16), x1);
        vmovdqu XMMWORD [esp+t1+48*SIZEOF_WORD], ymm3 ; _mm_storeu_si128((__m128i*)(t1+ko+24), x1);
        vmovdqu XMMWORD [esp+t2+0*SIZEOF_WORD], ymm4 ; _mm_storeu_si128((__m128i*)(t2+ko), neg);
        vmovdqu XMMWORD [esp+t2+16*SIZEOF_WORD], ymm5 ; _mm_storeu_si128((__m128i*)(t2+ko+8), neg);
        vmovdqu XMMWORD [esp+t2+32*SIZEOF_WORD], ymm6 ; _mm_storeu_si128((__m128i*)(t2+ko+16), neg);
        vmovdqu XMMWORD [esp+t2+48*SIZEOF_WORD], ymm7 ; _mm_storeu_si128((__m128i*)(t2+ko+24), neg);
        
        vperm2i128 ymm4, ymm0, ymm1, 32
        vperm2i128 ymm5, ymm0, ymm1, 49
        vperm2i128 ymm6, ymm2, ymm3, 32
        vperm2i128 ymm7, ymm2, ymm3, 49
       
        ; vpacksswb [bl,al],[bh,ah]: pack bh, bl ; pack ah, al 
        
        vpxor ymm0, ymm0, ymm0
        vpcmpeqw ymm4, ymm4, ymm0 ; tmp0 = _mm_cmpeq_epi16(tmp0, zero);
        vpcmpeqw ymm5, ymm5, ymm0 ; tmp1 = _mm_cmpeq_epi16(tmp1, zero);
        vpcmpeqw ymm6, ymm6, ymm0 ; tmp2 = _mm_cmpeq_epi16(tmp2, zero);
        vpcmpeqw ymm7, ymm7, ymm0 ; tmp3 = _mm_cmpeq_epi16(tmp3, zero);
        vpacksswb ymm4, ymm4, ymm5 ; tmp0 = _mm_packs_epi16(tmp0, tmp1);
        vpacksswb ymm6, ymm6, ymm7 ; tmp2 = _mm_packs_epi16(tmp2, tmp3);
        vpmovmskb edx, ymm4 ; index  = ((uint64_t)_mm_movemask_epi8(tmp0)) << 0;
        not edx ; index = ~index;

        lea esi, [esp+t1]
        mov ebp, POINTER [esp+actbl] ; ebp = actbl

.BLOOP:
        bsf ecx, edx ; r = __builtin_ctzl(index);
        jz .ELOOP
        lea esi, [esi+ecx*2] ; k += r;
        shr edx, cl  ; index >>= r;
        mov DWORD [esp+temp3], edx
.BRLOOP:
        cmp ecx, 16 ; while (r > 15) {
        jl .ERLOOP
        sub ecx, 16 ; r -= 16;
        mov DWORD [esp+temp], ecx
        mov   eax, INT [ebp + 240*4]   ; code_0xf0 = actbl->ehufco[0xf0];
        movzx ecx, byte [ebp + 1024 + 240] ; size_0xf0 = actbl->ehufsi[0xf0];
        EMIT_BITS eax ;EMIT_BITS(code_0xf0, size_0xf0)
        mov ecx, DWORD [esp+temp]
        jmp .BRLOOP
.ERLOOP:
        movsx eax, word [esi] ; temp = t1[k];
        bsr eax, eax ; nbits = 32 - __builtin_clz(temp);
        inc eax
        mov DWORD [esp+temp2], eax
        ; Emit Huffman symbol for run length / number of bits
        shl ecx, 4 ;temp3 = (r << 4) + nbits;
        add ecx, eax
        mov   eax,  INT [ebp + ecx * 4 ]   ; code = actbl->ehufco[temp3];
        movzx ecx, byte [ebp + ecx + 1024] ; size = actbl->ehufsi[temp3];
        EMIT_BITS eax
        
        movsx edx, word [esi+DCTSIZE2*2] ; temp2 = t2[k];
        ; Mask off any extra bits in code
        mov ecx, DWORD [esp+temp2]
        mov eax, 1
        shl eax, cl
        dec eax
        and eax, edx ; temp2 &= (((JLONG) 1)<<nbits) - 1;
        EMIT_BITS eax ; PUT_BITS(temp2, nbits)
        mov edx, DWORD [esp+temp3]
        add esi, 2 ;++k;
        shr edx, 1 ;index >>= 1;
        
        jmp .BLOOP
.ELOOP:
        vpmovmskb edx, ymm6 ; index  = ((uint64_t)_mm_movemask_epi8(tmp2)) << 16;
        not edx ; index = ~index;
        
        lea eax, [esp + t1 + (DCTSIZE2/2) * 2]
        sub eax, esi
        shr eax, 1
        bsf ecx, edx ; r = __builtin_ctzl(index);
        jz .ELOOP2
        shr edx, cl  ; index >>= r;
        add ecx, eax
        lea esi, [esi+ecx*2] ; k += r;
        mov DWORD [esp+temp3], edx
        jmp .BRLOOP2
.BLOOP2:
        bsf ecx, edx ; r = __builtin_ctzl(index);
        jz .ELOOP2
        lea esi, [esi+ecx*2] ; k += r;
        shr edx, cl  ; index >>= r;
        mov DWORD [esp+temp3], edx
.BRLOOP2:
        cmp ecx, 16 ; while (r > 15) {
        jl .ERLOOP2
        sub ecx, 16 ; r -= 16;
        mov DWORD [esp+temp], ecx
        mov   eax, INT [ebp + 240*4]   ; code_0xf0 = actbl->ehufco[0xf0];
        movzx ecx, byte [ebp + 1024 + 240] ; size_0xf0 = actbl->ehufsi[0xf0];
        EMIT_BITS eax ;EMIT_BITS(code_0xf0, size_0xf0)
        mov ecx, DWORD [esp+temp]
        jmp .BRLOOP2
.ERLOOP2:
        movsx eax, word [esi] ; temp = t1[k];
        bsr eax, eax ; nbits = 32 - __builtin_clz(temp);
        inc eax
        mov DWORD [esp+temp2], eax
        ; Emit Huffman symbol for run length / number of bits
        shl ecx, 4 ;temp3 = (r << 4) + nbits;
        add ecx, eax
        mov   eax,  INT [ebp + ecx * 4 ]   ; code = actbl->ehufco[temp3];
        movzx ecx, byte [ebp + ecx + 1024] ; size = actbl->ehufsi[temp3];
        EMIT_BITS eax
        
        movsx edx, word [esi+DCTSIZE2*2] ; temp2 = t2[k];
        ; Mask off any extra bits in code
        mov ecx, DWORD [esp+temp2]
        mov eax, 1
        shl eax, cl
        dec eax
        and eax, edx ; temp2 &= (((JLONG) 1)<<nbits) - 1;
        EMIT_BITS eax ; PUT_BITS(temp2, nbits)
        mov edx, DWORD [esp+temp3]
        add esi, 2 ;++k;
        shr edx, 1 ;index >>= 1;
        
        jmp .BLOOP2
.ELOOP2:
        ; If the last coef(s) were zero, emit an end-of-block code
        lea edx, [esp+ t1 + (DCTSIZE2-1) * 2] ; r = DCTSIZE2-1-k;
        cmp edx, esi ; if (r > 0) {
        je .EFN
        mov   eax,  INT [ebp]   ; code = actbl->ehufco[0];
        movzx ecx, byte [ebp + 1024] ; size = actbl->ehufsi[0];
        EMIT_BITS eax
.EFN:   
        vzeroupper
        mov eax, [esp+buffer]
        pop esi
        ; Save put_buffer & put_bits
        mov DWORD [esi+8], put_buffer ; state->cur.put_buffer = put_buffer;
        mov DWORD [esi+12], put_bits ; state->cur.put_bits = put_bits;

        pop     ebp
        pop     edi
        pop     esi
;       pop     edx             ; need not be preserved
        pop     ecx
        pop     ebx
        mov     esp,ebp         ; esp <- aligned ebp
        pop     esp             ; esp <- original ebp
        pop     ebp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   16
