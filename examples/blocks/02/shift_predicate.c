#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant bool control = superposition(0, 1);
    quant uint64_t value = control ? (UINT64_C(1) << 63) : UINT64_C(3);
    quant uint8_t count = superposition(0, 63);
    quant bool nonzero = (value << count) != 0;
    quant bool byte_nonzero = (uint8_t)(value << count);

    unsigned observed_nonzero = measure(nonzero);
    unsigned observed_byte = measure(byte_nonzero);
    uint64_t observed_value = measure(value);
    unsigned observed_count = measure(count);
    unsigned observed_control = measure(control);
    uint64_t expected = observed_count < 64 ? observed_value << observed_count : 0;
    printf("control=%u count=%u nonzero=%u byte_nonzero=%u\n",
           observed_control,
           observed_count,
           observed_nonzero,
           observed_byte);
    return observed_control > 1 || observed_count > 63 ||
           observed_value != (observed_control ? (UINT64_C(1) << 63) : UINT64_C(3)) ||
           observed_nonzero != (expected != 0) || observed_byte != ((uint8_t)expected != 0);
}
