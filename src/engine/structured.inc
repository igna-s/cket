/* Symbolic Boolean synthesis. Terms are masks of physical input wires,
   never a table of input values. A bounded term count selects the fallback. */
#define QC_SYMBOLIC_TERMS 256

typedef struct {
    qc_mask terms[QC_SYMBOLIC_TERMS];
    unsigned count;
} bit_poly;

static int poly_toggle(qc_context *c, bit_poly *p, qc_mask term) {
    if (++c->symbolic_work > 2000000)
        return qc_fail(c, "symbolic synthesis work budget exceeded");
    for (unsigned i = 0; i < p->count; ++i) {
        if (++c->symbolic_work > 2000000)
            return qc_fail(c, "symbolic synthesis work budget exceeded");
        if (p->terms[i] == term) {
            p->terms[i] = p->terms[--p->count];
            return 1;
        }
    }
    if (p->count == QC_SYMBOLIC_TERMS)
        return 0;
    p->terms[p->count++] = term;
    return 1;
}

static int poly_xor(qc_context *c, bit_poly *p, const bit_poly *b) {
    for (unsigned i = 0; i < b->count; ++i)
        if (!poly_toggle(c, p, b->terms[i]))
            return 0;
    return 1;
}

static int poly_product(qc_context *c, bit_poly *p, const bit_poly *a, const bit_poly *b) {
    p->count = 0;
    for (unsigned i = 0; i < a->count; ++i)
        for (unsigned j = 0; j < b->count; ++j)
            if (!poly_toggle(c, p, a->terms[i] | b->terms[j]))
                return 0;
    return 1;
}

static unsigned expr_nodes(const cj_doc *d, int e, int *nodes) {
    int args = cj_get(d, e, "args");
    unsigned n = 0;
    if (args >= 0)
        for (int a = args + 1; a < d->tokens[args].next && n < 3; a = d->tokens[a].next)
            nodes[n++] = a;
    return n;
}

static int
expr_bit(qc_context *c, const cj_doc *d, int e, unsigned bit, bit_poly *out, unsigned depth) {
    out->count = 0;
    if (depth > 128)
        return 0;
    if (++c->symbolic_work > 2000000)
        return qc_fail(c, "symbolic synthesis work budget exceeded");
    uint64_t width;
    if (!cj_u64(d, cj_get(d, e, "bits"), &width))
        return 0;
    int op = cj_get(d, e, "op"), sign = cj_bool(d, cj_get(d, e, "signed"));
    if (bit >= width) {
        if (!sign)
            return 1;
        bit = (unsigned)width - 1;
    }
    if (cj_equal(d, op, "const")) {
        uint64_t value;
        if (!cj_u64(d, cj_get(d, e, "value"), &value))
            return 0;
        if (width == 1)
            value = !!value;
        return !(value & (UINT64_C(1) << bit)) || poly_toggle(c, out, 0);
    }
    if (cj_equal(d, op, "reg")) {
        uint64_t id;
        if (!cj_u64(d, cj_get(d, e, "id"), &id))
            return 0;
        qc_register *r = reg(c, (uint32_t)id);
        if (r->basis_known)
            return !(r->basis_value & (UINT64_C(1) << bit)) || poly_toggle(c, out, 0);
        if (r->zero_mask & (UINT64_C(1) << bit))
            return 1;
        return poly_toggle(c, out, QC_WIRE(r->wires[bit]));
    }
    int nodes[3];
    unsigned n = expr_nodes(d, e, nodes);
    if (!n)
        return 0;
    if (cj_equal(d, op, "cast")) {
        uint64_t source_width;
        cj_u64(d, cj_get(d, nodes[0], "bits"), &source_width);
        if (width == 1 && source_width > 1)
            return 0; /* Boolean conversion is a zero test below. */
        return expr_bit(c, d, nodes[0], bit, out, depth + 1);
    }
    if (cj_equal(d, op, "shl") || cj_equal(d, op, "shr")) {
        uint64_t shift, source_width;
        if (n != 2 || !cj_equal(d, cj_get(d, nodes[1], "op"), "const") ||
            !cj_u64(d, cj_get(d, nodes[1], "value"), &shift) ||
            !cj_u64(d, cj_get(d, nodes[0], "bits"), &source_width) || shift >= source_width)
            return 0;
        if (cj_equal(d, op, "shr"))
            return expr_bit(c, d, nodes[0], bit + (unsigned)shift, out, depth + 1);
        if (sign)
            for (unsigned b = (unsigned)source_width - 1 - (unsigned)shift; b < source_width; ++b) {
                bit_poly high;
                if (!expr_bit(c, d, nodes[0], b, &high, depth + 1) || high.count)
                    return 0;
            }
        return bit < shift ? 1 : expr_bit(c, d, nodes[0], bit - (unsigned)shift, out, depth + 1);
    }
    if (cj_equal(d, op, "bnot"))
        return expr_bit(c, d, nodes[0], bit, out, depth + 1) && poly_toggle(c, out, 0);
    if (n != 2 || (!cj_equal(d, op, "band") && !cj_equal(d, op, "bor") && !cj_equal(d, op, "bxor")))
        return 0;
    bit_poly a, b;
    if (!expr_bit(c, d, nodes[0], bit, &a, depth + 1) ||
        !expr_bit(c, d, nodes[1], bit, &b, depth + 1))
        return 0;
    if (cj_equal(d, op, "bxor")) {
        *out = a;
        return poly_xor(c, out, &b);
    }
    if (!poly_product(c, out, &a, &b))
        return 0;
    return !cj_equal(d, op, "bor") || (poly_xor(c, out, &a) && poly_xor(c, out, &b));
}

