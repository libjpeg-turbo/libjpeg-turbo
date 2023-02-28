/*
 * jdhuff.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2009-2011, 2016, 2018-2019, D. R. Commander.
 * Copyright (C) 2018, Matthias Räncker.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains Huffman entropy decoding routines.
 *
 * Much of the complexity here has to do with supporting input suspension.
 * If the data source module demands suspension, we want to be able to back
 * up to the start of the current MCU.  To do this, we copy state variables
 * into local working storage, and update them back to the permanent
 * storage only upon successful completion of an MCU.
 *
 * NOTE: All referenced figures are from
 * Recommendation ITU-T T.81 (1992) | ISO/IEC 10918-1:1994.
 */

/* 
 * I made the following modifications in this file to achieve faster Huffman decoding.
 * 1. modified the structure huff_entropy_decoder.
 * 2. modified the macro FILL_BIT_BUFFER_FAST
 * 3. modified the following seven functions: 
 * METHODDEF(void) start_pass_huff_decoder(j_decompress_ptr cinfo);
 * GLOBAL(void) jpeg_make_d_derived_tbl(j_decompress_ptr cinfo, boolean isDC, 
 *                                                    int tblno, void** pdtbl);
 * GLOBAL(boolean) jpeg_fill_bit_buffer(bitread_working_state *state,
 *                    register bit_buf_type get_buffer, register int bits_left,
 *                    int nbits);
 * LOCAL(boolean) decode_mcu_slow(j_decompress_ptr cinfo, JBLOCKROW *const MCU_data);
 * LOCAL(boolean) decode_mcu_fast(j_decompress_ptr cinfo, JBLOCKROW *const MCU_data);
 * METHODDEF(boolean)  decode_mcu(j_decompress_ptr cinfo, JBLOCKROW *MCU_data);
 * GLOBAL(void)  jinit_huff_decoder(j_decompress_ptr cinfo);
 * while maintaining their original functional inputs/outputs.
 * 
 * 4. deprecated the function GLOBAL(void) jpeg_huff_decode();
 * 
 * Cody Wu, 06/28/2022
 * 
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdhuff.h"             /* Declarations shared with jdphuff.c */
#include "jpegcomp.h"
#include "jstdhuff.c"

/*
 * Expanded entropy decoder object for Huffman decoding.
 *
 * The savable_state subrecord contains fields that change within an MCU,
 * but must not be updated permanently until we complete the MCU.
 */

typedef struct {
  int last_dc_val[MAX_COMPS_IN_SCAN]; /* last DC coef for each component */
} savable_state;

typedef struct {
  struct jpeg_entropy_decoder pub; /* public fields */

  /* These fields are loaded into local variables at start of each MCU.
   * In case of suspension, we exit WITHOUT updating them.
   */
  bitread_perm_state bitstate;  /* Bit buffer at start of MCU */
  savable_state saved;          /* Other state at start of MCU */

  /* These fields are NOT loaded into local working state. */
  unsigned int restarts_to_go;  /* MCUs left in this restart interval */

  /* Pointers to derived tables (these workspaces have image lifespan) */
  DC_derived_tbl *dc_derived_tbls[NUM_HUFF_TBLS];
  AC_derived_tbl *ac_derived_tbls[NUM_HUFF_TBLS];

  /* Precalculated info set up by start_pass for use in decode_mcu: */

  /* Pointers to derived tables to be used for each block within an MCU */
  DC_derived_tbl *dc_cur_tbls[D_MAX_BLOCKS_IN_MCU];
  AC_derived_tbl *ac_cur_tbls[D_MAX_BLOCKS_IN_MCU];
  /* Whether we care about the DC and AC coefficient values for each block */
  boolean dc_needed[D_MAX_BLOCKS_IN_MCU];
  boolean ac_needed[D_MAX_BLOCKS_IN_MCU];
} huff_entropy_decoder;

typedef huff_entropy_decoder *huff_entropy_ptr;


/*
 * Initialize for a Huffman-compressed scan.
 */

