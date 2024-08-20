/*
 * Copyright (C)2011-2012, 2014-2015, 2017-2018, 2022-2024 D. R. Commander.
 *                                                         All Rights Reserved.
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

/*
 * This program demonstrates how to use the TurboJPEG C API to approximate the
 * functionality of the IJG's djpeg program.  djpeg features that are not
 * covered:
 *
 * - OS/2 BMP, GIF, and Targa output file formats [legacy feature]
 * - Color quantization and dithering [legacy feature]
 * - The floating-point IDCT method [legacy feature]
 * - Extracting an ICC color management profile
 * - Progress reporting
 * - Skipping rows (i.e. exclusive rather than inclusive partial decompression)
 * - Debug output
 */

import java.io.*;
import java.util.*;
import org.libjpegturbo.turbojpeg.*;


@SuppressWarnings("checkstyle:JavadocType")
final class TJDecomp {

  private TJDecomp() {}

  static final String CLASS_NAME =
    new TJDecomp().getClass().getName();


  private static boolean isCropped(java.awt.Rectangle cr) {
    return (cr.x != 0 || cr.y != 0 || cr.width != 0 || cr.height != 0);
  }


  private static String tjErrorMsg;
  private static int tjErrorCode = -1;

  static void handleTJException(TJException e, int stopOnWarning)
                                throws TJException {
    String errorMsg = e.getMessage();
    int errorCode = e.getErrorCode();

    if (stopOnWarning != 1 && errorCode == TJ.ERR_WARNING) {
      if (tjErrorMsg == null || !tjErrorMsg.equals(errorMsg) ||
          tjErrorCode != errorCode) {
        tjErrorMsg = errorMsg;
        tjErrorCode = errorCode;
        System.out.println("WARNING: " + errorMsg);
      }
    } else
      throw e;
  }


  static void usage() {
    int i;
    TJScalingFactor[] scalingFactors = TJ.getScalingFactors();
    int numScalingFactors = scalingFactors.length;

    System.out.println("\nUSAGE: java [Java options] " + CLASS_NAME +
                       " [options] <JPEG image> <Output image>\n");

    System.out.println("The output image will be in Windows BMP or PBMPLUS (PPM/PGM) format, depending");
    System.out.println("on the file extension.\n");

    System.out.println("GENERAL OPTIONS (CAN BE ABBREVBIATED)");
    System.out.println("-------------------------------------");
    System.out.println("-strict");
    System.out.println("    Treat all warnings as fatal; abort immediately if incomplete or corrupt");
    System.out.println("    data is encountered in the JPEG image, rather than trying to salvage the");
    System.out.println("    rest of the image\n");

    System.out.println("LOSSY JPEG OPTIONS (CAN BE ABBREVIATED)");
    System.out.println("---------------------------------------");
    System.out.println("-crop WxH+X+Y");
    System.out.println("    Decompress only the specified region of the JPEG image.  (W, H, X, and Y");
    System.out.println("    are the width, height, left boundary, and upper boundary of the region, all");
    System.out.println("    specified relative to the scaled image dimensions.)  If necessary, X will");
    System.out.println("    be shifted left to the nearest iMCU boundary, and W will be increased");
    System.out.println("    accordingly.");
    System.out.println("-dct fast");
    System.out.println("    Use less accurate IDCT algorithm [legacy feature]");
    System.out.println("-dct int");
    System.out.println("    Use more accurate IDCT algorithm [default]");
    System.out.println("-grayscale");
    System.out.println("    Decompress a full-color JPEG image into a grayscale output image");
    System.out.println("-maxmemory N");
    System.out.println("    Memory limit (in megabytes) for intermediate buffers used with progressive");
    System.out.println("    JPEG decompression [default = no limit]");
    System.out.println("-maxscans N");
    System.out.println("    Refuse to decompress progressive JPEG images that have more than N scans");
    System.out.println("-nosmooth");
    System.out.println("    Use the fastest chrominance upsampling algorithm available");
    System.out.println("-rgb");
    System.out.println("    Decompress a grayscale JPEG image into a full-color output image");
    System.out.println("-scale M/N");
    System.out.println("    Scale the width/height of the JPEG image by a factor of M/N when");
    System.out.print("    decompressing it (M/N = ");
    for (i = 0; i < numScalingFactors; i++) {
      System.out.format("%d/%d", scalingFactors[i].getNum(),
                        scalingFactors[i].getDenom());
      if (numScalingFactors == 2 && i != numScalingFactors - 1)
        System.out.print(" or ");
      else if (numScalingFactors > 2) {
        if (i != numScalingFactors - 1)
          System.out.print(", ");
        if (i == numScalingFactors - 2)
          System.out.print("or ");
      }
      if (i % 8 == 0 && i != 0) System.out.print("\n    ");
    }
    System.out.println(")\n");

    System.exit(1);
  }


