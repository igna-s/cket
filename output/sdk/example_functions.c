#include <quantum.h>
#include <stdio.h>

quant uint8_t sum_buffer(quant const uint8_t *values, size_t count) {
    quant uint8_t total = 0;
    for (size_t i = 0; i < count; ++i)
        total += values[i];
    return total;
}

void subtract_buffer(quant const uint8_t *values, size_t count, quant uint8_t total) {
    size_t i = 0;
    while (i < count) {
        total -= values[i];
        ++i;
    }
}

int main(void) {
    size_t count = 2;
    quant uint16_t words[2] = {0x123, 0x456};
    size_t slot = 1;
    quant uint16_t word = words[slot] ^ 0x100;
    unsigned selected = measure(word);
    unsigned second_word = measure(words[1]);
    unsigned first_word = measure(words[0]);
    quant uint8_t values[2] = {superposition(0, 3), 5};
    quant uint8_t result = sum_buffer(values, count);
    subtract_buffer(values, count, result);
    unsigned zero = measure(result);
    unsigned second = measure(values[1]);
    unsigned first = measure(values[0]);
    printf("restored=%u values=%u,%u word=%u words=%u,%u\n",
           zero,
           first,
           second,
           selected,
           first_word,
           second_word);
    return zero != 0 || second != 5 || first > 3 || selected != 0x556 || first_word != 0x123 ||
           second_word != 0x456;
}
