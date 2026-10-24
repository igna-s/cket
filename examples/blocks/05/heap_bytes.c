#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    quant uint8_t *values = calloc(3, sizeof *values);
    quant uint8_t *copy = calloc(3, sizeof *copy);
    if (!values || !copy)
        return 2;
    quant bool coin = superposition(0, 1);
    values[0] = coin ? 9 : 3;
    values[1] = 5;
    values[2] = 7;
    if (coin)
        phase(.31);

    size_t bytes = 3;
    void *destination = memcpy((void *)copy, (const void *)values, bytes);
    // Shift within one allocation using the original source bytes.
    memmove((unsigned char *)destination + 1, destination, bytes - 1);
    memset((void *)(values + 1), values[0], bytes - 1);
    if (copy[1] == 9)
        phase(.17);

    unsigned selected = measure(coin);
    unsigned a = measure(copy[0]), b = measure(copy[1]), c = measure(copy[2]);
    unsigned first = measure(values[0]), second = measure(values[1]), third = measure(values[2]);
    free((void *)values);
    free((void *)copy);
    unsigned expected = selected ? 9 : 3;
    printf("coin=%u copy={%u,%u,%u} fill={%u,%u,%u}\n", selected, a, b, c, first, second, third);
    return a != expected || b != expected || c != 5 || first != expected || second != expected ||
           third != expected;
}
