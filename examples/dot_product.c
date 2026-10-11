#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    const unsigned a[] = {1, 2, 3, 4};
    const unsigned b[] = {5, 6, 7, 8};
    quant uint8_t total = 0;
    /* CPU inputs and products, quantum accumulator; the answer fits 8 bits. */
    for (unsigned i = 0; i < 4; ++i) {
        unsigned product = a[i] * b[i];
        total += product;
    }
    unsigned result = measure(total);
    printf("dot product=%u\n", result);
    return result != 70;
}
