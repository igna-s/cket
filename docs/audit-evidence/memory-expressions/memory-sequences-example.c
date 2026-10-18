#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool bit = superposition(0, 1);
    quant uint8_t value = 5;
    if (bit)
        phase(.31);

    quant uint8_t result = (memset((void *)&value, bit, 1), value + 1);
    if (result == 2)
        phase(.17);

    unsigned observed_result = measure(result);
    unsigned observed_value = measure(value);
    unsigned observed_bit = measure(bit);
    return observed_value != observed_bit || observed_result != observed_bit + 1;
}
