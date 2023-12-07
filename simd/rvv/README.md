# Introduction

This `rvv` SIMD extension is based on RVV 1.0 version (frozen), implemented by utilizing RVV intrinsics. This implementation is now compatible with the newest rvv-intrinsic version of v0.12.0, which is determined by the compiler used.


## Build

The libjpeg-turbo can be cross-compiled targeting 64-bit RISC-V arch with V extension, so all we need to do is to provide a `toolchain.cmake` file:

```cmake
# borrowed from opencv: https://github.com/opencv/opencv/blob/4.x/platforms/linux/riscv64-clang.toolchain.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(RISCV_CLANG_BUILD_ROOT /path/to/my/llvm/build CACHE PATH "Path to CLANG for RISC-V cross compiler root directory")
set(RISCV_GCC_INSTALL_ROOT /opt/riscv CACHE PATH "Path to GCC for RISC-V cross compiler installation directory")
set(CMAKE_SYSROOT ${RISCV_GCC_INSTALL_ROOT}/sysroot CACHE PATH "RISC-V sysroot")

set(CLANG_TARGET_TRIPLE riscv64-unknown-linux-gnu)

set(CMAKE_C_COMPILER ${RISCV_CLANG_BUILD_ROOT}/bin/clang)
set(CMAKE_C_COMPILER_TARGET ${CLANG_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER ${RISCV_CLANG_BUILD_ROOT}/bin/clang++)
set(CMAKE_CXX_COMPILER_TARGET ${CLANG_TARGET_TRIPLE})
set(CMAKE_ASM_COMPILER ${RISCV_CLANG_BUILD_ROOT}/bin/clang)
set(CMAKE_ASM_COMPILER_TARGET ${CLANG_TARGET_TRIPLE})
# add crosscompiling emulator here to run test-suite locally
set(CMAKE_CROSSCOMPILING_EMULATOR /path/to/my/qemu-riscv64 "-cpu" "rv64,v=true,vlen=128")

# Don't run the linker on compiler check
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS "-march=rv64gcv --gcc-toolchain=${RISCV_GCC_INSTALL_ROOT} -w ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-march=rv64gcv --gcc-toolchain=${RISCV_GCC_INSTALL_ROOT} -w ${CXX_FLAGS}")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

Then following the commands below to build the libjpeg-turbo with `rvv`:
```bash
$ git clone https://github.com/isrc-cas/libjpeg-turbo.git -b riscv-dev
$ cd libjpeg-turbo
$ mkdir build && cd build
$ cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake -DENABLE_SHARED=FALSE ..
$ ninja
```

*(Note: The real machine we have only supports static binary, that is why `ENABLE_SHARED` is disabled here. )*

To verify the correctness of build artifacts, simply run:
```bash
$ ninja test
```

and if the end of test output looks like something below, it means successful build:
```bash
...
100% tests passed, 0 tests failed out of 287

Total Test time (real) = 609.20 sec
```

## Performance Comparison

Thanks for the first device that support rvv 1.0 ([Kendryte K230](https://www.canaan.io/product/k230)), we can make a comparison over the performance between the scale version and the rvv simd version on physical (real) machine.

As there is no build system on K230, we rewrite a test script to record the execution time of each test program based on `libjpeg-turbo/CMakeLists.txt`.

The comparison results are listed below, where we record execution time by `time` command provided by Linux:

*(Note: we remove the execution time of `cp` and `md5cmp`, which are meaningless to our comparison. )*

| test_name | scale | rvv | speedup |
|:---------:|:-----:|:---:|:-------:|
| tjunittest-static | 46.810 | 38.623 | **1.212** |
| tjunittest-static-alloc | 46.756 | 38.423 | **1.217** |
| tjunittest-static-yuv | 19.811 | 22.159 | 0.894 |
| tjunittest-static-yuv-alloc | 19.584 | 22.116 | 0.895 |
| tjunittest-static-lossless | 10.828 | 11.866 | 0.913 |
| tjunittest-static-lossless-alloc | 10.546 | 11.556 | 0.913 |
| tjunittest-static-bmp | 0.153 | 0.153 | 1 |
| tjbench-static-tile | 1.082 | 0.973 | **1.112** |
| tjbench-static-tilem | 1.058 | 0.973 | **1.087** |
|  example-8bit-static-compress | 0.057 | 0.033 | **1.727** |
| example-8bit-static-decompress | 0.032 | 0.031 | **1.032** |

As we only optimize the code for the 8-bit input sample, we do not take 12-bit or 16-bit tests into our consideration for now (but a future work). We only show the comparison of tests that ran enough time, because the performance improvement might be caused by measuring error.

In general, this implementation can be $1ms$ or $2ms$ faster than the scale version in most 8-bit tests, but this cannot prove the effectiveness of our implementation due to error caused by system or measures.
