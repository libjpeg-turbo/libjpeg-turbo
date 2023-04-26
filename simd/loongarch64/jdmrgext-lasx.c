/*
 * LOONGARCH LASX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2021 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Jin Bo (jinbo@loongson.cn)
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

GLOBAL(void)
jsimd_h2v1_merged_upsample_lasx(JDIMENSION output_width, JSAMPIMAGE input_buf,
                                JDIMENSION in_row_group_ctr,
                                JSAMPARRAY output_buf)
{
  register JSAMPROW ptr_y, ptr_cb, ptr_cr;
  register JSAMPROW ptr_rgb;
  register int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION cols;
  __m256i out_r0, out_g0, out_b0;
  __m256i vec_y0, vec_cb, vec_cr, vec_yo, vec_ye;
  __m256i vec_cbh_l, vec_crh_l;

  __m256i const128 = {128};
  __m256i alpha = {0xFF};
  __m256i zero = {0};

  v8i32 const1402 = {FIX_140200, FIX_140200, FIX_140200, FIX_140200, FIX_140200,
                     FIX_140200, FIX_140200, FIX_140200};
  v16i16 const_34414_28586 = {-FIX_34414, FIX_28586, -FIX_34414, FIX_28586,
                    -FIX_34414, FIX_28586, -FIX_34414, FIX_28586, -FIX_34414,
                     FIX_28586, -FIX_34414, FIX_28586, -FIX_34414, FIX_28586,
                    -FIX_34414, FIX_28586};
  v8i32 const1772 = {FIX_177200, FIX_177200, FIX_177200, FIX_177200, FIX_177200,
                     FIX_177200, FIX_177200, FIX_177200};

  alpha = __lasx_xvreplve0_b(alpha);
  const128 = __lasx_xvreplve0_b(const128);

  ptr_y = input_buf[0][in_row_group_ctr];
  ptr_cb = input_buf[1][in_row_group_ctr];
  ptr_cr = input_buf[2][in_row_group_ctr];
  ptr_rgb = output_buf[0];

  for (cols = output_width; cols >= 32; cols -= 32) {
    vec_y0 = LASX_LD(ptr_y);
    vec_cb = LASX_LD(ptr_cb);
    vec_cr = LASX_LD(ptr_cr);

    ptr_y += 32;
    ptr_cb += 16;
    ptr_cr += 16;

    /* Cb = Cb - 128, Cr = Cr - 128 */
    vec_cb = __lasx_xvsub_b(vec_cb, const128);
    vec_cr = __lasx_xvsub_b(vec_cr, const128);

    vec_ye = __lasx_xvpackev_b(zero, vec_y0);
    vec_yo = __lasx_xvpackod_b(zero, vec_y0);
    LASX_UNPCKL_H_B(vec_cb, vec_cbh_l);
    LASX_UNPCKL_H_B(vec_cr, vec_crh_l);

    /* R = Y + 1.40200 * Cr */
    {
      __m256i temp0, out0, out1;

      LASX_UNPCKLH_W_H(vec_crh_l, out1, out0);
      out0 = __lasx_xvmul_w(out0, (__m256i)const1402);
      out1 = __lasx_xvmul_w(out1, (__m256i)const1402);
      out0 = __lasx_xvsrari_w(out0, 16);
      out1 = __lasx_xvsrari_w(out1, 16);
      LASX_PCKEV_H(out1, out0, temp0);
      out0 = __lasx_xvadd_h(temp0, vec_ye);
      out1 = __lasx_xvadd_h(temp0, vec_yo);
      LASX_CLIP_H_0_255_2(out0, out1, out0, out1);
      out_r0 = __lasx_xvpackev_b(out1, out0);
    }

    /* G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */
    {
      __m256i temp0, temp1, out0, out1;

      LASX_ILVLH_H(vec_crh_l, vec_cbh_l, temp1, temp0);
      LASX_DP2_W_H(temp0, (__m256i)const_34414_28586, out0);
      LASX_DP2_W_H(temp1, (__m256i)const_34414_28586, out1);
      out0 = __lasx_xvsrari_w(out0, 16);
      out1 = __lasx_xvsrari_w(out1, 16);
      LASX_PCKEV_H(out1, out0, temp0);
      temp0 = __lasx_xvsub_h(temp0, vec_crh_l);
      out0 = __lasx_xvadd_h(temp0, vec_ye);
      out1 = __lasx_xvadd_h(temp0, vec_yo);
      LASX_CLIP_H_0_255_2(out0, out1, out0, out1);
      out_g0 = __lasx_xvpackev_b(out1, out0);
    }

    /* B = Y + 1.77200 * Cb */
    {
      __m256i temp0, out0, out1;

      LASX_UNPCKLH_W_H(vec_cbh_l, out1, out0);
      out0 = __lasx_xvmul_w(out0, (__m256i)const1772);
      out1 = __lasx_xvmul_w(out1, (__m256i)const1772);
      out0 = __lasx_xvsrari_w(out0, 16);
      out1 = __lasx_xvsrari_w(out1, 16);
      LASX_PCKEV_H(out1, out0, temp0);
      out0 = __lasx_xvadd_h(temp0, vec_ye);
      out1 = __lasx_xvadd_h(temp0, vec_yo);
      LASX_CLIP_H_0_255_2(out0, out1, out0, out1);
      out_b0 = __lasx_xvpackev_b(out1, out0);
    }

    /* Store to memory */
