#include <quantum.h>
#include <stdio.h>

static void mark(quant const bool a, quant const bool b) {
    if (a && b)
        phase(3.14159265358979323846);
}

static void mark_last(quant const bool *a, quant const bool *b) {
    mark(a[1], b[1]);
}

int main(void) {
    quant bool scalar = superposition(0, 1);
    quant bool values[2] = {superposition(0, 1), superposition(0, 1)};
    mark(scalar, scalar);
    mark_last(values, values);
    fourier(scalar);
    fourier(values[0]);
    fourier(values[1]);
    unsigned result = measure(scalar), first = measure(values[0]), last = measure(values[1]);
    printf("scalar=%u vector=%u,%u\n", result, first, last);
    return result != 1 || first != 0 || last != 1;
}
