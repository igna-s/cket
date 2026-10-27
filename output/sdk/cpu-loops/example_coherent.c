#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

static void transform(quant bool target, unsigned count) {
    unsigned visited = 0;
    int sign = 1;
    while (count) {
        if (visited == 2)
            return;
        --count;
        ++visited;
        sign = -sign;
        target ^= true;
        phase(sign * 3.141592653589793);
    }
}

int main(int argc, char **argv) {
    unsigned count = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 1;
    if (count > 4)
        return 2;
    quant bool control = superposition(0, 1), target = false;
    if (control)
        transform(target, count);
    if (count == 1)
        if (control)
            target ^= true;
    fourier(control);
    unsigned c = measure(control), t = measure(target);
    printf("count=%u control=%u target=%u\n", count, c, t);
    return t || c != (count == 1);
}