  static boolean matchArg(String arg, String string, int minChars) {
    if (arg.length() > string.length() || arg.length() < minChars)
      return false;

    int cmpChars = Math.max(arg.length(), minChars);
    string = string.substring(0, cmpChars);

    return arg.equalsIgnoreCase(string);
  }


  public static void main(String[] argv) {
    int exitStatus = 0;
    TJDecompressor tjd = null;
    FileInputStream fis = null;

    try {

      int i;
      int colorspace, fastDCT = -1, fastUpsample = -1, maxMemory = -1,
        maxScans = -1, pixelFormat = TJ.PF_UNKNOWN, precision,
        stopOnWarning = -1, subsamp;
      java.awt.Rectangle croppingRegion = TJ.UNCROPPED;
      TJScalingFactor scalingFactor = TJ.UNSCALED;
      int width, height;
      byte[] jpegBuf;
      Object dstBuf = null;

      for (i = 0; i < argv.length; i++) {
        if (matchArg(argv[i], "-crop", 2) && i < argv.length - 1) {
          int tempWidth = -1, tempHeight = -1, tempX = -1, tempY = -1;
          Scanner scanner = new Scanner(argv[++i]).useDelimiter("x|X|\\+");

          try {
            tempWidth = scanner.nextInt();
            tempHeight = scanner.nextInt();
            tempX = scanner.nextInt();
            tempY = scanner.nextInt();
          } catch (Exception e) {}

          if (tempWidth < 1 || tempHeight < 1 || tempX < 0 || tempY < 0)
            usage();
          croppingRegion.width = tempWidth;
          croppingRegion.height = tempHeight;
          croppingRegion.x = tempX;
          croppingRegion.y = tempY;
        } else if (matchArg(argv[i], "-dct", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "fast", 1))
            fastDCT = 1;
          else if (!matchArg(argv[i], "int", 1))
            usage();
        } else if (matchArg(argv[i], "-grayscale", 2) ||
                   matchArg(argv[i], "-greyscale", 2))
          pixelFormat = TJ.PF_GRAY;
        else if (matchArg(argv[i], "-maxscans", 5) && i < argv.length - 1) {
          int temp = -1;

          try {
            temp = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (temp < 0)
            usage();
          maxScans = temp;
        } else if (matchArg(argv[i], "-maxmemory", 2) && i < argv.length - 1) {
          int temp = -1;

          try {
            temp = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (temp < 0)
            usage();
          maxMemory = temp;
        } else if (matchArg(argv[i], "-nosmooth", 2))
          fastUpsample = 1;
        else if (matchArg(argv[i], "-rgb", 2))
          pixelFormat = TJ.PF_RGB;
        else if (matchArg(argv[i], "-strict", 3))
          stopOnWarning = 1;
        else if (matchArg(argv[i], "-scale", 2) && i < argv.length - 1) {
          int tempNum = 0, tempDenom = 0;
          boolean match = false, scanned = true;
          Scanner scanner = new Scanner(argv[++i]).useDelimiter("/");

          try {
            tempNum = scanner.nextInt();
            tempDenom = scanner.nextInt();
          } catch (Exception e) {}

          if (tempNum < 1 || tempDenom < 1)
            usage();

          TJScalingFactor[] scalingFactors = TJ.getScalingFactors();
          for (int j = 0; j < scalingFactors.length; j++) {
            if ((double)tempNum / (double)tempDenom ==
                (double)scalingFactors[j].getNum() /
                (double)scalingFactors[j].getDenom()) {
              scalingFactor = scalingFactors[j];
              match = true;  break;
            }
          }
          if (!match) usage();
        } else break;
      }

      if (i != argv.length - 2)
        usage();

      tjd = new TJDecompressor();

      if (stopOnWarning >= 0)
        tjd.set(TJ.PARAM_STOPONWARNING, stopOnWarning);
      if (fastUpsample >= 0)
        tjd.set(TJ.PARAM_FASTUPSAMPLE, fastUpsample);
      if (fastDCT >= 0)
        tjd.set(TJ.PARAM_FASTDCT, fastDCT);
      if (maxScans >= 0)
        tjd.set(TJ.PARAM_SCANLIMIT, maxScans);
      if (maxMemory >= 0)
        tjd.set(TJ.PARAM_MAXMEMORY, maxMemory);

      File jpegFile = new File(argv[i++]);
      fis = new FileInputStream(jpegFile);
      int jpegSize = fis.available();
      if (jpegSize < 1)
        throw new Exception("Input file contains no data");
      jpegBuf = new byte[jpegSize];
      fis.read(jpegBuf);
      fis.close();  fis = null;

      try {
        tjd.setSourceImage(jpegBuf, jpegSize);
      } catch (TJException e) { handleTJException(e, stopOnWarning); }
      subsamp = tjd.get(TJ.PARAM_SUBSAMP);
      width = tjd.get(TJ.PARAM_JPEGWIDTH);
      height = tjd.get(TJ.PARAM_JPEGHEIGHT);
      precision = tjd.get(TJ.PARAM_PRECISION);
      colorspace = tjd.get(TJ.PARAM_COLORSPACE);

      if (pixelFormat == TJ.PF_UNKNOWN) {
        if (colorspace == TJ.CS_GRAY)
          pixelFormat = TJ.PF_GRAY;
        else if (colorspace == TJ.CS_CMYK || colorspace == TJ.CS_YCCK)
          pixelFormat = TJ.PF_CMYK;
        else
          pixelFormat = TJ.PF_RGB;
      }

      if (tjd.get(TJ.PARAM_LOSSLESS) == 0) {
        tjd.setScalingFactor(scalingFactor);
        width = scalingFactor.getScaled(width);
        height = scalingFactor.getScaled(height);

        if (isCropped(croppingRegion)) {
          int adjustment;

          if (subsamp == TJ.SAMP_UNKNOWN)
            throw new Exception("Could not determine subsampling level of JPEG image");
          adjustment = croppingRegion.x %
                       scalingFactor.getScaled(TJ.getMCUWidth(subsamp));
          croppingRegion.x -= adjustment;
          croppingRegion.width += adjustment;
          tjd.setCroppingRegion(croppingRegion);
          width = croppingRegion.width;
          height = croppingRegion.height;
        }
      }

      if (precision <= 8)
        dstBuf = new byte[width * height * TJ.getPixelSize(pixelFormat)];
      else
        dstBuf = new short[width * height * TJ.getPixelSize(pixelFormat)];

      try {
        if (precision <= 8)
          tjd.decompress8((byte[])dstBuf, 0, 0, 0, pixelFormat);
        else if (precision <= 12)
          tjd.decompress12((short[])dstBuf, 0, 0, 0, pixelFormat);
        else
          tjd.decompress16((short[])dstBuf, 0, 0, 0, pixelFormat);
      } catch (TJException e) { handleTJException(e, stopOnWarning); }

      tjd.saveImage(argv[i], dstBuf, 0, 0, width, 0, height, pixelFormat);
    } catch (Exception e) {
      e.printStackTrace();
      exitStatus = -1;
    }

    try {
      if (fis != null) fis.close();
      if (tjd != null) tjd.close();
    } catch (Exception e) {}
    System.exit(exitStatus);
  }
};
