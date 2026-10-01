#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = 7;
    fourier(x);
    inverse_fourier(x);
    printf("%u\n", (unsigned)measure(x));
    return 0;
}
