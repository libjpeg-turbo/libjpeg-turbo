/*
 * jchuff-neon.c - Huffman encoding (Arm Neon)
 *
 * Copyright (C) 2020 Arm Limited. All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * NOTE: All referenced figures are from
 * Recommendation ITU-T T.81 (1992) | ISO/IEC 10918-1:1994.
 */

#define JPEG_INTERNALS
#include "../../../jconfigint.h"
#include "../../../jinclude.h"
#include "../../../jpeglib.h"
#include "../../../jsimd.h"
#include "../../../jdct.h"
#include "../../../jsimddct.h"
#include "../../jsimd.h"

#include <limits.h>

#include <arm_neon.h>

/* Expanded entropy encoder object for Huffman encoding.
 *
 * The savable_state subrecord contains fields that change within an MCU,
 * but must not be updated permanently until we complete the MCU.
 */

#define BIT_BUF_SIZE  64

typedef struct {
  size_t put_buffer;                    /* current bit accumulation buffer */
  int free_bits;                        /* # of bits available in it */
  int last_dc_val[MAX_COMPS_IN_SCAN];   /* last DC coef for each component */
} savable_state;

typedef struct {
  JOCTET *next_output_byte;     /* => next byte to write in buffer */
  size_t free_in_buffer;        /* # of byte spaces remaining in buffer */
  savable_state cur;            /* Current bit buffer & DC state */
  j_compress_ptr cinfo;         /* dump_buffer needs access to this */
  int simd;
} working_state;

/* Outputting bits to the file */

/* Output byte b and, speculatively, an additional 0 byte. 0xFF must be encoded
 * as 0xFF 0x00, so the output buffer pointer is advanced by 2 if the byte is
 * 0xFF. Otherwise, the output buffer pointer is advanced by 1, and the
 * speculative 0 byte will be overwritten by the next byte.
 */
#define EMIT_BYTE(b) { \
  buffer[0] = (JOCTET)(b); \
  buffer[1] = 0; \
  buffer -= -2 + ((JOCTET)(b) < 0xFF); \
}

/* Output the entire bit buffer. If there are no 0xFF bytes in it, then write
 * directly to the output buffer. Otherwise, use the EMIT_BYTE() macro to
 * encode 0xFF as 0xFF 0x00.
 */
#define FLUSH() { \
  if (put_buffer & 0x8080808080808080 & ~(put_buffer + 0x0101010101010101)) { \
    EMIT_BYTE(put_buffer >> 56) \
    EMIT_BYTE(put_buffer >> 48) \
    EMIT_BYTE(put_buffer >> 40) \
    EMIT_BYTE(put_buffer >> 32) \
    EMIT_BYTE(put_buffer >> 24) \
    EMIT_BYTE(put_buffer >> 16) \
    EMIT_BYTE(put_buffer >>  8) \
    EMIT_BYTE(put_buffer      ) \
  } else { \
    __asm__("rev %x0, %x1" : "=r"(put_buffer) : "r"(put_buffer)); \
    *((uint64_t *)buffer) = put_buffer; \
    buffer += 8; \
  } \
}

/* Fill the bit buffer to capacity with the leading bits from code, then output
 * the bit buffer and put the remaining bits from code into the bit buffer.
 */
#define PUT_AND_FLUSH(code, size) { \
  put_buffer = (put_buffer << (size + free_bits)) | (code >> -free_bits); \
  FLUSH() \
  free_bits += BIT_BUF_SIZE; \
  put_buffer = code; \
}

/* Insert code into the bit buffer and output the bit buffer if needed.
 * NOTE: We can't flush with free_bits == 0, since the left shift in
 * PUT_AND_FLUSH() would have undefined behavior.
 */
#define PUT_BITS(code, size) { \
  free_bits -= size; \
  if (free_bits < 0) \
    PUT_AND_FLUSH(code, size) \
  else \
    put_buffer = (put_buffer << size) | code; \
}

