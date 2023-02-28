/*
 * jdhuff.h
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2010-2011, 2015-2016, 2021, D. R. Commander.
 * Copyright (C) 2018, Matthias Räncker.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains declarations for Huffman entropy decoding routines
 * that are shared between the sequential decoder (jdhuff.c) and the
 * progressive decoder (jdphuff.c).  No other modules need to see these.
 */

/*
 * I have made the following modifications in this file to enable faster Huffman decoding. 
 * 1. Defined a new structure JHUFF_DEC;
 * 2. Defined and constructed two new decoder structure DC_derived_tbl and AC_derived_tbl, 
 * to replace the original structure d_derived_tbl.
 * 3. Re-defined the macor functions HUFF_DECODE() and HUFF_DECODE_FAST();
 * Cody Wu, 06 / 28 / 2022
 *
 */
#include "jconfigint.h"

#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif

#ifndef likely
#define likely(expr)     expect((expr) != 0, 1)
#endif
#ifndef unlikely
#define unlikely(expr)   expect((expr) != 0, 0)
#endif

typedef struct {
    unsigned char   next;         /* distance to its next non-zero coefficient; it is set to DCTSIZE2 in case of ending */
    unsigned char   nBits;        /* number of bits for Huffman code length and following extension bits */
    unsigned short  extMask;      /* mask for extension bits */
} JHUFF_DEC;

/* Derived data constructed for each Huffman table */
/* HUFF_LOOKAHEAD must be greater than the minimum length for the decoder to function properly */
/* In fact, HUFF_LOOKAHEAD must be greater than the average length to function efficiently */
#define DC_LOOKAHEAD  8          /* # of bits of lookahead */
#define AC_LOOKAHEAD  10          /* # of bits of lookahead */

/* Lookahead table: indexed by the next HUFF_LOOKAHEAD bits of
   * the input data stream.  If the next Huffman code is no more
   * than HUFF_LOOKAHEAD bits long, we can obtain its length and
   * the corresponding symbol directly from this tables.
   */
typedef struct { 
  unsigned   lookThr, look2Thr;   /* read threshold for the first and second look */
  JHUFF_DEC  lookup[1<<DC_LOOKAHEAD];      /* look up HUFF_LOOKAHEAD bits  */
  JHUFF_DEC  look2up[128];        /* 6:512; 7:256; 8:128; 9:64 is pre-calculated to be sufficient */
} DC_derived_tbl;

typedef struct {
    unsigned   lookThr, look2Thr;   /* read threshold for the first and second look */
    JHUFF_DEC  lookup[1 << AC_LOOKAHEAD];      /* look up HUFF_LOOKAHEAD bits  */
    JHUFF_DEC  look2up[280];        /* 8:472; 9:344; 10:280 is pre-calculated to be sufficient */
} AC_derived_tbl;

/* Expand a Huffman table definition into the derived format */
EXTERN(void) jpeg_make_d_derived_tbl(j_decompress_ptr cinfo, boolean isDC,
    int tblno, void ** pdtbl);


/*
 * Fetching the next N bits from the input stream is a time-critical operation
 * for the Huffman decoders.  We implement it with a combination of inline
 * macros and out-of-line subroutines.  Note that N (the number of bits
 * demanded at one time) never exceeds 15 for JPEG use.
 *
 * We read source bytes into get_buffer and dole out bits as needed.
 * If get_buffer already contains enough bits, they are fetched in-line
 * by the macros CHECK_BIT_BUFFER and GET_BITS.  When there aren't enough
 * bits, jpeg_fill_bit_buffer is called; it will attempt to fill get_buffer
 * as full as possible (not just to the number of bits needed; this
 * prefetching reduces the overhead cost of calling jpeg_fill_bit_buffer).
 * Note that jpeg_fill_bit_buffer may return FALSE to indicate suspension.
 * On TRUE return, jpeg_fill_bit_buffer guarantees that get_buffer contains
 * at least the requested number of bits --- dummy zeroes are inserted if
 * necessary.
 */

#if !defined(_WIN32) && !defined(SIZEOF_SIZE_T)
#error Cannot determine word size
#endif

#if SIZEOF_SIZE_T == 8 || defined(_WIN64)

typedef size_t bit_buf_type;            /* type of bit-extraction buffer */
#define BIT_BUF_SIZE  64                /* size of buffer in bits */

#elif defined(__x86_64__) && defined(__ILP32__)

typedef unsigned long long bit_buf_type; /* type of bit-extraction buffer */
#define BIT_BUF_SIZE  64                 /* size of buffer in bits */

#else

typedef unsigned long bit_buf_type;     /* type of bit-extraction buffer */
#define BIT_BUF_SIZE  32                /* size of buffer in bits */

#endif

/* If long is > 32 bits on your machine, and shifting/masking longs is
 * reasonably fast, making bit_buf_type be long and setting BIT_BUF_SIZE
 * appropriately should be a win.  Unfortunately we can't define the size
 * with something like  #define BIT_BUF_SIZE (sizeof(bit_buf_type)*8)
 * because not all machines measure sizeof in 8-bit bytes.
 */

typedef struct {                /* Bitreading state saved across MCUs */
  bit_buf_type get_buffer;      /* current bit-extraction buffer */
  int bits_left;                /* # of unused bits in it */
} bitread_perm_state;

