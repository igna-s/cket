/* Transport saved inverse inputs across a controlled wire exchange. The old
 * value is in the other owner on active amplitudes and in the same owner on
 * inactive amplitudes. This changes recipes, never copies a quantum state. */
typedef struct {
    uint32_t id, deps[QC_MAX_SOURCES];
    uint64_t epochs[QC_MAX_SOURCES];
    unsigned ndeps;
    char *definition;
    qc_mask mask, pattern;
} swap_inverse;

static char *swap_select(qc_context *c,
                         unsigned width,
                         int sign,
                         const char *condition,
                         const char *yes,
                         const char *no) {
    return heap_memory_text(c,
                            "{\"op\":\"select\",\"bits\":%u,\"signed\":%s,\"args\":[%s,%s,%s]}",
                            width,
                            sign ? "true" : "false",
                            condition,
                            yes,
                            no);
}

static qc_register *swap_guard_owner(qc_context *c, unsigned wire) {
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (r->alive && r->width == 1 && r->wires[0] == wire)
            return r;
    }
    return NULL;
}

static char *swap_stable_register(
    qc_context *c, uint32_t id, uint32_t a, uint32_t b, unsigned depth, unsigned *work);

// Recompute a private decision from unchanged roots instead of retaining a
// second correlated copy of its value for every outstanding inverse.
static char *swap_stable_text(
    qc_context *c, const char *text, uint32_t a, uint32_t b, unsigned depth, unsigned *work) {
    cj_doc d = {0};
    if (depth > 128 || ++*work > 4096 || !cj_parse(&d, text)) {
        cj_free(&d);
        qc_fail(c, "swap decision expansion proof budget or syntax failure");
        return NULL;
    }
    char *out = malloc(65537);
    if (!out) {
        cj_free(&d);
        qc_fail(c, "cannot allocate expanded swap decision");
        return NULL;
    }
    size_t copied = 0, used = 0;
    for (int t = 0; t < d.count; ++t) {
        uint64_t id;
        if (d.tokens[t].kind != '{' || !cj_equal(&d, cj_get(&d, t, "op"), "reg") ||
            !cj_u64(&d, cj_get(&d, t, "id"), &id))
            continue;
        qc_register *source = reg(c, (uint32_t)id);
        char *value =
            source && source->alive && source->prepared && !source->definition && id != a && id != b
                ? cj_copy(&d, t)
                : swap_stable_register(c, (uint32_t)id, a, b, depth + 1, work);
        size_t before = d.tokens[t].begin - copied;
        if (!value || used + before + strlen(value) > 65536) {
            free(value);
            free(out);
            cj_free(&d);
            if (!*c->error)
                qc_fail(c, "expanded swap decision text budget exceeded");
            return NULL;
        }
        memcpy(out + used, text + copied, before);
        used += before;
        memcpy(out + used, value, strlen(value));
        used += strlen(value);
        copied = d.tokens[t].end;
        free(value);
    }
    size_t after = strlen(text + copied);
    if (used + after > 65536) {
        free(out);
        cj_free(&d);
        qc_fail(c, "expanded swap decision text budget exceeded");
        return NULL;
    }
    memcpy(out + used, text + copied, after + 1);
    cj_free(&d);
    return out;
}

static char *swap_stable_register(
    qc_context *c, uint32_t id, uint32_t a, uint32_t b, unsigned depth, unsigned *work) {
    if (depth > 128 || ++*work > 4096) {
        qc_fail(c, "swap decision expansion proof budget exceeded");
        return NULL;
    }
    qc_register *r = reg(c, id);
    if (!r || !r->alive || !r->prepared || id == a || id == b) {
        qc_fail(c, "swap decision still borrows an exchanged or unavailable owner");
        return NULL;
    }
    if (!r->definition)
        return heap_memory_register(c, id);
    for (unsigned i = 0; i < r->ndeps; ++i) {
        qc_register *input = reg(c, r->deps[i]);
        if (!input || !input->alive || input->epoch != r->epochs[i]) {
            qc_fail(c, "swap decision input epochs changed");
            return NULL;
        }
    }
    char *value = swap_stable_text(c, r->definition, a, b, depth + 1, work);
    if (!value)
        return NULL;
    if (r->definition_mask) {
        char *zero = heap_memory_text(c,
                                      "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":0}",
                                      r->width,
                                      r->signed_type ? "true" : "false");
        if (!zero) {
            free(value);
            return NULL;
        }
        for (unsigned wire = 0; wire < QC_MAX_PHYSICAL; ++wire) {
            qc_mask bit = QC_WIRE(wire);
            if (!(r->definition_mask & bit))
                continue;
            qc_register *guard = swap_guard_owner(c, wire);
            char *condition =
                guard ? swap_stable_register(c, guard->id, a, b, depth + 1, work) : NULL;
            char *next = condition ? swap_select(c,
                                                 r->width,
                                                 r->signed_type,
                                                 condition,
                                                 r->definition_pattern & bit ? value : zero,
                                                 r->definition_pattern & bit ? zero : value)
                                   : NULL;
            free(condition);
            free(value);
            value = next;
            if (!value) {
                free(zero);
                if (!*c->error)
                    qc_fail(c, "swap decision gate control is unavailable");
                return NULL;
            }
        }
        free(zero);
    }
    return heap_memory_cast(c, r->width, r->signed_type > 0, value);
}

