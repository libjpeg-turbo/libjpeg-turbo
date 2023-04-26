/*
 * LOONGARCH LSX optimizations for libjpeg-turbo
 *
 * Copyright (C) 2023 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Song Ding (songding@loongson.cn)
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
jsimd_h2v1_merged_upsample_lsx(JDIMENSION output_width, JSAMPIMAGE input_buf,
                               JDIMENSION in_row_group_ctr,
                               JSAMPARRAY output_buf)
{
  register JSAMPROW ptr_y, ptr_cb, ptr_cr;
  register JSAMPROW ptr_rgb;
  register int y, cb, cr, cred, cgreen, cblue;
  JDIMENSION cols;
  __m128i out_r0, out_g0, out_b0;
  __m128i vec_y0, vec_cb, vec_cr, vec_yo, vec_ye;
  __m128i vec_cbh_l, vec_crh_l;

  __m128i const128 = {128};
  __m128i alpha = {0xFF};
  __m128i zero = {0};

  v4i32 const1402 = {FIX_140200, FIX_140200, FIX_140200, FIX_140200};
  v8i16 const_34414_28586 = {-FIX_34414, FIX_28586, -FIX_34414, FIX_28586,
                             -FIX_34414, FIX_28586, -FIX_34414, FIX_28586};
  v4i32 const1772 = {FIX_177200, FIX_177200, FIX_177200, FIX_177200};

  alpha = __lsx_vreplvei_b(alpha, 0);
  const128 = __lsx_vreplvei_b(const128, 0);

  ptr_y = input_buf[0][in_row_group_ctr];
  ptr_cb = input_buf[1][in_row_group_ctr];
  ptr_cr = input_buf[2][in_row_group_ctr];
  ptr_rgb = output_buf[0];

  for (cols = output_width; cols >= 16; cols -= 16) {
    vec_y0 = LSX_LD(ptr_y);
    vec_cb = LSX_LD(ptr_cb);
    vec_cr = LSX_LD(ptr_cr);

    ptr_y += 16;
    ptr_cb += 8;
    ptr_cr += 8;

    /* Cb = Cb - 128, Cr = Cr - 128 */
    vec_cb = __lsx_vsub_b(vec_cb, const128);
    vec_cr = __lsx_vsub_b(vec_cr, const128);

    vec_ye = __lsx_vpackev_b(zero, vec_y0);
    vec_yo = __lsx_vpackod_b(zero, vec_y0);
    LSX_UNPCKL_H_B(vec_cb, vec_cbh_l);
    LSX_UNPCKL_H_B(vec_cr, vec_crh_l);

    /* R = Y + 1.40200 * Cr */
    {
      __m128i temp0, out0, out1;

      LSX_UNPCKLH_W_H(vec_crh_l, out1, out0);
      out0 = __lsx_vmul_w(out0, (__m128i)const1402);
      out1 = __lsx_vmul_w(out1, (__m128i)const1402);
      out0 = __lsx_vsrari_w(out0, 16);
      out1 = __lsx_vsrari_w(out1, 16);
      LSX_PCKEV_H(out1, out0, temp0);
      out0 = __lsx_vadd_h(temp0, vec_ye);
      out1 = __lsx_vadd_h(temp0, vec_yo);
      LSX_CLIP_H_0_255_2(out0, out1, out0, out1);
      out_r0 = __lsx_vpackev_b(out1, out0);
    }

    /* G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */
    {
      __m128i temp0, temp1, out0, out1;

      LSX_ILVLH_H(vec_crh_l, vec_cbh_l, temp1, temp0);
      LSX_DP2_W_H(temp0, (__m128i)const_34414_28586, out0);
      LSX_DP2_W_H(temp1, (__m128i)const_34414_28586, out1);
      out0 = __lsx_vsrari_w(out0, 16);
      out1 = __lsx_vsrari_w(out1, 16);
      LSX_PCKEV_H(out1, out0, temp0);
      temp0 = __lsx_vsub_h(temp0, vec_crh_l);
      out0 = __lsx_vadd_h(temp0, vec_ye);
      out1 = __lsx_vadd_h(temp0, vec_yo);
      LSX_CLIP_H_0_255_2(out0, out1, out0, out1);
      out_g0 = __lsx_vpackev_b(out1, out0);
    }

    /* B = Y + 1.77200 * Cb */
    {
      __m128i temp0, out0, out1;

      LSX_UNPCKLH_W_H(vec_cbh_l, out1, out0);
      out0 = __lsx_vmul_w(out0, (__m128i)const1772);
      out1 = __lsx_vmul_w(out1, (__m128i)const1772);
      out0 = __lsx_vsrari_w(out0, 16);
      out1 = __lsx_vsrari_w(out1, 16);
      LSX_PCKEV_H(out1, out0, temp0);
      out0 = __lsx_vadd_h(temp0, vec_ye);
      out1 = __lsx_vadd_h(temp0, vec_yo);
      LSX_CLIP_H_0_255_2(out0, out1, out0, out1);
      out_b0 = __lsx_vpackev_b(out1, out0);
    }

    /* Store to memory */