/* A literal is a constant, one wire or its complement. */
static int poly_literal(const bit_poly *p, qc_mask *wire, int *inverted) {
    *wire = 0;
    *inverted = 0;
    for (unsigned i = 0; i < p->count; ++i) {
        if (!p->terms[i])
            *inverted ^= 1;
        else if (*wire || pop(p->terms[i]) != 1)
            return 0;
        else
            *wire = p->terms[i];
    }
    return 1;
}

static int
constrain(qc_mask wire, int inverted, int wanted, qc_mask *mask, qc_mask *pattern, int *possible) {
    int required = wanted ^ inverted;
    if (!wire) {
        if (required)
            *possible = 0;
        return 1;
    }
    if ((*mask & wire) && !!(*pattern & wire) != required)
        *possible = 0;
    *mask |= wire;
    if (required)
        *pattern |= wire;
    else
        *pattern &= ~wire;
    return 1;
}

static int emit_condition(qc_context *c,
                          unsigned output,
                          qc_mask local,
                          qc_mask values,
                          int possible,
                          qc_mask outer,
                          qc_mask state) {
    if (!possible || ((state ^ values) & outer & local))
        return 1;
    return qc_emit(c, QC_X, output, outer | local, state | values, 0);
}

static int structured_predicate(
    qc_context *c, const cj_doc *d, int e, qc_register *target, qc_mask outer, qc_mask state) {
    int op = cj_get(d, e, "op"), nodes[3];
    unsigned n = expr_nodes(d, e, nodes);
    uint64_t width;
    cj_u64(d, cj_get(d, e, "bits"), &width);
    while (cj_equal(d, op, "cast") && n == 1) {
        uint64_t child_width;
        cj_u64(d, cj_get(d, nodes[0], "bits"), &child_width);
        int child_op = cj_get(d, nodes[0], "op");
        int truth = cj_equal(d, child_op, "eq") || cj_equal(d, child_op, "ne") ||
                    cj_equal(d, child_op, "lt") || cj_equal(d, child_op, "le") ||
                    cj_equal(d, child_op, "gt") || cj_equal(d, child_op, "ge") ||
                    cj_equal(d, child_op, "not");
        if (width == 1 && child_width > 1 && !truth)
            break;
        e = nodes[0];
        op = cj_get(d, e, "op");
        width = child_width;
        n = expr_nodes(d, e, nodes);
    }
    int zero_test = cj_equal(d, op, "not") || (cj_equal(d, op, "cast") && width == 1);
    int equality = cj_equal(d, op, "eq") || cj_equal(d, op, "ne");
    int less = cj_equal(d, op, "lt") || cj_equal(d, op, "le");
    int greater = cj_equal(d, op, "gt") || cj_equal(d, op, "ge");
    if (!zero_test && !equality && !less && !greater)
        return 0;
    if (!n)
        return 0;
    int left = nodes[0], right = n > 1 ? nodes[1] : -1;
    uint64_t input_width;
    cj_u64(d, cj_get(d, left, "bits"), &input_width);
    qc_mask wires[64];
    int inversion[64], constant[64];
    for (unsigned b = 0; b < input_width; ++b) {
        bit_poly a, r = {.count = 0};
        if (!expr_bit(c, d, left, b, &a, 0))
            return 0;
        if (right >= 0 && !expr_bit(c, d, right, b, &r, 0))
            return 0;
        qc_mask rw;
        int ri;
        if (!poly_literal(&a, &wires[b], &inversion[b]) || !poly_literal(&r, &rw, &ri) || rw)
            return 0;
        constant[b] = ri;
    }
    qc_mask mask = 0, pattern = 0;
    int possible = 1;
    int sign = cj_bool(d, cj_get(d, left, "signed"));
    if (less || greater) {
        /* Sign-biased lexical comparison; every emitted prefix is disjoint. */
        for (unsigned step = 0; step < input_width; ++step) {
            unsigned b = (unsigned)input_width - 1 - step;
            int bias = sign && b == input_width - 1;
            int rhs = constant[b] ^ bias;
            if ((less && rhs) || (greater && !rhs)) {
                qc_mask trial_mask = mask, trial_pattern = pattern;
                int trial_possible = possible;
                constrain(wires[b],
                          inversion[b],
                          (less ? 0 : 1) ^ bias,
                          &trial_mask,
                          &trial_pattern,
                          &trial_possible);
                if (!emit_condition(c,
                                    target->wires[0],
                                    trial_mask,
                                    trial_pattern,
                                    trial_possible,
                                    outer,
                                    state))
                    return -1;
            }
            constrain(wires[b], inversion[b], constant[b], &mask, &pattern, &possible);
        }
        if (cj_equal(d, op, "le") || cj_equal(d, op, "ge"))
            if (!emit_condition(c, target->wires[0], mask, pattern, possible, outer, state))
                return -1;
        return 1;
    }
    for (unsigned b = 0; b < input_width; ++b)
        constrain(wires[b], inversion[b], constant[b], &mask, &pattern, &possible);
    int complement = cj_equal(d, op, "ne") || (zero_test && !cj_equal(d, op, "not"));
    if (complement && !qc_emit(c, QC_X, target->wires[0], outer, state, 0))
        return -1;
    return emit_condition(c, target->wires[0], mask, pattern, possible, outer, state) ? 1 : -1;
}

static int structured_compute(
    qc_context *c, const cj_doc *d, int expr, qc_register *target, qc_mask outer, qc_mask state) {
    c->symbolic_work = 0;
    bit_poly *bits = calloc(target->width, sizeof *bits);
    if (!bits) {
        qc_fail(c, "out of memory in structured synthesis");
        return -1;
    }
    int supported = 1;
    for (unsigned b = 0; b < target->width; ++b)
        if (!expr_bit(c, d, expr, b, &bits[b], 0)) {
            supported = 0;
            break;
        }
    if (!supported) {
        free(bits);
        if (c->error[0])
            return -1;
        int result = structured_predicate(c, d, expr, target, outer, state);
        return c->error[0] ? -1 : result;
    }
    int ok = 1;
    for (unsigned b = 0; b < target->width && ok; ++b)
        for (unsigned t = 0; t < bits[b].count && ok; ++t) {
            qc_mask term = bits[b].terms[t];
            if (((state ^ term) & outer & term))
                continue;
            ok = qc_emit(c, QC_X, target->wires[b], outer | term, state | term, 0);
        }
    free(bits);
    return ok ? 1 : -1;
}
