#include <quantum.h>
#include <stdint.h>
#include <stdio.h>
#include "read_input.h"

int main(void) {
    unsigned count = read_unsigned(8), data[8];
    if (count == 0 || count > 8) {
        fputs("Expected a count in 1..8 followed by values in 0..255.\n", stderr);
        return 1;
    }
    for (unsigned i = 0; i < count; ++i) {
        data[i] = read_unsigned(255);
        if (data[i] > 255) {
            fputs("Expected a count in 1..8 followed by values in 0..255.\n", stderr);
            return 1;
        }
    }
    unsigned minimum = 256, maximum = 0;
    unsigned min_index = 0, max_index = 0;
    quant uint8_t sample = 0;
    /* Basis preparation and measurement are quantum; this reduction is CPU C. */
    for (unsigned i = 0; i < count; ++i) {
        reset(sample);
        sample += data[i];
        unsigned value = measure(sample);
        if (value < minimum) { minimum = value; min_index = i; }
        if (i == 0 || value > maximum) { maximum = value; max_index = i; }
    }
    reset(sample);
    printf("min=%u index=%u max=%u index=%u\n", minimum, min_index, maximum, max_index);
    return 0;
}
