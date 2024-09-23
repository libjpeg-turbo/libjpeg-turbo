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
 * This program demonstrates how to use the TurboJPEG Java API to approximate
 * the functionality of the IJG's cjpeg program.  cjpeg features that are not
 * covered:
 *
 * - GIF and Targa input file formats [legacy feature]
 * - Separate quality settings for luminance and chrominance
 * - The floating-point DCT method [legacy feature]
 * - Input image smoothing
 * - Progress reporting
 * - Debug output
 * - Forcing baseline-compatible quantization tables
 * - Specifying arbitrary quantization tables
 * - Specifying arbitrary sampling factors
 * - Scan scripts
 */

import java.io.*;
import java.util.*;
import org.libjpegturbo.turbojpeg.*;


@SuppressWarnings("checkstyle:JavadocType")
final class TJComp {

  private TJComp() {}

  static final String CLASS_NAME =
    new TJComp().getClass().getName();

  static final int DEFAULT_SUBSAMP = TJ.SAMP_420;
  static final int DEFAULT_QUALITY = 75;


  static final String[] SUBSAMP_NAME = {
    "444", "422", "420", "GRAY", "440", "411", "441"
  };


  static void usage() {
    System.out.println("\nUSAGE: java [Java options] " + CLASS_NAME +
                       " [options] <Input image> <JPEG image>\n");

    System.out.println("The input image can be in Windows BMP or PBMPLUS (PPM/PGM) format.\n");

    System.out.println("GENERAL OPTIONS (CAN BE ABBREVIATED)");
    System.out.println("------------------------------------");
    System.out.println("-icc FILE");
    System.out.println("    Embed the ICC (International Color Consortium) color management profile");
    System.out.println("    from the specified file into the JPEG image");
    System.out.println("-lossless PSV[,Pt]");
    System.out.println("    Create a lossless JPEG image (implies -subsamp 444) using predictor");
    System.out.println("    selection value PSV (1-7) and optional point transform Pt (0 through");
    System.out.println("    {data precision} - 1)");
    System.out.println("-maxmemory N");
    System.out.println("    Memory limit (in megabytes) for intermediate buffers used with progressive");
    System.out.println("    JPEG compression, lossless JPEG compression, and Huffman table optimization");
    System.out.println("    [default = no limit]");
    System.out.println("-precision N");
    System.out.println("    Create a JPEG image with N-bit data precision [N = 2..16; default = 8; if N");
    System.out.println("    is not 8 or 12, then -lossless must also be specified] (-precision 12");
    System.out.println("    implies -optimize unless -arithmetic is also specified)");
    System.out.println("-restart N");
    System.out.println("    Add a restart marker every N MCU rows [default = 0 (no restart markers)].");
    System.out.println("    Append 'B' to specify the restart marker interval in MCUs (lossy only.)\n");

    System.out.println("LOSSY JPEG OPTIONS (CAN BE ABBREVIATED)");
    System.out.println("---------------------------------------");
    System.out.println("-arithmetic");
    System.out.println("    Use arithmetic entropy coding instead of Huffman entropy coding (can be");
    System.out.println("    combined with -progressive)");
    System.out.println("-dct fast");
    System.out.println("    Use less accurate DCT algorithm [legacy feature]");
    System.out.println("-dct int");
    System.out.println("    Use more accurate DCT algorithm [default]");
    System.out.println("-grayscale");
    System.out.println("    Create a grayscale JPEG image from a full-color input image");
    System.out.println("-optimize");
    System.out.println("    Use Huffman table optimization");
    System.out.println("-progressive");
    System.out.println("    Create a progressive JPEG image instead of a single-scan JPEG image (can be");
    System.out.println("    combined with -arithmetic; implies -optimize unless -arithmetic is also");
    System.out.println("    specified)");
    System.out.println("-quality {1..100}");
    System.out.format("    Create a JPEG image with the specified quality level [default = %d]\n",
           DEFAULT_QUALITY);
    System.out.println("-rgb");
    System.out.println("    Create a JPEG image that uses the RGB colorspace instead of the YCbCr");
    System.out.println("    colorspace");
    System.out.println("-subsamp {444|422|440|420|411|441}");
    System.out.println("    Create a JPEG image that uses the specified chrominance subsampling level");
    System.out.format("    [default = %s]\n\n", SUBSAMP_NAME[DEFAULT_SUBSAMP]);

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
    TJCompressor tjc = null;
    FileInputStream fis = null;
    FileOutputStream fos = null;

    try {
      int i;
      int arithmetic = -1, colorspace = -1, fastDCT = -1, losslessPSV = -1,
        losslessPt = -1, maxMemory = -1, optimize = -1,
        pixelFormat = TJ.PF_UNKNOWN, precision = 8, progressive = -1,
        quality = DEFAULT_QUALITY, restartIntervalBlocks = -1,
        restartIntervalRows = -1, subsamp = DEFAULT_SUBSAMP;
      String iccFilename = null;
      byte[] iccBuf, jpegBuf;

      for (i = 0; i < argv.length; i++) {
        if (matchArg(argv[i], "-arithmetic", 2))
          arithmetic = 1;
        else if (matchArg(argv[i], "-dct", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "fast", 1))
            fastDCT = 1;
          else if (!matchArg(argv[i], "int", 1))
            usage();
        } else if (matchArg(argv[i], "-grayscale", 2) ||
                   matchArg(argv[i], "-greyscale", 2))
          colorspace = TJ.CS_GRAY;
        else if (matchArg(argv[i], "-icc", 2) && i < argv.length - 1)
          iccFilename = argv[++i];
        else if (matchArg(argv[i], "-lossless", 2) && i < argv.length - 1) {
          Scanner scanner = new Scanner(argv[++i]).useDelimiter(",");

          try {
            if (scanner.hasNextInt())
              losslessPSV = scanner.nextInt();
            if (scanner.hasNextInt())
              losslessPt = scanner.nextInt();
          } catch (Exception e) {}

          if (losslessPSV < 1 || losslessPSV > 7)
            usage();
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
        else if (matchArg(argv[i], "-precision", 4) && i < argv.length - 1) {
          int temp = 0;

          try {
            temp = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (temp < 2 || temp > 16)
            usage();
          precision = temp;
        } else if (matchArg(argv[i], "-progressive", 2))
          progressive = 1;
        else if (matchArg(argv[i], "-quality", 2) && i < argv.length - 1) {
          int temp = 0;

          try {
            temp = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (temp < 1 || temp > 100)
            usage();
          quality = temp;
        } else if (matchArg(argv[i], "-rgb", 3))
          colorspace = TJ.CS_RGB;
        else if (matchArg(argv[i], "-restart", 2) && i < argv.length - 1) {
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
        } else if (matchArg(argv[i], "-subsamp", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "444", 3))
            subsamp = TJ.SAMP_444;
          else if (matchArg(argv[i], "422", 3))
            subsamp = TJ.SAMP_422;
          else if (matchArg(argv[i], "440", 3))
            subsamp = TJ.SAMP_440;
          else if (matchArg(argv[i], "420", 3))
            subsamp = TJ.SAMP_420;
          else if (matchArg(argv[i], "411", 3))
            subsamp = TJ.SAMP_411;
          else if (matchArg(argv[i], "441", 3))
            subsamp = TJ.SAMP_441;
          else
            usage();
        } else break;
      }

      if (i != argv.length - 2)
        usage();
      if (losslessPSV == -1 && precision != 8 && precision != 12)
        usage();

      tjc = new TJCompressor();

      tjc.set(TJ.PARAM_QUALITY, quality);
      tjc.set(TJ.PARAM_SUBSAMP, subsamp);
      tjc.set(TJ.PARAM_PRECISION, precision);
      if (fastDCT >= 0)
        tjc.set(TJ.PARAM_FASTDCT, fastDCT);
      if (optimize >= 0)
        tjc.set(TJ.PARAM_OPTIMIZE, optimize);
      if (progressive >= 0)
        tjc.set(TJ.PARAM_PROGRESSIVE, progressive);
      if (arithmetic >= 0)
        tjc.set(TJ.PARAM_ARITHMETIC, arithmetic);
      if (losslessPSV >= 1 && losslessPSV <= 7) {
        tjc.set(TJ.PARAM_LOSSLESS, 1);
        tjc.set(TJ.PARAM_LOSSLESSPSV, losslessPSV);
        if (losslessPt >= 0)
          tjc.set(TJ.PARAM_LOSSLESSPT, losslessPt);
      }
      if (restartIntervalBlocks >= 0)
        tjc.set(TJ.PARAM_RESTARTBLOCKS, restartIntervalBlocks);
      if (restartIntervalRows >= 0)
        tjc.set(TJ.PARAM_RESTARTROWS, restartIntervalRows);
      if (maxMemory >= 0)
        tjc.set(TJ.PARAM_MAXMEMORY, maxMemory);

      tjc.loadSourceImage(argv[i], 1, pixelFormat);
      pixelFormat = tjc.getPixelFormat();

      if (pixelFormat == TJ.PF_GRAY && colorspace < 0)
        colorspace = TJ.CS_GRAY;
      if (colorspace >= 0)
        tjc.set(TJ.PARAM_COLORSPACE, colorspace);

      if (iccFilename != null) {
        File iccFile = new File(iccFilename);
        fis = new FileInputStream(iccFile);
        int iccSize = fis.available();
        if (iccSize < 1)
          throw new Exception("ICC profile contains no data");
        iccBuf = new byte[iccSize];
        fis.read(iccBuf);
        fis.close();  fis = null;
        tjc.setICCProfile(iccBuf);
      }

      jpegBuf = tjc.compress();

      File outFile = new File(argv[++i]);
      fos = new FileOutputStream(outFile);
      fos.write(jpegBuf, 0, tjc.getCompressedSize());
    } catch (Exception e) {
      e.printStackTrace();
      exitStatus = -1;
    }

    try {
      if (fis != null) fis.close();
      if (fos != null) fos.close();
      if (tjc != null) tjc.close();
    } catch (Exception e) {}
    System.exit(exitStatus);
  }
};
