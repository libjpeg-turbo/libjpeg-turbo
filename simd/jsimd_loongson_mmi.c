/*
 * jidctint.c
 *
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a slow-but-accurate integer implementation of the
 * inverse DCT (Discrete Cosine Transform).  In the IJG code, this routine
 * must also perform dequantization of the input coefficients.
 *
 * A 2-D IDCT can be done by 1-D IDCT on each column followed by 1-D IDCT
 * on each row (or vice versa, but it's more convenient to emit a row at
 * a time).  Direct algorithms are also available, but they are much more
 * complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on an algorithm described in
 *   C. Loeffler, A. Ligtenberg and G. Moschytz, "Practical Fast 1-D DCT
 *   Algorithms with 11 Multiplications", Proc. Int'l. Conf. on Acoustics,
 *   Speech, and Signal Processing 1989 (ICASSP '89), pp. 988-991.
 * The primary algorithm described there uses 11 multiplies and 29 adds.
 * We use their alternate method with 12 multiplies and 32 adds.
 * The advantage of this method is that no data path contains more than one
 * multiplication; this allows a very simple and accurate implementation in
 * scaled fixed-point arithmetic, with a minimal number of shifts.
 *
 */

#define JPEG_INTERNALS
#include "../jinclude.h"
#include "../jpeglib.h"
#include "../jdct.h"		/* Private declarations for DCT subsystem */
#include "loongson-mmintrin.h"
#ifdef DCT_ISLOW_SUPPORTED

/*
 * This module is specialized to the case DCTSIZE = 8.
 */

#if DCTSIZE != 8
Sorry, this code only copes with 8x8 DCTs. /* deliberate syntax err */
#endif

/*
 * The poop on this scaling stuff is as follows:
 *
 * Each 1-D IDCT step produces outputs which are a factor of sqrt(N)
 * larger than the true IDCT outputs.  The final outputs are therefore
 * a factor of N larger than desired; since N=8 this can be cured by
 * a simple right shift at the end of the algorithm.  The advantage of
 * this arrangement is that we save two multiplications per 1-D IDCT,
 * because the y0 and y4 inputs need not be divided by sqrt(N).
 *
 * We have to do addition and subtraction of the integer inputs, which
 * is no problem, and multiplication by fractional constants, which is
 * a problem to do in integer arithmetic.  We multiply all the constants
 * by CONST_SCALE and convert them to integer constants (thus retaining
 * CONST_BITS bits of precision in the constants).  After doing a
 * multiplication we have to divide the product by CONST_SCALE, with proper
 * rounding, to produce the correct output.  This division can be done
 * cheaply as a right shift of CONST_BITS bits.  We postpone shifting
 * as long as possible so that partial sums can be added together with
 * full fractional precision.
 *
 * The outputs of the first pass are scaled up by PASS1_BITS bits so that
 * they are represented to better-than-integral precision.  These outputs
 * require BITS_IN_JSAMPLE + PASS1_BITS + 3 bits; this fits in a 16-bit word
 * with the recommended scaling.  (To scale up 12-bit sample data further, an
 * intermediate INT32 array would be needed.)
 *
 * To avoid overflow of the 32-bit intermediate results in pass 2, we must
 * have BITS_IN_JSAMPLE + CONST_BITS + PASS1_BITS <= 26.  Error analysis
 * shows that the values given below are the most effective.
 */

#if BITS_IN_JSAMPLE == 8
#define CONST_BITS  13
#define PASS1_BITS  2
#else
#define CONST_BITS  13
#define PASS1_BITS  1		/* lose a little precision to avoid overflow */
#endif

/* Some C compilers fail to reduce "FIX(constant)" at compile time, thus
 * causing a lot of useless floating-point operations at run time.
 * To get around this we use the following pre-calculated constants.
 * If you change CONST_BITS you may want to add appropriate values.
 * (With a reasonable C compiler, you can just rely on the FIX() macro...)
 */

#define DESCALE_P1       (CONST_BITS-PASS1_BITS)
#define IDESCALE_P2       (CONST_BITS+PASS1_BITS+3)
#define FDESCALE_P2       (CONST_BITS+PASS1_BITS)
#define CENTERJSAMPLE    128

#define BYTE_BIT 8
#define WORD_BIT         16   
#define F_0_081    ((short)   5329)             /* FIX(0.08131) */
#define F_0_114    ((short)   7471)             /* FIX(0.11400) */
#define F_0_168    ((short)  11059)             /* FIX(0.16874) */
#define F_0_250    ((short)  16384)             /* FIX(0.25000) */
#define F_0_299    ((short)  19595)             /* FIX(0.29900) */
#define F_0_331    ((short)  21709)             /* FIX(0.33126) */
#define F_0_418    ((short)  27439)             /* FIX(0.41869) */
#define F_0_587    ((short)  38470)             /* FIX(0.58700) */
#define F_0_337    ((short)   (F_0_587 - F_0_250))      /* FIX(0.58700) - FIX(0.25000) */

#if CONST_BITS == 13
#define FIX_0_298  ((short)  2446)	/* FIX(0.298631336) */
#define FIX_0_390  ((short)  3196)	/* FIX(0.390180644) */
#define FIX_0_541  ((short)  4433)	/* FIX(0.541196100) */
#define FIX_0_765  ((short)  6270)	/* FIX(0.765366865) */
#define FIX_0_899  ((short)  7373)	/* FIX(0.899976223) */
#define FIX_1_175  ((short)  9633)	/* FIX(1.175875602) */
#define FIX_1_501  ((short)  12299)	/* FIX(1.501321110) */
#define FIX_1_847  ((short)  15137)	/* FIX(1.847759065) */
#define FIX_1_961  ((short)  16069)	/* FIX(1.961570560) */
#define FIX_2_053  ((short)  16819)	/* FIX(2.053119869) */
#define FIX_2_562  ((short)  20995)	/* FIX(2.562915447) */
#define FIX_3_072  ((short)  25172)	/* FIX(3.072711026) */
#define FIX_0_298631336  ((INT32)  2446)        /* FIX(0.298631336) */
#define FIX_0_390180644  ((INT32)  3196)        /* FIX(0.390180644) */
#define FIX_0_541196100  ((INT32)  4433)        /* FIX(0.541196100) */
#define FIX_0_765366865  ((INT32)  6270)        /* FIX(0.765366865) */
#define FIX_0_899976223  ((INT32)  7373)        /* FIX(0.899976223) */
#define FIX_1_175875602  ((INT32)  9633)        /* FIX(1.175875602) */
#define FIX_1_501321110  ((INT32)  12299)       /* FIX(1.501321110) */
#define FIX_1_847759065  ((INT32)  15137)       /* FIX(1.847759065) */
#define FIX_1_961570560  ((INT32)  16069)       /* FIX(1.961570560) */
#define FIX_2_053119869  ((INT32)  16819)       /* FIX(2.053119869) */
#define FIX_2_562915447  ((INT32)  20995)       /* FIX(2.562915447) */
#define FIX_3_072711026  ((INT32)  25172)       /* FIX(3.072711026) */
#else
#define DECALE(x,n)  (((x)+(1<<((n)-1)))>>(n))
#define FIX_0_298  DECALE( 320652955,30-CONST_BITS)    /* FIX(0.298631336) */
#define FIX_0_390  DECALE( 418953276,30-CONST_BITS)    /* FIX(0.390180644) */
#define FIX_0_541  DECALE( 581104887,30-CONST_BITS)    /* FIX(0.541196100) */
#define FIX_0_765  DECALE( 821806413,30-CONST_BITS)    /* FIX(0.765366865) */
#define FIX_0_899  DECALE( 966342111,30-CONST_BITS)    /* FIX(0.899976223) */
#define FIX_1_175  DECALE(1262586813,30-CONST_BITS)    /* FIX(1.175875602) */
#define FIX_1_501  DECALE(1612031267,30-CONST_BITS)    /* FIX(1.501321110) */
#define FIX_1_847  DECALE(1984016188,30-CONST_BITS)    /* FIX(1.847759065) */
#define FIX_1_961  DECALE(2106220350,30-CONST_BITS)    /* FIX(1.961570560) */
#define FIX_2_053  DECALE(2204520673,30-CONST_BITS)    /* FIX(2.053119869) */
#define FIX_2_562  DECALE(2751909506,30-CONST_BITS)    /* FIX(2.562915447) */
#define FIX_3_072  DECALE(3299298341,30-CONST_BITS)    /* FIX(3.072711026) */
#define FIX_0_298631336  FIX(0.298631336)
#define FIX_0_390180644  FIX(0.390180644)
#define FIX_0_541196100  FIX(0.541196100)
#define FIX_0_765366865  FIX(0.765366865)
#define FIX_0_899976223  FIX(0.899976223)
#define FIX_1_175875602  FIX(1.175875602)
#define FIX_1_501321110  FIX(1.501321110)
#define FIX_1_847759065  FIX(1.847759065)
#define FIX_1_961570560  FIX(1.961570560)
#define FIX_2_053119869  FIX(2.053119869)
#define FIX_2_562915447  FIX(2.562915447)
#define FIX_3_072711026  FIX(3.072711026)
#endif


#define SCALEBITS	16

#define	F_0_344	((short)22554)				/* FIX(0.34414) */
#define	F_0_714 46802					/* FIX(0.71414) */
#define F_1_402	91881					/* FIX(1.40200) */
#define	F_1_772	116130					/* FIX(1.77200) */
#define	F_0_402	((short)26345)		/*FIX(1.40200) - FIX(1) */
#define	F_0_285	((short)18734)		/* FIX(1) - FIX(0.71414) */
#define F_0_228	((short)14942)		/* FIX(2) - FIX(1.77200) */



//EXTN(jconst_ycc_rgb_convert_mmi):





enum const_index{
	index_PW_F130_F054  ,
	index_PW_F054_MF130 ,
	index_PW_MF078_F117 ,
	index_PW_F117_F078  ,
	index_PW_MF060_MF089,
	index_PW_MF089_F060 ,
	index_PW_MF050_MF256,
	index_PW_MF256_F050 ,
	index_PD_DESCALE_P1 ,
	index_IPD_DESCALE_P2,
	index_FPD_DESCALE_P2,
	index_PB_CENTERJSAMP,
	index_PW_DESCALE_P2X,
	index_PW_ONE,
	index_PW_TWO,
	index_PW_THREE,
	index_PW_SEVEN,
	index_PW_EIGHT,
	index_PW_F0402,
	index_PW_MF0228,
	index_PW_MF0344_F0285,
	index_PD_ONEHALF,
	index_PW_F0299_F0337,
        index_PW_F0114_F0250,
        index_PW_MF016_MF033,
        index_PW_MF008_MF041,
        index_PD_ONEHALFM1_CJ
};

#define _uint64_set_pi8(a, b, c, d, e, f, g, h) (((uint64_t)(uint8_t)a << 56) | ((uint64_t)(uint8_t)b << 48) | ((uint64_t)(uint8_t)c << 40)|((uint64_t)(uint8_t)d << 32) | ((uint64_t)(uint8_t)e << 24) | ((uint64_t)(uint8_t)f << 16) | ((uint64_t)(uint8_t)g << 8)|((uint64_t)(uint8_t)h))
#define _uint64_set_pi16(a, b, c, d) (((uint64_t)(uint16_t)a << 48) | ((uint64_t)(uint16_t)b << 32) | ((uint64_t)(uint16_t)c << 16)|((uint64_t)(uint16_t)d))
#define _uint64_set_pi32(a, b) (((uint64_t)(uint32_t)a << 32) | ((uint64_t)(uint32_t)b))

uint64_t const_value[]={
	_uint64_set_pi16(FIX_0_541, (FIX_0_541+FIX_0_765), FIX_0_541, (FIX_0_541+FIX_0_765)),
	_uint64_set_pi16((FIX_0_541-FIX_1_847), FIX_0_541, (FIX_0_541-FIX_1_847), FIX_0_541),
	_uint64_set_pi16(FIX_1_175, (FIX_1_175-FIX_1_961), FIX_1_175, (FIX_1_175-FIX_1_961)),
	_uint64_set_pi16((FIX_1_175-FIX_0_390), FIX_1_175, (FIX_1_175-FIX_0_390), FIX_1_175),
	_uint64_set_pi16(-FIX_0_899, (FIX_0_298-FIX_0_899), -FIX_0_899, (FIX_0_298-FIX_0_899)),
	_uint64_set_pi16((FIX_1_501-FIX_0_899), -FIX_0_899, (FIX_1_501-FIX_0_899), -FIX_0_899),
	_uint64_set_pi16(-FIX_2_562, (FIX_2_053-FIX_2_562), -FIX_2_562, (FIX_2_053-FIX_2_562)),
	_uint64_set_pi16((FIX_3_072-FIX_2_562), -FIX_2_562, (FIX_3_072-FIX_2_562), -FIX_2_562),
	_uint64_set_pi32((1 << (DESCALE_P1-1)), (1 << (DESCALE_P1-1))),
	_uint64_set_pi32((1 << (IDESCALE_P2-1)), (1 << (IDESCALE_P2-1))),
	_uint64_set_pi32((1 << (FDESCALE_P2-1)), (1 << (FDESCALE_P2-1))),
	_uint64_set_pi8(CENTERJSAMPLE, CENTERJSAMPLE, CENTERJSAMPLE, CENTERJSAMPLE, CENTERJSAMPLE, CENTERJSAMPLE, CENTERJSAMPLE, CENTERJSAMPLE),
	_uint64_set_pi16((1 << (PASS1_BITS-1)), (1 << (PASS1_BITS-1)), (1 << (PASS1_BITS-1)), (1 << (PASS1_BITS-1))),
	_uint64_set_pi16(1,1,1,1),
	_uint64_set_pi16(2,2,2,2),
	_uint64_set_pi16(3,3,3,3),
	_uint64_set_pi16(7,7,7,7),
	_uint64_set_pi16(8,8,8,8),
	_uint64_set_pi16(F_0_402,F_0_402,F_0_402,F_0_402),
    	_uint64_set_pi16(-F_0_228,-F_0_228,-F_0_228,-F_0_228),
	_uint64_set_pi16(F_0_285,-F_0_344, F_0_285, -F_0_344),
	_uint64_set_pi32((int)(1 << (SCALEBITS-1)), (int)(1 << (SCALEBITS-1))),
	_uint64_set_pi16(F_0_337, F_0_299, F_0_337, F_0_299),
        _uint64_set_pi16(F_0_250, F_0_114, F_0_250, F_0_114),
        _uint64_set_pi16(-F_0_331, -F_0_168, -F_0_331, -F_0_168),
        _uint64_set_pi16(-F_0_418, -F_0_081, -F_0_418, -F_0_081),
        _uint64_set_pi32(((1 << (SCALEBITS-1)) - 1 + (CENTERJSAMPLE << SCALEBITS)), ((1 << (SCALEBITS-1)) - 1 + (CENTERJSAMPLE << SCALEBITS)))

};

#define get_const_value(index) (*(__m64*)&const_value[index])

