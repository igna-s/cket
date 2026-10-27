#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

static void transform(quant bool target, unsigned count) {
    if (!count)
        return;
    target ^= true;
    if (count == 2)
        return;
    target ^= true;
    phase(3.14159265358979323846);
}

int main(int argc, char **argv) {
    unsigned count = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 1;
    if (count > 3)
        return 2;
    quant bool control = superposition(0, 1), target = false;
    if (control)
        transform(target, count);
    // Undo the remaining controlled X before observing interference.
    if (count == 2)
        if (control)
            target ^= true;
    fourier(control);
    unsigned observed = measure(control), value = measure(target);
    printf("count=%u control=%u target=%u\n", count, observed, value);
    return value != 0 || observed != (count != 0 && count != 2);
}
