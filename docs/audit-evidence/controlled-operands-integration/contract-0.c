#include <quantum.h>

int main(int argc, char **argv) {
    int k = argc > 1 ? argv[1][0] - '0' : 0;
    quant bool enabled = false;
    quant uint8_t x = 5;
    if (enabled)
        x ^= 10 / k;
    unsigned value = measure(x);
    unsigned flag = measure(enabled);
    return value != 5 || flag != 0;
}
