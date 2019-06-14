/* libjpeg-turbo build number */
#define BUILD "20190613"

/* Compiler's inline keyword */
#undef inline

/* How to obtain function inlining. */
#if defined(_WIN32)
#define INLINE __forceinline
#else
#define INLINE  __inline__ __attribute__((always_inline))
#endif

/* Define to the full name of this package. */
#define PACKAGE_NAME "libjpeg-turbo"

/* Version number of package */
#define VERSION "2.0.2"

/* The size of `size_t', as reported by the compiler through the
 * builtin macro __SIZEOF_SIZE_T__. If the compiler does not
 * report __SIZEOF_SIZE_T__ add a custom rule for the compiler
 * here. Adapted from libjpeg-turbo config files for the skia 
 * library. */
#ifdef __SIZEOF_SIZE_T__
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#elif __WORDSIZE==64 || defined(_WIN64)
#define SIZEOF_SIZE_T 8
#else
#define SIZEOF_SIZE_T 4
#endif

/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#if !defined(_WIN32)
#define HAVE_BUILTIN_CTZL
#endif

/* Define to 1 if you have the <intrin.h> header file. */
#if defined(_WIN32)
#define HAVE_INTRIN_H
#endif

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif
