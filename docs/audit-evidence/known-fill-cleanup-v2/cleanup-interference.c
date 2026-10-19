#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool bit = superposition(0, 1);
    int byte = 7;
    {
        quant uint8_t temporary = 5;
        if (bit)
            memset((void *)&temporary, byte, 1);
        byte = 99;
        if (temporary == 7)
            phase(3.14159265358979323846);
    }
    inverse_fourier(bit);
    return measure(bit) != 1;
}
