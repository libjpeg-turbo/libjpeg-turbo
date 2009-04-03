/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005, 2006 Sun Microsystems, Inc.
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)) && defined(_WIN32) && defined(DLLDEFINE)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#define DLLCALL

/* Subsampling */
#define NUMSUBOPT 4

enum {TJ_444=0, TJ_422, TJ_411, TJ_GRAYSCALE};

/* Flags */
#define TJ_BGR       1
#define TJ_BOTTOMUP  2
#define TJ_FORCEMMX  8   /* Force IPP to use MMX code even if SSE available */
#define TJ_FORCESSE  16  /* Force IPP to use SSE1 code even if SSE2 available */
#define TJ_FORCESSE2 32  /* Force IPP to use SSE2 code (useful if auto-detect is not working properly) */
#define TJ_ALPHAFIRST 64 /* BGR buffer is ABGR and RGB buffer is ARGB */
#define TJ_FORCESSE3 128 /* Force IPP to use SSE3 code (useful if auto-detect is not working properly) */

typedef void* tjhandle;

#define TJPAD(p) (((p)+3)&(~3))
#ifndef max
 #define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* API follows */


/*
  tjhandle tjInitCompress(void)

  Creates a new JPEG compressor instance, allocates memory for the structures,
  and returns a handle to the instance.  Most applications will only
  need to call this once at the beginning of the program or once for each
  concurrent thread.  Don't try to create a new instance every time you
  compress an image, because this will cause performance to suffer.

  RETURNS: NULL on error
*/
DLLEXPORT tjhandle DLLCALL tjInitCompress(void);


/*
  int tjCompress(tjhandle j,
     unsigned char *srcbuf, int width, int pitch, int height, int pixelsize,
     unsigned char *dstbuf, unsigned long *size,
     int jpegsubsamp, int jpegqual, int flags)

  [INPUT] j = instance handle previously returned from a call to
     tjInitCompress()
  [INPUT] srcbuf = pointer to user-allocated image buffer containing pixels in
     RGB(A) or BGR(A) form
  [INPUT] width =  width (in pixels) of the source image
  [INPUT] pitch = bytes per line of the source image (width*pixelsize if the
     bitmap is unpadded, else TJPAD(width*pixelsize) if each line of the bitmap
     is padded to the nearest 32-bit boundary, such as is the case for Windows
     bitmaps.  You can also be clever and use this parameter to skip lines, etc.,
     as long as the pitch is greater than 0.)
  [INPUT] height = height (in pixels) of the source image
  [INPUT] pixelsize = size (in bytes) of each pixel in the source image
     RGBA and BGRA: 4, RGB and BGR: 3
  [INPUT] dstbuf = pointer to user-allocated image buffer which will receive
     the JPEG image.  Use the macro TJBUFSIZE(width, height) to determine
     the appropriate size for this buffer based on the image width and height.
  [OUTPUT] size = pointer to unsigned long which receives the size (in bytes)
     of the compressed image
  [INPUT] jpegsubsamp = Specifies either 4:1:1, 4:2:2, or 4:4:4 subsampling.
     When the image is converted from the RGB to YCbCr colorspace as part of the
     JPEG compression process, every other Cb and Cr (chrominance) pixel can be
     discarded to produce a smaller image with little perceptible loss of
     image clarity (the human eye is more sensitive to small changes in
     brightness than small changes in color.)

     TJ_411: 4:1:1 subsampling.  Discards every other Cb, Cr pixel in both
        horizontal and vertical directions.
     TJ_422: 4:2:2 subsampling.  Discards every other Cb, Cr pixel only in
        the horizontal direction.
     TJ_444: no subsampling.
     TJ_GRAYSCALE: Generate grayscale JPEG image

  [INPUT] jpegqual = JPEG quality (an integer between 0 and 100 inclusive.)
  [INPUT] flags = the bitwise OR of one or more of the following

     TJ_BGR: The components of each pixel in the source image are stored in
        B,G,R order, not R,G,B
     TJ_BOTTOMUP: The source image is stored in bottom-up (Windows) order,
        not top-down
     TJ_FORCEMMX: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use MMX code (bypass CPU auto-detection)
     TJ_FORCESSE: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use SSE code (bypass CPU auto-detection)
     TJ_FORCESSE2: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use SSE2 code (bypass CPU auto-detection)
     TJ_FORCESSE3: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use SSE3 code (bypass CPU auto-detection)

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjCompress(tjhandle j,
	unsigned char *srcbuf, int width, int pitch, int height, int pixelsize,
	unsigned char *dstbuf, unsigned long *size,
	int jpegsubsamp, int jpegqual, int flags);

DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height);

/*
  tjhandle tjInitDecompress(void)

  Creates a new JPEG decompressor instance, allocates memory for the
  structures, and returns a handle to the instance.  Most applications will
  only need to call this once at the beginning of the program or once for each
  concurrent thread.  Don't try to create a new instance every time you
  decompress an image, because this will cause performance to suffer.

  RETURNS: NULL on error
*/
DLLEXPORT tjhandle DLLCALL tjInitDecompress(void);


