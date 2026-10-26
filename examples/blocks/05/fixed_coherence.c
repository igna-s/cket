#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void replace_low_byte(quant uint16_t *p,
                             quant const uint8_t *source,
                             quant const bool control,
                             size_t length) {
    quant uint16_t saved = p[0] ^ 1;
    if (control)
        memcpy((void *)p, (const void *)source, length);
    if (saved & 0x100)
        phase(.17);
}

int main(int argc, char **argv) {
    size_t length = argc > 1 ? strtoul(argv[1], NULL, 10) : 1;
    if (length > 1)
        return 2;
    quant bool high = superposition(0, 1), low = superposition(0, 1);
    quant uint8_t source = superposition(0, 1);
    quant bool control = superposition(0, 1);
    quant uint16_t buffer[1] = {(high ? 256 : 0) | low};
    replace_low_byte(buffer, &source, control, length);
    unsigned value = measure(buffer[0]);
    unsigned h = measure(high), l = measure(low), x = measure(source), c = measure(control);
    unsigned expected = (h << 8) | (c && length ? x : l);
    printf("length=%zu control=%u high=%u value=%u expected=%u\n", length, c, h, value, expected);
    return value != expected;
}
