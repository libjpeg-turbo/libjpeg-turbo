/*
 * MIPS MSA optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * All rights reserved.
 * Authors: Vikram Dattu (vikram.dattu@imgtec.com)
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
 */

#ifndef __JMACROS_MSA_H__
#define __JMACROS_MSA_H__

#include <stdint.h>
#include <msa.h>


#ifdef CLANG_BUILD
#define MSA_ADDVI_B(a, b)  __msa_addvi_b((v16i8)a, b)
#define MSA_ADDVI_H(a, b)  __msa_addvi_h((v8i16)a, b)
#define MSA_ADDVI_W(a, b)  __msa_addvi_w((v4i32)a, b)
#define MSA_SLLI_B(a, b)   __msa_slli_b((v16i8)a, b)
#define MSA_SLLI_H(a, b)   __msa_slli_h((v8i16)a, b)
#define MSA_SLLI_W(a, b)   __msa_slli_w((v4i32)a, b)
#define MSA_SRAI_B(a, b)   __msa_srai_b((v16i8)a, b)
#define MSA_SRAI_H(a, b)   __msa_srai_h((v8i16)a, b)
#define MSA_SRAI_W(a, b)   __msa_srai_w((v4i32)a, b)
#else
#define MSA_ADDVI_B(a, b)  (a + b)
#define MSA_ADDVI_H(a, b)  (a + b)
#define MSA_ADDVI_W(a, b)  (a + b)
#define MSA_SLLI_B(a, b)   (a << b)
#define MSA_SLLI_H(a, b)   (a << b)
#define MSA_SLLI_W(a, b)   (a << b)
#define MSA_SRAI_B(a, b)   (a >> b)
#define MSA_SRAI_H(a, b)   (a >> b)
#define MSA_SRAI_W(a, b)   (a >> b)
#endif

#define LD_B(RTYPE, psrc)  *((RTYPE *)(psrc))
#define LD_UB(...)  LD_B(v16u8, __VA_ARGS__)
#define LD_SB(...)  LD_B(v16i8, __VA_ARGS__)

#define LD_H(RTYPE, psrc)  *((RTYPE *)(psrc))
#define LD_SH(...)  LD_H(v8i16, __VA_ARGS__)

#define ST_B(RTYPE, in, pdst)  *((RTYPE *)(pdst)) = (in)
#define ST_UB(...)  ST_B(v16u8, __VA_ARGS__)
#define ST_SB(...)  ST_B(v16i8, __VA_ARGS__)

#define ST_H(RTYPE, in, pdst)  *((RTYPE *)(pdst)) = (in)

