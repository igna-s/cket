#include <quantum.h>
#include <stdio.h>

void scan_buffer(quant const uint8_t *values,
                 size_t count,
                 quant uint8_t total,
                 quant uint8_t parity) {
    for (size_t i = 0; i < count; ++i) {
        total += values[i];
        parity ^= values[i];
    }
}

int main(void) {
    size_t count = 3, slot = 1;
    quant uint8_t values[3] = {superposition(0, 3), 3, 7};
    quant uint8_t total = 0, parity = 0;
    scan_buffer(values, count, total, parity);
    quant uint8_t picked = values[slot] ^ 1;
    slot = 0; // Saved cleanup must keep the index used by the read.
    unsigned chosen = measure(picked);
    unsigned sum = measure(total), folded = measure(parity);
    unsigned last = measure(values[2]);
    unsigned middle = measure(values[1]);
    unsigned first = measure(values[0]);
    printf("sum=%u parity=%u picked=%u values=%u,%u,%u slot=%zu\n",
           sum,
           folded,
           chosen,
           first,
           middle,
           last,
           slot);
    return sum != first + 10 || folded != (first ^ 4) || chosen != 2 || middle != 3 || last != 7 ||
           slot != 0;
}