METHODDEF(void)
start_pass_huff_decoder(j_decompress_ptr cinfo)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr)cinfo->entropy;
  int ci, blkn, dctbl, actbl;
  DC_derived_tbl **dc_pdtbl;
  AC_derived_tbl** ac_pdtbl;
  jpeg_component_info *compptr;

  /* Check that the scan parameters Ss, Se, Ah/Al are OK for sequential JPEG.
   * This ought to be an error condition, but we make it a warning because
   * there are some baseline files out there with all zeroes in these bytes.
   */
  if (cinfo->Ss != 0 || cinfo->Se != DCTSIZE2 - 1 ||
      cinfo->Ah != 0 || cinfo->Al != 0)
    WARNMS(cinfo, JWRN_NOT_SEQUENTIAL);
  /* printf("Al: %d\n", cinfo->Al); */

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    dctbl = compptr->dc_tbl_no;
    actbl = compptr->ac_tbl_no;
    /* Compute derived values for Huffman tables */
    /* We may do this more than once for a table, but it's not expensive */
    dc_pdtbl = (DC_derived_tbl **)(entropy->dc_derived_tbls) + dctbl;
    jpeg_make_d_derived_tbl(cinfo, TRUE, dctbl, dc_pdtbl);
    ac_pdtbl = (AC_derived_tbl **)(entropy->ac_derived_tbls) + actbl;
    jpeg_make_d_derived_tbl(cinfo, FALSE, actbl, ac_pdtbl);
    /* Initialize DC predictions to 0 */
    entropy->saved.last_dc_val[ci] = 0;
  }

  /* Precalculate decoding info for each block in an MCU of this scan */
  for (blkn = 0; blkn < cinfo->blocks_in_MCU; blkn++) {
    ci = cinfo->MCU_membership[blkn];
    compptr = cinfo->cur_comp_info[ci];
    /* Precalculate which table to use for each block */
    entropy->dc_cur_tbls[blkn] = entropy->dc_derived_tbls[compptr->dc_tbl_no];
    entropy->ac_cur_tbls[blkn] = entropy->ac_derived_tbls[compptr->ac_tbl_no];
    /* Decide whether we really care about the coefficient values */
    if (compptr->component_needed) {
      entropy->dc_needed[blkn] = TRUE;
      /* we don't need the ACs if producing a 1/8th-size image */
      entropy->ac_needed[blkn] = (compptr->_DCT_scaled_size > 1);
    } else {
      entropy->dc_needed[blkn] = entropy->ac_needed[blkn] = FALSE;
    }
  }

  /* Initialize bitread state variables */
  entropy->bitstate.bits_left = 0;
  entropy->bitstate.get_buffer = 0; /* unnecessary, but keeps Purify quiet */
  entropy->pub.insufficient_data = FALSE;

  /* Initialize restart counter */
  entropy->restarts_to_go = cinfo->restart_interval;
}

