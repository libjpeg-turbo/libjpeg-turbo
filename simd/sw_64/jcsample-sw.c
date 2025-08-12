/*
 * sw_64 optimizations for libjpeg-turbo
 *
 * Copyright (C) 2025, Sunway Limited.  All Rights Reserved.
 *
 * Authors:  WangCong     <776111474@qq.com> 
 *
 * Based on the x86 SIMD extension for IJG JPEG library
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
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

/* CHROMA DOWNSAMPLING */

#include "jsimd_sw.h"
#include "jcsample.h"

const short Fc_downsample[32] __attribute__((aligned(32))) = {1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
const unsigned int Fc_con[16] __attribute__((aligned(32))) = {0x06040200,0x0E0C0A08,0x80808080,0x80808080,0x06040200,0x0E0C0A08,0x80808080,0x80808080,0xDC985410,0,0,0,0,0,0,0};

void jsimd_h2v2_downsample_sw(JDIMENSION image_width, int max_v_samp_factor,
                               JDIMENSION v_samp_factor,
                               JDIMENSION width_in_blocks,
                               JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow, outrow;
  JDIMENSION outcol;
  JDIMENSION output_cols =  width_in_blocks * DCTSIZE;
  register _JSAMPROW inptr0, inptr1, outptr;
  register int bias;
  register double v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16;

  expand_right_edge(input_data, max_v_samp_factor, image_width,
                    output_cols * 2);

  inrow = 0;

  for (outrow = 0; outrow < v_samp_factor; outrow++)
  {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];

    bias = 1;

    for (outcol = 0; outcol < output_cols/32; outcol++)
    {

     __asm__ __volatile__(

     "VLDD  %16,0(%17)\n\t"   
     "VLDD  %15,32(%17)\n\t"  
     "VLDD  %14,0(%18)\n\t"   
     "VLDD  %13,32(%18)\n\t"  

     "VSLLH %16,8,%12\n\t"
     "VSRLH %12,8,%11\n\t"    
     "VSLLH %15,8,%12\n\t"
     "VSRLH %12,8,%10\n\t"    
     "VSRLH %16,8,%9\n\t"     
     "VSRLH %15,8,%8\n\t"      

     "VSLLH %14,8,%12\n\t"
     "VSRLH %12,8,%7\n\t"     
     "VSLLH %13,8,%12\n\t"
     "VSRLH %12,8,%6\n\t"     
     "VSRLH %14,8,%5\n\t"     
     "VSRLH %13,8,%4\n\t"     

     "VLDD  %0,0(%20)\n\t"
     "VUCADDH %11,%9,%3\n\t"
     "VUCADDH %3,%7,%2\n\t"
     "VUCADDH %2,%5,%1\n\t"
     "VUCADDH %1,%0,%16\n\t" 
     "VSRLH %16,2,%15\n\t"    

     "VUCADDH %10,%8,%3\n\t"
     "VUCADDH %3,%6,%2\n\t"
     "VUCADDH %2,%4,%1\n\t"
     "VUCADDH %1,%0,%14\n\t"  
     "VSRLH %14,2,%13\n\t"             

     "VLDD  %0,0(%21)\n\t"
     "VSHFQB %15,%0,%12\n\t"  
     "VSHFQB %13,%0,%11\n\t"  

     "VLDD  %0,32(%21)\n\t"
     "VSHFW %12,%11,%0,%10\n\t"           


     "VSTD  %10,0(%19)\n\t"

     :"=f"(v0),"=f"(v1),"=f"(v2),"=f"(v3),"=f"(v4),"=f"(v5),"=f"(v6),"=f"(v7),"=f"(v8),"=f"(v9),"=f"(v10),"=f"(v11),"=f"(v12),"=f"(v13),"=f"(v14),"=f"(v15),"=f"(v16)
     :"r"(inptr0),"r"(inptr1),"r"(outptr),"r"(Fc_downsample),"r"(Fc_con)
     :
     );

     inptr0 += 64;  inptr1 += 64; outptr +=32;
    }
    
    for (outcol = (output_cols - output_cols % 32); outcol < output_cols; outcol++)
    {
      *outptr++ = (_JSAMPLE)((inptr0[0] + inptr0[1] + inptr1[0] + inptr1[1] + bias) >> 2);
      bias ^= 3;                /* 1=>2, 2=>1 */
      inptr0 += 2;  inptr1 += 2;
    }

    inrow += 2;
  }
}