#if RGB_PIXELSIZE == 4

#if RGB_RED == 0
    /* rgbx */
    {
      __m128i out_rg_l, out_rg_h, out_bx_l, out_bx_h;
      __m128i out_rgbx0_l_l, out_rgbx0_l_h, out_rgbx0_h_l, out_rgbx0_h_h;

      LSX_ILVLH_B(out_g0, out_r0, out_rg_h, out_rg_l);
      LSX_ILVLH_B(alpha, out_b0, out_bx_h, out_bx_l);
      LSX_ILVLH_H(out_bx_l, out_rg_l, out_rgbx0_l_h, out_rgbx0_l_l);
      LSX_ILVLH_H(out_bx_h, out_rg_h, out_rgbx0_h_h, out_rgbx0_h_l);

      LSX_ST_4(out_rgbx0_l_l, out_rgbx0_l_h, out_rgbx0_h_l, out_rgbx0_h_h,
               ptr_rgb, 16);
      ptr_rgb += 64;
    }
#endif

#if RGB_RED == 1
    /* xrgb */
    {
      __m128i out_xr_l, out_xr_h, out_gb_l, out_gb_h;
      __m128i out_xrgb0_l_l, out_xrgb0_l_h, out_xrgb0_h_l, out_xrgb0_h_h;

      LSX_ILVLH_B(out_r0, alpha, out_xr_h, out_xr_l);
      LSX_ILVLH_B(out_b0, out_g0, out_gb_h, out_gb_l);
      LSX_ILVLH_H(out_gb_l, out_xr_l, out_xrgb0_l_h, out_xrgb0_l_l);
      LSX_ILVLH_H(out_gb_h, out_xr_h, out_xrgb0_h_h, out_xrgb0_h_l);

      LSX_ST_4(out_xrgb0_l_l, out_xrgb0_l_h, out_xrgb0_h_l, out_xrgb0_h_h,
               ptr_rgb, 16);
      ptr_rgb += 64;
    }
#endif

#if RGB_RED == 2
    /* bgrx */
    {
      __m128i out_bg_l, out_bg_h, out_rx_l, out_rx_h;
      __m128i out_bgrx0_l_l, out_bgrx0_l_h, out_bgrx0_h_l, out_bgrx0_h_h;

      LSX_ILVLH_B(out_g0, out_b0, out_bg_h, out_bg_l);
      LSX_ILVLH_B(alpha, out_r0, out_rx_h, out_rx_l);
      LSX_ILVLH_H(out_rx_l, out_bg_l, out_bgrx0_l_h, out_bgrx0_l_l);
      LSX_ILVLH_H(out_rx_h, out_bg_h, out_bgrx0_h_h, out_bgrx0_h_l);

      LSX_ST_4(out_bgrx0_l_l, out_bgrx0_l_h, out_bgrx0_h_l, out_bgrx0_h_h,
               ptr_rgb, 16);
      ptr_rgb += 64;
    }
#endif

#if RGB_RED == 3
    /* xbgr */
    {
      __m128i out_xb_l, out_xb_h, out_gr_l, out_gr_h;
      __m128i out_xbgr0_l_l, out_xbgr0_l_h, out_xbgr0_h_l, out_xbgr0_h_h;

      LSX_ILVLH_B(out_b0, alpha, out_xb_h, out_xb_l);
      LSX_ILVLH_B(out_r0, out_g0, out_gr_h, out_gr_l);
      LSX_ILVLH_H(out_gr_l, out_xb_l, out_xbgr0_l_h, out_xbgr0_l_l);
      LSX_ILVLH_H(out_gr_h, out_xb_h, out_xbgr0_h_h, out_xbgr0_h_l);

      LSX_ST_4(out_xbgr0_l_l, out_xbgr0_l_h, out_xbgr0_h_l, out_xbgr0_h_h,
               ptr_rgb, 16);
      ptr_rgb += 64;
    }
