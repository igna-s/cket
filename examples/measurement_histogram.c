#include <quantum.h>
#include <stdbool.h>
#include <stdio.h>

int main(void) {
    quant bool coin = false;
    unsigned zeros = 0, ones = 0, trials = 0;
    while (trials < 32) {
        reset(coin);
        fourier(coin);
        if (measure(coin))
            ++ones;
        else
            ++zeros;
        ++trials;
    }
    reset(coin);
    printf("zeros=%u ones=%u trials=%u\n", zeros, ones, trials);
    return zeros + ones != 32 || trials != 32;
}