#if RGB_PIXELSIZE == 4

#if RGB_RED == 0
    /* rgbx */
    {
      __m256i out_rg_l, out_rg_h, out_bx_l, out_bx_h;
      __m256i out_rgbx0_l_l, out_rgbx0_l_h, out_rgbx0_h_l, out_rgbx0_h_h;

      LASX_ILVLH_B(out_g0, out_r0, out_rg_h, out_rg_l);
      LASX_ILVLH_B(alpha, out_b0, out_bx_h, out_bx_l);
      LASX_ILVLH_H(out_bx_l, out_rg_l, out_rgbx0_l_h, out_rgbx0_l_l);
      LASX_ILVLH_H(out_bx_h, out_rg_h, out_rgbx0_h_h, out_rgbx0_h_l);

      LASX_ST_4(out_rgbx0_l_l, out_rgbx0_l_h, out_rgbx0_h_l, out_rgbx0_h_h,
                ptr_rgb, 32);
      ptr_rgb += 128;
    }
#endif

#if RGB_RED == 1
    /* xrgb */
    {
      __m256i out_xr_l, out_xr_h, out_gb_l, out_gb_h;
      __m256i out_xrgb0_l_l, out_xrgb0_l_h, out_xrgb0_h_l, out_xrgb0_h_h;

      LASX_ILVLH_B(out_r0, alpha, out_xr_h, out_xr_l);
      LASX_ILVLH_B(out_b0, out_g0, out_gb_h, out_gb_l);
      LASX_ILVLH_H(out_gb_l, out_xr_l, out_xrgb0_l_h, out_xrgb0_l_l);
      LASX_ILVLH_H(out_gb_h, out_xr_h, out_xrgb0_h_h, out_xrgb0_h_l);

      LASX_ST_4(out_xrgb0_l_l, out_xrgb0_l_h, out_xrgb0_h_l, out_xrgb0_h_h,
                ptr_rgb, 32);
      ptr_rgb += 128;
    }
#endif

#if RGB_RED == 2
    /* bgrx */
    {
      __m256i out_bg_l, out_bg_h, out_rx_l, out_rx_h;
      __m256i out_bgrx0_l_l, out_bgrx0_l_h, out_bgrx0_h_l, out_bgrx0_h_h;

      LASX_ILVLH_B(out_g0, out_b0, out_bg_h, out_bg_l);
      LASX_ILVLH_B(alpha, out_r0, out_rx_h, out_rx_l);
      LASX_ILVLH_H(out_rx_l, out_bg_l, out_bgrx0_l_h, out_bgrx0_l_l);
      LASX_ILVLH_H(out_rx_h, out_bg_h, out_bgrx0_h_h, out_bgrx0_h_l);

      LASX_ST_4(out_bgrx0_l_l, out_bgrx0_l_h, out_bgrx0_h_l, out_bgrx0_h_h,
                ptr_rgb, 32);
      ptr_rgb += 128;
    }
