/*
 * Copyright (c) 2020 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Shiyou Yin   <yinshiyou-hf@loongson.cn>
 *                Xiwei Gu     <guxiwei-hf@loongson.cn>
 *                Jin Bo       <jinbo@loongson.cn>
 *                Hao Chen     <chenhao@loongson.cn>
 *                Lu Wang      <wanglu@loongson.cn>
 *                Peng Zhou    <zhoupeng@loongson.cn>
 *
 * This file is maintained in LSOM project, don't change it directly.
 * You can get the latest version of this header from: ***
 *
 */

#ifndef GENERIC_MACROS_LASX_H
#define GENERIC_MACROS_LASX_H

#include <stdint.h>
#include <lasxintrin.h>

/**
 * MAJOR version: Macro usage changes.
 * MINOR version: Add new macros, or bug fix.
 * MICRO version: Comment changes or implementation changes。
 */
#define LSOM_LASX_VERSION_MAJOR 3
#define LSOM_LASX_VERSION_MINOR 0
#define LSOM_LASX_VERSION_MICRO 0

/* Description : Load 256-bit vector data with stride
 * Arguments   : Inputs  - psrc    (source pointer to load from)
 *                       - stride
 *               Outputs - out0, out1, ~
 * Details     : Load 256-bit data in 'out0' from (psrc)
 *               Load 256-bit data in 'out1' from (psrc + stride)
 */
#define LASX_LD(psrc) *((__m256i *)(psrc))

#define LASX_LD_2(psrc, stride, out0, out1)                                 \
{                                                                           \
    out0 = LASX_LD(psrc);                                                   \
    out1 = LASX_LD((psrc) + stride);                                        \
}

#define LASX_LD_4(psrc, stride, out0, out1, out2, out3)                     \
{                                                                           \
    LASX_LD_2((psrc), stride, out0, out1);                                  \
    LASX_LD_2((psrc) + 2 * stride , stride, out2, out3);                    \
}

#define LASX_LD_8(psrc, stride, out0, out1, out2, out3, out4, out5,         \
                  out6, out7)                                               \
{                                                                           \
    LASX_LD_4((psrc), stride, out0, out1, out2, out3);                      \
    LASX_LD_4((psrc) + 4 * stride, stride, out4, out5, out6, out7);         \
}

/* Description : Store 256-bit vector data with stride
 * Arguments   : Inputs  - in0, in1, ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store 256-bit data from 'in0' to (pdst)
 *               Store 256-bit data from 'in1' to (pdst + stride)
 */
#define LASX_ST(in, pdst) *((__m256i *)(pdst)) = (in)

#define LASX_ST_2(in0, in1, pdst, stride)                                   \
{                                                                           \
    LASX_ST(in0, (pdst));                                                   \
    LASX_ST(in1, (pdst) + stride);                                          \
}

#define LASX_ST_4(in0, in1, in2, in3, pdst, stride)                         \
{                                                                           \
    LASX_ST_2(in0, in1, (pdst), stride);                                    \
    LASX_ST_2(in2, in3, (pdst) + 2 * stride, stride);                       \
}

#define LASX_ST_8(in0, in1, in2, in3, in4, in5, in6, in7, pdst, stride)     \
{                                                                           \
    LASX_ST_4(in0, in1, in2, in3, (pdst), stride);                          \
    LASX_ST_4(in4, in5, in6, in7, (pdst) + 4 * stride, stride);             \
}

/* Description : Store half word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1,  ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store half word 'idx0' from 'in' to (pdst)
 *               Store half word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : LASX_ST_H(in, idx, pdst)
 *          in : 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
 *        idx0 : 0x01
 *        out0 : 2
 */
#define LASX_ST_H(in, idx, pdst)                                          \
{                                                                         \
    __lasx_xvstelm_h(in, pdst, 0, idx);                                   \
}

#define LASX_ST_H_2(in, idx0, idx1, pdst, stride)                         \
{                                                                         \
    LASX_ST_H(in, idx0, (pdst));                                          \
    LASX_ST_H(in, idx1, (pdst) + stride);                                 \
}

#define LASX_ST_H_4(in, idx0, idx1, idx2, idx3, pdst, stride)             \
{                                                                         \
    LASX_ST_H_2(in, idx0, idx1, (pdst), stride);                          \
    LASX_ST_H_2(in, idx2, idx3, (pdst) + 2 * stride, stride);             \
}


/* Description : Store word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1,  ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store word 'idx0' from 'in' to (pdst)
 *               Store word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : LASX_ST_W(in, idx, pdst)
 *          in : 1, 2, 3, 4, 5, 6, 7, 8
 *        idx0 : 0x01
 *        out0 : 2
 */
#define LASX_ST_W(in, idx, pdst)                                          \
{                                                                         \
    __lasx_xvstelm_w(in, pdst, 0, idx);                                   \
}

#define LASX_ST_W_2(in, idx0, idx1, pdst, stride)                         \
{                                                                         \
    LASX_ST_W(in, idx0, (pdst));                                          \
    LASX_ST_W(in, idx1, (pdst) + stride);                                 \
}

#define LASX_ST_W_4(in, idx0, idx1, idx2, idx3, pdst, stride)             \
{                                                                         \
    LASX_ST_W_2(in, idx0, idx1, (pdst), stride);                          \
    LASX_ST_W_2(in, idx2, idx3, (pdst) + 2 * stride, stride);             \
}

#define LASX_ST_W_8(in, idx0, idx1, idx2, idx3, idx4, idx5, idx6, idx7,   \
                    pdst, stride)                                         \
{                                                                         \
    LASX_ST_W_4(in, idx0, idx1, idx2, idx3, (pdst), stride);              \
    LASX_ST_W_4(in, idx4, idx5, idx6, idx7, (pdst) + 4 * stride, stride); \
}

/* Description : Store double word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1, ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store double word 'idx0' from 'in' to (pdst)
 *               Store double word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : See LASX_ST_W(in, idx, pdst)
 */
#define LASX_ST_D(in, idx, pdst)                                         \
{                                                                        \
    __lasx_xvstelm_d(in, pdst, 0, idx);                                  \
}

#define LASX_ST_D_2(in, idx0, idx1, pdst, stride)                        \
{                                                                        \
    LASX_ST_D(in, idx0, (pdst));                                         \
    LASX_ST_D(in, idx1, (pdst) + stride);                                \
}

#define LASX_ST_D_4(in, idx0, idx1, idx2, idx3, pdst, stride)            \
{                                                                        \
    LASX_ST_D_2(in, idx0, idx1, (pdst), stride);                         \
    LASX_ST_D_2(in, idx2, idx3, (pdst) + 2 * stride, stride);            \
}

/* Description : Store quad word elements of vector with stride
 * Arguments   : Inputs  - in   source vector
 *                       - idx, idx0, idx1, ~
 *                       - pdst    (destination pointer to store to)
 *                       - stride
 * Details     : Store quad word 'idx0' from 'in' to (pdst)
 *               Store quad word 'idx1' from 'in' to (pdst + stride)
 *               Similar for other elements
 * Example     : See LASX_ST_W(in, idx, pdst)
 */
#define LASX_ST_Q(in, idx, pdst)                                         \
{                                                                        \
    LASX_ST_D(in, (idx << 1), pdst);                                     \
    LASX_ST_D(in, (( idx << 1) + 1), (char*)(pdst) + 8);                 \
}

#define LASX_ST_Q_2(in, idx0, idx1, pdst, stride)                        \
{                                                                        \
    LASX_ST_Q(in, idx0, (pdst));                                         \
    LASX_ST_Q(in, idx1, (pdst) + stride);                                \
}

#define LASX_ST_Q_4(in, idx0, idx1, idx2, idx3, pdst, stride)            \
{                                                                        \
    LASX_ST_Q_2(in, idx0, idx1, (pdst), stride);                         \
    LASX_ST_Q_2(in, idx2, idx3, (pdst) + 2 * stride, stride);            \
}

/* Description : Dot product of byte vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - unsigned halfword
 * Details     : Unsigned byte elements from in0 are iniplied with
 *               unsigned byte elements from in0 producing a result
 *               twice the size of input i.e. unsigned halfword.
 *               Then this iniplication results of adjacent odd-even elements
 *               are added together and stored to the out vector
 *               (2 unsigned halfword results)
 * Example     : see LASX_DP2_W_H
 */
#define LASX_DP2_H_BU(in0, in1, out0)                   \
{                                                       \
    __m256i _tmp0_m ;                                   \
                                                        \
    _tmp0_m = __lasx_xvmulwev_h_bu( in0, in1 );         \
    out0 = __lasx_xvmaddwod_h_bu( _tmp0_m, in0, in1 );  \
}
#define LASX_DP2_H_BU_2(in0, in1, in2, in3, out0, out1) \
{                                                       \
    LASX_DP2_H_BU(in0, in1, out0);                      \
    LASX_DP2_H_BU(in2, in3, out1);                      \
}
#define LASX_DP2_H_BU_4(in0, in1, in2, in3,             \
                        in4, in5, in6, in7,             \
                        out0, out1, out2, out3)         \
{                                                       \
    LASX_DP2_H_BU_2(in0, in1, in0, in1, out0, out1);    \
    LASX_DP2_H_BU_2(in4, in5, in6, in7, out2, out3);    \
}

/* Description : Dot product of byte vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - signed halfword
 * Details     : Signed byte elements from in0 are iniplied with
 *               signed byte elements from in0 producing a result
 *               twice the size of input i.e. signed halfword.
 *               Then this iniplication results of adjacent odd-even elements
 *               are added together and stored to the out vector
 *               (2 signed halfword results)
 * Example     : see LASX_DP2_W_H
 */
#define LASX_DP2_H_B(in0, in1, out0)                      \
{                                                         \
    __m256i _tmp0_m ;                                     \
                                                          \
    _tmp0_m = __lasx_xvmulwev_h_b( in0, in1 );            \
    out0 = __lasx_xvmaddwod_h_b( _tmp0_m, in0, in1 );     \
}
#define LASX_DP2_H_B_2(in0, in1, in2, in3, out0, out1)    \
{                                                         \
    LASX_DP2_H_B(in0, in1, out0);                         \
    LASX_DP2_H_B(in2, in3, out1);                         \
}
#define LASX_DP2_H_B_4(in0, in1, in2, in3,                \
                       in4, in5, in6, in7,                \
                       out0, out1, out2, out3)            \
{                                                         \
    LASX_DP2_H_B_2(in0, in1, in2, in3, out0, out1);       \
    LASX_DP2_H_B_2(in4, in5, in6, in7, out2, out3);       \
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
 * Example     : LASX_DP2_W_H(in0, in1, out0)
 *               in0:   1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8,
 *               in0:   8,7,6,5, 4,3,2,1, 8,7,6,5, 4,3,2,1,
 *               out0:  22,38,38,22, 22,38,38,22
 */
#define LASX_DP2_W_H(in0, in1, out0)                   \
{                                                      \
    __m256i _tmp0_m ;                                  \
                                                       \
    _tmp0_m = __lasx_xvmulwev_w_h( in0, in1 );         \
    out0 = __lasx_xvmaddwod_w_h( _tmp0_m, in0, in1 );  \
}
#define LASX_DP2_W_H_2(in0, in1, in2, in3, out0, out1)             \
{                                                                  \
    LASX_DP2_W_H(in0, in1, out0);                                  \
    LASX_DP2_W_H(in2, in3, out1);                                  \
}
#define LASX_DP2_W_H_4(in0, in1, in2, in3,                         \
                       in4, in5, in6, in7, out0, out1, out2, out3) \
{                                                                  \
    LASX_DP2_W_H_2(in0, in1, in2, in3, out0, out1);                \
    LASX_DP2_W_H_2(in4, in5, in6, in7, out2, out3);                \
}
#define LASX_DP2_W_H_8(in0, in1, in2, in3, in4, in5, in6, in7,         \
                       in8, in9, in10, in11, in12, in13, in14, in15,   \
                       out0, out1, out2, out3, out4, out5, out6, out7) \
{                                                                      \
    LASX_DP2_W_H_4(in0, in1, in2, in3, in4, in5, in6, in7,             \
                   out0, out1, out2, out3);                            \
    LASX_DP2_W_H_4(in8, in9, in10, in11, in12, in13, in14, in15,       \
                   out4, out5, out6, out7);                            \
}

/* Description : Dot product of word vector elements
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 *               Retrun Type - signed double
 * Details     : Signed word elements from in* are iniplied with
 *               signed word elements from in* producing a result
 *               twice the size of input i.e. signed double word.
 *               Then this iniplication results of adjacent odd-even elements
 *               are added together and stored to the out vector.
 * Example     : see LASX_DP2_W_H
 */
#define LASX_DP2_D_W(in0, in1, out0)                    \
{                                                       \
    __m256i _tmp0_m ;                                   \
                                                        \
    _tmp0_m = __lasx_xvmulwev_d_w( in0, in1 );          \
    out0 = __lasx_xvmaddwod_d_w( _tmp0_m, in0, in1 );   \
}
#define LASX_DP2_D_W_2(in0, in1, in2, in3, out0, out1)  \
{                                                       \
    LASX_DP2_D_W(in0, in1, out0);                       \
    LASX_DP2_D_W(in2, in3, out1);                       \
}
#define LASX_DP2_D_W_4(in0, in1, in2, in3,                             \
                       in4, in5, in6, in7, out0, out1, out2, out3)     \
{                                                                      \
    LASX_DP2_D_W_2(in0, in1, in2, in3, out0, out1);                    \
    LASX_DP2_D_W_2(in4, in5, in6, in7, out2, out3);                    \
}
#define LASX_DP2_D_W_8(in0, in1, in2, in3, in4, in5, in6, in7,         \
                       in8, in9, in10, in11, in12, in13, in14, in15,   \
                       out0, out1, out2, out3, out4, out5, out6, out7) \
{                                                                      \
    LASX_DP2_D_W_4(in0, in1, in2, in3, in4, in5, in6, in7,             \
                   out0, out1, out2, out3);                            \
    LASX_DP2_D_W_4(in8, in9, in10, in11, in12, in13, in14, in15,       \
                   out4, out5, out6, out7);                            \
}

/* Description : Dot product of halfword vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Unsigned halfword elements from 'in0' are iniplied with
 *               halfword elements from 'in0' producing a result
 *               twice the size of input i.e. unsigned word.
 *               Multiplication result of adjacent odd-even elements
 *               are added together and written to the 'out0' vector
 */
#define LASX_DP2_W_HU_H(in0, in1, out0)                   \
{                                                         \
    __m256i _tmp0_m;                                      \
                                                          \
    _tmp0_m = __lasx_xvmulwev_w_hu_h( in0, in1 );         \
    out0 = __lasx_xvmaddwod_w_hu_h( _tmp0_m, in0, in1 );  \
}

#define LASX_DP2_W_HU_H_2(in0, in1, in2, in3, out0, out1) \
{                                                         \
    LASX_DP2_W_HU_H(in0, in1, out0);                      \
    LASX_DP2_W_HU_H(in2, in3, out1);                      \
}

#define LASX_DP2_W_HU_H_4(in0, in1, in2, in3,             \
                          in4, in5, in6, in7,             \
                          out0, out1, out2, out3)         \
{                                                         \
    LASX_DP2_W_HU_H_2(in0, in1, in2, in3, out0, out1);    \
    LASX_DP2_W_HU_H_2(in4, in5, in6, in7, out2, out3);    \
}

/* Description : Dot product & addition of byte vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Retrun Type - halfword
 * Details     : Signed byte elements from in0 are iniplied with
 *               signed byte elements from in0 producing a result
 *               twice the size of input i.e. signed halfword.
 *               Then this iniplication results of adjacent odd-even elements
 *               are added to the out vector
 *               (2 signed halfword results)
 * Example     : LASX_DP2ADD_H_B(in0, in1, in2, out0)
 *               in0:  1,2,3,4, 1,2,3,4, 1,2,3,4, 1,2,3,4,
 *               in1:  1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8,
 *                     1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8
 *               in2:  8,7,6,5, 4,3,2,1, 8,7,6,5, 4,3,2,1,
 *                     8,7,6,5, 4,3,2,1, 8,7,6,5, 4,3,2,1
 *               out0: 23,40,41,26, 23,40,41,26, 23,40,41,26, 23,40,41,26
 */
#define LASX_DP2ADD_H_B(in0, in1, in2, out0)                 \
{                                                            \
    __m256i _tmp0_m;                                         \
                                                             \
    _tmp0_m = __lasx_xvmaddwev_h_b( in0, in1, in2 );         \
    out0 = __lasx_xvmaddwod_h_b( _tmp0_m, in1, in2 );        \
}
#define LASX_DP2ADD_H_B_2(in0, in1, in2, in3, in4, in5, out0, out1)  \
{                                                                    \
    LASX_DP2ADD_H_B(in0, in1, in2, out0);                            \
    LASX_DP2ADD_H_B(in3, in4, in5, out1);                            \
}
#define LASX_DP2ADD_H_B_4(in0, in1, in2, in3, in4, in5,                \
                          in6, in7, in8, in9, in10, in11,              \
                          out0, out1, out2, out3)                      \
{                                                                      \
    LASX_DP2ADD_H_B_2(in0, in1, in2, in3, in4, in5, out0, out1);       \
    LASX_DP2ADD_H_B_2(in6, in7, in8, in9, in10, in11, out2, out3);     \
}

