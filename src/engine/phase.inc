/* Phase selection uses saved value/epoch certificates, never amplitudes or
 * measurements. Unknown predicates remain possible; only a proved conflict
 * permits skipping classical angle arithmetic. */
typedef struct {
    uint32_t id;
    int value;
} phase_fact;

typedef struct {
    qc_context *c;
    phase_fact facts[QC_MAX_SOURCES];
    unsigned count, work;
} phase_proof;

static int phase_known_integer(
    phase_proof *p, const cj_doc *d, int e, uint32_t ignored, uint64_t *value, unsigned depth);

static int phase_known_register(
    phase_proof *p, uint32_t id, uint32_t ignored, uint64_t *value, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "phase control proof work/depth budget exceeded");
    qc_register *r = reg(p->c, id);
    if (!r || !r->alive || !r->prepared)
        return 0;
    if (r->basis_known) {
        *value = r->basis_value;
        return 1;
    }
    if (id != ignored)
        for (unsigned i = 0; i < p->count; ++i)
            if (p->facts[i].id == id) {
                *value = (unsigned)p->facts[i].value;
                return 1;
            }
    if (!r->definition || r->definition_mask)
        return 0;
    for (unsigned i = 0; i < r->ndeps; ++i) {
        qc_register *input = reg(p->c, r->deps[i]);
        if (!input || !input->alive || input->epoch != r->epochs[i])
            return 0;
    }
    cj_doc local = {0};
    const cj_doc *saved = parsed_operation(p->c, r->definition, &local);
    int known = saved && phase_known_integer(p, saved, 0, ignored, value, depth + 1);
    cj_free(&local);
    return known;
}

static int phase_known_integer(
    phase_proof *p, const cj_doc *d, int e, uint32_t ignored, uint64_t *value, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "phase control proof work/depth budget exceeded");
    int op = cj_get(d, e, "op"), nodes[3];
    unsigned n = expr_nodes(d, e, nodes);
    uint64_t width, id;
    if (!cj_u64(d, cj_get(d, e, "bits"), &width))
        return 0;
    if (cj_equal(d, op, "reg") && cj_u64(d, cj_get(d, e, "id"), &id))
        return phase_known_register(p, (uint32_t)id, ignored, value, depth + 1);
    if (cj_equal(d, op, "select") && n == 3) {
        uint64_t decision;
        if (phase_known_integer(p, d, nodes[0], ignored, &decision, depth + 1))
            return phase_known_integer(p, d, nodes[decision ? 1 : 2], ignored, value, depth + 1);
        if (*p->c->error)
            return 0;
        uint64_t yes, no;
        if (phase_known_integer(p, d, nodes[1], ignored, &yes, depth + 1) &&
            phase_known_integer(p, d, nodes[2], ignored, &no, depth + 1) && yes == no) {
            *value = yes;
            return 1;
        }
        return 0;
    }
    domain inputs = {0};
    if (!leaves(p->c, d, e, &inputs))
        return 0;
    uint64_t values[QC_MAX_SOURCES];
    for (unsigned i = 0; i < inputs.count; ++i)
        if (!phase_known_register(p, inputs.sources[i].id, ignored, &values[i], depth + 1))
            return 0;
    /* Hypothetical control facts can be inconsistent. A failed value proof
     * is unknown, not an arithmetic error on an executed source-C path. */
    qc_context probe = *p->c;
    probe.error[0] = 0;
    return evaluate(&probe, d, e, &inputs, values, value);
}

static int phase_add_fact(phase_proof *p, uint32_t id, int value, int *inactive) {
    for (unsigned i = 0; i < p->count; ++i)
        if (p->facts[i].id == id) {
            *inactive |= p->facts[i].value != value;
            return 1;
        }
    if (p->count == QC_MAX_SOURCES)
        return qc_fail(p->c, "phase control fact budget exceeded");
    p->facts[p->count++] = (phase_fact){id, value};
    return 1;
}

