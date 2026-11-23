/* Test-only basis-action interpreter for arithmetic tails containing X/phase.
 * Source preparation is excluded; wide cases are finite label checks, not
 * dense wide-state simulations. Public and private wires are checked jointly. */
static qc_mask expression_basis(const qc_register *r, uint64_t value) {
    qc_mask result = qc_mask_zero();
    for (unsigned bit = 0; bit < r->width; ++bit)
        if ((value >> bit) & 1)
            result = qc_mask_or(result, QC_WIRE(r->wires[bit]));
    return result;
}

static void
expression_walk(const qc_context *c, size_t begin, size_t end, qc_mask *basis, double *angle) {
    for (size_t i = begin; i < end; ++i) {
        const qc_gate *g = &c->gates[i];
        if (g->kind != QC_X && g->kind != QC_PHASE)
            validation_fail("basis-action arithmetic introduced a nonpermutation gate");
        if (!qc_mask_eq(qc_mask_and(*basis, g->mask), g->pattern))
            continue;
        if (g->kind == QC_X)
            *basis = qc_mask_xor(*basis, QC_WIRE(g->target));
        else
            *angle += g->angle;
    }
}

static int64_t expression_signed(uint64_t raw, unsigned width) {
    uint64_t sign = UINT64_C(1) << (width - 1);
    if (!(raw & sign))
        return (int64_t)raw;
    /* Form the negative value without an out-of-range unsigned-to-signed cast. */
    return -1 - (int64_t)((~raw) & (width == 64 ? UINT64_MAX : (sign << 1) - 1));
}