static char *
swap_old_value(qc_context *c, const cj_doc *d, int inst, uint32_t same, uint32_t other) {
    qc_register *r = reg(c, same);
    char *inactive = heap_memory_register(c, same), *value = heap_memory_register(c, other);
    int guards = cj_get(d, inst, "controls");
    if (!inactive || !value)
        goto failed;
    if (guards >= 0)
        for (int t = guards + 1; t < d->tokens[guards].next; t = d->tokens[t].next) {
            uint64_t id, wanted;
            cj_u64(d, cj_get(d, t, "id"), &id);
            cj_u64(d, cj_get(d, t, "value"), &wanted);
            unsigned work = 0;
            char *condition = swap_stable_register(c, (uint32_t)id, same, other, 0, &work);
            char *next = condition ? swap_select(c,
                                                 r->width,
                                                 r->signed_type,
                                                 condition,
                                                 wanted ? value : inactive,
                                                 wanted ? inactive : value)
                                   : NULL;
            free(condition);
            free(value);
            value = next;
            if (!value)
                goto failed;
        }
    uint64_t selected = bits_mask(r->width);
    cj_u64(d, cj_get(d, inst, "bit_mask"), &selected);
    uint64_t selector;
    int quantum_mask = cj_u64(d, cj_get(d, inst, "mask_register"), &selector);
    if (quantum_mask || selected != bits_mask(r->width)) {
        char *written_mask;
        if (quantum_mask) {
            unsigned work = 0;
            written_mask = heap_memory_binary(
                c,
                "band",
                r->width,
                heap_memory_cast(
                    c,
                    r->width,
                    0,
                    swap_stable_register(c, (uint32_t)selector, same, other, 0, &work)),
                heap_memory_leaf(c, "const", r->width, selected));
        } else
            written_mask = heap_memory_leaf(c, "const", r->width, selected);
        if (!written_mask)
            goto failed;
        char *kept_mask =
            heap_memory_binary(c,
                               "bxor",
                               r->width,
                               heap_memory_leaf(c, "const", r->width, bits_mask(r->width)),
                               strdup(written_mask));
        char *untouched = heap_memory_binary(
            c, "band", r->width, heap_memory_cast(c, r->width, 0, inactive), kept_mask);
        inactive = NULL;
        char *written = heap_memory_binary(
            c, "band", r->width, heap_memory_cast(c, r->width, 0, value), written_mask);
        value = heap_memory_cast(c,
                                 r->width,
                                 r->signed_type,
                                 heap_memory_binary(c, "bor", r->width, untouched, written));
        if (!value)
            goto failed;
    }
    free(inactive);
    return value;
failed:
    free(inactive);
    free(value);
    return NULL;
}