static int phase_inactive(qc_context *c, const cj_doc *d, int inst, int *inactive) {
    phase_proof proof = {.c = c};
    ar_builder aliases = {.c = c, .d = d};
    int list = cj_get(d, inst, "controls");
    *inactive = 0;
    if (list < 0)
        return 1;
    for (int token = list + 1; token < d->tokens[list].next; token = d->tokens[token].next) {
        uint64_t id, wanted;
        uint32_t original;
        int inverted = 0;
        if (!number(c, d, token, "id", &id) || !number(c, d, token, "value", &wanted) ||
            !phase_add_fact(&proof, (uint32_t)id, (int)wanted, inactive))
            return 0;
        if (ar_register_boolean_alias(&aliases, (uint32_t)id, &original, &inverted, 0) &&
            !phase_add_fact(&proof, original, (int)wanted ^ inverted, inactive))
            return 0;
        if (*c->error || *inactive)
            return !*c->error;
    }
    for (int token = list + 1; token < d->tokens[list].next; token = d->tokens[token].next) {
        uint64_t id, wanted, value;
        if (!number(c, d, token, "id", &id) || !number(c, d, token, "value", &wanted))
            return 0;
        if (phase_known_register(&proof, (uint32_t)id, (uint32_t)id, &value, 0) &&
            (value != 0) != (wanted != 0)) {
            *inactive = 1;
            return 1;
        }
        if (*c->error)
            return 0;
    }
    return 1;
}

static int phase_integer_value(qc_context *c, const cj_doc *d, int e, uint64_t *value) {
    domain inputs;
    if (!collect_domain(c, d, e, &inputs))
        return 0;
    if (inputs.count)
        return qc_fail(c, "deferred phase arithmetic must contain only CPU values");
    return evaluate(c, d, e, &inputs, NULL, value);
}

static int phase_value(qc_context *c, const cj_doc *d, int e, double *value, unsigned depth) {
    if (depth > 128)
        return qc_fail(c, "phase expression depth budget exceeded");
    int op = cj_get(d, e, "op"), nodes[3];
    unsigned n = expr_nodes(d, e, nodes);
    uint64_t width;
    if (!number(c, d, e, "bits", &width))
        return 0;
    if (cj_equal(d, op, "real_const")) {
        *value = cj_double(d, cj_get(d, e, "value"));
        if (width == 32)
            *value = (float)*value;
        return 1;
    }
    if (cj_equal(d, op, "real_from_int") || cj_equal(d, op, "real_bits")) {
        uint64_t raw, bits;
        if (!phase_integer_value(c, d, nodes[0], &raw) || !number(c, d, nodes[0], "bits", &bits))
            return 0;
        if (cj_equal(d, op, "real_bits")) {
            if (width == 64)
                memcpy(value, &raw, sizeof *value);
            else {
                uint32_t encoded = (uint32_t)raw;
                float real;
                memcpy(&real, &encoded, sizeof real);
                *value = real;
            }
            return 1;
        }
        __int128 integer = numeric(raw, (unsigned)bits, cj_bool(d, cj_get(d, nodes[0], "signed")));
        *value = width == 32 ? (double)(float)integer : (double)integer;
        return 1;
    }
    if (cj_equal(d, op, "real_select")) {
        uint64_t condition;
        return phase_integer_value(c, d, nodes[0], &condition) &&
               phase_value(c, d, nodes[condition ? 1 : 2], value, depth + 1);
    }
    double a, b = 0;
    if (!phase_value(c, d, nodes[0], &a, depth + 1) ||
        (n == 2 && !phase_value(c, d, nodes[1], &b, depth + 1)))
        return 0;
    if (width == 32) {
        float fa = (float)a, fb = (float)b;
        volatile float rounded = cj_equal(d, op, "real_cast")  ? fa
                                 : cj_equal(d, op, "real_neg") ? -fa
                                 : cj_equal(d, op, "real_abs") ? fabsf(fa)
                                 : cj_equal(d, op, "real_add") ? fa + fb
                                 : cj_equal(d, op, "real_sub") ? fa - fb
                                 : cj_equal(d, op, "real_mul") ? fa * fb
                                                               : fa / fb;
        *value = rounded;
    } else
        *value = cj_equal(d, op, "real_cast")  ? a
                 : cj_equal(d, op, "real_neg") ? -a
                 : cj_equal(d, op, "real_abs") ? fabs(a)
                 : cj_equal(d, op, "real_add") ? a + b
                 : cj_equal(d, op, "real_sub") ? a - b
                 : cj_equal(d, op, "real_mul") ? a * b
                                               : a / b;
    return 1;
}
