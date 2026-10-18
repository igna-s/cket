#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint8_t value = 5;
    uint8_t *pointer = memset(&value, 17, 1);
    unsigned observed = measure(*pointer);
    return observed != 17 || pointer != &value;
}