#endif

#if RGB_RED == 3
    /* xbgr */
    {
      __m256i out_xb_l, out_xb_h, out_gr_l, out_gr_h;
      __m256i out_xbgr0_l_l, out_xbgr0_l_h, out_xbgr0_h_l, out_xbgr0_h_h;

      LASX_ILVLH_B(out_b0, alpha, out_xb_h, out_xb_l);
      LASX_ILVLH_B(out_r0, out_g0, out_gr_h, out_gr_l);
      LASX_ILVLH_H(out_gr_l, out_xb_l, out_xbgr0_l_h, out_xbgr0_l_l);
      LASX_ILVLH_H(out_gr_h, out_xb_h, out_xbgr0_h_h, out_xbgr0_h_l);

      LASX_ST_4(out_xbgr0_l_l, out_xbgr0_l_h, out_xbgr0_h_l, out_xbgr0_h_h,
                ptr_rgb, 32);
      ptr_rgb += 128;
    }
#endif

#else /* end RGB_PIXELSIZE == 4 */

#if RGB_RED == 0
    /* rgb */
    {
      __m256i out_rg_l, out_rg_h, out_bx_l, out_bx_h;
      __m256i temp0, temp1, temp2, temp3;
      v32i8 mask0 = {0,1,16,2,3,18,4,5,20,6,7,22,8,9,24,10,0,
                     1,16,2,3,18,4,5,20,6,7,22,8,9,24,10};
      v32i8 mask1 = {11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0,
                     11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0};

      LASX_ILVLH_B(out_g0, out_r0, out_rg_h, out_rg_l);
      LASX_ILVLH_B(alpha, out_b0, out_bx_h, out_bx_l);

      temp0 = __lasx_xvshuf_b(out_bx_l, out_rg_l, (__m256i)mask0);
      temp1 = __lasx_xvshuf_b(out_bx_l, out_rg_l, (__m256i)mask1);
      temp2 = __lasx_xvshuf_b(out_bx_h, out_rg_h, (__m256i)mask0);
      temp3 = __lasx_xvshuf_b(out_bx_h, out_rg_h, (__m256i)mask1);

      __lasx_xvstelm_d(temp0, ptr_rgb, 0, 0);
      __lasx_xvstelm_d(temp0, ptr_rgb, 8, 1);
      __lasx_xvstelm_d(temp1, ptr_rgb, 16, 0);
      __lasx_xvstelm_d(temp0, ptr_rgb, 24, 2);
      __lasx_xvstelm_d(temp0, ptr_rgb, 32, 3);
      __lasx_xvstelm_d(temp1, ptr_rgb, 40, 2);
      __lasx_xvstelm_d(temp2, ptr_rgb, 48, 0);
      __lasx_xvstelm_d(temp2, ptr_rgb, 56, 1);
      __lasx_xvstelm_d(temp3, ptr_rgb, 64, 0);
      __lasx_xvstelm_d(temp2, ptr_rgb, 72, 2);
      __lasx_xvstelm_d(temp2, ptr_rgb, 80, 3);
      __lasx_xvstelm_d(temp3, ptr_rgb, 88, 2);
      ptr_rgb += 96;
    }
#endif

