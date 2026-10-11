#include <quantum.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant bool flag = superposition(0, 1);
    quant uint8_t total = 0;
    for (unsigned i = 1; i <= 5; ++i) {
        /* No measurement or CPU effects inside these quantum branches. */
        if (flag) total += i;
        else total += 2 * i;
    }
    unsigned selected = measure(flag);
    unsigned result = measure(total);
    printf("flag=%u total=%u\n", selected, result);
    return selected > 1 || result != (selected ? 15u : 30u);
}
