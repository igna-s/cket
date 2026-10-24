#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    size_t count = 2;
    quant uint8_t *values = calloc(count, sizeof *values);
    if (!values)
        return 2;
    values[0] = 3;
    values[1] = 9;
    quant bool index = superposition(0, 1);
    if (index)
        phase(.31);
    quant uint8_t previous = values[index]++;
    if (previous == 9)
        phase(.17);
    unsigned old = measure(previous), selected = measure(index);
    unsigned first = measure(values[0]), second = measure(values[1]);
    free((void *)values);
    printf("index=%u previous=%u values={%u,%u}\n", selected, old, first, second);
    return old != (selected ? 9u : 3u) || first != (selected ? 3u : 4u) ||
           second != (selected ? 10u : 9u);
}
