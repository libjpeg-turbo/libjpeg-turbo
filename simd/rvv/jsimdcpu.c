/*
 * Copyright (C) 2025, Filip Wasil. Samsung.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * 64-bit Arm architecture.
 */

 #if defined(__linux__)
#include <asm/hwcap.h>
#include <asm/hwprobe.h>
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <asm/unistd.h>

#ifndef __NR_riscv_hwprobe
#define __NR_riscv_hwprobe 258
#endif

// This manual declaration is safe because:
// It matches the standard Linux ABI (long return, varargs)
// It's essentially documenting what the system already provides
// The actual implementation will be linked from glibc
#ifndef SYS_syscall
long syscall(long number, ...);
#endif

int
has_compliant_vsetvli (void);

static int
is_rvv_1_0_available ()
{
  struct riscv_hwprobe pair = {RISCV_HWPROBE_KEY_IMA_EXT_0, 0};

  if (syscall (__NR_riscv_hwprobe, &pair, 1, 0, 0, 0) < 0)
  {
    // At this point we already checked AT_HWCAP and we know we have
    // some version of RVV to our dispose. If this version of kernel
    // is failing on syscall __NR_riscv_hwprobe, we will check the RVV
    // version by looking at the vsetvli behaviour.
    return has_compliant_vsetvli ();
  }
  return (pair.value & RISCV_HWPROBE_IMA_V);
}

#endif

#include "../jsimd.h"

HIDDEN unsigned int
jpeg_simd_cpu_support(void)
{
#if defined(__linux__) && __riscv_v >= 1000000 && __riscv_v < 2000000
  return (getauxval (AT_HWCAP) & COMPAT_HWCAP_ISA_V
    && is_rvv_1_0_available ()) ? JSIMD_RVV : 0;
#endif
  return 0;
}
