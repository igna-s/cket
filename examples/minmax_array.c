#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    const unsigned data[] = {42, 17, 42, 0, 128, 255, 9, 255};
    unsigned minimum = 256, maximum = 0;
    unsigned min_index = 0, max_index = 0;
    quant uint8_t sample = 0;
    for (unsigned i = 0; i < sizeof data / sizeof *data; ++i) {
        /* Explicit reset allows loading a new basis value into the same owner. */
        reset(sample);
        sample += data[i];
        unsigned value = measure(sample);
        if (value < minimum) {
            minimum = value;
            min_index = i;
        }
        if (i == 0 || value > maximum) {
            maximum = value;
            max_index = i;
        }
    }
    reset(sample);
    printf("min=%u index=%u max=%u index=%u\n", minimum, min_index, maximum, max_index);
    return minimum != 0 || min_index != 3 || maximum != 255 || max_index != 5;
}
