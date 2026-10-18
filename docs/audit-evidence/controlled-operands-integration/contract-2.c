#include <quantum.h>

int main(void) {
    quant bool enabled = superposition(0, 1);
    quant bool x = false;
    if (enabled)
        x ^= true;
    if (enabled)
        phase(.48);
    if (enabled)
        x ^= true;
    unsigned flag = measure(enabled);
    unsigned value = measure(x);
    return flag > 1 || value != 0;
}
