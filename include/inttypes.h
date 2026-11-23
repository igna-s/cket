#ifndef CKET_INTTYPES_H
#define CKET_INTTYPES_H

#include_next <inttypes.h>

#ifdef __CKET__
#pragma GCC system_header

extern intmax_t __cket_strtoimax(const char *text, char **endptr, int base) __asm__("strtoimax");
extern uintmax_t __cket_strtoumax(const char *text, char **endptr, int base) __asm__("strtoumax");
#undef strtoimax
#undef strtoumax
#define strtoimax(text, endptr, base) __cket_strtoimax((text), (endptr), (base))
#define strtoumax(text, endptr, base) __cket_strtoumax((text), (endptr), (base))
#endif

#endif
