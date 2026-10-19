#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool bit = superposition(0, 1);
    quant uint8_t byte = 5;
    memset((void *)&byte, bit, 1);
    unsigned first = measure(bit);
    unsigned second = measure(byte);
    return first != second;
}
