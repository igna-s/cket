#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool a = 0;
    quant bool b = 0;
    fourier(a);
    if (a) b ^= 1;
    unsigned first = measure(a);
    unsigned second = measure(b);
    printf("%u %u\n", first, second);
    return first != second;
}
