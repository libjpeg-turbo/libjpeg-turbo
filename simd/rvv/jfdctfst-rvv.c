/*
 * jfdctfst-rvv.c - fast integer FDCT
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
 */
#include "jsimd_rvv.h"


#define F_0_382  98     /* FIX(0.382683433) */
#define F_0_541  139    /* FIX(0.541196100) */
#define F_0_707  181    /* FIX(0.707106781) */
#define F_1_306  334    /* FIX(1.306562965) */

#define CONST_BITS  8


#define DO_DCT() { \
    /* Even part */ \
    tmp10 = vadd_vv_i16m1(tmp0, tmp3, vl); \
    tmp13 = vsub_vv_i16m1(tmp0, tmp3, vl); \
    tmp11 = vadd_vv_i16m1(tmp1, tmp2, vl); \
    tmp12 = vsub_vv_i16m1(tmp1, tmp2, vl); \
    \
    out0 = vadd_vv_i16m1(tmp10, tmp11, vl); \
    out4 = vsub_vv_i16m1(tmp10, tmp11, vl); \
    \
    z1 = vadd_vv_i16m1(tmp12, tmp13, vl); \
    pdt = vwmul_vx_i32m2(z1, F_0_707, vl); \
    z1 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    out2 = vadd_vv_i16m1(tmp13, z1, vl); \
    out6 = vsub_vv_i16m1(tmp13, z1, vl); \
    \
    /* Odd part */ \
    tmp10 = vadd_vv_i16m1(tmp4, tmp5, vl);\
    tmp11 = vadd_vv_i16m1(tmp5, tmp6, vl); \
    tmp12 = vadd_vv_i16m1(tmp6, tmp7, vl); \
    \
    z5 = vsub_vv_i16m1(tmp10, tmp12, vl); \
    pdt = vwmul_vx_i32m2(z5, F_0_382, vl); \
    z5 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    pdt = vwmul_vx_i32m2(tmp10, F_0_541, vl); \
    z2 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    z2 = vadd_vv_i16m1(z2, z5, vl); \
    pdt = vwmul_vx_i32m2(tmp12, F_1_306, vl); \
    z4 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    z4 = vadd_vv_i16m1(z4, z5, vl); \
    pdt = vwmul_vx_i32m2(tmp11, F_0_707, vl); \
    z3 = vnsra_wx_i16m1(pdt, CONST_BITS, vl); \
    \
    z11 = vadd_vv_i16m1(tmp7, z3, vl); \
    z13 = vsub_vv_i16m1(tmp7, z3, vl); \
    \
    out5 = vadd_vv_i16m1(z13, z2, vl); \
    out3 = vsub_vv_i16m1(z13, z2, vl); \
    out1 = vadd_vv_i16m1(z11, z4, vl); \
    out7 = vsub_vv_i16m1(z11, z4, vl); \
}


/* Note: We can assume that a vector register has at least 128 bits. */
void jsimd_fdct_ifast_rvv(DCTELEM *data)
{
    vint16m1_t row0, row1, row2, row3, row4, row5, row6, row7,
               col0, col1, col2, col3, col4, col5, col6, col7,
               tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
               tmp10, tmp11, tmp12, tmp13,
               z1, z2, z3, z4, z5, z11, z13,
               out0, out1, out2, out3, out4, out5, out6, out7;
    vint32m2_t pdt;
    size_t vl = vsetvl_e16m1(DCTSIZE), 
           col_stride = DCTSIZE * sizeof(DCTELEM);

    /* Pass 1: process rows */
    /* Load columns */
    col0 = vlse16_v_i16m1(data + 0, col_stride, vl);
    col1 = vlse16_v_i16m1(data + 1, col_stride, vl);
    col2 = vlse16_v_i16m1(data + 2, col_stride, vl);
    col3 = vlse16_v_i16m1(data + 3, col_stride, vl);
    col4 = vlse16_v_i16m1(data + 4, col_stride, vl);
    col5 = vlse16_v_i16m1(data + 5, col_stride, vl);
    col6 = vlse16_v_i16m1(data + 6, col_stride, vl);
    col7 = vlse16_v_i16m1(data + 7, col_stride, vl);

    tmp0 = vadd_vv_i16m1(col0, col7, vl);
    tmp7 = vsub_vv_i16m1(col0, col7, vl);
    tmp1 = vadd_vv_i16m1(col1, col6, vl);
    tmp6 = vsub_vv_i16m1(col1, col6, vl);
    tmp2 = vadd_vv_i16m1(col2, col5, vl);
    tmp5 = vsub_vv_i16m1(col2, col5, vl);
    tmp3 = vadd_vv_i16m1(col3, col4, vl);
    tmp4 = vsub_vv_i16m1(col3, col4, vl);

    DO_DCT();

    /* Store columns */
    vsse16_v_i16m1(data + 0, col_stride, out0, vl);
    vsse16_v_i16m1(data + 1, col_stride, out1, vl);
    vsse16_v_i16m1(data + 2, col_stride, out2, vl);
    vsse16_v_i16m1(data + 3, col_stride, out3, vl);
    vsse16_v_i16m1(data + 4, col_stride, out4, vl);
    vsse16_v_i16m1(data + 5, col_stride, out5, vl);
    vsse16_v_i16m1(data + 6, col_stride, out6, vl);
    vsse16_v_i16m1(data + 7, col_stride, out7, vl);

    /* Pass 2: process columns */
    /* Load rows */
    row0 = vle16_v_i16m1(data + DCTSIZE * 0, vl);
    row1 = vle16_v_i16m1(data + DCTSIZE * 1, vl);
    row2 = vle16_v_i16m1(data + DCTSIZE * 2, vl);
    row3 = vle16_v_i16m1(data + DCTSIZE * 3, vl);
    row4 = vle16_v_i16m1(data + DCTSIZE * 4, vl);
    row5 = vle16_v_i16m1(data + DCTSIZE * 5, vl);
    row6 = vle16_v_i16m1(data + DCTSIZE * 6, vl);
    row7 = vle16_v_i16m1(data + DCTSIZE * 7, vl);

    tmp0 = vadd_vv_i16m1(row0, row7, vl);
    tmp7 = vsub_vv_i16m1(row0, row7, vl);
    tmp1 = vadd_vv_i16m1(row1, row6, vl);
    tmp6 = vsub_vv_i16m1(row1, row6, vl);
    tmp2 = vadd_vv_i16m1(row2, row5, vl);
    tmp5 = vsub_vv_i16m1(row2, row5, vl);
    tmp3 = vadd_vv_i16m1(row3, row4, vl);
    tmp4 = vsub_vv_i16m1(row3, row4, vl);

    DO_DCT();

    /* Store rows */
    vse16_v_i16m1(data + DCTSIZE * 0, out0, vl);
    vse16_v_i16m1(data + DCTSIZE * 1, out1, vl);
    vse16_v_i16m1(data + DCTSIZE * 2, out2, vl);
    vse16_v_i16m1(data + DCTSIZE * 3, out3, vl);
    vse16_v_i16m1(data + DCTSIZE * 4, out4, vl);
    vse16_v_i16m1(data + DCTSIZE * 5, out5, vl);
    vse16_v_i16m1(data + DCTSIZE * 6, out6, vl);
    vse16_v_i16m1(data + DCTSIZE * 7, out7, vl);
}
