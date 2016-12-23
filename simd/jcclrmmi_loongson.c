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

#if RGB_RED == 0
#define  mmA  mm0
#define  mmB  mm1
#elif RGB_GREEN == 0
#define  mmA  mm2
#define  mmB  mm3
#elif RGB_BLUE == 0
#define  mmA  mm4
#define  mmB  mm5
#else
#define  mmA  mm6
#define  mmB  mm7
#endif

#if RGB_RED == 1
#define  mmC  mm0
#define  mmD  mm1
#elif RGB_GREEN == 1
#define  mmC  mm2
#define  mmD  mm3
#elif RGB_BLUE == 1
#define  mmC  mm4
#define  mmD  mm5
#else
#define  mmC  mm6
#define  mmD  mm7
#endif

#if RGB_RED == 2
#define  mmE  mm0
#define  mmF  mm1
#elif RGB_GREEN == 2
#define  mmE  mm2
#define  mmF  mm3
#elif RGB_BLUE == 2
#define  mmE  mm4
#define  mmF  mm5
#else
#define  mmE  mm6
#define  mmF  mm7
#endif

#if RGB_RED == 3
#define  mmG  mm0
#define  mmH  mm1
#elif RGB_GREEN == 3
#define  mmG  mm2
#define  mmH  mm3
#elif RGB_BLUE == 3
#define  mmG  mm4
#define  mmH  mm5
#else
#define  mmG  mm6
#define  mmH  mm7
#endif

											 
GLOBAL(void)							    			 
jsimd_rgb_ycc_convert_mmi (JDIMENSION image_width,					 
			   JSAMPARRAY input_buf, JSAMPIMAGE output_buf, 		 
			   JDIMENSION output_row, int num_rows)				 
											 
