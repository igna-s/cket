#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    quant uint8_t *heap = calloc(2, sizeof *heap);
    if (!heap)
        return 2;
    quant bool coin = superposition(0, 1);
    quant uint8_t input[2] = {0, 5}, output[2] = {0, 0};
    input[0] = coin ? 9 : 3;
    if (coin)
        phase(.31);
    size_t bytes = 2;
    memcpy((void *)heap, (const void *)input, bytes);
    // The heap owns its result even after the fixed source changes.
    input[1] = 7;
    memcpy((void *)output, (const void *)heap, bytes);
    free((void *)heap);
    quant uint8_t saved = output[0] ^ 1;
    if (saved == 8)
        phase(.17);
    unsigned a = measure(output[0]), b = measure(output[1]);
    unsigned x = measure(input[0]), y = measure(input[1]);
    unsigned selected = measure(coin);
    printf("coin=%u input={%u,%u} output={%u,%u}\n", selected, x, y, a, b);
    return a != (selected ? 9u : 3u) || b != 5 || x != a || y != 7;
}