GLOBAL(void)
jpeg_make_d_derived_tbl(j_decompress_ptr cinfo, boolean isDC, int tblno, void** pdtbl)
{
    JHUFF_TBL* htbl;
    DC_derived_tbl* dtbl;
    AC_derived_tbl* atbl;
    unsigned lookbits, ctr, i;
    unsigned p, numsymbols;           /* position/index of a Huffman code,  valid number of Huffman symbols */
    unsigned len, minHuffLen, maxHuffLen;         /* bit length of a Huffman code,   min/max Huffman bit length */
    unsigned code, code2;                    /* a Huffman code */
    JHUFF_DEC* lookup, *look2up;
    unsigned* lookThr, * look2Thr;

    /* Note that huffsize[] and huffcode[] are filled in code-length order,
     * paralleling the order of the symbols themselves in htbl->huffval[].
     */

     /* Find the input Huffman table */
    if (tblno < 0 || tblno >= NUM_HUFF_TBLS)
        ERREXIT1(cinfo, JERR_NO_HUFF_TABLE, tblno);
    htbl =   isDC ? cinfo->dc_huff_tbl_ptrs[tblno] : cinfo->ac_huff_tbl_ptrs[tblno];
    const unsigned LOOKAHEAD = isDC ? DC_LOOKAHEAD : AC_LOOKAHEAD;
    if (htbl == NULL)
        ERREXIT1(cinfo, JERR_NO_HUFF_TABLE, tblno);

    /* Allocate a workspace if we haven't already done so. */
    if (isDC) {
        if (*pdtbl == NULL) *pdtbl = (DC_derived_tbl*)
            (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE, sizeof(DC_derived_tbl));
        dtbl = (DC_derived_tbl*)*pdtbl;
        lookup = dtbl->lookup;
        look2up = dtbl->look2up;
        lookThr = &(dtbl->lookThr);
        look2Thr = &(dtbl->look2Thr);

    }
    else {
        if (*pdtbl == NULL) *pdtbl = (AC_derived_tbl*)
            (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE, sizeof(AC_derived_tbl));
        atbl = (AC_derived_tbl*)*pdtbl;
        lookup = atbl->lookup;
        look2up = atbl->look2up;
        lookThr = &(atbl->lookThr);
        look2Thr = &(atbl->look2Thr);
    }


    /* Figure F.15: generate decoding tables for bit-sequential decoding */

    len = 16;
    while (0 == htbl->bits[len] && len > 0)
        len--;
    maxHuffLen = len;

    len = 1;   /* Skip inexistent cases until the minimum bit length */
    while (0 == htbl->bits[len] && len <= 16) {
        len++;
    }
    minHuffLen = len;    /* HUFF_LOOKAHEAD must be greater than minHuffLen for the decoder to function properly */

    /* Compute lookahead tables to speed up decoding.
     * First we set all the table entries to 0, indicating "too long";
     * then we iterate through the Huffman codes that are short enough and
     * fill in all the entries that correspond to bit sequences starting
     * with that code.
     */

    p = lookbits = 0;
    JHUFF_DEC huffDec;
    for (len = minHuffLen; len < LOOKAHEAD; len++) {
        for (i = htbl->bits[len]; i > 0; i--, p++) {
            huffDec.next = htbl->huffval[p] == 0 ? DCTSIZE2 : 1 + (htbl->huffval[p] >> 4);  /* skipping by DCTSIZE2 is equiv to break */
            huffDec.nBits = len + (htbl->huffval[p] & 15);
            huffDec.extMask = (1 << (htbl->huffval[p] & 15)) - 1;
            /* len = current code's length, p = its index in huffcode[] & huffval[]. */
            /* Generate left-justified code followed by all possible bit sequences */
            for (ctr = 1 << (LOOKAHEAD - len); ctr > 0; ctr--) {
                lookup[lookbits++] = huffDec;
            }
        }
    }
    /* Reduced computation on the last round */
    for (i = htbl->bits[LOOKAHEAD]; i > 0; i--, p++) {
        huffDec.next = htbl->huffval[p] == 0 ? DCTSIZE2 : 1 + (htbl->huffval[p] >> 4);
        huffDec.nBits = LOOKAHEAD + (htbl->huffval[p] & 15);
        huffDec.extMask = (1 << (htbl->huffval[p] & 15)) - 1;
        lookup[lookbits++] = huffDec;   /* no for-loop */
    }
    *lookThr = lookbits;

    /* for (len = 1; len <= 16; len++) printf("%d, ", htbl->bits[len]); printf("\n"); */

    code = 0;
    for (len = minHuffLen; len <= LOOKAHEAD; len++) {
        code += htbl->bits[len];
        code <<= 1;
    }
    code2 = code;

    /* Determine the minimum covering length for each prefix beyond lookThr */
    for (; len <= maxHuffLen; len++) {
        for (i = htbl->bits[len]; i > 0; i--) {
            lookup[code >> (len - LOOKAHEAD)].nBits = len;
            lookup[code >> (len - LOOKAHEAD)].next = (1 << (len - LOOKAHEAD)) - 1;  /* repurpose for extMask */
            code++;
        }
        code <<= 1;
    }

    code = code2;  /* starting Huffman code beyond HUFF_LOOKAHEAD bits */
    lookup[lookbits].extMask = 0;  /* reporpuse as offset */
    lookbits = 0;
    for (len = LOOKAHEAD + 1; len < maxHuffLen; len++) {
        for (i = htbl->bits[len]; i > 0; i--, p++) {
            ctr = 1 << (lookup[code >> (len - LOOKAHEAD)].nBits - len);
            huffDec.next = htbl->huffval[p] == 0 ? DCTSIZE2 : 1 + (htbl->huffval[p] >> 4);
            huffDec.nBits = len + (htbl->huffval[p] & 15);
            huffDec.extMask = (1 << (htbl->huffval[p] & 15)) - 1;
            while (ctr > 0) {
                ctr--;
                look2up[lookbits++] = huffDec;
            }
            ctr = 1 + (code >> (len - LOOKAHEAD));
            if (ctr < (1 << LOOKAHEAD))                /* Note the last useless ctr may cause overflowing */
                lookup[ctr].extMask = lookbits;     /* repurpose for position offset */
            code++;
        }
        code <<= 1;
    }

    /* reduced computation for the last round */
    for (i = htbl->bits[len]; i > 0; i--, p++) {
        huffDec.next = htbl->huffval[p]==0? DCTSIZE2 : 1 + (htbl->huffval[p] >> 4);
        huffDec.nBits = len + (htbl->huffval[p] & 15);
        huffDec.extMask = (1 << (htbl->huffval[p] & 15)) - 1;
        look2up[lookbits++] = huffDec;
        ctr = 1 + (code >> (len - LOOKAHEAD));
        if (ctr < (1 << LOOKAHEAD))          /* avoid overflowing */
            lookup[ctr].extMask = lookbits;     /* repurpose for starting position */
        code++;
    }
    *look2Thr = lookbits;  /* end of valid entry */
    numsymbols = p;
    if (p > 256) ERREXIT(cinfo, JERR_BAD_HUFF_TABLE);
    /* for (i = lookThr; i <= 256; i++)
        printf("\nlookup[%d] = {%d,  %d,  %d}\n", i, lookup[i].extMask, lookup[i].nBits, lookup[i].next);
    printf("Min-Len: %d,  Max-Len: %d, nSyms: %d,  LookThr: %d,  Look2Thr: %d\n", minHuffLen, maxHuffLen, p, lookThr, lookbits);
    */


    /* Validate symbols as being reasonable.
     * For AC tables, we make no check, but accept all byte values 0..255.
     * For DC tables, we require the symbols to be in range 0..15.
     * (Tighter bounds could be applied depending on the data depth and mode,
     * but this is sufficient to ensure safe decoding.)
     */
    if (isDC) {
        for (i = 0; i < numsymbols; i++) {
            int sym = htbl->huffval[i];
            if (sym < 0 || sym > 15)
                ERREXIT(cinfo, JERR_BAD_HUFF_TABLE);
        }
    }
}



