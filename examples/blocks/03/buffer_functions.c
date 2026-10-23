#include <quantum.h>
#include <stdio.h>

quant uint8_t selected_byte(quant const uint8_t *values, quant const bool index) {
    return values[index] ^ 3;
}

quant uint8_t through_buffer(quant const uint8_t *values, quant const bool index) {
    return selected_byte(values, index);
}

void update_buffer(quant uint8_t *values, quant const bool index) {
    values[index] = 11;
    values[index]++;
}

int main(void) {
    quant bool index = superposition(0, 1);
    quant uint8_t values[3] = {2, 5, 9};
    quant uint8_t *tail = values + 1;
    quant uint8_t answer = through_buffer(tail, index);
    unsigned result = measure(answer);
    update_buffer(tail, index);
    unsigned last = measure(values[2]);
    unsigned middle = measure(values[1]);
    unsigned first = measure(values[0]);
    unsigned selected = measure(index);
    printf("index=%u result=%u values=%u,%u,%u\n", selected, result, first, middle, last);
    return result != (selected ? 10 : 6) || first != 2 || middle != (selected ? 5 : 12) ||
           last != (selected ? 12 : 9);
}
