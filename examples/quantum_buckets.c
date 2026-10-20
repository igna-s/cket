#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    /* Compute the bucket coherently before either explicit measurement. */
    quant uint8_t value = superposition(0, 15);
    quant uint8_t bucket = value / 3;

    unsigned selected_bucket = measure(bucket);
    unsigned selected_value = measure(value);
    printf("value=%u bucket=%u\n", selected_value, selected_bucket);
    return selected_value > 15 || selected_bucket != selected_value / 3;
}