#ifdef CLANG_BUILD
#define LH(psrc) \
( { \
  unsigned char *psrc_lh_m = (unsigned char *)(psrc); \
  unsigned short val_m; \
  \
  asm volatile( \
    "lh %[val_m], %[psrc_lh_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_lh_m] "m" (*psrc_lh_m) \
  ); \
  \
  val_m; \
} )

#define LW(psrc) \
( { \
  unsigned char *psrc_lw_m = (unsigned char *)(psrc); \
  unsigned int val_m; \
  \
  asm volatile( \
    "lw %[val_m], %[psrc_lw_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_lw_m] "m" (*psrc_lw_m) \
  ); \
  \
  val_m; \
} )

#if (__mips == 64)
#define LD(psrc) \
( { \
  unsigned char *psrc_ld_m = (unsigned char *)(psrc); \
  uint64_t val_m = 0; \
  \
  asm volatile( \
    "ld %[val_m], %[psrc_ld_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_ld_m] "m" (*psrc_ld_m) \
  ); \
  \
  val_m; \
} )
#else /* !(__mips == 64) */
#define LD(psrc) \
( { \
  unsigned char *psrc_ld_m = (unsigned char *)(psrc); \
  unsigned int val0_m, val1_m; \
  uint64_t val_m = 0; \
  \
  val0_m = LW(psrc_ld_m); \
  val1_m = LW(psrc_ld_m + 4); \
  \
  val_m = (uint64_t)(val1_m); \
  val_m = (uint64_t)((val_m << 32) & 0xFFFFFFFF00000000); \
  val_m = (uint64_t)(val_m | (uint64_t)val0_m); \
  \
  val_m; \
} )
#endif

#define SH(val, pdst) { \
  unsigned char *pdst_sh_m = (unsigned char *)(pdst); \
  unsigned short val_m = (val); \
  \
  asm volatile( \
    "sh %[val_m], %[pdst_sh_m] \n\t" : \
    [pdst_sh_m] "=m" (*pdst_sh_m) : \
    [val_m] "r" (val_m) \
  ); \
}

#define SW(val, pdst) { \
  unsigned char *pdst_sw_m = (unsigned char *)(pdst); \
  unsigned int val_m = (val); \
  \
  asm volatile( \
    "sw %[val_m], %[pdst_sw_m] \n\t" : \
    [pdst_sw_m] "=m" (*pdst_sw_m) : \
    [val_m] "r" (val_m) \
  ); \
}

#if (__mips == 64)
#define SD(val, pdst) { \
  unsigned char *pdst_sd_m = (unsigned char *)(pdst); \
  uint64_t val_m = (val); \
  \
  asm volatile( \
    "sd %[val_m], %[pdst_sd_m] \n\t" : \
    [pdst_sd_m] "=m" (*pdst_sd_m) : \
    [val_m] "r" (val_m) \
  ); \
}
#else
#define SD(val, pdst) { \
  unsigned char *pdst_sd_m = (unsigned char *)(pdst); \
  unsigned int val0_m, val1_m; \
  \
  val0_m = (unsigned int)((val) & 0x00000000FFFFFFFF); \
  val1_m = (unsigned int)(((val) >> 32) & 0x00000000FFFFFFFF); \
  \
  SW(val0_m, pdst_sd_m); \
  SW(val1_m, pdst_sd_m + 4); \
}
#endif

#define SW_ZERO(pdst) { \
  unsigned char *pdst_m = (unsigned char *)(pdst); \
  \
  asm volatile( \
    "sw $0, %[pdst_m] \n\t" : \
    [pdst_m] "=m" (*pdst_m) : \
    \
  ); \
}

#else /* #ifndef CLANG_BUILD */

#if (__mips_isa_rev >= 6)
#define LW(psrc) \
( { \
  unsigned char *psrc_lw_m = (unsigned char *)(psrc); \
  unsigned int val_m; \
  \
  asm volatile( \
    "lw %[val_m], %[psrc_lw_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_lw_m] "m" (*psrc_lw_m) \
  ); \
  \
  val_m; \
} )

#if (__mips == 64)
#define LD(psrc) \
( { \
  unsigned char *psrc_ld_m = (unsigned char *)(psrc); \
  unsigned long long val_m = 0; \
  \
  asm volatile( \
    "ld %[val_m], %[psrc_ld_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_ld_m] "m" (*psrc_ld_m) \
  ); \
  \
  val_m; \
} )
#else /* !(__mips == 64) */
#define LD(psrc) \
( { \
  unsigned char *psrc_ld_m = (unsigned char *)(psrc); \
  unsigned int val0_m, val1_m; \
  unsigned long long val_m = 0; \
  \
  val0_m = LW(psrc_ld_m); \
  val1_m = LW(psrc_ld_m + 4); \
  \
  val_m = (unsigned long long)(val1_m); \
  val_m = (unsigned long long)((val_m << 32) & 0xFFFFFFFF00000000); \
  val_m = (unsigned long long)(val_m | (unsigned long long)val0_m); \
  \
  val_m; \
} )
#endif

#define SW(val, pdst) { \
  unsigned char *pdst_sw_m = (unsigned char *)(pdst); \
  unsigned int val_m = (val); \
  \
  asm volatile( \
    "sw %[val_m], %[pdst_sw_m] \n\t" : \
    [pdst_sw_m] "=m" (*pdst_sw_m) : \
    [val_m] "r" (val_m) \
  ); \
}

#define SD(val, pdst) { \
  unsigned char *pdst_sd_m = (unsigned char *)(pdst); \
  unsigned long long val_m = (val); \
  \
  asm volatile( \
    "sd %[val_m], %[pdst_sd_m] \n\t" : \
    [pdst_sd_m] "=m" (*pdst_sd_m) : \
    [val_m] "r" (val_m) \
  ); \
}
#else  /* !(__mips_isa_rev >= 6) */
#define LW(psrc) \
( { \
  unsigned char *psrc_lw_m = (unsigned char *)(psrc); \
  unsigned int val_m; \
  \
  asm volatile( \
    "ulw %[val_m], %[psrc_lw_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_lw_m] "m" (*psrc_lw_m) \
  ); \
  \
  val_m; \
} )

#if (__mips == 64)
#define LD(psrc) \
( { \
  unsigned char *psrc_ld_m = (unsigned char *)(psrc); \
  unsigned long long val_m = 0; \
  \
  asm volatile( \
    "uld %[val_m], %[psrc_ld_m] \n\t" : \
    [val_m] "=r" (val_m) : \
    [psrc_ld_m] "m" (*psrc_ld_m) \
  ); \
  \
  val_m; \
} )
#else /* !(__mips == 64) */
#define LD(psrc) \
( { \
  unsigned char *psrc_ld_m = (unsigned char *)(psrc); \
  unsigned int val0_m, val1_m; \
  unsigned long long val_m = 0; \
  \
  val0_m = LW(psrc_ld_m); \
  val1_m = LW(psrc_ld_m + 4); \
  \
  val_m = (unsigned long long)(val1_m); \
  val_m = (unsigned long long)((val_m << 32) & 0xFFFFFFFF00000000); \
  val_m = (unsigned long long)(val_m | (unsigned long long)val0_m); \
  \
  val_m; \
} )
#endif

#define SW(val, pdst) { \
  unsigned char *pdst_sw_m = (unsigned char *)(pdst); \
  unsigned int val_m = (val); \
  \
  asm volatile( \
    "usw %[val_m], %[pdst_sw_m] \n\t" : \
    [pdst_sw_m] "=m" (*pdst_sw_m) : \
    [val_m] "r" (val_m) \
  ); \
}

#define SD(val, pdst) { \
  unsigned char *pdst_sd_m = (unsigned char *)(pdst); \
  unsigned int val0_m, val1_m; \
  \
  val0_m = (unsigned int)((val) & 0x00000000FFFFFFFF); \
  val1_m = (unsigned int)(((val) >> 32) & 0x00000000FFFFFFFF); \
  \
  SW(val0_m, pdst_sd_m); \
  SW(val1_m, pdst_sd_m + 4); \
}
#endif
#endif

/* Description: Load vectors with 16 byte elements with stride
   Arguments  : Inputs  - psrc, stride
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Load 16 byte elements in 'out0' from (psrc)
                Load 16 byte elements in 'out1' from (psrc + stride)
*/
#define LD_B2(RTYPE, psrc, stride, out0, out1) { \
  out0 = LD_B(RTYPE, (psrc)); \
  out1 = LD_B(RTYPE, (psrc) + stride); \
}
#define LD_UB2(...)  LD_B2(v16u8, __VA_ARGS__)
#define LD_SB2(...)  LD_B2(v16i8, __VA_ARGS__)

#define LD_B3(RTYPE, psrc, stride, out0, out1, out2) { \
  LD_B2(RTYPE, (psrc), stride, out0, out1); \
  out2 = LD_B(RTYPE, (psrc) + 2 * stride); \
}
#define LD_UB3(...)  LD_B3(v16u8, __VA_ARGS__)

#define LD_B4(RTYPE, psrc, stride, out0, out1, out2, out3) { \
  LD_B2(RTYPE, (psrc), stride, out0, out1); \
  LD_B2(RTYPE, (psrc) + 2 * stride, stride, out2, out3); \
}
#define LD_UB4(...)  LD_B4(v16u8, __VA_ARGS__)
#define LD_SB4(...)  LD_B4(v16i8, __VA_ARGS__)

#define LD_B6(RTYPE, psrc, stride, out0, out1, out2, out3, out4, out5) { \
  LD_B4(RTYPE, (psrc), stride, out0, out1, out2, out3); \
  LD_B2(RTYPE, (psrc) + 4 * stride, stride, out4, out5); \
}
#define LD_UB6(...)  LD_B6(v16u8, __VA_ARGS__)

#define LD_B8(RTYPE, psrc, stride, out0, out1, out2, out3, \
out4, out5, out6, out7) { \
  LD_B4(RTYPE, (psrc), stride, out0, out1, out2, out3); \
  LD_B4(RTYPE, (psrc) + 4 * stride, stride, out4, out5, out6, out7); \
}
#define LD_UB8(...)  LD_B8(v16u8, __VA_ARGS__)
#define LD_SB8(...)  LD_B8(v16i8, __VA_ARGS__)

/* Description: Load vectors with 8 halfword elements with stride
   Arguments  : Inputs  - psrc, stride
                Outputs - out0, out1
   Details    : Load 8 halfword elements in 'out0' from (psrc)
                Load 8 halfword elements in 'out1' from (psrc + stride)
*/
#define LD_H2(RTYPE, psrc, stride, out0, out1) { \
  out0 = LD_H(RTYPE, (psrc)); \
  out1 = LD_H(RTYPE, (psrc) + (stride)); \
}
#define LD_SH2(...)  LD_H2(v8i16, __VA_ARGS__)

#define LD_H4(RTYPE, psrc, stride, out0, out1, out2, out3) { \
  LD_H2(RTYPE, (psrc), stride, out0, out1); \
  LD_H2(RTYPE, (psrc) + 2 * stride, stride, out2, out3); \
}
#define LD_UH4(...)  LD_H4(v8u16, __VA_ARGS__)
#define LD_SH4(...)  LD_H4(v8i16, __VA_ARGS__)

#define LD_H6(RTYPE, psrc, stride, out0, out1, out2, out3, out4, out5) { \
  LD_H4(RTYPE, (psrc), stride, out0, out1, out2, out3); \
  LD_H2(RTYPE, (psrc) + 4 * stride, stride, out4, out5); \
}
#define LD_SH6(...)  LD_H6(v8i16, __VA_ARGS__)

#define LD_H8(RTYPE, psrc, stride, out0, out1, out2, out3, \
out4, out5, out6, out7) { \
  LD_H4(RTYPE, (psrc), stride, out0, out1, out2, out3); \
  LD_H4(RTYPE, (psrc) + 4 * stride, stride, out4, out5, out6, out7); \
}
#define LD_SH8(...)  LD_H8(v8i16, __VA_ARGS__)

