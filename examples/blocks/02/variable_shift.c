#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant bool control = superposition(0, 1);
    quant uint32_t value = control ? UINT32_C(0x80000001) : UINT32_C(0xfedcba98);
    quant uint8_t count = superposition(0, 31);
    quant uint32_t shifted = value >> count;

    uint32_t observed_shifted = measure(shifted);
    uint32_t observed_value = measure(value);
    unsigned observed_count = measure(count);
    unsigned observed_control = measure(control);
    printf("control=%u value=%u count=%u shifted=%u\n",
           observed_control,
           (unsigned)observed_value,
           observed_count,
           (unsigned)observed_shifted);
    return observed_control > 1 || observed_count > 31 ||
           observed_value != (observed_control ? UINT32_C(0x80000001) : UINT32_C(0xfedcba98)) ||
           observed_shifted != (observed_value >> observed_count);
}
