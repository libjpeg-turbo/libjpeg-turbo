/*
 * Loongson MMI optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Loongson Technology Corporation Limited, BeiJing.
 *                          All Rights Reserved.
 * Authors:  ZhuChen     <zhuchen@loongson.cn>
 *           CaiWanwei   <caiwanwei@loongson.cn>
 *           SunZhangzhi <sunzhangzhi-cq@loongson.cn>
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
 */

#define JPEG_INTERNALS
#include "../../jinclude.h"
#include "../../jpeglib.h"
#include "../../jdct.h"
#include "loongson-mmintrin.h"


/* Common code */

#if defined(_ABI64) && _MIPS_SIM == _ABI64
# define mips_reg       int64_t
# define PTR_ADDU       "daddu "
# define PTR_ADDIU      "daddiu "
# define PTR_ADDI       "daddi "
# define PTR_SUBU       "dsubu "
# define PTR_L          "ld "
# define PTR_LI         "dli "
# define PTR_S          "sd "
# define PTR_SRA        "dsra "
# define PTR_SRL        "dsrl "
# define PTR_SLL        "dsll "
# define MTC1           "dmtc1 "
#else
# define mips_reg       int32_t
# define PTR_ADDU       "addu "
# define PTR_ADDIU      "addiu "
# define PTR_ADDI       "addi "
# define PTR_SUBU       "subu "
# define PTR_L          "lw "
# define PTR_LI         "li "
# define PTR_S          "sw "
# define PTR_SRA        "sra "
# define PTR_SRL        "srl "
# define PTR_SLL        "sll "
# define MTC1           "mtc1 "
#endif

#define SIZEOF_MMWORD  8
#define BYTE_BIT  8
#define WORD_BIT  16
#define SCALEBITS  16

#define _uint64_set_pi8(a, b, c, d, e, f, g, h) \
  (((uint64_t)(uint8_t)a << 56) | \
   ((uint64_t)(uint8_t)b << 48) | \
   ((uint64_t)(uint8_t)c << 40) | \
   ((uint64_t)(uint8_t)d << 32) | \
   ((uint64_t)(uint8_t)e << 24) | \
   ((uint64_t)(uint8_t)f << 16) | \
   ((uint64_t)(uint8_t)g << 8)  | \
   ((uint64_t)(uint8_t)h))
#define _uint64_set1_pi8(a)  _uint64_set_pi8(a, a, a, a, a, a, a, a)
#define _uint64_set_pi16(a, b, c, d)  (((uint64_t)(uint16_t)a << 48) | \
                                       ((uint64_t)(uint16_t)b << 32) | \
                                       ((uint64_t)(uint16_t)c << 16) | \
                                       ((uint64_t)(uint16_t)d))
#define _uint64_set1_pi16(a)  _uint64_set_pi16(a, a, a, a)
#define _uint64_set_pi32(a, b)  (((uint64_t)(uint32_t)a << 32) | \
                                 ((uint64_t)(uint32_t)b))

#define get_const_value(index)  (*(__m64 *)&const_value[index])