#endif

#else /* end RGB_PIXELSIZE == 4 */

#if RGB_RED == 0
    /* rgb */
    {
      __m128i out_rgb_l_l, out_rgb_l_h, out_rgb_h_l;
      __m128i tmp0, tmp1, tmp2;
      v16i8 mask0 = {0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23};
      v16i8 mask1 = {0,1,16,2,3,17,4,5,18,6,7,19,8,9,20,10};
      v16i8 mask2 = {21,6,22,7,23,8,24,9,25,10,26,11,27,12,28,13};
      v16i8 mask3 = {0,21,1,2,22,3,4,23,5,6,24,7,8,25,9,10};
      v16i8 mask4 = {11,27,12,28,13,29,14,30,15,31,0,0,0,0,0,0};
      v16i8 mask5 = {26,0,1,27,2,3,28,4,5,29,6,7,30,8,9,31};

      tmp0 = __lsx_vshuf_b(out_g0, out_r0, (__m128i)mask0);
      out_rgb_l_l = __lsx_vshuf_b(out_b0, tmp0, (__m128i)mask1);

      tmp1 = __lsx_vshuf_b(out_g0, out_r0, (__m128i)mask2);
      out_rgb_l_h = __lsx_vshuf_b(out_b0, tmp1, (__m128i)mask3);

      tmp2 = __lsx_vshuf_b(out_g0, out_r0, (__m128i)mask4);
      out_rgb_h_l = __lsx_vshuf_b(out_b0, tmp2, (__m128i)mask5);

      LSX_ST_2(out_rgb_l_l, out_rgb_l_h, ptr_rgb, 16);
      LSX_ST(out_rgb_h_l, ptr_rgb + 32);
      ptr_rgb += 48;
    }
#endif

#if RGB_RED == 2
    /* bgr */
    {
      __m128i out_bgr_l_l, out_bgr_l_h, out_bgr_h_l;
      __m128i tmp0, tmp1, tmp2;
      v16i8 mask0 = {0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23};
      v16i8 mask1 = {0,1,16,2,3,17,4,5,18,6,7,19,8,9,20,10};
      v16i8 mask2 = {21,6,22,7,23,8,24,9,25,10,26,11,27,12,28,13};
      v16i8 mask3 = {0,21,1,2,22,3,4,23,5,6,24,7,8,25,9,10};
      v16i8 mask4 = {11,27,12,28,13,29,14,30,15,31,0,0,0,0,0,0};
      v16i8 mask5 = {26,0,1,27,2,3,28,4,5,29,6,7,30,8,9,31};

      tmp0 = __lsx_vshuf_b(out_g0, out_b0, (__m128i)mask0);
      out_bgr_l_l = __lsx_vshuf_b(out_r0, tmp0, (__m128i)mask1);

      tmp1 = __lsx_vshuf_b(out_g0, out_b0, (__m128i)mask2);
      out_bgr_l_h = __lsx_vshuf_b(out_r0, tmp1, (__m128i)mask3);

      tmp2 = __lsx_vshuf_b(out_g0, out_b0, (__m128i)mask4);
      out_bgr_h_l = __lsx_vshuf_b(out_r0, tmp2, (__m128i)mask5);

      LSX_ST_2(out_bgr_l_l, out_bgr_l_h, ptr_rgb, 16);
      LSX_ST(out_bgr_h_l, ptr_rgb + 32);
      ptr_rgb += 48;
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
jsimd_h2v2_merged_upsample_lsx(JDIMENSION output_width,
                               JSAMPIMAGE input_buf,
                               JDIMENSION in_row_group_ctr,
                               JSAMPARRAY output_buf)
{
  JSAMPROW inptr, outptr;

  inptr = input_buf[0][in_row_group_ctr];
  outptr = output_buf[0];

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2];
  jsimd_h2v1_merged_upsample_lsx(output_width, input_buf, in_row_group_ctr,
                                 output_buf);

  input_buf[0][in_row_group_ctr] = input_buf[0][in_row_group_ctr * 2 + 1];
  output_buf[0] = output_buf[1];
  jsimd_h2v1_merged_upsample_lsx(output_width, input_buf, in_row_group_ctr,
                                 output_buf);

  input_buf[0][in_row_group_ctr] = inptr;
  output_buf[0] = outptr;
}
