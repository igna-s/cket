/* Prove a bit permutation or an affine map before emitting any update.
 * The certificate describes a unitary on the whole destination bit space;
 * arithmetic definedness is checked separately on its reachable support. */
typedef struct {
    uint64_t coefficient, offset;
} ip_affine;

static int ip_affine_expression(
    ar_builder *p, int e, uint32_t id, unsigned width, ip_affine *out, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "in-place proof work budget exceeded");
    uint64_t bits;
    cj_u64(p->d, cj_get(p->d, e, "bits"), &bits);
    int op = cj_get(p->d, e, "op"), nodes[3];
    unsigned count = expr_nodes(p->d, e, nodes);
    uint64_t mask = bits_mask(width);
    if (cj_equal(p->d, op, "const")) {
        uint64_t value;
        cj_u64(p->d, cj_get(p->d, e, "value"), &value);
        *out = (ip_affine){
            0,
            (uint64_t)numeric(value, (unsigned)bits, cj_bool(p->d, cj_get(p->d, e, "signed"))) &
                mask};
        return 1;
    }
    if (bits < width || bits == 1)
        return 0; /* Narrowing followed by widening need not be affine. */
    if (cj_equal(p->d, op, "reg")) {
        uint64_t source;
        cj_u64(p->d, cj_get(p->d, e, "id"), &source);
        if (source != id)
            return 0;
        *out = (ip_affine){1, 0};
        return 1;
    }
    if (cj_equal(p->d, op, "sequence"))
        return ip_affine_expression(p, nodes[1], id, width, out, depth + 1);
    if (cj_equal(p->d, op, "cast"))
        return ip_affine_expression(p, nodes[0], id, width, out, depth + 1);
    ip_affine a, b;
    if (!count || !ip_affine_expression(p, nodes[0], id, width, &a, depth + 1))
        return 0;
    if (cj_equal(p->d, op, "neg") || cj_equal(p->d, op, "bnot")) {
        *out = (ip_affine){(0 - a.coefficient) & mask,
                           (0 - a.offset - cj_equal(p->d, op, "bnot")) & mask};
        return 1;
    }
    if (count != 2 || !ip_affine_expression(p, nodes[1], id, width, &b, depth + 1))
        return 0;
    if (cj_equal(p->d, op, "add") || cj_equal(p->d, op, "sub")) {
        int subtract = cj_equal(p->d, op, "sub");
        *out = (ip_affine){(a.coefficient + (subtract ? 0 - b.coefficient : b.coefficient)) & mask,
                           (a.offset + (subtract ? 0 - b.offset : b.offset)) & mask};
        return 1;
    }
    if (cj_equal(p->d, op, "mul") && (!a.coefficient || !b.coefficient)) {
        *out = (ip_affine){(a.coefficient * b.offset + b.coefficient * a.offset) & mask,
                           (a.offset * b.offset) & mask};
        return 1;
    }
    return 0;
}

static int ip_truth(ar_builder *p, int e, bit_poly *out, unsigned depth);

