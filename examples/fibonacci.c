#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t a = 0;
    quant uint8_t b = 1;
    for (unsigned i = 0; i < 8; ++i) {
        a += b;
        /* Swap distinct registers reversibly, without an extra register. */
        a ^= b;
        b ^= a;
        a ^= b;
    }
    unsigned first = measure(a);
    unsigned second = measure(b);
    printf("F(8)=%u F(9)=%u\n", first, second);
    return first != 21 || second != 34;
}