static char *swap_definition(
    qc_context *c, const char *text, uint32_t a, uint32_t b, const char *old_a, const char *old_b) {
    cj_doc d = {0};
    if (!cj_parse(&d, text)) {
        cj_free(&d);
        qc_fail(c, "cannot parse a swapped inverse definition");
        return NULL;
    }
    size_t size = strlen(text);
    for (int t = 0; t < d.count; ++t) {
        uint64_t id;
        if (d.tokens[t].kind != '{' || !cj_equal(&d, cj_get(&d, t, "op"), "reg") ||
            !cj_u64(&d, cj_get(&d, t, "id"), &id) || (id != a && id != b))
            continue;
        size = size - (d.tokens[t].end - d.tokens[t].begin) + strlen(id == a ? old_a : old_b);
        if (size > 65536)
            break;
    }
    if (size > 65536) {
        cj_free(&d);
        qc_fail(c, "swapped inverse text budget exceeded");
        return NULL;
    }
    char *out = malloc(size + 1);
    if (!out) {
        cj_free(&d);
        qc_fail(c, "cannot allocate swapped inverse");
        return NULL;
    }
    size_t copied = 0, used = 0;
    for (int t = 0; t < d.count; ++t) {
        uint64_t id;
        if (d.tokens[t].kind != '{' || !cj_equal(&d, cj_get(&d, t, "op"), "reg") ||
            !cj_u64(&d, cj_get(&d, t, "id"), &id) || (id != a && id != b))
            continue;
        size_t begin = d.tokens[t].begin, end = d.tokens[t].end;
        memcpy(out + used, text + copied, begin - copied);
        used += begin - copied;
        const char *value = id == a ? old_a : old_b;
        memcpy(out + used, value, strlen(value));
        used += strlen(value);
        copied = end;
    }
    strcpy(out + used, text + copied);
    cj_free(&d);
    return out;
}

static int
swap_add_dependency(qc_context *c, swap_inverse *p, uint32_t id, uint32_t a, uint32_t b) {
    for (unsigned i = 0; i < p->ndeps; ++i)
        if (p->deps[i] == id)
            return 1;
    qc_register *r = reg(c, id);
    if (!r || !r->alive || !r->prepared || id == p->id || p->ndeps == QC_MAX_SOURCES)
        return qc_fail(c, "swapped inverse has unavailable, self or excessive dependencies");
    unsigned i = p->ndeps++;
    p->deps[i] = id;
    p->epochs[i] = r->epoch + (id == a || id == b);
    return 1;
}

static const swap_inverse *swap_plan(const swap_inverse *plans, unsigned count, uint32_t id) {
    for (unsigned i = 0; i < count; ++i)
        if (plans[i].id == id)
            return &plans[i];
    return NULL;
}

static int swap_acyclic(qc_context *c,
                        const swap_inverse *plans,
                        unsigned count,
                        uint32_t id,
                        uint32_t *path,
                        unsigned depth,
                        unsigned *work) {
    if (depth == QC_MAX_PHYSICAL || ++*work > 2000000)
        return qc_fail(c, "swapped inverse dependency proof budget exceeded");
    for (unsigned i = 0; i < depth; ++i)
        if (path[i] == id)
            return qc_fail(c, "swapped inverse would create a dependency cycle");
    qc_register *r = reg(c, id);
    if (!r || !r->alive)
        return qc_fail(c, "swapped inverse ancestor is unavailable");
    if (!r->definition)
        return 1;
    const swap_inverse *p = swap_plan(plans, count, id);
    path[depth] = id;
    unsigned n = p ? p->ndeps : r->ndeps;
    for (unsigned i = 0; i < n; ++i) {
        uint32_t source = p ? p->deps[i] : r->deps[i];
        if (!p) {
            qc_register *input = reg(c, source);
            if (!input || !input->alive || input->epoch != r->epochs[i])
                return qc_fail(c, "swapped inverse ancestor epochs changed");
        }
        if (!swap_acyclic(c, plans, count, source, path, depth + 1, work))
            return 0;
    }
    return 1;
}

static void swap_free_inverses(swap_inverse *plans, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        free(plans[i].definition);
    free(plans);
}

