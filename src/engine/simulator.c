#include "internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double random_unit(qc_context *c);
#include "sparse_simulator.inc"

int qc_sim_grow(qc_context *c, unsigned width) {
    if (!c->simulate || c->sparse_limit)
        return 1;
    if (width > QC_MAX_SIMULATED)
        return qc_fail(c, "native statevector execution is limited to %d qubits", QC_MAX_SIMULATED);
    size_t n = (size_t)1 << width;
    double complex *s = realloc(c->state, n * sizeof *s);
    if (!s)
        return qc_fail(c, "out of memory allocating the quantum statevector");
    memset(s + c->dimension, 0, (n - c->dimension) * sizeof *s);
    if (!c->dimension)
        s[0] = 1;
    c->state = s;
    c->dimension = n;
    return 1;
}

int qc_sim_gate(qc_context *c, const qc_gate *g) {
    if (c->error[0])
        return 0;
    if (!c->simulate)
        return 1;
    if (c->sparse_limit)
        return sparse_gate(c, g);
    uint64_t bit = UINT64_C(1) << g->target;
    uint64_t mask = g->mask.w[0], pattern = g->pattern.w[0];
    if (g->kind == QC_PHASE) {
        double complex factor = cos(g->angle) + I * sin(g->angle);
        for (size_t i = 0; i < c->dimension; ++i)
            if ((i & mask) == pattern)
                c->state[i] *= factor;
    } else if (g->kind == QC_H || g->kind == QC_X) {
        for (size_t base = 0; base < c->dimension; base += (size_t)bit * 2)
            for (size_t off = 0; off < bit; ++off) {
                size_t a = base + off, b = a + (size_t)bit;
                if ((a & mask) != pattern)
                    continue;
                double complex left = c->state[a], right = c->state[b];
                if (g->kind == QC_X) {
                    c->state[a] = right;
                    c->state[b] = left;
                } else {
                    c->state[a] = (left + right) * 0.70710678118654752440;
                    c->state[b] = (left - right) * 0.70710678118654752440;
                }
            }
    }
    return 1;
}

static double random_unit(qc_context *c) {
    uint64_t z = (c->rng += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    z ^= z >> 31;
    return (double)(z >> 11) * 0x1.0p-53;
}

int qc_sim_measure(qc_context *c, unsigned wire) {
    if (!c->simulate)
        return 0;
    if (c->sparse_limit)
        return sparse_measure(c, wire);
    uint64_t mask = UINT64_C(1) << wire;
    double p1 = 0, p0 = 0;
    for (size_t i = 0; i < c->dimension; ++i) {
        double a = creal(c->state[i]), b = cimag(c->state[i]), p = a * a + b * b;
        if (i & mask)
            p1 += p;
        else
            p0 += p;
    }
    int result = p0 == 0 ? 1 : p1 == 0 ? 0 : random_unit(c) < p1 / (p0 + p1);
    double scale = 1 / sqrt(result ? p1 : p0);
    for (size_t i = 0; i < c->dimension; ++i) {
        if (!!(i & mask) == result)
            c->state[i] *= scale;
        else
            c->state[i] = 0;
    }
    return result;
}

int qc_sim_clean(qc_context *c, const qc_register *r) {
    if (!c->simulate)
        return 1;
    if (c->sparse_limit)
        return sparse_clean(c, r);
    uint64_t mask = 0;
    for (unsigned b = 0; b < r->width; ++b)
        mask |= UINT64_C(1) << r->wires[b];
    double garbage = 0;
    for (size_t i = 0; i < c->dimension; ++i)
        if (i & mask) {
            double a = creal(c->state[i]), b = cimag(c->state[i]);
            garbage += a * a + b * b;
        }
    return garbage < 1e-10 || qc_fail(c, "release would leave nonzero garbage amplitudes");
}
