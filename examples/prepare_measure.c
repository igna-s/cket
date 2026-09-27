#include <stdint.h>
#include <stdio.h>
#include <quantum.h>

int main(void) {
    int classical = 10;
    quant uint8_t x = superposition(0, 255);
    uint8_t result = measure(x);
    printf("classical=%d quantum=%u\n", classical, (unsigned)result);
    return 0;
}
