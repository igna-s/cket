#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint16_t input = superposition(0, 65535);
    const uint16_t reference = 0xab00;
    quant bool earlier = strncmp((const char *)&input, (const char *)&reference, 2) <= 0;
    if (earlier)
        phase(0.17);
    measure(input);
    return 0;
}