/*
 * Out-of-line code for bit fetching (shared with jdphuff.c).
 * See jdhuff.h for info about usage.
 * Note: current values of get_buffer and bits_left are passed as parameters,
 * but are returned in the corresponding fields of the state struct.
 *
 * On most machines MIN_GET_BITS should be 25 to allow the full 32-bit width
 * of get_buffer to be used.  (On machines with wider words, an even larger
 * buffer could be used.)  However, on some machines 32-bit shifts are
 * quite slow and take time proportional to the number of places shifted.
 * (This is true with most PC compilers, for instance.)  In this case it may
 * be a win to set MIN_GET_BITS to the minimum value of 15.  This reduces the
 * average shift distance at the cost of more calls to jpeg_fill_bit_buffer.
 */

#ifdef SLOW_SHIFT_32
#define MIN_GET_BITS  15        /* minimum allowable value */
#else
#define MIN_GET_BITS  (BIT_BUF_SIZE - 7)
#endif


GLOBAL(boolean)
jpeg_fill_bit_buffer(bitread_working_state *state,
                     register bit_buf_type get_buffer, register int bits_left,
                     int nbits)
/* Load up the bit buffer to a depth of at least nbits */
{
  /* Copy heavily used state fields into locals (hopefully registers) */
  register const JOCTET *next_input_byte = state->next_input_byte;
  register size_t bytes_in_buffer = state->bytes_in_buffer;
  j_decompress_ptr cinfo = state->cinfo;

  /* Attempt to load at least MIN_GET_BITS bits into get_buffer. */
  /* (It is assumed that no request will be for more than that many bits.) */
  /* We fail to do so only if we hit a marker or are forced to suspend. */

  if (cinfo->unread_marker == 0) {      /* cannot advance past a marker */
    while (bits_left < MIN_GET_BITS) {
      register int c;

      /* Attempt to read a byte */
      if (bytes_in_buffer == 0) {
        if (!(*cinfo->src->fill_input_buffer) (cinfo))
          return FALSE;
        next_input_byte = cinfo->src->next_input_byte;
        bytes_in_buffer = cinfo->src->bytes_in_buffer;
      }
      bytes_in_buffer--;
      c = *next_input_byte++;

      /* If it's 0xFF, check and discard stuffed zero byte */
      if (c == 0xFF) {
        /* Loop here to discard any padding FF's on terminating marker,
         * so that we can save a valid unread_marker value.  NOTE: we will
         * accept multiple FF's followed by a 0 as meaning a single FF data
         * byte.  This data pattern is not valid according to the standard.
         */
        do {
          if (bytes_in_buffer == 0) {
            if (!(*cinfo->src->fill_input_buffer) (cinfo))
              return FALSE;
            next_input_byte = cinfo->src->next_input_byte;
            bytes_in_buffer = cinfo->src->bytes_in_buffer;
          }
          bytes_in_buffer--;
          c = *next_input_byte++;
        } while (c == 0xFF);

        if (c == 0) {
          /* Found FF/00, which represents an FF data byte */
          c = 0xFF;
        } else {
          /* Oops, it's actually a marker indicating end of compressed data.
           * Save the marker code for later use.
           * Fine point: it might appear that we should save the marker into
           * bitread working state, not straight into permanent state.  But
           * once we have hit a marker, we cannot need to suspend within the
           * current MCU, because we will read no more bytes from the data
           * source.  So it is OK to update permanent state right away.
           */
          cinfo->unread_marker = c;
          /* See if we need to insert some fake zero bits. */
          goto no_more_bytes;
        }
      }

      /* OK, load c into get_buffer */
      get_buffer = (get_buffer << 8) | c;
      bits_left += 8;
    } /* end while */
  } else {
no_more_bytes:
    /* We get here if we've read the marker that terminates the compressed
     * data segment.  There should be enough bits in the buffer register
     * to satisfy the request; if so, no problem.
     */

      /* Uh-oh.  Report corrupted data to user and stuff zeroes into
       * the data stream, so that we can produce some kind of image.
       * We use a nonvolatile flag to ensure that only one warning message
       * appears per data segment.
       */
      if (nbits > bits_left && !cinfo->entropy->insufficient_data) {
        WARNMS(cinfo, JWRN_HIT_MARKER);
        cinfo->entropy->insufficient_data = TRUE;
      }

      /* Fill the buffer with max zero bits */
      get_buffer <<= BIT_BUF_SIZE - bits_left;
      bits_left = BIT_BUF_SIZE;
  }

  /* Unload the local registers */
  state->next_input_byte = next_input_byte;
  state->bytes_in_buffer = bytes_in_buffer;
  state->get_buffer = get_buffer;
  state->bits_left = bits_left;

  return TRUE;
}


