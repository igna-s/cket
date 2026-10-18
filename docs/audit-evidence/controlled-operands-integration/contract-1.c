#include <quantum.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int k = argc > 1 ? argv[1][0] - '0' : 5;
    quant bool enabled = superposition(0, 1);
    quant uint8_t x = 5;
    if (enabled)
        x = abs(-10 / k);
    if (enabled)
        phase(.48);
    unsigned value = measure(x);
    unsigned flag = measure(enabled);
    return value != (flag ? 2 : 5);
}
