#include <quantum.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static quant uint16_t sum_with_bias(quant const uint16_t *vec, size_t len, uint16_t bias) {
    quant uint16_t acc = bias;
    for (size_t i = 0; i < len; ++i)
        acc += vec[i];
    return acc;
}

static void add_prefix(quant uint16_t *vec, size_t len, quant const uint16_t delta) {
    for (size_t i = 0; i < len; ++i)
        vec[i] += delta;
}

static quant uint16_t combine_weighted(quant const uint16_t *vec,
                                       size_t len,
                                       quant const uint16_t base) {
    quant uint16_t total = 0;
    total += base;
    for (size_t i = 0; i < len; ++i)
        total += (uint16_t)(vec[i] * (i + 1u));
    return total;
}

static void
cpu_reference(unsigned alt, uint16_t *out_items, uint16_t *out_s1, uint16_t *out_total) {
    uint16_t vec[3] = {alt ? 4u : 1u, 2u, 3u};
    uint16_t acc = 1u;
    for (size_t i = 0; i < 3; ++i)
        acc = (uint16_t)(acc + vec[i]);
    for (size_t i = 0; i < 2; ++i)
        vec[i] = (uint16_t)(vec[i] + acc);
    uint16_t total = acc;
    for (size_t i = 0; i < 3; ++i)
        total = (uint16_t)(total + (uint16_t)(vec[i] * (i + 1u)));
    for (size_t i = 0; i < 3; ++i)
        out_items[i] = vec[i];
    *out_s1 = acc;
    *out_total = total;
}

int main(void) {
    quant bool alt = superposition(0, 1);
    quant uint16_t items[3] = {alt ? 4u : 1u, 2u, 3u};
    quant uint16_t s1 = sum_with_bias(items, 3u, 1u);
    add_prefix(items, 2u, s1);
    quant uint16_t total = combine_weighted(items, 3u, s1);

    quant bool phase_probe = superposition(0, 1);
    if ((phase_probe ? 3u : 0u) > 0u)
        phase(3.14159265358979323846);
    fourier(phase_probe);
    unsigned interference = measure(phase_probe);

    unsigned got_alt = measure(alt);
    uint16_t got_s1 = measure(s1);
    uint16_t got_total = measure(total);
    uint16_t got_items[3];
    for (size_t i = 0; i < 3; ++i)
        got_items[i] = measure(items[i]);

    uint16_t want_items[3], want_s1 = 0, want_total = 0;
    cpu_reference(got_alt, want_items, &want_s1, &want_total);
    printf("alt=%u s1=%u total=%u items=%u,%u,%u interference=%u\n",
           got_alt,
           (unsigned)got_s1,
           (unsigned)got_total,
           (unsigned)got_items[0],
           (unsigned)got_items[1],
           (unsigned)got_items[2],
           interference);
    return got_alt > 1 || got_s1 != want_s1 || got_total != want_total ||
           got_items[0] != want_items[0] || got_items[1] != want_items[1] ||
           got_items[2] != want_items[2] || interference != 1;
}
