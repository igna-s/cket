#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t total = 0;
    for (unsigned i = 1; i <= 10; ++i) {
        total += i;
    }
    unsigned result = measure(total);
    printf("sum(1..10)=%u\n", result);
    return result != 55;
}