{											 
	register JSAMPROW inptr;							 
  	register JSAMPROW outptr0, outptr1, outptr2;					 
	register int col;							 
  	JDIMENSION num_cols = image_width;
	JDIMENSION col_tmp;						 
        register __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;				 
	__m64  wk[7];			 
	__m64  Y_BG, Cb_RG, Cr_BG;	 
	while (--num_rows >= 0) {							 
      		inptr = *input_buf++;							 
      		outptr0 = output_buf[0][output_row];					 
      		outptr1 = output_buf[1][output_row];					 
      		outptr2 = output_buf[2][output_row];					 
		output_row ++;								 
											 
		for (col = num_cols; col > 0; col -= 8 ) { 				 
#if RGB_PIXELSIZE == 3
				if (col < DCTSIZE) {
					col_tmp = col*3;
#if 1
					asm(".set noreorder\r\n"	
					
					"li     $8, 1\r\n"
					"move   $9, %3\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10,1f\r\n"
					"nop    \r\n"
					"subu   $9, $9, 1\r\n"
					"xor    $12, $12, $12\r\n"
					"move	$13, %5\r\n"
					"dadd	$13, $13, $9\r\n"
					"lbu	$12, 0($13)\r\n"
						
					"1:     li     $8, 2\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10, 2f\r\n"
					"nop    \r\n"
					"subu   $9,$9,2\r\n"
					"xor    $11, $11, $11\r\n"
					"move	$13, %5\r\n"
					"dadd	$13, $13, $9\r\n" 
					"lhu    $11, 0($13)\r\n"
					"sll    $12, $12, 16\r\n"
					"or     $12, $12, $11\r\n"

					"2:     \r\n"
					"dmtc1	$12, %0\r\n"
					"li     $8, 4\r\n" 
					"and    $10, $9, $8\r\n"
					"beqz   $10, 3f\r\n"
					"nop    \r\n"
					"subu   $9, $9, 4\r\n"
					"move	$13, %5\r\n"
					"dadd	$13, $13, $9\r\n"
					"lwu   	$14, 0($13)\r\n"
					"dmtc1	$14, %1\r\n"
					"dsll32 $12, $12, 0\r\n"
					"or     $12, $12, $14\r\n"
					"dmtc1	$12, %0\r\n"
					
					"3:	li     $8, 8\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10, 4f\r\n"
					"nop    \r\n"
					"mov.s  %1, %0\r\n"
					"ldc1   %0, 0(%5)\r\n"
					"li     $9, 8\r\n"
					"j     5f\r\n"
					"nop    \r\n"
					
					"4:	li     $8, 16\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10, 5f\r\n"
					"nop    \r\n"
					"mov.s  %2, %0\r\n"
					"ldc1   %0, 0(%5)\r\n"
					"ldc1   %1, 8(%5)\r\n"
					"5:	nop\r\n"
					".set reorder\r\n"	
				/*		
				*/	
					:"=f"(mmA), "=f"(mmG), "=f"(mmF)         
                  			:"r"(col_tmp), "r"(num_rows), "r"(inptr) 
                  			:"$f0", "$f2","$f4", "$8", "$9", "$10", "$11","$12","$13","$14", "memory"      
					);       
#endif
#if 0
					unsigned char cwwtmp[24]={0};
					int i;
					for(i=0; i< col_tmp; i++)
						cwwtmp[i]=inptr[i];
			 		
					mmA = _mm_load_si64((__m64 *)&cwwtmp[0 + 0*DCTSIZE]);	 
					mmG = _mm_load_si64((__m64 *)&cwwtmp[0 + 1*DCTSIZE]);	 
	      				mmF = _mm_load_si64((__m64 *)&cwwtmp[0 + 2*DCTSIZE]);	 
#endif
				}
				else
				{	
			 		mmA = _mm_load_si64((__m64 *)&inptr[0 + 0*DCTSIZE]);	 
					mmG = _mm_load_si64((__m64 *)&inptr[0 + 1*DCTSIZE]);	 
	      				mmF = _mm_load_si64((__m64 *)&inptr[0 + 2*DCTSIZE]);	 
	    				inptr += RGB_PIXELSIZE*DCTSIZE;					 
				}									 
				mmD = mmA;							 
				mmA = _mm_slli_si64(mmA, 4*BYTE_BIT);		 
	      			mmD = _mm_srli_si64(mmD, 4*BYTE_BIT);		 
												 
	      			mmA = _mm_unpackhi_pi8(mmA, mmG);		 
	      			mmG = _mm_slli_si64(mmG, 4*BYTE_BIT);				 
												 
	      			mmD = _mm_unpacklo_pi8(mmD, mmF);		 
	      			mmG = _mm_unpackhi_pi8(mmG, mmF);				 	 
												 
				mmE = mmA;							 
	      			mmA = _mm_slli_si64(mmA, 4*BYTE_BIT); 	 
	      			mmE = _mm_srli_si64(mmE, 4*BYTE_BIT);	 
													 
	      			mmA = _mm_unpackhi_pi8(mmA, mmD);	 
	      			mmD = _mm_slli_si64(mmD, 4*BYTE_BIT);	 
										 
	      			mmE = _mm_unpacklo_pi8(mmE, mmG);		 
	      			mmD = _mm_unpackhi_pi8(mmD, mmG);		 
				mmC = mmA;					 
	      			mmA = _mm_loadlo_pi8_f(mmA);		 
	      			mmC = _mm_loadhi_pi8_f(mmC);		 
										 
				mmB = mmE;					 
	      			mmE = _mm_loadlo_pi8_f(mmE);		 
	      			mmB = _mm_loadhi_pi8_f(mmB);		 
										 
				mmF = mmD;					 
	      			mmD = _mm_loadlo_pi8_f(mmD);		 
	      			mmF = _mm_loadhi_pi8_f(mmF);		 
				 
#else	//RGB_PIXELSIZE == 4				
			
				if (col < DCTSIZE){
#if 1
					col_tmp = col;
					asm(".set noreorder\r\n"
					"li     $8, 1\r\n"
					"move   $9, %4\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10, 1f\r\n"
					"nop    \r\n"
					"subu   $9, $9, 1\r\n"
					"dsll	$11, $9, 2\r\n"
					"move	$13, %5\r\n"
					"daddu	$13, $13, $11\r\n"
					"lwc1   %0, 0($13)\r\n"
 
					"1:	li     $8, 2\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10, 2f\r\n"
					"nop    \r\n"
					"subu   $9, $9, 2\r\n"
					"dsll	$11, $9, 2\r\n"
					"move	$13, %5\r\n"
					"daddu 	$13, $13, $11\r\n"
					"mov.s  %1, %0\r\n"
					"ldc1   %0, 0($13)\r\n"
						
					"2:	li     $8, 4\r\n"
					"and    $10, $9, $8\r\n"
					"beqz   $10, 3f\r\n"
					"nop    \r\n"
					"mov.s  %2, %0\r\n"
					"mov.s  %3, %1\r\n"
					"ldc1   %0, 0(%5)\r\n"
					"ldc1   %1, 8(%5)\r\n"
					"3:	nop	\r\n"
					".set reorder\r\n"	
					/*
					*/	
					:"=f"(mmA) ,"=f"(mmF), "=f"(mmD), "=f"(mmC)
					:"r"(col_tmp), "r"(inptr)
					:"$f0", "$f2", "$8", "$9", "$10","$11","$13", "memory"
					);	
#endif 					
#if 0
					unsigned char cwwtmp[32]={0};
					int i;

					col_tmp = col*4;

					for(i=0; i< col_tmp; i++)
						cwwtmp[i]=inptr[i];

					mmA = _mm_load_si64((__m64 *)&cwwtmp[0 + 0*DCTSIZE]);        
	                                mmF = _mm_load_si64((__m64 *)&cwwtmp[0 + 1*DCTSIZE]);        
	                                mmD = _mm_load_si64((__m64 *)&cwwtmp[0 + 2*DCTSIZE]);        
	                                mmC = _mm_load_si64((__m64 *)&cwwtmp[0 + 3*DCTSIZE]);        
#endif
				}
				else
				{
					mmA = _mm_load_si64((__m64 *)&inptr[0 + 0*DCTSIZE]);        
                                	mmF = _mm_load_si64((__m64 *)&inptr[0 + 1*DCTSIZE]);        
                                	mmD = _mm_load_si64((__m64 *)&inptr[0 + 2*DCTSIZE]);        
                                	mmC = _mm_load_si64((__m64 *)&inptr[0 + 3*DCTSIZE]);        
	    				inptr += RGB_PIXELSIZE*DCTSIZE;					 
					
                                }                                                               
                                mmB = mmA;                                                           
                                mmA = _mm_unpacklo_pi8(mmA, mmF);                  
                                mmB = _mm_unpackhi_pi8(mmB, mmF);                  
                                                                                                
                                mmG = mmD;                                                           
                                mmD = _mm_unpacklo_pi8(mmD, mmC);                  
                                mmG = _mm_unpackhi_pi8(mmG, mmC);                  
                                                                                                
                                mmE = mmA;                                                           
                                mmA = _mm_unpacklo_pi16(mmA, mmD);       
                                mmE = _mm_unpackhi_pi16(mmE, mmD);       
                                                                                                                
                                mmH = mmB;                                                           
                                mmB = _mm_unpacklo_pi16(mmB, mmG);       
                                mmH = _mm_unpackhi_pi16(mmH, mmG);       
                                                                                                                
                                mmC = mmA;                                                           
                                mmA = _mm_loadlo_pi8_f(mmA);                       
                                mmC = _mm_loadhi_pi8_f(mmC);                       
                                                                                                                
                                mmD = mmB;                                                           
                                mmB = _mm_loadlo_pi8_f(mmB);                       
                                mmD = _mm_loadhi_pi8_f(mmD);                       
                                                                                                                
                                mmG = mmE;                                                           
                                mmE = _mm_loadlo_pi8_f(mmE);                       
                                mmG = _mm_loadhi_pi8_f(mmG);                       
                                                                                                                
                                mmF = mmH;                                                           
                                mmF = _mm_unpacklo_pi8(mmF, mmH);       
                                mmH = _mm_unpackhi_pi8(mmH, mmH);       
                                mmF = _mm_srli_pi16(mmF, BYTE_BIT);               
                                mmH = _mm_srli_pi16(mmH, BYTE_BIT);               
#endif									
												
			wk[0] = mm0;					 
			wk[1] = mm1;					 
			wk[2] = mm4;					 
			wk[3] = mm5;					 
										 
			mm6 = mm1;						 
			mm1 = _mm_unpacklo_pi16(mm1, mm3); 				 
	    		mm6 = _mm_unpackhi_pi16(mm6, mm3); 				 
			mm7 = mm1;						 
			mm4 = mm6;						 
	    		mm1 = _mm_madd_pi16(mm1, PW_F0299_F0337);				 
	   		mm6 = _mm_madd_pi16(mm6, PW_F0299_F0337);				 
	    		mm7 = _mm_madd_pi16(mm7, PW_MF016_MF033);				 
	    		mm4 = _mm_madd_pi16(mm4, PW_MF016_MF033);		 
											 
			wk[4] = mm1;					 
			wk[5] = mm6;					 
										 
	    		mm1 = _mm_loadlo_pi16_f(mm5);			 
	    		mm6 = _mm_loadhi_pi16_f(mm5);			 
	    		mm1 = _mm_srli_pi32(mm1, 1);					 
	    		mm6 = _mm_srli_pi32(mm6, 1);					 
											 
			mm5 = PD_ONEHALFM1_CJ;								 
	    		mm7 = _mm_add_pi32(mm7, mm1);				 
	    		mm4 = _mm_add_pi32(mm4, mm6);				 
	    		mm7 = _mm_add_pi32(mm7, mm5);		 
	    		mm4 = _mm_add_pi32(mm4, mm5);		 
	    		mm7 = _mm_srli_pi32(mm7, SCALEBITS);			 
	    		mm4 = _mm_srli_pi32(mm4, SCALEBITS);			 
	    		mm7 = _mm_packs_pi32(mm7, mm4);		 
											 
			mm1 = wk[2];									 
			mm6 = mm0;									 
	   		mm0 = _mm_unpacklo_pi16(mm0, mm2); 	 
	   		mm6 = _mm_unpackhi_pi16(mm6, mm2); 	 
			mm5 = mm0; 									 
			mm4 = mm6;									 
	   		mm0 = _mm_madd_pi16(mm0, PW_F0299_F0337);	 
	    		mm6 = _mm_madd_pi16(mm6, PW_F0299_F0337);	 
	    		mm5 = _mm_madd_pi16(mm5, PW_MF016_MF033);	 
	    		mm4 = _mm_madd_pi16(mm4, PW_MF016_MF033);	 
											 
			wk[6] = mm0;						 
			wk[7] = mm6;						 
			mm0 = _mm_loadlo_pi16_f(mm1);			 
			mm6 = _mm_loadhi_pi16_f(mm1);			 
	    		mm0 = _mm_srli_pi32(mm0, 1);				 	 
	    		mm6 = _mm_srli_pi32(mm6, 1);					 
											 
		        mm1 = PD_ONEHALFM1_CJ;									 
	    		mm5 = _mm_add_pi32(mm5, mm0);				 
	    		mm4 = _mm_add_pi32(mm4, mm6);				 
	    		mm5 = _mm_add_pi32(mm5, mm1);		 
	    		mm4 = _mm_add_pi32(mm4, mm1);		 
	    		mm5 = _mm_srli_pi32(mm5, SCALEBITS);			 
	    		mm4 = _mm_srli_pi32(mm4, SCALEBITS);			 
	    		mm5 = _mm_packs_pi32(mm5, mm4);		 
											 
	    		mm7 = _mm_slli_pi16(mm7, BYTE_BIT);			 
	    		mm5  = _mm_or_si64(mm5, mm7);			 
			Cb_RG = mm5;						 
											 
			mm0 = wk[3];									 
		        mm6 = wk[2];									 
		        mm1 = wk[1];									 
											 
		        mm4 = mm0;									 
	    		mm0 = _mm_unpacklo_pi16(mm0, mm3); 	 
	    		mm4 = _mm_unpackhi_pi16(mm4, mm3); 	 
		        mm7 = mm0;									 
		        mm5 = mm4;									 
	    		mm0 = _mm_madd_pi16(mm0, PW_F0114_F0250);	 
	    		mm4 = _mm_madd_pi16(mm4, PW_F0114_F0250);	 
	    		mm7 = _mm_madd_pi16(mm7, PW_MF008_MF041);	 
	    		mm5 = _mm_madd_pi16(mm5, PW_MF008_MF041);	 
										 
			mm3 = PD_ONEHALF;								 
	    		mm0 = _mm_add_pi32(mm0, wk[4]);				 
	    		mm4 = _mm_add_pi32(mm4, wk[5]);				 
	    		mm0 = _mm_add_pi32(mm0, mm3);			 
	    		mm4 = _mm_add_pi32(mm4, mm3);			 
	    		mm0 = _mm_srli_pi32(mm0, SCALEBITS);			 
	    		mm4 = _mm_srli_pi32(mm4, SCALEBITS);			 
	    		mm0 = _mm_packs_pi32(mm0,mm4);			 
											 
	    		mm3 = _mm_loadlo_pi16_f(mm1);			 
	    		mm4 = _mm_loadhi_pi16_f(mm1);			 
	    		mm3 = _mm_srli_pi32(mm3, 1);				 
	    		mm4 = _mm_srli_pi32(mm4, 1);				 
										 
			mm1 = PD_ONEHALFM1_CJ;					 
	    		mm7 = _mm_add_pi32(mm7, mm3);				 
	    		mm5 = _mm_add_pi32(mm5, mm4);				 
	    		mm7 = _mm_add_pi32(mm7, mm1);		 
	    		mm5 = _mm_add_pi32(mm5, mm1);		 
	    		mm7 = _mm_srli_pi32(mm7, SCALEBITS);			 
	    		mm5 = _mm_srli_pi32(mm5, SCALEBITS);			 
	    		mm7 = _mm_packs_pi32(mm7, mm5);		 
											 
			mm3 = wk[0]; 								 
			mm4 = mm6; 								 
	    		mm6 = _mm_unpacklo_pi16(mm6, mm2); 	 
	    		mm4 = _mm_unpackhi_pi16(mm4, mm2); 	 
			mm1 = mm6; 								 
			mm5 = mm4; 								 
	    		mm6 = _mm_madd_pi16(mm6, PW_F0114_F0250);	 
	    		mm4 = _mm_madd_pi16(mm4, PW_F0114_F0250);	 
	    		mm1 = _mm_madd_pi16(mm1, PW_MF008_MF041);	 
	    		mm5 = _mm_madd_pi16(mm5, PW_MF008_MF041);	 
											 
			mm2 = PD_ONEHALF;								 
			mm6 = _mm_add_pi32(mm6, wk[6]);				 
			mm4 = _mm_add_pi32(mm4, wk[7]);				 
			mm6 = _mm_add_pi32(mm6, mm2);			 
			mm4 = _mm_add_pi32(mm4, mm2);			 
			mm6 = _mm_srli_pi32(mm6, SCALEBITS);			 
			mm4 = _mm_srli_pi32(mm4, SCALEBITS);			 
			mm6 = _mm_packs_pi32(mm6, mm4);		 	 
											 
	    		mm0 = _mm_slli_pi16(mm0, BYTE_BIT);				 
	    		mm6 = _mm_or_si64(mm6, mm0);				 
			Y_BG = mm6;								 
											 
	    		mm2 = _mm_loadlo_pi16_f(mm3);			 
	    		mm4 = _mm_loadhi_pi16_f(mm3);			 
	    		mm2 = _mm_srli_pi32(mm2, 1);					 
	    		mm4 = _mm_srli_pi32(mm4, 1);					 
											 
			mm0 = PD_ONEHALFM1_CJ;								 
	    		mm1 = _mm_add_pi32(mm1, mm2);				 
	    		mm5 = _mm_add_pi32(mm5, mm4);				 
	    		mm1 = _mm_add_pi32(mm1, mm0);		 
	    		mm5 = _mm_add_pi32(mm5, mm0);		 
	    		mm1 = _mm_srli_pi32(mm1, SCALEBITS);			 
	    		mm5 = _mm_srli_pi32(mm5, SCALEBITS);			 
	    		mm1 = _mm_packs_pi32(mm1, mm5);		 
	    										 
	    		mm7 = _mm_slli_pi16(mm7, BYTE_BIT);			 
	    		mm1 = _mm_or_si64(mm1, mm7);			 
	    		Cr_BG = mm1;								 
	    										 
	    		_mm_store_si64((__m64 *)&outptr0[0], Y_BG);			 
	    		_mm_store_si64((__m64 *)&outptr1[0], Cb_RG);			 
	    		_mm_store_si64((__m64 *)&outptr2[0], Cr_BG);			 
											 
	    		outptr0 += DCTSIZE;						 
	    		outptr1 += DCTSIZE;						 
	    		outptr2 += DCTSIZE;						 
		}									 
											 
	}										 
}										 
#undef mmA
#undef mmB
#undef mmC
#undef mmD
#undef mmE
#undef mmF
#undef mmG
#undef mmH

