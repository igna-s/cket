/* Independently compiled unitary modules. Parameter wires are remapped at
 * invocation; private wires start and finish at zero. */
struct qc_circuit {
    qc_gate *gates;
    size_t count, capacity;
    unsigned physical, nparams;
    qc_register params[QC_MAX_PHYSICAL];
    unsigned mutable[QC_MAX_PHYSICAL];
};

void qc_circuit_destroy(qc_circuit *f) {
    if (f) {
        free(f->gates);
        free(f);
    }
}

static qc_circuit *
compile_circuit(const char *ir, char *error, size_t size, int allow_tables, unsigned depth) {
    if (depth > 16) {
        if (error && size)
            snprintf(error, size, "circuit module nesting budget exceeded");
        return NULL;
    }
    qc_context *c = qc_create(0, 7);
    qc_circuit *f = calloc(1, sizeof *f);
    cj_doc d = {0};
    if (c) {
        qc_allow_tables(c, allow_tables);
        c->module_depth = depth;
    }
    int ok = c && f && cj_parse(&d, ir);
    if (ok)
        ok = qc_verify_document(c, &d);
    if (ok) {
        int list = cj_get(&d, 0, "instructions");
        int parameters_closed = 0;
        for (int t = list + 1; t < d.tokens[list].next && ok; t = d.tokens[t].next) {
            int op = cj_get(&d, t, "op");
            uint64_t result;
            if (cj_equal(&d, op, "qborrow")) {
                if (parameters_closed) {
                    ok = qc_fail(c, "circuit ABI parameters must precede every body operation");
                    break;
                }
            } else
                parameters_closed = 1;
            if (cj_equal(&d, op, "qmeasure") || cj_equal(&d, op, "qobserve") ||
                cj_equal(&d, op, "qreset") || cj_equal(&d, op, "qfind") ||
                cj_equal(&d, op, "qsearch") || cj_equal(&d, op, "qindexed") ||
                cj_equal(&d, op, "qphi") || cj_equal(&d, op, "qdispose") ||
                cj_equal(&d, op, "qassign") || cj_equal(&d, op, "qslot") ||
                cj_equal(&d, op, "cmemory") || cj_equal(&d, op, "cloop_begin") ||
                cj_equal(&d, op, "cloop_end") || cj_equal(&d, op, "ccontrol_begin") ||
                cj_equal(&d, op, "ccontrol_end") || cj_equal(&d, op, "ccontrol_else") ||
                cj_equal(&d, op, "ctype_require") || cj_equal(&d, op, "ctype_capture") ||
                cj_bool(&d, cj_get(&d, t, "dynamic"))) {
                ok = qc_fail(
                    c,
                    "compiled unitary modules cannot measure, reset, search, execute CPU feedback or require a CPU locale precondition");
                break;
            }
            ok = qc_apply_token(c, &d, t, 0, 0, &result);
            if (ok && cj_equal(&d, op, "qborrow")) {
                uint64_t id;
                cj_u64(&d, cj_get(&d, t, "id"), &id);
                if (f->nparams == QC_MAX_PHYSICAL) {
                    ok = qc_fail(c, "circuit ABI parameter budget exceeded");
                    break;
                }
                f->params[f->nparams++] = *reg(c, (uint32_t)id);
            }
        }
    }
    if (ok)
        for (size_t j = 0; j < c->nregisters && ok; ++j)
            if (c->registers[j].alive) {
                int parameter = 0;
                for (unsigned k = 0; k < f->nparams; ++k)
                    parameter |= f->params[k].id == c->registers[j].id;
                if (!parameter || c->registers[j].definition)
                    ok = qc_fail(
                        c,
                        "circuit module must release all private storage and pending inverse certificates");
            }
    if (ok) {
        f->physical = c->physical;
        f->count = c->ngates;
        f->capacity = c->gate_capacity;
        f->gates = c->gates;
        c->gates = NULL;
        for (size_t j = 0; j < f->count; ++j)
            if (f->gates[j].kind == QC_X || f->gates[j].kind == QC_H)
                for (unsigned k = 0; k < f->nparams; ++k)
                    for (unsigned b = 0; b < f->params[k].width; ++b)
                        if (f->params[k].wires[b] == f->gates[j].target)
                            f->mutable[k] = 1;
    }
    if (!ok && error && size)
        snprintf(error,
                 size,
                 "%s",
                 c && *qc_error(c) ? qc_error(c)
                 : *d.error        ? d.error
                                   : "out of memory compiling circuit");
    cj_free(&d);
    qc_destroy(c);
    if (!ok) {
        qc_circuit_destroy(f);
        return NULL;
    }
    return f;
}

qc_circuit *qc_circuit_compile(const char *ir, char *error, size_t size) {
    return compile_circuit(ir, error, size, 1, 0);
}

static qc_mask circuit_remap(qc_mask mask, const unsigned *map, unsigned physical) {
    qc_mask result = 0;
    for (unsigned j = 0; j < physical; ++j)
        if (mask & QC_WIRE(j))
            result |= QC_WIRE(map[j]);
    return result;
}

