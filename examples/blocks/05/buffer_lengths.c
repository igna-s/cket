#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_prefix(quant uint8_t *destination, quant const uint8_t *source, size_t length) {
    memcpy((void *)destination, (const void *)source, length);
}

int main(int argc, char **argv) {
    size_t length = argc > 1 ? strtoul(argv[1], NULL, 10) : 2;
    if (length > 2)
        return 2;
    quant bool coin = superposition(0, 1);
    quant uint8_t input[2] = {0, 5}, output[2] = {7, 11};
    input[0] = coin ? 9 : 3;
    copy_prefix(output, input, length);
    size_t shifted = length ? length - 1 : 0;
    memmove((void *)(output + 1), (const void *)output, shifted);
    size_t filled = length ? 1 : 0;
    memset((void *)output, coin ? 1 : 2, filled);
    unsigned last = measure(output[1]);
    unsigned first = measure(output[0]);
    unsigned source = measure(input[0]);
    measure(input[1]);
    unsigned selected = measure(coin);
    printf("length=%zu output={%u,%u} source=%u\n", length, first, last, source);
    return first != (length ? (selected ? 1 : 2) : 7) || last != (length > 1 ? source : 11) ||
           source != (selected ? 9 : 3);
}
