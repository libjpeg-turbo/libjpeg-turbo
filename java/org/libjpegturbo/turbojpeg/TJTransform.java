/*
 * Copyright (C)2011, 2013, 2018, 2022-2024 D. R. Commander.
 *                                          All Rights Reserved.
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

import java.awt.*;

/**
 * Lossless transform parameters
 */
public class TJTransform extends Rectangle {

  private static final long serialVersionUID = -127367705761430371L;

  /**
   * The number of lossless transform operations
   */
  public static final int NUMOP         = 8;
  /**
   * Do not transform the position of the image pixels.
   */
  public static final int OP_NONE       = 0;
  /**
   * Flip (mirror) image horizontally.  This transform is imperfect if there
   * are any partial iMCUs on the right edge.
   * @see #OPT_PERFECT
   */
  public static final int OP_HFLIP      = 1;
  /**
   * Flip (mirror) image vertically.  This transform is imperfect if there are
   * any partial iMCUs on the bottom edge.
   * @see #OPT_PERFECT
   */
  public static final int OP_VFLIP      = 2;
  /**
   * Transpose image (flip/mirror along upper left to lower right axis).  This
   * transform is always perfect.
   * @see #OPT_PERFECT
   */
  public static final int OP_TRANSPOSE  = 3;
  /**
   * Transverse transpose image (flip/mirror along upper right to lower left
   * axis).  This transform is imperfect if there are any partial iMCUs in the
   * image.
   * @see #OPT_PERFECT
   */
  public static final int OP_TRANSVERSE = 4;
  /**
   * Rotate image clockwise by 90 degrees.  This transform is imperfect if
   * there are any partial iMCUs on the bottom edge.
   * @see #OPT_PERFECT
   */
  public static final int OP_ROT90      = 5;
  /**
   * Rotate image 180 degrees.  This transform is imperfect if there are any
   * partial iMCUs in the image.
   * @see #OPT_PERFECT
   */
  public static final int OP_ROT180     = 6;
  /**
   * Rotate image counter-clockwise by 90 degrees.  This transform is imperfect
   * if there are any partial iMCUs on the right edge.
   * @see #OPT_PERFECT
   */
  public static final int OP_ROT270     = 7;


  /**
   * This option causes {@link TJTransformer#transform
   * TJTransformer.transform()} to throw an exception if the transform is not
   * perfect.  Lossless transforms operate on iMCUs, the size of which depends
   * on the level of chrominance subsampling used.  If the image's width or
   * height is not evenly divisible by the iMCU size (see {@link TJ#getMCUWidth
   * TJ.getMCUWidth()} and {@link TJ#getMCUHeight TJ.getMCUHeight()}), then
   * there will be partial iMCUs on the right and/or bottom edges.  It is not
   * possible to move these partial iMCUs to the top or left of the image, so
   * any transform that would require that is "imperfect."  If this option is
   * not specified, then any partial iMCUs that cannot be transformed will be
   * left in place, which will create odd-looking strips on the right or bottom
   * edge of the image.
   */
  public static final int OPT_PERFECT     = (1 << 0);
  /**
   * Discard any partial iMCUs that cannot be transformed.
   */
  public static final int OPT_TRIM        = (1 << 1);
  /**
   * Enable lossless cropping.
   */
  public static final int OPT_CROP        = (1 << 2);
  /**
   * Discard the color data in the source image, and generate a grayscale
   * destination image.
   */
  public static final int OPT_GRAY        = (1 << 3);
  /**
   * Do not generate a destination image.  This can be used in conjunction with
   * a custom filter to capture the transformed DCT coefficients without
   * transcoding them.
   */
  public static final int OPT_NOOUTPUT    = (1 << 4);
  /**
   * Generate a progressive destination image instead of a single-scan
   * destination image.  Progressive JPEG images generally have better
   * compression ratios than single-scan JPEG images (much better if the image
   * has large areas of solid color), but progressive JPEG decompression is
   * considerably slower than single-scan JPEG decompression.  Can be combined
   * with {@link #OPT_ARITHMETIC}.  Implies {@link #OPT_OPTIMIZE} unless
   * {@link #OPT_ARITHMETIC} is also specified.
   */
  public static final int OPT_PROGRESSIVE = (1 << 5);
  /**
   * Do not copy any extra markers (including Exif and ICC profile data) from
   * the source image to the destination image.
   */
  public static final int OPT_COPYNONE    = (1 << 6);
  /**
   * Enable arithmetic entropy coding in the destination image.  Arithmetic
   * entropy coding generally improves compression relative to Huffman entropy
   * coding (the default), but it reduces decompression performance
   * considerably.  Can be combined with {@link #OPT_PROGRESSIVE}.
   */
  public static final int OPT_ARITHMETIC  = (1 << 7);
  /**
   * Enable Huffman table optimization for the destination image.  Huffman
   * table optimization improves compression slightly (generally 5% or less.)
   */
  public static final int OPT_OPTIMIZE    = (1 << 8);


