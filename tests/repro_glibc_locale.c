#define _XOPEN_SOURCE 700
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((noinline)) static void project_leak(void) {
    volatile unsigned char *value = malloc(43);
    if (!value)
        exit(2);
    *value = 17;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    if (!strcmp(argv[1], "project")) {
        project_leak();
        return 0;
    }
    locale_t locale = newlocale(LC_CTYPE_MASK, "C.UTF-8", 0);
    if (!locale) {
        perror("newlocale");
        return 2;
    }
    if (!strcmp(argv[1], "locale-handle"))
        return 0;
    freelocale(locale);
    return 0;
}
