#include <quantum.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    bool found = search(x, x >= 8 && x <= 13 && x % 3 == 1, 80);
    if (found) {
        unsigned candidate = measure(x);
        printf("candidate=%u\n", candidate);
        return candidate < 8 || candidate > 13 || candidate % 3 != 1;
    }
    reset(x);
    puts("Search budget exhausted");
    return 0;
}