/* Macro version of the above, which performs much better but does not
   handle markers.  We have to hand off any blocks with markers to the
   slower routines. */

#define GET_BYTE { \
  register int c0, c1; \
  c0 = *buffer++; \
  c1 = *buffer; \
  /* Pre-execute most common case */ \
  get_buffer = (get_buffer << 8) | c0; \
  if (c0 == 0xFF) { \
    /* Pre-execute case of FF/00, which represents an FF data byte */ \
    buffer++; \
    if (c1 != 0) { \
      /* Oops, it's actually a marker indicating end of compressed data. */ \
      cinfo->unread_marker = c1; \
      /* Back out pre-execution and fill the buffer with zero bits */ \
      buffer -= 2; \
      get_buffer &= ~0xFF; \
    } \
  } \
}


#if SIZEOF_SIZE_T == 8 || defined(_WIN64) || (defined(__x86_64__) && defined(__ILP32__))

/* Pre-fetch 32 bits, because the holding register is 64-bit */
#define FILL_BIT_BUFFER_FAST \
  if (bits_left <= 32) { \
    bits_left +=32;  \
    GET_BYTE GET_BYTE    GET_BYTE GET_BYTE \
  }

#else

/* Pre-fetch 16 bits, because the holding register is 32-bit */
#define FILL_BIT_BUFFER_FAST \
  if (bits_left <= 16) { \
    bits_left += 16;   \
    GET_BYTE GET_BYTE \
  }

