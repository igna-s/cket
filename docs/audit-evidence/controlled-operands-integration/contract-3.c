#include <quantum.h>

int main(void) {
    quant bool enabled = superposition(0, 1);
    quant uint8_t x = 250;
    for (int i = 0; i < 3; ++i)
        if (enabled)
            x += i + 1;
    if (enabled)
        phase(.48);
    for (int i = 2; i >= 0; --i)
        if (enabled)
            x -= i + 1;
    unsigned flag = measure(enabled);
    unsigned value = measure(x);
    return flag > 1 || value != 250;
}
