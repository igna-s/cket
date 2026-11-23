#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool append = superposition(0, 1);
    quant char destination[3] = {'x', 0, 'Z'};
    quant char source[2] = {'y', 0};
    if (append) {
        strcat(destination, source);
        phase(0.17);
    }
    unsigned first = measure(destination[0]);
    unsigned second = measure(destination[1]);
    unsigned third = measure(destination[2]);
    unsigned s0 = measure(source[0]), s1 = measure(source[1]);
    unsigned flag = measure(append);
    char expected[3] = {'x', 0, 'Z'};
    strcat(expected, flag ? "y" : "");
    quant bool oracle = superposition(0, 1);
    quant char scratch[2] = {0, 0};
    if (oracle)
        strncat(scratch, "z", 1);
    if (scratch[0] == 'z')
        phase(3.14159265358979323846);
    if (oracle)
        scratch[0] ^= 'z';
    fourier(oracle);
    unsigned interference = measure(oracle);

    printf("append=%u destination=%u,%u,%u interference=%u\n",
           flag,
           first,
           second,
           third,
           interference);
    return first != (unsigned char)expected[0] || second != (unsigned char)expected[1] ||
           third != (unsigned char)expected[2] || s0 != 'y' || s1 || interference != 1;
}
