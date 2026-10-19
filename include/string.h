#ifndef CKET_STRING_H
#define CKET_STRING_H

#include_next <string.h>

#ifdef __CKET__
#pragma GCC system_header

/* Keep checked byte operations visible before GCC folds them into host
 * loads or stores. Classical calls still link to the real libc
 * symbol, and taking the standard function's address retains the C interface. */
extern int __cket_memcmp(const void *left, const void *right, size_t count) __asm__("memcmp");

extern void *__cket_memset(void *destination, int value, size_t count) __asm__("memset");
#undef memset
#define memset(destination, value, count) __cket_memset((destination), (value), (count))

extern int __cket_strncmp(const char *left, const char *right, size_t count) __asm__("strncmp");
#undef strncmp
#define strncmp(left, right, count) __cket_strncmp((left), (right), (count))

extern int __cket_strcmp(const char *left, const char *right) __asm__("strcmp");
#undef strcmp
#define strcmp(left, right) __cket_strcmp((left), (right))

extern size_t __cket_strlen(const char *string) __asm__("strlen");
#undef strlen
#define strlen(string) __cket_strlen((string))

#undef memcmp
#define memcmp(left, right, count) __cket_memcmp((left), (right), (count))
#endif

#endif
