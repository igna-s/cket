#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint16_t input = superposition(0, 65535);
    const uint16_t reference = 0xab20;
    quant bool earlier = memcmp((const void *)&input, &reference, sizeof input) < 0;
    if (earlier)
        phase(0.23);
    measure(input);
    return 0;
}
