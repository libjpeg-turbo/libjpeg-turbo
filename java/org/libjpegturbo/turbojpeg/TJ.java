/*
 * Copyright (C)2011 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

package org.libjpegturbo.turbojpeg;

/**
 * TurboJPEG utility class (cannot be instantiated)
 */
final public class TJ {


  /**
   * The number of chrominance subsampling options
   */
  final public static int NUMSAMP   = 5;
  /**
   * 4:4:4 chrominance subsampling (no chrominance subsampling).  The JPEG
   * or YUV image will contain one chrominance component for every pixel in the
   * source image.
   */
  final public static int SAMP_444  = 0;
  /**
   * 4:2:2 chrominance subsampling.  The JPEG or YUV image will contain one
   * chrominance component for every 2x1 block of pixels in the source image.
   */
  final public static int SAMP_422  = 1;
  /**
   * 4:2:0 chrominance subsampling.  The JPEG or YUV image will contain one
   * chrominance component for every 2x2 block of pixels in the source image.
   */
  final public static int SAMP_420  = 2;
  /**
   * Grayscale.  The JPEG or YUV image will contain no chrominance components.
   */
  final public static int SAMP_GRAY = 3;
  /**
   * 4:4:0 chrominance subsampling.  The JPEG or YUV image will contain one
   * chrominance component for every 1x2 block of pixels in the source image.
   */
  final public static int SAMP_440  = 4;


  /**
   * Returns the MCU block width for the given level of chrominance
   * subsampling.
   *
   * @param subsamp the level of chrominance subsampling (one of
   * <code>SAMP_*</code>)
   *
   * @return the MCU block width for the given level of chrominance subsampling
   */
  public static int getMCUWidth(int subsamp) throws Exception {
    if(subsamp < 0 || subsamp >= NUMSAMP)
      throw new Exception("Invalid subsampling type");
    return mcuWidth[subsamp];
  }

  final private static int mcuWidth[] = {
    8, 16, 16, 8, 8
  };


  /**
   * Returns the MCU block height for the given level of chrominance
   * subsampling.
   *
   * @param subsamp the level of chrominance subsampling (one of
   * <code>SAMP_*</code>)
   *
   * @return the MCU block height for the given level of chrominance
   * subsampling
   */
  public static int getMCUHeight(int subsamp) throws Exception {
    if(subsamp < 0 || subsamp >= NUMSAMP)
      throw new Exception("Invalid subsampling type");
    return mcuHeight[subsamp];
  }

  final private static int mcuHeight[] = {
    8, 8, 16, 8, 16
  };


  /**
   * The number of pixel formats
   */
  final public static int NUMPF   = 11;
  /**
   * RGB pixel format.  The red, green, and blue components in the image are
   * stored in 3-byte pixels in the order R, G, B from lowest to highest byte
   * address within each pixel.
   */
  final public static int PF_RGB  = 0;
  /**
   * BGR pixel format.  The red, green, and blue components in the image are
   * stored in 3-byte pixels in the order B, G, R from lowest to highest byte
   * address within each pixel.
   */
  final public static int PF_BGR  = 1;
  /**
   * RGBX pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order R, G, B from lowest to highest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  final public static int PF_RGBX = 2;
  /**
   * BGRX pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order B, G, R from lowest to highest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  final public static int PF_BGRX = 3;
  /**
   * XBGR pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order R, G, B from highest to lowest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  final public static int PF_XBGR = 4;
  /**
   * XRGB pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order B, G, R from highest to lowest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  final public static int PF_XRGB = 5;
  /**
   * Grayscale pixel format.  Each 1-byte pixel represents a luminance
   * (brightness) level from 0 to 255.
   */
  final public static int PF_GRAY = 6;
  /**
   * RGBA pixel format.  This is the same as {@link #PF_RGBX}, except that when
   * decompressing, the X byte is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  final public static int PF_RGBA = 7;
  /**
   * BGRA pixel format.  This is the same as {@link #PF_BGRX}, except that when
   * decompressing, the X byte is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  final public static int PF_BGRA = 8;
  /**
   * ABGR pixel format.  This is the same as {@link #PF_XBGR}, except that when
   * decompressing, the X byte is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  final public static int PF_ABGR = 9;
  /**
   * ARGB pixel format.  This is the same as {@link #PF_XRGB}, except that when
   * decompressing, the X byte is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  final public static int PF_ARGB = 10;


  /**
   * Returns the pixel size (in bytes) of the given pixel format.
   *
   * @param pixelFormat the pixel format (one of <code>PF_*</code>)
   *
   * @return the pixel size (in bytes) of the given pixel format
   */
  public static int getPixelSize(int pixelFormat) throws Exception {
    if(pixelFormat < 0 || pixelFormat >= NUMPF)
      throw new Exception("Invalid pixel format");
    return pixelSize[pixelFormat];
  }

  final private static int pixelSize[] = {
    3, 3, 4, 4, 4, 4, 1, 4, 4, 4, 4
  };