/* Description : Dot product of halfword vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Signed halfword elements from 'in0' are iniplied with
 *               signed halfword elements from 'in0' producing a result
 *               twice the size of input i.e. signed word.
 *               Multiplication result of adjacent odd-even elements
 *               are added together and written to the 'out0' vector
 */
#define LASX_DP2ADD_W_H(in0, in1, in2, out0)                 \
{                                                            \
    __m256i _tmp0_m;                                         \
                                                             \
    _tmp0_m = __lasx_xvmaddwev_w_h( in0, in1, in2 );         \
    out0 = __lasx_xvmaddwod_w_h( _tmp0_m, in1, in2 );        \
}
#define LASX_DP2ADD_W_H_2(in0, in1, in2, in3, in4, in5, out0, out1 ) \
{                                                                    \
    LASX_DP2ADD_W_H(in0, in1, in2, out0);                            \
    LASX_DP2ADD_W_H(in3, in4, in5, out1);                            \
}
#define LASX_DP2ADD_W_H_4(in0, in1, in2, in3, in4, in5,              \
                          in6, in7, in8, in9, in10, in11,            \
                          out0, out1, out2, out3)                    \
{                                                                    \
    LASX_DP2ADD_W_H_2(in0, in1, in2, in3, in4, in5, out0, out1);     \
    LASX_DP2ADD_W_H_2(in6, in7, in8, in9, in10, in11, out2, out3);   \
}

/* Description : Dot product of halfword vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Unsigned halfword elements from 'in0' are iniplied with
 *               unsigned halfword elements from 'in0' producing a result
 *               twice the size of input i.e. unsigned word.
 *               Multiplication result of adjacent odd-even elements
 *               are added together and written to the 'out0' vector
 */
#define LASX_DP2ADD_W_HU(in0, in1, in2, out0)          \
{                                                      \
    __m256i _tmp0_m;                                   \
                                                       \
    _tmp0_m = __lasx_xvmaddwev_w_hu( in0, in1, in2 );  \
    out0 = __lasx_xvmaddwod_w_hu( _tmp0_m, in1, in2 ); \
}
#define LASX_DP2ADD_W_HU_2(in0, in1, in2, in3, in4, in5, out0, out1) \
{                                                                    \
    LASX_DP2ADD_W_HU(in0, in1, in2, out0);                           \
    LASX_DP2ADD_W_HU(in3, in4, in5, out1);                           \
}
#define LASX_DP2ADD_W_HU_4(in0, in1, in2, in3, in4, in5,             \
                           in6, in7, in8, in9, in10, in11,           \
                           out0, out1, out2, out3)                   \
{                                                                    \
    LASX_DP2ADD_W_HU_2(in0, in1, in2, in3, in4, in5, out0, out1);    \
    LASX_DP2ADD_W_HU_2(in6, in7, in8, in9, in10, in11, out2, out3);  \
}

/* Description : Dot product of halfword vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Unsigned halfword elements from 'in0' are iniplied with
 *               halfword elements from 'in0' producing a result
 *               twice the size of input i.e. unsigned word.
 *               Multiplication result of adjacent odd-even elements
 *               are added together and written to the 'out0' vector
 */
#define LASX_DP2ADD_W_HU_H(in0, in1, in2, out0)           \
{                                                         \
    __m256i _tmp0_m;                                      \
                                                          \
    _tmp0_m = __lasx_xvmaddwev_w_hu_h( in0, in1, in2 );   \
    out0 = __lasx_xvmaddwod_w_hu_h( _tmp0_m, in1, in2 );  \
}

#define LASX_DP2ADD_W_HU_H_2(in0, in1, in2, in3, in4, in5, out0, out1) \
{                                                                      \
    LASX_DP2ADD_W_HU_H(in0, in1, in2, out0);                           \
    LASX_DP2ADD_W_HU_H(in3, in4, in5, out1);                           \
}

#define LASX_DP2ADD_W_HU_H_4(in0, in1, in2, in3, in4, in5,             \
                             in6, in7, in8, in9, in10, in11,           \
                             out0, out1, out2, out3)                   \
{                                                                      \
    LASX_DP2ADD_W_HU_H_2(in0, in1, in2, in3, in4, in5, out0, out1);    \
    LASX_DP2ADD_W_HU_H_2(in6, in7, in8, in9, in10, in11, out2, out3);  \
}

/* Description : Vector Unsigned Dot Product and Subtract.
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Unsigned byte elements from 'in0' are iniplied with
 *               unsigned byte elements from 'in0' producing a result
 *               twice the size of input i.e. signed word.
 *               Multiplication result of adjacent odd-even elements
 *               are added together and subtract from double width elements,
 *               then written to the 'out0' vector.
 */
#define LASX_DP2SUB_H_BU(in0, in1, in2, out0)             \
{                                                         \
    __m256i _tmp0_m;                                      \
                                                          \
    _tmp0_m = __lasx_xvmulwev_h_bu( in1, in2 );           \
    _tmp0_m = __lasx_xvmaddwod_h_bu( _tmp0_m, in1, in2 ); \
    out0 = __lasx_xvsub_h( in0, _tmp0_m );                \
}

#define LASX_DP2SUB_H_BU_2(in0, in1, in2, in3, in4, in5, out0, out1) \
{                                                                    \
    LASX_DP2SUB_H_BU(in0, in1, in2, out0);                           \
    LASX_DP2SUB_H_BU(in0, in1, in2, out0);                           \
}

#define LASX_DP2SUB_H_BU_4(in0, in1, in2, in3, in4, in5,             \
                           in6, in7, in8, in9, in10, in11,           \
                           out0, out1, out2, out3)                   \
{                                                                    \
    LASX_DP2SUB_H_BU_2(in0, in1, in2, in3, in4, in5, out0, out1);    \
    LASX_DP2SUB_H_BU_2(in6, in7, in8, in9, in10, in11, out2, out3);  \
}

/* Description : Vector Signed Dot Product and Subtract.
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Signed halfword elements from 'in0' are iniplied with
 *               signed halfword elements from 'in0' producing a result
 *               twice the size of input i.e. signed word.
 *               Multiplication result of adjacent odd-even elements
 *               are added together and subtract from double width elements,
 *               then written to the 'out0' vector.
 */
#define LASX_DP2SUB_W_H(in0, in1, in2, out0)             \
{                                                        \
    __m256i _tmp0_m;                                     \
                                                         \
    _tmp0_m = __lasx_xvmulwev_w_h( in1, in2 );           \
    _tmp0_m = __lasx_xvmaddwod_w_h( _tmp0_m, in1, in2 ); \
    out0 = __lasx_xvsub_w( in0, _tmp0_m );               \
}

#define LASX_DP2SUB_W_H_2(in0, in1, in2, in3, in4, in5, out0, out1) \
{                                                                   \
    LASX_DP2SUB_W_H(in0, in1, in2, out0);                           \
    LASX_DP2SUB_W_H(in3, in4, in5, out1);                           \
}

#define LASX_DP2SUB_W_H_4(in0, in1, in2, in3, in4, in5,             \
                          in6, in7, in8, in9, in10, in11,           \
                          out0, out1, out2, out3)                   \
{                                                                   \
    LASX_DP2SUB_W_H_2(in0, in1, in2, in3, in4, in5, out0, out1);    \
    LASX_DP2SUB_W_H_2(in6, in7, in8, in9, in10, in11, out2, out3);  \
}

/* Description : Dot product of half word vector elements
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 *               Return Type - signed word
 * Details     : Signed half word elements from in* are iniplied with
 *               signed half word elements from in* producing a result
 *               twice the size of input i.e. signed word.
 *               Then this iniplication results of four adjacent elements
 *               are added together and stored to the out vector.
 * Example     : LASX_DP2_W_H(in0, in0, out0)
 *               in0:   3,1,3,0, 0,0,0,1, 0,0,1,-1, 0,0,0,1,
 *               in0:   -2,1,1,0, 1,0,0,0, 0,0,1,0, 1,0,0,1,
 *               out0:  -2,0,1,1,
 */
#define LASX_DP4_D_H(in0, in1, out0)                         \
{                                                            \
    __m256i _tmp0_m ;                                        \
                                                             \
    _tmp0_m = __lasx_xvmulwev_w_h( in0, in1 );               \
    _tmp0_m = __lasx_xvmaddwod_w_h( _tmp0_m, in0, in1 );     \
    out0  = __lasx_xvhaddw_d_w( _tmp0_m, _tmp0_m );          \
}
#define LASX_DP4_D_H_2(in0, in1, in2, in3, out0, out1)       \
{                                                            \
    LASX_DP4_D_H(in0, in1, out0);                            \
    LASX_DP4_D_H(in2, in3, out1);                            \
}
#define LASX_DP4_D_H_4(in0, in1, in2, in3,                            \
                       in4, in5, in6, in7, out0, out1, out2, out3)    \
{                                                                     \
    LASX_DP4_D_H_2(in0, in1, in2, in3, out0, out1);                   \
    LASX_DP4_D_H_2(in4, in5, in6, in7, out2, out3);                   \
}

/* Description : The high half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in0 vector and the in1 vector are added after the
 *               higher half of the two-fold sign extension ( signed byte
 *               to signed half word ) and stored to the out vector.
 * Example     : see LASX_ADDWL_W_H_128SV
 */
#define LASX_ADDWH_H_B_128SV(in0, in1, out0)                                  \
{                                                                             \
    __m256i _tmp0_m, _tmp1_m;                                                 \
                                                                              \
    _tmp0_m = __lasx_xvilvh_b( in0, in0 );                                    \
    _tmp1_m = __lasx_xvilvh_b( in1, in1 );                                    \
    out0 = __lasx_xvaddwev_h_b( _tmp0_m, _tmp1_m );                           \
}
#define LASX_ADDWH_H_B_2_128SV(in0, in1, in2, in3, out0, out1)                \
{                                                                             \
    LASX_ADDWH_H_B_128SV(in0, in1, out0);                                     \
    LASX_ADDWH_H_B_128SV(in2, in3, out1);                                     \
}
#define LASX_ADDWH_H_B_4_128SV(in0, in1, in2, in3,                            \
                               in4, in5, in6, in7, out0, out1, out2, out3)    \
{                                                                             \
    LASX_ADDWH_H_B_2_128SV(in0, in1, in2, in3, out0, out1);                   \
    LASX_ADDWH_H_B_2_128SV(in4, in5, in6, in7, out2, out3);                   \
}

/* Description : The high half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in0 vector and the in1 vector are added after the
 *               higher half of the two-fold sign extension ( signed half word
 *               to signed word ) and stored to the out vector.
 * Example     : see LASX_ADDWL_W_H_128SV
 */
#define LASX_ADDWH_W_H_128SV(in0, in1, out0)                                  \
{                                                                             \
    __m256i _tmp0_m, _tmp1_m;                                                 \
                                                                              \
    _tmp0_m = __lasx_xvilvh_h( in0, in0 );                                    \
    _tmp1_m = __lasx_xvilvh_h( in1, in1 );                                    \
    out0 = __lasx_xvaddwev_w_h( _tmp0_m, _tmp1_m );                           \
}
#define LASX_ADDWH_W_H_2_128SV(in0, in1, in2, in3, out0, out1)                \
{                                                                             \
    LASX_ADDWH_W_H_128SV(in0, in1, out0);                                     \
    LASX_ADDWH_W_H_128SV(in2, in3, out1);                                     \
}
#define LASX_ADDWH_W_H_4_128SV(in0, in1, in2, in3,                            \
                               in4, in5, in6, in7, out0, out1, out2, out3)    \
{                                                                             \
    LASX_ADDWH_W_H_2_128SV(in0, in1, in2, in3, out0, out1);                   \
    LASX_ADDWH_W_H_2_128SV(in4, in5, in6, in7, out2, out3);                   \
}

/* Description : The low half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in0 vector and the in1 vector are added after the
 *               lower half of the two-fold sign extension ( signed byte
 *               to signed half word ) and stored to the out vector.
 * Example     : see LASX_ADDWL_W_H_128SV
 */
#define LASX_ADDWL_H_B_128SV(in0, in1, out0)                                  \
{                                                                             \
    __m256i _tmp0_m, _tmp1_m;                                                 \
                                                                              \
    _tmp0_m = __lasx_xvsllwil_h_b( in0, 0 );                                  \
    _tmp1_m = __lasx_xvsllwil_h_b( in1, 0 );                                  \
    out0 = __lasx_xvadd_h( _tmp0_m, _tmp1_m );                                \
}
#define LASX_ADDWL_H_B_2_128SV(in0, in1, in2, in3, out0, out1)                \
{                                                                             \
    LASX_ADDWL_H_B_128SV(in0, in1, out0);                                     \
    LASX_ADDWL_H_B_128SV(in2, in3, out1);                                     \
}
#define LASX_ADDWL_H_B_4_128SV(in0, in1, in2, in3,                            \
                               in4, in5, in6, in7, out0, out1, out2, out3)    \
{                                                                             \
    LASX_ADDWL_H_B_2_128SV(in0, in1, in2, in3, out0, out1);                   \
    LASX_ADDWL_H_B_2_128SV(in4, in5, in6, in7, out2, out3);                   \
}

/* Description : The low half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in0 vector and the in1 vector are added after the
 *               lower half of the two-fold sign extension ( signed half word
 *               to signed word ) and stored to the out vector.
 * Example     : LASX_ADDWL_W_H_128SV(in0, in1, out0)
 *               in0   3,0,3,0, 0,0,0,-1, 0,0,1,-1, 0,0,0,1,
 *               in1   2,-1,1,2, 1,0,0,0, 1,0,1,0, 1,0,0,1,
 *               out0  5,-1,4,2, 1,0,2,-1,
 */
#define LASX_ADDWL_W_H_128SV(in0, in1, out0)                                  \
{                                                                             \
    __m256i _tmp0_m;                                                          \
                                                                              \
    _tmp0_m = __lasx_xvilvl_h(in1, in0);                                      \
    out0 = __lasx_xvhaddw_w_h( _tmp0_m, _tmp0_m );                            \
}
#define LASX_ADDWL_W_H_2_128SV(in0, in1, in2, in3, out0, out1)                \
{                                                                             \
    LASX_ADDWL_W_H_128SV(in0, in1, out0);                                     \
    LASX_ADDWL_W_H_128SV(in2, in3, out1);                                     \
}
#define LASX_ADDWL_W_H_4_128SV(in0, in1, in2, in3,                            \
                               in4, in5, in6, in7, out0, out1, out2, out3)    \
{                                                                             \
    LASX_ADDWL_W_H_2_128SV(in0, in1, in2, in3, out0, out1);                   \
    LASX_ADDWL_W_H_2_128SV(in4, in5, in6, in7, out2, out3);                   \
}

/* Description : The low half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in0 vector and the in1 vector are added after the
 *               lower half of the two-fold zero extension ( unsigned byte
 *               to unsigned half word ) and stored to the out vector.
 */
#define LASX_ADDWL_H_BU_128SV(in0, in1, out0)                                 \
{                                                                             \
    __m256i _tmp0_m;                                                          \
                                                                              \
    _tmp0_m = __lasx_xvilvl_b(in1, in0);                                      \
    out0 = __lasx_xvhaddw_hu_bu( _tmp0_m, _tmp0_m );                          \
}
#define LASX_ADDWL_H_BU_2_128SV(in0, in1, in2, in3, out0, out1)               \
{                                                                             \
    LASX_ADDWL_H_BU_128SV(in0, in1, out0);                                    \
    LASX_ADDWL_H_BU_128SV(in2, in3, out1);                                    \
}
#define LASX_ADDWL_H_BU_4_128SV(in0, in1, in2, in3,                           \
                                in4, in5, in6, in7, out0, out1, out2, out3)   \
{                                                                             \
    LASX_ADDWL_H_BU_2_128SV(in0, in1, in2, in3, out0, out1);                  \
    LASX_ADDWL_H_BU_2_128SV(in4, in5, in6, in7, out2, out3);                  \
}

/* Description : The low half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : In1 vector plus in0 vector after double zero extension
 *               ( unsigned byte to half word ),add and stored to the out vector.
 * Example     : reference to LASX_ADDW_W_W_H_128SV(in0, in1, out0)
 */