/* Description: Store vectors of 16 byte elements with stride
   Arguments  : Inputs - in0, in1, pdst, stride
   Details    : Store 16 byte elements from 'in0' to (pdst)
                Store 16 byte elements from 'in1' to (pdst + stride)
*/
#define ST_B2(RTYPE, in0, in1, pdst, stride) { \
  ST_B(RTYPE, in0, (pdst)); \
  ST_B(RTYPE, in1, (pdst) + stride); \
}
#define ST_UB2(...)  ST_B2(v16u8, __VA_ARGS__)
#define ST_SB2(...)  ST_B2(v16i8, __VA_ARGS__)

#define ST_B4(RTYPE, in0, in1, in2, in3, pdst, stride) { \
  ST_B2(RTYPE, in0, in1, (pdst), stride); \
  ST_B2(RTYPE, in2, in3, (pdst) + 2 * stride, stride); \
}
#define ST_UB4(...)  ST_B4(v16u8, __VA_ARGS__)
#define ST_SB4(...)  ST_B4(v16i8, __VA_ARGS__)

#define ST_B8(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, pdst, stride) { \
  ST_B4(RTYPE, in0, in1, in2, in3, pdst, stride); \
  ST_B4(RTYPE, in4, in5, in6, in7, (pdst) + 4 * stride, stride); \
}
#define ST_UB8(...)  ST_B8(v16u8, __VA_ARGS__)

/* Description: Store vectors of 8 halfword elements with stride
   Arguments  : Inputs - in0, in1, pdst, stride
   Details    : Store 8 halfword elements from 'in0' to (pdst)
                Store 8 halfword elements from 'in1' to (pdst + stride)
*/
#define ST_H2(RTYPE, in0, in1, pdst, stride) { \
  ST_H(RTYPE, in0, (pdst)); \
  ST_H(RTYPE, in1, (pdst) + stride); \
}
#define ST_H4(RTYPE, in0, in1, in2, in3, pdst, stride) { \
  ST_H2(RTYPE, in0, in1, (pdst), stride); \
  ST_H2(RTYPE, in2, in3, (pdst) + 2 * stride, stride); \
}
#define ST_SH4(...)  ST_H4(v8i16, __VA_ARGS__)

#define ST_H8(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, pdst, stride) { \
  ST_H4(RTYPE, in0, in1, in2, in3, (pdst), stride); \
  ST_H4(RTYPE, in4, in5, in6, in7, (pdst) + 4 * stride, stride); \
}
#define ST_SH8(...)  ST_H8(v8i16, __VA_ARGS__)

/* Description: Store 8x1 byte block to destination memory from input vector
   Arguments  : Inputs - in, pdst
   Details    : Index 0 double word element from 'in' vector is copied to the
                GP register and stored to (pdst)
*/
#define ST8x1_UB(in, pdst) { \
  unsigned long long out0_m; \
  out0_m = __msa_copy_u_d((v2i64)in, 0); \
  SD(out0_m, pdst); \
}

/* Description: Immediate number of elements to slide with zero
   Arguments  : Inputs  - in0, in1, slide_val
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Byte elements from 'zero_m' vector are slid into 'in0' by
                value specified in the 'slide_val'
*/
#define SLDI_B2_0(RTYPE, in0, in1, out0, out1, slide_val) { \
  v16i8 zero_m = { 0 }; \
  out0 = (RTYPE)__msa_sldi_b((v16i8)zero_m, (v16i8)in0, slide_val); \
  out1 = (RTYPE)__msa_sldi_b((v16i8)zero_m, (v16i8)in1, slide_val); \
}

/* Description: Immediate number of elements to slide
   Arguments  : Inputs  - in0_0, in0_1, in1_0, in1_1, slide_val
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Byte elements from 'in0_0' vector are slid into 'in1_0' by
                value specified in the 'slide_val'
*/
#define SLDI_B2(RTYPE, in0_0, in0_1, in1_0, in1_1, out0, out1, slide_val) { \
  out0 = (RTYPE)__msa_sldi_b((v16i8)in0_0, (v16i8)in1_0, slide_val); \
  out1 = (RTYPE)__msa_sldi_b((v16i8)in0_1, (v16i8)in1_1, slide_val); \
}
#define SLDI_B2_UB(...)  SLDI_B2(v16u8, __VA_ARGS__)

