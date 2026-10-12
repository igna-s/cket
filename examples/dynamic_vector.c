#include <quantum.h>
#include <stdio.h>

int main(void) {
    unsigned index;
    if (scanf("%u", &index) != 1 || index >= 2)
        return 1;
    quant uint8_t values[2] = {1, 7};
    values[index] += 2;
    printf("%u\n", (unsigned)measure(values[index]));
    // Selection consumes only the chosen measurement. Every owner still
    // needs a defined end of life, regardless of the CPU index supplied.
    reset(values[0]);
    reset(values[1]);
    return 0;
}
