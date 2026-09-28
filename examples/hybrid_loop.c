#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = 1;
    for (int i = 0; i < 8; ++i) {
        x += i;
    }
    printf("%u\n", (unsigned)measure(x));
    return 0;
}
