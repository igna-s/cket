#include <quantum.h>
#include <stdbool.h>
#include <stdio.h>

int main(void) {
    quant bool coin = false;
    bool found = false;
    unsigned attempts = 0;
    /* CPU feedback from explicit measurement, with a finite retry budget. */
    while (!found && attempts < 16) {
        reset(coin);
        fourier(coin);
        found = measure(coin);
        ++attempts;
    }
    reset(coin);
    printf("found=%u attempts=%u\n", (unsigned)found, attempts);
    /* Budget exhaustion is also a valid result, not proof of impossibility. */
    return attempts == 0 || attempts > 16 || (!found && attempts != 16);
}