#define PW_F130_F054    get_const_value(index_PW_F130_F054)
#define PW_F054_MF130   get_const_value(index_PW_F054_MF130)
#define PW_MF078_F117   get_const_value(index_PW_MF078_F117)
#define PW_F117_F078    get_const_value(index_PW_F117_F078)
#define PW_MF060_MF089  get_const_value(index_PW_MF060_MF089)
#define PW_MF089_F060   get_const_value(index_PW_MF089_F060)
#define PW_MF050_MF256  get_const_value(index_PW_MF050_MF256)
#define PW_MF256_F050   get_const_value(index_PW_MF256_F050)
#define PD_DESCALE_P1   get_const_value(index_PD_DESCALE_P1)
#define IPD_DESCALE_P2  get_const_value(index_IPD_DESCALE_P2)
#define FPD_DESCALE_P2  get_const_value(index_FPD_DESCALE_P2)
#define PB_CENTERJSAMP  get_const_value(index_PB_CENTERJSAMP)
#define PW_DESCALE_P2X  get_const_value(index_PW_DESCALE_P2X)
#define PW_ONE			get_const_value(index_PW_ONE)
#define PW_TWO			get_const_value(index_PW_TWO)
#define PW_THREE		get_const_value(index_PW_THREE)
#define PW_SEVEN		get_const_value(index_PW_SEVEN)
#define PW_EIGHT		get_const_value(index_PW_EIGHT)
#define PW_F0402		get_const_value(index_PW_F0402)
#define PW_MF0228		get_const_value(index_PW_MF0228)
#define PW_MF0344_F0285		get_const_value(index_PW_MF0344_F0285)
#define PD_ONEHALF		get_const_value(index_PD_ONEHALF)
#define PW_F0299_F0337  get_const_value(index_PW_F0299_F0337)
#define PW_F0114_F0250  get_const_value(index_PW_F0114_F0250)
#define PW_MF016_MF033  get_const_value(index_PW_MF016_MF033)
#define PW_MF008_MF041  get_const_value(index_PW_MF008_MF041)
#define PD_ONEHALFM1_CJ get_const_value(index_PD_ONEHALFM1_CJ)


#ifndef MMWORD
#define MMWORD long long 
#endif
#ifndef BYTE_BIT
#define BYTE_BIT 8
#endif

/* Multiply an INT32 variable by an INT32 constant to yield an INT32 result.
 *  * For 8-bit samples with the recommended scaling, all the variable
 *   * and constant values involved are no more than 16 bits wide, so a
 *    * 16x16->32 bit multiply can be used instead of a full 32x32 multiply.
 *     * For 12-bit samples, a full 32-bit multiplication will be needed.
 *      */

#if BITS_IN_JSAMPLE == 8
#define MULTIPLY(var,const)  MULTIPLY16C16(var,const)
#else
#define MULTIPLY(var,const)  ((var) * (const))
#endif


/* Dequantize a coefficient by multiplying it by the multiplier-table
 *  * entry; produce an int result.  In this module, both inputs and result
 *   * are 16 bits or less, so either int or short multiply will work.
 *    */

#define DEQUANTIZE(coef,quantval)  (((ISLOW_MULT_TYPE) (coef)) * (quantval))


/*
 * Perform dequantization and inverse DCT on one block of coefficients.
 */

//#define DEBUG_PASS1
//#define DEBUG_PASS2
//#define get_m64_value(addr) (*(__m64 *)addr)
#define test_m32_zero(mm32) (!(*(uint32_t *)&mm32))
#define test_m64_zero(mm64) (!(*(uint64_t *)&mm64))

GLOBAL(void)
jsimd_idct_islow_mmi (void * dct_table,
		JCOEFPTR coef_block,
		JSAMPARRAY output_buf, JDIMENSION output_col)
{
	__m64 tmp0,tmp1,tmp2,tmp3;
	__m64 z2, z3, z4;
	JCOEFPTR inptr;
	ISLOW_MULT_TYPE * quantptr;
	JCOEF *wsptr;
	JSAMPROW outptr; //char类型指针 
	int ctr;
	JCOEF workspace[DCTSIZE2]; /* buffers data between passes缓冲区间数据转换 */
	SHIFT_TEMPS

		/* Pass 1: process columns from input, store into work array. */
		/* Note results are scaled up by sqrt(8) compared to a true IDCT; */
		/* furthermore, we scale the results by 2**PASS1_BITS. */
	inptr = coef_block;
	quantptr = (ISLOW_MULT_TYPE *) dct_table;
	wsptr = workspace;
	//DCTSIZE == 8
	int loopsize = 4; //TODO: 4*sizeof(inptr[0]) = sizeof(__m64)
	int loopcount = DCTSIZE / loopsize; //=2

#ifdef DEBUG_PASS1
    	printf("***********************1st PASS========================\n");
#endif

	for (ctr = loopcount; ctr > 0; ctr--) {
		/* Due to quantization, we will usually find that many of the input
		 * coefficients are zero, especially the AC terms.  We can exploit this
		 * by short-circuiting the IDCT calculation for any column in which all
		 * the AC terms are zero.  In that case each output is equal to the
		 * DC coefficient (with scale factor as needed).
		 * With typical images and quantization tables, half or more of the
		 * column DCT calculations can be simplified this way.
		 */
		
		__m32 inptra = _mm_load_si32((__m32 *)&inptr[DCTSIZE*1]);
		__m32 inptrb = _mm_load_si32((__m32 *)&inptr[DCTSIZE*2]);
		__m32 mm0 = _mm_or_si32(inptra, inptrb);
			
		if (test_m32_zero(mm0))	{
			
			__m64 inptr0 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*0]);
			__m64 inptr1 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*1]);
			__m64 inptr2 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*2]);
			__m64 inptr3 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*3]);
			__m64 inptr4 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*4]);
			__m64 inptr5 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*5]);
			__m64 inptr6 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*6]);
			__m64 inptr7 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*7]);
			
			__m64 mm1 = _mm_or_si64(inptr1, inptr3);
			__m64 mm2 = _mm_or_si64(inptr2, inptr4);
			mm1 = _mm_or_si64(mm1,inptr5 );
			mm2 = _mm_or_si64(mm2,inptr6);
			mm1 = _mm_or_si64(mm1,inptr7);
			mm1 = _mm_or_si64(mm1, mm2);
			
#ifdef DEBUG_PASS1
    			printf("input:%d\n", ctr);
    			//printf("0x%16llx\n", to_uint64(inptr0));
    			printf("0x%16llx\n", to_uint64(inptr1));
    			printf("0x%16llx\n", to_uint64(inptr2));
    			printf("0x%16llx\n", to_uint64(inptr3));
    			printf("0x%16llx\n", to_uint64(inptr4));
    			printf("0x%16llx\n", to_uint64(inptr5));
    			printf("0x%16llx\n", to_uint64(inptr6));
    			printf("0x%16llx\n", to_uint64(inptr7));
#endif

			if (test_m64_zero(mm1)) {
			/* AC terms all zero */
			__m64 quantptr0 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*0]);
			
			__m64 dcval = _mm_mullo_pi16(inptr0, quantptr0);
			dcval =  _mm_slli_pi16(dcval, (int64_t)PASS1_BITS);	//dcval=in0=(00 01 02 03)
			
			__m64 tmplo = _mm_unpacklo_pi16(dcval, dcval);		//dcvalL=in0=(00 00 01 01)
			__m64 tmphi = _mm_unpackhi_pi16(dcval, dcval);		//dcvalL=in0=(02 02 03 03)

			__m64 dcval1 = _mm_unpacklo_pi32(tmplo, tmplo);	//dcvalL0=in0=(00 00 00 00)
			__m64 dcval2 = _mm_unpackhi_pi32(tmplo, tmplo);	//dcvalL1=in0=(01 01 01 01)
			__m64 dcval3 = _mm_unpacklo_pi32(tmphi, tmphi);	//dcvalH0=in0=(02 02 02 02)
			__m64 dcval4 = _mm_unpackhi_pi32(tmphi, tmphi);	//dcvalH1=in0=(03 03 03 03)
			
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*0], dcval1);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*0 + loopsize], dcval1);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*1],dcval2);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*1 + loopsize], dcval2);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*2],dcval3);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*2 + loopsize], dcval3);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*3],dcval4);
			_mm_store_si64((__m64 *)&wsptr[DCTSIZE*3 + loopsize], dcval4);

#ifdef DEBUG_PASS1
    			printf("wsptr1:%d\n", ctr);
    			printf("0x%16llx 0x%16llx\n", to_uint64(dcval1), to_uint64(dcval1));
    			printf("0x%16llx 0x%16llx\n", to_uint64(dcval2), to_uint64(dcval2));
    			printf("0x%16llx 0x%16llx\n", to_uint64(dcval3), to_uint64(dcval3));
    			printf("0x%16llx 0x%16llx\n", to_uint64(dcval4), to_uint64(dcval4));
