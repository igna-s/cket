#include <quantum.h>
#include <stdio.h>

void accumulate(quant uint8_t values[2]) {
    values[0] += values[1];
    for (int i = 0; i < 2; ++i) {
        values[i] ^= 3;
    }
}

int main(void) {
    quant uint8_t values[2] = {7, 2};
    accumulate(values);
    printf("%u %u\n", (unsigned)measure(values[0]),
                         (unsigned)measure(values[1]));
    return 0;
}
