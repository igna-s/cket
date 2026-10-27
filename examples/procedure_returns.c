#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

static void visit(quant bool *value, unsigned limit, unsigned *visited) {
    for (unsigned i = 0; i < 4; ++i) {
        if (i == limit)
            return;
        value[0] ^= true;
        ++*visited;
    }
    value[0] ^= true;
}

static void mark(quant const bool *signal, unsigned limit) {
    quant bool temporary = !signal[0];
    if (temporary)
        phase(3.14159265358979323846);
    if (!limit)
        return;
    if (temporary)
        phase(3.14159265358979323846);
}

int main(int argc, char **argv) {
    unsigned limit = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 2;
    if (limit > 4)
        return 2;
    quant bool values[1] = {false};
    unsigned visited = 0;
    visit(values, limit, &visited);
    quant bool signal = superposition(0, 1);
    mark(&signal, limit);
    fourier(signal);
    unsigned result = measure(values[0]), interference = measure(signal);
    printf("limit=%u visited=%u result=%u interference=%u\n", limit, visited, result, interference);
    return visited != limit || result != ((limit + (limit == 4)) & 1) ||
           interference != (limit == 0);
}