typedef struct {                /* Bitreading working state within an MCU */
  /* Current data source location */
  /* We need a copy, rather than munging the original, in case of suspension */
  const JOCTET *next_input_byte; /* => next byte to read from source */
  size_t bytes_in_buffer;       /* # of bytes remaining in source buffer */
  /* Bit input buffer --- note these values are kept in register variables,
   * not in this struct, inside the inner loops.
   */
  bit_buf_type get_buffer;      /* current bit-extraction buffer */
  int bits_left;                /* # of unused bits in it */
  /* Pointer needed by jpeg_fill_bit_buffer. */
  j_decompress_ptr cinfo;       /* back link to decompress master record */
} bitread_working_state;

/* Macros to declare and load/save bitread local variables. */
#define BITREAD_STATE_VARS \
  register bit_buf_type get_buffer; \
  register int bits_left; \
  bitread_working_state br_state

#define BITREAD_LOAD_STATE(cinfop, permstate) \
  br_state.cinfo = cinfop; \
  br_state.next_input_byte = cinfop->src->next_input_byte; \
  br_state.bytes_in_buffer = cinfop->src->bytes_in_buffer; \
  get_buffer = permstate.get_buffer; \
  bits_left = permstate.bits_left;

#define BITREAD_SAVE_STATE(cinfop, permstate) \
  cinfop->src->next_input_byte = br_state.next_input_byte; \
  cinfop->src->bytes_in_buffer = br_state.bytes_in_buffer; \
  permstate.get_buffer = get_buffer; \
  permstate.bits_left = bits_left

/*
 * These macros provide the in-line portion of bit fetching.
 * Use CHECK_BIT_BUFFER to ensure there are N bits in get_buffer
 * before using GET_BITS, PEEK_BITS, or DROP_BITS.
 * The variables get_buffer and bits_left are assumed to be locals,
 * but the state struct might not be (jpeg_huff_decode needs this).
 *      CHECK_BIT_BUFFER(state, n, action);
 *              Ensure there are N bits in get_buffer; if suspend, take action.
 *      val = GET_BITS(n);
 *              Fetch next N bits.
 *      val = PEEK_BITS(n);
 *              Fetch next N bits without removing them from the buffer.
 *      DROP_BITS(n);
 *              Discard next N bits.
 * The value N should be a simple variable, not an expression, because it
 * is evaluated multiple times.
 */

#define CHECK_BIT_BUFFER(state, nbits, action) { \
  if (bits_left < (nbits)) { \
    if (!jpeg_fill_bit_buffer(&(state), get_buffer, bits_left, nbits)) \
      { action; } \
    get_buffer = (state).get_buffer;  bits_left = (state).bits_left; \
  } \
}

#define GET_BITS(nbits) \
  (((int)(get_buffer >> (bits_left -= (nbits)))) & ((1 << (nbits)) - 1))

#define PEEK_BITS(nbits) \
  (((int)(get_buffer >> (bits_left -  (nbits)))) & ((1 << (nbits)) - 1))

#define DROP_BITS(nbits) \
  (bits_left -= (nbits))


 /* 
  * Figure F.12: extend sign bit.
  * On some machines, a shift and add will be faster than a table lookup.
  */
#define NEG_1  ((unsigned int)-1)
#define HUFF_EXTEND(x, mask) \
((x) + ((((x) - ( mask^ (mask>>1))) >> 31) & (-(int)mask)))

/* Original: ((x) + ((((x) - (1 << ((s) - 1))) >> 31) & (((NEG_1) << (s)) + 1))) */


/* Load up the bit buffer to a depth of at least nbits */
EXTERN(boolean) jpeg_fill_bit_buffer(bitread_working_state *state,
                                     register bit_buf_type get_buffer,
                                     register int bits_left, int nbits);


/*
 * Code for extracting next Huffman-coded symbol from input bit stream.
 * Again, this is time-critical and we make the main paths be macros.
 *
 * We use a lookahead table to process codes of up to HUFF_LOOKAHEAD bits
 * without looping.  Usually, more than 95% of the Huffman codes will be 8
 * or fewer bits long.  The few overlength codes are handled with a loop,
 * which need not be inline code.
 *
 * Notes about the HUFF_DECODE macro:
 * 1. Near the end of the data segment, we may fail to get enough bits.
 *    In that case, we simply pad enough zero bits.
 */

#define HUFF_DECODE(huffRes, state, LOOKAHEAD, htbl, failAction) { \
  if (bits_left <= 32) { \
    if (!jpeg_fill_bit_buffer(&state, get_buffer, bits_left, 0)) \
      { failAction; } \
    get_buffer = state.get_buffer;  bits_left = state.bits_left; \
  } \
  s = PEEK_BITS(LOOKAHEAD); \
  huffRes = htbl->lookup[s];     \
  if( unlikely( s>= htbl->lookThr) ) { /* .extMask and .next are repurposed as offset and mask respectively */  \
     s = huffRes.extMask + ( (get_buffer >> (bits_left - huffRes.nBits))  & huffRes.next );  \
     if ( likely(s< htbl->look2Thr) ) { huffRes = htbl->look2up[s]; } \
     else { failAction; } \
  }  \
}

#define HUFF_DECODE_FAST(huffRes, LOOKAHEAD, htbl) { \
  FILL_BIT_BUFFER_FAST;          \
  s = PEEK_BITS(LOOKAHEAD); \
  huffRes = htbl->lookup[s];     \
  if( unlikely(s>= htbl->lookThr) ) { /* .extMask and .next are repurposed as offset and mask respectively */   \
     s = huffRes.extMask + ( (get_buffer >> (bits_left - huffRes.nBits))  & huffRes.next );  \
     if ( likely(s< htbl->look2Thr) ) { huffRes = htbl->look2up[s]; } \
     else { return FALSE; } \
  }                         \
}
   
