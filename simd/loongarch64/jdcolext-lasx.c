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
jsimd_ycc_rgb_convert_lasx(JDIMENSION out_width,
                           JSAMPIMAGE input_buf, JDIMENSION input_row,
                           JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW ptr_y, ptr_cb, ptr_cr;
  register JSAMPROW ptr_rgb;
  int y, cb, cr;
  JDIMENSION cols;
  __m256i vec_y, vec_cb, vec_cr;
  __m256i out_r0, out_g0, out_b0;
  __m256i vec_yh_l, vec_yh_h;
  __m256i vec_cbh_l, vec_cbh_h;
  __m256i vec_crh_l, vec_crh_h;
  __m256i const128 = {128};
  __m256i alpha = {0xFF};

  v8i32 const1402 = {FIX_140200, FIX_140200, FIX_140200, FIX_140200, FIX_140200,
                     FIX_140200, FIX_140200, FIX_140200};
  v16i16 const_34414_28586 = {-FIX_34414, FIX_28586, -FIX_34414, FIX_28586,
                    -FIX_34414, FIX_28586, -FIX_34414, FIX_28586, -FIX_34414,
                     FIX_28586, -FIX_34414, FIX_28586, -FIX_34414, FIX_28586,
                    -FIX_34414, FIX_28586};
  v8i32 const1772 = {FIX_177200, FIX_177200, FIX_177200, FIX_177200, FIX_177200,
                     FIX_177200, FIX_177200, FIX_177200};

  alpha    = __lasx_xvreplve0_b(alpha);
  const128 = __lasx_xvreplve0_b(const128);

  while (--num_rows >= 0) {
    ptr_y   = input_buf[0][input_row];
    ptr_cb  = input_buf[1][input_row];
    ptr_cr  = input_buf[2][input_row];
    ptr_rgb = *output_buf++;
    input_row++;

    for (cols = out_width; cols >= 32; cols -= 32) {
      vec_y  = LASX_LD(ptr_y);
      vec_cb = LASX_LD(ptr_cb);
      vec_cr = LASX_LD(ptr_cr);
      ptr_y  += 32;
      ptr_cb += 32;
      ptr_cr += 32;

      /* Cb = Cb - 128, Cr = Cr - 128 */
      vec_cb = __lasx_xvsub_b(vec_cb, const128);
      vec_cr = __lasx_xvsub_b(vec_cr, const128);

      LASX_UNPCKLH_HU_BU(vec_y, vec_yh_h, vec_yh_l);
      LASX_UNPCKLH_H_B(vec_cb, vec_cbh_h, vec_cbh_l);
      LASX_UNPCKLH_H_B(vec_cr, vec_crh_h, vec_crh_l);

      /* R: Y + 1.40200 * Cr */
      {
        __m256i tmp0_m, tmp1_m;
        __m256i out0_m, out1_m, out2_m, out3_m;

        LASX_UNPCKLH_W_H(vec_crh_l, out1_m, out0_m);
        LASX_UNPCKLH_W_H(vec_crh_h, out3_m, out2_m);
        out0_m = __lasx_xvmul_w(out0_m, (__m256i)const1402);
        out1_m = __lasx_xvmul_w(out1_m, (__m256i)const1402);
        out2_m = __lasx_xvmul_w(out2_m, (__m256i)const1402);
        out3_m = __lasx_xvmul_w(out3_m, (__m256i)const1402);

        out0_m = __lasx_xvsrari_w(out0_m, 16);
        out1_m = __lasx_xvsrari_w(out1_m, 16);
        out2_m = __lasx_xvsrari_w(out2_m, 16);
        out3_m = __lasx_xvsrari_w(out3_m, 16);

        LASX_PCKEV_H_2(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);
        tmp0_m = __lasx_xvadd_h(tmp0_m, vec_yh_l);
        tmp1_m = __lasx_xvadd_h(tmp1_m, vec_yh_h);
        LASX_CLIP_H_0_255_2(tmp0_m, tmp1_m, tmp0_m, tmp1_m);
        out_r0 = __lasx_xvpickev_b(tmp1_m, tmp0_m);
        out_r0 = __lasx_xvpermi_d(out_r0, 0xd8);
      }

      /* G = Y - 0.34414 * Cb + 0.28586 * Cr - Cr */
      {
        __m256i tmp0_m, tmp1_m;
        __m256i out0_m, out1_m, out2_m, out3_m;

        LASX_ILVLH_H(vec_crh_l, vec_cbh_l, tmp1_m, tmp0_m);
        LASX_DOTP2_W_H(tmp0_m, (__m256i)const_34414_28586, out0_m);
        LASX_DOTP2_W_H(tmp1_m, (__m256i)const_34414_28586, out1_m);
        out0_m = __lasx_xvsrari_w(out0_m, 16);
        out1_m = __lasx_xvsrari_w(out1_m, 16);

        LASX_ILVLH_H(vec_crh_h, vec_cbh_h, tmp1_m, tmp0_m);
        LASX_DOTP2_W_H(tmp0_m, (__m256i)const_34414_28586, out2_m);
        LASX_DOTP2_W_H(tmp1_m, (__m256i)const_34414_28586, out3_m);
        out2_m = __lasx_xvsrari_w(out2_m, 16);
        out3_m = __lasx_xvsrari_w(out3_m, 16);

        LASX_PCKEV_H_2(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);
        tmp0_m = __lasx_xvadd_h(tmp0_m, vec_yh_l);
        tmp1_m = __lasx_xvadd_h(tmp1_m, vec_yh_h);
        tmp0_m = __lasx_xvsub_h(tmp0_m, vec_crh_l);
        tmp1_m = __lasx_xvsub_h(tmp1_m, vec_crh_h);
        LASX_CLIP_H_0_255_2(tmp0_m, tmp1_m, tmp0_m, tmp1_m);
        out_g0 = __lasx_xvpickev_b(tmp1_m, tmp0_m);
        out_g0 = __lasx_xvpermi_d(out_g0, 0xd8);
      }

      /* B: Y + 1.77200 * Cb */
      {
        __m256i tmp0_m, tmp1_m;
        __m256i out0_m, out1_m, out2_m, out3_m;

        LASX_UNPCKLH_W_H(vec_cbh_l, out1_m, out0_m);
        LASX_UNPCKLH_W_H(vec_cbh_h, out3_m, out2_m);
        out0_m = __lasx_xvmul_w(out0_m, (__m256i)const1772);
        out1_m = __lasx_xvmul_w(out1_m, (__m256i)const1772);
        out2_m = __lasx_xvmul_w(out2_m, (__m256i)const1772);
        out3_m = __lasx_xvmul_w(out3_m, (__m256i)const1772);

        out0_m = __lasx_xvsrari_w(out0_m, 16);
        out1_m = __lasx_xvsrari_w(out1_m, 16);
        out2_m = __lasx_xvsrari_w(out2_m, 16);
        out3_m = __lasx_xvsrari_w(out3_m, 16);

        LASX_PCKEV_H_2(out1_m, out0_m, out3_m, out2_m, tmp0_m, tmp1_m);
        tmp0_m = __lasx_xvadd_h(tmp0_m, vec_yh_l);
        tmp1_m = __lasx_xvadd_h(tmp1_m, vec_yh_h);
        LASX_CLIP_H_0_255_2(tmp0_m, tmp1_m, tmp0_m, tmp1_m);
        out_b0 = __lasx_xvpickev_b(tmp1_m, tmp0_m);
        out_b0 = __lasx_xvpermi_d(out_b0, 0xd8);
      }

      /* Store to memory */

#if RGB_PIXELSIZE == 4

#if RGB_RED == 0
      /* rgbx */
      {
        __m256i out_rg_l, out_rg_h, out_bx_l, out_bx_h;
        __m256i out_rgbx_l_l, out_rgbx_l_h, out_rgbx_h_l, out_rgbx_h_h;
        LASX_ILVLH_B(out_g0, out_r0, out_rg_h, out_rg_l);
        LASX_ILVLH_B(alpha, out_b0, out_bx_h, out_bx_l);
        LASX_ILVLH_H(out_bx_l, out_rg_l, out_rgbx_l_h, out_rgbx_l_l);
        LASX_ILVLH_H(out_bx_h, out_rg_h, out_rgbx_h_h, out_rgbx_h_l);
        LASX_ST_4(out_rgbx_l_l, out_rgbx_l_h, out_rgbx_h_l, out_rgbx_h_h, ptr_rgb, 32);
        ptr_rgb += 128;
      }
#endif

#if RGB_RED == 1
      /* xrgb */
      {
        __m256i out_xr_l, out_xr_h, out_gb_l, out_gb_h;
        __m256i out_xrgb_l_l, out_xrgb_l_h, out_xrgb_h_l, out_xrgb_h_h;
        LASX_ILVLH_B(out_r0, alpha, out_xr_h, out_xr_l);
        LASX_ILVLH_B(out_b0, out_g0, out_gb_h, out_gb_l);
        LASX_ILVLH_H(out_gb_l, out_xr_l, out_xrgb_l_h, out_xrgb_l_l);
        LASX_ILVLH_H(out_gb_h, out_xr_h, out_xrgb_h_h, out_xrgb_h_l);
        LASX_ST_4(out_xrgb_l_l, out_xrgb_l_h, out_xrgb_h_l, out_xrgb_h_h, ptr_rgb, 32);
        ptr_rgb += 128;
      }
#endif

#if RGB_RED == 2
      /* bgrx */
      {
        __m256i out_bg_l, out_bg_h, out_rx_l, out_rx_h;
        __m256i out_bgrx_l_l, out_bgrx_l_h, out_bgrx_h_l, out_bgrx_h_h;
        LASX_ILVLH_B(out_g0, out_b0, out_bg_h, out_bg_l);
        LASX_ILVLH_B(alpha, out_r0, out_rx_h, out_rx_l);
        LASX_ILVLH_H(out_rx_l, out_bg_l, out_bgrx_l_h, out_bgrx_l_l);
        LASX_ILVLH_H(out_rx_h, out_bg_h, out_bgrx_h_h, out_bgrx_h_l);
        LASX_ST_4(out_bgrx_l_l, out_bgrx_l_h, out_bgrx_h_l, out_bgrx_h_h, ptr_rgb, 32);
        ptr_rgb += 128;
      }
#endif

#if RGB_RED == 3
      /* xbgr */
      {
        __m256i out_xb_l, out_xb_h, out_gr_l, out_gr_h;
        __m256i out_xbgr_l_l, out_xbgr_l_h, out_xbgr_h_l, out_xbgr_h_h;
        LASX_ILVLH_B(out_b0, alpha, out_xb_h, out_xb_l);
        LASX_ILVLH_B(out_r0, out_g0, out_gr_h, out_gr_l);
        LASX_ILVLH_H(out_gr_l, out_xb_l, out_xbgr_l_h, out_xbgr_l_l);
        LASX_ILVLH_H(out_gr_h, out_xb_h, out_xbgr_h_h, out_xbgr_h_l);
        LASX_ST_4(out_xbgr_l_l, out_xbgr_l_h, out_xbgr_h_l, out_xbgr_h_h, ptr_rgb, 32);
        ptr_rgb += 128;
      }
#endif

#else /* RGB_PIXELSIZE == 3 */

#if RGB_RED == 0
      /* rgb */
      {
        __m256i out_rg_l, out_rg_h, out_bx_l, out_bx_h;
        __m256i out_rgb_l_l, out_rgb_l_h, out_rgb_h_l;
        __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
        v32i8 mask0 = {0,1,16,2,3,18,4,5,20,6,7,22,8,9,24,10,0,
                       1,16,2,3,18,4,5,20,6,7,22,8,9,24,10};
        v32i8 mask1 = {11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0,
                       11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0};
        LASX_ILVLH_B(out_g0, out_r0, out_rg_h, out_rg_l);
        LASX_ILVLH_B(alpha, out_b0, out_bx_h, out_bx_l);

        tmp0 = __lasx_xvshuf_b(out_bx_l, out_rg_l, (__m256i)mask0);
        tmp1 = __lasx_xvshuf_b(out_bx_l, out_rg_l, (__m256i)mask1);
        tmp1 = __lasx_xvpermi_d(tmp1, 0xd8);
        tmp2 = __lasx_xvpermi_q(tmp0, tmp1, 0x12);
        tmp3 = __lasx_xvpermi_q(tmp0, tmp1, 0x03);
        tmp3 = __lasx_xvpermi_d(tmp3, 0xd2);
        out_rgb_l_l = __lasx_xvpermi_q(tmp2, tmp3, 0x02);
        tmp4 = __lasx_xvpermi_q(tmp2, tmp3, 0x31);

        tmp0 = __lasx_xvshuf_b(out_bx_h, out_rg_h, (__m256i)mask0);
        tmp1 = __lasx_xvshuf_b(out_bx_h, out_rg_h, (__m256i)mask1);
        tmp1 = __lasx_xvpermi_d(tmp1, 0xd8);
        tmp2 = __lasx_xvpermi_q(tmp0, tmp1, 0x12);
        tmp3 = __lasx_xvpermi_q(tmp0, tmp1, 0x03);
        tmp3 = __lasx_xvpermi_d(tmp3, 0xd2);
        tmp5 = __lasx_xvpermi_q(tmp2, tmp3, 0x02);
        tmp6 = __lasx_xvpermi_q(tmp2, tmp3, 0x31);

        out_rgb_l_h = __lasx_xvpermi_q(tmp4, tmp5, 0x02);
        tmp7 = __lasx_xvpermi_q(tmp4, tmp5, 0x13);
        out_rgb_h_l = __lasx_xvpermi_q(tmp6, tmp7, 0x21);
        LASX_ST_2(out_rgb_l_l, out_rgb_l_h, ptr_rgb, 32);
        LASX_ST(out_rgb_h_l, ptr_rgb + 64);
        ptr_rgb += 96;
      }
#endif

#if RGB_RED == 2
      /* bgr */
      {
        __m256i out_bg_l, out_bg_h, out_rx_l, out_rx_h;
        __m256i out_bgr_l_l, out_bgr_l_h, out_bgr_h_l;
        __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
        v32i8 mask0 = {0,1,16,2,3,18,4,5,20,6,7,22,8,9,24,10,0,
                       1,16,2,3,18,4,5,20,6,7,22,8,9,24,10};
        v32i8 mask1 = {11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0,
                       11,26,12,13,28,14,15,30,0,0,0,0,0,0,0,0};
        LASX_ILVLH_B(out_g0, out_b0, out_bg_h, out_bg_l);
        LASX_ILVLH_B(alpha, out_r0, out_rx_h, out_rx_l);

        tmp0 = __lasx_xvshuf_b(out_rx_l, out_bg_l, (__m256i)mask0);
        tmp1 = __lasx_xvshuf_b(out_rx_l, out_bg_l, (__m256i)mask1);
        tmp1 = __lasx_xvpermi_d(tmp1, 0xd8);
        tmp2 = __lasx_xvpermi_q(tmp0, tmp1, 0x12);
        tmp3 = __lasx_xvpermi_q(tmp0, tmp1, 0x03);
        tmp3 = __lasx_xvpermi_d(tmp3, 0xd2);
        out_bgr_l_l = __lasx_xvpermi_q(tmp2, tmp3, 0x02);
        tmp4 = __lasx_xvpermi_q(tmp2, tmp3, 0x31);

        tmp0 = __lasx_xvshuf_b(out_rx_h, out_bg_h, (__m256i)mask0);
        tmp1 = __lasx_xvshuf_b(out_rx_h, out_bg_h, (__m256i)mask1);
        tmp1 = __lasx_xvpermi_d(tmp1, 0xd8);
        tmp2 = __lasx_xvpermi_q(tmp0, tmp1, 0x12);
        tmp3 = __lasx_xvpermi_q(tmp0, tmp1, 0x03);
        tmp3 = __lasx_xvpermi_d(tmp3, 0xd2);
        tmp5 = __lasx_xvpermi_q(tmp2, tmp3, 0x02);
        tmp6 = __lasx_xvpermi_q(tmp2, tmp3, 0x31);

        out_bgr_l_h = __lasx_xvpermi_q(tmp4, tmp5, 0x02);
        tmp7 = __lasx_xvpermi_q(tmp4, tmp5, 0x13);
        out_bgr_h_l = __lasx_xvpermi_q(tmp6, tmp7, 0x21);
        LASX_ST_2(out_bgr_l_l, out_bgr_l_h, ptr_rgb, 32);
        LASX_ST(out_bgr_h_l, ptr_rgb + 64);
        ptr_rgb += 96;
      }
#endif

#endif /* RGB_PIXELSIZE == 3 */
    } /* for (cols = out_width; cols >= 32; cols -= 32) */

    for (int i = 0; i < cols; i++) {
      y  = (int)(ptr_y[i]);
      cb = (int)(ptr_cb[i]) - 128;
      cr = (int)(ptr_cr[i]) - 128;
      ptr_rgb[RGB_RED]   = clip_pixel(y + DESCALE(FIX_140200 * cr, 16));
      ptr_rgb[RGB_GREEN] = clip_pixel(y + DESCALE(((-FIX_34414) * cb - FIX_71414 * cr), 16));
      ptr_rgb[RGB_BLUE]  = clip_pixel(y + DESCALE(FIX_177200 * cb, 16));
#ifdef RGB_ALPHA
      ptr_rgb[RGB_ALPHA] = 0xFF;
#endif
      ptr_rgb += RGB_PIXELSIZE;
    }
  }
}
