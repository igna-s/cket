#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t total = 0;
    /* Add the entries of a 3-by-4 matrix containing 1 through 12. */
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            unsigned entry = row * 4 + column + 1;
            total += entry;
        }
    }
    unsigned result = measure(total);
    printf("matrix sum=%u\n", result);
    return result != 78;
}
