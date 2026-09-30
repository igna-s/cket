#include <quantum.h>
#include <stdio.h>

struct Pair {
    uint8_t first;
    uint8_t second;
};

int main(void) {
    quant uint8_t values[2] = {7, 2};
    values[0] += values[1];
    printf("%u %u\n", (unsigned)measure(values[0]), (unsigned)measure(values[1]));
    reset(values[0]);
    reset(values[1]);

    quant struct Pair pair = {3, 4};
    pair.second += pair.first;
    printf("%u %u\n", (unsigned)measure(pair.first), (unsigned)measure(pair.second));
    return 0;
}
