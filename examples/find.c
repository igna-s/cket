#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    quant uint8_t answer = find(x, x * x == 9);
    printf("%u\n", (unsigned)measure(answer));
    return 0;
}
