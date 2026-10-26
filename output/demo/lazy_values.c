#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool condition = superposition(0, 1), value = superposition(0, 1);
    if (condition)
        phase(.19);
    if (value)
        phase(.31);
    quant bool previous = condition ? value++ : value--;
    if (previous)
        phase(.37);
    unsigned old = measure(previous), next = measure(value), flag = measure(condition);
    unsigned expected = flag ? 1u : !old;
    printf("previous=%u value=%u condition=%u expected=%u\n", old, next, flag, expected);
    return next != expected;
}
