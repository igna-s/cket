#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    quant bool flag = (x * x == 9);
    if (flag) {
        phase_flip();
    }
    x += 5;
    printf("%u\n", (unsigned)measure(x));
    return 0;
}
