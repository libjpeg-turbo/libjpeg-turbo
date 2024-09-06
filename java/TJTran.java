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
 * functionality of the IJG's jpegtran program.  jpegtran features that are not
 * covered:
 *
 * - Scan scripts
 * - Expanding the input image when cropping
 * - Wiping a region of the input image
 * - Dropping another JPEG image into the input image
 * - Copying only comment markers or ICC profile markers
 * - Embedding an ICC color management profile
 * - Progress reporting
 * - Treating warnings as non-fatal [limitation of the TurboJPEG Java API]
 * - Debug output
 */

import java.io.*;
import java.util.*;
import org.libjpegturbo.turbojpeg.*;


@SuppressWarnings("checkstyle:JavadocType")
final class TJTran {

  private TJTran() {}

  static final String CLASS_NAME =
    new TJTran().getClass().getName();


  private static boolean isCropped(java.awt.Rectangle cr) {
    return (cr.x != 0 || cr.y != 0 || cr.width != 0 || cr.height != 0);
  }


  static void usage() {
    int i;
    TJScalingFactor[] scalingFactors = TJ.getScalingFactors();
    int numScalingFactors = scalingFactors.length;

    System.out.println("\nUSAGE: java [Java options] " + CLASS_NAME +
                       " [options] <JPEG input image> <JPEG output image>\n");

    System.out.println("This program reads the DCT coefficients from the lossy JPEG input image,");
    System.out.println("optionally transforms them, and writes them to a lossy JPEG output image.\n");

    System.out.println("OPTIONS (CAN BE ABBREVBIATED)");
    System.out.println("-----------------------------");
    System.out.println("-arithmetic");
    System.out.println("    Use arithmetic entropy coding in the output image instead of Huffman");
    System.out.println("    entropy coding (can be combined with -progressive)");
    System.out.println("-copy all");
    System.out.println("    Copy all extra markers (including comments, JFIF thumbnails, Exif data, and");
    System.out.println("    ICC profile data) from the input image to the output image [default]");
    System.out.println("-copy none");
    System.out.println("    Copy no extra markers from the input image to the output image");
    System.out.println("-crop WxH+X+Y");
    System.out.println("    Include only the specified region of the input image.  (W, H, X, and Y are");
    System.out.println("    the width, height, left boundary, and upper boundary of the region, all");
    System.out.println("    specified relative to the transformed image dimensions.)  If necessary, X");
    System.out.println("    and Y will be shifted up and left to the nearest iMCU boundary, and W and H");
    System.out.println("    will be increased accordingly.");
    System.out.println("-flip {horizontal|vertical}, -rotate {90|180|270}, -transpose, -transverse");
    System.out.println("    Perform the specified lossless transform operation (these options are");
    System.out.println("    mutually exclusive)");
    System.out.println("-grayscale");
    System.out.println("    Create a grayscale output image from a full-color input image");
    System.out.println("-maxmemory N");
    System.out.println("    Memory limit (in megabytes) for intermediate buffers used with progressive");
    System.out.println("    JPEG compression, Huffman table optimization, and lossless transformation");
    System.out.println("    [default = no limit]");
    System.out.println("-maxscans N");
    System.out.println("    Refuse to transform progressive JPEG images that have more than N scans");
    System.out.println("-optimize");
    System.out.println("    Use Huffman table optimization in the output image");
    System.out.println("-perfect");
    System.out.println("    Abort if the requested transform operation is imperfect (non-reversible.)");
    System.out.println("    '-flip horizontal', '-rotate 180', '-rotate 270', and '-transverse' are");
    System.out.println("    imperfect if the image width is not evenly divisible by the iMCU width.");
    System.out.println("    '-flip vertical', '-rotate 90', '-rotate 180', and '-transverse' are");
    System.out.println("    imperfect if the image height is not evenly divisible by the iMCU height.");
    System.out.println("-progressive");
    System.out.println("    Create a progressive output image instead of a single-scan output image");
    System.out.println("    (can be combined with -arithmetic; implies -optimize unless -arithmetic is");
    System.out.println("    also specified)");
    System.out.println("-restart N");
    System.out.println("    Add a restart marker every N MCU rows [default = 0 (no restart markers)].");
    System.out.println("    Append 'B' to specify the restart marker interval in MCUs.");
    System.out.println("-trim");
    System.out.println("    If necessary, trim the partial iMCUs at the right or bottom edge of the");
    System.out.println("    image to make the requested transform perfect\n");

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
    TJTransformer tjt = null;
    FileInputStream fis = null;
    FileOutputStream fos = null;

    try {

      int i;
      int arithmetic = 0, maxMemory = -1, maxScans = -1, optimize = -1,
        progressive = 0, restartIntervalBlocks = -1, restartIntervalRows = -1,
        subsamp;
      TJTransform[] xform = new TJTransform[1];
      xform[0] = new TJTransform();
      byte[] srcBuf;
      int width, height;
      byte[][] dstBuf;

      for (i = 0; i < argv.length; i++) {
        if (matchArg(argv[i], "-arithmetic", 2))
          arithmetic = 1;
        else if (matchArg(argv[i], "-crop", 3) && i < argv.length - 1) {
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
          xform[0].options |= TJTransform.OPT_CROP;
          xform[0].width = tempWidth;
          xform[0].height = tempHeight;
          xform[0].x = tempX;
          xform[0].y = tempY;
        } else if (matchArg(argv[i], "-copy", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "none", 1))
            xform[0].options |= TJTransform.OPT_COPYNONE;
          else if (!matchArg(argv[i], "all", 1))
            usage();
        } else if (matchArg(argv[i], "-flip", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "horizontal", 1))
            xform[0].op = TJTransform.OP_HFLIP;
          else if (matchArg(argv[i], "vertical", 1))
            xform[0].op = TJTransform.OP_VFLIP;
          else
            usage();
        } else if (matchArg(argv[i], "-grayscale", 2) ||
                   matchArg(argv[i], "-greyscale", 2))
          xform[0].options |= TJTransform.OPT_GRAY;
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
        } else if (matchArg(argv[i], "-optimize", 2) ||
                   matchArg(argv[i], "-optimise", 2))
          optimize = 1;
        else if (matchArg(argv[i], "-perfect", 3))
          xform[0].options |= TJTransform.OPT_PERFECT;
        else if (matchArg(argv[i], "-progressive", 2))
          progressive = 1;
        else if (matchArg(argv[i], "-rotate", 3) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "90", 2))
            xform[0].op = TJTransform.OP_ROT90;
          else if (matchArg(argv[i], "180", 3))
            xform[0].op = TJTransform.OP_ROT180;
          else if (matchArg(argv[i], "270", 3))
            xform[0].op = TJTransform.OP_ROT270;
          else
            usage();
        } else if (matchArg(argv[i], "-restart", 2) && i < argv.length - 1) {
          int temp = -1;
          String arg = argv[++i];
          Scanner scanner = new Scanner(arg).useDelimiter("b|B");

          try {
            temp = scanner.nextInt();
          } catch (Exception e) {}

          if (temp < 0 || temp > 65535 || scanner.hasNext())
            usage();
          if (arg.endsWith("B") || arg.endsWith("b"))
            restartIntervalBlocks = temp;
          else
            restartIntervalRows = temp;
        } else if (matchArg(argv[i], "-transverse", 7))
          xform[0].op = TJTransform.OP_TRANSVERSE;
        else if (matchArg(argv[i], "-trim", 4))
          xform[0].options |= TJTransform.OPT_TRIM;
        else if (matchArg(argv[i], "-transpose", 2))
          xform[0].op = TJTransform.OP_TRANSPOSE;
        else break;
      }

      if (i != argv.length - 2)
        usage();

      tjt = new TJTransformer();

      if (optimize >= 0)
        tjt.set(TJ.PARAM_OPTIMIZE, optimize);
      if (maxScans >= 0)
        tjt.set(TJ.PARAM_SCANLIMIT, maxScans);
      if (restartIntervalBlocks >= 0)
        tjt.set(TJ.PARAM_RESTARTBLOCKS, restartIntervalBlocks);
      if (restartIntervalRows >= 0)
        tjt.set(TJ.PARAM_RESTARTROWS, restartIntervalRows);
      if (maxMemory >= 0)
        tjt.set(TJ.PARAM_MAXMEMORY, maxMemory);

      File inFile = new File(argv[i++]);
      fis = new FileInputStream(inFile);
      int srcSize = fis.available();
      if (srcSize < 1)
        throw new Exception("Input file contains no data");
      srcBuf = new byte[srcSize];
      fis.read(srcBuf);
      fis.close();  fis = null;

      tjt.setSourceImage(srcBuf, srcSize);
      subsamp = tjt.get(TJ.PARAM_SUBSAMP);
      if ((xform[0].options & TJTransform.OPT_GRAY) != 0)
        subsamp = TJ.SAMP_GRAY;
      if (xform[0].op == TJTransform.OP_TRANSPOSE ||
          xform[0].op == TJTransform.OP_TRANSVERSE ||
          xform[0].op == TJTransform.OP_ROT90 ||
          xform[0].op == TJTransform.OP_ROT270) {
        width = tjt.get(TJ.PARAM_JPEGHEIGHT);
        height = tjt.get(TJ.PARAM_JPEGWIDTH);
        if (subsamp == TJ.SAMP_422)
          subsamp = TJ.SAMP_440;
        else if (subsamp == TJ.SAMP_440)
          subsamp = TJ.SAMP_422;
        else if (subsamp == TJ.SAMP_411)
          subsamp = TJ.SAMP_441;
        else if (subsamp == TJ.SAMP_441)
          subsamp = TJ.SAMP_411;
      } else {
        width = tjt.get(TJ.PARAM_JPEGWIDTH);
        height = tjt.get(TJ.PARAM_JPEGHEIGHT);
      }

      if (progressive >= 0)
        tjt.set(TJ.PARAM_PROGRESSIVE, progressive);
      if (arithmetic >= 0)
        tjt.set(TJ.PARAM_ARITHMETIC, arithmetic);

      if (isCropped(xform[0])) {
        int xAdjust, yAdjust;

        if (subsamp == TJ.SAMP_UNKNOWN)
          throw new Exception("Could not determine subsampling level of input image");
        xAdjust = xform[0].x % TJ.getMCUWidth(subsamp);
        yAdjust = xform[0].y % TJ.getMCUHeight(subsamp);
        xform[0].x -= xAdjust;
        xform[0].width += xAdjust;
        xform[0].y -= yAdjust;
        xform[0].height += yAdjust;
      }

      dstBuf = new byte[1][TJ.bufSize(width, height, subsamp)];

      tjt.transform(dstBuf, xform);

      File outFile = new File(argv[i]);
      fos = new FileOutputStream(outFile);
      fos.write(dstBuf[0], 0, tjt.getTransformedSizes()[0]);
    } catch (Exception e) {
      e.printStackTrace();
      exitStatus = -1;
    }

    try {
      if (fis != null) fis.close();
      if (tjt != null) tjt.close();
      if (fos != null) fos.close();
    } catch (Exception e) {}
    System.exit(exitStatus);
  }
};
