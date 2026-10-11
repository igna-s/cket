#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t remaining = 20;
    unsigned iterations = 0;
    do {
        remaining -= 3;
        ++iterations;
    } while (iterations < 5);
    unsigned result = measure(remaining);
    printf("remaining=%u iterations=%u\n", result, iterations);
    return result != 5 || iterations != 5;
}
