/* A sparse statevector stores occupied computational-basis labels, not sampled
 * classical trajectories. X permutes labels; phase changes complex amplitudes;
 * H combines both paths, including destructive interference. Only exact zero
 * sums are removed. The support budget is checked before replacing the state. */
static int sparse_compare(const void *left, const void *right) {
    const qc_sparse_term *a = left, *b = right;
    return qc_mask_cmp(a->basis, b->basis);
}

static int sparse_h(qc_context *c, const qc_gate *g) {
    size_t count = 0;
    qc_sparse_term *next = malloc(2 * c->sparse_count * sizeof *next);
    if (!next)
        return qc_fail(c, "out of memory applying sparse Hadamard gate");
    for (size_t row = 0; row < c->sparse_count; ++row) {
        qc_sparse_term term = c->sparse[row];
        if (!qc_mask_eq(qc_mask_and(term.basis, g->mask), g->pattern)) {
            next[count++] = term;
            continue;
        }
        double complex half = term.amplitude * 0.70710678118654752440;
        next[count++] = (qc_sparse_term){qc_mask_clear(term.basis, g->target), half};
        next[count++] = (qc_sparse_term){qc_mask_set(term.basis, g->target),
                                         qc_mask_test(term.basis, g->target) ? -half : half};
    }
    qsort(next, count, sizeof *next, sparse_compare);
    size_t occupied = 0;
    for (size_t row = 0; row < count;) {
        qc_mask basis = next[row].basis;
        double complex amplitude = 0;
        do {
            amplitude += next[row++].amplitude;
        } while (row < count && qc_mask_eq(next[row].basis, basis));
        if (amplitude != 0)
            next[occupied++] = (qc_sparse_term){basis, amplitude};
    }
    if (occupied > c->sparse_limit) {
        free(next);
        return qc_fail(
            c, "sparse statevector support exceeds %zu occupied basis states", c->sparse_limit);
    }
    free(c->sparse);
    c->sparse = next;
    c->sparse_count = occupied;
    return 1;
}

static int sparse_gate(qc_context *c, const qc_gate *g) {
    if (g->kind == QC_H)
        return sparse_h(c, g);
    if (g->kind == QC_X || g->kind == QC_PHASE) {
        qc_mask bit = QC_WIRE(g->target);
        double complex factor = g->kind == QC_PHASE ? cos(g->angle) + I * sin(g->angle) : 1;
        for (size_t row = 0; row < c->sparse_count; ++row)
            if (qc_mask_eq(qc_mask_and(c->sparse[row].basis, g->mask), g->pattern)) {
                if (g->kind == QC_X)
                    c->sparse[row].basis = qc_mask_xor(c->sparse[row].basis, bit);
                else
                    c->sparse[row].amplitude *= factor;
            }
    }
    return 1;
}

static int sparse_measure(qc_context *c, unsigned wire) {
    double probability[2] = {0, 0};
    for (size_t row = 0; row < c->sparse_count; ++row) {
        double complex amplitude = c->sparse[row].amplitude;
        double re = creal(amplitude), im = cimag(amplitude);
        probability[qc_mask_test(c->sparse[row].basis, wire)] += re * re + im * im;
    }
    int result = probability[0] == 0 ? 1
                 : probability[1] == 0
                     ? 0
                     : random_unit(c) < probability[1] / (probability[0] + probability[1]);
    double scale = 1 / sqrt(probability[result]);
    size_t count = 0;
    for (size_t row = 0; row < c->sparse_count; ++row)
        if (qc_mask_test(c->sparse[row].basis, wire) == result) {
            c->sparse[count] = c->sparse[row];
            c->sparse[count++].amplitude *= scale;
        }
    c->sparse_count = count;
    return result;
}

static int sparse_clean(qc_context *c, const qc_register *r) {
    qc_mask mask = qc_mask_zero();
    for (unsigned bit = 0; bit < r->width; ++bit)
        mask = qc_mask_set(mask, r->wires[bit]);
    double garbage = 0;
    for (size_t row = 0; row < c->sparse_count; ++row)
        if (qc_mask_any(qc_mask_and(c->sparse[row].basis, mask))) {
            double complex amplitude = c->sparse[row].amplitude;
            double re = creal(amplitude), im = cimag(amplitude);
            garbage += re * re + im * im;
        }
    return garbage < 1e-10 || qc_fail(c, "release would leave nonzero garbage amplitudes");
}
