include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src/spng)
add_definitions(-DSPNG_STATIC -DSPNG_USE_MINIZ -D_LARGEFILE64_SOURCE)

add_library(spng OBJECT ../../src/spng/spng.c ../../src/spng/miniz.c)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(spng PRIVATE -w)
endif()
if(CMAKE_C_COMPILER_ID MATCHES SunPro)
  target_compile_options(spng PRIVATE -erroff)
endif()
