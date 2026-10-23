#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool index = superposition(0, 1);
    quant uint8_t values[2] = {3, 9};
    quant uint8_t before = values[index];
    unsigned initial = measure(before);
    quant uint8_t assignment = (values[index] = 7);
    unsigned assigned = measure(assignment);
    quant uint8_t prior = values[index]++;
    unsigned old = measure(prior);
    quant uint8_t after = values[index];
    unsigned updated = measure(after);
    unsigned second = measure(values[1]);
    unsigned first = measure(values[0]);
    unsigned flag = measure(index);
    printf("index=%u before=%u assigned=%u old=%u after=%u values=%u,%u\n",
           flag,
           initial,
           assigned,
           old,
           updated,
           first,
           second);
    return initial != (flag ? 9 : 3) || assigned != 7 || old != 7 || updated != 8 ||
           first != (flag ? 3 : 8) || second != (flag ? 8 : 9);
}