static int ip_bit(ar_builder *p, int e, unsigned bit, bit_poly *out, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "in-place proof work budget exceeded");
    int op = cj_get(p->d, e, "op"), nodes[3];
    uint64_t bits;
    cj_u64(p->d, cj_get(p->d, e, "bits"), &bits);
    expr_nodes(p->d, e, nodes);
    if (!bit && cj_equal(p->d, op, "not"))
        return ip_truth(p, nodes[0], out, depth + 1) && poly_toggle(p->c, out, 0);
    if (!bit && bits == 1 && cj_equal(p->d, op, "cast"))
        return ip_truth(p, nodes[0], out, depth + 1);
    if (!bit && bits == 1 && (cj_equal(p->d, op, "land") || cj_equal(p->d, op, "lor")))
        return ip_truth(p, e, out, depth + 1);
    if (!bit && bits == 1 && (cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne")))
        return ip_truth(p, e, out, depth + 1);
    if (!bit && bits == 1 && cj_equal(p->d, op, "mul"))
        return ip_truth(p, e, out, depth + 1);
    return expr_bit(p->c, p->d, e, bit, out, depth + 1);
}

static int ip_truth(ar_builder *p, int e, bit_poly *out, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "in-place proof work budget exceeded");
    out->count = 0;
    int op = cj_get(p->d, e, "op"), nodes[3];
    unsigned count = expr_nodes(p->d, e, nodes);
    if (cj_equal(p->d, op, "const")) {
        uint64_t value, bits;
        cj_u64(p->d, cj_get(p->d, e, "value"), &value);
        cj_u64(p->d, cj_get(p->d, e, "bits"), &bits);
        return !(value & bits_mask((unsigned)bits)) || poly_toggle(p->c, out, 0);
    }
    if (p->ranges[e].valid) {
        int known = ar_known_truth(p->ranges[e]);
        if (known >= 0)
            return !known || poly_toggle(p->c, out, 0);
    }
    if (cj_equal(p->d, op, "cast")) {
        uint64_t to, from;
        cj_u64(p->d, cj_get(p->d, e, "bits"), &to);
        cj_u64(p->d, cj_get(p->d, nodes[0], "bits"), &from);
        if (to == 1 || to >= from)
            return ip_truth(p, nodes[0], out, depth + 1);
    }
    if (cj_equal(p->d, op, "not"))
        return ip_truth(p, nodes[0], out, depth + 1) && poly_toggle(p->c, out, 0);
    if (cj_equal(p->d, op, "neg"))
        return ip_truth(p, nodes[0], out, depth + 1);
    if (count == 2 && cj_equal(p->d, op, "mul")) {
        unsigned side = cj_equal(p->d, cj_get(p->d, nodes[1], "op"), "const") ? 1 : 0;
        if (cj_equal(p->d, cj_get(p->d, nodes[side], "op"), "const") &&
            !ar_zero_constant(p, nodes[side])) {
            ar_range other = p->ranges[nodes[!side]];
            if (other.valid && other.lo >= 0 && other.hi <= 1)
                return ip_truth(p, nodes[!side], out, depth + 1);
        }
        return 0;
    }
    if (count == 2 && (cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne"))) {
        int equality = cj_equal(p->d, op, "eq");
        int value = ar_zero_constant(p, nodes[0])   ? nodes[1]
                    : ar_zero_constant(p, nodes[1]) ? nodes[0]
                                                    : -1;
        if (value >= 0)
            return ip_truth(p, value, out, depth + 1) && (!equality || poly_toggle(p->c, out, 0));
        ar_range a = p->ranges[nodes[0]], b = p->ranges[nodes[1]];
        if (a.valid && b.valid && a.lo >= 0 && a.hi <= 1 && b.lo >= 0 && b.hi <= 1) {
            bit_poly other;
            return ip_truth(p, nodes[0], out, depth + 1) &&
                   ip_truth(p, nodes[1], &other, depth + 1) && poly_xor(p->c, out, &other) &&
                   (!equality || poly_toggle(p->c, out, 0));
        }
        return 0;
    }
    if (count == 2 && (cj_equal(p->d, op, "land") || cj_equal(p->d, op, "lor"))) {
        bit_poly a, b;
        if (!ip_truth(p, nodes[0], &a, depth + 1) || !ip_truth(p, nodes[1], &b, depth + 1) ||
            !poly_product(p->c, out, &a, &b))
            return 0;
        return !cj_equal(p->d, op, "lor") || (poly_xor(p->c, out, &a) && poly_xor(p->c, out, &b));
    }
    if (p->ranges[e].valid && p->ranges[e].lo >= 0 && p->ranges[e].hi <= 1)
        return ip_bit(p, e, 0, out, depth + 1);
    return 0;
}

static int
ip_emit_affine(qc_context *c, qc_register *r, ip_affine value, qc_mask outer, qc_mask state) {
    uint64_t mask = bits_mask(r->width);
    if (!(value.coefficient & 1))
        return qc_fail(
            c,
            "affine assignment is not injective on its declared bit space; retained history or a proven support extension is required");
    if (value.coefficient == mask) {
        for (unsigned bit = 0; bit < r->width; ++bit)
            if (!qc_emit(c, QC_X, r->wires[bit], outer, state, 0))
                return 0;
        return add_constant(c, r, value.offset + 1, outer, state);
    }
    /* For odd a, a-1 is even. In descending order, adding (a-1)*2^i
     * touches only bits above i, so the control is still the original x_i.
     * No copy, multiplication table or private register is needed. */
    uint64_t delta = value.coefficient - 1;
    for (unsigned step = r->width; step; --step) {
        unsigned bit = step - 1;
        uint64_t amount = (delta << bit) & mask;
        qc_mask control = QC_WIRE(r->wires[bit]);
        if (amount && !add_constant(c, r, amount, outer | control, state | control))
            return 0;
    }
    return add_constant(c, r, value.offset, outer, state);
}

static int inplace_structured(
    qc_context *c, const cj_doc *d, int e, qc_register *r, qc_mask outer, qc_mask state) {
    if (d->count > 100000)
        return qc_fail(c, "expression token budget exceeded"), -1;
    ar_builder p = {.c = c, .d = d};
    p.ranges = calloc((size_t)d->count, sizeof *p.ranges);
    if (!p.ranges)
        return qc_fail(c, "out of memory analyzing in-place arithmetic"), -1;
    if (!ar_bound(&p, e, 0)) {
        int result = *c->error ? -1 : 0;
        ar_free(&p);
        return result;
    }
    /* Do not turn a known basis bit into a literal while proving a global
     * bit permutation. A basis preparation is not a bijectivity certificate. */
    int saved_basis = r->basis_known;
    uint64_t saved_zero = r->zero_mask;
    r->basis_known = 0;
    r->zero_mask = 0;
    int global = 1;
    if (r->width == 1) {
        ar_free(&p);
        p = (ar_builder){.c = c, .d = d};
        p.ranges = calloc((size_t)d->count, sizeof *p.ranges);
        if (!p.ranges)
            qc_fail(c, "out of memory analyzing Boolean in-place arithmetic");
        global = p.ranges && ar_bound(&p, e, 0);
    }
    unsigned wanted[64], current[64];
    int inverted[64], bit_map = global;
    uint64_t seen = 0;
    for (unsigned bit = 0; bit < r->width && bit_map; ++bit) {
        bit_poly polynomial;
        qc_mask wire;
        bit_map = ip_bit(&p, e, bit, &polynomial, 0) &&
                  poly_literal(&polynomial, &wire, &inverted[bit]) && wire;
        if (!bit_map)
            break;
        unsigned source = 0;
        while (source < r->width && wire != QC_WIRE(r->wires[source]))
            ++source;
        if (source == r->width || (seen & (UINT64_C(1) << source))) {
            bit_map = 0;
            break;
        }
        wanted[bit] = source;
        seen |= UINT64_C(1) << source;
    }
    r->basis_known = saved_basis;
    r->zero_mask = saved_zero;
    ip_affine affine;
    int affine_map = !*c->error && !bit_map && r->width > 1 &&
                     ip_affine_expression(&p, e, r->id, r->width, &affine, 0);
    int error = !!*c->error;
    ar_free(&p);
    if (error)
        return -1;
    if (!bit_map && !affine_map)
        return 0;
    ++c->synthesis_runs;
    if (affine_map)
        return ip_emit_affine(c, r, affine, outer, state) ? 1 : -1;
    for (unsigned bit = 0; bit < r->width; ++bit)
        current[bit] = bit;
    for (unsigned bit = 0; bit < r->width; ++bit) {
        unsigned source = bit;
        while (current[source] != wanted[bit])
            ++source;
        if (source != bit) {
            qc_mask a = QC_WIRE(r->wires[bit]), b = QC_WIRE(r->wires[source]);
            if (!qc_emit(c, QC_X, r->wires[bit], outer | b, state | b, 0) ||
                !qc_emit(c, QC_X, r->wires[source], outer | a, state | a, 0) ||
                !qc_emit(c, QC_X, r->wires[bit], outer | b, state | b, 0))
                return -1;
            unsigned swap = current[bit];
            current[bit] = current[source];
            current[source] = swap;
        }
        if (inverted[bit] && !qc_emit(c, QC_X, r->wires[bit], outer, state, 0))
            return -1;
    }
    return 1;
}