#endif
			
			inptr += loopsize;			/* advance pointers to next column */
			quantptr += loopsize;
			wsptr += DCTSIZE*loopsize;
			continue;
		}
		}
		/* Even part: reverse the even part of the forward DCT. */
		/* The rotator is sqrt(2)*c(-6). */

		/*(Original)
		 * z1 = (z2 + z3) * 0.541196100;
		 * tmp2 = z1 + z3 * -1.847759065;
		 * tmp3 = z1 + z2 * 0.765366865;
		 *
		 * (This implementation)
		 * tmp2 = z2 * 0.541196100 + z3 * (0.541196100 - 1.847759065);
		 * tmp3 = z2 * (0.541196100 + 0.765366865) + z3 * 0.541196100;
		 */

		__m64 inptr0 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*0]);
		__m64 inptr2 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*2]);
		__m64 inptr4 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*4]);
		__m64 inptr6 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*6]);

		__m64 quantptr0 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*0]);
		__m64 quantptr2 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*2]);
		__m64 quantptr4 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*4]);
		__m64 quantptr6 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*6]);

		__m64 z2 = _mm_mullo_pi16(inptr2, quantptr2);
		__m64 z3 = _mm_mullo_pi16(inptr6, quantptr6);
		
		__m64 tmplo = _mm_unpacklo_pi16(z2, z3);
		__m64 tmphi = _mm_unpackhi_pi16(z2, z3);
		__m64 tmp3L = _mm_madd_pi16(tmplo, PW_F130_F054);	//get the tmp3L
		__m64 tmp3H = _mm_madd_pi16(tmphi, PW_F130_F054);	//get the tmp3H
		__m64 tmp2L = _mm_madd_pi16(tmplo, PW_F054_MF130);	//get the tmp2L
		__m64 tmp2H = _mm_madd_pi16(tmphi, PW_F054_MF130);	//get the tmp2H

		z2 = _mm_mullo_pi16(inptr0, quantptr0);
		z3 = _mm_mullo_pi16(inptr4, quantptr4);

		__m64 z23a = _mm_add_pi16(z2, z3);
		__m64 z23s = _mm_sub_pi16(z2, z3);

		__m64 tmp0L = _mm_loadlo_pi16_f(z23a);	
		__m64 tmp0H = _mm_loadhi_pi16_f(z23a);	

		tmp0L = _mm_srai_pi32(tmp0L, (16-CONST_BITS));	//get the tmp0L
		tmp0H = _mm_srai_pi32(tmp0H, (16-CONST_BITS));	//get the tmp0H

		__m64 tmp10L = _mm_add_pi32(tmp0L,tmp3L);	//tmp10L
		__m64 tmp13L = _mm_sub_pi32(tmp0L,tmp3L);	//tmp13L

		__m64 tmp10H = _mm_add_pi32(tmp0H,tmp3H);	//tmp10H
		__m64 tmp13H = _mm_sub_pi32(tmp0H,tmp3H);	//tmp13H

		__m64 tmp1L = _mm_loadlo_pi16_f(z23s);
		__m64 tmp1H = _mm_loadhi_pi16_f(z23s);

		tmp1L = _mm_srai_pi32(tmp1L, (16-CONST_BITS));	//tmp1L
		tmp1H = _mm_srai_pi32(tmp1H, (16-CONST_BITS));	//tmp2H

		__m64 tmp11L = _mm_add_pi32(tmp1L,tmp2L);	//tmp11L
		__m64 tmp12L = _mm_sub_pi32(tmp1L,tmp2L);	//tmp12L

		__m64 tmp11H = _mm_add_pi32(tmp1H,tmp2H);	//tmp11H
		__m64 tmp12H = _mm_sub_pi32(tmp1H,tmp2H);	//tmp12H

		//TODO: store tmp11L and so on

		/* Odd part per figure 8; the matrix is unitary and hence its
		 * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
		 */	

		__m64 inptr1 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*1]);
		__m64 inptr3 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*3]);
		__m64 inptr5 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*5]);
		__m64 inptr7 = _mm_load_si64((__m64 *)&inptr[DCTSIZE*7]);

		__m64 quantptr1 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*1]);
		__m64 quantptr3 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*3]);
		__m64 quantptr5 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*5]);
		__m64 quantptr7 = _mm_load_si64((__m64 *)&quantptr[DCTSIZE*7]);

		tmp0 = _mm_mullo_pi16(inptr7, quantptr7);
		tmp1 = _mm_mullo_pi16(inptr5, quantptr5);
		tmp2 = _mm_mullo_pi16(inptr3, quantptr3);
		tmp3 = _mm_mullo_pi16(inptr1, quantptr1);

		z3 = _mm_add_pi16(tmp0,tmp2);
		z4 = _mm_add_pi16(tmp1,tmp3);
		/*
		 * (Original)
		 * z5 = (z3 + z4) * 1.175875602;
		 * z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
		 * z3 += z5;  z4 += z5;
		 *
		 * (This implementation)
		 * z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
		 * z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);
		 */	
		tmplo = _mm_unpacklo_pi16(z3, z4);
		tmphi = _mm_unpackhi_pi16(z3, z4);
		__m64 z3L = _mm_madd_pi16(tmplo, PW_MF078_F117);	//get the z3L
		__m64 z3H = _mm_madd_pi16(tmphi, PW_MF078_F117);	//get the z3H
		__m64 z4L = _mm_madd_pi16(tmplo, PW_F117_F078);	//get the z4L
		__m64 z4H = _mm_madd_pi16(tmphi, PW_F117_F078);	//get the z4H

		//TODO: store z3L and z3H
		
		/*
		 * (Original)
		 * z1 = tmp0 + tmp3;  z2 = tmp1 + tmp2;
		 * tmp0 = tmp0 * 0.298631336;  tmp1 = tmp1 * 2.053119869;
		 * tmp2 = tmp2 * 3.072711026;  tmp3 = tmp3 * 1.501321110;
		 * z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
		 * tmp0 += z1 + z3;  tmp1 += z2 + z4;
		 * tmp2 += z2 + z3;  tmp3 += z1 + z4;
		 *
		 * (This implementation)
		 * tmp0 = tmp0 * (0.298631336 - 0.899976223) + tmp3 * -0.899976223;
		 * tmp1 = tmp1 * (2.053119869 - 2.562915447) + tmp2 * -2.562915447;
		 * tmp2 = tmp1 * -2.562915447 + tmp2 * (3.072711026 - 2.562915447);
		 * tmp3 = tmp0 * -0.899976223 + tmp3 * (1.501321110 - 0.899976223);
		 * tmp0 += z3;  tmp1 += z4;
		 * tmp2 += z3;  tmp3 += z4;
		 */
		tmplo = _mm_unpacklo_pi16(tmp0,tmp3);
		tmphi = _mm_unpackhi_pi16(tmp0,tmp3);

		tmp0L = _mm_madd_pi16(tmplo, PW_MF060_MF089);
		tmp0H = _mm_madd_pi16(tmphi, PW_MF060_MF089);
		tmp3L = _mm_madd_pi16(tmplo, PW_MF089_F060);
		tmp3H = _mm_madd_pi16(tmphi, PW_MF089_F060);   	

		tmp0L = _mm_add_pi32(tmp0L, z3L);	//tmp0L
		tmp0H = _mm_add_pi32(tmp0H, z3H); 	//tmp0H
		tmp3L = _mm_add_pi32(tmp3L, z4L);	//tmp3L
		tmp3H = _mm_add_pi32(tmp3H, z4H);	//tmp3H
		//TODO: store tmp0L and so on

		tmplo = _mm_unpacklo_pi16(tmp1,tmp2);
		tmphi = _mm_unpackhi_pi16(tmp1,tmp2);

		tmp1L = _mm_madd_pi16(tmplo, PW_MF050_MF256);
		tmp1H = _mm_madd_pi16(tmphi, PW_MF050_MF256);
		tmp2L = _mm_madd_pi16(tmplo, PW_MF256_F050);
		tmp2H = _mm_madd_pi16(tmphi, PW_MF256_F050);

		tmp1L = _mm_add_pi32(tmp1L, z4L);	//tmp1L
		tmp1H = _mm_add_pi32(tmp1H, z4H);	//tmp1H
		tmp2L = _mm_add_pi32(tmp2L, z3L);	//tmp2L
		tmp2H = _mm_add_pi32(tmp2H, z3H);	//tmp2H
		//TODO: store tmp1L and so on

		/* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

		__m64 data0L = _mm_add_pi32(tmp10L,tmp3L);  //data0L
		__m64 data0H = _mm_add_pi32(tmp10H,tmp3H);  //data0H
		__m64 data7L = _mm_sub_pi32(tmp10L,tmp3L);  //data7L
		__m64 data7H = _mm_sub_pi32(tmp10H,tmp3H);  //data7H

		data0L = _mm_add_pi32(data0L, PD_DESCALE_P1);
		data0H = _mm_add_pi32(data0H, PD_DESCALE_P1);	
		data0L = _mm_srai_pi32(data0L, DESCALE_P1);
		data0H = _mm_srai_pi32(data0H, DESCALE_P1);

		data7L = _mm_add_pi32(data7L, PD_DESCALE_P1);
		data7H = _mm_add_pi32(data7H, PD_DESCALE_P1);
		data7L = _mm_srai_pi32(data7L, DESCALE_P1);
		data7H = _mm_srai_pi32(data7H, DESCALE_P1);

		//merge
		__m64 data0 = _mm_packs_pi32(data0L,data0H);  //data0 =(00 01 02 03)
		__m64 data7 = _mm_packs_pi32(data7L,data7H);  //data7 =(70 71 72 73)

		__m64 data1L = _mm_add_pi32(tmp11L,tmp2L);  //data1L
		__m64 data1H = _mm_add_pi32(tmp11H,tmp2H);  //data1H
		__m64 data6L = _mm_sub_pi32(tmp11L,tmp2L);  //data6L
		__m64 data6H = _mm_sub_pi32(tmp11H,tmp2H);  //data6H

		data1L = _mm_add_pi32(data1L, PD_DESCALE_P1);
		data1H = _mm_add_pi32(data1H, PD_DESCALE_P1);
		data1L = _mm_srai_pi32(data1L, DESCALE_P1);
		data1H = _mm_srai_pi32(data1H, DESCALE_P1);

		data6L = _mm_add_pi32(data6L, PD_DESCALE_P1);
		data6H = _mm_add_pi32(data6H, PD_DESCALE_P1);
		data6L = _mm_srai_pi32(data6L, DESCALE_P1);
		data6H = _mm_srai_pi32(data6H, DESCALE_P1);

		__m64 data1 = _mm_packs_pi32(data1L,data1H);  //data1 =(10 11 12 13)
		__m64 data6 = _mm_packs_pi32(data6L,data6H);  //data6 =(60 61 62 63)

		__m64 data01L = _mm_unpacklo_pi16(data0,data1); //data01L = (00 10 01 11)
		__m64 data01H = _mm_unpackhi_pi16(data0,data1); //data01H = (02 12 03 13)
		__m64 data67L = _mm_unpacklo_pi16(data6,data7); //data67L = (60 70 61 71)
		__m64 data67H = _mm_unpackhi_pi16(data6,data7); //data67H = (62 72 63 73)

		__m64 data2L = _mm_add_pi32(tmp12L,tmp1L);  //data2L
		__m64 data2H = _mm_add_pi32(tmp12H,tmp1H);  //data2H
		__m64 data5L = _mm_sub_pi32(tmp12L,tmp1L);  //data5L
		__m64 data5H = _mm_sub_pi32(tmp12H,tmp1H);  //data5H

		data2L = _mm_add_pi32(data2L, PD_DESCALE_P1);
		data2H = _mm_add_pi32(data2H, PD_DESCALE_P1);
		data2L = _mm_srai_pi32(data2L, DESCALE_P1);
		data2H = _mm_srai_pi32(data2H, DESCALE_P1);

		data5L = _mm_add_pi32(data5L, PD_DESCALE_P1);
		data5H = _mm_add_pi32(data5H, PD_DESCALE_P1);
		data5L = _mm_srai_pi32(data5L, DESCALE_P1);
		data5H = _mm_srai_pi32(data5H, DESCALE_P1);

		__m64 data2 = _mm_packs_pi32(data2L,data2H);  //data2 =(20 21 22 23)
		__m64 data5 = _mm_packs_pi32(data5L,data5H);  //data5 =(50 51 52 53)

		__m64 data3L = _mm_add_pi32(tmp13L,tmp0L);  //data3L
		__m64 data3H = _mm_add_pi32(tmp13H,tmp0H);  //data3H

		__m64 data4L = _mm_sub_pi32(tmp13L,tmp0L);  //data4L
		__m64 data4H = _mm_sub_pi32(tmp13H,tmp0H);  //data4H

		data3L = _mm_add_pi32(data3L, PD_DESCALE_P1);
		data3H = _mm_add_pi32(data3H, PD_DESCALE_P1);
		data3L = _mm_srai_pi32(data3L, DESCALE_P1);
		data3H = _mm_srai_pi32(data3H, DESCALE_P1);

		data4L = _mm_add_pi32(data4L, PD_DESCALE_P1);
		data4H = _mm_add_pi32(data4H, PD_DESCALE_P1);
		data4L = _mm_srai_pi32(data4L, DESCALE_P1);
		data4H = _mm_srai_pi32(data4H, DESCALE_P1);

		__m64 data3 = _mm_packs_pi32(data3L,data3H);  //data3 =(30 31 32 33)
		__m64 data4 = _mm_packs_pi32(data4L,data4H);  //data4 =(40 41 42 43)

		__m64 data23L = _mm_unpacklo_pi16(data2,data3); //data23L = (20 30 21 31)
		__m64 data23H = _mm_unpackhi_pi16(data2,data3); //data23H = (22 32 23 33)
		__m64 data45L = _mm_unpacklo_pi16(data4,data5); //data45L = (40 50 41 51)
		__m64 data45H = _mm_unpackhi_pi16(data4,data5); //data45H = (42 52 43 53)

		//data01L = (00 10 01 11) punpcklwd data23L = (20 30 21 31)
		//data01L = (00 10 01 11) punpckhwd data23L = (20 30 21 31)
		__m64 data0123LL = _mm_unpacklo_pi32(data01L,data23L); //(00 10 20 30)
		__m64 data0123LH = _mm_unpackhi_pi32(data01L,data23L); //(01 11 21 31)
		//data01H = (02 12 03 13) punpcklwd data23H = (22 32 23 33)
		//data01H = (02 12 03 13) punpckhwd data23H = (22 32 23 33)
		__m64 data0123HL = _mm_unpacklo_pi32(data01H,data23H); //(02 12 22 32)
		__m64 data0123HH = _mm_unpackhi_pi32(data01H,data23H); //(03 13 23 33)

		//data45L = (40 50 41 51) punpcklwd data67L = (60 70 61 71)
		//data45L = (40 50 41 51) punpckhwd data67L = (60 70 61 71)
		__m64 data4567LL = _mm_unpacklo_pi32(data45L,data67L); //(40 50 60 70)
		__m64 data4567LH = _mm_unpackhi_pi32(data45L,data67L); //(41 51 61 71)
		//data45H = (42 52 43 53) punpcklwd data67H = (62 72 63 73)
		//data45H = (42 52 43 53) punpckhwd data67H = (62 72 63 73) 
		__m64 data4567HL = _mm_unpacklo_pi32(data45H,data67H); //(42 52 62 72)
		__m64 data4567HH = _mm_unpackhi_pi32(data45H,data67H); //(43 53 63 73)
		
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*0], data0123LL);		  //wsptr[DCTSIZE*0+0/1/2/3]=(00 10 20 30)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*0 + loopsize], data4567LL); //wsptr[DCTSIZE*1+0/1/2/3]=(40 50 60 70)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*1], data0123LH);		  //wsptr[DCTSIZE*2+0/1/2/3]=(01 11 21 31)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*1 + loopsize], data4567LH); //wsptr[DCTSIZE*3+0/1/2/3]=(41 51 61 71)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*2], data0123HL);		  //wsptr[DCTSIZE*4+0/1/2/3]=(02 12 22 32)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*2 + loopsize], data4567HL); //wsptr[DCTSIZE*5+0/1/2/3]=(42 52 62 72)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*3], data0123HH);		  //wsptr[DCTSIZE*6+0/1/2/3]=(03 13 23 33)	
		_mm_store_si64((__m64*)&wsptr[DCTSIZE*3 + loopsize], data4567HH); //wsptr[DCTSIZE*7+0/1/2/3]=(43 53 63 73)	

#ifdef DEBUG_PASS1
    		printf("wsptr2:%d\n", ctr);
    		printf("0x%16llx 0x%16llx\n", to_uint64(data0123LL), to_uint64(data4567LL));
    		printf("0x%16llx 0x%16llx\n", to_uint64(data0123LH), to_uint64(data4567LH));
    		printf("0x%16llx 0x%16llx\n", to_uint64(data0123HL), to_uint64(data4567HL));
    		printf("0x%16llx 0x%16llx\n\n", to_uint64(data0123HH), to_uint64(data4567HH));
#endif
		
		inptr += loopsize;			/* advance pointers to next column */
		quantptr += loopsize;
		wsptr += DCTSIZE*loopsize;
	}

#if defined(DEBUG_PASS1) && !defined(DEBUG_PASS2)
	while(1);
#endif

	/* Pass 2: process rows from work array, store into output array. */
	/* Note that we must descale the results by a factor of 8 == 2**3, */
	/* and also undo the PASS1_BITS scaling. */

#ifdef DEBUG_PASS2
    	printf("***********************2ed PASS========================\n");
#endif

	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr+=loopsize) {
		outptr = output_buf[ctr] + output_col;	

		__m64 wsptr0 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*0]); //???
		__m64 wsptr1 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*1]);
		__m64 wsptr2 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*2]);
		__m64 wsptr3 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*3]);
		__m64 wsptr4 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*4]);
		__m64 wsptr5 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*5]);
		__m64 wsptr6 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*6]);
		__m64 wsptr7 = _mm_load_si64((__m64 *)&wsptr[DCTSIZE*7]);

#ifdef DEBUG_PASS2
    		printf("wsptr:%d\n", ctr);
    		printf("0x%16llx\n", to_uint64(wsptr0));
    		printf("0x%16llx\n", to_uint64(wsptr1));
    		printf("0x%16llx\n", to_uint64(wsptr2));
    		printf("0x%16llx\n", to_uint64(wsptr3));
    		printf("0x%16llx\n", to_uint64(wsptr4));
    		printf("0x%16llx\n", to_uint64(wsptr5));
    		printf("0x%16llx\n", to_uint64(wsptr6));
    		printf("0x%16llx\n", to_uint64(wsptr7));
