/*
 * Copyright (c) 2023 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Song Ding   <songding@loongson.cn>
 *
 * This file base on generic_macros_lasx.h.
 */

#ifndef GENERIC_MACROS_LSX_H
#define GENERIC_MACROS_LSX_H

#include <stdint.h>
#include <lsxintrin.h>

/* Description : Load 128-bit vector data with stride
 * Arguments   : Inputs  - psrc    (source pointer to load from)
 *                       - stride
 *               Outputs - out0, out1, ~
 * Details     : Load 128-bit data in 'out0' from (psrc)
 *               Load 128-bit data in 'out1' from (psrc + stride)
 */
#define LSX_LD(psrc) *((__m128i *)(psrc))

#define LSX_LD_2(psrc, stride, out0, out1)                       \
{                                                                \
    out0 = LSX_LD(psrc);                                         \
    out1 = LSX_LD((psrc) + stride);                              \
}

#define LSX_LD_4(psrc, stride, out0, out1, out2, out3)           \
{                                                                \
    LSX_LD_2((psrc), stride, out0, out1);                        \
    LSX_LD_2((psrc) + 2 * stride , stride, out2, out3);          \
}

#define LSX_LD_8(psrc, stride, out0, out1, out2, out3,               \
                 out4, out5, out6, out7)                             \
{                                                                    \
    LSX_LD_4((psrc), stride, out0, out1, out2, out3);                \
    LSX_LD_4((psrc) + 4 * stride, stride, out4, out5, out6, out7);   \
}

/* Description : Store 128-bit vector data with stride
 * Arguments   : Inputs  - in0, in1, ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store 128-bit data from 'in0' to (pdst)
 *               Store 128-bit data from 'in1' to (pdst + stride)
 */
#define LSX_ST(in, pdst) *((__m128i *)(pdst)) = (in)

#define LSX_ST_2(in0, in1, pdst, stride)                         \
{                                                                \
    LSX_ST(in0, (pdst));                                         \
    LSX_ST(in1, (pdst) + stride);                                \
}

#define LSX_ST_4(in0, in1, in2, in3, pdst, stride)               \
{                                                                \
    LSX_ST_2(in0, in1, (pdst), stride);                          \
    LSX_ST_2(in2, in3, (pdst) + 2 * stride, stride);             \
}

#define LSX_ST_8(in0, in1, in2, in3, in4, in5, in6, in7, pdst, stride)      \
{                                                                           \
    LSX_ST_4(in0, in1, in2, in3, (pdst), stride);                           \
    LSX_ST_4(in4, in5, in6, in7, (pdst) + 4 * stride, stride);              \
}

/* Description : Store half word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1,  ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store half word 'idx0' from 'in' to (pdst)
 *               Store half word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : LSX_ST_H(in, idx, pdst)
 *          in : 1, 2, 3, 4, 5, 6, 7, 8
 *        idx0 : 0x01
 *        out0 : 2
 */
#define LSX_ST_H(in, idx, pdst)                                  \
{                                                                \
    __lasx_xvstelm_h(in, pdst, 0, idx);                          \
}

#define LSX_ST_H_2(in, idx0, idx1, pdst, stride)                 \
{                                                                \
    LSX_ST_H(in, idx0, (pdst));                                  \
    LSX_ST_H(in, idx1, (pdst) + stride);                         \
}

#define LSX_ST_H_4(in, idx0, idx1, idx2, idx3, pdst, stride)     \
{                                                                \
    LSX_ST_H_2(in, idx0, idx1, (pdst), stride);                  \
    LSX_ST_H_2(in, idx2, idx3, (pdst) + 2 * stride, stride);     \
}


/* Description : Store word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1,  ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store word 'idx0' from 'in' to (pdst)
 *               Store word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : LSX_ST_W(in, idx, pdst)
 *          in : 1, 2, 3, 4
 *        idx0 : 0x01
 *        out0 : 2
 */
#define LSX_ST_W(in, idx, pdst)                                  \
{                                                                \
    __lasx_xvstelm_w(in, pdst, 0, idx);                          \
}

#define LSX_ST_W_2(in, idx0, idx1, pdst, stride)                 \
{                                                                \
    LSX_ST_W(in, idx0, (pdst));                                  \
    LSX_ST_W(in, idx1, (pdst) + stride);                         \
}

#define LSX_ST_W_4(in, idx0, idx1, idx2, idx3, pdst, stride)     \
{                                                                \
    LSX_ST_W_2(in, idx0, idx1, (pdst), stride);                  \
    LSX_ST_W_2(in, idx2, idx3, (pdst) + 2 * stride, stride);     \
}

#define LSX_ST_W_8(in, idx0, idx1, idx2, idx3, idx4, idx5,                  \
                   idx6, idx7, pdst, stride)                                \
{                                                                           \
    LSX_ST_W_4(in, idx0, idx1, idx2, idx3, (pdst), stride);                 \
    LSX_ST_W_4(in, idx4, idx5, idx6, idx7, (pdst) + 4 * stride, stride);    \
}

