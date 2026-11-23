#ifndef CKET_STDIO_H
#define CKET_STDIO_H

#include_next <stdio.h>

#ifdef __CKET__
#pragma GCC system_header

extern int __cket_snprintf(char *buffer, size_t size, const char *format, ...) __asm__("snprintf");
extern int __cket_sscanf(const char *buffer, const char *format, ...) __asm__("sscanf");
#undef snprintf
#define snprintf(buffer, size, ...) __cket_snprintf((buffer), (size), __VA_ARGS__)
#undef sscanf
#define sscanf(buffer, ...) __cket_sscanf((buffer), __VA_ARGS__)
#endif

#endif
