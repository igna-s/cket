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

extern char *__cket_strcpy(char *destination, const char *source) __asm__("strcpy");
#undef strcpy
#define strcpy(destination, source) __cket_strcpy((destination), (source))

extern char *__cket_strncpy(char *destination, const char *source, size_t count) __asm__("strncpy");
#undef strncpy
#define strncpy(destination, source, count) __cket_strncpy((destination), (source), (count))

extern char *__cket_strcat(char *destination, const char *source) __asm__("strcat");
#undef strcat
#define strcat(destination, source) __cket_strcat((destination), (source))

extern char *__cket_strncat(char *destination, const char *source, size_t count) __asm__("strncat");
#undef strncat
#define strncat(destination, source, count) __cket_strncat((destination), (source), (count))

extern int __cket_strncmp(const char *left, const char *right, size_t count) __asm__("strncmp");
#undef strncmp
#define strncmp(left, right, count) __cket_strncmp((left), (right), (count))

extern int __cket_strcmp(const char *left, const char *right) __asm__("strcmp");
#undef strcmp
#define strcmp(left, right) __cket_strcmp((left), (right))

extern size_t __cket_strlen(const char *string) __asm__("strlen");
#undef strlen
#define strlen(string) __cket_strlen((string))

extern size_t __cket_strspn(const char *string, const char *accept) __asm__("strspn");
#undef strspn
#define strspn(string, accept) __cket_strspn((string), (accept))

extern size_t __cket_strcspn(const char *string, const char *reject) __asm__("strcspn");
#undef strcspn
#define strcspn(string, reject) __cket_strcspn((string), (reject))

extern void *__cket_memchr(const void *storage, int character, size_t count) __asm__("memchr");
#undef memchr
#define memchr(storage, character, count) __cket_memchr((storage), (character), (count))

extern char *__cket_strchr(const char *string, int character) __asm__("strchr");
#undef strchr
#define strchr(string, character) __cket_strchr((string), (character))

extern char *__cket_strrchr(const char *string, int character) __asm__("strrchr");
#undef strrchr
#define strrchr(string, character) __cket_strrchr((string), (character))

extern char *__cket_strpbrk(const char *string, const char *set) __asm__("strpbrk");
#undef strpbrk
#define strpbrk(string, set) __cket_strpbrk((string), (set))

extern char *__cket_strstr(const char *string, const char *needle) __asm__("strstr");
#undef strstr
#define strstr(string, needle) __cket_strstr((string), (needle))

#undef memcmp
#define memcmp(left, right, count) __cket_memcmp((left), (right), (count))
#endif

#endif