/* Description : Store double word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1, ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store double word 'idx0' from 'in' to (pdst)
 *               Store double word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : See LSX_ST_W(in, idx, pdst)
 */
#define LSX_ST_D(in, idx, pdst)                                  \
{                                                                \
    __lasx_xvstelm_d(in, pdst, 0, idx);                          \
}

#define LSX_ST_D_2(in, idx0, idx1, pdst, stride)                 \
{                                                                \
    LSX_ST_D(in, idx0, (pdst));                                  \
    LSX_ST_D(in, idx1, (pdst) + stride);                         \
}

#define LSX_ST_D_4(in, idx0, idx1, idx2, idx3, pdst, stride)     \
{                                                                \
    LSX_ST_D_2(in, idx0, idx1, (pdst), stride);                  \
    LSX_ST_D_2(in, idx2, idx3, (pdst) + 2 * stride, stride);     \
}

/* Description : Dot product of half word vector elements
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 *               Return Type - signed word
 * Details     : Signed half word elements from in* are iniplied with
 *               signed half word elements from in* producing a result
 *               twice the size of input i.e. signed word.
 *               Then this iniplication results of adjacent odd-even elements
 *               are added together and stored to the out vector.
 * Example     : LSX_DP2_W_H(in0, in1, out0)
 *               in0:   1,2,3,4, 5,6,7,8
 *               in0:   8,7,6,5, 4,3,2,1
 *               out0:  22,38,38,22
 */
#define LSX_DP2_W_H(in0, in1, out0)                              \
{                                                                \
    __m128i _tmp0_m ;                                            \
    _tmp0_m = __lsx_vmulwev_w_h( in0, in1 );                     \
    out0 = __lsx_vmaddwod_w_h( _tmp0_m, in0, in1 );              \
}

#define LSX_DP2_W_H_2(in0, in1, in2, in3, out0, out1)            \
{                                                                \
    LSX_DP2_W_H(in0, in1, out0);                                 \
    LSX_DP2_W_H(in2, in3, out1);                                 \
}

#define LSX_DP2_W_H_4(in0, in1, in2, in3, in4, in5,              \
                      in6, in7, out0, out1, out2, out3)          \
{                                                                \
    LSX_DP2_W_H_2(in0, in1, in2, in3, out0, out1);               \
    LSX_DP2_W_H_2(in4, in5, in6, in7, out2, out3);               \
}

#define LSX_DP2_W_H_8(in0, in1, in2, in3, in4, in5, in6, in7,               \
                      in8, in9, in10, in11, in12, in13, in14, in15,         \
                      out0, out1, out2, out3, out4, out5, out6, out7)       \
{                                                                           \
    LSX_DP2_W_H_4(in0, in1, in2, in3, in4, in5, in6, in7,                   \
                  out0, out1, out2, out3);                                  \
    LSX_DP2_W_H_4(in8, in9, in10, in11, in12, in13, in14, in15,             \
                  out4, out5, out6, out7);                                  \
}

/* Description : Low 8-bit vector elements unsigned extension to halfword
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low 8-bit elements from in0 unsigned extension to halfword,
 *               written to output vector out0. Similar for in1.
 * Example     : See LSX_UNPCK_L_W_H(in0, out0)
 */
#define LSX_UNPCK_L_HU_BU(in0, out0)                                           \
{                                                                              \
    out0 = __lsx_vsllwil_hu_bu(in0, 0);                                        \
}

#define LSX_UNPCK_L_HU_BU_2(in0, in1, out0, out1)                              \
{                                                                              \
    LSX_UNPCK_L_HU_BU(in0, out0);                                              \
    LSX_UNPCK_L_HU_BU(in1, out1);                                              \
}

#define LSX_UNPCK_L_HU_BU_4(in0, in1, in2, in3, out0, out1, out2, out3)        \
{                                                                              \
    LSX_UNPCK_L_HU_BU_2(in0, in1, out0, out1);                                 \
    LSX_UNPCK_L_HU_BU_2(in2, in3, out2, out3);                                 \
}

#define LSX_UNPCK_L_HU_BU_8(in0, in1, in2, in3, in4, in5, in6, in7,            \
                            out0, out1, out2, out3, out4, out5, out6, out7)    \
{                                                                              \
    LSX_UNPCK_L_HU_BU_4(in0, in1, in2, in3, out0, out1, out2, out3);           \
    LSX_UNPCK_L_HU_BU_4(in4, in5, in6, in7, out4, out5, out6, out7);           \
}


/* Description : Low halfword vector elements signed extension to word
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low halfword elements from in0 signed extension to
 *               word, written to output vector out0. Similar for in1.
 *               Similar for other pairs.
 * Example     : LSX_UNPCK_L_W_H(in0, out0)
 *         in0 : 3, 0, 3, 0,  0, 0, 0, -1
 *        out0 : 3, 0, 3, 0
 */
