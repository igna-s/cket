#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool control = superposition(0, 1);
    quant uint8_t value = 7;
    if (control) circuit("increment.qir.json", value);
    if (control) inverse_circuit("increment.qir.json", value);
    unsigned result = measure(value);
    measure(control);
    printf("%u\n", result);
    return result != 7;
}