/* Description: Shuffle byte vector elements as per mask vector
   Arguments  : Inputs  - in0, in1, in2, in3, mask0, mask1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Byte elements from 'in0' & 'in1' are copied selectively to
                'out0' as per control vector 'mask0'
*/
#define VSHF_B2(RTYPE, in0, in1, in2, in3, mask0, mask1, out0, out1) { \
  out0 = (RTYPE)__msa_vshf_b((v16i8)mask0, (v16i8)in1, (v16i8)in0); \
  out1 = (RTYPE)__msa_vshf_b((v16i8)mask1, (v16i8)in3, (v16i8)in2); \
}
#define VSHF_B2_UB(...)  VSHF_B2(v16u8, __VA_ARGS__)
#define VSHF_B2_SB(...)  VSHF_B2(v16i8, __VA_ARGS__)
#define VSHF_B2_SH(...)  VSHF_B2(v8i16, __VA_ARGS__)

/* Description: Dot product of halfword vector elements
   Arguments  : Inputs  - mult0, mult1, cnst0, cnst1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Signed halfword elements from 'mult0' are multiplied with
                signed halfword elements from 'cnst0' producing a result
                twice the size of input i.e. signed word.
                The multiplication result of adjacent odd-even elements
                are added together and written to the 'out0' vector
*/
#define DOTP_SH2(RTYPE, mult0, mult1, cnst0, cnst1, out0, out1) { \
  out0 = (RTYPE)__msa_dotp_s_w((v8i16)mult0, (v8i16)cnst0); \
  out1 = (RTYPE)__msa_dotp_s_w((v8i16)mult1, (v8i16)cnst1); \
}
#define DOTP_SH2_SW(...)  DOTP_SH2(v4i32, __VA_ARGS__)

#define DOTP_SH4(RTYPE, mult0, mult1, mult2, mult3, \
cnst0, cnst1, cnst2, cnst3, out0, out1, out2, out3) { \
  DOTP_SH2(RTYPE, mult0, mult1, cnst0, cnst1, out0, out1); \
  DOTP_SH2(RTYPE, mult2, mult3, cnst2, cnst3, out2, out3); \
}
#define DOTP_SH4_SW(...)  DOTP_SH4(v4i32, __VA_ARGS__)

/* Description: Dot product & addition of halfword vector elements
   Arguments  : Inputs  - mult0, mult1, cnst0, cnst1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Signed halfword elements from 'mult0' are multiplied with
                signed halfword elements from 'cnst0' producing a result
                twice the size of input i.e. signed word.
                The multiplication result of adjacent odd-even elements
                are added to the 'out0' vector
*/
#define DPADD_SH2(RTYPE, mult0, mult1, cnst0, cnst1, out0, out1) { \
  out0 = (RTYPE)__msa_dpadd_s_w((v4i32)out0, (v8i16)mult0, (v8i16)cnst0); \
  out1 = (RTYPE)__msa_dpadd_s_w((v4i32)out1, (v8i16)mult1, (v8i16)cnst1); \
}
#define DPADD_SH2_SW(...)  DPADD_SH2(v4i32, __VA_ARGS__)

#define DPADD_SH4(RTYPE, mult0, mult1, mult2, mult3, \
cnst0, cnst1, cnst2, cnst3, out0, out1, out2, out3) { \
  DPADD_SH2(RTYPE, mult0, mult1, cnst0, cnst1, out0, out1); \
  DPADD_SH2(RTYPE, mult2, mult3, cnst2, cnst3, out2, out3); \
}
#define DPADD_SH4_SW(...)  DPADD_SH4(v4i32, __VA_ARGS__)

/* Description: Clips all halfword elements of input vector between min & max
                out = (in < min) ? min : ((in > max) ? max : in)
   Arguments  : Inputs - in, min, max
                Output - out_m
                Return Type - signed halfword
*/
#define CLIP_SH(in, min, max) \
( { \
  v8i16 out_m; \
  \
  out_m = __msa_max_s_h((v8i16)min, (v8i16)in); \
  out_m = __msa_min_s_h((v8i16)max, (v8i16)out_m); \
  out_m; \
} )

/* Description: Clips all signed halfword elements of input vector
                between 0 & 255
   Arguments  : Input  - in
                Output - out_m
                Return Type - signed halfword
*/
#define CLIP_SH_0_255(in) \
( { \
  v8i16 max_m = __msa_ldi_h(255); \
  v8i16 out_m; \
  \
  out_m = __msa_maxi_s_h((v8i16)in, 0); \
  out_m = __msa_min_s_h((v8i16)max_m, (v8i16)out_m); \
  out_m; \
} )
#define CLIP_SH2_0_255(in0, in1) { \
  in0 = CLIP_SH_0_255(in0); \
  in1 = CLIP_SH_0_255(in1); \
}
#define CLIP_SH4_0_255(in0, in1, in2, in3) { \
  CLIP_SH2_0_255(in0, in1); \
  CLIP_SH2_0_255(in2, in3); \
}

/* Description: Clips all signed word elements of input vector
                between 0 & 255
   Arguments  : Input  - in
                Output - out_m
                Return Type - signed word
*/
#define CLIP_SW_0_255(in) \
( { \
  v4i32 max_m = __msa_ldi_w(255); \
  v4i32 out_m; \
  \
  out_m = __msa_maxi_s_w((v4i32)in, 0); \
  out_m = __msa_min_s_w((v4i32)max_m, (v4i32)out_m); \
  out_m \
} )

/* Description: Horizontal addition of unsigned byte vector elements
   Arguments  : Inputs  - in0, in1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Each unsigned odd byte element from 'in0' is added to
                even unsigned byte element from 'in0' (pairwise) and the
                halfword result is written to 'out0'
*/
#define HADD_UB2(RTYPE, in0, in1, out0, out1) { \
  out0 = (RTYPE)__msa_hadd_u_h((v16u8)in0, (v16u8)in0); \
  out1 = (RTYPE)__msa_hadd_u_h((v16u8)in1, (v16u8)in1); \
}
#define HADD_UB2_SH(...)  HADD_UB2(v8i16, __VA_ARGS__)

