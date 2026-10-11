#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t total = 0;
    unsigned i = 1;
    while (i <= 10) {
        total += i;
        ++i;
    }
    /* The CPU loop may have zero iterations: consume the owner afterwards. */
    unsigned result = measure(total);
    printf("while sum=%u iterations=%u\n", result, i - 1);
    return result != 55 || i != 11;
}
