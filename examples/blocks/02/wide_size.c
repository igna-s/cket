#include <quantum.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant bool control = superposition(0, 1);
    quant size_t count = control ? SIZE_MAX : (SIZE_MAX / 2 + 1);
    quant uint8_t tail = count / 3;

    unsigned observed_tail = measure(tail);
    size_t observed_count = measure(count);
    unsigned observed_control = measure(control);
    printf("control=%u count=%zu tail=%u\n", observed_control, observed_count, observed_tail);
    size_t expected = observed_control ? SIZE_MAX : (SIZE_MAX / 2 + 1);
    return observed_control > 1 || observed_count != expected ||
           observed_tail != (uint8_t)(expected / 3);
}
