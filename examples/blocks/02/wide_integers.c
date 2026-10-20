#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool control = superposition(0, 1);
    quant int value = control ? -7 : 10;
    quant int quotient = value / 3;

    int observed_quotient = measure(quotient);
    int observed_value = measure(value);
    unsigned observed_control = measure(control);
    printf(
        "control=%u value=%d quotient=%d\n", observed_control, observed_value, observed_quotient);
    return observed_control > 1 || observed_value != (observed_control ? -7 : 10) ||
           observed_quotient != observed_value / 3;
}
