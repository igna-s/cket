#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t value = 250;
    value += 20;  /* Unsigned updates wrap modulo 256. */
    unsigned added = measure(value);
    value -= 30;
    unsigned subtracted = measure(value);
    value ^= 15;
    unsigned masked = measure(value);
    printf("add=%u subtract=%u xor=%u\n", added, subtracted, masked);
    return added != 14 || subtracted != 240 || masked != 255;
}
