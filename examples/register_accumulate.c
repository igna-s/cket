#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t term = 3;
    quant uint8_t total = 0;
    for (unsigned i = 0; i < 5; ++i) {
        total += term;
    }
    unsigned sum = measure(total);
    /* Measurement preserves the basis value; inverse updates restore zero. */
    for (unsigned i = 0; i < 5; ++i) {
        total -= term;
    }
    unsigned restored = measure(total);
    unsigned source = measure(term);
    printf("sum=%u restored=%u term=%u\n", sum, restored, source);
    return sum != 15 || restored != 0 || source != 3;
}
