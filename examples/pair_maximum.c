#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t a = 19;
    quant uint8_t b = 7;
    quant uint8_t maximum = a > b ? a : b;
    unsigned result = measure(maximum);
    unsigned first = measure(a);
    unsigned second = measure(b);
    printf("max(%u,%u)=%u\n", first, second, result);
    return first != 19 || second != 7 || result != 19;
}
