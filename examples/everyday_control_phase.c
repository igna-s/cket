#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

static quant uint8_t classify_step(quant const uint8_t mode,
                                   quant const uint8_t x,
                                   unsigned limit) {
    quant uint8_t acc = 0;
    for (unsigned i = 0; i < limit; ++i) {
        switch (mode) {
        case 0:
            acc += (uint8_t)(x + 1u);
            break;
        case 1:
            if (i == 0)
                continue;
            acc += (uint8_t)(x + 2u);
            break;
        default:
            break;
        }
    }
    return acc;
}

int main(void) {
    int status = 1;
    quant uint8_t mode = superposition(0, 1);
    quant uint8_t x = 3;
    quant uint8_t guard = 5;
    quant uint8_t res = classify_step(mode, x, 3u);

    quant uint8_t count = 0;
    unsigned iter = 0;
    while (iter < 4u && count < 2u) {
        if (mode)
            count += 1u;
        ++iter;
    }

    quant bool phase_q = superposition(0, 1);
    if (phase_q)
        phase(3.14159265358979323846);
    fourier(phase_q);
    unsigned interference = measure(phase_q);

    unsigned got_mode = measure(mode);
    unsigned got_res = measure(res);
    unsigned got_count = measure(count);
    unsigned got_x = measure(x);
    if (got_mode > 1)
        goto cleanup;

    unsigned want_res = got_mode ? (2u * (3u + 2u)) : (3u * (3u + 1u));
    unsigned want_count = got_mode ? 2u : 0u;
    printf("mode=%u res=%u count=%u x=%u interference=%u\n",
           got_mode,
           got_res,
           got_count,
           got_x,
           interference);
    status = got_res != want_res || got_count != want_count || got_x != 3 || interference != 1;

cleanup:
    if (measure(guard) != 5)
        status = 1;
    return status;
}