#define PUT_CODE(code, size, diff) { \
  diff |= code << nbits; \
  nbits += size; \
  PUT_BITS(diff, nbits) \
}

ALIGN(16) static const uint8_t jsimd_huff_encode_one_block_consts[] = {
    0,   1,   2,   3,  16,  17,  32,  33,
   18,  19,   4,   5,   6,   7,  20,  21,
   34,  35,  48,  49, 255, 255,  50,  51,
   36,  37,  22,  23,   8,   9,  10,  11,
  255, 255,   6,   7,  20,  21,  34,  35,
   48,  49, 255, 255,  50,  51,  36,  37,
   54,  55,  40,  41,  26,  27,  12,  13,
   14,  15,  28,  29,  42,  43,  56,  57,
    6,   7,  20,  21,  34,  35,  48,  49,
   50,  51,  36,  37,  22,  23,   8,   9,
   26,  27,  12,  13, 255, 255,  14,  15,
   28,  29,  42,  43,  56,  57, 255, 255,
   52,  53,  54,  55,  40,  41,  26,  27,
   12,  13, 255, 255,  14,  15,  28,  29,
   26,  27,  40,  41,  42,  43,  28,  29,
   14,  15,  30,  31,  44,  45,  46,  47
};

JOCTET *jsimd_huff_encode_one_block_neon(void *state,
                                         JOCTET *buffer,
                                         JCOEFPTR block,
                                         int last_dc_val,
                                         c_derived_tbl *dctbl,
                                         c_derived_tbl *actbl)
{
  uint16_t block_diff[DCTSIZE2];

  /* Load lookup table indices for rows of zig-zag ordering. */
#if defined(__clang__) || defined(_MSC_VER)
  const uint8x16x4_t idx_rows_0123 =
                vld1q_u8_x4(jsimd_huff_encode_one_block_consts + 0 * DCTSIZE);
  const uint8x16x4_t idx_rows_4567 =
                vld1q_u8_x4(jsimd_huff_encode_one_block_consts + 8 * DCTSIZE);
#else
  /* GCC does not currently support intrinsics vl1dq_<type>_x4(). */
  const uint8x16x4_t idx_rows_0123 = {
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 0 * DCTSIZE),
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 2 * DCTSIZE),
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 4 * DCTSIZE),
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 6 * DCTSIZE)
                };
  const uint8x16x4_t idx_rows_4567 = {
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 8 * DCTSIZE),
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 10 * DCTSIZE),
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 12 * DCTSIZE),
                  vld1q_u8(jsimd_huff_encode_one_block_consts + 14 * DCTSIZE)
                };
#endif

  /* Load 8x8 block of DCT coefficients. */
#if defined(__clang__) || defined(_MSC_VER)
  const int8x16x4_t tbl_rows_0123 =
                              vld1q_s8_x4((int8_t *)(block + 0 * DCTSIZE));
  const int8x16x4_t tbl_rows_4567 =
                              vld1q_s8_x4((int8_t *)(block + 4 * DCTSIZE));
#else
  const int8x16x4_t tbl_rows_0123 = {
                              vld1q_s8((int8_t *)(block + 0 * DCTSIZE)),
                              vld1q_s8((int8_t *)(block + 1 * DCTSIZE)),
                              vld1q_s8((int8_t *)(block + 2 * DCTSIZE)),
                              vld1q_s8((int8_t *)(block + 3 * DCTSIZE))
                            };
  const int8x16x4_t tbl_rows_4567 = {
                              vld1q_s8((int8_t *)(block + 4 * DCTSIZE)),
                              vld1q_s8((int8_t *)(block + 5 * DCTSIZE)),
                              vld1q_s8((int8_t *)(block + 6 * DCTSIZE)),
                              vld1q_s8((int8_t *)(block + 7 * DCTSIZE))
                            };