#endif

		//TODO: zero test
		if(0)
		{
		}

		/* Even part: reverse the even part of the forward DCT. */
		/* The rotator is sqrt(2)*c(-6). 
		 * (Original)
		 * z1 = (z2 + z3) * 0.541196100;
		 * tmp2 = z1 + z3 * -1.847759065;
		 * tmp3 = z1 + z2 * 0.765366865;
		 *
		 * (This implementation)
		 * tmp2 = z2 * 0.541196100 + z3 * (0.541196100 - 1.847759065);
		 * tmp3 = z2 * (0.541196100 + 0.765366865) + z3 * 0.541196100;
		 */

		z2 = wsptr2;
		z3 = wsptr6;

		__m64 tmplo = _mm_unpacklo_pi16(z2,z3);
		__m64 tmphi = _mm_unpackhi_pi16(z2,z3);

		__m64 tmp3L = _mm_madd_pi16(tmplo, PW_F130_F054);	//tmp3L
		__m64 tmp3H = _mm_madd_pi16(tmphi, PW_F130_F054);	//tmp3H
		__m64 tmp2L = _mm_madd_pi16(tmplo, PW_F054_MF130);	//tmp2L
		__m64 tmp2H = _mm_madd_pi16(tmphi, PW_F054_MF130);	//tmp2H
		
		z2 = wsptr0;
		z3 = wsptr4;
		
		__m64 z23a = _mm_add_pi16(z2,z3);
		__m64 z23s = _mm_sub_pi16(z2,z3);

		__m64 tmp0L = _mm_loadlo_pi16_f(z23a);
		__m64 tmp0H = _mm_loadhi_pi16_f(z23a);

		tmp0L = _mm_srai_pi32(tmp0L,(16-CONST_BITS)); //tmp0L
		tmp0H = _mm_srai_pi32(tmp0H,(16-CONST_BITS)); //tmp0H

		__m64 tmp10L = _mm_add_pi32(tmp0L,tmp3L);
		__m64 tmp13L = _mm_sub_pi32(tmp0L,tmp3L);

		__m64 tmp10H = _mm_add_pi32(tmp0H,tmp3H);
		__m64 tmp13H = _mm_sub_pi32(tmp0H,tmp3H);

		__m64 tmp1L = _mm_loadlo_pi16_f(z23s);
		__m64 tmp1H = _mm_loadhi_pi16_f(z23s);

		tmp1L = _mm_srai_pi32(tmp1L,(16-CONST_BITS)); //tmp1L
		tmp1H = _mm_srai_pi32(tmp1H,(16-CONST_BITS)); //tmp1H

		__m64 tmp11L = _mm_add_pi32(tmp1L,tmp2L);
		__m64 tmp12L = _mm_sub_pi32(tmp1L,tmp2L);

		__m64 tmp11H = _mm_add_pi32(tmp1H,tmp2H);
		__m64 tmp12H = _mm_sub_pi32(tmp1H,tmp2H);

		/* Odd part per figure 8; the matrix is unitary and hence its
		 * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
		 */
		/*   ; (Original)
		 * z5 = (z3 + z4) * 1.175875602;
		 * z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
		 * z3 += z5;  z4 += z5;
		 *
		 * (This implementation)
		 * z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
		 *z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);
		 */
		
		tmp0 = wsptr7;
		tmp1 = wsptr5;
		tmp2 = wsptr3;
		tmp3 = wsptr1;

		z3 = _mm_add_pi16(tmp0,tmp2);	//z3 = tmp0 + tmp2
		z4 = _mm_add_pi16(tmp1,tmp3);	//z4 = tmp1 + tmp3

		tmplo = _mm_unpacklo_pi16(z3,z4);
		tmphi = _mm_unpackhi_pi16(z3,z4);

		__m64 z3L = _mm_madd_pi16(tmplo, PW_MF078_F117);	//z3L
		__m64 z3H = _mm_madd_pi16(tmphi, PW_MF078_F117);	//z3H
		__m64 z4L = _mm_madd_pi16(tmplo, PW_F117_F078);	//z4L
		__m64 z4H = _mm_madd_pi16(tmphi, PW_F117_F078);	//z4H

		/*
		 * (Original)
		 * z1 = tmp0 + tmp3;  z2 = tmp1 + tmp2;
		 * tmp0 = tmp0 * 0.298631336;  tmp1 = tmp1 * 2.053119869;
		 * tmp2 = tmp2 * 3.072711026;  tmp3 = tmp3 * 1.501321110;
		 * z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
		 * tmp0 += z1 + z3;  tmp1 += z2 + z4;
		 * tmp2 += z2 + z3;  tmp3 += z1 + z4;
		 *
		 * (This implementation)
		 * tmp0 = tmp0 * (0.298631336 - 0.899976223) + tmp3 * -0.899976223;
		 * tmp1 = tmp1 * (2.053119869 - 2.562915447) + tmp2 * -2.562915447;
		 * tmp2 = tmp1 * -2.562915447 + tmp2 * (3.072711026 - 2.562915447);
		 * tmp3 = tmp0 * -0.899976223 + tmp3 * (1.501321110 - 0.899976223);
		 * tmp0 += z3;  tmp1 += z4;
		 * tmp2 += z3;  tmp3 += z4;
		 */
		tmplo = _mm_unpacklo_pi16(tmp0,tmp3);
		tmphi = _mm_unpackhi_pi16(tmp0,tmp3);

		tmp0L = _mm_madd_pi16(tmplo, PW_MF060_MF089);
		tmp0H = _mm_madd_pi16(tmphi, PW_MF060_MF089);
		tmp3L = _mm_madd_pi16(tmplo, PW_MF089_F060);
		tmp3H = _mm_madd_pi16(tmphi, PW_MF089_F060);

		tmp0L = _mm_add_pi32(tmp0L, z3L);	//tmp0L
		tmp0H = _mm_add_pi32(tmp0H, z3H);	//tmp0H
		tmp3L = _mm_add_pi32(tmp3L, z4L);	//tmp3L
		tmp3H = _mm_add_pi32(tmp3H, z4H);	//tmp3H

		tmplo = _mm_unpacklo_pi16(tmp1,tmp2);
		tmphi = _mm_unpackhi_pi16(tmp1,tmp2);

		tmp1L = _mm_madd_pi16(tmplo, PW_MF050_MF256);
		tmp1H = _mm_madd_pi16(tmphi, PW_MF050_MF256);
		tmp2L = _mm_madd_pi16(tmplo, PW_MF256_F050);
		tmp2H = _mm_madd_pi16(tmphi, PW_MF256_F050);

		tmp1L = _mm_add_pi32(tmp1L, z4L);	//tmp1L
		tmp1H = _mm_add_pi32(tmp1H, z4H);	//tmp1H
		tmp2L = _mm_add_pi32(tmp2L, z3L);	//tmp2L
		tmp2H = _mm_add_pi32(tmp2H, z3H);	//tmp2H

		/* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

		__m64 data0L = _mm_add_pi32(tmp10L,tmp3L);  //data0L
		__m64 data0H = _mm_add_pi32(tmp10H,tmp3H);  //data0H
		__m64 data7L = _mm_sub_pi32(tmp10L,tmp3L);  //data7L
		__m64 data7H = _mm_sub_pi32(tmp10H,tmp3H);  //data7H

		data0L = _mm_add_pi32(data0L, IPD_DESCALE_P2);
		data0H = _mm_add_pi32(data0H, IPD_DESCALE_P2);
		data0L = _mm_srai_pi32(data0L, IDESCALE_P2);
		data0H = _mm_srai_pi32(data0H, IDESCALE_P2);

		data7L = _mm_add_pi32(data7L, IPD_DESCALE_P2);
		data7H = _mm_add_pi32(data7H, IPD_DESCALE_P2);
		data7L = _mm_srai_pi32(data7L, IDESCALE_P2);
		data7H = _mm_srai_pi32(data7H, IDESCALE_P2);

		//merge
		__m64 data0 = _mm_packs_pi32(data0L,data0H);  //data0 =(00 10 20 30)
		__m64 data7 = _mm_packs_pi32(data7L,data7H);  //data7 =(07 17 27 37)

		__m64 data1L = _mm_add_pi32(tmp11L,tmp2L);  //data1L
		__m64 data1H = _mm_add_pi32(tmp11H,tmp2H);  //data1H
		__m64 data6L = _mm_sub_pi32(tmp11L,tmp2L);  //data6L
		__m64 data6H = _mm_sub_pi32(tmp11H,tmp2H);  //data6H

		data1L = _mm_add_pi32(data1L, IPD_DESCALE_P2);
		data1H = _mm_add_pi32(data1H, IPD_DESCALE_P2);
		data1L = _mm_srai_pi32(data1L, IDESCALE_P2);
		data1H = _mm_srai_pi32(data1H, IDESCALE_P2);

		data6L = _mm_add_pi32(data6L, IPD_DESCALE_P2);
		data6H = _mm_add_pi32(data6H, IPD_DESCALE_P2);
		data6L = _mm_srai_pi32(data6L, IDESCALE_P2);
		data6H = _mm_srai_pi32(data6H, IDESCALE_P2);

		__m64 data1 = _mm_packs_pi32(data1L,data1H);  //data1 =(01 11 21 31)
		__m64 data6 = _mm_packs_pi32(data6L,data6H);  //data6 =(06 16 26 36)

		__m64 data06 = _mm_packs_pi16(data0,data6);	//data06 = (00 10 20 30 06 16 26 36)
		__m64 data17 = _mm_packs_pi16(data1,data7);	//data17 = (01 11 21 31 07 17 27 37)

		__m64 data2L = _mm_add_pi32(tmp12L,tmp1L);  //data2L
		__m64 data2H = _mm_add_pi32(tmp12H,tmp1H);  //data2H
		__m64 data5L = _mm_sub_pi32(tmp12L,tmp1L);  //data5L
		__m64 data5H = _mm_sub_pi32(tmp12H,tmp1H);  //data5H

		data2L = _mm_add_pi32(data2L, IPD_DESCALE_P2); 
		data2H = _mm_add_pi32(data2H, IPD_DESCALE_P2);
		data2L = _mm_srai_pi32(data2L, IDESCALE_P2);
		data2H = _mm_srai_pi32(data2H, IDESCALE_P2);

		data5L = _mm_add_pi32(data5L, IPD_DESCALE_P2); 
		data5H = _mm_add_pi32(data5H, IPD_DESCALE_P2);
		data5L = _mm_srai_pi32(data5L, IDESCALE_P2);
		data5H = _mm_srai_pi32(data5H, IDESCALE_P2);

		__m64 data2 = _mm_packs_pi32(data2L,data2H);  //data2 =(02 12 22 32)
		__m64 data5 = _mm_packs_pi32(data5L,data5H);  //data5 =(05 15 25 35)

		__m64 data3L = _mm_add_pi32(tmp13L,tmp0L);  //data3L
		__m64 data3H = _mm_add_pi32(tmp13H,tmp0H);  //data3H
		__m64 data4L = _mm_sub_pi32(tmp13L,tmp0L);  //data4L
		__m64 data4H = _mm_sub_pi32(tmp13H,tmp0H);  //data4H

		data3L = _mm_add_pi32(data3L, IPD_DESCALE_P2);
		data3H = _mm_add_pi32(data3H, IPD_DESCALE_P2);
		data3L = _mm_srai_pi32(data3L, IDESCALE_P2);
		data3H = _mm_srai_pi32(data3H, IDESCALE_P2);

		data4L = _mm_add_pi32(data4L, IPD_DESCALE_P2);
		data4H = _mm_add_pi32(data4H, IPD_DESCALE_P2);
		data4L = _mm_srai_pi32(data4L, IDESCALE_P2);
		data4H = _mm_srai_pi32(data4H, IDESCALE_P2);

		__m64 data3 = _mm_packs_pi32(data3L,data3H);  //data3 =(03 13 23 33)
		__m64 data4 = _mm_packs_pi32(data4L,data4H);  //data4 =(04 14 24 34)

		__m64 data24 = _mm_packs_pi16(data2,data4);		//data24 = (02 12 22 32 04 14 24 34)
		__m64 data35 = _mm_packs_pi16(data3,data5); 	//data35 = (03 13 23 33 05 15 25 35)

		//	CENTERJSAMPLE = 128 = [0000 0000 0000 0080] --> [0080] = [0000 0000 1000 0000]
		//	                  ==> [0000 0000 0000 8080] --> [8080] = [1000 0000 1000 0000]
		//			  ==> [1000 1000 1000 1000]
		//	data06 = (00 10 20 30 06 16 26 36)	
		//	data17 = (01 11 21 31 07 17 27 37)
		data06 = _mm_add_pi8(data06, PB_CENTERJSAMP);
		data17 = _mm_add_pi8(data17, PB_CENTERJSAMP); 	
		data24 = _mm_add_pi8(data24, PB_CENTERJSAMP);
		data35 = _mm_add_pi8(data35, PB_CENTERJSAMP);
		/* transpose coefficients(phase 1) */	
		__m64 dataAL = _mm_unpacklo_pi8(data06,data17);		//data0617L = (00 01 10 11 20 21 30 31)
		__m64 dataAH = _mm_unpackhi_pi8(data06,data17); 	//data0617H = (06 07 16 17 26 27 36 37)
		__m64 dataBL = _mm_unpacklo_pi8(data24,data35); 	//data2435L = (02 03 12 13 22 23 32 33)
		__m64 dataBH = _mm_unpackhi_pi8(data24,data35);		//data2435H = (04 05 14 15 24 25 34 35)
		/* transpose coefficients(phase 2) */
		__m64 dataLL = _mm_unpacklo_pi16(dataAL,dataBL);	//dataLL = (00 01 02 03 10 11 12 13)
		__m64 dataLH = _mm_unpackhi_pi16(dataAL,dataBL);  	//dataLH = (20 21 22 23 30 31 32 33)
		__m64 dataHL = _mm_unpacklo_pi16(dataBH,dataAH);  	//dataHL = (04 05 06 07 14 15 16 17)
		__m64 dataHH = _mm_unpackhi_pi16(dataBH,dataAH);  	//dataHH = (24 25 26 27 34 35 36 37)
		/* transpose coefficients(phase 3) */
		__m64 dataHLL = _mm_unpacklo_pi32(dataLL,dataHL);	//dataHLL = (00 01 02 03 04 05 06 07)
		__m64 dataHLH = _mm_unpackhi_pi32(dataLL,dataHL);	//dataHLH = (10 11 12 13 14 15 16 17)
		__m64 dataHHL = _mm_unpacklo_pi32(dataLH,dataHH);	//dataHHL = (20 21 22 23 24 25 26 27)
		__m64 dataHHH = _mm_unpackhi_pi32(dataLH,dataHH);	//dataHHH = (30 31 32 33 34 35 36 37)

		/*  dataHLL = (00 01 02 03 04 05 06 07) punpcklbh 0000 -->(0000 0001 0002 0003)  -->
		 *					punpckhbh 0000 -->(0004 0005 0006 0007)	 -->	
		 *  pextrh --> (0000 0000 0000 0000) (0000 0000 0000 0001) (0000 0000 0000 0002) (0000 0000 0000 0003)
		 *  pextrh --> (0004 0000 0000 0004) (0000 0000 0000 0005) (0000 0000 0000 0006) (0000 0000 0000 0007)
		 *  outptr[DCTSIZE*0 + 0] = (00) 
		 * */
		JSAMPROW outptr0 = output_buf[ctr + 0] + output_col;	
		JSAMPROW outptr1 = output_buf[ctr + 1] + output_col;	
		JSAMPROW outptr2 = output_buf[ctr + 2] + output_col;	
		JSAMPROW outptr3 = output_buf[ctr + 3] + output_col;	

		_mm_store_si64((__m64*)outptr0, dataHLL);
		_mm_store_si64((__m64*)outptr1, dataHLH);
		_mm_store_si64((__m64*)outptr2, dataHHL);
		_mm_store_si64((__m64*)outptr3, dataHHH);

#ifdef DEBUG_PASS2
    		printf("output2:%d\n", ctr);
    		printf("0x%16llx\n", to_uint64(dataHLL));
    		printf("0x%16llx\n", to_uint64(dataHLH));
    		printf("0x%16llx\n", to_uint64(dataHHL));
    		printf("0x%16llx\n\n", to_uint64(dataHHH));
#endif

		wsptr += loopsize;		/* advance pointer to next row */
	
	}
#ifdef DEBUG_PASS2
	while(1);
#endif
}

//#endif /* DCT_ISLOW_SUPPORTED */

//#define DEBUG_FDCT_PASS1
//#define DEBUG_FDCT_PASS2
/*
 * Perform the forward DCT on one block of samples.
 */

GLOBAL(void)
jsimd_fdct_islow_mmi (DCTELEM * data)
{
	__m64 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	__m64 tmp10, tmp11, tmp12, tmp13;
	__m64 z3, z4;
  	DCTELEM *dataptr;
	DCTELEM *tmpptr;
  	int ctr;
  	SHIFT_TEMPS

  	/* Pass 1: process rows. */
  	/* Note results are scaled up by sqrt(8) compared to a true DCT; */
  	/* furthermore,  we scale the results by 2**PASS1_BITS. */

  	int loopsize = 4; //TODO: 4*sizeof(dataptr[0]) = sizeof(__m64)
  	int loopcount = DCTSIZE / loopsize; //=2
  	dataptr = data;
	tmpptr = data;

#ifdef DEBUG_FDCT_PASS1
        printf("***********************1st PASS========================\n");
#endif

	for (ctr = loopcount; ctr > 0; ctr--){
		__m64 dataptr0 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*0]);			//(00 01 02 03)		
		__m64 dataptr1 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*0 + loopsize]);	//(04 05 06 07)
		__m64 dataptr2 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*1]);			//(10 11 12 13)	
		__m64 dataptr3 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*1 + loopsize]);	//(14 15 16 17)	
		__m64 dataptr4 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*2]);			//(20 21 22 23)	
		__m64 dataptr5 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*2 + loopsize]);	//(24 25 26 27)
		__m64 dataptr6 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*3]);			//(30 31 32 33)
		__m64 dataptr7 = _mm_load_si64((__m64 *)&dataptr[DCTSIZE*3 + loopsize]);	//(34 35 36 37)

#ifdef DEBUG_FDCT_PASS1
                        printf("input:%d\n", ctr);
                        printf("0x%16llx\n", to_uint64(dataptr0));
                        printf("0x%16llx\n", to_uint64(dataptr1));
                        printf("0x%16llx\n", to_uint64(dataptr2));
                        printf("0x%16llx\n", to_uint64(dataptr3));
                        printf("0x%16llx\n", to_uint64(dataptr4));
                        printf("0x%16llx\n", to_uint64(dataptr5));
                        printf("0x%16llx\n", to_uint64(dataptr6));
                        printf("0x%16llx\n", to_uint64(dataptr7));
