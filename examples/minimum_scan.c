#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    unsigned best_value = 256;
    unsigned best_x = 0;
    /* This fixed loop expands: each quantum temporary has a bounded lifetime.
       Quantum arithmetic evaluates f(x); the measured reduction is classical.
       Scan the whole domain, keeping the first index when values tie. */
    for (unsigned i = 0; i < 16; ++i) {
        quant uint8_t x = i;
        quant uint8_t score = x < 8 ? x : 15 - x;
        unsigned value = measure(score);
        unsigned candidate = measure(x);
        reset(score);
        reset(x);
        if (value < best_value) {
            best_value = value;
            best_x = candidate;
        }
    }
    printf("minimum f(x)=min(x,15-x): value=%u x=%u\n", best_value, best_x);
    return best_value != 0 || best_x != 0;
}
