#include <quantum.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t f32_bits(float v) {
    uint32_t bits = 0;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

static uint64_t f64_bits(double v) {
    uint64_t bits = 0;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

int main(void) {
    quant bool coin = superposition(0, 1);
    quant float fa = coin ? 1.5f : -2.25f;
    quant float fb = 0.5f;
    quant float fsum = fa + fb;
    quant double dx = coin ? 3.25 : 1.75;
    quant double dprod = dx * 2.0 + (double)fa;
    quant bool neg = fa < 0.0f;

    quant bool kick = superposition(0, 1);
    if ((kick ? 1.5f : -1.5f) > 0.0f)
        phase(3.14159265358979323846);
    fourier(kick);
    unsigned interference = measure(kick);

    float got_f = measure(fsum);
    double got_d = measure(dprod);
    unsigned got_neg = measure(neg);
    unsigned got_coin = measure(coin);
    reset(fa);
    reset(fb);
    reset(dx);

    float want_f = (got_coin ? 1.5f : -2.25f) + 0.5f;
    double want_d = (got_coin ? 3.25 : 1.75) * 2.0 + (double)(got_coin ? 1.5f : -2.25f);
    printf("coin=%u neg=%u fsum=%.2f dprod=%.2f interference=%u\n",
           got_coin,
           got_neg,
           (double)got_f,
           got_d,
           interference);
    return got_coin > 1 || got_neg != (got_coin == 0) || f32_bits(got_f) != f32_bits(want_f) ||
           f64_bits(got_d) != f64_bits(want_d) || interference != 1;
}
