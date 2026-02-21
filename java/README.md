TurboJPEG Java API
==================

The TurboJPEG shared library can optionally be built with a Java Native
Interface (JNI) wrapper, which allows the library to be loaded and used
directly from Java applications.  The Java front end for this (the TurboJPEG
Java API) is defined in classes located under **org/libjpegturbo/turbojpeg**.
The source code for these Java classes is licensed under a BSD-style license,
so the files can be incorporated directly into both open source and proprietary
projects without restriction.  A Java archive (JAR) file containing these
classes is also shipped with the "official" distribution packages of
libjpeg-turbo.

[TJComp.java](TJComp.java), [TJDecomp.java](TJDecomp.java), and
[TJTran.java](TJTran.java), which should be located in the same directory as
this README file, demonstrate how to use the TurboJPEG Java API to compress,
decompress, and transform JPEG images in memory.


Performance Pitfalls
--------------------

The TurboJPEG Java API defines convenience methods that can allocate image
buffers or instantiate classes to hold the result of compress, decompress, or
transform operations.  However, if you use these methods, then be mindful of
the amount of new data you are creating on the heap.  It may be necessary to
manually invoke the garbage collector to prevent heap exhaustion or to prevent
performance degradation.  Background garbage collection can kill performance,
particularly in a multi-threaded environment.  (Java pauses all threads when
the GC runs.)

The TurboJPEG Java API always gives you the option of pre-allocating your own
source and destination buffers, which allows you to re-use those buffers for
compressing/decompressing multiple images.  If the image sequence you are
compressing or decompressing consists of images of the same size, then
pre-allocating the buffers is recommended.


Installation Directory
----------------------

The TurboJPEG Java classes will look for the TurboJPEG JNI library
(**libturbojpeg.so**, **libturbojpeg.dylib**, or **turbojpeg.dll**) in the
system library paths or in any paths specified in `LD_LIBRARY_PATH` (Un\*x),
`DYLD_LIBRARY_PATH` (Mac), `PATH` (Windows), or the `java.library.path` Java
system property.  Failing this, on Un*x and Mac systems, the classes will look
for the JNI library under the library directory configured when libjpeg-turbo
was built.  If that library directory is **/opt/libjpeg-turbo/lib32**, then
**/opt/libjpeg-turbo/lib64** is also searched, and vice versa.
