#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    bool found = search(x, x * x == 1000, 80);
    if (found) printf("%u\n", (unsigned)measure(x));
    else {
        reset(x);
        puts("Search budget exhausted");
    }
    return found ? 1 : 0;
}