#define HADD_UB4(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3) { \
  HADD_UB2(RTYPE, in0, in1, out0, out1); \
  HADD_UB2(RTYPE, in2, in3, out2, out3); \
}
#define HADD_UB4_SH(...)  HADD_UB4(v8i16, __VA_ARGS__)

/* Description: Horizontal subtraction of unsigned byte vector elements
   Arguments  : Inputs  - in0, in1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Each unsigned odd byte element from 'in0' is subtracted from
                even unsigned byte element from 'in0' (pairwise) and the
                halfword result is written to 'out0'
*/
#define HSUB_UB2(RTYPE, in0, in1, out0, out1) { \
  out0 = (RTYPE)__msa_hsub_u_h((v16u8)in0, (v16u8)in0); \
  out1 = (RTYPE)__msa_hsub_u_h((v16u8)in1, (v16u8)in1); \
}
#define HSUB_UB4(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3) { \
  HSUB_UB2(RTYPE, in0, in1, out0, out1); \
  HSUB_UB2(RTYPE, in2, in3, out2, out3); \
}
#define HSUB_UB4_SH(...)  HSUB_UB4(v8i16, __VA_ARGS__)

/* Description: Interleave left half of byte elements from vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Left half of byte elements of 'in0' and 'in1' are interleaved
                and written to 'out0'.
*/
#define ILVL_B2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_ilvl_b((v16i8)in0, (v16i8)in1); \
  out1 = (RTYPE)__msa_ilvl_b((v16i8)in2, (v16i8)in3); \
}
#define ILVL_B2_SH(...)  ILVL_B2(v8i16, __VA_ARGS__)

/* Description: Interleave left half of halfword elements from vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Left half of halfword elements of 'in0' and 'in1' are
                interleaved and written to 'out0'.
*/
#define ILVL_H2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_ilvl_h((v8i16)in0, (v8i16)in1); \
  out1 = (RTYPE)__msa_ilvl_h((v8i16)in2, (v8i16)in3); \
}
#define ILVL_H2_SH(...)  ILVL_H2(v8i16, __VA_ARGS__)

/* Description: Interleave right half of byte elements from vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Right half of byte elements of 'in0' and 'in1' are interleaved
                and written to out0.
*/
#define ILVR_B2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_ilvr_b((v16i8)in0, (v16i8)in1); \
  out1 = (RTYPE)__msa_ilvr_b((v16i8)in2, (v16i8)in3); \
}
#define ILVR_B2_SH(...)  ILVR_B2(v8i16, __VA_ARGS__)

#define ILVR_B4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  ILVR_B2(RTYPE, in0, in1, in2, in3, out0, out1); \
  ILVR_B2(RTYPE, in4, in5, in6, in7, out2, out3); \
}
#define ILVR_B4_UB(...)  ILVR_B4(v16u8, __VA_ARGS__)
#define ILVR_B4_SB(...)  ILVR_B4(v16i8, __VA_ARGS__)

/* Description: Interleave right half of halfword elements from vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Right half of halfword elements of 'in0' and 'in1' are
                interleaved and written to 'out0'.
*/
#define ILVR_H2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_ilvr_h((v8i16)in0, (v8i16)in1); \
  out1 = (RTYPE)__msa_ilvr_h((v8i16)in2, (v8i16)in3); \
}
#define ILVR_H2_UB(...)  ILVR_H2(v16u8, __VA_ARGS__)
#define ILVR_H2_SH(...)  ILVR_H2(v8i16, __VA_ARGS__)

#define ILVR_H4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  ILVR_H2(RTYPE, in0, in1, in2, in3, out0, out1); \
  ILVR_H2(RTYPE, in4, in5, in6, in7, out2, out3); \
}
#define ILVR_H4_UB(...)  ILVR_H4(v16u8, __VA_ARGS__)

/* Description: Interleave both left and right half of input vectors
   Arguments  : Inputs  - in0, in1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Right half of byte elements from 'in0' and 'in1' are
                interleaved and written to 'out0'
*/
#define ILVRL_B2(RTYPE, in0, in1, out0, out1) { \
  out0 = (RTYPE)__msa_ilvr_b((v16i8)in0, (v16i8)in1); \
  out1 = (RTYPE)__msa_ilvl_b((v16i8)in0, (v16i8)in1); \
}
#define ILVRL_B2_UB(...)  ILVRL_B2(v16u8, __VA_ARGS__)
#define ILVRL_B2_SB(...)  ILVRL_B2(v16i8, __VA_ARGS__)
#define ILVRL_B2_SH(...)  ILVRL_B2(v8i16, __VA_ARGS__)

#define ILVRL_H2(RTYPE, in0, in1, out0, out1) { \
  out0 = (RTYPE)__msa_ilvr_h((v8i16)in0, (v8i16)in1); \
  out1 = (RTYPE)__msa_ilvl_h((v8i16)in0, (v8i16)in1); \
}
#define ILVRL_H2_UB(...)  ILVRL_H2(v16u8, __VA_ARGS__)
#define ILVRL_H2_SH(...)  ILVRL_H2(v8i16, __VA_ARGS__)
#define ILVRL_H2_SW(...)  ILVRL_H2(v4i32, __VA_ARGS__)

#define ILVRL_W2(RTYPE, in0, in1, out0, out1) { \
  out0 = (RTYPE)__msa_ilvr_w((v4i32)in0, (v4i32)in1); \
  out1 = (RTYPE)__msa_ilvl_w((v4i32)in0, (v4i32)in1); \
}
#define ILVRL_W2_SH(...)  ILVRL_W2(v8i16, __VA_ARGS__)
#define ILVRL_W2_SW(...)  ILVRL_W2(v4i32, __VA_ARGS__)

/* Description: Indexed halfword element values are replicated to all
                elements in output vector
   Arguments  : Inputs  - in, idx0, idx1
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : 'idx0' element value from 'in' vector is replicated to all
                elements in 'out0' vector
                Valid index range for halfword operation is 0-7
*/
#define SPLATI_H2(RTYPE, in, idx0, idx1, out0, out1) { \
  out0 = (RTYPE)__msa_splati_h((v8i16)in, idx0); \
  out1 = (RTYPE)__msa_splati_h((v8i16)in, idx1); \
}
#define SPLATI_H4(RTYPE, in, idx0, idx1, idx2, idx3, \
out0, out1, out2, out3) { \
  SPLATI_H2(RTYPE, in, idx0, idx1, out0, out1); \
  SPLATI_H2(RTYPE, in, idx2, idx3, out2, out3); \
}
#define SPLATI_H4_SH(...)  SPLATI_H4(v8i16, __VA_ARGS__)

