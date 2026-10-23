/* Add an independent typed value, then invert precisely its private plan.
 * The destination update is deliberately outside that recorded compute plan. */
static int add_expression(qc_context *c,
                          const cj_doc *d,
                          int expr,
                          int checked,
                          qc_register *target,
                          int subtract,
                          qc_mask outer,
                          qc_mask pattern) {
    if (d->count > 100000)
        return qc_fail(c, "expression token budget exceeded");
    ar_builder p = {.c = c, .d = d};
    p.ranges = calloc((size_t)d->count, sizeof *p.ranges);
    if (!p.ranges)
        return qc_fail(c, "out of memory analyzing modular expression addition");
    uint32_t id = target->id;
    unsigned width = target->width;
    int ok = ar_bound(&p, checked, 0) && ar_bound(&p, expr, 0);
    ar_value value;
    if (ok)
        ok = ar_lower(&p, expr, width, &value);
    size_t computed = p.count;
    if (ok) {
        target = reg(c, id); // Private allocation can relocate the register array.
        for (unsigned b = 0; b < width && ok; ++b) {
            ar_bit bit = ar_at(&value, b);
            qc_mask mask = outer, state = pattern;
            int possible = 1;
            constrain(bit.wire, bit.inverted, 1, &mask, &state, &possible);
            uint64_t amount = UINT64_C(1) << b;
            if (subtract)
                amount = 0 - amount;
            if (possible)
                ok = add_constant(c, target, amount, mask, state);
        }
    }
    if (ok)
        for (size_t j = computed; j && ok;) {
            qc_gate g = p.gates[--j];
            ok = qc_emit(c, g.kind, g.target, g.mask, g.pattern, g.angle);
        }
    if (ok)
        for (unsigned j = p.nids; j && ok;) {
            qc_register *temporary = reg(c, p.ids[--j]);
            temporary->clean = 1;
            ok = release(c, temporary);
        }
    ar_free(&p);
    return ok || (*c->error ? 0 : qc_fail(c, "typed modular addition domain could not be proven"));
}
