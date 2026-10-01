#include <quantum.h>
#include <stdio.h>

/* The eigenstate |1> has eigenphase 3/256. The compiler expands the
   fixed classical loop and synthesizes controlled phases and inverse QFT. */
int main(void) {
    quant uint8_t estimate = 0;
    quant bool eigenstate = 1;
    fourier(estimate);
    for (int i = 0; i < 8; ++i) {
        if (estimate & (1u << i)) {
            if (eigenstate) {
                phase(6.28318530717958647692 * (3.0 / 256.0) * (1u << i));
            }
        }
    }
    inverse_fourier(estimate);
    printf("%u\n", (unsigned)measure(estimate));
    measure(eigenstate);
    return 0;
}