#if RGB_RED == 2
    /* bgr */
    {
      __m256i out_bg_l, out_bg_h, out_rx_l, out_rx_h;
      __m256i temp0, temp1, temp2, temp3;
      v32i8 mask0 = {0,1,16,2,3,18,4,5,20,6,7,22,8,9,24,10,0,
                     1,16,2,3,18,4,5,20,6,7,22,8,9,24,10};
      v32i8 mask1 = {11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0,
                     11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0};

      LASX_ILVLH_B(out_g0, out_b0, out_bg_h, out_bg_l);
      LASX_ILVLH_B(alpha, out_r0, out_rx_h, out_rx_l);

      temp0 = __lasx_xvshuf_b(out_rx_l, out_bg_l, (__m256i)mask0);
      temp1 = __lasx_xvshuf_b(out_rx_l, out_bg_l, (__m256i)mask1);
      temp2 = __lasx_xvshuf_b(out_rx_h, out_bg_h, (__m256i)mask0);
      temp3 = __lasx_xvshuf_b(out_rx_h, out_bg_h, (__m256i)mask1);

      __lasx_xvstelm_d(temp0, ptr_rgb, 0, 0);
      __lasx_xvstelm_d(temp0, ptr_rgb, 8, 1);
      __lasx_xvstelm_d(temp1, ptr_rgb, 16, 0);
      __lasx_xvstelm_d(temp0, ptr_rgb, 24, 2);
      __lasx_xvstelm_d(temp0, ptr_rgb, 32, 3);
      __lasx_xvstelm_d(temp1, ptr_rgb, 40, 2);
      __lasx_xvstelm_d(temp2, ptr_rgb, 48, 0);
      __lasx_xvstelm_d(temp2, ptr_rgb, 56, 1);
      __lasx_xvstelm_d(temp3, ptr_rgb, 64, 0);
      __lasx_xvstelm_d(temp2, ptr_rgb, 72, 2);
      __lasx_xvstelm_d(temp2, ptr_rgb, 80, 3);
      __lasx_xvstelm_d(temp3, ptr_rgb, 88, 2);
      ptr_rgb += 96;
    }
#endif

#endif
  }

  for (int i = 0; i < (cols & ~1); i += 2) {
    cb = (int)(*ptr_cb++) - 128;
    cr = (int)(*ptr_cr++) - 128;
    cred = DESCALE(FIX_140200 * cr, 16);
    cgreen = DESCALE(((-FIX_34414) * cb - FIX_71414 * cr), 16);
    cblue = DESCALE(FIX_177200 * cb, 16);

    y = (int)(*ptr_y++);
    ptr_rgb[RGB_RED] = clip_pixel(y + cred);
    ptr_rgb[RGB_GREEN] = clip_pixel(y + cgreen);
    ptr_rgb[RGB_BLUE] = clip_pixel(y + cblue);
#ifdef RGB_ALPHA
    ptr_rgb[RGB_ALPHA] = 0xFF;
#endif
    ptr_rgb += RGB_PIXELSIZE;

    y = (int)(*ptr_y++);
    ptr_rgb[RGB_RED] = clip_pixel(y + cred);
    ptr_rgb[RGB_GREEN] = clip_pixel(y + cgreen);
    ptr_rgb[RGB_BLUE] = clip_pixel(y + cblue);
#ifdef RGB_ALPHA
    ptr_rgb[RGB_ALPHA] = 0xFF;
#endif
    ptr_rgb += RGB_PIXELSIZE;
  }

  if (output_width & 1) {
    cb = (int)(*ptr_cb) - 128;
    cr = (int)(*ptr_cr) - 128;
    cred = DESCALE(FIX_140200 * cr, 16);
    cgreen = DESCALE(((-FIX_34414) * cb - FIX_71414 * cr), 16);
    cblue = DESCALE(FIX_177200 * cb, 16);

    y = (int)(*ptr_y);
    ptr_rgb[RGB_RED] = clip_pixel(y + cred);
    ptr_rgb[RGB_GREEN] = clip_pixel(y + cgreen);
    ptr_rgb[RGB_BLUE] = clip_pixel(y + cblue);
#ifdef RGB_ALPHA
    ptr_rgb[RGB_ALPHA] = 0xFF;
#endif
  }
}

GLOBAL(void)
jsimd_h2v2_merged_upsample_lasx(JDIMENSION output_width,
                                JSAMPIMAGE input_buf,
                                JDIMENSION in_row_group_ctr,
                                JSAMPARRAY output_buf)
{
  JSAMPROW inptr, outptr;

  inptr = input_buf[0][in_row_group_ctr];
  outptr = output_buf[0];

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2];
  jsimd_h2v1_merged_upsample_lasx(output_width, input_buf, in_row_group_ctr,
                                  output_buf);

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2 + 1];
  output_buf[0] = output_buf[1];
  jsimd_h2v1_merged_upsample_lasx(output_width, input_buf, in_row_group_ctr,
                                  output_buf);

  input_buf[0][in_row_group_ctr] = inptr;
  output_buf[0] = outptr;
}