#define LASX_ADDW_H_H_BU_128SV(in0, in1, out0)                                \
{                                                                             \
    __m256i _tmp1_m;                                                          \
                                                                              \
    _tmp1_m = __lasx_xvsllwil_hu_bu( in1, 0 );                                \
    out0 = __lasx_xvadd_h( in0, _tmp1_m );                                    \
}
#define LASX_ADDW_H_H_BU_2_128SV(in0, in1, in2, in3, out0, out1)              \
{                                                                             \
    LASX_ADDW_H_H_BU_128SV(in0, in1, out0);                                   \
    LASX_ADDW_H_H_BU_128SV(in2, in3, out1);                                   \
}
#define LASX_ADDW_H_H_BU_4_128SV(in0, in1, in2, in3,                          \
                                 in4, in5, in6, in7, out0, out1, out2, out3)  \
{                                                                             \
    LASX_ADDW_H_H_BU_2_128SV(in0, in1, in2, in3, out0, out1);                 \
    LASX_ADDW_H_H_BU_2_128SV(in4, in5, in6, in7, out2, out3);                 \
}

/* Description : The low half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : In1 vector plus in0 vector after double sign extension
 *               ( signed half word to word ),add and stored to the out vector.
 * Example     : LASX_ADDW_W_W_H_128SV(in0, in1, out0)
 *               in0   0,1,0,0, -1,0,0,1,
 *               in1   2,-1,1,2, 1,0,0,0, 0,0,1,0, 1,0,0,1,
 *               out0  2,0,1,2, -1,0,1,1,
 */
#define LASX_ADDW_W_W_H_128SV(in0, in1, out0)                                 \
{                                                                             \
    __m256i _tmp1_m;                                                          \
                                                                              \
    _tmp1_m = __lasx_xvsllwil_w_h( in1, 0 );                                  \
    out0 = __lasx_xvadd_w( in0, _tmp1_m );                                    \
}
#define LASX_ADDW_W_W_H_2_128SV(in0, in1, in2, in3, out0, out1)               \
{                                                                             \
    LASX_ADDW_W_W_H_128SV(in0, in1, out0);                                    \
    LASX_ADDW_W_W_H_128SV(in2, in3, out1);                                    \
}
#define LASX_ADDW_W_W_H_4_128SV(in0, in1, in2, in3,                           \
                                in4, in5, in6, in7, out0, out1, out2, out3)   \
{                                                                             \
    LASX_ADDW_W_W_H_2_128SV(in0, in1, in2, in3, out0, out1);                  \
    LASX_ADDW_W_W_H_2_128SV(in4, in5, in6, in7, out2, out3);                  \
}

/* Description : Multiplication and addition calculation after expansion
 *               of the lower half of the vector
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in1 vector and the in0 vector are multiplied after
 *               the lower half of the two-fold sign extension ( signed
 *               half word to signed word ) , and the result is added to
 *               the vector in0, the stored to the out vector.
 * Example     : LASX_MADDWL_W_H_128SV(in0, in1, in2, out0)
 *               in0   1,2,3,4, 5,6,7 8
 *               in1   1,2,3,4, 1,2,3,4, 5,6,7,8, 5,6,7,8
 *               in2   200,300,400,500, 2000,3000,4000,5000,
 *                     -200,-300,-400,-500, -2000,-3000,-4000,-5000
 *               out0  5,-1,4,2, 1,0,2,-1,
 */
#define LASX_MADDWL_W_H_128SV(in0, in1, in2, out0)                            \
{                                                                             \
    __m256i _tmp0_m, _tmp1_m;                                                 \
                                                                              \
    _tmp0_m = __lasx_xvsllwil_w_h( in1, 0 );                                  \
    _tmp1_m = __lasx_xvsllwil_w_h( in2, 0 );                                  \
    _tmp0_m = __lasx_xvmul_w( _tmp0_m, _tmp1_m );                             \
    out0 = __lasx_xvadd_w( _tmp0_m, in0 );                                    \
}
#define LASX_MADDWL_W_H_2_128SV(in0, in1, in2, in3, in4, in5, out0, out1)     \
{                                                                             \
    LASX_MADDWL_W_H_128SV(in0, in1, in2, out0);                               \
    LASX_MADDWL_W_H_128SV(in3, in4, in5, out1);                               \
}
#define LASX_MADDWL_W_H_4_128SV(in0, in1, in2, in3, in4, in5,                \
                                in6, in7, in8, in9, in10, in11,              \
                                out0, out1, out2, out3)                      \
{                                                                            \
    LASX_MADDWL_W_H_2_128SV(in0, in1, in2, in3, in4, in5, out0, out1);       \
    LASX_MADDWL_W_H_2_128SV(in6, in7, in8, in9, in10, in11, out2, out3);     \
}

/* Description : Multiplication and addition calculation after expansion
 *               of the higher half of the vector
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in1 vector and the in0 vector are multiplied after
 *               the higher half of the two-fold sign extension ( signed
 *               half word to signed word ) , and the result is added to
 *               the vector in0, the stored to the out vector.
 * Example     : see LASX_MADDWL_W_H_128SV
 */
#define LASX_MADDWH_W_H_128SV(in0, in1, in2, out0)                            \
{                                                                             \
    __m256i _tmp0_m, _tmp1_m;                                                 \
                                                                              \
    _tmp0_m = __lasx_xvilvh_h( in1, in1 );                                    \
    _tmp1_m = __lasx_xvilvh_h( in2, in2 );                                    \
    _tmp0_m = __lasx_xvmulwev_w_h( _tmp0_m, _tmp1_m );                        \
    out0 = __lasx_xvadd_w( _tmp0_m, in0 );                                    \
}
#define LASX_MADDWH_W_H_2_128SV(in0, in1, in2, in3, in4, in5, out0, out1)     \
{                                                                             \
    LASX_MADDWH_W_H_128SV(in0, in1, in2, out0);                               \
    LASX_MADDWH_W_H_128SV(in3, in4, in5, out1);                               \
}
#define LASX_MADDWH_W_H_4_128SV(in0, in1, in2, in3, in4, in5,                \
                                in6, in7, in8, in9, in10, in11,              \
                                out0, out1, out2, out3)                      \
{                                                                            \
    LASX_MADDWH_W_H_2_128SV(in0, in1, in2, in3, in4, in5, out0, out1);       \
    LASX_MADDWH_W_H_2_128SV(in6, in7, in8, in9, in10, in11, out2, out3);     \
}

/* Description : Multiplication calculation after expansion
 *               of the lower half of the vector
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in1 vector and the in0 vector are multiplied after
 *               the lower half of the two-fold sign extension ( signed
 *               half word to signed word ) , the stored to the out vector.
 * Example     : LASX_MULWL_W_H_128SV(in0, in1, out0)
 *               in0   3,-1,3,0, 0,0,0,-1, 0,0,1,-1, 0,0,0,1,
 *               in1   2,-1,1,2, 1,0,0,0,  0,0,1,0, 1,0,0,1,
 *               out0  6,1,3,0, 0,0,1,0,
 */
#define LASX_MULWL_W_H_128SV(in0, in1, out0)                    \
{                                                               \
    __m256i _tmp0_m, _tmp1_m;                                   \
                                                                \
    _tmp0_m = __lasx_xvsllwil_w_h( in0, 0 );                    \
    _tmp1_m = __lasx_xvsllwil_w_h( in1, 0 );                    \
    out0 = __lasx_xvmul_w( _tmp0_m, _tmp1_m );                  \
}
#define LASX_MULWL_W_H_2_128SV(in0, in1, in2, in3, out0, out1)  \
{                                                               \
    LASX_MULWL_W_H_128SV(in0, in1, out0);                       \
    LASX_MULWL_W_H_128SV(in2, in3, out1);                       \
}
#define LASX_MULWL_W_H_4_128SV(in0, in1, in2, in3,              \
                               in4, in5, in6, in7,              \
                               out0, out1, out2, out3)          \
{                                                               \
    LASX_MULWL_W_H_2_128SV(in0, in1, in2, in3, out0, out1);     \
    LASX_MULWL_W_H_2_128SV(in4, in5, in6, in7, out2, out3);     \
}

/* Description : Multiplication calculation after expansion
 *               of the lower half of the vector
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in1 vector and the in0 vector are multiplied after
 *               the lower half of the two-fold sign extension ( signed
 *               half word to signed word ) , the stored to the out vector.
 * Example     : see LASX_MULWL_W_H_128SV
 */
#define LASX_MULWH_W_H_128SV(in0, in1, out0)                    \
{                                                               \
    __m256i _tmp0_m, _tmp1_m;                                   \
                                                                \
    _tmp0_m = __lasx_xvilvh_h( in0, in0 );                      \
    _tmp1_m = __lasx_xvilvh_h( in1, in1 );                      \
    out0 = __lasx_xvmulwev_w_h( _tmp0_m, _tmp1_m );             \
}
#define LASX_MULWH_W_H_2_128SV(in0, in1, in2, in3, out0, out1)  \
{                                                               \
    LASX_MULWH_W_H_128SV(in0, in1, out0);                       \
    LASX_MULWH_W_H_128SV(in2, in3, out1);                       \
}
#define LASX_MULWH_W_H_4_128SV(in0, in1, in2, in3,              \
                               in4, in5, in6, in7,              \
                               out0, out1, out2, out3)          \
{                                                               \
    LASX_MULWH_W_H_2_128SV(in0, in1, in2, in3, out0, out1);     \
    LASX_MULWH_W_H_2_128SV(in4, in5, in6, in7, out2, out3);     \
}

/* Description : The low half of the vector elements are expanded and
 *               added after being doubled
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0,  out1,  ~
 * Details     : The in1 vector add the in0 vector after the
 *               lower half of the two-fold zero extension ( unsigned byte
 *               to unsigned half word ) and stored to the out vector.
 */
#define LASX_SADDW_HU_HU_BU_128SV(in0, in1, out0)                    \
{                                                                    \
    __m256i _tmp1_m;                                                 \
    __m256i _zero_m = { 0 };                                         \
                                                                     \
    _tmp1_m = __lasx_xvilvl_b( _zero_m, in1 );                       \
    out0 = __lasx_xvsadd_hu( in0, _tmp1_m );                         \
}
#define LASX_SADDW_HU_HU_BU_2_128SV(in0, in1, in2, in3, out0, out1)  \
{                                                                    \
    LASX_SADDW_HU_HU_BU_128SV(in0, in1, out0);                       \
    LASX_SADDW_HU_HU_BU_128SV(in2, in3, out1);                       \
}
#define LASX_SADDW_HU_HU_BU_4_128SV(in0, in1, in2, in3,              \
                                    in4, in5, in6, in7,              \
                                    out0, out1, out2, out3)          \
{                                                                    \
    LASX_SADDW_HU_HU_BU_2_128SV(in0, in1, in2, in3, out0, out1);     \
    LASX_SADDW_HU_HU_BU_2_128SV(in4, in5, in6, in7, out2, out3);     \
}

/* Description : Low 8-bit vector elements unsigned extension to halfword
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low 8-bit elements from in0 unsigned extension to halfword,
 *               written to output vector out0. Similar for in1.
 * Example     : See LASX_UNPCK_L_W_H(in0, out0)
 */
#define LASX_UNPCK_L_HU_BU(in0, out0)                                          \
{                                                                              \
    out0 = __lasx_vext2xv_hu_bu(in0);                                          \
}

#define LASX_UNPCK_L_HU_BU_2(in0, in1, out0, out1)                             \
{                                                                              \
    LASX_UNPCK_L_HU_BU(in0, out0);                                             \
    LASX_UNPCK_L_HU_BU(in1, out1);                                             \
}

#define LASX_UNPCK_L_HU_BU_4(in0, in1, in2, in3, out0, out1, out2, out3)       \
{                                                                              \
    LASX_UNPCK_L_HU_BU_2(in0, in1, out0, out1);                                \
    LASX_UNPCK_L_HU_BU_2(in2, in3, out2, out3);                                \
}

#define LASX_UNPCK_L_HU_BU_8(in0, in1, in2, in3, in4, in5, in6, in7,           \
                             out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                              \
    LASX_UNPCK_L_HU_BU_4(in0, in1, in2, in3, out0, out1, out2, out3);          \
    LASX_UNPCK_L_HU_BU_4(in4, in5, in6, in7, out4, out5, out6, out7);          \
}

/* Description : Low 8-bit vector elements unsigned extension to word
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low 8-bit elements from in0 unsigned extension to word,
 *               written to output vector out0. Similar for in1.
 * Example     : See LASX_UNPCK_L_W_H(in0, out0)
 */
#define LASX_UNPCK_L_WU_BU(in0, out0)                                         \
{                                                                             \
    out0 = __lasx_vext2xv_wu_bu(in0);                                         \
}

#define LASX_UNPCK_L_WU_BU_2(in0, in1, out0, out1)                            \
{                                                                             \
    LASX_UNPCK_L_WU_BU(in0, out0);                                            \
    LASX_UNPCK_L_WU_BU(in1, out1);                                            \
}

#define LASX_UNPCK_L_WU_BU_4(in0, in1, in2, in3, out0, out1, out2, out3)      \
{                                                                             \
    LASX_UNPCK_L_WU_BU_2(in0, in1, out0, out1);                               \
    LASX_UNPCK_L_WU_BU_2(in2, in3, out2, out3);                               \
}

#define LASX_UNPCK_L_WU_BU_8(in0, in1, in2, in3, in4, in5, in6, in7,          \
                             out0, out1, out2, out3, out4, out5, out6, out7)  \
{                                                                             \
    LASX_UNPCK_L_WU_BU_4(in0, in1, in2, in3, out0, out1, out2, out3);         \
    LASX_UNPCK_L_WU_BU_4(in4, in5, in6, in7, out4, out5, out6, out7);         \
}

/* Description : Low 8-bit vector elements signed extension to halfword
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low 8-bit elements from in0 signed extension to halfword,
 *               written to output vector out0. Similar for in1.
 * Example     : See LASX_UNPCK_L_W_H(in0, out0)
 */
#define LASX_UNPCK_L_H_B(in0, out0)                                          \
{                                                                            \
    out0 = __lasx_vext2xv_h_b(in0);                                          \
}

#define LASX_UNPCK_L_H_B_2(in0, in1, out0, out1)                             \
{                                                                            \
    LASX_UNPCK_L_H_B(in0, out0);                                             \
    LASX_UNPCK_L_H_B(in1, out1);                                             \
}

#define LASX_UNPCK_L_H_B_4(in0, in1, in2, in3, out0, out1, out2, out3)       \
{                                                                            \
    LASX_UNPCK_L_H_B_2(in0, in1, out0, out1);                                \
    LASX_UNPCK_L_H_B_2(in2, in3, out2, out3);                                \
}

/* Description : Low halfword vector elements signed extension to word
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low halfword elements from in0 signed extension to
 *               word, written to output vector out0. Similar for in1.
 *               Similar for other pairs.
 * Example     : LASX_UNPCK_L_W_H(in0, out0)
 *         in0 : 3, 0, 3, 0,  0, 0, 0, -1,  0, 0, 1, 1,  0, 0, 0, 1
 *        out0 : 3, 0, 3, 0,  0, 0, 0, -1
 */
#define LASX_UNPCK_L_W_H(in0, out0)                                         \
{                                                                           \
    out0 = __lasx_vext2xv_w_h(in0);                                         \
}

#define LASX_UNPCK_L_W_H_2(in0, in1, out0, out1)                            \
{                                                                           \
    LASX_UNPCK_L_W_H(in0, out0);                                            \
    LASX_UNPCK_L_W_H(in1, out1);                                            \
}

#define LASX_UNPCK_L_W_H_4(in0, in1, in2, in3, out0, out1, out2, out3)      \
{                                                                           \
    LASX_UNPCK_L_W_H_2(in0, in1, out0, out1);                               \
    LASX_UNPCK_L_W_H_2(in2, in3, out2, out3);                               \
}

#define LASX_UNPCK_L_W_H_8(in0, in1, in2, in3, in4, in5, in6, in7,          \
                           out0, out1, out2, out3, out4, out5, out6, out7)  \
{                                                                           \
    LASX_UNPCK_L_W_H_4(in0, in1, in2, in3, out0, out1, out2, out3);         \
    LASX_UNPCK_L_W_H_4(in4, in5, in6, in7, out4, out5, out6, out7);         \
}

/* Description : Interleave odd byte elements from vectors.
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out, out0, ~
 * Details     : Odd byte elements of in_h and odd byte
 *               elements of in_l are interleaved and copied to out.
 * Example     : See LASX_ILVOD_W(in_h, in_l, out)
 */
#define LASX_ILVOD_B(in_h, in_l, out)                                            \
{                                                                                \
    out = __lasx_xvpackod_b((in_h, in1_l);                                       \
}

#define LASX_ILVOD_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                   \
{                                                                                \
    LASX_ILVOD_B(in0_h, in0_l, out0);                                            \
    LASX_ILVOD_B(in1_h, in1_l, out1);                                            \
}

#define LASX_ILVOD_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,   \
                       out0, out1, out2, out3)                                   \
{                                                                                \
    LASX_ILVOD_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                      \
    LASX_ILVOD_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                      \
}

