#include <quantum.h>
#include <stdio.h>

quant uint16_t selected_word(quant const uint16_t *values, quant const bool index) {
    return values[index] ^ 0x100;
}

void replace_word(quant uint16_t *values, quant const bool index) {
    values[index] = 0x1234;
}

int main(void) {
    quant bool index = superposition(0, 1);
    quant uint16_t values[2] = {0x201, 0x405};
    quant uint16_t answer = selected_word(values, index);
    unsigned before = measure(answer);
    replace_word(values, index);
    unsigned second = measure(values[1]);
    unsigned first = measure(values[0]);
    unsigned selected = measure(index);
    printf("index=%u before=%u values=%u,%u\n", selected, before, first, second);
    return before != (selected ? 0x505 : 0x301) || first != (selected ? 0x201 : 0x1234) ||
           second != (selected ? 0x1234 : 0x405);
}
