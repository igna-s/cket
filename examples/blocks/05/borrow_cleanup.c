#include <quantum.h>
#include <stdio.h>

static void replace(quant bool *value, quant const bool source, quant const bool control) {
    quant bool saved = *value ^ 1;
    if (control)
        *value = source ^ 0;
    if (saved)
        phase(3.14159265358979323846);
}

int main(void) {
    quant bool control = superposition(0, 1);
    quant bool source = superposition(0, 1);
    quant bool value = superposition(0, 1);
    replace(&value, source, control);
    // The scoped predicate's phase must survive its actual uncomputation.
    // On the inactive branch it makes |->, whose Hadamard result is 1.
    if (!control)
        fourier(value);
    unsigned output = measure(value);
    unsigned selected = measure(control);
    unsigned input = measure(source);
    printf("control=%u source=%u value=%u expected=%u\n",
           selected,
           input,
           output,
           selected ? input : 1);
    return output != (selected ? input : 1);
}