int qc_circuit_apply(qc_context *c,
                     const qc_circuit *f,
                     const uint32_t *bindings,
                     size_t count,
                     int inverse,
                     const uint32_t *control_ids,
                     const unsigned *values,
                     size_t ncontrols) {
    if (!f || count != f->nparams || (count && !bindings) ||
        (ncontrols && (!control_ids || !values)) || ncontrols > QC_MAX_PHYSICAL)
        return qc_fail(c, "invalid circuit ABI arguments");
    unsigned map[QC_MAX_PHYSICAL];
    for (unsigned j = 0; j < f->physical; ++j)
        map[j] = QC_MAX_PHYSICAL;
    qc_mask occupied = 0, outer = 0, pattern = 0;
    for (size_t j = 0; j < count; ++j) {
        qc_register *r = reg(c, bindings[j]);
        if (!r || !r->alive || !r->prepared || r->width != f->params[j].width ||
            r->signed_type != f->params[j].signed_type)
            return qc_fail(
                c,
                "circuit ABI requires an available borrowed register of the declared C representation");
        if (f->mutable[j] && (r->definition || !unborrowed(c, r->id)))
            return qc_fail(c, "circuit mutation would invalidate a pending inverse");
        for (unsigned b = 0; b < r->width; ++b) {
            qc_mask wire = QC_WIRE(r->wires[b]);
            if (occupied & wire)
                return qc_fail(c, "circuit ABI register parameters cannot alias");
            occupied |= wire;
            map[f->params[j].wires[b]] = r->wires[b];
        }
    }
    for (size_t j = 0; j < ncontrols; ++j) {
        qc_register *r = reg(c, control_ids[j]);
        if (!r || !r->alive || r->width != 1 || !r->prepared || values[j] > 1)
            return qc_fail(c, "invalid coherent module control");
        qc_mask wire = QC_WIRE(r->wires[0]);
        if (occupied & wire || ((outer & wire) && !!(pattern & wire) != values[j]))
            return qc_fail(c, "module control aliases a parameter or contradicts another control");
        outer |= wire;
        if (values[j])
            pattern |= wire;
    }
    uint32_t scratch[QC_MAX_PHYSICAL];
    unsigned nscratch = 0;
    for (unsigned j = 0; j < f->physical; ++j)
        if (map[j] == QC_MAX_PHYSICAL) {
            uint32_t id = UINT32_MAX;
            while (reg(c, id) && reg(c, id)->alive)
                --id;
            if (!allocate(c, id, 1, 1, 0))
                return 0;
            scratch[nscratch++] = id;
            map[j] = reg(c, id)->wires[0];
        }
    for (size_t j = 0; j < f->count; ++j) {
        qc_gate g = f->gates[inverse ? f->count - 1 - j : j];
        qc_mask mask = circuit_remap(g.mask, map, f->physical) | outer;
        qc_mask state = circuit_remap(g.pattern, map, f->physical) | pattern;
        if (!qc_emit(c,
                     g.kind,
                     g.kind == QC_PHASE ? 0 : map[g.target],
                     mask,
                     state,
                     inverse ? -g.angle : g.angle))
            return 0;
    }
    for (unsigned j = 0; j < nscratch; ++j)
        if (!release(c, reg(c, scratch[j])))
            return 0;
    taint_controls(c, outer);
    for (size_t j = 0; j < count; ++j) {
        qc_mask seen = 0;
        taint_uniform(c, bindings[j], &seen);
        if (f->mutable[j]) {
            qc_register *r = reg(c, bindings[j]);
            unknown_support(r);
            r->clean = 0;
            ++r->epoch;
        }
    }
    return 1;
}

static int apply_module(qc_context *c, const cj_doc *d, int inst) {
    char *text = cj_copy(d, cj_get(d, inst, "circuit"));
    if (!text)
        return qc_fail(c, "out of memory reading embedded module");
    qc_module_cache *module = c->modules;
    while (module && (module->allow_tables != c->allow_tables || strcmp(module->text, text)))
        module = module->next;
    if (!module) {
        char error[512];
        qc_circuit *f =
            compile_circuit(text, error, sizeof error, c->allow_tables, c->module_depth + 1);
        if (!f) {
            free(text);
            return qc_fail(c, "module compilation: %s", error);
        }
        size_t bytes =
            sizeof *f + sizeof(qc_module_cache) + strlen(text) + 1 + f->capacity * sizeof(qc_gate);
        if (c->module_bytes + bytes > 64 * 1024 * 1024) {
            qc_circuit_destroy(f);
            free(text);
            return qc_fail(c, "compiled module budget exceeded");
        }
        module = calloc(1, sizeof *module);
        if (!module) {
            qc_circuit_destroy(f);
            free(text);
            return qc_fail(c, "out of memory caching module");
        }
        module->text = text;
        text = NULL;
        module->circuit = f;
        module->allow_tables = c->allow_tables;
        module->next = c->modules;
        c->modules = module;
        c->module_bytes += bytes;
    }
    free(text);
    uint32_t ids[QC_MAX_PHYSICAL], controls[QC_MAX_PHYSICAL];
    unsigned values[QC_MAX_PHYSICAL], nids = 0, ncontrols = 0;
    int list = cj_get(d, inst, "ids");
    for (int t = list + 1; t < d->tokens[list].next; t = d->tokens[t].next) {
        uint64_t id;
        cj_u64(d, t, &id);
        ids[nids++] = (uint32_t)id;
    }
    list = cj_get(d, inst, "controls");
    if (list >= 0)
        for (int t = list + 1; t < d->tokens[list].next; t = d->tokens[t].next) {
            uint64_t id, value;
            cj_u64(d, cj_get(d, t, "id"), &id);
            cj_u64(d, cj_get(d, t, "value"), &value);
            controls[ncontrols] = (uint32_t)id;
            values[ncontrols++] = (unsigned)value;
        }
    return qc_circuit_apply(c,
                            module->circuit,
                            ids,
                            nids,
                            cj_bool(d, cj_get(d, inst, "inverse")),
                            controls,
                            values,
                            ncontrols);
}
