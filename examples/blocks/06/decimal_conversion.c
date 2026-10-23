#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    quant bool choice = superposition(0, 1);
    quant char text[3] = {choice ? '4' : '2', '7', 0};
    quant int number = atoi((const char *)text);
    int observed = (int)measure(number);
    quant int8_t long_number = atol((const char *)text);
    int observed_long = (int)measure(long_number);
    quant int8_t long_long_number = atoll((const char *)text);
    int observed_long_long = (int)measure(long_long_number);
    char reference[3];
    reference[0] = (char)measure(text[0]);
    reference[1] = (char)measure(text[1]);
    reference[2] = (char)measure(text[2]);
    unsigned flag = measure(choice);
    printf("choice=%u text=%s atoi=%d atol=%d atoll=%d\n",
           flag,
           reference,
           observed,
           observed_long,
           observed_long_long);
    return reference[0] != (flag ? '4' : '2') || reference[1] != '7' || reference[2] ||
           observed != atoi(reference) || observed_long != atol(reference) ||
           observed_long_long != atoll(reference);
}