#endif


/*
 * Out-of-line code for Huffman code decoding.
 * See jdhuff.h for info about usage.
 */

GLOBAL(void)
jpeg_huff_decode(bitread_working_state *state,
                 register bit_buf_type get_buffer, register int bits_left,
                 DC_derived_tbl *htbl)
{ ; }


/*
 * Check for a restart marker & resynchronize decoder.
 * Returns FALSE if must suspend.
 */

LOCAL(boolean)
process_restart(j_decompress_ptr cinfo)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr)cinfo->entropy;
  int ci;

  /* Throw away any unused bits remaining in bit buffer; */
  /* include any full bytes in next_marker's count of discarded bytes */
  cinfo->marker->discarded_bytes += entropy->bitstate.bits_left / 8;
  entropy->bitstate.bits_left = 0;

  /* Advance past the RSTn marker */
  if (!(*cinfo->marker->read_restart_marker) (cinfo))
    return FALSE;

  /* Re-initialize DC predictions to 0 */
  for (ci = 0; ci < cinfo->comps_in_scan; ci++)
    entropy->saved.last_dc_val[ci] = 0;

  /* Reset restart counter */
  entropy->restarts_to_go = cinfo->restart_interval;

  /* Reset out-of-data flag, unless read_restart_marker left us smack up
   * against a marker.  In that case we will end up treating the next data
   * segment as empty, and we can avoid producing bogus output pixels by
   * leaving the flag set.
   */
  if (cinfo->unread_marker == 0)
    entropy->pub.insufficient_data = FALSE;

  return TRUE;
}


#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
__attribute__((no_sanitize("signed-integer-overflow"),
               no_sanitize("unsigned-integer-overflow")))
#endif
#endif

