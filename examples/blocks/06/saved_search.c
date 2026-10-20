#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool repeated = superposition(0, 1);
    quant char text[3] = {'a', repeated ? 'a' : 'b', 0};
    const char *last = strrchr(text, 'a');
    const char *alias = last;
    const char *maybe_b = strchr(text, 'b');
    quant int8_t position = alias - text;
    quant char character = *alias;
    quant char next = alias[1];
    quant bool has_b = maybe_b != NULL;
    quant bool same = alias == last;
    if (has_b)
        phase(0.17);

    int observed_position = measure(position);
    char observed_character = (char)measure(character);
    char observed_next = (char)measure(next);
    unsigned observed_b = measure(has_b);
    unsigned observed_same = measure(same);
    char reference[3];
    reference[0] = (char)measure(text[0]);
    reference[1] = (char)measure(text[1]);
    reference[2] = (char)measure(text[2]);
    unsigned flag = measure(repeated);
    const char *expected = strrchr(reference, 'a');
    printf("text=%s position=%d character=%u next=%u has_b=%u\n",
           reference,
           observed_position,
           (unsigned char)observed_character,
           (unsigned char)observed_next,
           observed_b);
    return reference[0] != 'a' || reference[1] != (flag ? 'a' : 'b') || reference[2] ||
           observed_position != expected - reference || observed_character != *expected ||
           observed_next != expected[1] || observed_b != (strchr(reference, 'b') != NULL) ||
           !observed_same;
}
