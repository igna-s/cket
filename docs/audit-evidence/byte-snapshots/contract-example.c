#include <quantum.h>
#include <string.h>

int main(void) {
    unsigned char reference = 65;
    reference = 17;
    quant uint8_t input = superposition(0, 255);
    quant bool earlier = memcmp((const void *)&input, &reference, 1) <= 0;
    reference = 99;
    if (earlier)
        phase(0.17);
    measure(input);
    return reference != 99;
}
