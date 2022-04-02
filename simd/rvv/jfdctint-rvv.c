/*
 * jfdctint-rvv.c - accurate integer FDCT
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

#define CONST_BITS  13
#define PASS1_BITS  2

#define F_0_298  2446           /* FIX(0.298631336) */
#define F_0_390  3196           /* FIX(0.390180644) */
#define F_0_541  4433           /* FIX(0.541196100) */
#define F_0_765  6270           /* FIX(0.765366865) */
#define F_0_899  7373           /* FIX(0.899976223) */
#define F_1_175  9633           /* FIX(1.175875602) */
#define F_1_501  12299          /* FIX(1.501321110) */
#define F_1_847  15137          /* FIX(1.847759065) */
#define F_1_961  16069          /* FIX(1.961570560) */
#define F_2_053  16819          /* FIX(2.053119869) */
#define F_2_562  20995          /* FIX(2.562915447) */
#define F_3_072  25172          /* FIX(3.072711026) */

#define ROUND_ADD(n)    (int32_t)1 << ((n) - 1)


/* Note: We can assume that a vector register has at least 128 bits. */
void jsimd_fdct_islow_rvv(DCTELEM *data)
{
    vint16m1_t row0, row1, row2, row3, row4, row5, row6, row7,
               col0, col1, col2, col3, col4, col5, col6, col7,
               tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7,
               tmp10, tmp11, tmp12, tmp13,
               z1, z2, z3, z4, z5, z11, z13,
               out0, out1, out2, out3, out4, out5, out6, out7;
    vint32m2_t p1, p2, p3, p4, p5, t4, t5, t6, t7, temp;
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

    /* Even part */
    tmp10 = vadd_vv_i16m1(tmp0, tmp3, vl);
    tmp13 = vsub_vv_i16m1(tmp0, tmp3, vl);
    tmp11 = vadd_vv_i16m1(tmp1, tmp2, vl);
    tmp12 = vsub_vv_i16m1(tmp1, tmp2, vl);
    
    out0 = vadd_vv_i16m1(tmp10, tmp11, vl);
    out0 = vsll_vx_i16m1(out0, PASS1_BITS, vl);
    out4 = vsub_vv_i16m1(tmp10, tmp11, vl);
    out4 = vsll_vx_i16m1(out4, PASS1_BITS, vl);
    
    z1 = vadd_vv_i16m1(tmp12, tmp13, vl);
    p1 = vwmul_vx_i32m2(z1, F_0_541, vl);
    
    temp = vwmul_vx_i32m2(tmp13, F_0_765, vl);
    temp = vadd_vv_i32m2(p1, temp, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS - PASS1_BITS), vl);
    out2 = vnsra_wx_i16m1(temp, CONST_BITS - PASS1_BITS, vl);
    
    temp = vwmul_vx_i32m2(tmp12, F_1_847, vl);
    temp = vsub_vv_i32m2(p1, temp, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS - PASS1_BITS), vl);
    out6 = vnsra_wx_i16m1(temp, CONST_BITS - PASS1_BITS, vl);
    
    /* Odd part */
    z1 = vadd_vv_i16m1(tmp4, tmp7, vl);
    z2 = vadd_vv_i16m1(tmp5, tmp6, vl);
    z3 = vadd_vv_i16m1(tmp4, tmp6, vl);
    z4 = vadd_vv_i16m1(tmp5, tmp7, vl);
    z5 = vadd_vv_i16m1(z3, z4, vl);
    p5 = vwmul_vx_i32m2(z5, F_1_175, vl);
    
    t4 = vwmul_vx_i32m2(tmp4, F_0_298, vl);
    t5 = vwmul_vx_i32m2(tmp5, F_2_053, vl);
    t6 = vwmul_vx_i32m2(tmp6, F_3_072, vl);
    t7 = vwmul_vx_i32m2(tmp7, F_1_501, vl);
    
    p1 = vwmul_vx_i32m2(z1, -F_0_899, vl);
    p2 = vwmul_vx_i32m2(z2, -F_2_562, vl);
    p3 = vwmul_vx_i32m2(z3, -F_1_961, vl);
    p4 = vwmul_vx_i32m2(z4, -F_0_390, vl);
    
    p3 = vadd_vv_i32m2(p3, p5, vl);
    p4 = vadd_vv_i32m2(p4, p5, vl);
    
    temp = vadd_vv_i32m2(t4, p1, vl);
    temp = vadd_vv_i32m2(temp, p3, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS - PASS1_BITS), vl);
    out7 = vnsra_wx_i16m1(temp, CONST_BITS - PASS1_BITS, vl);
    
    temp = vadd_vv_i32m2(t5, p2, vl);
    temp = vadd_vv_i32m2(temp, p4, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS - PASS1_BITS), vl);
    out5 = vnsra_wx_i16m1(temp, CONST_BITS - PASS1_BITS, vl);
    
    temp = vadd_vv_i32m2(t6, p2, vl);
    temp = vadd_vv_i32m2(temp, p3, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS - PASS1_BITS), vl);
    out3 = vnsra_wx_i16m1(temp, CONST_BITS - PASS1_BITS, vl);
    
    temp = vadd_vv_i32m2(t7, p1, vl);
    temp = vadd_vv_i32m2(temp, p4, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS - PASS1_BITS), vl);
    out1 = vnsra_wx_i16m1(temp, CONST_BITS - PASS1_BITS, vl);

    /* Store columns */
    vsse16_v_i16m1(data + 0, col_stride, out0, vl);
    vsse16_v_i16m1(data + 1, col_stride, out1, vl);
    vsse16_v_i16m1(data + 2, col_stride, out2, vl);
    vsse16_v_i16m1(data + 3, col_stride, out3, vl);
    vsse16_v_i16m1(data + 4, col_stride, out4, vl);
    vsse16_v_i16m1(data + 5, col_stride, out5, vl);
    vsse16_v_i16m1(data + 6, col_stride, out6, vl);
    vsse16_v_i16m1(data + 7, col_stride, out7, vl);

    /* Pass 2: process rows */
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

    /* Even part */
    tmp10 = vadd_vv_i16m1(tmp0, tmp3, vl);
    tmp13 = vsub_vv_i16m1(tmp0, tmp3, vl);
    tmp11 = vadd_vv_i16m1(tmp1, tmp2, vl);
    tmp12 = vsub_vv_i16m1(tmp1, tmp2, vl);
    
    out0 = vadd_vv_i16m1(tmp10, tmp11, vl);
    out0 = vadd_vx_i16m1(out0, ROUND_ADD(PASS1_BITS), vl);
    out0 = vsra_vx_i16m1(out0, PASS1_BITS, vl);
    out4 = vsub_vv_i16m1(tmp10, tmp11, vl);
    out4 = vadd_vx_i16m1(out4, ROUND_ADD(PASS1_BITS), vl);
    out4 = vsra_vx_i16m1(out4, PASS1_BITS, vl);
    
    z1 = vadd_vv_i16m1(tmp12, tmp13, vl);
    p1 = vwmul_vx_i32m2(z1, F_0_541, vl);
    
    temp = vwmul_vx_i32m2(tmp13, F_0_765, vl);
    temp = vadd_vv_i32m2(p1, temp, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS + PASS1_BITS), vl);
    out2 = vnsra_wx_i16m1(temp, CONST_BITS + PASS1_BITS, vl);
    
    temp = vwmul_vx_i32m2(tmp12, F_1_847, vl);
    temp = vsub_vv_i32m2(p1, temp, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS + PASS1_BITS), vl);
    out6 = vnsra_wx_i16m1(temp, CONST_BITS + PASS1_BITS, vl);
    
    /* Odd part */
    z1 = vadd_vv_i16m1(tmp4, tmp7, vl);
    z2 = vadd_vv_i16m1(tmp5, tmp6, vl);
    z3 = vadd_vv_i16m1(tmp4, tmp6, vl);
    z4 = vadd_vv_i16m1(tmp5, tmp7, vl);
    z5 = vadd_vv_i16m1(z3, z4, vl);
    p5 = vwmul_vx_i32m2(z5, F_1_175, vl);
    
    t4 = vwmul_vx_i32m2(tmp4, F_0_298, vl);
    t5 = vwmul_vx_i32m2(tmp5, F_2_053, vl);
    t6 = vwmul_vx_i32m2(tmp6, F_3_072, vl);
    t7 = vwmul_vx_i32m2(tmp7, F_1_501, vl);
    
    p1 = vwmul_vx_i32m2(z1, -F_0_899, vl);
    p2 = vwmul_vx_i32m2(z2, -F_2_562, vl);
    p3 = vwmul_vx_i32m2(z3, -F_1_961, vl);
    p4 = vwmul_vx_i32m2(z4, -F_0_390, vl);
    
    p3 = vadd_vv_i32m2(p3, p5, vl);
    p4 = vadd_vv_i32m2(p4, p5, vl);
    
    temp = vadd_vv_i32m2(t4, p1, vl);
    temp = vadd_vv_i32m2(temp, p3, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS + PASS1_BITS), vl);
    out7 = vnsra_wx_i16m1(temp, CONST_BITS + PASS1_BITS, vl);
    
    temp = vadd_vv_i32m2(t5, p2, vl);
    temp = vadd_vv_i32m2(temp, p4, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS + PASS1_BITS), vl);
    out5 = vnsra_wx_i16m1(temp, CONST_BITS + PASS1_BITS, vl);
    
    temp = vadd_vv_i32m2(t6, p2, vl);
    temp = vadd_vv_i32m2(temp, p3, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS + PASS1_BITS), vl);
    out3 = vnsra_wx_i16m1(temp, CONST_BITS + PASS1_BITS, vl);
    
    temp = vadd_vv_i32m2(t7, p1, vl);
    temp = vadd_vv_i32m2(temp, p4, vl);
    temp = vadd_vx_i32m2(temp, ROUND_ADD(CONST_BITS + PASS1_BITS), vl);
    out1 = vnsra_wx_i16m1(temp, CONST_BITS + PASS1_BITS, vl);

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