LOCAL(boolean)  decode_mcu_slow(j_decompress_ptr cinfo, JBLOCKROW *const MCU_data)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr)cinfo->entropy;
  BITREAD_STATE_VARS;
  int blkn;
  savable_state state;
  /* Outer loop handles each block in the MCU */

  /* Load up working state */
  BITREAD_LOAD_STATE(cinfo, entropy->bitstate);
  state = entropy->saved;
  register JHUFF_DEC huffRes;
  register int s, k;
  

  for (blkn = 0; blkn < cinfo->blocks_in_MCU; blkn++) {
    JBLOCKROW block = MCU_data ? MCU_data[blkn] : NULL;
    const DC_derived_tbl *const dctbl = entropy->dc_cur_tbls[blkn];
    const AC_derived_tbl *const actbl = entropy->ac_cur_tbls[blkn];
    

    /* Decode a single block's worth of coefficients */

    /* Section F.2.2.1: decode the DC coefficient difference */
    HUFF_DECODE(huffRes, br_state, DC_LOOKAHEAD, dctbl, return FALSE);
    DROP_BITS(huffRes.nBits);
    s = (get_buffer >> bits_left) & huffRes.extMask;
    s = HUFF_EXTEND(s, huffRes.extMask);
    

    if (entropy->dc_needed[blkn]) {
      /* Convert DC difference to actual value, update last_dc_val */
      int ci = cinfo->MCU_membership[blkn];
      /* Certain malformed JPEG images produce repeated DC coefficient
       * differences of 2047 or -2047, which causes state.last_dc_val[ci] to
       * grow until it overflows or underflows a 32-bit signed integer.  This
       * behavior is, to the best of our understanding, innocuous, and it is
       * unclear how to work around it without potentially affecting
       * performance.  Thus, we (hopefully temporarily) suppress UBSan integer
       * overflow errors for this function and decode_mcu_fast().
       */
      s += state.last_dc_val[ci];
      state.last_dc_val[ci] = s;
      if (MCU_data) {
        /* Output the DC coefficient (assumes jpeg_natural_order[0] = 0) */
        (*block)[0] = (JCOEF)s;
      }
    }

    k = 0;
    if (entropy->ac_needed[blkn] && MCU_data) {

        /* Section F.2.2.2: decode the AC coefficients */
        /* Since zeroes are skipped, output area must be cleared beforehand */
        do {
            HUFF_DECODE(huffRes, br_state, AC_LOOKAHEAD, actbl, return FALSE);
            k += huffRes.next;
            DROP_BITS(huffRes.nBits);

            if (huffRes.extMask) {
                s = (get_buffer >> bits_left) & huffRes.extMask;
                /* Output coefficient in natural (dezigzagged) order.
                * Note: the extra entries in jpeg_natural_order[] will save us
                * if k >= DCTSIZE2, which could happen if the data is corrupted.
                */
                (*block)[jpeg_natural_order[k]] = (JCOEF)HUFF_EXTEND(s, huffRes.extMask);
            }
        } while (k < DCTSIZE2 - 1);
    } else {
      /* Section F.2.2.2: decode the AC coefficients */
      /* In this path we just discard the values */
      do {
        HUFF_DECODE(huffRes, br_state, AC_LOOKAHEAD, actbl, return FALSE);
        DROP_BITS(huffRes.nBits);
        k += huffRes.next;
      } while (k < DCTSIZE2 - 1);
    }
  }

  /* Completed MCU, so update state */
  BITREAD_SAVE_STATE(cinfo, entropy->bitstate);
  entropy->saved = state;
  return TRUE;
}


#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
__attribute__((no_sanitize("signed-integer-overflow"),
               no_sanitize("unsigned-integer-overflow")))
#endif
#endif
LOCAL(boolean)  decode_mcu_fast(j_decompress_ptr cinfo, JBLOCKROW *const MCU_data)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr)cinfo->entropy;
  BITREAD_STATE_VARS;
  JOCTET *buffer;
  int blkn, k; 
  savable_state state;
  /* Outer loop handles each block in the MCU */

  /* Load up working state */
  BITREAD_LOAD_STATE(cinfo, entropy->bitstate);
  buffer = (JOCTET *)br_state.next_input_byte;
  state = entropy->saved;
  register int s;
  register JHUFF_DEC huffRes;

  for (blkn = 0; blkn < cinfo->blocks_in_MCU; blkn++) {
    JBLOCKROW block = MCU_data ? MCU_data[blkn] : NULL;
    const DC_derived_tbl* const dctbl = entropy->dc_cur_tbls[blkn];
    const AC_derived_tbl* const actbl = entropy->ac_cur_tbls[blkn];
    
    HUFF_DECODE_FAST(huffRes, DC_LOOKAHEAD, dctbl);
    DROP_BITS(huffRes.nBits);
    s = (get_buffer >> bits_left)  & huffRes.extMask;
    s = HUFF_EXTEND(s, huffRes.extMask);

    if (entropy->dc_needed[blkn]) {
      int ci = cinfo->MCU_membership[blkn];
      /* Refer to the comment in decode_mcu_slow() regarding the supression of
       * a UBSan integer overflow error in this line of code.
       */
      s += state.last_dc_val[ci];
      state.last_dc_val[ci] = s;
      if (MCU_data)
        (*block)[0] = (JCOEF)s;
    }

    k = 0;
    if (entropy->ac_needed[blkn] && MCU_data) {
      do {
        HUFF_DECODE_FAST(huffRes, AC_LOOKAHEAD, actbl);
        k += huffRes.next;
        DROP_BITS(huffRes.nBits);
        if (huffRes.extMask) {
            s = (get_buffer >> bits_left) & huffRes.extMask;
            (*block)[jpeg_natural_order[k]] = (JCOEF)HUFF_EXTEND(s, huffRes.extMask);
        }
      } while (k < DCTSIZE2 - 1);
    } else {
      do {
        HUFF_DECODE_FAST(huffRes, AC_LOOKAHEAD, actbl);
        DROP_BITS(huffRes.nBits);
        k += huffRes.next;
      } while (k < DCTSIZE2 - 1);
    }
  }

  if (cinfo->unread_marker != 0) {
    cinfo->unread_marker = 0;
    return FALSE;
  }

  br_state.bytes_in_buffer -= (buffer - br_state.next_input_byte);
  br_state.next_input_byte = buffer;
  BITREAD_SAVE_STATE(cinfo, entropy->bitstate);
  entropy->saved = state;
  return TRUE;
}