/* Description : Interleave odd half word elements from vectors.
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out, out0, ~
 * Details     : Odd half word elements of in_h and odd half word
 *               elements of in_l are interleaved and copied to out.
 * Example     : See LASX_ILVOD_W(in_h, in_l, out)
 */
#define LASX_ILVOD_H(in_h, in_l, out)                                           \
{                                                                               \
    out = __lasx_xvpackod_h(in_h, in_l);                                        \
}

#define LASX_ILVOD_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                  \
{                                                                               \
    LASX_ILVOD_H(in0_h, in0_l, out0);                                           \
    LASX_ILVOD_H(in1_h, in1_l, out1);                                           \
}

#define LASX_ILVOD_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                       out0, out1, out2, out3)                                  \
{                                                                               \
    LASX_ILVOD_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                     \
    LASX_ILVOD_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                     \
}

/* Description : Interleave odd word elements from vectors.
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out, out0, ~
 * Details     : Odd word elements of in_h and odd word
 *               elements of in_l are interleaved and copied to out.
 * Example     : See LASX_ILVOD_W(in_h, in_l, out)
 *        in_h : 1, 2, 3, 4,   5, 6, 7, 8
 *        in_l : 1, 0, 3, 1,   1, 2, 3, 4
 *         out : 0, 2, 1, 4,   2, 6, 4, 8
 */
#define LASX_ILVOD_W(in_h, in_l, out)                                           \
{                                                                               \
    out = __lasx_xvpackod_w(in_h, in_l);                                        \
}

#define LASX_ILVOD_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                  \
{                                                                               \
    LASX_ILVOD_W(in0_h, in0_l, out0);                                           \
    LASX_ILVOD_W(in1_h, in1_l, out1);                                           \
}

#define LASX_ILVOD_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                       out0, out1, out2, out3)                                  \
{                                                                               \
    LASX_ILVOD_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                     \
    LASX_ILVOD_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                     \
}

/* Description : Interleave odd double word elements from vectors.
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out, out0, ~
 * Details     : Odd double word elements of in_h and odd double word
 *               elements of in_l are interleaved and copied to out.
 * Example     : LASX_ILVOD_W(in_h, in_l, out)
 */
#define LASX_ILVOD_D(in_h, in_l, out)                                           \
{                                                                               \
    out = __lasx_xvpackod_d(in_h, in_l);                                        \
}

#define LASX_ILVOD_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                  \
{                                                                               \
    LASX_ILVOD_D(in0_h, in0_l, out0);                                           \
    LASX_ILVOD_D(in1_h, in1_l, out1);                                           \
}

#define LASX_ILVOD_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                       out0, out1, out2, out3)                                  \
{                                                                               \
    LASX_ILVOD_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                     \
    LASX_ILVOD_D_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                     \
}

/* Description : Interleave right half of byte elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of byte elements of in_l and high half of byte
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : See LASX_ILVL_W(in_h, in_l, out0)
 */
#define LASX_ILVL_B(in_h, in_l, out0)                                      \
{                                                                          \
    __m256i tmp0, tmp1;                                                    \
    tmp0 = __lasx_xvilvl_b(in_h, in_l);                                    \
    tmp1 = __lasx_xvilvh_b(in_h, in_l);                                    \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                             \
}

#define LASX_ILVL_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVL_B(in0_h, in0_l, out0)                                  \
    LASX_ILVL_B(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVL_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVL_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVL_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVL_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVL_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVL_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave low half of byte elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of byte elements of in_l and low half of byte
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : See LASX_ILVL_W_128SV(in_h, in_l, out0)
 */
#define LASX_ILVL_B_128SV(in_h, in_l, out0)                                   \
{                                                                             \
    out0 = __lasx_xvilvl_b(in_h, in_l);                                       \
}

#define LASX_ILVL_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVL_B_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVL_B_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVL_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVL_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVL_B_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVL_B_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVL_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVL_B_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave low half of half word elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of half word elements of in_l and right half of
 *               half word elements of in_h are interleaved and copied to
 *               out0. Similar for other pairs.
 * Example     : See LASX_ILVL_W(in_h, in_l, out0)
 */
#define LASX_ILVL_H(in_h, in_l, out0)                                      \
{                                                                          \
    __m256i tmp0, tmp1;                                                    \
    tmp0 = __lasx_xvilvl_h(in_h, in_l);                                    \
    tmp1 = __lasx_xvilvh_h(in_h, in_l);                                    \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                             \
}

#define LASX_ILVL_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVL_H(in0_h, in0_l, out0)                                  \
    LASX_ILVL_H(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVL_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVL_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVL_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVL_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVL_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVL_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave low half of half word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l,  ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of half word elements of in_l and low half of half
 *               word elements of in_h are interleaved and copied to
 *               out0. Similar for other pairs.
 * Example     : See LASX_ILVL_W_128SV(in_h, in_l, out0)
 */
#define LASX_ILVL_H_128SV(in_h, in_l, out0)                                   \
{                                                                             \
    out0 = __lasx_xvilvl_h(in_h, in_l);                                       \
}

#define LASX_ILVL_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVL_H_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVL_H_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVL_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVL_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVL_H_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVL_H_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVL_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVL_H_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave low half of word elements from vectors
 * Arguments   : Inputs  - in0_h, in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of halfword elements of in_l and low half of word
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : LASX_ILVL_W(in_h, in_l, out0)
 *        in_h : 0, 1, 0, 1,  0, 1, 0, 1
 *        in_l : 1, 2, 3, 4,  5, 6, 7, 8
 *        out0 : 1, 0, 2, 1,  3, 0, 4, 1
 */
#define LASX_ILVL_W(in_h, in_l, out0)                                      \
{                                                                          \
    __m256i tmp0, tmp1;                                                    \
    tmp0 = __lasx_xvilvl_w(in_h, in_l);                                    \
    tmp1 = __lasx_xvilvh_w(in_h, in_l);                                    \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                             \
}

#define LASX_ILVL_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVL_W(in0_h, in0_l, out0)                                  \
    LASX_ILVL_W(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVL_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVL_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVL_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVL_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVL_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVL_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave low half of word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h, in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of halfword elements of in_l and low half of word
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : LASX_ILVL_W_128SV(in_h, in_l, out0)
 *        in_h : 0, 1, 0, 1, 0, 1, 0, 1
 *        in_l : 1, 2, 3, 4, 5, 6, 7, 8
 *        out0 : 1, 0, 2, 1, 5, 0, 6, 1
 */
#define LASX_ILVL_W_128SV(in_h, in_l, out0)                             \
{                                                                       \
    out0 = __lasx_xvilvl_w(in_h, in_l);                                 \
}

#define LASX_ILVL_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVL_W_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVL_W_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVL_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVL_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVL_W_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVL_W_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVL_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVL_W_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave low half of double word elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Low half of double word elements of in_l and low half of
 *               double word elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs
 * Example     : See LASX_ILVL_W(in_h, in_l, out0)
 */
#define LASX_ILVL_D(in_h, in_l, out0)                                   \
{                                                                       \
    __m256i tmp0, tmp1;                                                 \
    tmp0 = __lasx_xvilvl_d(in_h, in_l);                                 \
    tmp1 = __lasx_xvilvh_d(in_h, in_l);                                 \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                          \
}

#define LASX_ILVL_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVL_D(in0_h, in0_l, out0)                                  \
    LASX_ILVL_D(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVL_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVL_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVL_D_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVL_D_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVL_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVL_D_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave right half of double word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Right half of double word elements of in_l and right half of
 *               double word elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs.
 * Example     : See LASX_ILVL_W_128SV(in_h, in_l, out0)
 */
#define LASX_ILVL_D_128SV(in_h, in_l, out0)                              \
{                                                                        \
    out0 = __lasx_xvilvl_d(in_h, in_l);                                  \
}

#define LASX_ILVL_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVL_D_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVL_D_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVL_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVL_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVL_D_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVL_D_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVL_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVL_D_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of byte elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of byte elements of in_l and high half of
 *               byte
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs.
 * Example     : see LASX_ILVH_W(in_h, in_l, out0)
 */
#define LASX_ILVH_B(in_h, in_l, out0)                            \
{                                                                \
    __m256i tmp0, tmp1;                                          \
    tmp0 = __lasx_xvilvl_b(in_h, in_l);                          \
    tmp1 = __lasx_xvilvh_b(in_h, in_l);                          \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                   \
}

#define LASX_ILVH_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVH_B(in0_h, in0_l, out0)                                  \
    LASX_ILVH_B(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVH_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVH_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVH_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVH_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of byte elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of  byte elements  of  in_l and high half
 *               of byte elements of in_h are interleaved and copied
 *               to out0.
 *               Similar for other pairs.
 * Example     : see LASX_ILVH_W_128SV(in_h, in_l, out0)
 */
#define LASX_ILVH_B_128SV(in_h, in_l, out0)                     \
{                                                               \
    out0 = __lasx_xvilvh_b(in_h, in_l);                         \
}

#define LASX_ILVH_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVH_B_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVH_B_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVH_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVH_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVH_B_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVH_B_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVH_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVH_B_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of half word elements from vectors
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of half word elements of in_l and high half of
 *               half word
 *               elements of in_h are interleaved and copied to out0.
 *               Similar for other pairs.
 * Example     : see LASX_ILVH_W(in_h, in_l, out0)
 */
#define LASX_ILVH_H(in_h, in_l, out0)                           \
{                                                                \
    __m256i tmp0, tmp1;                                          \
    tmp0 = __lasx_xvilvl_h(in_h, in_l);                          \
    tmp1 = __lasx_xvilvh_h(in_h, in_l);                          \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                   \
}

#define LASX_ILVH_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVH_H(in0_h, in0_l, out0)                                  \
    LASX_ILVH_H(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVH_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVH_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVH_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVH_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVH_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVH_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of half word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of  half word elements  of  in_l and high half
 *               of half word elements of in_h are interleaved and copied
 *               to out0.
 *               Similar for other pairs.
 * Example     : see LASX_ILVH_W_128SV(in_h, in_l, out0)
 */
#define LASX_ILVH_H_128SV(in_h, in_l, out0)                     \
{                                                               \
    out0 = __lasx_xvilvh_h(in_h, in_l);                         \
}

#define LASX_ILVH_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVH_H_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVH_H_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVH_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVH_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVH_H_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVH_H_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVH_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVH_H_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of word elements from vectors
 * Arguments   : Inputs  - in0_h, in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of word elements of in_l and high half of
 *               word elements of in_h are interleaved and copied to
 *               out0.
 *               Similar for other pairs.
 * Example     : LASX_ILVH_W(in_h, in_l, out0)
 *         in_h:-1, -2, -3, -4, -5, -6, -7, -8
 *         in_l: 1,  2,  3,  4,  5,  6,  7,  8
 *         out0: 5, -5,  6, -6,  7, -7,  8, -8
 */
#define LASX_ILVH_W(in_h, in_l, out0)                            \
{                                                                \
    __m256i tmp0, tmp1;                                          \
    tmp0 = __lasx_xvilvl_w(in_h, in_l);                          \
    tmp1 = __lasx_xvilvh_w(in_h, in_l);                          \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                   \
}

#define LASX_ILVH_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVH_W(in0_h, in0_l, out0)                                  \
    LASX_ILVH_W(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVH_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVH_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVH_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVH_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h, in0_l, ~
 *               Outputs - out0, out1, ~
 * Details     : High half of word elements of every 128-bit of in_l
 *               and high half of word elements of every 128-bit of
 *               in_h are interleaved and copied to out0.
 *               Similar for other pairs.
 * Example     : LASX_ILVH_W_128SV(in_h, in_l, out0)
 *         in_h:-1, -2, -3, -4, -5, -6, -7, -8
 *         in_l: 1,  2,  3,  4,  5,  6,  7,  8
 *         out0: 3, -3,  4, -4,  7, -7,  8, -8*
 */
#define LASX_ILVH_W_128SV(in_h, in_l, out0)                        \
{                                                                  \
    out0 = __lasx_xvilvh_w(in_h, in_l);                            \
}

#define LASX_ILVH_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVH_W_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVH_W_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVH_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVH_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVH_W_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVH_W_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVH_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVH_W_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of double word elements from vectors
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out0, out1, ~
 :* Details    : High half of double word elements of in_l and high half of
 *               double word elements of in_h are interleaved and copied to
 *               out0.
 *               Similar for other pairs.
 * Example    : see LASX_ILVH_W(in_h, in_l, out0)
 */
#define LASX_ILVH_D(in_h, in_l, out0)                           \
{                                                               \
    __m256i tmp0, tmp1;                                         \
    tmp0 = __lasx_xvilvl_d(in_h, in_l);                         \
    tmp1 = __lasx_xvilvh_d(in_h, in_l);                         \
    out0 = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                  \
}

#define LASX_ILVH_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)        \
{                                                                    \
    LASX_ILVH_D(in0_h, in0_l, out0)                                  \
    LASX_ILVH_D(in1_h, in1_l, out1)                                  \
}

#define LASX_ILVH_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                      in3_h, in3_l, out0, out1, out2, out3)          \
{                                                                    \
    LASX_ILVH_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)            \
    LASX_ILVH_D_2(in2_h, in2_l, in3_h, in3_l, out2, out3)            \
}

#define LASX_ILVH_D_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                      in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                      out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                              \
    LASX_ILVH_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                  out0, out1, out2, out3);                                     \
    LASX_ILVH_D_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                  out4, out5, out6, out7);                                     \
}

/* Description : Interleave high half of double word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0,  in1,  ~
 *               Outputs - out0, out1, ~
 * Details     : High half of double word elements of every 128-bit in_l and
 *               high half of double word elements of every 128-bit in_h are
 *               interleaved and copied to out0.
 *               Similar for other pairs.
 * Example     : see LASX_ILVH_W_128SV(in_h, in_l, out0)
 */
#define LASX_ILVH_D_128SV(in_h, in_l, out0)                             \
{                                                                       \
    out0 = __lasx_xvilvh_d(in_h, in_l);                                 \
}

#define LASX_ILVH_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)           \
{                                                                             \
    LASX_ILVH_D_128SV(in0_h, in0_l, out0);                                    \
    LASX_ILVH_D_128SV(in1_h, in1_l, out1);                                    \
}

#define LASX_ILVH_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,         \
                            in3_h, in3_l, out0, out1, out2, out3)             \
{                                                                             \
    LASX_ILVH_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);              \
    LASX_ILVH_D_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);              \
}

#define LASX_ILVH_D_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,  \
                            in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,  \
                            out0, out1, out2, out3, out4, out5, out6, out7)          \
{                                                                                    \
    LASX_ILVH_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,      \
                        out0, out1, out2, out3);                                     \
    LASX_ILVH_D_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,      \
                        out4, out5, out6, out7);                                     \
}

/* Description : Interleave byte elements from vectors
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of  byte elements  of in_l and low half of byte
 *               elements  of in_h  are interleaved  and copied  to  out_l.
 *               High half of byte elements of in_l and high half of byte
 *               elements of in_h are interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : see LASX_ILVLH_W(in_h, in_l, out_l, out_h)
 */
#define LASX_ILVLH_B(in_h, in_l, out_h, out_l)                          \
{                                                                       \
    __m256i tmp0, tmp1;                                                 \
    tmp0  = __lasx_xvilvl_b(in_h, in_l);                                \
    tmp1  = __lasx_xvilvh_b(in_h, in_l);                                \
    out_l = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                         \
    out_h = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                         \
}

#define LASX_ILVLH_B_2(in0_h, in0_l, in1_h, in1_l,                      \
                       out0_h, out0_l, out1_h, out1_l)                  \
{                                                                       \
    LASX_ILVLH_B(in0_h, in0_l, out0_h, out0_l);                         \
    LASX_ILVLH_B(in1_h, in1_l, out1_h, out1_l);                         \
}

#define LASX_ILVLH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                       \
    LASX_ILVLH_B_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LASX_ILVLH_B_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LASX_ILVLH_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                       out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                       \
    LASX_ILVLH_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                   out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                   out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave byte elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of byte elements of in_l and low half of byte elements
 *               of in_h are interleaved and copied to out_l. High  half  of byte
 *               elements  of in_h  and high half  of byte elements  of in_l  are
 *               interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : see LASX_ILVLH_W_128SV(in_h, in_l, out_h, out_l)
 */
#define LASX_ILVLH_B_128SV(in_h, in_l, out_h, out_l)                           \
{                                                                              \
    LASX_ILVL_B_128SV(in_h, in_l, out_l);                                      \
    LASX_ILVH_B_128SV(in_h, in_l, out_h);                                      \
}

#define LASX_ILVLH_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l)  \
{                                                                                         \
    LASX_ILVLH_B_128SV(in0_h, in0_l, out0_h, out0_l);                                     \
    LASX_ILVLH_B_128SV(in1_h, in1_l, out1_h, out1_l);                                     \
}

