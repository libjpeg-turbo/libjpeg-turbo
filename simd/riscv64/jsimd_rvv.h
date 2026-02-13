/*
 * Copyright (C) 2014, 2026, D. R. Commander.
 * Copyright (C) 2026, Olaf Bernstein.
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

#define TRANSPOSE_8x8(row, col) { \
  vint8m1_t veven = __riscv_vmv_v_x_i8m1(-86, __riscv_vsetvlmax_e8m1()); \
  vint8m1_t vodd = __riscv_vnot(veven, __riscv_vsetvlmax_e8m1()); \
  size_t VL; \
  vint64m1_t r04l, r04h, r15l, r15h, r26l, r26h, r37l, r37h; \
  vint32m1_t c01e, c01o, c23e, c23o, c45e, c45o, c67e, c67o; \
  \
  /* row##0 = 0A 0B 0C 0D 0E 0F 0G 0H \
   * row##1 = 1A 1B 1C 1D 1E 1F 1G 1H \
   * row##2 = 2A 2B 2C 2D 2E 2F 2G 2H \
   * row##3 = 3A 3B 3C 3D 3E 3F 3G 3H \
   * row##4 = 4A 4B 4C 4D 4E 4F 4G 4H \
   * row##5 = 5A 5B 5C 5D 5E 5F 5G 5H \
   * row##6 = 6A 6B 6C 6D 6E 6F 6G 6H \
   * row##7 = 7A 7B 7C 7D 7E 7F 7G 7H \
   */ \
  \
  VL = __riscv_vsetvlmax_e64m1(); \
  r04l = __riscv_vslide1up_mu(__riscv_vreinterpret_b64(veven), \
                              __riscv_vreinterpret_i64m1(row##0), \
                              __riscv_vreinterpret_i64m1(row##4), 0, VL); \
                                              /* 0A 0B 0C 0D | 4A 4B 4C 4D */ \
  r15l = __riscv_vslide1up_mu(__riscv_vreinterpret_b64(veven), \
                              __riscv_vreinterpret_i64m1(row##1), \
                              __riscv_vreinterpret_i64m1(row##5), 0, VL); \
                                              /* 1A 1B 1C 1D | 5A 5B 5C 5D */ \
  r26l = __riscv_vslide1up_mu(__riscv_vreinterpret_b64(veven), \
                              __riscv_vreinterpret_i64m1(row##2), \
                              __riscv_vreinterpret_i64m1(row##6), 0, VL); \
                                              /* 2A 2B 2C 2D | 6A 6B 6C 6D */ \
  r37l = __riscv_vslide1up_mu(__riscv_vreinterpret_b64(veven), \
                              __riscv_vreinterpret_i64m1(row##3), \
                              __riscv_vreinterpret_i64m1(row##7), 0, VL); \
                                              /* 3A 3B 3C 3D | 7A 7B 7C 7D */ \
  r04h = __riscv_vslide1down_mu(__riscv_vreinterpret_b64(vodd), \
                                __riscv_vreinterpret_i64m1(row##4), \
                                __riscv_vreinterpret_i64m1(row##0), 0, VL); \
                                              /* 0E 0F 0G 0H | 4E 4F 4G 4H */ \
  r15h = __riscv_vslide1down_mu(__riscv_vreinterpret_b64(vodd), \
                                __riscv_vreinterpret_i64m1(row##5), \
                                __riscv_vreinterpret_i64m1(row##1), 0, VL); \
                                              /* 1E 1F 1G 1H | 5E 5F 5G 5H */ \
  r26h = __riscv_vslide1down_mu(__riscv_vreinterpret_b64(vodd), \
                                __riscv_vreinterpret_i64m1(row##6), \
                                __riscv_vreinterpret_i64m1(row##2), 0, VL); \
                                              /* 2E 2F 2G 2H | 6E 6F 6G 6H */ \
  r37h = __riscv_vslide1down_mu(__riscv_vreinterpret_b64(vodd), \
                                __riscv_vreinterpret_i64m1(row##7), \
                                __riscv_vreinterpret_i64m1(row##3), 0, VL); \
                                              /* 3E 3F 3G 3H | 7E 7F 7G 7H */ \
  \
  VL = __riscv_vsetvlmax_e32m1(); \
  c23e = __riscv_vslide1down_mu(__riscv_vreinterpret_b32(vodd), \
                                __riscv_vreinterpret_i32m1(r26l), \
                                __riscv_vreinterpret_i32m1(r04l), 0, VL); \
                                          /* 0C 0D | 2C 2D | 4C 4D | 6C 6D */ \
  c23o = __riscv_vslide1down_mu(__riscv_vreinterpret_b32(vodd), \
                                __riscv_vreinterpret_i32m1(r37l), \
                                __riscv_vreinterpret_i32m1(r15l), 0, VL); \
                                          /* 1C 1D | 3C 3D | 5C 5D | 7C 7D */ \
  c67e = __riscv_vslide1down_mu(__riscv_vreinterpret_b32(vodd), \
                                __riscv_vreinterpret_i32m1(r26h), \
                                __riscv_vreinterpret_i32m1(r04h), 0, VL); \
                                          /* 0G 0H | 2G 2H | 4G 4H | 6G 6H */ \
  c67o = __riscv_vslide1down_mu(__riscv_vreinterpret_b32(vodd), \
                                __riscv_vreinterpret_i32m1(r37h), \
                                __riscv_vreinterpret_i32m1(r15h), 0, VL); \
                                          /* 1G 1H | 3G 3H | 5G 5H | 7G 7H */ \
  c01e = __riscv_vslide1up_mu(__riscv_vreinterpret_b32(veven), \
                              __riscv_vreinterpret_i32m1(r04l), \
                              __riscv_vreinterpret_i32m1(r26l), 0, VL); \
                                          /* 0A 0B | 2A 2B | 4A 4B | 6A 6B */ \
  c01o = __riscv_vslide1up_mu(__riscv_vreinterpret_b32(veven), \
                              __riscv_vreinterpret_i32m1(r15l), \
                              __riscv_vreinterpret_i32m1(r37l), 0, VL); \
                                          /* 1A 1B | 3A 3B | 5A 5B | 7A 7B */ \
  c45e = __riscv_vslide1up_mu(__riscv_vreinterpret_b32(veven), \
                              __riscv_vreinterpret_i32m1(r04h), \
                              __riscv_vreinterpret_i32m1(r26h), 0, VL); \
                                          /* 0E 0F | 2E 2F | 4E 4F | 6E 6F */ \
  c45o = __riscv_vslide1up_mu(__riscv_vreinterpret_b32(veven), \
                              __riscv_vreinterpret_i32m1(r15h), \
                              __riscv_vreinterpret_i32m1(r37h), 0, VL); \
                                          /* 1E 1F | 3E 3F | 5E 5F | 7E 7F */ \
  \
  VL = __riscv_vsetvlmax_e16m1(); \
  col##0 = __riscv_vslide1up_mu(__riscv_vreinterpret_b16(veven), \
                                __riscv_vreinterpret_i16m1(c01e), \
                                __riscv_vreinterpret_i16m1(c01o), 0, VL); \
                                                /* 0A 1A 2A 3A 4A 5A 6A 7A */ \
  col##2 = __riscv_vslide1up_mu(__riscv_vreinterpret_b16(veven), \
                                __riscv_vreinterpret_i16m1(c23e), \
                                __riscv_vreinterpret_i16m1(c23o), 0, VL); \
                                                /* 0C 1C 2C 3C 4C 5C 6C 7C */ \
  col##4 = __riscv_vslide1up_mu(__riscv_vreinterpret_b16(veven), \
                                __riscv_vreinterpret_i16m1(c45e), \
                                __riscv_vreinterpret_i16m1(c45o), 0, VL); \
                                                /* 0E 1E 2E 3E 4E 5E 6E 7E */ \
  col##6 = __riscv_vslide1up_mu(__riscv_vreinterpret_b16(veven), \
                                __riscv_vreinterpret_i16m1(c67e), \
                                __riscv_vreinterpret_i16m1(c67o), 0, VL); \
                                                /* 0G 1G 2G 3G 4G 5G 6G 7G */ \
  col##1 = __riscv_vslide1down_mu(__riscv_vreinterpret_b16(vodd), \
                                  __riscv_vreinterpret_i16m1(c01o), \
                                  __riscv_vreinterpret_i16m1(c01e), 0, VL); \
                                                /* 0B 1B 2B 3B 4B 5B 6B 7B */ \
  col##3 = __riscv_vslide1down_mu(__riscv_vreinterpret_b16(vodd), \
                                  __riscv_vreinterpret_i16m1(c23o), \
                                  __riscv_vreinterpret_i16m1(c23e), 0, VL); \
                                                /* 0D 1D 2D 3D 4D 5D 6D 7D */ \
  col##5 = __riscv_vslide1down_mu(__riscv_vreinterpret_b16(vodd), \
                                  __riscv_vreinterpret_i16m1(c45o), \
                                  __riscv_vreinterpret_i16m1(c45e), 0, VL); \
                                                /* 0F 1F 2F 3F 4F 5F 6F 7F */ \
  col##7 = __riscv_vslide1down_mu(__riscv_vreinterpret_b16(vodd), \
                                  __riscv_vreinterpret_i16m1(c67o), \
                                  __riscv_vreinterpret_i16m1(c67e), 0, VL); \
                                                /* 0H 1H 2H 3H 4H 5H 6H 7H */ \
}