  /**
   * For the given pixel format, returns the number of bytes that the red
   * component is offset from the start of the pixel.  For instance, if a pixel
   * of format <code>TJ.PF_BGRX</code> is stored in <code>char pixel[]</code>,
   * then the red component will be
   * <code>pixel[TJ.getRedOffset(TJ.PF_BGRX)]</code>.
   *
   * @param pixelFormat the pixel format (one of <code>PF_*</code>)
   *
   * @return the red offset for the given pixel format
   */
  public static int getRedOffset(int pixelFormat) throws Exception {
    if(pixelFormat < 0 || pixelFormat >= NUMPF)
      throw new Exception("Invalid pixel format");
    return redOffset[pixelFormat];
  }

  final private static int redOffset[] = {
    0, 2, 0, 2, 3, 1, 0, 0, 2, 3, 1
  };


  /**
   * For the given pixel format, returns the number of bytes that the green
   * component is offset from the start of the pixel.  For instance, if a pixel
   * of format <code>TJ.PF_BGRX</code> is stored in <code>char pixel[]</code>,
   * then the green component will be
   * <code>pixel[TJ.getGreenOffset(TJ.PF_BGRX)]</code>.
   *
   * @param pixelFormat the pixel format (one of <code>PF_*</code>)
   *
   * @return the green offset for the given pixel format
   */
  public static int getGreenOffset(int pixelFormat) throws Exception {
    if(pixelFormat < 0 || pixelFormat >= NUMPF)
      throw new Exception("Invalid pixel format");
    return greenOffset[pixelFormat];
  }

  final private static int greenOffset[] = {
    1, 1, 1, 1, 2, 2, 0, 1, 1, 2, 2
  };


  /**
   * For the given pixel format, returns the number of bytes that the blue
   * component is offset from the start of the pixel.  For instance, if a pixel
   * of format <code>TJ.PF_BGRX</code> is stored in <code>char pixel[]</code>,
   * then the blue component will be
   * <code>pixel[TJ.getBlueOffset(TJ.PF_BGRX)]</code>.
   *
   * @param pixelFormat the pixel format (one of <code>PF_*</code>)
   *
   * @return the blue offset for the given pixel format
   */
  public static int getBlueOffset(int pixelFormat) throws Exception {
    if(pixelFormat < 0 || pixelFormat >= NUMPF)
      throw new Exception("Invalid pixel format");
    return blueOffset[pixelFormat];
  }

  final private static int blueOffset[] = {
    2, 0, 2, 0, 1, 3, 0, 2, 0, 1, 3
  };


  /**
   * The uncompressed source/destination image is stored in bottom-up (Windows,
   * OpenGL) order, not top-down (X11) order.
   */
  final public static int FLAG_BOTTOMUP     = 2;
  /**
   * Turn off CPU auto-detection and force TurboJPEG to use MMX code
   * (IPP and 32-bit libjpeg-turbo versions only.)
   */
  final public static int FLAG_FORCEMMX     = 8;
  /**
   * Turn off CPU auto-detection and force TurboJPEG to use SSE code
   * (32-bit IPP and 32-bit libjpeg-turbo versions only.)
   */
  final public static int FLAG_FORCESSE     = 16;
  /**
   * Turn off CPU auto-detection and force TurboJPEG to use SSE2 code
   * (32-bit IPP and 32-bit libjpeg-turbo versions only.)
   */
  final public static int FLAG_FORCESSE2    = 32;
  /**
   * Turn off CPU auto-detection and force TurboJPEG to use SSE3 code
   *(64-bit IPP version only.)
   */
  final public static int FLAG_FORCESSE3    = 128;
  /**
   * Use fast, inaccurate chrominance upsampling routines in the JPEG
   * decompressor (libjpeg and libjpeg-turbo versions only.)
   */
  final public static int FLAG_FASTUPSAMPLE = 256;


  /**
   * Returns the maximum size of the buffer (in bytes) required to hold a JPEG
   * image with the given width and height, and level of chrominance
   * subsampling.
   *
   * @param width the width (in pixels) of the JPEG image
   *
   * @param height the height (in pixels) of the JPEG image
   *
   * @param jpegSubsamp the level of chrominance subsampling to be used when
   * generating the JPEG image (one of {@link TJ TJ.SAMP_*})
   *
   * @return the maximum size of the buffer (in bytes) required to hold a JPEG
   * image with the given width and height, and level of chrominance
   * subsampling
   */
  public native static int bufSize(int width, int height, int jpegSubsamp)
    throws Exception;

  /**
   * Returns the size of the buffer (in bytes) required to hold a YUV planar
   * image with the given width, height, and level of chrominance subsampling.
   *
   * @param width the width (in pixels) of the YUV image
   *
   * @param height the height (in pixels) of the YUV image
   *
   * @param subsamp the level of chrominance subsampling used in the YUV
   * image (one of {@link TJ TJ.SAMP_*})
   *
   * @return the size of the buffer (in bytes) required to hold a YUV planar
   * image with the given width, height, and level of chrominance subsampling
   */
  public native static int bufSizeYUV(int width, int height,
    int subsamp)
    throws Exception;

  /**
   * Returns a list of fractional scaling factors that the JPEG decompressor in
   * this implementation of TurboJPEG supports.
   *
   * @return a list of fractional scaling factors that the JPEG decompressor in
   * this implementation of TurboJPEG supports
   */
  public native static TJScalingFactor[] getScalingFactors()
    throws Exception;

  static {
    TJLoader.load();
  }
};