/*
  int tjDecompressHeader(tjhandle j,
     unsigned char *srcbuf, unsigned long size,
     int *width, int *height)

  [INPUT] j = instance handle previously returned from a call to
     tjInitDecompress()
  [INPUT] srcbuf = pointer to a user-allocated buffer containing the JPEG image
     to decompress
  [INPUT] size = size of the JPEG image buffer (in bytes)
  [OUTPUT] width = width (in pixels) of the JPEG image
  [OUTPUT] height = height (in pixels) of the JPEG image

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle j,
	unsigned char *srcbuf, unsigned long size,
	int *width, int *height);


/*
  int tjDecompress(tjhandle j,
     unsigned char *srcbuf, unsigned long size,
     unsigned char *dstbuf, int width, int pitch, int height, int pixelsize,
     int flags)

  [INPUT] j = instance handle previously returned from a call to
     tjInitDecompress()
  [INPUT] srcbuf = pointer to a user-allocated buffer containing the JPEG image
     to decompress
  [INPUT] size = size of the JPEG image buffer (in bytes)
  [INPUT] dstbuf = pointer to user-allocated image buffer which will receive
     the bitmap image.  This buffer should normally be pitch*height
     bytes in size, although this pointer may also be used to decompress into
     a specific region of a larger buffer.
  [INPUT] width =  width (in pixels) of the destination image
  [INPUT] pitch = bytes per line of the destination image (width*pixelsize if the
     bitmap is unpadded, else TJPAD(width*pixelsize) if each line of the bitmap
     is padded to the nearest 32-bit boundary, such as is the case for Windows
     bitmaps.  You can also be clever and use this parameter to skip lines, etc.,
     as long as the pitch is greater than 0.)
  [INPUT] height = height (in pixels) of the destination image
  [INPUT] pixelsize = size (in bytes) of each pixel in the destination image
     RGBA/RGBx and BGRA/BGRx: 4, RGB and BGR: 3
  [INPUT] flags = the bitwise OR of one or more of the following

     TJ_BGR: The components of each pixel in the destination image should be
        written in B,G,R order, not R,G,B
     TJ_BOTTOMUP: The destination image should be stored in bottom-up
        (Windows) order, not top-down
     TJ_FORCEMMX: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use MMX code (bypass CPU auto-detection)
     TJ_FORCESSE: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use SSE code (bypass CPU auto-detection)
     TJ_FORCESSE2: Valid only for the Intel Performance Primitives implementation
        of this codec-- force IPP to use SSE2 code (bypass CPU auto-detection)

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDecompress(tjhandle j,
	unsigned char *srcbuf, unsigned long size,
	unsigned char *dstbuf, int width, int pitch, int height, int pixelsize,
	int flags);


/*
  int tjDestroy(tjhandle h)

  Frees structures associated with a compression or decompression instance
  
  [INPUT] h = instance handle (returned from a previous call to
     tjInitCompress() or tjInitDecompress()

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDestroy(tjhandle h);


/*
  char *tjGetErrorStr(void)
  
  Returns a descriptive error message explaining why the last command failed
*/
DLLEXPORT char* DLLCALL tjGetErrorStr(void);

#ifdef __cplusplus
}
#endif