/*
 * Decode and return one MCU's worth of Huffman-compressed coefficients.
 * The coefficients are reordered from zigzag order into natural array order,
 * but are not dequantized.
 *
 * The i'th block of the MCU is stored into the block pointed to by
 * MCU_data[i].  WE ASSUME THIS AREA HAS BEEN ZEROED BY THE CALLER.
 * (Wholesale zeroing is usually a little faster than retail...)
 *
 * Returns FALSE if data source requested suspension.  In that case no
 * changes have been made to permanent state.  (Exception: some output
 * coefficients may already have been assigned.  This is harmless for
 * this module, since we'll just re-assign them on the next call.)
 */

#define BUFSIZE  (DCTSIZE2 * 8)

METHODDEF(boolean)  decode_mcu(j_decompress_ptr cinfo, JBLOCKROW *MCU_data)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr)cinfo->entropy;
  int usefast = 1;

  /* Process restart marker if needed; may have to suspend */
  if (cinfo->restart_interval) {
    if (entropy->restarts_to_go == 0)
      if (!process_restart(cinfo))
        return FALSE;
    usefast = 0;
  }

  if (cinfo->src->bytes_in_buffer < BUFSIZE * (size_t)cinfo->blocks_in_MCU || cinfo->unread_marker != 0)
    usefast = 0;

  /* If we've run out of data, just leave the MCU set to zeroes.
   * This way, we return uniform gray for the remainder of the segment.
   */
  if (!entropy->pub.insufficient_data) {

    if (usefast) {
      if (!decode_mcu_fast(cinfo, MCU_data)) goto use_slow;
    } else {
use_slow:
      if (!decode_mcu_slow(cinfo, MCU_data)) return FALSE;
    }
  }

  /* Account for restart interval (no-op if not using restarts) */
  if (cinfo->restart_interval)
    entropy->restarts_to_go--;

  return TRUE;
}


/*
 * Module initialization routine for Huffman entropy decoding.
 */

GLOBAL(void)
jinit_huff_decoder(j_decompress_ptr cinfo)
{
  huff_entropy_ptr entropy;
  int i;

  /* Motion JPEG frames typically do not include the Huffman tables if they
     are the default tables.  Thus, if the tables are not set by the time
     the Huffman decoder is initialized (usually within the body of
     jpeg_start_decompress()), we set them to default values. */
  std_huff_tables((j_common_ptr)cinfo);

  entropy = (huff_entropy_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                sizeof(huff_entropy_decoder));
  cinfo->entropy = (struct jpeg_entropy_decoder *)entropy;
  entropy->pub.start_pass = start_pass_huff_decoder;
  entropy->pub.decode_mcu = decode_mcu;

  /* Mark tables unallocated */
  for (i = 0; i < NUM_HUFF_TBLS; i++) {
      entropy->dc_derived_tbls[i] = NULL;
      entropy->ac_derived_tbls[i] = NULL;
  }
}
