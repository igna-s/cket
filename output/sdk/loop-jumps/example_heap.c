#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    unsigned rounds = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 3;
    if (rounds > 4)
        return 2;
    quant uint8_t *values = calloc(1, sizeof *values);
    if (!values)
        return 2;
    quant bool coin = superposition(0, 1);
    values[0] = coin ? 9 : 3;
    quant uint8_t output = 7, sum = 0;
    for (unsigned i = 0; i < rounds; ++i) {
        sum += values[0]++;
        memcpy((void *)&output, (const void *)values, 1);
    }
    unsigned input = measure(coin), result = measure(output), total = measure(sum);
    unsigned last = measure(values[0]);
    free((void *)values);
    unsigned initial = input ? 9 : 3;
    unsigned expected_sum = rounds * initial + rounds * (rounds ? rounds - 1 : 0) / 2;
    printf("rounds=%u output=%u sum=%u\n", rounds, result, total);
    return result != (rounds ? initial + rounds : 7) || total != expected_sum ||
           last != initial + rounds;
}
