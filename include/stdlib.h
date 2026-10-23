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
#undef atoi
#undef atol
#undef atoll
#define atoi(text) __cket_atoi((text))
#define atol(text) __cket_atol((text))
#define atoll(text) __cket_atoll((text))
#endif

#endif