#define LASX_ILVLH_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,           \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)   \
{                                                                                              \
    LASX_ILVLH_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);          \
    LASX_ILVLH_B_2_128SV(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);          \
}

#define LASX_ILVLH_B_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                             in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                             out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                             \
    LASX_ILVLH_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                         out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_B_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
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
 * Example     : see LASX_ILVLH_W(in_h, in_l, out_h, out_l)
 */
#define LASX_ILVLH_H(in_h, in_l, out_h, out_l)                           \
{                                                                        \
    __m256i tmp0, tmp1;                                                  \
    tmp0  = __lasx_xvilvl_h(in_h, in_l);                                 \
    tmp1  = __lasx_xvilvh_h(in_h, in_l);                                 \
    out_l = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                          \
    out_h = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                          \
}

#define LASX_ILVLH_H_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l,       \
                       out1_h, out1_l)                                   \
{                                                                        \
    LASX_ILVLH_H(in0_h, in0_l, out0_h, out0_l);                          \
    LASX_ILVLH_H(in1_h, in1_l, out1_h, out1_l);                          \
}

#define LASX_ILVLH_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                       \
    LASX_ILVLH_H_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LASX_ILVLH_H_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LASX_ILVLH_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                       out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                       \
    LASX_ILVLH_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                   out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                   out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave half word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l,  ~
 *               Outputs - out0_h, out0_l, ~
 * Details     : Low half of half word elements  of every 128-bit of in_l and
 *               low half of half word elements  of every 128-bit of in_h are
 *               interleaved and copied to out_l.
 *               High half of half word elements of every 128-bit of in_l and
 *               high half of half word elements of every 128-bit of in_h are
 *               interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : see LASX_ILVLH_W_128SV(in_h, in_l, out_h, out_l)
 */
#define LASX_ILVLH_H_128SV(in_h, in_l, out_h, out_l)                            \
{                                                                               \
    LASX_ILVL_H_128SV(in_h, in_l, out_l);                                       \
    LASX_ILVH_H_128SV(in_h, in_l, out_h);                                       \
}

#define LASX_ILVLH_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l)  \
{                                                                                         \
    LASX_ILVLH_H_128SV(in0_h, in0_l, out0_h, out0_l);                                     \
    LASX_ILVLH_H_128SV(in1_h, in1_l, out1_h, out1_l);                                     \
}

#define LASX_ILVLH_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,           \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)   \
{                                                                                              \
    LASX_ILVLH_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);          \
    LASX_ILVLH_H_2_128SV(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);          \
}

#define LASX_ILVLH_H_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                             in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                             out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                             \
    LASX_ILVLH_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                         out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_H_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                         out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave word elements from vectors
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of  word elements  of in_l and low half of word
 *               elements of in_h  are  interleaved  and  copied  to out_l.
 *               High half of word elements of in_l and high half of word
 *               elements of in_h are interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : LASX_ILVLH_W(in_h, in_l, out_h, out_l)
 *         in_h:-1, -2, -3, -4, -5, -6, -7, -8
 *         in_l: 1,  2,  3,  4,  5,  6,  7,  8
 *        out_h: 5, -5,  6, -6,  7, -7,  8, -8
 *        out_l: 1, -1,  2, -2,  3, -3,  4, -4
 */
#define LASX_ILVLH_W(in_h, in_l, out_h, out_l)                           \
{                                                                        \
    __m256i tmp0, tmp1;                                                  \
    tmp0  = __lasx_xvilvl_w(in_h, in_l);                                 \
    tmp1  = __lasx_xvilvh_w(in_h, in_l);                                 \
    out_l = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                          \
    out_h = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                          \
}

#define LASX_ILVLH_W_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l,       \
                       out1_h, out1_l)                                   \
{                                                                        \
    LASX_ILVLH_W(in0_h, in0_l, out0_h, out0_l);                          \
    LASX_ILVLH_W(in1_h, in1_l, out1_h, out1_l);                          \
}

#define LASX_ILVLH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                       \
    LASX_ILVLH_W_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LASX_ILVLH_W_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LASX_ILVLH_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                       out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                       \
    LASX_ILVLH_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                   out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                   out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in0_h,  in0_l,  ~
 *               Outputs - out0_h, out0_l, ~
 * Details     : Low half of word elements  of every 128-bit of in_l and
 *               low half of word elements  of every 128-bit of in_h are
 *               interleaved and copied to out_l.
 *               High half of word elements of every 128-bit of in_l and
 *               high half of word elements of every 128-bit of in_h are
 *               interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : LASX_ILVLH_W_128SV(in_h, in_l, out_h, out_l)
 *         in_h:-1, -2, -3, -4, -5, -6, -7, -8
 *         in_l: 1,  2,  3,  4,  5,  6,  7,  8
 *        out_h: 3, -3,  4, -4,  7, -7,  8, -8
 *        out_l: 1, -1,  2, -2,  5, -5,  6, -6
 */
#define LASX_ILVLH_W_128SV(in_h, in_l, out_h, out_l)                            \
{                                                                               \
    LASX_ILVL_W_128SV(in_h, in_l, out_l);                                       \
    LASX_ILVH_W_128SV(in_h, in_l, out_h);                                       \
}

#define LASX_ILVLH_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l)  \
{                                                                                         \
    LASX_ILVLH_W_128SV(in0_h, in0_l, out0_h, out0_l);                                     \
    LASX_ILVLH_W_128SV(in1_h, in1_l, out1_h, out1_l);                                     \
}

#define LASX_ILVLH_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,           \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)   \
{                                                                                              \
    LASX_ILVLH_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);          \
    LASX_ILVLH_W_2_128SV(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);          \
}

#define LASX_ILVLH_W_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                             in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                             out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                             \
    LASX_ILVLH_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                         out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_W_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                         out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave double word elements from vectors
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of double word  elements  of in_l and low half of
 *               double word elements of in_h are interleaved and copied to
 *               out_l. High half of double word  elements  of in_l and high
 *               half of double word  elements  of in_h are interleaved and
 *               copied to out_h.
 *               Similar for other pairs.
 * Example     : see LASX_ILVLH_W(in_h, in_l, out_h, out_l)
 */
#define LASX_ILVLH_D(in_h, in_l, out_h, out_l)                           \
{                                                                        \
    __m256i tmp0, tmp1;                                                  \
    tmp0  = __lasx_xvilvl_d(in_h, in_l);                                 \
    tmp1  = __lasx_xvilvh_d(in_h, in_l);                                 \
    out_l = __lasx_xvpermi_q(tmp0, tmp1, 0x02);                          \
    out_h = __lasx_xvpermi_q(tmp0, tmp1, 0x13);                          \
}

#define LASX_ILVLH_D_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l,       \
                       out1_h, out1_l)                                   \
{                                                                        \
    LASX_ILVLH_D(in0_h, in0_l, out0_h, out0_l);                          \
    LASX_ILVLH_D(in1_h, in1_l, out1_h, out1_l);                          \
}

#define LASX_ILVLH_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)  \
{                                                                                       \
    LASX_ILVLH_D_2(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);         \
    LASX_ILVLH_D_2(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);         \
}

#define LASX_ILVLH_D_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                       out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                       out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                       \
    LASX_ILVLH_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                   out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_D_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                   out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Interleave double word elements from vectors
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h,  in_l,  ~
 *               Outputs - out_h, out_l, ~
 * Details     : Low half of double word elements of every 128-bit  of in_l and
 *               low half of double word elements of every 128-bit  of in_h are
 *               interleaved and copied to out_l.
 *               High half of double word elements of every 128-bit of in_l and
 *               high half of double word elements of every 128-bit of in_h are
 *               interleaved and copied to out_h.
 *               Similar for other pairs.
 * Example     : see LASX_ILVLH_W_128SV(in_h, in_l, out_h, out_l)
 */
#define LASX_ILVLH_D_128SV(in_h, in_l, out_h, out_l)                            \
{                                                                               \
    LASX_ILVL_D_128SV(in_h, in_l, out_l);                                       \
    LASX_ILVH_D_128SV(in_h, in_l, out_h);                                       \
}

#define LASX_ILVLH_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l)  \
{                                                                                         \
    LASX_ILVLH_D_128SV(in0_h, in0_l, out0_h, out0_l);                                     \
    LASX_ILVLH_D_128SV(in1_h, in1_l, out1_h, out1_l);                                     \
}

#define LASX_ILVLH_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,           \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l)   \
{                                                                                              \
    LASX_ILVLH_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0_h, out0_l, out1_h, out1_l);          \
    LASX_ILVLH_D_2_128SV(in2_h, in2_l, in3_h, in3_l, out2_h, out2_l, out3_h, out3_l);          \
}

#define LASX_ILVLH_D_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,          \
                             in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,          \
                             out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l,  \
                             out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l)  \
{                                                                                             \
    LASX_ILVLH_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l,              \
                         out0_h, out0_l, out1_h, out1_l, out2_h, out2_l, out3_h, out3_l);     \
    LASX_ILVLH_D_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l,              \
                         out4_h, out4_l, out5_h, out5_l, out6_h, out6_l, out7_h, out7_l);     \
}

/* Description : Immediate number of columns to slide with zero
 * Arguments   : Inputs  - in0, in1, slide_val, ~
 *               Outputs - out0, out1, ~
 * Details     : Byte elements from every 128-bit of in0 vector
 *               are slide into  out0  by  number  of  elements
 *               specified by slide_val.
 * Example     : LASX_SLDI_B_0_128SV(in0, out0, slide_val)
 *          in0: 1, 2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,
 *               19,20,21,22,23,24,25,26,27,28,29,30,31,32
 *         out0: 4, 5,6,7,8,9,10,11,12,13,14,15,16,0,0,0,20,21,
 *               22,23,24,25,26,27,28,29,30,31,32,0,0,0
 *    slide_val: 3
 */
#define LASX_SLDI_B_0_128SV(in0, out0, slide_val)                   \
{                                                                   \
    out0 = __lasx_xvbsrl_v(in0, slide_val);                         \
}

#define LASX_SLDI_B_2_0_128SV(in0, in1, out0, out1, slide_val)      \
{                                                                   \
    LASX_SLDI_B_0_128SV(in0, out0, slide_val);                      \
    LASX_SLDI_B_0_128SV(in1, out1, slide_val);                      \
}

#define LASX_SLDI_B_4_0_128SV(in0, in1, in2, in3,                   \
                              out0, out1, out2, out3, slide_val)    \
{                                                                   \
    LASX_SLDI_B_2_0_128SV(in0, in1, out0, out1, slide_val);         \
    LASX_SLDI_B_2_0_128SV(in2, in3, out2, out3, slide_val);         \
}

/* Description : Pack even byte elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even byte elements of in_l are copied to the low half of
 *               out0.  Even byte elements of in_h are copied to the high
 *               half of out0.
 *               Similar for other pairs.
 * Example     : see LASX_PCKEV_W(in_h, in_l, out0)
 */
#define LASX_PCKEV_B(in_h, in_l, out0)                                  \
{                                                                       \
    out0 = __lasx_xvpickev_b(in_h, in_l);                               \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                                \
}

#define LASX_PCKEV_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)          \
{                                                                       \
    LASX_PCKEV_B(in0_h, in0_l, out0);                                   \
    LASX_PCKEV_B(in1_h, in1_l, out1);                                   \
}

#define LASX_PCKEV_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,        \
                       in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                       \
    LASX_PCKEV_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1);             \
    LASX_PCKEV_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3);             \
}

#define LASX_PCKEV_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKEV_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKEV_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack even byte elements of vector pairs
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even byte elements of in_l are copied to the low half of
 *               out0.  Even byte elements of in_h are copied to the high
 *               half of out0.
 *               Similar for other pairs.
 * Example     : see LASX_PCKEV_W_128SV(in_h, in_l, out0)
 */
#define LASX_PCKEV_B_128SV(in_h, in_l, out0)                            \
{                                                                       \
    out0 = __lasx_xvpickev_b(in_h, in_l);                               \
}

#define LASX_PCKEV_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                       \
    LASX_PCKEV_B_128SV(in0_h, in0_l, out0);                             \
    LASX_PCKEV_B_128SV(in1_h, in1_l, out1);                             \
}

#define LASX_PCKEV_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                             in3_h, in3_l, out0, out1, out2, out3)      \
{                                                                       \
    LASX_PCKEV_B_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LASX_PCKEV_B_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LASX_PCKEV_B_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                             in3_h, in3_l, in4_h, in4_l, in5_h, in5_l,  \
                             in6_h, in6_l, in7_h, in7_l, out0, out1,    \
                             out2, out3, out4, out5, out6, out7)        \
{                                                                       \
    LASX_PCKEV_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                         in3_h, in3_l, out0, out1, out2, out3);         \
    LASX_PCKEV_B_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,      \
                         in7_h, in7_l, out4, out5, out6, out7);         \
}

/* Description : Pack even half word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even half word elements of in_l are copied to the  low
 *               half of out0.  Even  half  word  elements  of in_h are
 *               copied to the high half of out0.
 * Example     : see LASX_PCKEV_W(in_h, in_l, out0)
 */
#define LASX_PCKEV_H(in_h, in_l, out0)                                 \
{                                                                      \
    out0 = __lasx_xvpickev_h(in_h, in_l);                              \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                               \
}

#define LASX_PCKEV_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)          \
{                                                                       \
    LASX_PCKEV_H(in0_h, in0_l, out0);                                   \
    LASX_PCKEV_H(in1_h, in1_l, out1);                                   \
}

#define LASX_PCKEV_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,        \
                       in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                       \
    LASX_PCKEV_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1);             \
    LASX_PCKEV_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3);             \
}

#define LASX_PCKEV_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKEV_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKEV_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack even half word elements of vector pairs
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even half word elements of in_l are copied to the  low
 *               half of out0.  Even  half  word  elements  of in_h are
 *               copied to the high half of out0.
 * Example     : see LASX_PCKEV_W_128SV(in_h, in_l, out0)
 */
#define LASX_PCKEV_H_128SV(in_h, in_l, out0)                            \
{                                                                       \
    out0 = __lasx_xvpickev_h(in_h, in_l);                               \
}

#define LASX_PCKEV_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)    \
{                                                                       \
    LASX_PCKEV_H_128SV(in0_h, in0_l, out0);                             \
    LASX_PCKEV_H_128SV(in1_h, in1_l, out1);                             \
}

#define LASX_PCKEV_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                       in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                       \
    LASX_PCKEV_H_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);       \
    LASX_PCKEV_H_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);       \
}

#define LASX_PCKEV_H_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,  \
                             in3_h, in3_l, in4_h, in4_l, in5_h, in5_l,  \
                             in6_h, in6_l, in7_h, in7_l, out0, out1,    \
                             out2, out3, out4, out5, out6, out7)        \
{                                                                       \
    LASX_PCKEV_H_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,      \
                   in3_h, in3_l, out0, out1, out2, out3);               \
    LASX_PCKEV_H_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,      \
                   in7_h, in7_l, out4, out5, out6, out7);               \
}

/* Description : Pack even word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even word  elements  of  in_l are copied to
 *               the low  half of out0.  Even word elements
 *               of in_h are copied to the high half of out0.
 * Example     : LASX_PCKEV_W(in_h, in_l, out0)
 *         in_h: -1, -2, -3, -4, -5, -6, -7, -8
 *         in_l:  1,  2,  3,  4,  5,  6,  7,  8
 *         out0:  1,  3,  5,  7, -1, -3, -5, -7
 */
#define LASX_PCKEV_W(in_h, in_l, out0)                    \
{                                                         \
    out0 = __lasx_xvpickev_w(in_h, in_l);                 \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                  \
}

#define LASX_PCKEV_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)          \
{                                                                       \
    LASX_PCKEV_W(in0_h, in0_l, out0);                                   \
    LASX_PCKEV_W(in1_h, in1_l, out1);                                   \
}

#define LASX_PCKEV_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,        \
                       in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                       \
    LASX_PCKEV_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1);             \
    LASX_PCKEV_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3);             \
}

#define LASX_PCKEV_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKEV_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKEV_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack even word elements of vector pairs
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even word  elements  of  in_l are copied to
 *               the low  half of out0.  Even word elements
 *               of in_h are copied to the high half of out0.
 * Example     : LASX_PCKEV_W_128SV(in_h, in_l, out0)
 *         in_h: -1, -2, -3, -4, -5, -6, -7, -8
 *         in_l:  1,  2,  3,  4,  5,  6,  7,  8
 *         out0:  1,  3, -1, -3,  5,  7, -5, -7
 */
#define LASX_PCKEV_W_128SV(in_h, in_l, out0)                           \
{                                                                      \
    out0 = __lasx_xvpickev_w(in_h, in_l);                              \
}