static int swap_prepare_inverses(qc_context *c,
                                 const cj_doc *d,
                                 int inst,
                                 uint32_t a,
                                 uint32_t b,
                                 swap_inverse **result,
                                 unsigned *count) {
    *result = NULL;
    *count = 0;
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (!r->alive || !r->definition)
            continue;
        for (unsigned j = 0; j < r->ndeps; ++j)
            if (r->deps[j] == a || r->deps[j] == b) {
                ++*count;
                break;
            }
    }
    if (!*count)
        return 1;
    if (*count > QC_MAX_PHYSICAL)
        return qc_fail(c, "too many swapped inverse owners");
    swap_inverse *plans = calloc(*count, sizeof *plans);
    char *old_a = swap_old_value(c, d, inst, a, b), *old_b = swap_old_value(c, d, inst, b, a);
    if (!plans || !old_a || !old_b) {
        free(plans);
        free(old_a);
        free(old_b);
        return *c->error ? 0 : qc_fail(c, "cannot allocate swapped inverse plans");
    }
    unsigned n = 0;
    qc_mask exchanged = 0;
    for (unsigned bit = 0; bit < reg(c, a)->width; ++bit)
        exchanged |= QC_WIRE(reg(c, a)->wires[bit]) | QC_WIRE(reg(c, b)->wires[bit]);
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (!r->alive || !r->definition)
            continue;
        int affected = 0;
        for (unsigned j = 0; j < r->ndeps; ++j)
            affected |= r->deps[j] == a || r->deps[j] == b;
        if (!affected)
            continue;
        swap_inverse *p = &plans[n++];
        p->id = r->id;
        p->mask = r->definition_mask;
        p->pattern = r->definition_pattern;
        for (unsigned j = 0; j < r->ndeps; ++j) {
            qc_register *input = reg(c, r->deps[j]);
            if (!input || !input->alive || input->epoch != r->epochs[j]) {
                qc_fail(c, "cannot transport an inverse with changed input epochs");
                goto failed;
            }
        }
        p->definition = swap_definition(c, r->definition, a, b, old_a, old_b);
        if (!p->definition)
            goto failed;
        if (p->mask & exchanged) {
            char *zero =
                heap_memory_text(c,
                                 "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":0}",
                                 r->width,
                                 r->signed_type ? "true" : "false");
            if (!zero)
                goto failed;
            for (unsigned wire = 0; wire < QC_MAX_PHYSICAL; ++wire) {
                qc_mask bit = QC_WIRE(wire);
                if (!(p->mask & bit))
                    continue;
                qc_register *guard = NULL;
                for (size_t k = 0; k < c->nregisters; ++k) {
                    qc_register *candidate = &c->registers[k];
                    if (candidate->alive && candidate->width == 1 && candidate->wires[0] == wire) {
                        guard = candidate;
                        break;
                    }
                }
                if (!guard) {
                    free(zero);
                    qc_fail(c, "swapped inverse control owner is unavailable");
                    goto failed;
                }
                char *leaf = guard->id == a   ? strdup(old_a)
                             : guard->id == b ? strdup(old_b)
                                              : heap_memory_register(c, guard->id);
                char *next = leaf ? swap_select(c,
                                                r->width,
                                                r->signed_type,
                                                leaf,
                                                p->pattern & bit ? p->definition : zero,
                                                p->pattern & bit ? zero : p->definition)
                                  : NULL;
                free(leaf);
                free(p->definition);
                p->definition = next;
                if (!next) {
                    free(zero);
                    goto failed;
                }
            }
            free(zero);
            p->mask = p->pattern = 0;
        }
        cj_doc expression = {0};
        domain inputs;
        int valid =
            cj_parse(&expression, p->definition) && collect_domain(c, &expression, 0, &inputs);
        cj_free(&expression);
        if (!valid)
            goto failed;
        for (unsigned j = 0; j < inputs.count; ++j)
            if (!swap_add_dependency(c, p, inputs.sources[j].id, a, b))
                goto failed;
        // Original external controls remain part of the inverse certificate.
        for (unsigned j = 0; j < r->ndeps; ++j)
            if (!swap_add_dependency(c, p, r->deps[j], a, b))
                goto failed;
    }
    uint32_t path[QC_MAX_PHYSICAL];
    unsigned work = 0;
    for (unsigned i = 0; i < n; ++i)
        if (!swap_acyclic(c, plans, n, plans[i].id, path, 0, &work))
            goto failed;
    free(old_a);
    free(old_b);
    *result = plans;
    return 1;
failed:
    free(old_a);
    free(old_b);
    swap_free_inverses(plans, *count);
    return *c->error ? 0 : qc_fail(c, "cannot construct a swapped inverse");
}

static void swap_support_bounds(const qc_register *r, uint64_t *lower, uint64_t *upper) {
    *lower = r->basis_known ? r->basis_value : r->support_interval ? r->support_lower : 0;
    *upper = r->basis_known        ? r->basis_value
             : r->support_interval ? r->support_upper
                                   : bits_mask(r->width) & ~r->zero_mask;
}