#define LSX_UNPCK_L_W_H(in0, out0)                                             \
{                                                                              \
    out0 = __lsx_vsllwil_w_h(in0, 0);                                          \
}

#define LSX_UNPCK_L_W_H_2(in0, in1, out0, out1)                                \
{                                                                              \
    LSX_UNPCK_L_W_H(in0, out0);                                                \
    LSX_UNPCK_L_W_H(in1, out1);                                                \
}

#define LSX_UNPCK_L_W_H_4(in0, in1, in2, in3, out0, out1, out2, out3)          \
{                                                                              \
    LSX_UNPCK_L_W_H_2(in0, in1, out0, out1);                                   \
    LSX_UNPCK_L_W_H_2(in2, in3, out2, out3);                                   \
}

#define LSX_UNPCK_L_W_H_8(in0, in1, in2, in3, in4, in5, in6, in7,              \
                          out0, out1, out2, out3, out4, out5, out6, out7)      \
{                                                                              \
    LSX_UNPCK_L_W_H_4(in0, in1, in2, in3, out0, out1, out2, out3);             \
    LSX_UNPCK_L_W_H_4(in4, in5, in6, in7, out4, out5, out6, out7);             \
}

/* Description : Interleave right half of byte elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of byte elements of in_l and high half of byte
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : See LSX_ILVL_W(in_h, in_l, out0)
 */
#define LSX_ILVL_B(in_h, in_l, out0)                                           \
{                                                                              \
    out0 = __lsx_vilvl_b(in_h, in_l);                                          \
}

#define LSX_ILVL_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                   \
{                                                                              \
    LSX_ILVL_B(in0_h, in0_l, out0)                                             \
    LSX_ILVL_B(in1_h, in1_l, out1)                                             \
}

#define LSX_ILVL_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                 \
                     in3_h, in3_l, out0, out1, out2, out3)                     \
{                                                                              \
    LSX_ILVL_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                       \
    LSX_ILVL_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3)                       \
}

#define LSX_ILVL_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,   \
                     in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,   \
                     out0, out1, out2, out3, out4, out5, out6, out7)           \
{                                                                              \
    LSX_ILVL_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,       \
                 out0, out1, out2, out3);                                      \
    LSX_ILVL_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,       \
                 out4, out5, out6, out7);                                      \
}

/* Description : Interleave low half of word elements from vectors
 * Arguments   : Inputs  - in0_h, in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of halfword elements of in_l and low half of word
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : LSX_ILVL_W(in_h, in_l, out0)
 *        in_h : 0, 1, 0, 1
 *        in_l : 1, 2, 3, 4
 *        out0 : 1, 0, 2, 1
 */
#define LSX_ILVL_W(in_h, in_l, out0)                                           \
{                                                                              \
    out0 = __lsx_vilvl_w(in_h, in_l);                                          \
}

#define LSX_ILVL_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                   \
{                                                                              \
    LSX_ILVL_W(in0_h, in0_l, out0);                                            \
    LSX_ILVL_W(in1_h, in1_l, out1);                                            \
}

#define LSX_ILVL_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                 \
                     in3_h, in3_l, out0, out1, out2, out3)                     \
{                                                                              \
    LSX_ILVL_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                      \
    LSX_ILVL_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                      \
}

#define LSX_ILVL_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,   \
                     in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,   \
                     out0, out1, out2, out3, out4, out5, out6, out7)           \
{                                                                              \
    LSX_ILVL_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,       \
                 out0, out1, out2, out3);                                      \
    LSX_ILVL_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,       \
                 out4, out5, out6, out7);                                      \
}

/* Description : Interleave high half of byte elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of  byte elements  of  in_l and high half
 *               of byte elements of in_h are interleaved and copied
 *               to out0.
 *               Similar for other pairs.
 * Example     : see LSX_ILVH_W(in_h, in_l, out0)
 */
#define LSX_ILVH_B(in_h, in_l, out0)                                           \
{                                                                              \
    out0 = __lsx_vilvh_b(in_h, in_l);                                          \
}

#define LSX_ILVH_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                   \
{                                                                              \
    LSX_ILVH_B(in0_h, in0_l, out0);                                            \
    LSX_ILVH_B(in1_h, in1_l, out1);                                            \
}

#define LSX_ILVH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                 \
                     in3_h, in3_l, out0, out1, out2, out3)                     \
{                                                                              \
    LSX_ILVH_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                      \
    LSX_ILVH_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                      \
}

#define LSX_ILVH_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,   \
                     in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,   \
                     out0, out1, out2, out3, out4, out5, out6, out7)           \
{                                                                              \
    LSX_ILVH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,       \
                 out0, out1, out2, out3);                                      \
    LSX_ILVH_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,       \
                 out4, out5, out6, out7);                                      \
}

/* Description : Interleave high half of word elements from vectors
 * Arguments   : Inputs  - in0_h, in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of word elements of in_l and high half of
 *               word elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs.
 * Example     : LSX_ILVH_W(in_h, in_l, out0)
 *         in_h:-1, -2, -3, -4
 *         in_l: 1,  2,  3,  4
 *         out0: 3, -3,  4, -4
 */
