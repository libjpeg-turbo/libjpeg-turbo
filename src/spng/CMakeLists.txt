include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DSPNG_STATIC -DSPNG_USE_MINIZ -D_LARGEFILE64_SOURCE)

add_library(spng-static OBJECT spng.c miniz.c)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(spng-static PRIVATE -w)
endif()
if(CMAKE_C_COMPILER_ID MATCHES SunPro)
  target_compile_options(spng-static PRIVATE -erroff)
endif()