#define LASX_PCKEV_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)   \
{                                                                      \
    LASX_PCKEV_W_128SV(in0_h, in0_l, out0);                            \
    LASX_PCKEV_W_128SV(in1_h, in1_l, out1);                            \
}

#define LASX_PCKEV_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, \
                             in3_h, in3_l, out0, out1, out2, out3)     \
{                                                                      \
    LASX_PCKEV_W_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1);      \
    LASX_PCKEV_W_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3);      \
}

#define LASX_PCKEV_W_8_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, \
                             in3_h, in3_l, in4_h, in4_l, in5_h, in5_l, \
                             in6_h, in6_l, in7_h, in7_l, out0, out1,   \
                             out2, out3, out4, out5, out6, out7)       \
{                                                                      \
    LASX_PCKEV_W_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,     \
                         in3_h, in3_l, out0, out1, out2, out3);        \
    LASX_PCKEV_W_4_128SV(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,     \
                         in7_h, in7_l, out4, out5, out6, out7);        \
}

/* Description : Pack even half word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even half word elements of in_l are copied to the  low
 *               half of out0.  Even  half  word  elements  of in_h are
 *               copied to the high half of out0.
 * Example     : See LASX_PCKEV_W(in_h, in_l, out0)
 */
#define LASX_PCKEV_D(in_h, in_l, out0)                                        \
{                                                                             \
    out0 = __lasx_xvpickev_d(in_h, in_l);                                     \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                                      \
}

#define LASX_PCKEV_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                \
{                                                                             \
    LASX_PCKEV_D(in0_h, in0_l, out0)                                          \
    LASX_PCKEV_D(in1_h, in1_l, out1)                                          \
}

#define LASX_PCKEV_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,              \
                       in3_h, in3_l, out0, out1, out2, out3)                  \
{                                                                             \
    LASX_PCKEV_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                    \
    LASX_PCKEV_D_2(in2_h, in2_l, in3_h, in3_l, out2, out3)                    \
}

/* Description : Pack even half word elements of vector pairs
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even half word elements of in_l are copied to the  low
 *               half of out0.  Even  half  word  elements  of in_h are
 *               copied to the high half of out0.
 * Example     : LASX_PCKEV_D_128SV(in_h, in_l, out0)
 *        in_h : 1, 2, 3, 4
 *        in_l : 5, 6, 7, 8
 *        out0 : 5, 1, 7, 3
 */
#define LASX_PCKEV_D_128SV(in_h, in_l, out0)                                  \
{                                                                             \
    out0 = __lasx_xvpickev_d(in_h, in_l);                                     \
}

#define LASX_PCKEV_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)          \
{                                                                             \
    LASX_PCKEV_D_128SV(in0_h, in0_l, out0)                                    \
    LASX_PCKEV_D_128SV(in1_h, in1_l, out1)                                    \
}

#define LASX_PCKEV_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,        \
                             in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                             \
    LASX_PCKEV_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)              \
    LASX_PCKEV_D_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3)              \
}

/* Description : Pack even quad word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Even quad elements of in_l are copied to the low
 *               half of out0. Even  quad  elements  of  in_h are
 *               copied to the high half of out0.
 *               Similar for other pairs.
 * Example     : see LASX_PCKEV_W(in_h, in_l, out0)
 */
#define LASX_PCKEV_Q(in_h, in_l, out0)                          \
{                                                               \
    out0 = __lasx_xvpermi_q(in_h, in_l, 0x20);                  \
}

#define LASX_PCKEV_Q_2(in0_h, in0_l, in1_h, in1_l, out0, out1)          \
{                                                                       \
    LASX_PCKEV_Q(in0_h, in0_l, out0);                                   \
    LASX_PCKEV_Q(in1_h, in1_l, out1);                                   \
}

#define LASX_PCKEV_Q_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,        \
                       in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                       \
    LASX_PCKEV_Q_2(in0_h, in0_l, in1_h, in1_l, out0, out1);             \
    LASX_PCKEV_Q_2(in2_h, in2_l, in3_h, in3_l, out2, out3);             \
}

#define LASX_PCKEV_Q_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKEV_Q_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKEV_Q_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack odd byte elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd byte elements of in_l are copied to the low half of
 *               out0. Odd byte elements of in_h are copied to the high
 *               half of out0.
 *               Similar for other pairs.
 * Example     : see LASX_PCKOD_W(in_h, in_l, out0)
 */
#define LASX_PCKOD_B(in_h, in_l, out0)                                         \
{                                                                              \
    out0 = __lasx_xvpickod_b(in_h, in_l);                                      \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                                       \
}

#define LASX_PCKOD_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                 \
{                                                                              \
    LASX_PCKOD_B(in0_h, in0_l, out0);                                          \
    LASX_PCKOD_B(in1_h, in1_l, out1);                                          \
}

#define LASX_PCKOD_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,               \
                       in3_h, in3_l, out0, out1, out2, out3)                   \
{                                                                              \
    LASX_PCKOD_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                    \
    LASX_PCKOD_B_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                    \
}

#define LASX_PCKOD_B_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKOD_B_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKOD_B_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack odd half word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd half word elements of in_l are copied to the low
 *               half of out0. Odd half word elements of in_h are copied
 *               to the high half of out0.
 * Example     : see LASX_PCKOD_W(in_h, in_l, out0)
 */
#define LASX_PCKOD_H(in_h, in_l, out0)                                         \
{                                                                              \
    out0 = __lasx_xvpickod_h(in_h, in_l);                                      \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                                       \
}

#define LASX_PCKOD_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                 \
{                                                                              \
    LASX_PCKOD_H(in0_h, in0_l, out0);                                          \
    LASX_PCKOD_H(in1_h, in1_l, out1);                                          \
}

#define LASX_PCKOD_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,               \
                       in3_h, in3_l, out0, out1, out2, out3)                   \
{                                                                              \
    LASX_PCKOD_H_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                    \
    LASX_PCKOD_H_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                    \
}

#define LASX_PCKOD_H_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKOD_H_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKOD_H_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack odd word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd word elements of in_l are copied to the low half of out0.
 *               Odd word elements of in_h are copied to the high half of out0.
 * Example     : LASX_PCKOD_W(in_h, in_l, out0)
 *         in_h: -1, -2, -3, -4, -5, -6, -7, -8
 *         in_l:  1,  2,  3,  4,  5,  6,  7,  8
 *         out0:  2,  4,  6,  8, -2, -4, -6, -8
 */
#define LASX_PCKOD_W(in_h, in_l, out0)                                         \
{                                                                              \
    out0 = __lasx_xvpickod_w(in_h, in_l);                                      \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                                       \
}

#define LASX_PCKOD_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                 \
{                                                                              \
    LASX_PCKOD_W(in0_h, in0_l, out0);                                          \
    LASX_PCKOD_W(in1_h, in1_l, out1);                                          \
}

#define LASX_PCKOD_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,               \
                       in3_h, in3_l, out0, out1, out2, out3)                   \
{                                                                              \
    LASX_PCKOD_W_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                    \
    LASX_PCKOD_W_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                    \
}

#define LASX_PCKOD_W_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKOD_W_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKOD_W_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack odd half word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd half word elements of in_l are copied to the low
 *               half of out0. Odd half word elements of in_h are
 *               copied to the high half of out0.
 * Example     : See LASX_PCKOD_W(in_h, in_l, out0)
 */
#define LASX_PCKOD_D(in_h, in_l, out0)                                        \
{                                                                             \
    out0 = __lasx_xvpickod_d(in_h, in_l);                                     \
    out0 = __lasx_xvpermi_d(out0, 0xd8);                                      \
}

#define LASX_PCKOD_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                \
{                                                                             \
    LASX_PCKOD_D(in0_h, in0_l, out0)                                          \
    LASX_PCKOD_D(in1_h, in1_l, out1)                                          \
}

#define LASX_PCKOD_D_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,              \
                       in3_h, in3_l, out0, out1, out2, out3)                  \
{                                                                             \
    LASX_PCKOD_D_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                    \
    LASX_PCKOD_D_2(in2_h, in2_l, in3_h, in3_l, out2, out3)                    \
}

/* Description : Pack odd quad word elements of vector pairs
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd quad elements of in0_h are copied to the high half of
 *               out0 & odd quad elements of in0_l are copied to the low
 *               half of out0.
 *               Odd quad elements of in1_h are copied to the high half of
 *               out1 & odd quad elements of in1_l are copied to the low
 *               half of out1.
 *               LASX_PCKOD_Q(in_h, in_l, out0)
 *               in_h:   0,0,0,0, 0,0,0,0, 19,10,11,12, 13,14,15,16
 *               in_l:   0,0,0,0, 0,0,0,0, 1,2,3,4, 5,6,7,8
 *               out0:  1,2,3,4, 5,6,7,8, 19,10,11,12, 13,14,15,16
 */
#define LASX_PCKOD_Q(in_h, in_l, out0)                                         \
{                                                                              \
    out0 = __lasx_xvpermi_q(in_h, in_l, 0x31);                                 \
}

#define LASX_PCKOD_Q_2(in0_h, in0_l, in1_h, in1_l, out0, out1)                 \
{                                                                              \
    LASX_PCKOD_Q(in0_h, in0_l, out0);                                          \
    LASX_PCKOD_Q(in1_h, in1_l, out1);                                          \
}

#define LASX_PCKOD_Q_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,               \
                       in3_h, in3_l, out0, out1, out2, out3)                   \
{                                                                              \
    LASX_PCKOD_Q_2(in0_h, in0_l, in1_h, in1_l, out0, out1);                    \
    LASX_PCKOD_Q_2(in2_h, in2_l, in3_h, in3_l, out2, out3);                    \
}

#define LASX_PCKOD_Q_8(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l, in3_h, in3_l, \
                       in4_h, in4_l, in5_h, in5_l, in6_h, in6_l, in7_h, in7_l, \
                       out0, out1, out2, out3, out4, out5, out6, out7)         \
{                                                                              \
    LASX_PCKOD_Q_4(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,                   \
                   in3_h, in3_l, out0, out1, out2, out3);                      \
    LASX_PCKOD_Q_4(in4_h, in4_l, in5_h, in5_l, in6_h, in6_l,                   \
                   in7_h, in7_l, out4, out5, out6, out7);                      \
}

/* Description : Pack odd half word elements of vector pairsi
 *               (128-bit symmetry version)
 * Arguments   : Inputs  - in_h, in_l, ~
 *               Outputs - out0, out1, ~
 * Details     : Odd half word elements of in_l are copied to the low
 *               half of out0 of . Odd half word elements of in_h are
 *               copied to the high half of out0.
 * Example     : LASX_PCKOD_D_128SV(in_h, in_l, out0)
 *        in_h : 1, 2, 3, 4
 *        in_l : 5, 6, 7, 8
 *        out0 : 6, 2, 8, 4
 */
#define LASX_PCKOD_D_128SV(in_h, in_l, out0)                                  \
{                                                                             \
    out0 = __lasx_xvpickod_d(in_h, in_l);                                     \
}

#define LASX_PCKOD_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)          \
{                                                                             \
    LASX_PCKOD_D_128SV(in0_h, in0_l, out0)                                    \
    LASX_PCKOD_D_128SV(in1_h, in1_l, out1)                                    \
}

#define LASX_PCKOD_D_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,        \
                             in3_h, in3_l, out0, out1, out2, out3)            \
{                                                                             \
    LASX_PCKOD_D_2_128SV(in0_h, in0_l, in1_h, in1_l, out0, out1)              \
    LASX_PCKOD_D_2_128SV(in2_h, in2_l, in3_h, in3_l, out2, out3)              \
}


/* Description : Transposes 8x8 block with half word elements in vectors.
 * Arguments   : Inputs  - in0, in1, ~
 *               Outputs - out0, out1, ~
 * Details     : The rows of the matrix become columns, and the columns become rows.
 * Example     : LASX_TRANSPOSE8x8_H_128SV
 *         in0 : 1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8
 *         in1 : 8,2,3,4, 5,6,7,8, 8,2,3,4, 5,6,7,8
 *         in2 : 8,2,3,4, 5,6,7,8, 8,2,3,4, 5,6,7,8
 *         in3 : 1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8
 *         in4 : 9,2,3,4, 5,6,7,8, 9,2,3,4, 5,6,7,8
 *         in5 : 1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8
 *         in6 : 1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8
 *         in7 : 9,2,3,4, 5,6,7,8, 9,2,3,4, 5,6,7,8
 *
 *        out0 : 1,8,8,1, 9,1,1,9, 1,8,8,1, 9,1,1,9
 *        out1 : 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2
 *        out2 : 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3
 *        out3 : 4,4,4,4, 4,4,4,4, 4,4,4,4, 4,4,4,4
 *        out4 : 5,5,5,5, 5,5,5,5, 5,5,5,5, 5,5,5,5
 *        out5 : 6,6,6,6, 6,6,6,6, 6,6,6,6, 6,6,6,6
 *        out6 : 7,7,7,7, 7,7,7,7, 7,7,7,7, 7,7,7,7
 *        out7 : 8,8,8,8, 8,8,8,8, 8,8,8,8, 8,8,8,8
 */
#define LASX_TRANSPOSE8x8_H_128SV(in0, in1, in2, in3, in4, in5, in6, in7,           \
                                  out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                                   \
    __m256i s0_m, s1_m;                                                             \
    __m256i tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                         \
    __m256i tmp4_m, tmp5_m, tmp6_m, tmp7_m;                                         \
                                                                                    \
    LASX_ILVL_H_2_128SV(in6, in4, in7, in5, s0_m, s1_m);                            \
    LASX_ILVLH_H_128SV(s1_m, s0_m, tmp1_m, tmp0_m);                                 \
    LASX_ILVH_H_2_128SV(in6, in4, in7, in5, s0_m, s1_m);                            \
    LASX_ILVLH_H_128SV(s1_m, s0_m, tmp3_m, tmp2_m);                                 \
                                                                                    \
    LASX_ILVL_H_2_128SV(in2, in0, in3, in1, s0_m, s1_m);                            \
    LASX_ILVLH_H_128SV(s1_m, s0_m, tmp5_m, tmp4_m);                                 \
    LASX_ILVH_H_2_128SV(in2, in0, in3, in1, s0_m, s1_m);                            \
    LASX_ILVLH_H_128SV(s1_m, s0_m, tmp7_m, tmp6_m);                                 \
                                                                                    \
    LASX_PCKEV_D_4_128SV(tmp0_m, tmp4_m, tmp1_m, tmp5_m, tmp2_m, tmp6_m,            \
                         tmp3_m, tmp7_m, out0, out2, out4, out6);                   \
    LASX_PCKOD_D_4_128SV(tmp0_m, tmp4_m, tmp1_m, tmp5_m, tmp2_m, tmp6_m,            \
                         tmp3_m, tmp7_m, out1, out3, out5, out7);                   \
}

/* Description : Transposes 8x8 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 * Details     :
 */
#define LASX_TRANSPOSE8x8_W(in0, in1, in2, in3, in4, in5, in6, in7,         \
                            out0, out1, out2, out3, out4, out5, out6, out7) \
{                                                                           \
    __m256i s0_m, s1_m;                                                     \
    __m256i tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                 \
    __m256i tmp4_m, tmp5_m, tmp6_m, tmp7_m;                                 \
                                                                            \
    LASX_ILVL_W_2_128SV(in2, in0, in3, in1, s0_m, s1_m);                    \
    LASX_ILVLH_W_128SV(s1_m, s0_m, tmp1_m, tmp0_m);                         \
    LASX_ILVH_W_2_128SV(in2, in0, in3, in1, s0_m, s1_m);                    \
    LASX_ILVLH_W_128SV(s1_m, s0_m, tmp3_m, tmp2_m);                         \
                                                                            \
    LASX_ILVL_W_2_128SV(in6, in4, in7, in5, s0_m, s1_m);                    \
    LASX_ILVLH_W_128SV(s1_m, s0_m, tmp5_m, tmp4_m);                         \
    LASX_ILVH_W_2_128SV(in6, in4, in7, in5, s0_m, s1_m);                    \
    LASX_ILVLH_W_128SV(s1_m, s0_m, tmp7_m, tmp6_m);                         \
    LASX_PCKEV_Q_4(tmp4_m, tmp0_m, tmp5_m, tmp1_m, tmp6_m, tmp2_m,          \
                   tmp7_m, tmp3_m, out0, out1, out2, out3);                 \
    LASX_PCKOD_Q_4(tmp4_m, tmp0_m, tmp5_m, tmp1_m, tmp6_m, tmp2_m,          \
                   tmp7_m, tmp3_m, out4, out5, out6, out7);                 \
}

/* Description : Transposes 2x2 block with quad word elements in vectors
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 * Details     :
 */
#define LASX_TRANSPOSE2x2_Q(in0, in1, out0, out1) \
{                                                 \
    __m256i tmp0;                                 \
    tmp0 = __lasx_xvpermi_q(in1, in0, 0x02);      \
    out1 = __lasx_xvpermi_q(in1, in0, 0x13);      \
    out0 = tmp0;                                  \
}

