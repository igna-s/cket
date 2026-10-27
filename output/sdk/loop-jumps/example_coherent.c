#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

static void transform(quant bool target, unsigned skip) {
    int sign = 1;
    for (unsigned i = 0; i < 4; ++i) {
        if (i == skip)
            continue;
        if (i == 2)
            break;
        sign = -sign;
        target ^= true;
        phase(sign * 3.141592653589793);
    }
}

int main(int argc, char **argv) {
    unsigned skip = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 2;
    if (skip > 4)
        return 2;
    quant bool control = superposition(0, 1), target = false;
    if (control)
        transform(target, skip);
    if (skip < 3)
        if (control)
            target ^= true;
    fourier(control);
    unsigned c = measure(control), t = measure(target);
    printf("skip=%u control=%u target=%u\n", skip, c, t);
    return t || c != (skip < 3);
}