#endif
		
		__m64 dataptr46L = _mm_unpacklo_pi16(dataptr4, dataptr6);	//dataptr46L=(20 30 21 31)
		__m64 dataptr46H = _mm_unpackhi_pi16(dataptr4, dataptr6);	//dataptr46H=(22 32 23 33)
		__m64 dataptr57L = _mm_unpacklo_pi16(dataptr5, dataptr7);	//dataptr57L=(24 34 25 35)
		__m64 dataptr57H = _mm_unpackhi_pi16(dataptr5, dataptr7);	//dataptr57H=(26 36 27 37)
		
		__m64 dataptr02L = _mm_unpacklo_pi16(dataptr0, dataptr2);	//dadaptr02L=(00 10 01 11)
		__m64 dataptr02H = _mm_unpackhi_pi16(dataptr0, dataptr2);	//dadaptr02H=(02 12 03 13)
		__m64 dataptr13L = _mm_unpacklo_pi16(dataptr1, dataptr3);	//dadaptr13L=(04 14 05 15)
		__m64 dataptr13H = _mm_unpackhi_pi16(dataptr1, dataptr3);	//dadaptr13H=(06 16 07 17)
		
		__m64 data0 = _mm_unpacklo_pi32(dataptr02L, dataptr46L);	//data0=(00 10 20 30)
		__m64 data1 = _mm_unpackhi_pi32(dataptr02L, dataptr46L);	//data1=(01 11 21 31)
		__m64 data6 = _mm_unpacklo_pi32(dataptr13H, dataptr57H);	//data6=(06 16 26 36)
		__m64 data7 = _mm_unpackhi_pi32(dataptr13H, dataptr57H);	//data7=(07 17 27 37)
									
		tmp6 = _mm_sub_pi16(data1, data6);	//tmp6=data1-data6=tmp6	
		tmp7 = _mm_sub_pi16(data0, data7);	//tmp7=data0-data7=tmp7
		tmp1 = _mm_add_pi16(data1, data6);	//tmp1=data1+data6=tmp1
		tmp0 = _mm_add_pi16(data0, data7);	//tmp0=data0+data7=tmp0
		
		__m64 data2 = _mm_unpacklo_pi32(dataptr02H, dataptr46H);	//data2=(02 12 22 32)	
		__m64 data3 = _mm_unpackhi_pi32(dataptr02H, dataptr46H);	//data3=(03 13 23 33)
		__m64 data4 = _mm_unpacklo_pi32(dataptr13L, dataptr57L);	//data4=(04 14 24 34)
		__m64 data5 = _mm_unpackhi_pi32(dataptr13L, dataptr57L);	//data5=(05 15 25 35)
		
		tmp3 = _mm_add_pi16(data3, data4);	//tmp3=data3+data4
		tmp2 = _mm_add_pi16(data2, data5);	//tmp2=data2+data5
		tmp4 = _mm_sub_pi16(data3, data4);	//tmp4=data3-data4
		tmp5 = _mm_sub_pi16(data2, data5);	//tmp5=data2-data5
#if 1	
#ifdef DEBUG_FDCT_PASS1
                        printf("tmp:%d\n", ctr);
                        printf("0x%16llx\n", to_uint64(tmp0));
                        printf("0x%16llx\n", to_uint64(tmp1));
                        printf("0x%16llx\n", to_uint64(tmp2));
                        printf("0x%16llx\n", to_uint64(tmp3));
                        printf("0x%16llx\n", to_uint64(tmp4));
                        printf("0x%16llx\n", to_uint64(tmp5));
                        printf("0x%16llx\n", to_uint64(tmp6));
                        printf("0x%16llx\n", to_uint64(tmp7));
#endif
#endif	
		/* Even part per LL&M figure 1 --- note that published figure is faulty;
 		 * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
 		 */

		tmp10 = _mm_add_pi16(tmp0, tmp3);	//tmp10=tmp0+tmp3
		tmp13 = _mm_sub_pi16(tmp0, tmp3);	//tmp13=tmp0-tmp3
		tmp11 = _mm_add_pi16(tmp1, tmp2);	//tmp11=tmp1+tmp2
		tmp12 = _mm_sub_pi16(tmp1, tmp2);	//tmp12=tmp1-tmp2
		
		data0 = _mm_add_pi16(tmp10, tmp11);	//data0=tmp10+tmp11
		data4 = _mm_sub_pi16(tmp10, tmp11);	//data4=tmp10-tmp11
		data0 = _mm_slli_pi16(data0, PASS1_BITS);	//data0=data0 << PASS1_BITS
		data4 = _mm_slli_pi16(data4, PASS1_BITS);	//data0=data4 << PASS1_BITS
		
		/*(Original)
        	 *z1 = (tmp12 + tmp13) * 0.541196100;
        	 *data2 = z1 + tmp13 * 0.765366865;
        	 *data6 = z1 + tmp12 * -1.847759065;
        	 *
        	 *(This implementation)
        	 *data2 = tmp13 * (0.541196100 + 0.765366865) + tmp12 * 0.541196100;
        	 *data6 = tmp13 * 0.541196100 + tmp12 * (0.541196100 - 1.847759065);
		 */
		__m64 data2lo = _mm_unpacklo_pi16(tmp13, tmp12);
		__m64 data2hi = _mm_unpackhi_pi16(tmp13, tmp12);
		
		__m64 data2L = _mm_madd_pi16(data2lo, PW_F130_F054);	//data2L
		__m64 data2H = _mm_madd_pi16(data2hi, PW_F130_F054);	//data2H
		__m64 data6L = _mm_madd_pi16(data2lo, PW_F054_MF130);	//data6L
		__m64 data6H = _mm_madd_pi16(data2hi, PW_F054_MF130);	//data6H
		
		data2L = _mm_add_pi32(data2L, PD_DESCALE_P1);	//data2L=data2L+PD_DESCALE_P1
		data2H = _mm_add_pi32(data2H, PD_DESCALE_P1);	//data2H=data2H+PD_DESCALE_P1
		data2L = _mm_srai_pi32(data2L, DESCALE_P1);	//data2L=data2L >> DESCALE_P1
		data2H = _mm_srai_pi32(data2H, DESCALE_P1);	//data2H=data2H >> DESCALE_P1
		
		data6L = _mm_add_pi32(data6L, PD_DESCALE_P1);	//data6L=data6L+PD_DESCALE_P1
		data6H = _mm_add_pi32(data6H, PD_DESCALE_P1);	//data6H=data6H+PD_DESCALE_P1
		data6L = _mm_srai_pi32(data6L, DESCALE_P1);	//data6L=data6L >> DESCALE_P1
		data6H = _mm_srai_pi32(data6H, DESCALE_P1);	//data6H=data6H >> DESCALE_P1
		
		data2 = _mm_packs_pi32(data2L, data2H);		//data2
		data6 = _mm_packs_pi32(data6L, data6H);		//data6
		
    		/* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     		 * cK represents cos(K*pi/16).
     		 * i0..i3 in the paper are tmp4..tmp7 here.
     		 */
		
		z3 = _mm_add_pi16(tmp4, tmp6);	//z3=tmp4+tmp6
		z4 = _mm_add_pi16(tmp5, tmp7);	//z4=tmp5+tmp7
			
		/*(Original)
        	 *z5 = (z3 + z4) * 1.175875602;
        	 *z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
        	 *z3 += z5;  z4 += z5;
        	 *
        	 * (This implementation)
        	 * z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
        	 * z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);
		 */
		__m64 z34lo = _mm_unpacklo_pi16(z3, z4);	//
		__m64 z34hi = _mm_unpackhi_pi16(z3, z4); 	//
		__m64 z3L = _mm_madd_pi16(z34lo, PW_MF078_F117);	//z3L
		__m64 z3H = _mm_madd_pi16(z34hi, PW_MF078_F117);	//z3H
		__m64 z4L = _mm_madd_pi16(z34lo, PW_F117_F078);		//z4L
		__m64 z4H = _mm_madd_pi16(z34hi, PW_F117_F078);		//z4H
		
	 	/*(Original)
        	* z1 = tmp4 + tmp7;  z2 = tmp5 + tmp6;
        	* tmp4 = tmp4 * 0.298631336;  tmp5 = tmp5 * 2.053119869;
        	* tmp6 = tmp6 * 3.072711026;  tmp7 = tmp7 * 1.501321110;
        	* z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
        	* data7 = tmp4 + z1 + z3;  data5 = tmp5 + z2 + z4;
        	* data3 = tmp6 + z2 + z3;  data1 = tmp7 + z1 + z4;
        	*
        	* (This implementation)
        	* tmp4 = tmp4 * (0.298631336 - 0.899976223) + tmp7 * -0.899976223;
        	* tmp5 = tmp5 * (2.053119869 - 2.562915447) + tmp6 * -2.562915447;
        	* tmp6 = tmp5 * -2.562915447 + tmp6 * (3.072711026 - 2.562915447);
        	* tmp7 = tmp4 * -0.899976223 + tmp7 * (1.501321110 - 0.899976223);
        	* data7 = tmp4 + z3;  data5 = tmp5 + z4;
        	* data3 = tmp6 + z3;  data1 = tmp7 + z4;
		*/
		__m64 tmp47lo = _mm_unpacklo_pi16(tmp4, tmp7);	//
		__m64 tmp47hi = _mm_unpackhi_pi16(tmp4, tmp7);	//
		
		__m64 tmp4L = _mm_madd_pi16(tmp47lo, PW_MF060_MF089);	//tmp4L
		__m64 tmp4H = _mm_madd_pi16(tmp47hi, PW_MF060_MF089);	//tmp4H
		__m64 tmp7L = _mm_madd_pi16(tmp47lo, PW_MF089_F060);	//tmp7L
		__m64 tmp7H = _mm_madd_pi16(tmp47hi, PW_MF089_F060);	//tmp7H
		
		__m64 data7L = _mm_add_pi32(tmp4L, z3L);	//data7L
		__m64 data7H = _mm_add_pi32(tmp4H, z3H);	//data7H
		__m64 data1L = _mm_add_pi32(tmp7L, z4L);	//data1L
		__m64 data1H = _mm_add_pi32(tmp7H, z4H);	//data1H
	
		data7L = _mm_add_pi32(data7L, PD_DESCALE_P1);
		data7H = _mm_add_pi32(data7H, PD_DESCALE_P1);
		data7L = _mm_srai_pi32(data7L, DESCALE_P1);
		data7H = _mm_srai_pi32(data7H, DESCALE_P1);
		
		data1L = _mm_add_pi32(data1L, PD_DESCALE_P1);
		data1H = _mm_add_pi32(data1H, PD_DESCALE_P1);
		data1L = _mm_srai_pi32(data1L, DESCALE_P1);
		data1H = _mm_srai_pi32(data1H, DESCALE_P1);
		
		data7 = _mm_packs_pi32(data7L, data7H);          //data7
                data1 = _mm_packs_pi32(data1L, data1H);          //data1
		
		__m64 tmp56lo = _mm_unpacklo_pi16(tmp5, tmp6);	//
		__m64 tmp56hi = _mm_unpackhi_pi16(tmp5, tmp6);	//
		
		__m64 tmp5L = _mm_madd_pi16(tmp56lo, PW_MF050_MF256);	//tmp5L
		__m64 tmp5H = _mm_madd_pi16(tmp56hi, PW_MF050_MF256);	//tmp5H
		__m64 tmp6L = _mm_madd_pi16(tmp56lo, PW_MF256_F050);	//tmp6L
		__m64 tmp6H = _mm_madd_pi16(tmp56hi, PW_MF256_F050);	//tmp6H
		
		__m64 data5L = _mm_add_pi32(tmp5L, z4L);	//data5L
		__m64 data5H = _mm_add_pi32(tmp5H, z4H);	//data5H
		__m64 data3L = _mm_add_pi32(tmp6L, z3L);	//data3L
		__m64 data3H = _mm_add_pi32(tmp6H, z3H);	//data3H
	
		data5L = _mm_add_pi32(data5L, PD_DESCALE_P1);
		data5H = _mm_add_pi32(data5H, PD_DESCALE_P1);
		data5L = _mm_srai_pi32(data5L, DESCALE_P1);
		data5H = _mm_srai_pi32(data5H, DESCALE_P1);
		
		data3L = _mm_add_pi32(data3L, PD_DESCALE_P1);
		data3H = _mm_add_pi32(data3H, PD_DESCALE_P1);
		data3L = _mm_srai_pi32(data3L, DESCALE_P1);
		data3H = _mm_srai_pi32(data3H, DESCALE_P1);
		
		data5 = _mm_packs_pi32(data5L, data5H);          //data5
                data3 = _mm_packs_pi32(data3L, data3H);          //data3
#if 0	
		_loopsize;mm_store_si64((__m64*)&tmpptr[DCTSIZE*0], data0);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*1], data1);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*2], data2);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*3], data3);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*4], data4);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*5], data5);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*6], data6);
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*7], data7);
#endif
#if 1		
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*0], data0);	//data0
                _mm_store_si64((__m64*)&tmpptr[DCTSIZE*0 + loopsize], data4);	//data4
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*1], data1);	//data1
                _mm_store_si64((__m64*)&tmpptr[DCTSIZE*1 + loopsize], data5);	//data5
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*2], data2);	//data2
                _mm_store_si64((__m64*)&tmpptr[DCTSIZE*2 + loopsize], data6);	//data6
		_mm_store_si64((__m64*)&tmpptr[DCTSIZE*3], data3);	//data3
                _mm_store_si64((__m64*)&tmpptr[DCTSIZE*3 + loopsize], data7);	//data7
#endif
	
#ifdef DEBUG_FDCT_PASS1
                printf("tmpptr1:%d\n", ctr);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*0]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*0 + loopsize]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*1]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*1 + loopsize]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*2]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*2 + loopsize]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*3]);
                printf("0x%16llx\n", *(__m64*)&tmpptr[DCTSIZE*3 + loopsize]);
#endif
		

		dataptr += DCTSIZE*loopsize;	/* advance pointer to next row */
		tmpptr += DCTSIZE*loopsize; 
	}
#if 0
#if defined(DEBUG_FDCT_PASS1) && !defined(DEBUG_FDCT_PASS2)
        while(1);
#endif
#endif
	
  	/* Pass 2: process columns.
   	 * We remove the PASS1_BITS scaling,  but leave the results scaled up
   	 * by an overall factor of 8.
   	 */

#ifdef DEBUG_FDCT_PASS2
        printf("***********************2ed PASS========================\n");
#endif

	dataptr = data;
  	tmpptr = data;
	  
	for (ctr = loopcount; ctr > 0; ctr--) {
		__m64 dataptr0 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*0]);	//(00 10 20 30)
		__m64 dataptr1 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*1]);	//(01 11 21 31)
		__m64 dataptr2 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*2]);	//(02 12 22 32)
		__m64 dataptr3 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*3]);	//(03 13 23 33)
		__m64 dataptr4 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*4]);	//(40 50 60 70)
		__m64 dataptr5 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*5]);	//(41 51 61 71)
		__m64 dataptr6 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*6]);	//(42 52 62 72)
		__m64 dataptr7 = _mm_load_si64((__m64 *)&tmpptr[DCTSIZE*7]);	//(43 53 63 73)
