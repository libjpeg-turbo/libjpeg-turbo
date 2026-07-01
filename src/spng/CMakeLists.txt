if(NOT WITH_SYSTEM_ZLIB)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/zlib)

  add_library(zlib-static OBJECT zlib/adler32.c zlib/compress.c zlib/crc32.c
    zlib/deflate.c zlib/inffast.c zlib/inflate.c zlib/inftrees.c zlib/trees.c
    zlib/zutil.c)

  if(NOT WIN32 AND (CMAKE_POSITION_INDEPENDENT_CODE OR ENABLE_SHARED))
    set_target_properties(zlib-static PROPERTIES POSITION_INDEPENDENT_CODE 1)
  endif()

  if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(zlib-static PRIVATE -w)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES SunPro)
    target_compile_options(zlib-static PRIVATE -erroff)
  endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DSPNG_STATIC)

add_library(spng-static OBJECT spng.c)

if(NOT WIN32 AND (CMAKE_POSITION_INDEPENDENT_CODE OR ENABLE_SHARED))
  set_target_properties(spng-static PROPERTIES POSITION_INDEPENDENT_CODE 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(spng-static PRIVATE -w)
endif()
if(CMAKE_C_COMPILER_ID MATCHES SunPro)
  target_compile_options(spng-static PRIVATE -erroff)
endif()
