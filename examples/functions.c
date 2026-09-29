#include <quantum.h>
#include <stdio.h>

quant bool square_is_nine(quant const uint8_t value) {
    return value * value == 9;
}

void increase(quant uint8_t value, unsigned amount) {
    value += amount;
}

int main(void) {
    quant uint8_t x = superposition(0, 15);
    quant uint8_t answer = find(x, square_is_nine(x));
    increase(answer, 1);
    printf("%u\n", (unsigned)measure(answer));
    return 0;
}