/* Description: Indexed word element values are replicated to all
                elements in output vector
   Arguments  : Inputs  - in, stidx
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : 'stidx' element value from 'in' vector is replicated to all
                elements in 'out0' vector
                'stidx + 1' element value from 'in' vector is replicated to all
                elements in 'out1' vector
                Valid index range for word operation is 0-3
*/
#define SPLATI_W2(RTYPE, in, stidx, out0, out1) { \
  out0 = (RTYPE)__msa_splati_w((v4i32)in, stidx); \
  out1 = (RTYPE)__msa_splati_w((v4i32)in, (stidx + 1)); \
}
#define SPLATI_W2_SW(...)  SPLATI_W2(v4i32, __VA_ARGS__)

#define SPLATI_W4(RTYPE, in, out0, out1, out2, out3) { \
  SPLATI_W2(RTYPE, in, 0, out0, out1); \
  SPLATI_W2(RTYPE, in, 2, out2, out3); \
}
#define SPLATI_W4_SW(...)  SPLATI_W4(v4i32, __VA_ARGS__)

/* Description: Pack even byte elements of vector pairs
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Even byte elements of 'in0' are copied to the left half of
                'out0' & even byte elements of 'in1' are copied to the right
                half of 'out0'.
*/
#define PCKEV_B2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_pckev_b((v16i8)in0, (v16i8)in1); \
  out1 = (RTYPE)__msa_pckev_b((v16i8)in2, (v16i8)in3); \
}
#define PCKEV_B2_SB(...)  PCKEV_B2(v16i8, __VA_ARGS__)

#define PCKEV_B4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  PCKEV_B2(RTYPE, in0, in1, in2, in3, out0, out1); \
  PCKEV_B2(RTYPE, in4, in5, in6, in7, out2, out3); \
}
#define PCKEV_B4_UB(...)  PCKEV_B4(v16u8, __VA_ARGS__)

/* Description: Pack even halfword elements of vector pairs
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Even halfword elements of 'in0' are copied to the left half of
                'out0' & even halfword elements of 'in1' are copied to the
                right half of 'out0'.
*/
#define PCKEV_H2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_pckev_h((v8i16)in0, (v8i16)in1); \
  out1 = (RTYPE)__msa_pckev_h((v8i16)in2, (v8i16)in3); \
}
#define PCKEV_H2_SH(...)  PCKEV_H2(v8i16, __VA_ARGS__)
#define PCKEV_H2_SW(...)  PCKEV_H2(v4i32, __VA_ARGS__)

/* Description: Pack even double word elements of vector pairs
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Even double elements of 'in0' are copied to the left half of
                'out0' & even double elements of 'in1' are copied to the right
                half of 'out0'.
*/
#define PCKEV_D2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_pckev_d((v2i64)in0, (v2i64)in1); \
  out1 = (RTYPE)__msa_pckev_d((v2i64)in2, (v2i64)in3); \
}
#define PCKEV_D4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  PCKEV_D2(RTYPE, in0, in1, in2, in3, out0, out1); \
  PCKEV_D2(RTYPE, in4, in5, in6, in7, out2, out3); \
}

/* Description: Pack odd half elements of vector pairs
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Odd half elements of 'in0' are copied to the left half of
                'out0' & odd half elements of 'in1' are copied to the right
                half of 'out0'.
*/
#define PCKOD_H2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)__msa_pckod_h((v8i16)in0, (v8i16)in1); \
  out1 = (RTYPE)__msa_pckod_h((v8i16)in2, (v8i16)in3); \
}
#define PCKOD_H2_SH(...)  PCKOD_H2(v8i16, __VA_ARGS__)

/* Description: Shift left all elements of word vector
   Arguments  : Inputs  - in0, in1, in2, in3, shift
                Outputs - in place operation
                Return Type - as per input vector RTYPE
   Details    : Each element of vector 'in0' is left shifted by 'shift' and
                the result is written in-place.
*/
#define SLLI_W4(RTYPE, in0, in1, in2, in3, shift_val) { \
  in0 = (RTYPE)MSA_SLLI_W(in0, shift_val); \
  in1 = (RTYPE)MSA_SLLI_W(in1, shift_val); \
  in2 = (RTYPE)MSA_SLLI_W(in2, shift_val); \
  in3 = (RTYPE)MSA_SLLI_W(in3, shift_val); \
}
#define SLLI_W4_SW(...)  SLLI_W4(v4i32, __VA_ARGS__)

/* Description: Arithmetic shift right all elements of half-word vector
   Arguments  : Inputs  - in0, in1, in2, in3, shift
                Outputs - in place operation
                Return Type - as per input vector RTYPE
   Details    : Each element of vector 'in0' is right shifted by 'shift' and
                the result is written in-place. 'shift' is a GP variable.
*/
#define SRAI_H2(RTYPE, in0, in1, shift) { \
  in0 = (RTYPE)__msa_srai_h((v8i16)in0, shift); \
  in1 = (RTYPE)__msa_srai_h((v8i16)in1, shift); \
}
#define SRAI_H2_SH(...)  SRAI_H2(v8i16, __VA_ARGS__)

#define SRAI_H4(RTYPE, in0, in1, in2, in3, shift) { \
  SRAI_H2(RTYPE, in0, in1, shift); \
  SRAI_H2(RTYPE, in2, in3, shift); \
}
#define SRAI_H4_SH(...)  SRAI_H4(v8i16, __VA_ARGS__)

