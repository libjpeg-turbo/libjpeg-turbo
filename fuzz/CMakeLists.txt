if(NOT ENABLE_STATIC)
  message(FATAL_ERROR "Fuzz targets require static libraries.")
endif()
if(NOT WITH_TURBOJPEG)
  message(FATAL_ERROR "Fuzz targets require the TurboJPEG API library.")
endif()

# Add include directories for libjpeg headers
include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})

set(FUZZ_BINDIR "" CACHE PATH
  "Directory into which fuzz targets should be installed")
if(NOT FUZZ_BINDIR)
  message(FATAL_ERROR "FUZZ_BINDIR must be specified.")
endif()
message(STATUS "FUZZ_BINDIR = ${FUZZ_BINDIR}")

set(FUZZ_LIBRARY "" CACHE STRING
  "Path to fuzzer library or flags necessary to link with it")
if(NOT FUZZ_LIBRARY)
  message(FATAL_ERROR "FUZZ_LIBRARY must be specified.")
endif()
message(STATUS "FUZZ_LIBRARY = ${FUZZ_LIBRARY}")

enable_language(CXX)

set(EFFECTIVE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UC}}")
message(STATUS "C++ Compiler flags = ${EFFECTIVE_CXX_FLAGS}")

add_executable(cjpeg_fuzzer${FUZZER_SUFFIX} cjpeg.cc ../src/cdjpeg.c
  ../src/rdbmp.c ../src/rdgif.c ../src/rdppm.c ../src/rdswitch.c
  ../src/rdtarga.c)
set_property(TARGET cjpeg_fuzzer${FUZZER_SUFFIX} PROPERTY COMPILE_FLAGS
  ${COMPILE_FLAGS})
target_link_libraries(cjpeg_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY} jpeg-static)
install(TARGETS cjpeg_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

macro(add_fuzz_target target source_file)
  add_executable(${target}_fuzzer${FUZZER_SUFFIX} ${source_file})
  target_link_libraries(${target}_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
    turbojpeg-static)
  install(TARGETS ${target}_fuzzer${FUZZER_SUFFIX}
    RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)
endmacro()

add_fuzz_target(compress compress.cc)

add_fuzz_target(compress_yuv compress_yuv.cc)

add_fuzz_target(compress_lossless compress_lossless.cc)

add_fuzz_target(compress12 compress12.cc)

add_fuzz_target(compress12_lossless compress12_lossless.cc)

add_fuzz_target(compress16_lossless compress16_lossless.cc)

# NOTE: This target is named libjpeg_turbo_fuzzer instead of decompress_fuzzer
# in order to preserve the corpora from Google's OSS-Fuzz target for
# libjpeg-turbo, which this target replaces.
add_fuzz_target(libjpeg_turbo decompress.cc)

add_fuzz_target(decompress_yuv decompress_yuv.cc)

add_fuzz_target(transform transform.cc)

# Raw libjpeg API fuzzers (link against jpeg-static, not turbojpeg-static)
# These target code paths not covered by TurboJPEG API fuzzers.

# Raw libjpeg decompression fuzzer - targets slow-path Huffman decode,
# multiple colorspaces, scaling, and various decompression options.
add_executable(decompress_libjpeg_fuzzer${FUZZER_SUFFIX} decompress_libjpeg.cc)
target_link_libraries(decompress_libjpeg_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS decompress_libjpeg_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Progressive/buffered-image mode fuzzer - targets jpeg_start_output(),
# jpeg_finish_output(), jpeg_consume_input() for incremental decoding.
add_executable(progressive_fuzzer${FUZZER_SUFFIX} progressive.cc)
target_link_libraries(progressive_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS progressive_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Marker processing fuzzer - targets jpeg_save_markers(),
# jpeg_set_marker_processor(), jpeg_read_icc_profile().
add_executable(marker_fuzzer${FUZZER_SUFFIX} marker.cc)
target_link_libraries(marker_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS marker_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# DCT coefficient access fuzzer - targets jpeg_read_coefficients()
# and virtual block array management.
add_executable(coefficient_fuzzer${FUZZER_SUFFIX} coefficient.cc)
target_link_libraries(coefficient_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS coefficient_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Partial decode fuzzer - targets jpeg_skip_scanlines(),
# jpeg_crop_scanline() for partial image decoding.
add_executable(partial_fuzzer${FUZZER_SUFFIX} partial.cc)
target_link_libraries(partial_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS partial_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Error recovery fuzzer - targets jpeg_abort(), jpeg_abort_decompress(),
# error handling paths, and cleanup after partial decode.
add_executable(error_recovery_fuzzer${FUZZER_SUFFIX} error_recovery.cc)
target_link_libraries(error_recovery_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS error_recovery_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Raw data fuzzer - targets jpeg_read_raw_data() for raw YUV plane access.
add_executable(raw_data_fuzzer${FUZZER_SUFFIX} raw_data.cc)
target_link_libraries(raw_data_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS raw_data_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Transcode fuzzer - targets jpeg_read_coefficients(), jpeg_write_coefficients(),
# jpeg_copy_critical_parameters() for lossless transcoding.
add_executable(transcode_fuzzer${FUZZER_SUFFIX} transcode.cc)
target_link_libraries(transcode_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS transcode_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Raw libjpeg compression fuzzer - targets jpeg_write_scanlines(),
# jpeg_write_marker(), jpeg_write_icc_profile(), Huffman table generation.
add_executable(compress_libjpeg_fuzzer${FUZZER_SUFFIX} compress_libjpeg.cc)
target_link_libraries(compress_libjpeg_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS compress_libjpeg_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Raw YUV compression fuzzer - targets jpeg_write_raw_data() for
# raw downsampled YUV plane compression.
add_executable(compress_raw_data_fuzzer${FUZZER_SUFFIX} compress_raw_data.cc)
target_link_libraries(compress_raw_data_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS compress_raw_data_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

# Restart marker resync fuzzer - targets jpeg_resync_to_restart() and
# custom resync handler for restart marker error recovery.
add_executable(resync_fuzzer${FUZZER_SUFFIX} resync.cc)
target_link_libraries(resync_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS resync_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)