#endif

  /* Initialise extra lookup tables. */
  const int8x16x4_t tbl_rows_2345 = { tbl_rows_0123.val[2],
                                      tbl_rows_0123.val[3],
                                      tbl_rows_4567.val[0],
                                      tbl_rows_4567.val[1]
                                    };
  const int8x16x3_t tbl_rows_567 = { tbl_rows_4567.val[1],
                                     tbl_rows_4567.val[2],
                                     tbl_rows_4567.val[3]
                                   };

  /* Shuffle coefficients into zig-zag order. */
  int16x8_t row0 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123,
                                                   idx_rows_0123.val[0]));
  int16x8_t row1 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123,
                                                   idx_rows_0123.val[1]));
  int16x8_t row2 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345,
                                                   idx_rows_0123.val[2]));
  int16x8_t row3 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123,
                                                   idx_rows_0123.val[3]));
  int16x8_t row4 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567,
                                                   idx_rows_4567.val[0]));
  int16x8_t row5 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345,
                                                   idx_rows_4567.val[1]));
  int16x8_t row6 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567,
                                                   idx_rows_4567.val[2]));
  int16x8_t row7 = vreinterpretq_s16_s8(vqtbl3q_s8(tbl_rows_567,
                                                   idx_rows_4567.val[3]));

  /* Compute DC coefficient difference value (F.1.1.5.1). */
  row0 = vsetq_lane_s16(block[0] - last_dc_val, row0, 0);
  /* Initialize AC coefficient lanes not reachable by lookup tables. */
  row1 = vsetq_lane_s16(vgetq_lane_s16(
                vreinterpretq_s16_s8(tbl_rows_4567.val[0]), 0), row1, 2);
  row2 = vsetq_lane_s16(vgetq_lane_s16(
                vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 4), row2, 0);
  row2 = vsetq_lane_s16(vgetq_lane_s16(
                vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 0), row2, 5);
  row5 = vsetq_lane_s16(vgetq_lane_s16(
                vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 7), row5, 2);
  row5 = vsetq_lane_s16(vgetq_lane_s16(
                vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 3), row5, 7);
  row6 = vsetq_lane_s16(vgetq_lane_s16(
                vreinterpretq_s16_s8(tbl_rows_0123.val[3]), 7), row6, 5);

  /* DCT block now in zig-zag order, start Huffman encoding process. */
  int16x8_t abs_row0 = vabsq_s16(row0);
  int16x8_t abs_row1 = vabsq_s16(row1);
  int16x8_t abs_row2 = vabsq_s16(row2);
  int16x8_t abs_row3 = vabsq_s16(row3);
  int16x8_t abs_row4 = vabsq_s16(row4);
  int16x8_t abs_row5 = vabsq_s16(row5);
  int16x8_t abs_row6 = vabsq_s16(row6);
  int16x8_t abs_row7 = vabsq_s16(row7);

  /* For negative coeffs: diff = abs(coeff) -1 = ~abs(coeff). */
  uint16x8_t row0_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row0, vshrq_n_s16(row0, 15)));
  uint16x8_t row1_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row1, vshrq_n_s16(row1, 15)));
  uint16x8_t row2_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row2, vshrq_n_s16(row2, 15)));
  uint16x8_t row3_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row3, vshrq_n_s16(row3, 15)));
  uint16x8_t row4_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row4, vshrq_n_s16(row4, 15)));
  uint16x8_t row5_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row5, vshrq_n_s16(row5, 15)));
  uint16x8_t row6_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row6, vshrq_n_s16(row6, 15)));
  uint16x8_t row7_diff = vreinterpretq_u16_s16(
                              veorq_s16(abs_row7, vshrq_n_s16(row7, 15)));

  /* Construct bitmap to accelerate encoding of AC coefficients. */
  /* A set bit denotes the corresponding coefficient != 0. */
  uint8x8_t abs_row0_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row0),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row1_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row1),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row2_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row2),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row3_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row3),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row4_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row4),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row5_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row5),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row6_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row6),
                                               vdupq_n_u16(0)));
  uint8x8_t abs_row7_gt0 = vmovn_u16(vcgtq_u16(vreinterpretq_u16_s16(abs_row7),
                                               vdupq_n_u16(0)));

  const uint8x8_t bitmap_mask = { 0x80, 0x40, 0x20, 0x10,
                                  0x08, 0x04, 0x02, 0x01
                                };

  abs_row0_gt0 = vand_u8(abs_row0_gt0, bitmap_mask);
  abs_row1_gt0 = vand_u8(abs_row1_gt0, bitmap_mask);
  abs_row2_gt0 = vand_u8(abs_row2_gt0, bitmap_mask);
  abs_row3_gt0 = vand_u8(abs_row3_gt0, bitmap_mask);
  abs_row4_gt0 = vand_u8(abs_row4_gt0, bitmap_mask);
  abs_row5_gt0 = vand_u8(abs_row5_gt0, bitmap_mask);
  abs_row6_gt0 = vand_u8(abs_row6_gt0, bitmap_mask);
  abs_row7_gt0 = vand_u8(abs_row7_gt0, bitmap_mask);

  uint8x8_t bitmap_rows_10 = vpadd_u8(abs_row1_gt0, abs_row0_gt0);
  uint8x8_t bitmap_rows_32 = vpadd_u8(abs_row3_gt0, abs_row2_gt0);
  uint8x8_t bitmap_rows_54 = vpadd_u8(abs_row5_gt0, abs_row4_gt0);
  uint8x8_t bitmap_rows_76 = vpadd_u8(abs_row7_gt0, abs_row6_gt0);
  uint8x8_t bitmap_rows_3210 = vpadd_u8(bitmap_rows_32, bitmap_rows_10);
  uint8x8_t bitmap_rows_7654 = vpadd_u8(bitmap_rows_76, bitmap_rows_54);
  uint8x8_t bitmap_all = vpadd_u8(bitmap_rows_7654, bitmap_rows_3210);

  /* Shift left to remove DC bit. */
  bitmap_all = vreinterpret_u8_u64(
                        vshl_n_u64(vreinterpret_u64_u8(bitmap_all), 1));
  /* Count bits set (number of non-zero coefficients) in bitmap. */
  unsigned int non_zero_coefficients = vaddv_u8(vcnt_u8(bitmap_all));
  /* Move bitmap to 64-bit scalar register. */
  uint64_t bitmap = vget_lane_u64(vreinterpret_u64_u8(bitmap_all), 0);

  /* Setup state and bit buffer for output bitstream. */
  working_state *state_ptr = (working_state *)state;
  int free_bits = state_ptr->cur.free_bits;
  size_t put_buffer = state_ptr->cur.put_buffer;

  /* Encode DC coefficient. */
  /* Find nbits required to specify sign and amplitude of coefficient. */
  unsigned int lz;
  __asm__("clz %w0, %w1" : "=r"(lz) : "r"(vgetq_lane_s16(abs_row0, 0)));
  unsigned int nbits = 32 - lz;
  /* Emit Huffman-coded symbol and additional diff bits. */
  unsigned int diff = (unsigned int)(vgetq_lane_u16(row0_diff, 0) << lz) >> lz;
  PUT_CODE(dctbl->ehufco[nbits], dctbl->ehufsi[nbits], diff)

  /* Encode AC coefficients. */
  unsigned int r = 0; /* r = run length of zeros. */
  unsigned int i = 1; /* i = number of coefficients encoded. */
  /* Code and size information for a run length of 16 zero coefficients. */
  const unsigned int code_0xf0 = actbl->ehufco[0xf0];
  const unsigned int size_0xf0 = actbl->ehufsi[0xf0];

  /* The most efficient method of computing nbits and diff depends on the */
  /* number of non-zero coefficients. If the bitmap is not too sparse (> 8 */
  /* non-zero AC coefficients) it is beneficial to use NEON; else we compute */
  /* nbits and diff on demand using scalar code. */
  if (non_zero_coefficients > 8) {
    uint8_t block_nbits[DCTSIZE2];

    int16x8_t row0_lz = vclzq_s16(abs_row0);
    int16x8_t row1_lz = vclzq_s16(abs_row1);
    int16x8_t row2_lz = vclzq_s16(abs_row2);
    int16x8_t row3_lz = vclzq_s16(abs_row3);
    int16x8_t row4_lz = vclzq_s16(abs_row4);
    int16x8_t row5_lz = vclzq_s16(abs_row5);
    int16x8_t row6_lz = vclzq_s16(abs_row6);
    int16x8_t row7_lz = vclzq_s16(abs_row7);
    /* Compute nbits needed to specify magnitude of each coefficient. */
    uint8x8_t row0_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row0_lz)));
    uint8x8_t row1_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row1_lz)));
    uint8x8_t row2_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row2_lz)));
    uint8x8_t row3_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row3_lz)));
    uint8x8_t row4_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row4_lz)));
    uint8x8_t row5_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row5_lz)));
    uint8x8_t row6_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row6_lz)));
    uint8x8_t row7_nbits = vsub_u8(vdup_n_u8(16),
                                   vmovn_u16(vreinterpretq_u16_s16(row7_lz)));
    /* Store nbits. */
    vst1_u8(block_nbits + 0 * DCTSIZE, row0_nbits);
    vst1_u8(block_nbits + 1 * DCTSIZE, row1_nbits);
    vst1_u8(block_nbits + 2 * DCTSIZE, row2_nbits);
    vst1_u8(block_nbits + 3 * DCTSIZE, row3_nbits);
    vst1_u8(block_nbits + 4 * DCTSIZE, row4_nbits);
    vst1_u8(block_nbits + 5 * DCTSIZE, row5_nbits);
    vst1_u8(block_nbits + 6 * DCTSIZE, row6_nbits);
    vst1_u8(block_nbits + 7 * DCTSIZE, row7_nbits);
    /* Mask bits not required to specify sign and amplitude of diff. */
    row0_diff = vshlq_u16(row0_diff, row0_lz);
    row1_diff = vshlq_u16(row1_diff, row1_lz);
    row2_diff = vshlq_u16(row2_diff, row2_lz);
    row3_diff = vshlq_u16(row3_diff, row3_lz);
    row4_diff = vshlq_u16(row4_diff, row4_lz);
    row5_diff = vshlq_u16(row5_diff, row5_lz);
    row6_diff = vshlq_u16(row6_diff, row6_lz);
    row7_diff = vshlq_u16(row7_diff, row7_lz);
    row0_diff = vshlq_u16(row0_diff, vnegq_s16(row0_lz));
    row1_diff = vshlq_u16(row1_diff, vnegq_s16(row1_lz));
    row2_diff = vshlq_u16(row2_diff, vnegq_s16(row2_lz));
    row3_diff = vshlq_u16(row3_diff, vnegq_s16(row3_lz));
    row4_diff = vshlq_u16(row4_diff, vnegq_s16(row4_lz));
    row5_diff = vshlq_u16(row5_diff, vnegq_s16(row5_lz));
    row6_diff = vshlq_u16(row6_diff, vnegq_s16(row6_lz));
    row7_diff = vshlq_u16(row7_diff, vnegq_s16(row7_lz));
    /* Store diff bits. */
    vst1q_u16(block_diff + 0 * DCTSIZE, row0_diff);
    vst1q_u16(block_diff + 1 * DCTSIZE, row1_diff);
    vst1q_u16(block_diff + 2 * DCTSIZE, row2_diff);
    vst1q_u16(block_diff + 3 * DCTSIZE, row3_diff);
    vst1q_u16(block_diff + 4 * DCTSIZE, row4_diff);
    vst1q_u16(block_diff + 5 * DCTSIZE, row5_diff);
    vst1q_u16(block_diff + 6 * DCTSIZE, row6_diff);
    vst1q_u16(block_diff + 7 * DCTSIZE, row7_diff);

    while (bitmap != 0) {
      r = __builtin_clzl(bitmap);
      i += r;
      bitmap <<= r;
      nbits = block_nbits[i];
      diff = block_diff[i];
      while (r > 15) {
        /* If run length > 15, emit special run-length-16 codes. */
        PUT_BITS(code_0xf0, size_0xf0)
        r -= 16;
      }
      /* Emit Huffman symbol for run length / number of bits. (F.1.2.2.1) */
      unsigned int rs = (r << 4) + nbits;
      PUT_CODE(actbl->ehufco[rs], actbl->ehufsi[rs], diff)
      i++;
      bitmap <<= 1;
    }
  } else if (bitmap != 0) {
    uint16_t block_abs[DCTSIZE2];
    /* Store absolute value of coefficients. */
    vst1q_u16(block_abs + 0 * DCTSIZE, vreinterpretq_u16_s16(abs_row0));
    vst1q_u16(block_abs + 1 * DCTSIZE, vreinterpretq_u16_s16(abs_row1));
    vst1q_u16(block_abs + 2 * DCTSIZE, vreinterpretq_u16_s16(abs_row2));
    vst1q_u16(block_abs + 3 * DCTSIZE, vreinterpretq_u16_s16(abs_row3));
    vst1q_u16(block_abs + 4 * DCTSIZE, vreinterpretq_u16_s16(abs_row4));
    vst1q_u16(block_abs + 5 * DCTSIZE, vreinterpretq_u16_s16(abs_row5));
    vst1q_u16(block_abs + 6 * DCTSIZE, vreinterpretq_u16_s16(abs_row6));
    vst1q_u16(block_abs + 7 * DCTSIZE, vreinterpretq_u16_s16(abs_row7));
    /* Store diff bits. */
    vst1q_u16(block_diff + 0 * DCTSIZE, row0_diff);
    vst1q_u16(block_diff + 1 * DCTSIZE, row1_diff);
    vst1q_u16(block_diff + 2 * DCTSIZE, row2_diff);
    vst1q_u16(block_diff + 3 * DCTSIZE, row3_diff);
    vst1q_u16(block_diff + 4 * DCTSIZE, row4_diff);
    vst1q_u16(block_diff + 5 * DCTSIZE, row5_diff);
    vst1q_u16(block_diff + 6 * DCTSIZE, row6_diff);
    vst1q_u16(block_diff + 7 * DCTSIZE, row7_diff);

    /* Same as above but must mask diff bits and compute nbits on demand. */
    while (bitmap != 0) {
      r = __builtin_clzl(bitmap);
      i += r;
      bitmap <<= r;
      lz = __builtin_clz(block_abs[i]);
      nbits = 32 - lz;
      diff = (unsigned int)(block_diff[i] << lz) >> lz;
      while (r > 15) {
        /* If run length > 15, emit special run-length-16 codes. */
        PUT_BITS(code_0xf0, size_0xf0)
        r -= 16;
      }
      /* Emit Huffman symbol for run length / number of bits. (F.1.2.2.1) */
      unsigned int rs = (r << 4) + nbits;
      PUT_CODE(actbl->ehufco[rs], actbl->ehufsi[rs], diff)
      i++;
      bitmap <<= 1;
    }
  }

  /* If the last coefficient(s) were zero, emit an end-of-block (EOB) code. */
  /* The value of RS for the EOB code is 0. */
  if (i != 64) {
    PUT_BITS(actbl->ehufco[0], actbl->ehufsi[0])
  }

  state_ptr->cur.put_buffer = put_buffer;
  state_ptr->cur.free_bits = free_bits;

  return buffer;
}