#define LSX_ILVH_W(in_h, in_l, out0)                                           \
{                                                                              \
    out0 = __lsx_vilvh_w(in_h, in_l);                                          \
}

#define LSX_ILVH_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                   \
{                                                                              \
    LSX_ILVH_W(in0_h, in0_l, out0);                                            \
    LSX_ILVH_W(in1_h, in1_l, out1);                                            \
}

#define LSX_ILVH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                 \
                     in3_h, in3_l, out0, out1, out2, out3)                     \
{                                                                              \
    LSX_ILVH_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                      \
    LSX_ILVH_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                      \
}

#define LSX_ILVH_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,   \
                     in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,   \
                     out0, out1, out2, out3, out4, out5, out6, out7)           \
{                                                                              \
    LSX_ILVH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,       \
                 out0, out1, out2, out3);                                      \
    LSX_ILVH_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,       \
                 out4, out5, out6, out7);                                      \
}

/* Description : Interleave byte elements from vectors
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of  byte elements  of in_l and low half of byte
 *               elements  of in_h  are interleaved  and copied  to  out_l.
 *               High half of byte elements of in_l and high half of byte
 *               elements of in_h are interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : see LSX_ILVLH_W(in_h, in_l, out_l, out_h)
 */
#define LSX_ILVLH_B(in_h, in_l, out_h, out_l)                  \
{                                                              \
    out_l  = __lsx_vilvl_b(in_h, in_l);                        \
    out_h  = __lsx_vilvh_b(in_h, in_l);                        \
}

#define LSX_ILVLH_B_2(in0_h, in0_l, in1_h, in1_l,              \
                      out0_h, out0_l, out1_h, out1_l)          \
{                                                              \
    LSX_ILVLH_B(in0_h, in0_l, out0_h, out0_l);                 \
    LSX_ILVLH_B(in1_h, in1_l, out1_h, out1_l);                 \
}

#define LSX_ILVLH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                      \
    LSX_ILVLH_B_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LSX_ILVLH_B_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LSX_ILVLH_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                      out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                      out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                      \
    LSX_ILVLH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                  out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LSX_ILVLH_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                  out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave half word elements from vectors
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of  half word elements  of in_l and low half of half
 *               word elements of in_h  are  interleaved  and  copied  to out_l.
 *               High half of half word elements of in_l and high half of half
 *               word elements of in_h are interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : see LSX_ILVLH_W(in_h, in_l, out_h, out_l)
 */
#define LSX_ILVLH_H(in_h, in_l, out_h, out_l)                    \
{                                                                \
    out_l = __lsx_vilvl_h(in_h, in_l);                           \
    out_h = __lsx_vilvh_h(in_h, in_l);                           \
}

#define LSX_ILVLH_H_2(in0_h, in0_l, in1_h, in1_l,                \
                      out0_h, out0_l, out1_h, out1_l)            \
{                                                                \
    LSX_ILVLH_H(in0_h, in0_l, out0_h, out0_l);                   \
    LSX_ILVLH_H(in1_h, in1_l, out1_h, out1_l);                   \
}

#define LSX_ILVLH_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                      \
    LSX_ILVLH_H_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LSX_ILVLH_H_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LSX_ILVLH_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                      out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                      out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                      \
    LSX_ILVLH_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                  out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LSX_ILVLH_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                  out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave word elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l,  ~
 *               Outputs - out0_h, out0_l, ~
 * Details     : Low half of word elements  of in_l and low half of word
 *               elements of in_h are interleaved and copied to out_l.
 *               High half of word elements of in_l and high half of
 *               word elements of in_h are interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : LSX_ILVLH_W(in_h, in_l, out_h, out_l)
 *         in_h:-1, -2, -3, -4
 *         in_l: 1,  2,  3,  4
 *        out_h: 3, -3,  4, -4
 *        out_l: 1, -1,  2, -2
 */
#define LSX_ILVLH_W(in_h, in_l, out_h, out_l)                    \
{                                                                \
    LSX_ILVL_W(in_h, in_l, out_l);                               \
    LSX_ILVH_W(in_h, in_l, out_h);                               \
}

#define LSX_ILVLH_W_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l)      \
{                                                                                      \
    LSX_ILVLH_W(in0_h, in0_l, out0_h, out0_l);                                         \
    LSX_ILVLH_W(in1_h, in1_l, out1_h, out1_l);                                         \
}

#define LSX_ILVLH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                      \
    LSX_ILVLH_W_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LSX_ILVLH_W_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LSX_ILVLH_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                      out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                      out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                      \
    LSX_ILVLH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                  out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LSX_ILVLH_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                  out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Pack even byte elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even byte elements of in_l are copied to the low half of
 *               out0.  Even byte elements of in_h are copied to the high
 *               half of out0.
 *               Similar for other pairs.
 * Example     : see LSX_PCKEV_W(in_h, in_l, out0)
 */
