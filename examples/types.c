#include <quantum.h>

int main(void) {
    int classical = 10;
    quant int quantum_integer = 10;
    quant bool quantum_boolean = superposition(0, 1);
    int measured = (int)measure(quantum_integer);
    bool bit = (bool)measure(quantum_boolean);
    return classical != measured || (bit != false && bit != true);
}