static int apply_swap(qc_context *c, const cj_doc *d, int inst, qc_mask mask, qc_mask pattern) {
    uint64_t left_id, right_id;
    cj_u64(d, cj_get(d, inst, "id"), &left_id);
    cj_u64(d, cj_get(d, inst, "other"), &right_id);
    uint32_t left = (uint32_t)left_id, right = (uint32_t)right_id;
    uint64_t selector_id;
    qc_register *selector = cj_u64(d, cj_get(d, inst, "mask_register"), &selector_id)
                                ? reg(c, (uint32_t)selector_id)
                                : NULL;
    int inactive;
    if (!phase_inactive(c, d, inst, &inactive))
        return 0;
    if (inactive)
        return 1;
    if (selector && selector->basis_known && !selector->basis_value)
        return 1;
    int rebind = cj_bool(d, cj_get(d, inst, "rebind"));
    if (!rebind && (!unborrowed(c, left) || !unborrowed(c, right)))
        return 0;
    swap_inverse *plans = NULL;
    unsigned count = 0;
    if (rebind && !swap_prepare_inverses(c, d, inst, left, right, &plans, &count))
        return 0;
    qc_register *a = reg(c, left), *b = reg(c, right);
    uint64_t lower_a, upper_a, lower_b, upper_b;
    swap_support_bounds(a, &lower_a, &upper_a);
    swap_support_bounds(b, &lower_b, &upper_b);
    uint64_t selected = bits_mask(a->width);
    cj_u64(d, cj_get(d, inst, "bit_mask"), &selected);
    uint64_t zeros[2] = {(a->zero_mask & ~selected) | (a->zero_mask & b->zero_mask & selected),
                         (b->zero_mask & ~selected) | (a->zero_mask & b->zero_mask & selected)};
    uint64_t lower[2], upper[2];
    if (!selector && selected == bits_mask(a->width)) {
        lower[0] = lower[1] = lower_a < lower_b ? lower_a : lower_b;
        upper[0] = upper[1] = upper_a > upper_b ? upper_a : upper_b;
    } else {
        uint64_t common =
            a->basis_known && b->basis_known ? a->basis_value & b->basis_value & selected : 0;
        lower[0] = (a->basis_known ? a->basis_value & ~selected : 0) | common;
        lower[1] = (b->basis_known ? b->basis_value & ~selected : 0) | common;
        upper[0] = bits_mask(a->width) & ~zeros[0];
        upper[1] = bits_mask(b->width) & ~zeros[1];
    }
    if (!c->structural)
        for (unsigned bit = 0; bit < a->width; ++bit) {
            if (!(selected & (UINT64_C(1) << bit)))
                continue;
            qc_mask x = QC_WIRE(a->wires[bit]), y = QC_WIRE(b->wires[bit]);
            qc_mask active = mask, value = pattern;
            if (selector) {
                qc_mask wire = QC_WIRE(selector->wires[bit]);
                // A contradictory Boolean outer guard makes this bit inactive.
                if ((active & wire) && !(value & wire))
                    continue;
                active |= wire;
                value |= wire;
            }
            if (!qc_emit(c, QC_X, b->wires[bit], active | x, value | x, 0) ||
                !qc_emit(c, QC_X, a->wires[bit], active | y, value | y, 0) ||
                !qc_emit(c, QC_X, b->wires[bit], active | x, value | x, 0)) {
                swap_free_inverses(plans, count);
                return 0;
            }
        }
    taint_controls(c, mask);
    if (selector) {
        qc_mask seen = 0;
        taint_uniform(c, selector->id, &seen);
    }
    unknown_support(a);
    unknown_support(b);
    for (unsigned i = 0; i < 2; ++i) {
        qc_register *r = i ? b : a;
        r->zero_mask = zeros[i];
        // This certificate describes nonnegative numbers, not an unsigned
        // interval of encoded negative bit patterns.
        r->support_interval = r->signed_type == 0 || upper[i] <= bits_mask(r->width - 1);
        r->support_lower = lower[i];
        r->support_upper = upper[i];
        r->basis_known = lower[i] == upper[i];
        r->basis_value = r->basis_known ? lower[i] : 0;
        r->clean = r->basis_known && !lower[i];
        ++r->epoch;
    }
    for (unsigned i = 0; i < count; ++i) {
        swap_inverse *p = &plans[i];
        qc_register *r = reg(c, p->id);
        free(r->definition);
        r->definition = p->definition;
        p->definition = NULL;
        r->definition_mask = p->mask;
        r->definition_pattern = p->pattern;
        r->ndeps = p->ndeps;
        memcpy(r->deps, p->deps, p->ndeps * sizeof *p->deps);
        memcpy(r->epochs, p->epochs, p->ndeps * sizeof *p->epochs);
    }
    swap_free_inverses(plans, count);
    return 1;
}
