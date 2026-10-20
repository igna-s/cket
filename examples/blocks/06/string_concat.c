#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool leading = superposition(0, 1);
    quant char destination[3] = {leading ? 'a' : 0, 0, 'Z'};
    quant char source[2] = {'b', 0};
    char *alias = strcat(destination, source);
    if (destination[0] == 'a')
        phase(0.17);
    unsigned a0 = measure(alias[0]), a1 = measure(alias[1]), a2 = measure(alias[2]);
    unsigned s0 = measure(source[0]), s1 = measure(source[1]);
    unsigned flag = measure(leading);

    quant char bounded[3] = {'x', 0, 'Z'};
    quant char raw[2] = {'y', 'z'};
    strncat(bounded, raw, 1);
    unsigned b0 = measure(bounded[0]), b1 = measure(bounded[1]), b2 = measure(bounded[2]);
    unsigned r0 = measure(raw[0]), r1 = measure(raw[1]);
    char expected[3] = {flag ? 'a' : 0, 0, 'Z'};
    strcat(expected, "b");
    printf("leading=%u concat=%u,%u,%u bounded=%u,%u,%u\n", flag, a0, a1, a2, b0, b1, b2);
    return a0 != (unsigned char)expected[0] || a1 != (unsigned char)expected[1] ||
           a2 != (unsigned char)expected[2] || s0 != 'b' || s1 || b0 != 'x' || b1 != 'y' || b2 ||
           r0 != 'y' || r1 != 'z';
}