#if 0
#ifdef DEBUG_FDCT_PASS2
                printf("dataptr2:%d\n", ctr);
                printf("0x%16llx\n", to_uint64(dataptr0));
                printf("0x%16llx\n", to_uint64(dataptr1));
                printf("0x%16llx\n", to_uint64(dataptr2));
                printf("0x%16llx\n", to_uint64(dataptr3));
                printf("0x%16llx\n", to_uint64(dataptr4));
                printf("0x%16llx\n", to_uint64(dataptr5));
                printf("0x%16llx\n", to_uint64(dataptr6));
                printf("0x%16llx\n\n", to_uint64(dataptr7));
#endif
#endif
		__m64 dataptr23L = _mm_unpacklo_pi16(dataptr2, dataptr3);       //dataptr23L=(02 03 13 13)
                __m64 dataptr23H = _mm_unpackhi_pi16(dataptr2, dataptr3);       //dataptr23H=(22 23 32 33)
                __m64 dataptr67L = _mm_unpacklo_pi16(dataptr6, dataptr7);       //dataptr67L=(42 43 52 53)
                __m64 dataptr67H = _mm_unpackhi_pi16(dataptr6, dataptr7);       //dataptr67H=(62 63 72 73)
		
		__m64 dataptr01L = _mm_unpacklo_pi16(dataptr0, dataptr1);       //dataptr01L=(00 01 10 11)
                __m64 dataptr01H = _mm_unpackhi_pi16(dataptr0, dataptr1);       //dataptr01L=(20 21 30 31)
                __m64 dataptr45L = _mm_unpacklo_pi16(dataptr4, dataptr5);       //dataptr45L=(40 41 50 51)
                __m64 dataptr45H = _mm_unpackhi_pi16(dataptr4, dataptr5);       //dataptr45H=(60 61 70 71)
		
		__m64 data0 = _mm_unpacklo_pi32(dataptr01L, dataptr23L);       //data0=(00 01 02 03)
                __m64 data1 = _mm_unpackhi_pi32(dataptr01L, dataptr23L);       //data1=(10 11 12 13)
                __m64 data6 = _mm_unpacklo_pi32(dataptr45H, dataptr67H);       //data6=(60 61 62 63)
                __m64 data7 = _mm_unpackhi_pi32(dataptr45H, dataptr67H);       //data7=(70 71 72 73)
		
		tmp6 = _mm_sub_pi16(data1, data6);         //tmp6=data1-data6  
                tmp7 = _mm_sub_pi16(data0, data7);         //tmp7=data0-data7
                tmp1 = _mm_add_pi16(data1, data6);         //tmp1=data1+data6
                tmp0 = _mm_add_pi16(data0, data7);         //tmp0=data0+data7

                __m64 data2 = _mm_unpacklo_pi32(dataptr01H, dataptr23H);         //data2=(20 21 22 23)       
                __m64 data3 = _mm_unpackhi_pi32(dataptr01H, dataptr23H);         //data3=(30 31 32 33)
                __m64 data4 = _mm_unpacklo_pi32(dataptr45L, dataptr67L);         //data4=(40 41 42 43)
                __m64 data5 = _mm_unpackhi_pi32(dataptr45L, dataptr67L);         //data5=(50 51 52 53)

               	tmp3 = _mm_add_pi16(data3, data4);         //tmp3=data3+data4
                tmp2 = _mm_add_pi16(data2, data5);         //tmp2=data2+data5
                tmp4 = _mm_sub_pi16(data3, data4);         //tmp4=data3-data4
                tmp5 = _mm_sub_pi16(data2, data5);         //tmp5=data2-data5
#if 0	
#ifdef DEBUG_FDCT_PASS2
                        printf("data:%d\n", ctr);
                        printf("0x%16llx\n", to_uint64(data0));
                        printf("0x%16llx\n", to_uint64(data1));
                        printf("0x%16llx\n", to_uint64(data2));
                        printf("0x%16llx\n", to_uint64(data3));
                        printf("0x%16llx\n", to_uint64(data4));
                        printf("0x%16llx\n", to_uint64(data5));
                        printf("0x%16llx\n", to_uint64(data6));
                        printf("0x%16llx\n\n", to_uint64(data7));
#endif
#endif	

#if 0	
#ifdef DEBUG_FDCT_PASS2
                        printf("tmp:%d\n", ctr);
                        printf("0x%16llx\n", to_uint64(tmp0));
                        printf("0x%16llx\n", to_uint64(tmp1));
                        printf("0x%16llx\n", to_uint64(tmp2));
                        printf("0x%16llx\n", to_uint64(tmp3));
                        printf("0x%16llx\n", to_uint64(tmp4));
                        printf("0x%16llx\n", to_uint64(tmp5));
                        printf("0x%16llx\n", to_uint64(tmp6));
                        printf("0x%16llx\n\n", to_uint64(tmp7));
#endif
#endif	
		/* Even part per LL&M figure 1 --- note that published figure is faulty;
 		 * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
 		 */

		tmp10 = _mm_add_pi16(tmp0, tmp3);          //tmp10=tmp0+tmp3
                tmp13 = _mm_sub_pi16(tmp0, tmp3);          //tmp13=tmp0-tmp3
                tmp11 = _mm_add_pi16(tmp1, tmp2);          //tmp11=tmp1+tmp2
                tmp12 = _mm_sub_pi16(tmp1, tmp2);          //tmp12=tmp1-tmp2
#if 0	
#ifdef DEBUG_FDCT_PASS2
		
                printf("tmp1:%d\n", ctr);
		printf("0x%16llx\n", to_uint64(tmp10));
		printf("0x%16llx\n", to_uint64(tmp11));
		printf("0x%16llx\n", to_uint64(tmp12));
		printf("0x%16llx\n", to_uint64(tmp13));
#endif
#endif		
                data0 = _mm_add_pi16(tmp10, tmp11);              //data0=tmp10+tmp11
                data4 = _mm_sub_pi16(tmp10, tmp11);              //data4=tmp10-tmp11
                
		data0 = _mm_add_pi16(data0, PW_DESCALE_P2X);	 //data0
                data4 = _mm_add_pi16(data4, PW_DESCALE_P2X);	 //data4
                data0 = _mm_srai_pi16(data0, PASS1_BITS);       //data0=data0 >> PASS1_BITS
                data4 = _mm_srai_pi16(data4, PASS1_BITS);       //data0=data4 >> PASS1_BITS
		
#if 0	
#ifdef DEBUG_FDCT_PASS2
		
		printf("ouput2:%d\n", ctr);
		printf("0x%16llx\n", to_uint64(data0));	
		printf("0x%16llx\n", to_uint64(data4));	
#endif
#endif		
		
		/*(Original)
 		 *z1 = (tmp12 + tmp13) * 0.541196100;
        	 * data2 = z1 + tmp13 * 0.765366865;
        	 * data6 = z1 + tmp12 * -1.847759065;
        	 *
        	 * (This implementation)
        	 * data2 = tmp13 * (0.541196100 + 0.765366865) + tmp12 * 0.541196100;
        	 * data6 = tmp13 * 0.541196100 + tmp12 * (0.541196100 - 1.847759065);
		 */ 	
		__m64 data2lo = _mm_unpacklo_pi16(tmp13, tmp12);
                __m64 data2hi = _mm_unpackhi_pi16(tmp13, tmp12);

                __m64 data2L = _mm_madd_pi16(data2lo, PW_F130_F054);     //data2L
                __m64 data2H = _mm_madd_pi16(data2hi, PW_F130_F054);     //data2H
                __m64 data6L = _mm_madd_pi16(data2lo, PW_F054_MF130);    //data6L
                __m64 data6H = _mm_madd_pi16(data2hi, PW_F054_MF130);    //data6H

                data2L = _mm_add_pi32(data2L, FPD_DESCALE_P2);      //data2L=data2L+FPD_DESCALE_P2
               	data2H = _mm_add_pi32(data2H, FPD_DESCALE_P2);      //data2H=data2H+FPD_DESCALE_P2
                data2L = _mm_srai_pi32(data2L, FDESCALE_P2);        //data2L=data2L >> DESCALE_P2
                data2H = _mm_srai_pi32(data2H, FDESCALE_P2);        //data2H=data2H >> DESCALE_P2

                data6L = _mm_add_pi32(data6L, FPD_DESCALE_P2);      //data6L=data6L+FPD_DESCALE_P2
                data6H = _mm_add_pi32(data6H, FPD_DESCALE_P2);      //data6H=data6H+FPD_DESCALE_P2
                data6L = _mm_srai_pi32(data6L, FDESCALE_P2);        //data6L=data6L >> DESCALE_P2
                data6H = _mm_srai_pi32(data6H, FDESCALE_P2);        //data6H=data6H >> DESCALE_P2

                data2 = _mm_packs_pi32(data2L, data2H);          //data2
                data6 = _mm_packs_pi32(data6L, data6H);          //data6

#if 0	
#ifdef DEBUG_FDCT_PASS2
		printf("outdata26:%d\n", ctr);
		printf("0x%16llx\n", to_uint64(data2));	
		printf("0x%16llx\n", to_uint64(data6));	
#endif
#endif		
			
		/* Odd part per figure 8 --- note paper omits factor of sqrt(2).
                 * cK represents cos(K*pi/16).
                 * i0..i3 in the paper are tmp4..tmp7 here.
                 */
                z3 = _mm_add_pi16(tmp4, tmp6);         //z3=tmp4+tmp6
                z4 = _mm_add_pi16(tmp5, tmp7);         //z4=tmp5+tmp7
		
		/* (Original)
         	 * z5 = (z3 + z4) * 1.175875602;
        	 * z3 = z3 * -1.961570560;  z4 = z4 * -0.390180644;
        	 * z3 += z5;  z4 += z5;
        
        	 * (This implementation)
        	 * z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
        	 * z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);
		 */
		__m64 z34lo = _mm_unpacklo_pi16(z3, z4); //
                __m64 z34hi = _mm_unpackhi_pi16(z3, z4); //
                __m64 z3L = _mm_madd_pi16(z34lo, PW_MF078_F117);         //z3L
                __m64 z3H = _mm_madd_pi16(z34hi, PW_MF078_F117);         //z3H
                __m64 z4L = _mm_madd_pi16(z34lo, PW_F117_F078);          //z4L
                __m64 z4H = _mm_madd_pi16(z34hi, PW_F117_F078);          //z4H
		
	 	/*(Original)
        	* z1 = tmp4 + tmp7;  z2 = tmp5 + tmp6;
        	* tmp4 = tmp4 * 0.298631336;  tmp5 = tmp5 * 2.053119869;
        	* tmp6 = tmp6 * 3.072711026;  tmp7 = tmp7 * 1.501321110;
        	* z1 = z1 * -0.899976223;  z2 = z2 * -2.562915447;
        	* data7 = tmp4 + z1 + z3;  data5 = tmp5 + z2 + z4;
        	* data3 = tmp6 + z2 + z3;  data1 = tmp7 + z1 + z4;
        	*
        	* (This implementation)
        	* tmp4 = tmp4 * (0.298631336 - 0.899976223) + tmp7 * -0.899976223;
        	* tmp5 = tmp5 * (2.053119869 - 2.562915447) + tmp6 * -2.562915447;
        	* tmp6 = tmp5 * -2.562915447 + tmp6 * (3.072711026 - 2.562915447);
        	* tmp7 = tmp4 * -0.899976223 + tmp7 * (1.501321110 - 0.899976223);
        	* data7 = tmp4 + z3;  data5 = tmp5 + z4;
        	* data3 = tmp6 + z3;  data1 = tmp7 + z4;
		*/
		__m64 tmp47lo = _mm_unpacklo_pi16(tmp4, tmp7);	//
		__m64 tmp47hi = _mm_unpackhi_pi16(tmp4, tmp7);	//
		
		__m64 tmp4L = _mm_madd_pi16(tmp47lo, PW_MF060_MF089);	//tmp4L
		__m64 tmp4H = _mm_madd_pi16(tmp47hi, PW_MF060_MF089);	//tmp4H
		__m64 tmp7L = _mm_madd_pi16(tmp47lo, PW_MF089_F060);	//tmp7L
		__m64 tmp7H = _mm_madd_pi16(tmp47hi, PW_MF089_F060);	//tmp7H
		
		__m64 data7L = _mm_add_pi32(tmp4L, z3L);	//data7L
		__m64 data7H = _mm_add_pi32(tmp4H, z3H);	//data7H
		__m64 data1L = _mm_add_pi32(tmp7L, z4L);	//data1L
		__m64 data1H = _mm_add_pi32(tmp7H, z4H);	//data1H
	
		data7L = _mm_add_pi32(data7L, FPD_DESCALE_P2);
		data7H = _mm_add_pi32(data7H, FPD_DESCALE_P2);
		data7L = _mm_srai_pi32(data7L, FDESCALE_P2);
		data7H = _mm_srai_pi32(data7H, FDESCALE_P2);
		
		data1L = _mm_add_pi32(data1L, FPD_DESCALE_P2);
		data1H = _mm_add_pi32(data1H, FPD_DESCALE_P2);
		data1L = _mm_srai_pi32(data1L, FDESCALE_P2);
		data1H = _mm_srai_pi32(data1H, FDESCALE_P2);
		
		data7 = _mm_packs_pi32(data7L, data7H);          //data7
                data1 = _mm_packs_pi32(data1L, data1H);          //data1
		
		__m64 tmp56lo = _mm_unpacklo_pi16(tmp5, tmp6);	//
		__m64 tmp56hi = _mm_unpackhi_pi16(tmp5, tmp6);	//
		
		__m64 tmp5L = _mm_madd_pi16(tmp56lo, PW_MF050_MF256);	//tmp5L
		__m64 tmp5H = _mm_madd_pi16(tmp56hi, PW_MF050_MF256);	//tmp5H
		__m64 tmp6L = _mm_madd_pi16(tmp56lo, PW_MF256_F050);	//tmp6L
		__m64 tmp6H = _mm_madd_pi16(tmp56hi, PW_MF256_F050);	//tmp6H
		
		__m64 data5L = _mm_add_pi32(tmp5L, z4L);	//data5L
		__m64 data5H = _mm_add_pi32(tmp5H, z4H);	//data5H
		__m64 data3L = _mm_add_pi32(tmp6L, z3L);	//data3L
		__m64 data3H = _mm_add_pi32(tmp6H, z3H);	//data3H
		
		data5L = _mm_add_pi32(data5L, FPD_DESCALE_P2);
		data5H = _mm_add_pi32(data5H, FPD_DESCALE_P2);
		data5L = _mm_srai_pi32(data5L, FDESCALE_P2);
		data5H = _mm_srai_pi32(data5H, FDESCALE_P2);
		
		data3L = _mm_add_pi32(data3L, FPD_DESCALE_P2);
		data3H = _mm_add_pi32(data3H, FPD_DESCALE_P2);
		data3L = _mm_srai_pi32(data3L, FDESCALE_P2);
		data3H = _mm_srai_pi32(data3H, FDESCALE_P2);
		
		data5 = _mm_packs_pi32(data5L, data5H);          //data5
                data3 = _mm_packs_pi32(data3L, data3H);          //data3
		
		_mm_store_si64((__m64*)&dataptr[DCTSIZE*0], data0);	//data0
                _mm_store_si64((__m64*)&dataptr[DCTSIZE*1], data1);	//data1
		_mm_store_si64((__m64*)&dataptr[DCTSIZE*2], data2);	//data2
                _mm_store_si64((__m64*)&dataptr[DCTSIZE*3], data3);	//data3
		_mm_store_si64((__m64*)&dataptr[DCTSIZE*4], data4);	//data4
                _mm_store_si64((__m64*)&dataptr[DCTSIZE*5], data5);	//data5
		_mm_store_si64((__m64*)&dataptr[DCTSIZE*6], data6);	//data6
                _mm_store_si64((__m64*)&dataptr[DCTSIZE*7], data7);	//data7
