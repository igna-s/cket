#include <quantum.h>
#include <stdint.h>
#include <stdio.h>
#include "read_input.h"

int main(void) {
    /* Validate CPU input before creating quantum storage. */
    unsigned count = read_unsigned(16);
    if (count > 16) {
        fputs("Expected a count in 0..16.\n", stderr);
        return 1;
    }
    quant uint8_t total = 0;
    for (unsigned i = 1; i <= count; ++i) {
        total += i;
    }
    unsigned result = measure(total);
    printf("count=%u sum=%u\n", count, result);
    return result != count * (count + 1) / 2;
}
