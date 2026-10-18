#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool value = superposition(0, 1);
    quant uint8_t bytes[2] = {17, 34};

    if (value)
        phase(.31);
    memset(bytes, value--, sizeof bytes);
    if (bytes[0] == 1)
        phase(.17);

    unsigned first = measure(bytes[0]);
    unsigned second = measure(bytes[1]);
    unsigned updated = measure(value);
    return first != second || first > 1 || updated != !first;
}