#if 0	
#ifdef DEBUG_FDCT_PASS2
                printf("ouput2:%d\n", ctr);
                printf("0x%16llx\n", to_uint64(data0));
                printf("0x%16llx\n", to_uint64(data1));
                printf("0x%16llx\n", to_uint64(data2));
                printf("0x%16llx\n", to_uint64(data3));
                printf("0x%16llx\n", to_uint64(data4));
                printf("0x%16llx\n", to_uint64(data5));
                printf("0x%16llx\n", to_uint64(data6));
                printf("0x%16llx\n\n", to_uint64(data7));
#endif
#endif
    		dataptr += loopsize;		/* advance pointer to next column */
		tmpptr += loopsize;
  }

#if 0
#ifdef DEBUG_FDCT_PASS2
        while(1);
#endif
#endif
}


GLOBAL(void)
jsimd_h2v2_fancy_upsample_mmi (int max_v_samp_factor,JDIMENSION downsampled_width,JSAMPARRAY input_data,JSAMPARRAY * output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  register int colctr = downsampled_width;
 // register int rowctr =  max_v_samp_factor;
  register JSAMPROW inptr0, inptr1,outptr0,outptr1;
  register __m64 mm0,mm1,mm2,mm3,mm4,mm5,mm6,mm7;
  __m64 wk[4],mm_tmp;
	register JSAMPROW inptr1_a,inptr1_b;			//inptr1(above),inptr1(below)
	int outrow,inrow,tmp,tmp1;
	outrow = 0;
	inrow = 0;
	if(0 == downsampled_width)
		return;
	if(0 == max_v_samp_factor)
		return;
//		fprintf(stderr,"max_v_samp_factor = %d\n" , max_v_samp_factor);
	while(outrow < max_v_samp_factor){
		inptr0	= input_data[inrow];
		inptr1_a = input_data[(inrow-1)];
		inptr1_b = input_data[(inrow+1)];
		outptr0 = output_data[outrow];
		outptr1 = output_data[(outrow+1)];
 		if((downsampled_width&0x7) != 0)
		{
			tmp = (downsampled_width-1)*sizeof(JSAMPLE);
//			fprintf(stderr,"tmp = %d\n",tmp);
			tmp1 =  downsampled_width*sizeof(JSAMPLE);
//			fprintf(stderr,"tmp1 = %d\n",tmp1);
			asm("daddu	$8,%3,%6\r\n"
			"lb	$9,($8)\r\n"
			"daddu	$8,%3,%7\r\n"
			"sb	$9,($8)\r\n"
			"daddu	$8,%4,%6\r\n"
			"lb	$9,($8)\r\n"
			"daddu	$8,%4,%7\r\n"
			"sb	$9,($8)\r\n"
			"daddu	$8,%5,%6\r\n"
			"lb	$9,($8)\r\n"
			"daddu	$8,%5,%7\r\n"
			"sb	$9,($8)\r\n"
			:"=m"(*inptr1_a),"=m"(*inptr0),"=m"(*inptr1_b)
			:"r"(inptr1_a),"r"(inptr0),"r"(inptr1_b),"r"(tmp),"r"(tmp1)
			:"$8","$9"
			);
		}

		/*process the first column block*/
		mm0 = _mm_load_si64((__m64 *)inptr0);			//mm0 = row[0][0]
		mm1 = _mm_load_si64((__m64 *)inptr1_a);			//mm1 = row[-1][0]
		mm2 = _mm_load_si64((__m64 *)inptr1_b);			//mm2 = row[1][0]

		mm3 = _mm_xor_si64(mm3,mm3);					//mm3 = (all 0's)
		mm4 = mm0;
		mm0 = _mm_unpacklo_pi8(mm0,mm3);				//mm0 = row[0][0](0 1 2 3)
		mm4 = _mm_unpackhi_pi8(mm4,mm3);				//mm4 = row[0][0](4 5 6 7)
		mm5 = mm1;
		mm1 = _mm_unpacklo_pi8(mm1,mm3);				//mm1 = row[-1][0](0 1 2 3)
		mm5 = _mm_unpackhi_pi8(mm5,mm3);				//mm5 = row[-1][0](4 5 6 7)
		mm6 = mm2;
		mm2 = _mm_unpacklo_pi8(mm2,mm3);				//mm2 = row[+1][0]( 0 1 2 3)
		mm6 = _mm_unpackhi_pi8(mm6,mm3);				//mm6 = row[+1][0]( 4 5 6 7)

		mm0 = _mm_mullo_pi16(mm0,PW_THREE);
		mm4 = _mm_mullo_pi16(mm4,PW_THREE);

		mm7 = _mm_cmpeq_pi8(mm7,mm7);
		mm7 = _mm_srli_si64(mm7,(sizeof(MMWORD) - 2)*BYTE_BIT);

		mm1 = _mm_add_pi16(mm1,mm0);				//mm1=Int0L=( 0 1 2 3)
		mm5 = _mm_add_pi16(mm5,mm4);				//mm5=Int0H=( 4 5 6 7)
		mm2 = _mm_add_pi16(mm2,mm0);				//mm2=Int1L=( 0 1 2 3)
		mm6 = _mm_add_pi16(mm6,mm4);				//mm6=Int1H=( 4 5 6 7)

		_mm_store_si64((__m64*)outptr0,mm1);								//temporarily save
		_mm_store_si64((__m64*)(outptr0)+1,mm5);			//the intermediate data
   	    _mm_store_si64((__m64*)outptr1,mm2);
        _mm_store_si64((__m64*)(outptr1)+1,mm6);

		mm1 = _mm_and_si64(mm1,mm7);							//mm1 = (0 - - - )
		mm2 = _mm_and_si64(mm2,mm7);							//mm2 = (0 - - - )

		wk[0] = mm1;
		wk[1] = mm2;

		colctr += 7;
		colctr = colctr&(-8);
//		fprintf(stderr,"colctr_or = %d\n" , colctr);
		while(colctr > 0){
			if(colctr > 8){
				mm0 = _mm_load_si64((__m64 *)(inptr0)+1);				//mm0 = row[0][1]
				mm1 = _mm_load_si64((__m64 *)(inptr1_a)+1);				//mm1 = row[-1][1]
				mm2 = _mm_load_si64((__m64 *)(inptr1_b)+1);				//mm2 = row[+1][1]
			
				mm3 = _mm_setzero_si64();		//mm3 = (all 0's)
				mm4 = mm0;
				mm0 = _mm_unpacklo_pi8(mm0,mm3);										//mm0 = row[0][1](0 1 2 3)
				mm4 = _mm_unpackhi_pi8(mm4,mm3);										//mm4 = row[0][1](4 5 6 7)
				mm5 = mm1;
     		   	mm1 = _mm_unpacklo_pi8(mm1,mm3);										//mm1 = row[-1][1](0 1 2 3)
                mm5 = _mm_unpackhi_pi8(mm5,mm3);										//mm5 = row[-1][1](4 5 6 7)
				mm6 = mm2;
				mm2 = _mm_unpacklo_pi8(mm2,mm3);										//mm2 = row[+1][1](0 1 2 3)
				mm6 = _mm_unpackhi_pi8(mm6,mm3);										//mm6 = row[+1][1](4 5 6 7)
		
				mm0 = _mm_mullo_pi16(mm0,PW_THREE);
				mm4 = _mm_mullo_pi16(mm4,PW_THREE);

				mm1 = _mm_add_pi16(mm1,mm0);											//mm1 = Int0L = ( 0 1 2 3)
				mm5 = _mm_add_pi16(mm5,mm4);											//mm5 = Int0H = ( 4 5 6 7)
				mm2 = _mm_add_pi16(mm2,mm0);											//mm2 = Int1L = ( 0 1 2 3)
				mm6 = _mm_add_pi16(mm6,mm4);											//mm6 = Int1H = ( 4 5 6 7)
			
                _mm_store_si64((__m64*)(outptr0)+2,mm1);				//temporarily save
                _mm_store_si64((__m64*)(outptr0)+3,mm5);				//the intermediate data
                _mm_store_si64((__m64*)(outptr1)+2,mm2);
                _mm_store_si64((__m64*)(outptr1)+3,mm6);

				mm1=_mm_slli_si64(mm1,(sizeof(MMWORD)-2)*BYTE_BIT);						//mm1=( - - - 0)
				mm2=_mm_slli_si64(mm2,(sizeof(MMWORD)-2)*BYTE_BIT);						//mm2=( - - - 0) 

				wk[2] = mm1;
				wk[3] = mm2;
			}else{																		//columnloop_last
				mm1 = _mm_cmpeq_pi8(mm1,mm1);
				mm1 = _mm_slli_si64(mm1,(sizeof(MMWORD)-2)*BYTE_BIT);
				mm2 = mm1;
			
				mm_tmp = _mm_load_si64((__m64 *)(outptr0)+1);
				mm1 = _mm_and_si64(mm1,mm_tmp);											//mm1 = ( - - - 7)
				mm_tmp = _mm_load_si64((__m64 *)(outptr1)+1);
				mm2 = _mm_and_si64(mm2,mm_tmp);											//mm2 = ( - - - 7)

				wk[2] = mm1;
				wk[3] = mm2;
//				fprintf(stderr,"last_colctr = %d\n" , colctr);
			}

/* .upsample:  */
			mm7 = _mm_load_si64((__m64 *)(outptr0));				//mm7=Int0L=( 0 1 2 3)
			mm3 = _mm_load_si64((__m64 *)(outptr0)+1);		//mm3=Int0H=( 4 5 6 7)
			
			mm0 = mm7;
			mm4 = mm3;
			mm0 = _mm_srli_si64(mm0,2*BYTE_BIT);					//mm0=( 1 2 3 -)
			mm4 = _mm_slli_si64(mm4,(sizeof(MMWORD) - 2)*BYTE_BIT);			//mm4=( - - - 4)
			mm5 = mm7;
			mm6 = mm3;
			mm5 = _mm_srli_si64(mm5,(sizeof(MMWORD) - 2)*BYTE_BIT);			//mm5=(3 - - -)
			mm6 = _mm_slli_si64(mm6,2*BYTE_BIT);					//mm6=(- 4 5 6)

			mm0 = _mm_or_si64(mm0,mm4);						//mm0=(1 2 3 4)
			mm5 = _mm_or_si64(mm5,mm6);						//mm5=(3 4 5 6)

			mm1 = mm7;
			mm2 = mm3;
			mm1 = _mm_slli_si64(mm1,2*BYTE_BIT);					//mm1=(- 0 1 2)
			mm2 = _mm_srli_si64(mm2,2*BYTE_BIT);					//mm2=(5 6 7 -)
			mm4 = mm3;
			mm4 = _mm_srli_si64(mm4,(sizeof(MMWORD)-2)*BYTE_BIT);			//mm4=(7 - - -)

			mm1 = _mm_or_si64(mm1,wk[0]);						//mm1=(-1 0 1 2)
			mm2 = _mm_or_si64(mm2,wk[2]);						//mm2=( 5 6 6 8)

			wk[0] = mm4;

			mm7 = _mm_mullo_pi16(mm7,PW_THREE);
			mm3 = _mm_mullo_pi16(mm3,PW_THREE);
			mm1 = _mm_add_pi16(mm1,PW_EIGHT);
			mm5 = _mm_add_pi16(mm5,PW_EIGHT);
			mm0 = _mm_add_pi16(mm0,PW_SEVEN);
			mm2 = _mm_add_pi16(mm2,PW_SEVEN);

			mm1 = _mm_add_pi16(mm1,mm7);
			mm5 = _mm_add_pi16(mm5,mm3);
			mm1 = _mm_srli_pi16(mm1,4);						//mm1=Out0LE=( 0  2  4  6)
			mm5 = _mm_srli_pi16(mm5,4);						//mm5=Out0HE=( 8 10 12 14)
			mm0 = _mm_add_pi16(mm0,mm7);
			mm2 = _mm_add_pi16(mm2,mm3);
			mm0 = _mm_srli_pi16(mm0,4);						//mm0=Out0LO=( 1  3  5  7)
			mm2 = _mm_srli_pi16(mm2,4);						//mm2=Out0HO=( 9 11 13 15)

			mm0 = _mm_slli_pi16(mm0,BYTE_BIT);
			mm2 = _mm_slli_pi16(mm2,BYTE_BIT);
			mm1 = _mm_or_si64(mm1,mm0);						//mm1=Out0L=( 0  1  2  3  4  5  6  7)
			mm5 = _mm_or_si64(mm5,mm2);						//mm5=Out0H=( 8  9 10 11 12 13 14 15)
			
			_mm_store_si64((__m64 *)(outptr0),mm1);
			_mm_store_si64((__m64 *)(outptr0)+1,mm5);

			//process the lower row

			mm6 = _mm_load_si64((__m64 *)(outptr1));		//mm6 = Int1L =( 0 1 2 3)
			mm4 = _mm_load_si64((__m64 *)(outptr1)+1);		//mm4 = Int1H =( 4 5 6 7)

			mm7 = mm6;
			mm3 = mm4;
			mm7 = _mm_srli_si64(mm7,2*BYTE_BIT);			//mm7 = (1 2 3 -) 
			mm3 = _mm_slli_si64(mm3,(sizeof(MMWORD)-2)*BYTE_BIT);		//mm3 = (- - - 4)
			mm0 = mm6;
			mm2 = mm4;
			mm0 = _mm_srli_si64(mm0,(sizeof(MMWORD)-2)*BYTE_BIT);		// mm0 = ( 3 - - - )
			mm2 = _mm_slli_si64(mm2,2*BYTE_BIT);						// mm2 = ( - 4 5 6)

			mm7 = _mm_or_si64(mm7,mm3);			//mm7 = ( 1 2 3 4)
			mm0 = _mm_or_si64(mm0,mm2);			//mm0 = ( 3 4 5 6)

			mm1 = mm6;
			mm5 = mm4;
			mm1 = _mm_slli_si64(mm1,2*BYTE_BIT);		//mm1 = (- 0 1 2)
			mm5 = _mm_srli_si64(mm5,2*BYTE_BIT);		//mm5 = (5 6 7 - )
			mm3 = mm4;
			mm3 = _mm_srli_si64(mm3,(sizeof(MMWORD)-2)*BYTE_BIT);	//mm3 = ( 7 - - - )

			mm1 = _mm_or_si64(mm1,wk[1]);			//mm1 = (-1 0 1 2)
			mm5 = _mm_or_si64(mm5,wk[3]);			//mm5 = ( 5 6 7 8)

			wk[1] = mm3;

			mm6 = _mm_mullo_pi16(mm6,PW_THREE);
			mm4 = _mm_mullo_pi16(mm4,PW_THREE);
			mm1 = _mm_add_pi16(mm1,PW_EIGHT);
			mm0 = _mm_add_pi16(mm0,PW_EIGHT);
			mm7 = _mm_add_pi16(mm7,PW_SEVEN);
			mm5 = _mm_add_pi16(mm5,PW_SEVEN);

			mm1 = _mm_add_pi16(mm1,mm6);
			mm0 = _mm_add_pi16(mm0,mm4);
			mm1 = _mm_srli_pi16(mm1,4);
			mm0 = _mm_srli_pi16(mm0,4);
			mm7 = _mm_add_pi16(mm7,mm6);
			mm5 = _mm_add_pi16(mm5,mm4);
			mm7 = _mm_srli_pi16(mm7,4);
			mm5 = _mm_srli_pi16(mm5,4);

			mm7 = _mm_slli_pi16(mm7,BYTE_BIT);
			mm5 = _mm_slli_pi16(mm5,BYTE_BIT);
			mm1 = _mm_or_si64(mm1,mm7);
			mm0 = _mm_or_si64(mm0,mm5);

            _mm_store_si64((__m64 *)(outptr1),mm1);
            _mm_store_si64((__m64 *)(outptr1)+1,mm0);

			colctr -= sizeof(MMWORD);
			inptr1_a += sizeof(MMWORD);
			inptr0 += sizeof(MMWORD);
			inptr1_b += sizeof(MMWORD);
			outptr0 += 2*sizeof(MMWORD);
			outptr1 += 2*sizeof(MMWORD);

/*			asm(".set noreorder\r\n"
			"subu	%0,%6,8\r\n"
			"daddu	%1,%7,8\r\n"
			"daddu	%2,%8,8\r\n"
			"daddu	%3,%9,8\r\n"
			"daddu	%4,%10,16\r\n"
			"daddu	%5,%11,16\r\n"
			:"=r"(colctr),"=r"(inptr1_a),"=r"(inptr0),"=r"(inptr1_b),"=r"(outptr0),"=r"(outptr1)
			:"r"(colctr),"r"(inptr1_a),"r"(inptr0),"r"(inptr1_b),"r"(outptr0),"r"(outptr1)
			:
			);*/
		}
    colctr = downsampled_width;
	inrow += 1;
	outrow += 2;
//	rowctr -= 2;
	}
}