  /**
   * Create a new lossless transform instance.
   */
  public TJTransform() {
  }

  /**
   * Create a new lossless transform instance with the given parameters.
   *
   * @param x the left boundary of the cropping region.  This must be evenly
   * divisible by the iMCU width (see {@link TJ#getMCUWidth TJ.getMCUWidth()})
   * of the destination image.
   *
   * @param y the upper boundary of the cropping region.  This must be evenly
   * divisible by the iMCU height (see {@link TJ#getMCUHeight
   * TJ.getMCUHeight()}) of the destination image.
   *
   * @param w the width of the cropping region.  Setting this to 0 is the
   * equivalent of setting it to (width of the source JPEG image -
   * <code>x</code>).
   *
   * @param h the height of the cropping region.  Setting this to 0 is the
   * equivalent of setting it to (height of the source JPEG image -
   * <code>y</code>).
   *
   * @param op one of the transform operations ({@link #OP_NONE OP_*})
   *
   * @param options the bitwise OR of one or more of the transform options
   * ({@link #OPT_PERFECT OPT_*})
   *
   * @param cf an instance of an object that implements the
   * {@link TJCustomFilter} interface, or null if no custom filter is needed
   */
  @SuppressWarnings("checkstyle:HiddenField")
  public TJTransform(int x, int y, int w, int h, int op, int options,
                     TJCustomFilter cf) {
    super(x, y, w, h);
    this.op = op;
    this.options = options;
    this.cf = cf;
  }

  /**
   * Create a new lossless transform instance with the given parameters.
   *
   * @param r a <code>java.awt.Rectangle</code> instance that specifies the
   * cropping region.  See
   * {@link #TJTransform(int, int, int, int, int, int, TJCustomFilter)} for
   * more details.
   *
   * @param op one of the transform operations ({@link #OP_NONE OP_*})
   *
   * @param options the bitwise OR of one or more of the transform options
   * ({@link #OPT_PERFECT OPT_*})
   *
   * @param cf an instance of an object that implements the
   * {@link TJCustomFilter} interface, or null if no custom filter is needed
   */
  @SuppressWarnings("checkstyle:HiddenField")
  public TJTransform(Rectangle r, int op, int options,
                     TJCustomFilter cf) {
    super(r);
    this.op = op;
    this.options = options;
    this.cf = cf;
  }

  /**
   * Transform operation (one of {@link #OP_NONE OP_*})
   */
  @SuppressWarnings("checkstyle:VisibilityModifier")
  public int op = 0;

  /**
   * Transform options (bitwise OR of one or more of
   * {@link #OPT_PERFECT OPT_*})
   */
  @SuppressWarnings("checkstyle:VisibilityModifier")
  public int options = 0;

  /**
   * Custom filter instance
   */
  @SuppressWarnings("checkstyle:VisibilityModifier")
  public TJCustomFilter cf = null;
}
