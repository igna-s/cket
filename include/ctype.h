#ifndef CKET_CTYPE_H
#define CKET_CTYPE_H

#include_next <ctype.h>

#ifdef __CKET__
#pragma GCC system_header

/* Preserve native libc calls for classical arguments. Distinct declarations
 * keep GCC from replacing quantum calls with host table loads or builtins. */
#define CKET_CTYPE_DECLARE(name) \
    int __cket_##name(int value) __asm__(#name) __attribute__((cket_intrinsic("ctype:" #name)))
CKET_CTYPE_DECLARE(isalnum);
CKET_CTYPE_DECLARE(isalpha);
CKET_CTYPE_DECLARE(isblank);
CKET_CTYPE_DECLARE(iscntrl);
CKET_CTYPE_DECLARE(isdigit);
CKET_CTYPE_DECLARE(isgraph);
CKET_CTYPE_DECLARE(islower);
CKET_CTYPE_DECLARE(isprint);
CKET_CTYPE_DECLARE(ispunct);
CKET_CTYPE_DECLARE(isspace);
CKET_CTYPE_DECLARE(isupper);
CKET_CTYPE_DECLARE(isxdigit);
CKET_CTYPE_DECLARE(tolower);
CKET_CTYPE_DECLARE(toupper);
#undef CKET_CTYPE_DECLARE

#undef isalnum
#undef isalpha
#undef isblank
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit
#undef tolower
#undef toupper
#define isalnum(value) __cket_isalnum(value)
#define isalpha(value) __cket_isalpha(value)
#define isblank(value) __cket_isblank(value)
#define iscntrl(value) __cket_iscntrl(value)
#define isdigit(value) __cket_isdigit(value)
#define isgraph(value) __cket_isgraph(value)
#define islower(value) __cket_islower(value)
#define isprint(value) __cket_isprint(value)
#define ispunct(value) __cket_ispunct(value)
#define isspace(value) __cket_isspace(value)
#define isupper(value) __cket_isupper(value)
#define isxdigit(value) __cket_isxdigit(value)
#define tolower(value) __cket_tolower(value)
#define toupper(value) __cket_toupper(value)
#endif

#endif
