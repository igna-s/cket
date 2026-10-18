#include <quantum.h>
#include <string.h>
#include <stddef.h>
int main(void) {
    quant uint16_t a[2] = {1, 2};
    uint16_t *p = memmove(a + 1, a, 2);
    ptrdiff_t distance = p - a;
    unsigned first = measure(a[0]), second = measure(a[1]);
    return distance != 1 || first != 1 || second != 1;
}
