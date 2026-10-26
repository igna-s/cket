/* Prove a store is the existing word without reading amplitudes. Only pure
 * bit operations are expanded; unsupported expressions keep normal validation
 * and synthesis, including their arithmetic definedness checks. */
typedef struct {
    qc_context *c;
    qc_mask mask, pattern;
    unsigned work;
} heap_identity_proof;

static int heap_identity_defined(heap_identity_proof *p, const char *text) {
    cj_doc d = {0};
    uint64_t width;
    int valid = cj_parse(&d, text) && cj_u64(&d, cj_get(&d, 0, "bits"), &width);
    for (unsigned bit = 0; valid && bit < width; ++bit) {
        bit_poly value;
        valid = expr_bit(p->c, &d, 0, bit, &value, 0);
    }
    cj_free(&d);
    return valid;
}

static int heap_identity_truth(heap_identity_proof *p, const cj_doc *d, int e, int *value) {
    uint64_t width, raw;
    int op = cj_get(d, e, "op");
    if (!cj_u64(d, cj_get(d, e, "bits"), &width))
        return 0;
    if (cj_equal(d, op, "const") && cj_u64(d, cj_get(d, e, "value"), &raw)) {
        *value = width == 1 ? raw != 0 : (raw & bits_mask((unsigned)width)) != 0;
        return 1;
    }
    if (!cj_equal(d, op, "reg")) {
        // A runtime CPU length becomes typed constants at binding. Resolve
        // its comparisons without treating any quantum input as a constant.
        qc_context constant = *p->c;
        constant.error[0] = 0;
        domain inputs;
        if (collect_domain(&constant, d, e, &inputs) && !inputs.count &&
            evaluate(&constant, d, e, &inputs, NULL, &raw)) {
            *value = raw != 0;
            return 1;
        }
        return 0;
    }
    if (width != 1 || !cj_equal(d, op, "reg") || !cj_u64(d, cj_get(d, e, "id"), &raw))
        return 0;
    qc_register *r = reg(p->c, (uint32_t)raw);
    if (!r || !r->alive || !r->prepared || r->width != 1)
        return 0;
    qc_mask bit = QC_WIRE(r->wires[0]);
    if (p->mask & bit) {
        *value = (p->pattern & bit) != 0;
        return 1;
    }
    if (!r->basis_known)
        return 0;
    *value = r->basis_value != 0;
    return 1;
}