#define LSX_PCKEV_B(in_h, in_l, out0)                            \
{                                                                \
    out0 = __lsx_vpickev_b(in_h, in_l);                          \
}

#define LSX_PCKEV_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                \
    LSX_PCKEV_B(in0_h, in0_l, out0);                             \
    LSX_PCKEV_B(in1_h, in1_l, out1);                             \
}

#define LSX_PCKEV_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                      in3_h, in3_l, out0, out1, out2, out3)      \
{                                                                \
    LSX_PCKEV_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LSX_PCKEV_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LSX_PCKEV_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LSX_PCKEV_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                    \
                  in3_h, in3_l, out0, out1, out2, out3);                       \
    LSX_PCKEV_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                    \
                  in7_h, in7_l, out4, out5, out6, out7);                       \
}

/* Description : Pack even half word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even half word elements of in_l are copied to the  low
 *               half of out0.  Even  half  word  elements  of in_h are
 *               copied to the high half of out0.
 * Example     : see LSX_PCKEV_W(in_h, in_l, out0)
 */
#define LSX_PCKEV_H(in_h, in_l, out0)                            \
{                                                                \
    out0 = __lsx_vpickev_h(in_h, in_l);                          \
}

#define LSX_PCKEV_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                \
    LSX_PCKEV_H(in0_h, in0_l, out0);                             \
    LSX_PCKEV_H(in1_h, in1_l, out1);                             \
}

#define LSX_PCKEV_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                      in3_h, in3_l, out0, out1, out2, out3)      \
{                                                                \
    LSX_PCKEV_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LSX_PCKEV_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LSX_PCKEV_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LSX_PCKEV_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                    \
                  in3_h, in3_l, out0, out1, out2, out3);                       \
    LSX_PCKEV_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                    \
                  in7_h, in7_l, out4, out5, out6, out7);                       \
}

/* Description : Pack odd byte elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd byte elements of in_l are copied to the low half of
 *               out0. Odd byte elements of in_h are copied to the high
 *               half of out0.
 *               Similar for other pairs.
 * Example     : see LSX_PCKOD_W(in_h, in_l, out0)
 */
#define LSX_PCKOD_B(in_h, in_l, out0)                            \
{                                                                \
    out0 = __lsx_vpickod_b(in_h, in_l);                          \
}

#define LSX_PCKOD_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                \
    LSX_PCKOD_B(in0_h, in0_l, out0);                             \
    LSX_PCKOD_B(in1_h, in1_l, out1);                             \
}

#define LSX_PCKOD_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                      in3_h, in3_l, out0, out1, out2, out3)      \
{                                                                \
    LSX_PCKOD_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LSX_PCKOD_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LSX_PCKOD_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LSX_PCKOD_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                    \
                  in3_h, in3_l, out0, out1, out2, out3);                       \
    LSX_PCKOD_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                    \
                  in7_h, in7_l, out4, out5, out6, out7);                       \
}

/* Description : Pack odd half word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd half word elements of in_l are copied to the low
 *               half of out0. Odd half word elements of in_h are copied
 *               to the high half of out0.
 * Example     : see LSX_PCKOD_W(in_h, in_l, out0)
 */
#define LSX_PCKOD_H(in_h, in_l, out0)                            \
{                                                                \
    out0 = __lsx_vpickod_h(in_h, in_l);                          \
}

#define LSX_PCKOD_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                \
    LSX_PCKOD_H(in0_h, in0_l, out0);                             \
    LSX_PCKOD_H(in1_h, in1_l, out1);                             \
}

#define LSX_PCKOD_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                      in3_h, in3_l, out0, out1, out2, out3)      \
{                                                                \
    LSX_PCKOD_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LSX_PCKOD_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LSX_PCKOD_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LSX_PCKOD_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                    \
                  in3_h, in3_l, out0, out1, out2, out3);                       \
    LSX_PCKOD_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                    \
                  in7_h, in7_l, out4, out5, out6, out7);                       \
}

/* Description : Pack odd word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd word elements of in_l are copied to the low half of out0.
 *               Odd word elements of in_h are copied to the high half of out0.
 * Example     : LSX_PCKOD_W(in_h, in_l, out0)
 *         in_h: -1, -2, -3, -4
 *         in_l:  1,  2,  3,  4
 *         out0:  2,  4, -2, -4
 */
#define LSX_PCKOD_W(in_h, in_l, out0)                            \
{                                                                \
    out0 = __lsx_vpickod_w(in_h, in_l);                          \
}

#define LSX_PCKOD_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                \
    LSX_PCKOD_W(in0_h, in0_l, out0);                             \
    LSX_PCKOD_W(in1_h, in1_l, out1);                             \
}