/* Description: Arithmetic shift right all elements of word vector
   Arguments  : Inputs  - in0, in1, in2, in3, shift
                Outputs - in place operation
                Return Type - as per input vector RTYPE
   Details    : Each element of vector 'in0' is right shifted by 'shift' and
                the result is written in-place. 'shift' is a GP variable.
*/
#define SRAI_W4(RTYPE, in0, in1, in2, in3, shift_val) { \
  in0 = (RTYPE)MSA_SRAI_W(in0, shift_val); \
  in1 = (RTYPE)MSA_SRAI_W(in1, shift_val); \
  in2 = (RTYPE)MSA_SRAI_W(in2, shift_val); \
  in3 = (RTYPE)MSA_SRAI_W(in3, shift_val); \
}
#define SRAI_W4_SW(...)  SRAI_W4(v4i32, __VA_ARGS__)

/* Description: Shift right arithmetic rounded (immediate)
   Arguments  : Inputs  - in0, in1, shift
                Outputs - in place operation
                Return Type - as per RTYPE
   Details    : Each element of vector 'in0' is shifted right arithmetically by
                the value in 'shift'. The last discarded bit is added to the
                shifted value for rounding and the result is written in-place.
                'shift' is an immediate value.
*/
#define SRARI_H2(RTYPE, in0, in1, shift) { \
  in0 = (RTYPE)__msa_srari_h((v8i16)in0, shift); \
  in1 = (RTYPE)__msa_srari_h((v8i16)in1, shift); \
}
#define SRARI_H2_UH(...)  SRARI_H2(v8u16, __VA_ARGS__)
#define SRARI_H2_SH(...)  SRARI_H2(v8i16, __VA_ARGS__)

#define SRARI_H4(RTYPE, in0, in1, in2, in3, shift) { \
  SRARI_H2(RTYPE, in0, in1, shift); \
  SRARI_H2(RTYPE, in2, in3, shift); \
}
#define SRARI_H4_UH(...)  SRARI_H4(v8u16, __VA_ARGS__)
#define SRARI_H4_SH(...)  SRARI_H4(v8i16, __VA_ARGS__)

#define SRARI_W2(RTYPE, in0, in1, shift) { \
  in0 = (RTYPE)__msa_srari_w((v4i32)in0, shift); \
  in1 = (RTYPE)__msa_srari_w((v4i32)in1, shift); \
}
#define SRARI_W2_SW(...)  SRARI_W2(v4i32, __VA_ARGS__)

#define SRARI_W4(RTYPE, in0, in1, in2, in3, shift) { \
  SRARI_W2(RTYPE, in0, in1, shift); \
  SRARI_W2(RTYPE, in2, in3, shift); \
}
#define SRARI_W4_SW(...)  SRARI_W4(v4i32, __VA_ARGS__)

/* Description: Multiplication of pairs of vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
   Details    : Each element from 'in0' is multiplied with elements from 'in1'
                and the result is written to 'out0'
*/
#define MUL2(in0, in1, in2, in3, out0, out1) { \
  out0 = in0 * in1; \
  out1 = in2 * in3; \
}
#define MUL4(in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  MUL2(in0, in1, in2, in3, out0, out1); \
  MUL2(in4, in5, in6, in7, out2, out3); \
}

/* Description: XOR of 2 pairs of vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
                Return Type - as per RTYPE
   Details    : Each element in 'in0' is xor'ed with 'in1' and result is
                 written to 'out0'.
*/
#define XOR_V2(RTYPE, in0, in1, in2, in3, out0, out1) { \
  out0 = (RTYPE)((v16u8)in0 ^ (v16u8)in1); \
  out1 = (RTYPE)((v16u8)in2 ^ (v16u8)in3); \
}
#define XOR_V4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  XOR_V2(RTYPE, in0, in1, in2, in3, out0, out1); \
  XOR_V2(RTYPE, in4, in5, in6, in7, out2, out3); \
}
#define XOR_V4_SH(...)  XOR_V4(v8i16, __VA_ARGS__);

/* Description: Addition of 2 pairs of vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
   Details    : Each element in 'in0' is added to 'in1' and result is written
                to 'out0'.
*/
#define ADD2(in0, in1, in2, in3, out0, out1) { \
  out0 = in0 + in1; \
  out1 = in2 + in3; \
}
#define ADD4(in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  ADD2(in0, in1, in2, in3, out0, out1); \
  ADD2(in4, in5, in6, in7, out2, out3); \
}

/* Description: Subtraction of 2 pairs of vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1
   Details    : Each element in 'in1' is subtracted from 'in0' and result is
                written to 'out0'.
*/
#define SUB2(in0, in1, in2, in3, out0, out1) { \
  out0 = in0 - in1; \
  out1 = in2 - in3; \
}
#define SUB4(in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3) { \
  out0 = in0 - in1; \
  out1 = in2 - in3; \
  out2 = in4 - in5; \
  out3 = in6 - in7; \
}

/* Description: Sign extend byte elements from input vector and return
                halfword results in pair of vectors
   Arguments  : Input   - in         (byte vector)
                Outputs - out0, out1 (sign extended halfword vectors)
                Return Type - signed halfword
   Details    : Sign bit of byte elements from input vector 'in' is
                extracted and interleaved right with same vector 'in0' to
                generate 8 signed halfword elements in 'out0'
                Then interleaved left with same vector 'in0' to
                generate 8 signed halfword elements in 'out1'
*/
#define UNPCK_SB_SH(in, out0, out1) { \
  v16i8 tmp_m; \
  \
  tmp_m = __msa_clti_s_b((v16i8)in, 0); \
  ILVRL_B2_SH(tmp_m, in, out0, out1); \
}

/* Description: Zero extend unsigned byte elements to halfword elements
   Arguments  : Input   - in         (unsigned byte vector)
                Outputs - out0, out1 (unsigned  halfword vectors)
                Return Type - signed halfword
   Details    : Zero extended right half of vector is returned in 'out0'
                Zero extended left half of vector is returned in 'out1'
*/
#define UNPCK_UB_SH(in, out0, out1) { \
  v16i8 zero_m = { 0 }; \
  \
  ILVRL_B2_SH(zero_m, in, out0, out1); \
}

