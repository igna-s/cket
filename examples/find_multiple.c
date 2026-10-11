#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    /* Exactly four solutions in this domain: 1, 5, 9, 13. */
    quant uint8_t answer = find_known(x, x % 4 == 1, 4);
    unsigned result = measure(answer);
    printf("candidate=%u\n", result);
    return result > 15 || result % 4 != 1;
}
