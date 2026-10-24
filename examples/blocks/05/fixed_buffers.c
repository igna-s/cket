#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void evolve(quant uint8_t *values, unsigned rounds) {
    while (rounds--) {
        memmove((void *)(values + 1), (const void *)values, 2);
        memset((void *)values, *values + 1, 1);
    }
}

int main(int argc, char **argv) {
    unsigned rounds = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 2;
    if (rounds > 3)
        return 2;
    quant bool coin = superposition(0, 1);
    quant uint8_t values[3] = {0, 5, 7};
    values[0] = coin ? 9 : 3;
    evolve(values, rounds);
    unsigned last = measure(values[2]);
    unsigned middle = measure(values[1]);
    unsigned first = measure(values[0]);
    unsigned input = measure(coin), initial = input ? 9 : 3;
    printf("rounds=%u values={%u,%u,%u}\n", rounds, first, middle, last);
    return first != initial + rounds || middle != (rounds ? initial + rounds - 1 : 5) ||
           last != (rounds > 1 ? initial + rounds - 2
                    : rounds   ? 5
                               : 7);
}