#define LSX_PCKOD_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                      in3_h, in3_l, out0, out1, out2, out3)      \
{                                                                \
    LSX_PCKOD_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LSX_PCKOD_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LSX_PCKOD_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                      out0, out1, out2, out3, out4, out5, out6, out7)                  \
{                                                                                      \
    LSX_PCKOD_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                            \
                  in3_h, in3_l, out0, out1, out2, out3);                               \
    LSX_PCKOD_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                            \
                  in7_h, in7_l, out4, out5, out6, out7);                               \
}

/*
 * =============================================================================
 * Description : Transpose 4x4 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     :
 * Example     :
 *               1, 2, 3, 4            1, 5, 9,13
 *               5, 6, 7, 8    to      2, 6,10,14
 *               9,10,11,12  =====>    3, 7,11,15
 *              13,14,15,16            4, 8,12,16
 * =============================================================================
 */
#define LSX_TRANSPOSE4x4_W(_in0, _in1, _in2, _in3, _out0, _out1, _out2, _out3) \
{                                                                              \
  __m128i _t0, _t1, _t2, _t3;                                                  \
                                                                               \
  _t0 = __lsx_vilvl_w(_in1, _in0);                                             \
  _t1 = __lsx_vilvh_w(_in1, _in0);                                             \
  _t2 = __lsx_vilvl_w(_in3, _in2);                                             \
  _t3 = __lsx_vilvh_w(_in3, _in2);                                             \
  _out0 = __lsx_vilvl_d(_t2, _t0);                                             \
  _out1 = __lsx_vilvh_d(_t2, _t0);                                             \
  _out2 = __lsx_vilvl_d(_t3, _t1);                                             \
  _out3 = __lsx_vilvh_d(_t3, _t1);                                             \
}

/*
 * =============================================================================
 * Description : Transpose 8x8 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 * Details     :
 * Example     :
 *              00,01,02,03,04,05,06,07           00,10,20,30,40,50,60,70
 *              10,11,12,13,14,15,16,17           01,11,21,31,41,51,61,71
 *              20,21,22,23,24,25,26,27           02,12,22,32,42,52,62,72
 *              30,31,32,33,34,35,36,37    to     03,13,23,33,43,53,63,73
 *              40,41,42,43,44,45,46,47  ======>  04,14,24,34,44,54,64,74
 *              50,51,52,53,54,55,56,57           05,15,25,35,45,55,65,75
 *              60,61,62,63,64,65,66,67           06,16,26,36,46,56,66,76
 *              70,71,72,73,74,75,76,77           07,17,27,37,47,57,67,77
 * =============================================================================
 */
#define LSX_TRANSPOSE8x8_H(_in0, _in1, _in2, _in3, _in4, _in5, _in6, _in7,  \
                           _out0, _out1, _out2, _out3, _out4, _out5, _out6, \
                           _out7)                                           \
{                                                                           \
  __m128i _s0, _s1, _t0, _t1, _t2, _t3, _t4, _t5, _t6, _t7;                 \
                                                                            \
  _s0 = __lsx_vilvl_h(_in6, _in4);                                          \
  _s1 = __lsx_vilvl_h(_in7, _in5);                                          \
  _t0 = __lsx_vilvl_h(_s1, _s0);                                            \
  _t1 = __lsx_vilvh_h(_s1, _s0);                                            \
  _s0 = __lsx_vilvh_h(_in6, _in4);                                          \
  _s1 = __lsx_vilvh_h(_in7, _in5);                                          \
  _t2 = __lsx_vilvl_h(_s1, _s0);                                            \
  _t3 = __lsx_vilvh_h(_s1, _s0);                                            \
  _s0 = __lsx_vilvl_h(_in2, _in0);                                          \
  _s1 = __lsx_vilvl_h(_in3, _in1);                                          \
  _t4 = __lsx_vilvl_h(_s1, _s0);                                            \
  _t5 = __lsx_vilvh_h(_s1, _s0);                                            \
  _s0 = __lsx_vilvh_h(_in2, _in0);                                          \
  _s1 = __lsx_vilvh_h(_in3, _in1);                                          \
  _t6 = __lsx_vilvl_h(_s1, _s0);                                            \
  _t7 = __lsx_vilvh_h(_s1, _s0);                                            \
                                                                            \
  _out0 = __lsx_vpickev_d(_t0, _t4);                                        \
  _out2 = __lsx_vpickev_d(_t1, _t5);                                        \
  _out4 = __lsx_vpickev_d(_t2, _t6);                                        \
  _out6 = __lsx_vpickev_d(_t3, _t7);                                        \
  _out1 = __lsx_vpickod_d(_t0, _t4);                                        \
  _out3 = __lsx_vpickod_d(_t1, _t5);                                        \
  _out5 = __lsx_vpickod_d(_t2, _t6);                                        \
  _out7 = __lsx_vpickod_d(_t3, _t7);                                        \
}

/* Description : Transposes input 8x8 byte block
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *                         (input 8x8 byte block)
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 *                         (output 8x8 byte block)
 * Details     :
 */
