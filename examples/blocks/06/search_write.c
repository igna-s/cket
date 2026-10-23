#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool repeated = superposition(0, 1);
    quant char text[3] = {'a', repeated ? 'a' : 'b', 0};
    char *last = strrchr(text, 'a');
    quant char old = (*last)++;
    char observed_old = (char)measure(old);
    quant char assigned = (*last = 'x');
    char observed_assigned = (char)measure(assigned);
    char second = (char)measure(text[1]);
    char first = (char)measure(text[0]);
    unsigned flag = measure(repeated);
    printf("repeated=%u text=%c%c old=%c assigned=%c\n",
           flag,
           first,
           second,
           observed_old,
           observed_assigned);
    return observed_old != 'a' || observed_assigned != 'x' || first != (flag ? 'a' : 'x') ||
           second != (flag ? 'x' : 'b');
}
