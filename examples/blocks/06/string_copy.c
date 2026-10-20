#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool letter = superposition(0, 1);
    quant char source[2] = {letter ? 'a' : 0, 0};
    quant char copied[3] = {'X', 'Y', 'Z'};
    quant char padded[3] = {'X', 'Y', 'Z'};
    char *alias = strcpy(copied, source);
    strncpy(padded, source, sizeof padded);
    if (copied[0] == 'a')
        phase(0.17);

    unsigned c0 = measure(alias[0]), c1 = measure(alias[1]), c2 = measure(alias[2]);
    unsigned p0 = measure(padded[0]), p1 = measure(padded[1]), p2 = measure(padded[2]);
    unsigned s0 = measure(source[0]), s1 = measure(source[1]);
    unsigned flag = measure(letter);
    quant uint16_t word = 0xA5C3;
    char cpu[2] = {0, 'Q'};
    strcpy((char *)&word + 1, cpu);
    cpu[0] = 'x';
    unsigned observed_word = measure(word);

    char reference[2] = {(char)s0, 0};
    char expected_copy[3] = {'X', 'Y', 'Z'};
    char expected_pad[3] = {'X', 'Y', 'Z'};
    strcpy(expected_copy, reference);
    strncpy(expected_pad, reference, sizeof expected_pad);
    printf("source=%u copy=%u,%u,%u padded=%u,%u,%u word=%u\n",
           s0,
           c0,
           c1,
           c2,
           p0,
           p1,
           p2,
           observed_word);
    return s1 || s0 != (flag ? 'a' : 0) || c0 != (unsigned char)expected_copy[0] ||
           c1 != (unsigned char)expected_copy[1] || c2 != (unsigned char)expected_copy[2] ||
           p0 != (unsigned char)expected_pad[0] || p1 != (unsigned char)expected_pad[1] ||
           p2 != (unsigned char)expected_pad[2] || observed_word != 0x00C3;
}