#define LSX_TRANSPOSE8x8_B(_in0, _in1, _in2, _in3, _in4, _in5, _in6, _in7,  \
                           _out0, _out1, _out2, _out3, _out4, _out5, _out6, \
                           _out7)                                           \
{                                                                           \
  __m128i zero = { 0 };                                                     \
  __m128i shuf8 = { 0x0F0E0D0C0B0A0908, 0x1716151413121110 };               \
  __m128i _t0, _t1, _t2, _t3, _t4, _t5, _t6, _t7;                           \
                                                                            \
  _t0 = __lsx_vilvl_b(_in2, _in0);                                          \
  _t1 = __lsx_vilvl_b(_in3, _in1);                                          \
  _t2 = __lsx_vilvl_b(_in6, _in4);                                          \
  _t3 = __lsx_vilvl_b(_in7, _in5);                                          \
  _t4 = __lsx_vilvl_b(_t1, _t0);                                            \
  _t5 = __lsx_vilvh_b(_t1, _t0);                                            \
  _t6 = __lsx_vilvl_b(_t3, _t2);                                            \
  _t7 = __lsx_vilvh_b(_t3, _t2);                                            \
  _out0 = __lsx_vilvl_w(_t6, _t4);                                          \
  _out2 = __lsx_vilvh_w(_t6, _t4);                                          \
  _out4 = __lsx_vilvl_w(_t7, _t5);                                          \
  _out6 = __lsx_vilvh_w(_t7, _t5);                                          \
  _out1 = __lsx_vshuf_b(zero, _out0, shuf8);                                \
  _out3 = __lsx_vshuf_b(zero, _out2, shuf8);                                \
  _out5 = __lsx_vshuf_b(zero, _out4, shuf8);                                \
  _out7 = __lsx_vshuf_b(zero, _out6, shuf8);                                \
}

/* Description : Clips all signed word elements of input vector
 *               between 0 & 255
 * Arguments   : Inputs  - in       (input vector)
 *               Outputs - out_m    (output vector with clipped elements)
 *               Return Type - signed word
 */
#define LSX_CLIP_W_0_255(in, out_m)       \
{                                         \
    out_m = __lsx_vmaxi_w(in, 0);         \
    out_m = __lsx_vsat_wu(out_m, 7);      \
}

#define LSX_CLIP_W_0_255_2(in0, in1, out0, out1)  \
{                                                 \
    LSX_CLIP_W_0_255(in0, out0);                  \
    LSX_CLIP_W_0_255(in1, out1);                  \
}

#define LSX_CLIP_W_0_255_4(in0, in1, in2, in3, out0, out1, out2, out3)  \
{                                                                       \
    LSX_CLIP_W_0_255_2(in0, in1, out0, out1);                           \
    LSX_CLIP_W_0_255_2(in2, in3, out2, out3);                           \
}

/* Description : Clips all signed halfword elements of input vector
 *               between 0 & 255
 * Arguments   : Inputs  - in       (input vector)
 *               Outputs - out_m    (output vector with clipped elements)
 *               Return Type - signed halfword
 */
#define LSX_CLIP_H_0_255(in, out_m)       \
{                                         \
    out_m = __lsx_vmaxi_h(in, 0);         \
    out_m = __lsx_vsat_hu(out_m, 7);      \
}

#define LSX_CLIP_H_0_255_2(in0, in1, out0, out1)  \
{                                                 \
    LSX_CLIP_H_0_255(in0, out0);                  \
    LSX_CLIP_H_0_255(in1, out1);                  \
}

#define LSX_CLIP_H_0_255_4(in0, in1, in2, in3, out0, out1, out2, out3)  \
{                                                                       \
    LSX_CLIP_H_0_255_2(in0, in1, out0, out1);                           \
    LSX_CLIP_H_0_255_2(in2, in3, out2, out3);                           \
}

/* Description : Shift right arithmetic rounded (immediate)
 * Arguments   : Inputs  - in0, in1, shift
 *               Outputs - in0, in1, (in place)
 * Details     : Each element of vector 'in0' is shifted right arithmetic by
 *               value in 'shift'.
 *               The last discarded bit is added to shifted value for rounding
 *               and the result is in place written to 'in0'
 *               Similar for other pairs
 * Example     : LSX_SRARI_H(in0, out0, shift)
 *               in0:   1,2,3,4, 19,10,11,12
 *               shift: 2
 *               out0:  0,1,1,1, 5,3,3,3
 */
#define LSX_SRARI_H(in0, out0, shift)                                    \
{                                                                        \
    out0 = __lsx_vsrari_h(in0, shift);                                   \
}

#define LSX_SRARI_H_2(in0, in1, out0, out1, shift)                       \
{                                                                        \
    LSX_SRARI_H(in0, out0, shift);                                       \
    LSX_SRARI_H(in1, out1, shift);                                       \
}

#define LSX_SRARI_H_4(in0, in1, in2, in3, out0, out1, out2, out3, shift) \
{                                                                        \
    LSX_SRARI_H_2(in0, in1, out0, out1, shift);                          \
    LSX_SRARI_H_2(in2, in3, out2, out3, shift);                          \
}

