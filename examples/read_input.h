#ifndef CKET_EXAMPLE_READ_INPUT_H
#define CKET_EXAMPLE_READ_INPUT_H

#include <ctype.h>
#include <stdio.h>

/* Read one whitespace-delimited decimal token, checking the bound before
   multiplication. Consume invalid tokens completely, without numeric wraparound
   or a fixed-size input buffer. Signs and nondecimal characters are rejected.
   Return maximum+1 on failure; the example bounds are at most 255. */
static unsigned read_unsigned(unsigned maximum) {
    int ch;
    do {
        ch = getchar();
    } while (ch != EOF && isspace((unsigned char)ch));
    if (ch == EOF) return maximum + 1;

    unsigned result = 0;
    int valid = 1;
    do {
        if (ch < '0' || ch > '9') {
            valid = 0;
        } else if (valid) {
            unsigned digit = (unsigned)(ch - '0');
            if (result > maximum / 10 ||
                (result == maximum / 10 && digit > maximum % 10)) {
                valid = 0;
            } else {
                result = result * 10 + digit;
            }
        }
        ch = getchar();
    } while (ch != EOF && !isspace((unsigned char)ch));

    return valid ? result : maximum + 1;
}

#endif
