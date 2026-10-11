#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t a = 19;
    quant uint8_t b = 7;
    /* Compute into new storage: both inputs remain available. */
    quant uint8_t minimum = a < b ? a : b;
    unsigned result = measure(minimum);
    unsigned first = measure(a);
    unsigned second = measure(b);
    printf("min(%u,%u)=%u\n", first, second, result);
    return first != 19 || second != 7 || result != 7;
}