/* Description : Addition of 2 pairs of vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 * Details     : Each halfwords element from 2 pairs vectors is added
 *               and 2 results are produced
 * Example     : LSX_ADD_H(in0, in1, out)
 *               in0:  1,2,3,4, 5,6,7,8
 *               in1:  8,7,6,5, 4,3,2,1
 *               out:  9,9,9,9, 9,9,9,9
 */
#define LSX_ADD_H(in0, in1, out)     \
{                                    \
    out = __lsx_vadd_h(in0, in1);    \
}

#define LSX_ADD_H_2(in0, in1, in2, in3, out0, out1) \
{                                                   \
    LSX_ADD_H(in0, in1, out0);                      \
    LSX_ADD_H(in2, in3, out1);                      \
}

#define LSX_ADD_H_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3)     \
{                                                                                       \
    LSX_ADD_H_2(in0, in1, in2, in3, out0, out1);                                        \
    LSX_ADD_H_2(in4, in5, in6, in7, out2, out3);                                        \
}

#define LSX_ADD_H_8(in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, in11, in12, \
                    in13, in14, in15, out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                                       \
    LSX_ADD_H_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3);        \
    LSX_ADD_H_4(in8, in9, in10, in11, in12, in13, in14, in15, out4, out5, out6, out7);  \
}

#define LSX_ADD_W(in0, in1, out)       \
{                                      \
    out = __lsx_vadd_w(in0, in1);      \
}

#define LSX_ADD_W_2(in0, in1, in2, in3, out0, out1)  \
{                                                    \
    LSX_ADD_W(in0, in1, out0);                       \
    LSX_ADD_W(in2, in3, out1);                       \
}

#define LSX_ADD_W_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3)  \
{                                                                                    \
    LSX_ADD_W_2(in0, in1, in2, in3, out0, out1);                                     \
    LSX_ADD_W_2(in4, in5, in6, in7, out2, out3);                                     \
}

#define LSX_ADD_W_8(in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, in11, in12, \
                    in13, in14, in15, out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                                       \
    LSX_ADD_W_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3);        \
    LSX_ADD_W_4(in8, in9, in10, in11, in12, in13, in14, in15, out4, out5, out6, out7);  \
}

#define LSX_SUB_W(in0, in1, out)       \
{                                      \
    out = __lsx_vsub_w(in0, in1);      \
}

#define LSX_SUB_W_2(in0, in1, in2, in3, out0, out1)  \
{                                                    \
    LSX_SUB_W(in0, in1, out0);                       \
    LSX_SUB_W(in2, in3, out1);                       \
}

#define LSX_SUB_W_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3)     \
{                                                                                       \
    LSX_SUB_W_2(in0, in1, in2, in3, out0, out1);                                        \
    LSX_SUB_W_2(in4, in5, in6, in7, out2, out3);                                        \
}

#define LSX_SUB_W_8(in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, in11, in12, \
                    in13, in14, in15, out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                                       \
    LSX_SUB_W_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3);        \
    LSX_SUB_W_4(in8, in9, in10, in11, in12, in13, in14, in15, out4, out5, out6, out7);  \
}

/* Description : Butterfly of 4 input vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     : Butterfly operationuu
 */
#define LSX_BUTTERFLY_4(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3)  \
{                                                                           \
    out0 = (__m128i)( (RTYPE)in0 + (RTYPE)in3 );                            \
    out1 = (__m128i)( (RTYPE)in1 + (RTYPE)in2 );                            \
                                                                            \
    out2 = (__m128i)( (RTYPE)in1 - (RTYPE)in2 );                            \
    out3 = (__m128i)( (RTYPE)in0 - (RTYPE)in3 );                            \
}

/* Description : Butterfly of 8 input vectors
 * Arguments   : Inputs  - in0 in1 in2 ~
 *               Outputs - out0 out1 out2 ~
 * Details     : Butterfly operation
 */
#define LSX_BUTTERFLY_8(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,      \
                        out0, out1, out2, out3, out4, out5, out6, out7)     \
{                                                                           \
    out0 = (__m128i)( (RTYPE)in0 + (RTYPE)in7 );                            \
    out1 = (__m128i)( (RTYPE)in1 + (RTYPE)in6 );                            \
    out2 = (__m128i)( (RTYPE)in2 + (RTYPE)in5 );                            \
    out3 = (__m128i)( (RTYPE)in3 + (RTYPE)in4 );                            \
                                                                            \
    out4 = (__m128i)( (RTYPE)in3 - (RTYPE)in4 );                            \
    out5 = (__m128i)( (RTYPE)in2 - (RTYPE)in5 );                            \
    out6 = (__m128i)( (RTYPE)in1 - (RTYPE)in6 );                            \
    out7 = (__m128i)( (RTYPE)in0 - (RTYPE)in7 );                            \
}

#endif /* GENERIC_MACROS_LSX_H */