#define RGBX_FILLER_0XFF 1


#include "jdclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_RGB_RED
#define RGB_GREEN EXT_RGB_GREEN
#define RGB_BLUE EXT_RGB_BLUE
#define RGB_PIXELSIZE EXT_RGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_mmi jsimd_ycc_extrgb_convert_mmi
#include "jdclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_RGBX_RED
#define RGB_GREEN EXT_RGBX_GREEN
#define RGB_BLUE EXT_RGBX_BLUE
#define RGB_PIXELSIZE EXT_RGBX_PIXELSIZE
#define jsimd_ycc_rgb_convert_mmi jsimd_ycc_extrgbx_convert_mmi
#include "jdclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_BGR_RED
#define RGB_GREEN EXT_BGR_GREEN
#define RGB_BLUE EXT_BGR_BLUE
#define RGB_PIXELSIZE EXT_BGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_mmi jsimd_ycc_extbgr_convert_mmi
#include "jdclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_BGRX_RED
#define RGB_GREEN EXT_BGRX_GREEN
#define RGB_BLUE EXT_BGRX_BLUE
#define RGB_PIXELSIZE EXT_BGRX_PIXELSIZE
#define jsimd_ycc_rgb_convert_mmi jsimd_ycc_extbgrx_convert_mmi
#include "jdclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_XBGR_RED
#define RGB_GREEN EXT_XBGR_GREEN
#define RGB_BLUE EXT_XBGR_BLUE
#define RGB_PIXELSIZE EXT_XBGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_mmi jsimd_ycc_extxbgr_convert_mmi
#include "jdclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_XRGB_RED
#define RGB_GREEN EXT_XRGB_GREEN
#define RGB_BLUE EXT_XRGB_BLUE
#define RGB_PIXELSIZE EXT_XRGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_mmi jsimd_ycc_extxrgb_convert_mmi
#include "jdclrmmi_loongson.c"


/*
 * Convert some rows of samples to the JPEG colorspace.
 *
 * Note that we change from the application's interleaved-pixel format
 * to our internal noninterleaved, one-plane-per-component format.
 * The input buffer is therefore three times as wide as the output buffer.
 *
 * A starting row offset is provided only for the output buffer.  The caller
 * can easily adjust the passed input_buf value to accommodate any row
 * offset required on that side.
 */

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED 	0	
#define RGB_GREEN 	1
#define RGB_BLUE 	2
#define RGB_PIXELSIZE	3	
#include "jcclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_RGB_RED
#define RGB_GREEN EXT_RGB_GREEN
#define RGB_BLUE EXT_RGB_BLUE
#define RGB_PIXELSIZE EXT_RGB_PIXELSIZE
#define jsimd_rgb_ycc_convert_mmi jsimd_extrgb_ycc_convert_mmi
#include "jcclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_RGBX_RED
#define RGB_GREEN EXT_RGBX_GREEN
#define RGB_BLUE EXT_RGBX_BLUE
#define RGB_PIXELSIZE EXT_RGBX_PIXELSIZE
#define jsimd_rgb_ycc_convert_mmi jsimd_extrgbx_ycc_convert_mmi
#include "jcclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_BGR_RED
#define RGB_GREEN EXT_BGR_GREEN
#define RGB_BLUE EXT_BGR_BLUE
#define RGB_PIXELSIZE EXT_BGR_PIXELSIZE
#define jsimd_rgb_ycc_convert_mmi jsimd_extbgr_ycc_convert_mmi
#include "jcclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_BGRX_RED
#define RGB_GREEN EXT_BGRX_GREEN
#define RGB_BLUE EXT_BGRX_BLUE
#define RGB_PIXELSIZE EXT_BGRX_PIXELSIZE
#define jsimd_rgb_ycc_convert_mmi jsimd_extbgrx_ycc_convert_mmi
#include "jcclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_XBGR_RED
#define RGB_GREEN EXT_XBGR_GREEN
#define RGB_BLUE EXT_XBGR_BLUE
#define RGB_PIXELSIZE EXT_XBGR_PIXELSIZE
#define jsimd_rgb_ycc_convert_mmi jsimd_extxbgr_ycc_convert_mmi
#include "jcclrmmi_loongson.c"

#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#define RGB_RED EXT_XRGB_RED
#define RGB_GREEN EXT_XRGB_GREEN
#define RGB_BLUE EXT_XRGB_BLUE
#define RGB_PIXELSIZE EXT_XRGB_PIXELSIZE
#define jsimd_rgb_ycc_convert_mmi jsimd_extxrgb_ycc_convert_mmi
#include "jcclrmmi_loongson.c"


/*
 * Quantize/descale the coefficients, and store into coef_blocks[].
 */

GLOBAL(void)
jsimd_quantize_mmi(JCOEFPTR coef_block, DCTELEM * divisors, DCTELEM * workspace)
{
        int i,j;
        JCOEFPTR output_ptr = coef_block;
        int loopcount = 2;
        int loopsize = DCTSIZE2/DCTSIZE/loopcount;
	__m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
	__m64 corr0, corr1, recip0, recip1, scale0, scale1;

	for (i = loopcount; i > 0; i --) {

                for (j = loopsize; j > 0; j --) {

                        mm2 = _mm_load_si64((__m64 *)&workspace[0]);
                        mm3 = _mm_load_si64((__m64 *)&workspace[0 + loopsize]);

			mm0 = mm2;
			mm1 = mm3;

                        mm2 = _mm_srai_pi16(mm2, (WORD_BIT-1));                 	//-1 if value < 0, 0 otherwise
                        mm3 = _mm_srai_pi16(mm3, (WORD_BIT-1));
			
                        mm0 = _mm_xor_si64(mm0, mm2);                                	//val = -val
                        mm1 = _mm_xor_si64(mm1, mm3);
                        mm0 = _mm_sub_pi16(mm0, mm2);
                        mm1 = _mm_sub_pi16(mm1, mm3);
			
                        corr0 = _mm_load_si64((__m64 *)&divisors[DCTSIZE2*1]);    	//correction
                        corr1 = _mm_load_si64((__m64 *)&divisors[DCTSIZE2*1 + loopsize]);

                        mm0 = _mm_add_pi16(mm0, corr0);                               	//correction + roundfactor
                        mm1 = _mm_add_pi16(mm1, corr1);
			
			mm4 = mm0;
			mm5 = mm1;
			
                        recip0 = _mm_load_si64((__m64 *)&divisors[DCTSIZE2*0]);   	// reciprocal
                        recip1 = _mm_load_si64((__m64 *)&divisors[DCTSIZE2*0 + loopsize]);

                        mm0 = _mm_mulhi_pi16(mm0, recip0);
                        mm1 = _mm_mulhi_pi16(mm1, recip1);

			mm0 = _mm_add_pi16(mm0, mm4);                        		//reciprocal is always negative (MSB=1),
			mm1 = _mm_add_pi16(mm1, mm5);                        		//so we always need to add the initial value
									     		//(input value is never negative as we
									     		//inverted it at the start of this routine)

			scale0 = _mm_load_si64((__m64 *)&divisors[DCTSIZE2*2]);    	// scale
                        scale1 = _mm_load_si64((__m64 *)&divisors[DCTSIZE2*2 + loopsize]);

			mm6 = scale0;
			mm7 = scale1;
			mm4 = mm0;
			mm5 = mm1;
			
                        mm0 = _mm_mulhi_pi16(mm0, mm6);
                        mm1 = _mm_mulhi_pi16(mm1, mm7);

                        mm6 = _mm_srai_pi16(mm6, (WORD_BIT-1));             		//determine if scale is negative
                        mm7 = _mm_srai_pi16(mm7, (WORD_BIT-1));

                        mm6 = _mm_and_si64(mm6, mm4);                        		//and add input if it is
                        mm7 = _mm_and_si64(mm7, mm5);
                        mm0 = _mm_add_pi16(mm0, mm6);
                        mm1 = _mm_add_pi16(mm1, mm7);

                        mm4 = _mm_srai_pi16(mm4, (WORD_BIT-1));               		//then check if negative input
                        mm5 = _mm_srai_pi16(mm5, (WORD_BIT-1));

                        mm4 = _mm_and_si64(mm4, scale0);                      		//and add scale if it is
                        mm5 = _mm_and_si64(mm5, scale1);
                        mm0 = _mm_add_pi16(mm0, mm4);
                        mm1 = _mm_add_pi16(mm1, mm5);

                        mm0 = _mm_xor_si64(mm0, mm2);                        		//val = -val
                        mm1 = _mm_xor_si64(mm1, mm3);
                        mm0 = _mm_sub_pi16(mm0, mm2);
                        mm1 = _mm_sub_pi16(mm1, mm3);
			
                        _mm_store_si64((__m64 *)&output_ptr[0], mm0);
                        _mm_store_si64((__m64 *)&output_ptr[0 + loopsize], mm1);

			workspace += DCTSIZE;
                        divisors += DCTSIZE;
                        output_ptr += DCTSIZE;
                        }
                }
}

/*
 * Downsample pixel values of a single component.
 * This version handles the standard case of 2:1 horizontal and 2:1 vertical,
 * without smoothing.
 */

GLOBAL(void)
jsimd_h2v2_downsample_mmi(JDIMENSION image_width, int max_v_samp_factor,
        		  JDIMENSION v_samp_factor, JDIMENSION width_in_blocks,
        		  JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  	int inrow, outrow;
  	JDIMENSION outcol;
  	JDIMENSION output_cols = width_in_blocks << 3;
  	register int bias;
	register JSAMPROW inptr0, inptr1, outptr;
	__m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;

	// expand_right_edge
	register JSAMPROW ptr;
  	register JSAMPLE pixval;
  	register int count;
  	int row;
	output_cols <<= 1;			//output_cols * 2
	JDIMENSION input_cols = image_width;
  	int numcols = (int) (output_cols - input_cols);		
	int num_rows = max_v_samp_factor;
	if (numcols > 0) {
		for (row = 0; row < num_rows; row++) {
			ptr = input_data[row] + image_width;
			pixval = ptr[-1];       // don't need GETJSAMPLE() here	
			for (count = numcols; count > 0; count--)
				*ptr++ = pixval;
		}
	}

	//h2v2_downsample	
	output_cols >>= 1;	
	bias = (1 << 17) + 1;  			//0x00020001
	mm7 = _mm_set1_pi32(bias);		//mm7 = {1,2,1,2} bias = 1,2,1,2,... for successive samples
	mm6 = _mm_cmpeq_pi16(mm6, mm6);		//{FFFF FFFF FFFF FFFF}
	mm6 = _mm_srli_pi16(mm6, BYTE_BIT);	//{0xFF 0x00 0xFF 0x00 ..}
  	for (outrow = v_samp_factor; outrow > 0; outrow --) {
		outptr = output_data[0];
    		inptr0 = input_data[0];
    		inptr1 = input_data[1];
		for (outcol = output_cols; outcol > 0; outcol -= 8) {

			mm0 = _mm_load_si64((__m64 *) &inptr0[0]);
			mm1 = _mm_load_si64((__m64 *) &inptr1[0]);
			mm2 = _mm_load_si64((__m64 *) &inptr0[0 + DCTSIZE]);
			mm3 = _mm_load_si64((__m64 *) &inptr1[0 + DCTSIZE]);
			
			mm4 = mm0;
			mm5 = mm1;
			mm0 = _mm_and_si64(mm0, mm6);
			mm4 = _mm_srli_pi16(mm4, BYTE_BIT);
			mm1 = _mm_and_si64(mm1, mm6);
			mm5 = _mm_srli_pi16(mm5, BYTE_BIT);
			mm0 = _mm_add_pi16(mm0, mm4);	
			mm1 = _mm_add_pi16(mm1, mm5);

			mm4 = mm2;
			mm5 = mm3;
			mm2 = _mm_and_si64(mm2, mm6);
			mm4 = _mm_srli_pi16(mm4, BYTE_BIT);
			mm3 = _mm_and_si64(mm3, mm6);
			mm5 = _mm_srli_pi16(mm5, BYTE_BIT);
			mm2 = _mm_add_pi16(mm2, mm4);	
			mm3 = _mm_add_pi16(mm3, mm5);
			
			mm0 = _mm_add_pi16(mm0, mm1);
			mm2 = _mm_add_pi16(mm2, mm3);
			mm0 = _mm_add_pi16(mm0, mm7);
			mm2 = _mm_add_pi16(mm2, mm7);
			mm0 = _mm_srli_pi16(mm0, 2);
			mm2 = _mm_srli_pi16(mm2, 2);
						
			mm0 = _mm_packs_pu16(mm0, mm2);	

			_mm_store_si64((__m64 *)&outptr[0], mm0);

			inptr0 += 2*DCTSIZE;
			inptr1 += 2*DCTSIZE;
			outptr += 1*DCTSIZE;	
			
		}
	input_data += 2;
	output_data += 1;
	}
}
	
#endif /* DCT_ISLOW_SUPPORTED */