static char *
heap_identity_expression(heap_identity_proof *p, const cj_doc *d, int e, unsigned depth) {
    if (depth > 128 || ++p->work > 4096)
        return NULL;
    int op = cj_get(d, e, "op");
    if (cj_equal(d, op, "const"))
        return cj_copy(d, e);
    if (cj_equal(d, op, "reg")) {
        uint64_t id;
        if (!cj_u64(d, cj_get(d, e, "id"), &id))
            return NULL;
        qc_register *r = reg(p->c, (uint32_t)id);
        if (!r || !r->alive || !r->prepared)
            return NULL;
        if (r->width == 1 && (p->mask & QC_WIRE(r->wires[0]))) {
            char constant[96];
            snprintf(constant,
                     sizeof constant,
                     "{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":%u}",
                     (p->pattern & QC_WIRE(r->wires[0])) != 0);
            return strdup(constant);
        }
        int valid = r->definition && (r->definition_mask & p->mask) == r->definition_mask &&
                    ((r->definition_pattern ^ p->pattern) & r->definition_mask) == 0;
        for (unsigned i = 0; valid && i < r->ndeps; ++i) {
            qc_register *source = reg(p->c, r->deps[i]);
            valid = source && source->alive && source->epoch == r->epochs[i];
        }
        if (valid) {
            cj_doc saved = {0};
            char *expanded = cj_parse(&saved, r->definition)
                                 ? heap_identity_expression(p, &saved, 0, depth + 1)
                                 : NULL;
            cj_free(&saved);
            if (expanded)
                return expanded;
        }
        return cj_copy(d, e);
    }
    int nodes[3];
    unsigned count = expr_nodes(d, e, nodes);
    if (cj_equal(d, op, "sequence") && count == 2) {
        char *discarded = heap_identity_expression(p, d, nodes[0], depth + 1);
        char *result = discarded && heap_identity_defined(p, discarded)
                           ? heap_identity_expression(p, d, nodes[1], depth + 1)
                           : NULL;
        free(discarded);
        return result;
    }
    if (cj_equal(d, op, "select") && count == 3) {
        int value;
        return heap_identity_truth(p, d, nodes[0], &value)
                   ? heap_identity_expression(p, d, nodes[value ? 1 : 2], depth + 1)
                   : NULL;
    }
    const char *operation = NULL;
    const char *allowed[] = {"cast", "bnot", "band", "bor", "bxor", "shl", "shr"};
    for (unsigned i = 0; i < sizeof allowed / sizeof *allowed; ++i)
        if (cj_equal(d, op, allowed[i]))
            operation = allowed[i];
    unsigned arity =
        operation && (!strcmp(operation, "cast") || !strcmp(operation, "bnot")) ? 1 : 2;
    if (!operation || count != arity)
        return NULL;
    char *arguments[2] = {NULL, NULL};
    size_t size = 160;
    for (unsigned i = 0; i < count; ++i) {
        arguments[i] = heap_identity_expression(p, d, nodes[i], depth + 1);
        if (!arguments[i])
            goto done;
        size += strlen(arguments[i]) + 1;
    }
    if (size <= 65536) {
        uint64_t width;
        if (!cj_u64(d, cj_get(d, e, "bits"), &width))
            goto done;
        char *text = malloc(size);
        if (text) {
            snprintf(text,
                     size,
                     "{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[%s%s%s]}",
                     operation,
                     (unsigned)width,
                     cj_bool(d, cj_get(d, e, "signed")) ? "true" : "false",
                     arguments[0],
                     count == 2 ? "," : "",
                     count == 2 ? arguments[1] : "");
            for (unsigned i = 0; i < count; ++i)
                free(arguments[i]);
            return text;
        }
    }
done:
    for (unsigned i = 0; i < count; ++i)
        free(arguments[i]);
    return NULL;
}

static int heap_store_identity(qc_context *c, const cj_doc *d, int inst, uint32_t owner) {
    qc_context proof = *c;
    proof.symbolic_work = 0;
    proof.error[0] = 0;
    heap_identity_proof p = {.c = &proof};
    int impossible;
    if (!controls(&proof, d, inst, &p.mask, &p.pattern, &impossible) || impossible)
        return 0;
    char *text = heap_identity_expression(&p, d, cj_get(d, inst, "expr"), 0);
    cj_doc expression = {0};
    if (!text || !cj_parse(&expression, text)) {
        free(text);
        cj_free(&expression);
        return 0;
    }
    // Symbolic proof has a private work/error budget and emits no gates.
    proof.symbolic_work = 0;
    proof.error[0] = 0;
    qc_register *r = reg(c, owner);
    int same = 1;
    for (unsigned bit = 0; same && bit < r->width; ++bit) {
        bit_poly value;
        same = expr_bit(&proof, &expression, 0, bit, &value, 0);
        qc_mask wire = QC_WIRE(r->wires[bit]);
        if (same && (p.mask & wire)) {
            if (p.pattern & wire)
                same = poly_toggle(&proof, &value, 0);
        } else if (same && r->basis_known && ((r->basis_value >> bit) & 1))
            same = poly_toggle(&proof, &value, 0);
        else if (same && !r->basis_known && !((r->zero_mask >> bit) & 1))
            same = poly_toggle(&proof, &value, wire);
        same = same && value.count == 0;
    }
    cj_free(&expression);
    free(text);
    return same;
}
