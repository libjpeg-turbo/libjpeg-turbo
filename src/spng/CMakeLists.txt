include_directories(${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
add_definitions(-DSPNG_STATIC)

add_library(spng-static OBJECT spng.c zlib/adler32.c zlib/compress.c
  zlib/crc32.c zlib/deflate.c zlib/inffast.c zlib/inflate.c zlib/inftrees.c
  zlib/trees.c zlib/zutil.c)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(spng-static PRIVATE -w)
endif()
if(CMAKE_C_COMPILER_ID MATCHES SunPro)
  target_compile_options(spng-static PRIVATE -erroff)
endif()