/* Description: Sign extend halfword elements from input vector and return
                the result in pair of vectors
   Arguments  : Input   - in         (halfword vector)
                Outputs - out0, out1 (sign extended word vectors)
                Return Type - signed word
   Details    : Sign bit of halfword elements from input vector 'in' is
                extracted and interleaved right with same vector 'in0' to
                generate 4 signed word elements in 'out0'
                Then interleaved left with same vector 'in0' to
                generate 4 signed word elements in 'out1'
*/
#define UNPCK_SH_SW(in, out0, out1) { \
  v8i16 tmp_m; \
  \
  tmp_m = __msa_clti_s_h((v8i16)in, 0); \
  ILVRL_H2_SW(tmp_m, in, out0, out1); \
}
#define UNPCK_SH2_SW(in0, in1, out0, out1, out2, out3) { \
  UNPCK_SH_SW(in0, out0, out1); \
  UNPCK_SH_SW(in1, out2, out3); \
}
#define UNPCK_SH4_SW(in0, in1, in2, in3, out0, out1, out2, out3, \
out4, out5, out6, out7) { \
  UNPCK_SH2_SW(in0, in1, out0, out1, out2, out3); \
  UNPCK_SH2_SW(in2, in3, out4, out5, out6, out7); \
}

/* Description: Butterfly of 4 input vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1, out2, out3
   Details    : Butterfly operation
*/
#define BUTTERFLY_4(in0, in1, in2, in3, out0, out1, out2, out3) { \
  out0 = in0 + in3; \
  out1 = in1 + in2; \
  \
  out2 = in1 - in2; \
  out3 = in0 - in3; \
}

/* Description: Butterfly of 8 input vectors
   Arguments  : Inputs  - in0 ...  in7
                Outputs - out0 .. out7
   Details    : Butterfly operation
*/
#define BUTTERFLY_8(in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3, out4, out5, out6, out7) { \
  out0 = in0 + in7; \
  out1 = in1 + in6; \
  out2 = in2 + in5; \
  out3 = in3 + in4; \
  \
  out4 = in3 - in4; \
  out5 = in2 - in5; \
  out6 = in1 - in6; \
  out7 = in0 - in7; \
}

/* Description: Transpose input 8x8 byte block
   Arguments  : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
                Outputs - out0, out1, out2, out3, out4, out5, out6, out7
                Return Type - as per RTYPE
*/
#define TRANSPOSE8x8_UB(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3, out4, out5, out6, out7) { \
  v16i8 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
  v16i8 tmp4_m, tmp5_m, tmp6_m, tmp7_m; \
  \
  ILVR_B4_SB(in2, in0, in3, in1, in6, in4, in7, in5, \
             tmp0_m, tmp1_m, tmp2_m, tmp3_m); \
  ILVRL_B2_SB(tmp1_m, tmp0_m, tmp4_m, tmp5_m); \
  ILVRL_B2_SB(tmp3_m, tmp2_m, tmp6_m, tmp7_m); \
  ILVRL_W2(RTYPE, tmp6_m, tmp4_m, out0, out2); \
  ILVRL_W2(RTYPE, tmp7_m, tmp5_m, out4, out6); \
  SLDI_B2_0(RTYPE, out0, out2, out1, out3, 8); \
  SLDI_B2_0(RTYPE, out4, out6, out5, out7, 8); \
}
#define TRANSPOSE8x8_UB_UB(...)  TRANSPOSE8x8_UB(v16u8, __VA_ARGS__)

/* Description: Transpose 8x8 block with half word elements in vectors
   Arguments  : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
                Outputs - out0, out1, out2, out3, out4, out5, out6, out7
                Return Type - as per RTYPE
*/
#define TRANSPOSE8x8_H(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, \
out0, out1, out2, out3, out4, out5, out6, out7) { \
  v8i16 s0_m, s1_m; \
  v8i16 tmp0_m, tmp1_m, tmp2_m, tmp3_m; \
  v8i16 tmp4_m, tmp5_m, tmp6_m, tmp7_m; \
  \
  ILVR_H2_SH(in6, in4, in7, in5, s0_m, s1_m); \
  ILVRL_H2_SH(s1_m, s0_m, tmp0_m, tmp1_m); \
  ILVL_H2_SH(in6, in4, in7, in5, s0_m, s1_m); \
  ILVRL_H2_SH(s1_m, s0_m, tmp2_m, tmp3_m); \
  ILVR_H2_SH(in2, in0, in3, in1, s0_m, s1_m); \
  ILVRL_H2_SH(s1_m, s0_m, tmp4_m, tmp5_m); \
  ILVL_H2_SH(in2, in0, in3, in1, s0_m, s1_m); \
  ILVRL_H2_SH(s1_m, s0_m, tmp6_m, tmp7_m); \
  PCKEV_D4(RTYPE, tmp0_m, tmp4_m, tmp1_m, tmp5_m, tmp2_m, tmp6_m, \
           tmp3_m, tmp7_m, out0, out2, out4, out6); \
  out1 = (RTYPE)__msa_pckod_d((v2i64)tmp0_m, (v2i64)tmp4_m); \
  out3 = (RTYPE)__msa_pckod_d((v2i64)tmp1_m, (v2i64)tmp5_m); \
  out5 = (RTYPE)__msa_pckod_d((v2i64)tmp2_m, (v2i64)tmp6_m); \
  out7 = (RTYPE)__msa_pckod_d((v2i64)tmp3_m, (v2i64)tmp7_m); \
}
#define TRANSPOSE8x8_SH_SH(...)  TRANSPOSE8x8_H(v8i16, __VA_ARGS__)

/* Description: Transpose 4x4 block with word elements in vectors
   Arguments  : Inputs  - in0, in1, in2, in3
                Outputs - out0, out1, out2, out3
                Return Type - as per RTYPE
*/
#define TRANSPOSE4x4_W(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3) { \
  v4i32 s0_m, s1_m, s2_m, s3_m; \
  \
  ILVRL_W2_SW(in1, in0, s0_m, s1_m); \
  ILVRL_W2_SW(in3, in2, s2_m, s3_m); \
  \
  out0 = (RTYPE)__msa_ilvr_d((v2i64)s2_m, (v2i64)s0_m); \
  out1 = (RTYPE)__msa_ilvl_d((v2i64)s2_m, (v2i64)s0_m); \
  out2 = (RTYPE)__msa_ilvr_d((v2i64)s3_m, (v2i64)s1_m); \
  out3 = (RTYPE)__msa_ilvl_d((v2i64)s3_m, (v2i64)s1_m); \
}
#define TRANSPOSE4x4_SW_SW(...)  TRANSPOSE4x4_W(v4i32, __VA_ARGS__)

#endif /* __JMACROS_MSA_H__ */
