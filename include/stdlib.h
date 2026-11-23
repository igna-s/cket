#ifndef CKET_STDLIB_H
#define CKET_STDLIB_H

#include_next <stdlib.h>

#ifdef __CKET__
#pragma GCC system_header

/* Preserve direct decimal calls until the quantum pass; CPU calls and
 * standard function addresses retain the ordinary libc ABI. */
extern int __cket_atoi(const char *text) __asm__("atoi");
extern long __cket_atol(const char *text) __asm__("atol");
extern long long __cket_atoll(const char *text) __asm__("atoll");
extern long __cket_strtol(const char *text, char **endptr, int base) __asm__("strtol");
extern long long __cket_strtoll(const char *text, char **endptr, int base) __asm__("strtoll");
extern unsigned long __cket_strtoul(const char *text, char **endptr, int base) __asm__("strtoul");
extern unsigned long long
__cket_strtoull(const char *text, char **endptr, int base) __asm__("strtoull");
#if defined(__USE_ISOC11) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
extern void *__cket_aligned_alloc(size_t alignment, size_t size) __asm__("aligned_alloc");
#endif
extern void *__cket_bsearch(const void *key,
                            const void *base,
                            size_t count,
                            size_t size,
                            int (*compare)(const void *, const void *)) __asm__("bsearch");
extern void __cket_qsort(void *base,
                         size_t count,
                         size_t size,
                         int (*compare)(const void *, const void *)) __asm__("qsort");
#undef atoi
#undef atol
#undef atoll
#undef strtol
#undef strtoll
#undef strtoul
#undef strtoull
#undef bsearch
#undef qsort
#define atoi(text) __cket_atoi((text))
#define atol(text) __cket_atol((text))
#define atoll(text) __cket_atoll((text))
#define strtol(text, endptr, base) __cket_strtol((text), (endptr), (base))
#define strtoll(text, endptr, base) __cket_strtoll((text), (endptr), (base))
#define strtoul(text, endptr, base) __cket_strtoul((text), (endptr), (base))
#define strtoull(text, endptr, base) __cket_strtoull((text), (endptr), (base))
#if defined(__USE_ISOC11) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#undef aligned_alloc
#define aligned_alloc(alignment, size) __cket_aligned_alloc((alignment), (size))
#endif
#define bsearch(key, base, count, size, compare) \
    __cket_bsearch((key), (base), (count), (size), (compare))
#define qsort(base, count, size, compare) __cket_qsort((base), (count), (size), (compare))
#endif

#endif