/* Description : Transposes 4x4 block with double word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     :
 */
#define LASX_TRANSPOSE4x4_D(in0, in1, in2, in3, out0, out1, out2, out3) \
{                                                                       \
    __m256i tmp0, tmp1, tmp2, tmp3;                                     \
    LASX_ILVLH_D_2_128SV(in1, in0, in3, in2, tmp0, tmp1, tmp2, tmp3);   \
    out0 = __lasx_xvpermi_q(tmp2, tmp0, 0x20);                          \
    out2 = __lasx_xvpermi_q(tmp2, tmp0, 0x31);                          \
    out1 = __lasx_xvpermi_q(tmp3, tmp1, 0x20);                          \
    out3 = __lasx_xvpermi_q(tmp3, tmp1, 0x31);                          \
}

/* Description : Transpose 4x4 block with half word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 *               Return Type - signed halfword
 */
#define LASX_TRANSPOSE4x4_H_128SV(in0, in1, in2, in3, out0, out1, out2, out3) \
{                                                                             \
    __m256i s0_m, s1_m;                                                       \
                                                                              \
    LASX_ILVL_H_2_128SV(in1, in0, in3, in2, s0_m, s1_m);                      \
    LASX_ILVLH_W_128SV(s1_m, s0_m, out2, out0);                               \
    out1 = __lasx_xvilvh_d(out0, out0);                                       \
    out3 = __lasx_xvilvh_d(out2, out2);                                       \
}

/* Description : Transposes input 8x8 byte block
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *                         (input 8x8 byte block)
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 *                         (output 8x8 byte block)
 * Details     :
 */
#define LASX_TRANSPOSE8x8_B(in0, in1, in2, in3, in4, in5, in6, in7,         \
                            out0, out1, out2, out3, out4, out5, out6, out7) \
{                                                                           \
    __m256i tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                 \
    __m256i tmp4_m, tmp5_m, tmp6_m, tmp7_m;                                 \
    LASX_ILVL_B_4_128SV(in2, in0, in3, in1, in6, in4, in7, in5,             \
                       tmp0_m, tmp1_m, tmp2_m, tmp3_m);                     \
    LASX_ILVLH_B_128SV(tmp1_m, tmp0_m, tmp5_m, tmp4_m);                     \
    LASX_ILVLH_B_128SV(tmp3_m, tmp2_m, tmp7_m, tmp6_m);                     \
    LASX_ILVLH_W_128SV(tmp6_m, tmp4_m, out2, out0);                         \
    LASX_ILVLH_W_128SV(tmp7_m, tmp5_m, out6, out4);                         \
    LASX_SLDI_B_2_0_128SV(out0, out2, out1, out3, 8);                       \
    LASX_SLDI_B_2_0_128SV(out4, out6, out5, out7, 8);                       \
}

/* Description : Transposes input 16x8 byte block
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7,
 *                         in8, in9, in10, in11, in12, in13, in14, in15
 *                         (input 16x8 byte block)
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 *                         (output 8x16 byte block)
 * Details     :
 */
#define LASX_TRANSPOSE16x8_B(in0, in1, in2, in3, in4, in5, in6, in7,              \
                             in8, in9, in10, in11, in12, in13, in14, in15,        \
                             out0, out1, out2, out3, out4, out5, out6, out7)      \
{                                                                                 \
    __m256i tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                       \
    __m256i tmp4_m, tmp5_m, tmp6_m, tmp7_m;                                       \
    __m256i t0, t1, t2, t3, t4, t5, t6, t7;                                       \
    LASX_ILVL_B_8_128SV(in2, in0, in3, in1, in6, in4, in7, in5,                   \
                        in10, in8, in11, in9, in14, in12, in15, in13,             \
                        tmp0_m, tmp1_m, tmp2_m, tmp3_m,                           \
                        tmp4_m, tmp5_m, tmp6_m, tmp7_m);                          \
    LASX_ILVLH_B_2_128SV(tmp1_m, tmp0_m, tmp3_m, tmp2_m, t1, t0, t3, t2);         \
    LASX_ILVLH_B_2_128SV(tmp5_m, tmp4_m, tmp7_m, tmp6_m, t5, t4, t7, t6);         \
    LASX_ILVLH_W_2_128SV(t2, t0, t3, t1, tmp2_m, tmp0_m, tmp6_m, tmp4_m);         \
    LASX_ILVLH_W_2_128SV(t6, t4, t7, t5, tmp3_m, tmp1_m, tmp7_m, tmp5_m);         \
    LASX_ILVLH_D_2_128SV(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out1, out0, out3, out2); \
    LASX_ILVLH_D_2_128SV(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out5, out4, out7, out6); \
}

/* Description : Transposes input 16x8 byte block
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7,
 *                         in8, in9, in10, in11, in12, in13, in14, in15
 *                         (input 16x8 byte block)
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 *                         (output 8x16 byte block)
 * Details     : The rows of the matrix become columns, and the columns become rows.
 * Example     : LASX_TRANSPOSE16x8_H
 *         in0 : 1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in1 : 2,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in2 : 3,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in3 : 4,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in4 : 5,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in5 : 6,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in6 : 7,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in7 : 8,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in8 : 9,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *         in9 : 1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *        in10 : 0,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *        in11 : 2,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *        in12 : 3,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *        in13 : 7,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *        in14 : 5,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *        in15 : 6,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0
 *
 *        out0 : 1,2,3,4,5,6,7,8,9,1,0,2,3,7,5,6
 *        out1 : 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
 *        out2 : 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
 *        out3 : 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
 *        out4 : 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
 *        out5 : 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6
 *        out6 : 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
 *        out7 : 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
 */
#define LASX_TRANSPOSE16x8_H(in0, in1, in2, in3, in4, in5, in6, in7,              \
                             in8, in9, in10, in11, in12, in13, in14, in15,        \
                             out0, out1, out2, out3, out4, out5, out6, out7)      \
{                                                                                 \
    __m256i tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                       \
    __m256i tmp4_m, tmp5_m, tmp6_m, tmp7_m;                                       \
    __m256i t0, t1, t2, t3, t4, t5, t6, t7;                                       \
    LASX_ILVL_H_8_128SV(in2, in0, in3, in1, in6, in4, in7, in5,                   \
                        in10, in8, in11, in9, in14, in12, in15, in13,             \
                        tmp0_m, tmp1_m, tmp2_m, tmp3_m,                           \
                        tmp4_m, tmp5_m, tmp6_m, tmp7_m);                          \
    LASX_ILVLH_H_2_128SV(tmp1_m, tmp0_m, tmp3_m, tmp2_m, t1, t0, t3, t2);         \
    LASX_ILVLH_H_2_128SV(tmp5_m, tmp4_m, tmp7_m, tmp6_m, t5, t4, t7, t6);         \
    LASX_ILVLH_D_2_128SV(t2, t0, t3, t1, tmp2_m, tmp0_m, tmp6_m, tmp4_m);         \
    LASX_ILVLH_D_2_128SV(t6, t4, t7, t5, tmp3_m, tmp1_m, tmp7_m, tmp5_m);         \
    LASX_PCKEV_Q_2(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out0, out1);                   \
    LASX_PCKEV_Q_2(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out2, out3);                   \
                                                                                  \
    LASX_ILVH_H_8_128SV(in2, in0, in3, in1, in6, in4, in7, in5,                   \
                        in10, in8, in11, in9, in14, in12, in15, in13,             \
                        tmp0_m, tmp1_m, tmp2_m, tmp3_m,                           \
                        tmp4_m, tmp5_m, tmp6_m, tmp7_m);                          \
    LASX_ILVLH_H_2_128SV(tmp1_m, tmp0_m, tmp3_m, tmp2_m, t1, t0, t3, t2);         \
    LASX_ILVLH_H_2_128SV(tmp5_m, tmp4_m, tmp7_m, tmp6_m, t5, t4, t7, t6);         \
    LASX_ILVLH_D_2_128SV(t2, t0, t3, t1, tmp2_m, tmp0_m, tmp6_m, tmp4_m);         \
    LASX_ILVLH_D_2_128SV(t6, t4, t7, t5, tmp3_m, tmp1_m, tmp7_m, tmp5_m);         \
    LASX_PCKEV_Q_2(tmp1_m, tmp0_m, tmp3_m, tmp2_m, out4, out5);                   \
    LASX_PCKEV_Q_2(tmp5_m, tmp4_m, tmp7_m, tmp6_m, out6, out7);                   \
}

/* Description : Clips all signed word elements of input vector
 *               between 0 & 255
 * Arguments   : Inputs  - in       (input vector)
 *               Outputs - out_m    (output vector with clipped elements)
 *               Return Type - signed word
 */
#define LASX_CLIP_W_0_255(in, out_m)        \
{                                           \
    out_m = __lasx_xvmaxi_w(in, 0);         \
    out_m = __lasx_xvsat_wu(out_m, 7);      \
}

#define LASX_CLIP_W_0_255_2(in0, in1, out0, out1)  \
{                                                  \
    LASX_CLIP_W_0_255(in0, out0);                  \
    LASX_CLIP_W_0_255(in1, out1);                  \
}

#define LASX_CLIP_W_0_255_4(in0, in1, in2, in3, out0, out1, out2, out3)  \
{                                                                        \
    LASX_CLIP_W_0_255_2(in0, in1, out0, out1);                           \
    LASX_CLIP_W_0_255_2(in2, in3, out2, out3);                           \
}

/* Description : Clips all signed halfword elements of input vector
 *               between 0 & 255
 * Arguments   : Inputs  - in       (input vector)
 *               Outputs - out_m    (output vector with clipped elements)
 *               Return Type - signed halfword
 */
#define LASX_CLIP_H_0_255(in, out_m)        \
{                                           \
    out_m = __lasx_xvmaxi_h(in, 0);         \
    out_m = __lasx_xvsat_hu(out_m, 7);      \
}

#define LASX_CLIP_H_0_255_2(in0, in1, out0, out1)  \
{                                                  \
    LASX_CLIP_H_0_255(in0, out0);                  \
    LASX_CLIP_H_0_255(in1, out1);                  \
}

#define LASX_CLIP_H_0_255_4(in0, in1, in2, in3, out0, out1, out2, out3)  \
{                                                                        \
    LASX_CLIP_H_0_255_2(in0, in1, out0, out1);                           \
    LASX_CLIP_H_0_255_2(in2, in3, out2, out3);                           \
}

/* Description : Clips all halfword elements of input vector between min & max
 *               out = ((in) < (min)) ? (min) : (((in) > (max)) ? (max) : (in))
 * Arguments   : Inputs  - in    (input vector)
 *                       - min   (min threshold)
 *                       - max   (max threshold)
 *               Outputs - in    (output vector with clipped elements)
 *               Return Type - signed halfword
 */
#define LASX_CLIP_H(in, min, max)    \
{                                    \
    in = __lasx_xvmax_h(min, in);    \
    in = __lasx_xvmin_h(max, in);    \
}

/* Description : Dot product and addition of 3 signed byte input vectors
 * Arguments   : Inputs  - in0, in1, in2, coeff0, coeff1, coeff2
 *               Outputs - out0_m
 *               Return Type - signed halfword
 * Details     : Dot product of 'in0' with 'coeff0'
 *               Dot product of 'in1' with 'coeff1'
 *               Dot product of 'in2' with 'coeff2'
 *               Addition of all the 3 vector results
 *               out0_m = (in0 * coeff0) + (in1 * coeff1) + (in2 * coeff2)
 */
#define LASX_DP2ADD_H_B_3(in0, in1, in2, out0_m, coeff0, coeff1, coeff2) \
{                                                                        \
    LASX_DP2_H_B(in0, coeff0, out0_m);                                   \
    LASX_DP2ADD_H_B(out0_m, in1, coeff1, out0_m);                        \
    LASX_DP2ADD_H_B(out0_m, in2, coeff2, out0_m);                        \
}

/* Description : Each byte element is logically xor'ed with immediate 128
 * Arguments   : Inputs  - in0, in1
 *               Outputs - in0, in1 (in-place)
 * Details     : Each unsigned byte element from input vector 'in0' is
 *               logically xor'ed with 128 and result is in-place stored in
 *               'in0' vector
 *               Each unsigned byte element from input vector 'in1' is
 *               logically xor'ed with 128 and result is in-place stored in
 *               'in1' vector
 *               Similar for other pairs
 * Example     : LASX_XORI_B_128(in0)
 *               in0: 9,10,11,12, 13,14,15,16, 121,122,123,124, 125,126,127,128, 17,18,19,20, 21,22,23,24,
 *               248,249,250,251, 252,253,254,255,
 *               in0: 137,138,139,140, 141,142,143,144, 249,250,251,252, 253,254,255,0, 145,146,147,148,
 *               149,150,151,152, 120,121,122,123, 124,125,126,127
 */
#define LASX_XORI_B_128(in0)                                 \
{                                                            \
    in0 = __lasx_xvxori_b(in0, 128);                         \
}
#define LASX_XORI_B_2_128(in0, in1)                          \
{                                                            \
    LASX_XORI_B_128(in0);                                    \
    LASX_XORI_B_128(in1);                                    \
}
#define LASX_XORI_B_4_128(in0, in1, in2, in3)                \
{                                                            \
    LASX_XORI_B_2_128(in0, in1);                             \
    LASX_XORI_B_2_128(in2, in3);                             \
}
#define LASX_XORI_B_8_128(in0, in1, in2, in3, in4, in5, in6, in7)  \
{                                                                  \
    LASX_XORI_B_4_128(in0, in1, in2, in3);                         \
    LASX_XORI_B_4_128(in4, in5, in6, in7);                         \
}

/* Description : Indexed halfword element values are replicated to all
 *               elements in output vector. If 'indx0 < 8' use SPLATI_R_*,
 *               if 'indx0 >= 8' use SPLATI_L_*
 * Arguments   : Inputs  - in, idx0, idx1
 *               Outputs - out0, out1
 * Details     : 'idx0' element value from 'in' vector is replicated to all
 *                elements in 'out0' vector
 *                Valid index range for halfword operation is 0-7
 */
#define LASX_SPLATI_L_H(in, idx0, out0)                        \
{                                                              \
    in = __lasx_xvpermi_q(in, in, 0x02);                       \
    out0 = __lasx_xvrepl128vei_h(in, idx0);                    \
}
#define LASX_SPLATI_H_H(in, idx0, out0)                        \
{                                                              \
    in = __lasx_xvpermi_q(in, in, 0X13);                       \
    out0 = __lasx_xvrepl128vei_h(in, idx0 - 8);                \
}
#define LASX_SPLATI_L_H_2(in, idx0, idx1, out0, out1)          \
{                                                              \
    LASX_SPLATI_L_H(in, idx0, out0);                           \
    out1 = __lasx_xvrepl128vei_h(in, idx1);                    \
}
#define LASX_SPLATI_H_H_2(in, idx0, idx1, out0, out1)          \
{                                                              \
    LASX_SPLATI_H_H(in, idx0, out0);                           \
    out1 = __lasx_xvrepl128vei_h(in, idx1 - 8);                \
}
#define LASX_SPLATI_L_H_4(in, idx0, idx1, idx2, idx3,          \
                          out0, out1, out2, out3)              \
{                                                              \
    LASX_SPLATI_L_H_2(in, idx0, idx1, out0, out1);             \
    out2 = __lasx_xvrepl128vei_h(in, idx2);                    \
    out3 = __lasx_xvrepl128vei_h(in, idx3);                    \
}
#define SPLATI_H_H_4(in, idx0, idx1, idx2, idx3,               \
                     out0, out1, out2, out3)                   \
{                                                              \
    LASX_SPLATI_H_H_2(in, idx0, idx1, out0, out1);             \
    out2 = __lasx_xvrepl128vei_h(in, idx2 - 8);                \
    out3 = __lasx_xvrepl128vei_h(in, idx3 - 8);                \
}

/* Description : Pack even elements of input vectors & xor with 128
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out_m
 * Details     : Signed byte even elements from 'in0' and 'in1' are packed
 *               together in one vector and the resulted vector is xor'ed with
 *               128 to shift the range from signed to unsigned byte
 */
#define LASX_PICKEV_XORI128_B(in0, in1, out_m)  \
{                                               \
    out_m = __lasx_xvpickev_b(in1, in0);        \
    out_m = __lasx_xvxori_b(out_m, 128);        \
}

/* Description : Shift right logical all byte elements of vector.
 * Arguments   : Inputs  - in, shift
 *               Outputs - in (in place)
 * Details     : Each element of vector in is shifted right logical by
 *               number of bits respective element holds in vector shift and
 *               result is in place written to in.
 *               Here, shift is a vector passed in.
 * Example     : See LASX_SRL_W(in, shift)
     */
