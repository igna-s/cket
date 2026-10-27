#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

static void transform(quant bool target, unsigned count) {
    unsigned limit = count + 1;
    bool done = count == 2;
    if (done)
        return;
    if (limit & 1)
        target ^= true;
    if (!target)
        phase(3.141592653589793);
}

int main(int argc, char **argv) {
    unsigned count = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 3;
    quant bool control = superposition(0, 1), target = false;
    if (control)
        transform(target, count);
    if (count != 2 && ((count + 1) & 1)) {
        if (control)
            target ^= true;
    }
    fourier(control);
    unsigned c = measure(control), t = measure(target);
    printf("count=%u control=%u target=%u\n", count, c, t);
    return t || c != (count != 2 && !((count + 1) & 1));
}