#define LASX_SRL_B(in, shift)                                         \
{                                                                     \
    in = __lasx_xvsrl_b(in, shift);                                   \
}

#define LASX_SRL_B_2(in0, in1, shift)                                 \
{                                                                     \
    LASX_SRL_B(in0, shift);                                           \
    LASX_SRL_B(in1, shift);                                           \
}

#define LASX_SRL_B_4(in0, in1, in2, in3, shift)                       \
{                                                                     \
    LASX_SRL_B_2(in0, in1, shift);                                    \
    LASX_SRL_B_2(in2, in3, shift);                                    \
}

/* Description : Shift right logical all halfword elements of vector.
 * Arguments   : Inputs  - in, shift
 *               Outputs - in (in place)
 * Details     : Each element of vector in is shifted right logical by
 *               number of bits respective element holds in vector shift and
 *               result is in place written to in.
 *               Here, shift is a vector passed in.
 * Example     : See LASX_SRL_W(in, shift)
 */
#define LASX_SRL_H(in, shift)                                         \
{                                                                     \
    in = __lasx_xvsrl_h(in, shift);                                   \
}

#define LASX_SRL_H_2(in0, in1, shift)                                 \
{                                                                     \
    LASX_SRL_H(in0, shift);                                           \
    LASX_SRL_H(in1, shift);                                           \
}

#define LASX_SRL_H_4(in0, in1, in2, in3, shift)                       \
{                                                                     \
    LASX_SRL_H_2(in0, in1, shift);                                    \
    LASX_SRL_H_2(in2, in3, shift);                                    \
}

/* Description : Shift right logical all word elements of vector.
 * Arguments   : Inputs  - in, shift
 *               Outputs - in (in place)
 * Details     : Each element of vector in is shifted right logical by
 *               number of bits respective element holds in vector shift and
 *               result is in place written to in.
 *               Here, shift is a vector passed in.
 * Example     : LASX_SRL_W(in, shift)
 *          in : 1, 3, 2, -4,      0, -2, 25, 0
 *       shift : 1, 1, 1, 1,       2, 2, 2, 2
 *  in(output) : 0, 1, 1, 32766,   0, 16383, 6, 0
 */
#define LASX_SRL_W(in, shift)                                         \
{                                                                     \
    in = __lasx_xvsrl_w(in, shift);                                   \
}

#define LASX_SRL_W_2(in0, in1, shift)                                 \
{                                                                     \
    LASX_SRL_W(in0, shift);                                           \
    LASX_SRL_W(in1, shift);                                           \
}

#define LASX_SRL_W_4(in0, in1, in2, in3, shift)                       \
{                                                                     \
    LASX_SRL_W_2(in0, in1, shift);                                    \
    LASX_SRL_W_2(in2, in3, shift);                                    \
}

/* Description : Shift right logical all double word elements of vector.
 * Arguments   : Inputs  - in, shift
 *               Outputs - in (in place)
 * Details     : Each element of vector in is shifted right logical by
 *               number of bits respective element holds in vector shift and
 *               result is in place written to in.
 *               Here, shift is a vector passed in.
 * Example     : See LASX_SRL_W(in, shift)
 */
#define LASX_SRL_D(in, shift)                                         \
{                                                                     \
    in = __lasx_xvsrl_d(in, shift);                                   \
}

#define LASX_SRL_D_2(in0, in1, shift)                                 \
{                                                                     \
    LASX_SRL_D(in0, shift);                                           \
    LASX_SRL_D(in1, shift);                                           \
}

#define LASX_SRL_D_4(in0, in1, in2, in3, shift)                       \
{                                                                     \
    LASX_SRL_D_2(in0, in1, shift);                                    \
    LASX_SRL_D_2(in2, in3, shift);                                    \
}


/* Description : Shift right arithmetic rounded (immediate)
 * Arguments   : Inputs  - in0, in1, shift
 *               Outputs - in0, in1, (in place)
 * Details     : Each element of vector 'in0' is shifted right arithmetic by
 *               value in 'shift'.
 *               The last discarded bit is added to shifted value for rounding
 *               and the result is in place written to 'in0'
 *               Similar for other pairs
 * Example     : LASX_SRARI_H(in0, out0, shift)
 *               in0:   1,2,3,4, -5,-6,-7,-8, 19,10,11,12, 13,14,15,16
 *               shift: 2
 *               out0:  0,1,1,1, -1,-1,-2,-2, 5,3,3,3, 3,4,4,4
 */
#define LASX_SRARI_H(in0, out0, shift)                              \
{                                                                   \
    out0 = __lasx_xvsrari_h(in0, shift);                            \
}
#define LASX_SRARI_H_2(in0, in1, out0, out1, shift)                 \
{                                                                   \
    LASX_SRARI_H(in0, out0, shift);                                 \
    LASX_SRARI_H(in1, out1, shift);                                 \
}
#define LASX_SRARI_H_4(in0, in1, in2, in3, out0, out1, out2, out3, shift) \
{                                                                         \
    LASX_SRARI_H_2(in0, in1, out0, out1, shift);                          \
    LASX_SRARI_H_2(in2, in3, out2, out3, shift);                          \
}

/* Description : Shift right arithmetic (immediate)
 * Arguments   : Inputs  - in0, in1, shift
 *               Outputs - in0, in1, (in place)
 * Details     : Each element of vector 'in0' is shifted right arithmetic by
 *               value in 'shift'.
 *               Similar for other pairs
 * Example     : see LASX_SRARI_H(in0, out0, shift)
 */
#define LASX_SRAI_W(in0, out0, shift)                                    \
{                                                                        \
    out0 = __lasx_xvsrai_w(in0, shift);                                  \
}
#define LASX_SRAI_W_2(in0, in1, out0, out1, shift)                       \
{                                                                        \
    LASX_SRAI_W(in0, out0, shift);                                       \
    LASX_SRAI_W(in1, out1, shift);                                       \
}
#define LASX_SRAI_W_4(in0, in1, in2, in3, out0, out1, out2, out3, shift) \
{                                                                        \
    LASX_SRAI_W_2(in0, in1, out0, out1, shift);                          \
    LASX_SRAI_W_2(in2, in3, out2, out3, shift);                          \
}
#define LASX_SRAI_W_8(in0, in1, in2, in3, in4, in5, in6, in7,                 \
                      out0, out1, out2, out3, out4, out5, out6, out7, shift)  \
{                                                                             \
    LASX_SRAI_W_4(in0, in1, in2, in3, out0, out1, out2, out3, shift);         \
    LASX_SRAI_W_4(in4, in5, in6, in7, out4, out5, out6, out7, shift);         \
}

/* Description : Saturate the halfword element values to the max
 *               unsigned value of (sat_val+1 bits)
 *               The element data width remains unchanged
 * Arguments   : Inputs  - in0, in1, in2, in3, sat_val
 *               Outputs - in0, in1, in2, in3 (in place)
 *               Return Type - unsigned halfword
 * Details     : Each unsigned halfword element from 'in0' is saturated to the
 *               value generated with (sat_val+1) bit range
 *               Results are in placed to original vectors
 * Example     : LASX_SAT_H(in0, out0, sat_val)
 *               in0:    1,2,3,4, 5,6,7,8, 19,10,11,12, 13,14,15,16
 *               sat_val:3
 *               out0:   1,2,3,4, 5,6,7,7, 7,7,7,7, 7,7,7,7
 */
#define LASX_SAT_H(in0, out0, sat_val)                                     \
{                                                                          \
    out0 = __lasx_xvsat_h(in0, sat_val);                                   \
} //some error in xvsat_h built-in function
#define LASX_SAT_H_2(in0, in1, out0, out1, sat_val)                        \
{                                                                          \
    LASX_SAT_H(in0, out0, sat_val);                                        \
    LASX_SAT_H(in1, out1, sat_val);                                        \
}
#define LASX_SAT_H_4(in0, in1, in2, in3, out0, out1, out2, out3, sat_val)  \
{                                                                          \
    LASX_SAT_H_2(in0, in1, out0, out1, sat_val);                           \
    LASX_SAT_H_2(in2, in3, out2, out3, sat_val);                           \
}

/* Description : Addition of 2 pairs of vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 * Details     : Each halfwords element from 2 pairs vectors is added
 *               and 2 results are produced
 * Example     : LASX_ADD_H(in0, in1, out)
 *               in0:  1,2,3,4, 5,6,7,8, 1,2,3,4, 5,6,7,8
 *               in1:  8,7,6,5, 4,3,2,1, 8,7,6,5, 4,3,2,1
 *               out:  9,9,9,9, 9,9,9,9, 9,9,9,9, 9,9,9,9
 */
#define LASX_ADD_H(in0, in1, out)             \
{                                             \
    out = __lasx_xvadd_h(in0, in1);           \
}
#define LASX_ADD_H_2(in0, in1, in2, in3, out0, out1) \
{                                                    \
    LASX_ADD_H(in0, in1, out0);                      \
    LASX_ADD_H(in2, in3, out1);                      \
}
#define LASX_ADD_H_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3)      \
{                                                                                         \
    LASX_ADD_H_2(in0, in1, in2, in3, out0, out1);                                         \
    LASX_ADD_H_2(in4, in5, in6, in7, out2, out3);                                         \
}
#define LASX_ADD_H_8(in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, in11, in12, \
                     in13, in14, in15, out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                                        \
    LASX_ADD_H_4(in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, out2, out3);        \
    LASX_ADD_H_4(in8, in9, in10, in11, in12, in13, in14, in15, out4, out5, out6, out7);  \
}

/* Description : Horizontal subtraction of unsigned byte vector elements
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Each unsigned odd byte element from 'in0' is subtracted from
 *               even unsigned byte element from 'in0' (pairwise) and the
 *               halfword result is written to 'out0'
 */
#define LASX_HSUB_UB_2(in0, in1, out0, out1)   \
{                                              \
    out0 = __lasx_xvhsubw_hu_bu(in0, in0);     \
    out1 = __lasx_xvhsubw_hu_bu(in1, in1);     \
}

#define LASX_HSUB_UB_4(in0, in1, in2, in3, out0, out1, out2, out3)    \
{                                                                     \
    LASX_HSUB_UB_2(in0, in1, out0, out1);                                   \
    LASX_HSUB_UB_2(in2, in3, out2, out3);                                   \
}

/* Description : Shuffle byte vector elements as per mask vector
 * Arguments   : Inputs  - in0, in1, in2, in3, mask0, mask1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Selective byte elements from in0 & in1 are copied to out0 as
 *               per control vector mask0
 *               Selective byte elements from in2 & in3 are copied to out1 as
 *               per control vector mask1
 * Example     : LASX_SHUF_B_128SV(in0, in1,  mask0, out0)
 *               in_h :  9,10,11,12, 13,14,15,16, 0,0,0,0, 0,0,0,0,
 *                      17,18,19,20, 21,22,23,24, 0,0,0,0, 0,0,0,0
 *               in_l :  1, 2, 3, 4,  5, 6, 7, 8, 0,0,0,0, 0,0,0,0,
 *                      25,26,27,28, 29,30,31,32, 0,0,0,0, 0,0,0,0
 *               mask0:  0, 1, 2, 3,  4, 5, 6, 7, 16,17,18,19, 20,21,22,23,
 *                      16,17,18,19, 20,21,22,23,  0, 1, 2, 3,  4, 5, 6, 7
 *               out0 :  1, 2, 3, 4,  5, 6, 7, 8,  9,10,11,12, 13,14,15,16,
 *                      17,18,19,20, 21,22,23,24, 25,26,27,28, 29,30,31,32
 */

#define LASX_SHUF_B_128SV(in_h, in_l,  mask0, out0)                            \
{                                                                              \
    out0 = __lasx_xvshuf_b(in_h, in_l, mask0);                                 \
}
#define LASX_SHUF_B_2_128SV(in0_h, in0_l, in1_h, in1_l, mask0, mask1,          \
                            out0, out1)                                        \
{                                                                              \
    LASX_SHUF_B_128SV(in0_h, in0_l,  mask0, out0);                             \
    LASX_SHUF_B_128SV(in1_h, in1_l,  mask1, out1);                             \
}
#define LASX_SHUF_B_4_128SV(in0_h, in0_l, in1_h, in1_l, in2_h, in2_l,          \
                            in3_h, in3_l, mask0, mask1, mask2, mask3,          \
                            out0, out1, out2, out3)                            \
{                                                                              \
    LASX_SHUF_B_2_128SV(in0_h, in0_l, in1_h, in1_l, mask0, mask1, out0, out1); \
    LASX_SHUF_B_2_128SV(in2_h, in2_l, in3_h, in3_l, mask2, mask3, out2, out3); \
}

/* Description : Addition of signed halfword elements and signed saturation
 * Arguments   : Inputs  - in0, in1, in2, in3 ~
 *               Outputs - out0, out1 ~
 * Details     : Signed halfword elements from 'in0' are added to signed
 *               halfword elements of 'in1'. The result is then signed saturated
 *               between -32768 to +32767 (as per halfword data type)
 *               Similar for other pairs
 * Example     : LASX_SADD_H(in0, in1, out0)
 *               in0:   1,2,32766,4, 5,6,7,8, 1,2,3,4, 5,6,7,8,
 *               in1:   8,7,30586,5, 4,3,2,1, 8,7,6,5, 4,3,2,1,
 *               out0:  9,9,32767,9, 9,9,9,9, 9,9,9,9, 9,9,9,9,
 */
#define LASX_SADD_H(in0, in1, out0)                            \
{                                                              \
    out0 = __lasx_xvsadd_h(in0, in1);                          \
}
#define LASX_SADD_H_2(in0, in1, in2, in3, out0, out1)          \
{                                                              \
    LASX_SADD_H(in0, in1, out0);                               \
    LASX_SADD_H(in2, in3, out1);                               \
}
#define LASX_SADD_H_4(in0, in1, in2, in3, in4, in5, in6, in7,  \
                      out0, out1, out2, out3)                  \
{                                                              \
    LASX_SADD_H_2(in0, in1, in2, in3, out0, out1);             \
    LASX_SADD_H_2(in4, in5, in6, in7, out2, out3);             \
}

/* Description : Average with rounding (in0 + in1 + 1) / 2.
 * Arguments   : Inputs  - in0, in1, in2, in3,
 *               Outputs - out0, out1
 * Details     : Each unsigned byte element from 'in0' vector is added with
 *               each unsigned byte element from 'in1' vector.
 *               Average with rounding is calculated and written to 'out0'
 */
#define LASX_AVER_BU( in0, in1, out0 )   \
{                                        \
    out0 = __lasx_xvavgr_bu( in0, in1 ); \
}

#define LASX_AVER_BU_2( in0, in1, in2, in3, out0, out1 )  \
{                                                         \
    LASX_AVER_BU( in0, in1, out0 );                       \
    LASX_AVER_BU( in2, in3, out1 );                       \
}

#define LASX_AVER_BU_4( in0, in1, in2, in3, in4, in5, in6, in7,  \
                        out0, out1, out2, out3 )                 \
{                                                                \
    LASX_AVER_BU_2( in0, in1, in2, in3, out0, out1 );            \
    LASX_AVER_BU_2( in4, in5, in6, in7, out2, out3 );            \
}

/* Description : Butterfly of 4 input vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     : Butterfly operationuu
 */
#define LASX_BUTTERFLY_4(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3)  \
{                                                                            \
    out0 = (__m256i)( (RTYPE)in0 + (RTYPE)in3 );                             \
    out1 = (__m256i)( (RTYPE)in1 + (RTYPE)in2 );                             \
                                                                             \
    out2 = (__m256i)( (RTYPE)in1 - (RTYPE)in2 );                             \
    out3 = (__m256i)( (RTYPE)in0 - (RTYPE)in3 );                             \
}

/* Description : Butterfly of 8 input vectors
 * Arguments   : Inputs  - in0 in1 in2 ~
 *               Outputs - out0 out1 out2 ~
 * Details     : Butterfly operation
 */
#define LASX_BUTTERFLY_8(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,    \
                         out0, out1, out2, out3, out4, out5, out6, out7)   \
{                                                                          \
    out0 = (__m256i)( (RTYPE)in0 + (RTYPE)in7 );                           \
    out1 = (__m256i)( (RTYPE)in1 + (RTYPE)in6 );                           \
    out2 = (__m256i)( (RTYPE)in2 + (RTYPE)in5 );                           \
    out3 = (__m256i)( (RTYPE)in3 + (RTYPE)in4 );                           \
                                                                           \
    out4 = (__m256i)( (RTYPE)in3 - (RTYPE)in4 );                           \
    out5 = (__m256i)( (RTYPE)in2 - (RTYPE)in5 );                           \
    out6 = (__m256i)( (RTYPE)in1 - (RTYPE)in6 );                           \
    out7 = (__m256i)( (RTYPE)in0 - (RTYPE)in7 );                           \
}

#endif /* GENERIC_MACROS_LASX_H */
