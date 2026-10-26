/* Complete labeled action on the explicitly prepared input subspaces.
 * Sixteen-H profiles enumerate every word value; one-H profiles enumerate
 * both Boolean labels. The checked reversible suffix contains X/phase gates.
 * These checks do not allocate a dense statevector over all physical wires. */
static qc_mask memory_return_bits(const qc_register *r, unsigned value) {
    if (!r || !r->alive || r->width > 16 || value >= (1u << r->width))
        validation_fail("memory return register/value invariant failed");
    qc_mask result = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        result |= (qc_mask)((value >> bit) & 1) << r->wires[bit];
    return result;
}

/* Whole-word identities need not name separate old/current registers after a
 * masked exchange. Follow every prepared label, check C values and phases,
 * and require exactly the history equivalence induced by the overwritten byte.
 * In particular, retaining an untouched low byte would fail this channel test. */
static void memory_return_word_action(qc_context *c, unsigned profile, unsigned current) {
    unsigned count = profile == 2 ? 65536 : 1, preparation = profile == 2 ? 16 : 0;
    qc_register *input = qc_lookup(c, 0), *output = qc_lookup(c, current);
    if (!input || !output || !output->alive || input->width != 16 || output->width != 16)
        validation_fail("memory return word input/output mapping changed");
    qc_mask *states = calloc(count, sizeof *states);
    double complex *amplitudes = malloc(count * sizeof *amplitudes);
    if (!states || !amplitudes)
        validation_fail("memory return word reference allocation failed");
    for (unsigned label = 0; label < count; ++label)
        amplitudes[label] = 1 / sqrt((double)count);
    unsigned prepared = 0;
    for (size_t i = 0; i < c->ngates; ++i) {
        qc_gate g = c->gates[i];
        if (g.kind == QC_H) {
            if (prepared == preparation || g.mask || g.target != input->wires[prepared] ||
                (states[0] & QC_WIRE(g.target)))
                validation_fail("memory return word requires canonical H preparation");
            for (unsigned label = 0; label < count; ++label)
                if ((label >> prepared) & 1)
                    states[label] |= QC_WIRE(g.target);
            ++prepared;
            continue;
        }
        if ((prepared && prepared != preparation) || (g.kind != QC_X && g.kind != QC_PHASE))
            validation_fail("unexpected gate or channel in memory return word reference");
        double complex phase = cexp(I * g.angle);
        for (unsigned label = 0; label < count; ++label)
            if ((states[label] & g.mask) == g.pattern) {
                if (g.kind == QC_X)
                    states[label] ^= QC_WIRE(g.target);
                else
                    amplitudes[label] *= phase;
            }
    }
    if (prepared != preparation || c->measure || c->reset)
        validation_fail("memory return word preparation or implicit channel mismatch");
    qc_mask public_mask = memory_return_bits(output, UINT16_MAX), live_mask = 0;
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (!r->alive)
            continue;
        if (r->id != current && !r->history)
            validation_fail("memory return word left a nonhistory temporary alive");
        for (unsigned bit = 0; bit < r->width; ++bit)
            live_mask |= QC_WIRE(r->wires[bit]);
    }
    qc_mask history[256] = {0};
    unsigned char seen[256] = {0};
    for (unsigned label = 0; label < count; ++label) {
        uint16_t original = profile == 2 ? (uint16_t)label : 0x1234, value = original;
        unsigned history_class = profile == 2 ? ((unsigned char *)&original)[1] : 0;
        void *pointer = memset((unsigned char *)&value + 1, 0xab, 1);
        if (pointer != (unsigned char *)&value + 1 || memset(pointer, 0xcd, 1) != pointer)
            validation_fail("ordinary C word-return reference failed");
        double angle = profile == 2 ? .00001 * label + (value == 0xcd34 ? .17 : 0) : 0;
        if ((states[label] & public_mask) != memory_return_bits(output, value) ||
            (states[label] & ~live_mask) ||
            cabs(amplitudes[label] - cexp(I * angle) / sqrt((double)count)) > 1e-10)
            validation_fail("memory return word C value/phase/private-zero mismatch label=%u",
                            label);
        qc_mask private = states[label] & ~public_mask;
        if (seen[history_class] && history[history_class] != private)
            validation_fail("memory return word history copied an untouched byte");
        seen[history_class] = 1;
        history[history_class] = private;
    }
    for (unsigned a = 0; a < 256; ++a)
        for (unsigned b = 0; b < a; ++b)
            if (seen[a] && seen[b] && history[a] == history[b])
                validation_fail("memory return word history lost overwritten information");

    /* Independently invert the complete gate stream, including preparation and
     * source phases. This is a gate inverse, not compiler-emitted scope cleanup
     * or a saved qcompute inverse of a materialized patch. */
    unsigned active = count;
    double discarded = 0;
    for (size_t i = c->ngates; i-- > 0;) {
        qc_gate g = c->gates[i];
        if (g.kind == QC_H) {
            if (active < 2 || !prepared || g.target != input->wires[prepared - 1])
                validation_fail("unexpected H in independent word-return inverse");
            unsigned half = active / 2;
            qc_mask bit = QC_WIRE(g.target);
            for (unsigned row = 0; row < half; ++row) {
                if ((states[row] & bit) || states[row + half] != (states[row] | bit))
                    validation_fail("word-return inverse did not recover prepared labels");
                double complex low = amplitudes[row], high = amplitudes[row + half];
                amplitudes[row] = (low + high) / sqrt(2.0);
                double complex residual = (low - high) / sqrt(2.0);
                discarded += creal(residual * conj(residual));
            }
            active = half;
            --prepared;
            continue;
        }
        double complex phase = cexp(-I * g.angle);
        for (unsigned row = 0; row < active; ++row)
            if ((states[row] & g.mask) == g.pattern) {
                if (g.kind == QC_X)
                    states[row] ^= QC_WIRE(g.target);
                else
                    amplitudes[row] *= phase;
            }
    }
    if (active != 1 || prepared || states[0] || cabs(amplitudes[0] - 1) > 1e-10 ||
        discarded > 1e-20)
        validation_fail("independent complete word-return inverse left a nonzero state");
    free(amplitudes);
    free(states);
}

static void
memory_return_saved_action(qc_context *c, unsigned profile, unsigned current, int inverse) {
    if (profile != 1 && profile != 3 && profile != 4)
        validation_fail("unexpected saved-inverse memory return profile");
    unsigned count = profile >= 3 ? 2 : 1;
    unsigned input_id = profile == 3 ? 2 : 0;
    unsigned preparation = profile >= 3 ? 1 : 0;
    qc_register *input = qc_lookup(c, input_id);
    if (!input || !input->alive || (preparation && input->width != preparation))
        validation_fail("memory return input mapping changed");
    qc_mask *states = calloc(count, sizeof *states);
    double complex *amplitudes = malloc(count * sizeof *amplitudes);
    if (!states || !amplitudes)
        validation_fail("memory return labeled state allocation failed");
    for (unsigned label = 0; label < count; ++label)
        amplitudes[label] = 1 / sqrt((double)count);
    unsigned prepared = 0;
    for (size_t i = 0; i < c->ngates; ++i) {
        const qc_gate *gate = &c->gates[i];
        if (gate->kind == QC_H) {
            if (prepared >= preparation || gate->mask || gate->target != input->wires[prepared] ||
                (states[0] & QC_WIRE(gate->target)))
                validation_fail("memory return requires canonical H preparation");
            for (unsigned label = 0; label < count; ++label)
                if ((label >> prepared) & 1)
                    states[label] |= QC_WIRE(gate->target);
            ++prepared;
            continue;
        }
        if ((prepared && prepared != preparation) || (gate->kind != QC_X && gate->kind != QC_PHASE))
            validation_fail("unexpected memory return preparation, suffix or channel");
        double complex phase = cexp(I * gate->angle);
        for (unsigned label = 0; label < count; ++label)
            if ((states[label] & gate->mask) == gate->pattern) {
                if (gate->kind == QC_X)
                    states[label] ^= QC_WIRE(gate->target);
                else
                    amplitudes[label] *= phase;
            }
    }
    if (prepared != preparation || c->measure || c->reset)
        validation_fail("memory return preparation or implicit channel mismatch");
    for (unsigned label = 0; label < count; ++label) {
        unsigned old_bit = profile >= 3 ? label : 0;
        bool updated = (int)old_bit - 1;
        uint8_t bytes[2] = {5, 7}, controlled = 5;
        size_t zero_count = 0;
        if (memset(bytes + 2, (int)old_bit, zero_count) != bytes + 2)
            validation_fail("ordinary C one-past zero-count reference failed");
        if (profile == 4 && old_bit && memset(&controlled, 17, 1) != &controlled)
            validation_fail("ordinary C controlled pointer reference failed");
        double angle = profile == 3   ? .31 * old_bit + .17 * updated
                       : profile == 4 ? .48 * old_bit
                                      : 0;
        qc_mask wanted = 0;
        unsigned live = 0, guards = 0;
        for (size_t i = 0; i < c->nregisters; ++i) {
            const qc_register *r = &c->registers[i];
            if (!r->alive)
                continue;
            ++live;
            unsigned value = 0, width = 0;
            if (profile == 1 || profile == 3) {
                width = r->id < 2 ? 8 : 1;
                if (r->id < 2)
                    value = bytes[r->id];
                else if (r->id == 2)
                    value = old_bit;
                else if (!inverse && r->id == current)
                    value = updated;
                else
                    validation_fail("unexpected memory return zero-count history");
            } else {
                width = r->id == 1 ? 8 : 1;
                if (r->id == 0)
                    value = old_bit;
                else if (r->id == 1)
                    value = inverse ? 5 : controlled;
                else if (!inverse && r->ancillary && r->definition && !guards++)
                    value = old_bit;
                else
                    validation_fail("unexpected memory return control history");
            }
            if (r->width != width)
                validation_fail("memory return register width mismatch");
            wanted |= memory_return_bits(r, value);
        }
        unsigned expected_live = profile == 4 ? 2 + guards : (inverse ? 3 : 4);
        if (live != expected_live || states[label] != wanted ||
            cabs(amplitudes[label] - cexp(I * angle) / sqrt((double)count)) > 1e-10)
            validation_fail(
                "memory return profile=%u inverse=%d label=%u result/phase/history/private-wire mismatch",
                profile,
                inverse,
                label);
    }
    free(amplitudes);
    free(states);
}

typedef struct {
    unsigned id, width;
    char *expression;
} return_definition;

static return_definition return_saved[256];
static unsigned return_nsaved;

static void return_invert_definition(qc_context *c, unsigned id) {
    qc_register *r = qc_lookup(c, id);
    if (!r || !r->alive || !r->definition)
        validation_fail("missing return_saved definition during inversion");
    size_t capacity = strlen(r->definition) + 128;
    char *operation = malloc(capacity);
    if (!operation)
        validation_fail("inverse allocation failed");
    snprintf(
        operation, capacity, "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}", id, r->definition);
    if (!qc_apply(c, operation, NULL))
        validation_fail("%s", qc_error(c));
    snprintf(operation, capacity, "{\"op\":\"qrelease\",\"id\":%u}", id);
    if (!qc_apply(c, operation, NULL))
        validation_fail("%s", qc_error(c));
    free(operation);
}

static int return_restore_guard(qc_context *c, unsigned id) {
    qc_register *r = qc_lookup(c, id);
    if (r && r->alive)
        return 0;
    const return_definition *definition = NULL;
    for (unsigned i = return_nsaved; i;)
        if (return_saved[--i].id == id) {
            definition = &return_saved[i];
            break;
        }
    if (!definition || definition->width != 1)
        validation_fail("released control has no recorded Boolean definition");
    size_t capacity = strlen(definition->expression) + 128;
    char *operation = malloc(capacity);
    if (!operation)
        validation_fail("control reconstruction allocation failed");
    snprintf(operation,
             capacity,
             "{\"op\":\"qalloc\",\"id\":%u,\"width\":1,\"signed\":false,\"ancilla\":true}",
             id);
    if (!qc_apply(c, operation, NULL))
        validation_fail("%s", qc_error(c));
    snprintf(operation,
             capacity,
             "{\"op\":\"qcompute\",\"id\":%u,\"expr\":%s}",
             id,
             definition->expression);
    if (!qc_apply(c, operation, NULL))
        validation_fail("%s", qc_error(c));
    free(operation);
    return 1;
}

static unsigned memory_return_state_probe(const char *ir, unsigned profile, unsigned *maximum) {
    cj_doc d = {0};
    if (!cj_parse(&d, ir))
        validation_fail("invalid memory return IR");
    for (unsigned tables = 0; tables < 2; ++tables) {
        return_nsaved = 0;
        qc_context *c = qc_create(0, 7);
        if (!c || !qc_verify_document(c, &d))
            validation_fail("memory return verification failed: %s",
                            c ? qc_error(c) : "allocation");
        qc_allow_tables(c, tables);
        int list = cj_get(&d, 0, "instructions");
        unsigned current = UINT32_MAX, naction = 0;

        struct {
            unsigned id;
            char *inverse;
        } actions[256];

        for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next) {
            int op = cj_get(&d, token, "op");
            uint64_t value;
            if (cj_equal(&d, op, "qmeasure")) {
                if (!cj_u64(&d, cj_get(&d, token, "id"), &value) || value > UINT32_MAX)
                    validation_fail("invalid observed ID");
                current = (unsigned)value;
                break;
            }
            if (cj_equal(&d, op, "qxor_inplace") || cj_equal(&d, op, "qcompute")) {
                uint64_t id;
                if (naction == sizeof actions / sizeof *actions ||
                    !cj_u64(&d, cj_get(&d, token, "id"), &id) || id > UINT32_MAX)
                    validation_fail("write/definition recording budget or ID");
                actions[naction].id = (unsigned)id;
                actions[naction++].inverse =
                    cj_equal(&d, op, "qxor_inplace") ? cj_copy(&d, token) : NULL;
            }
            if (cj_equal(&d, op, "qadd_inplace")) {
                uint64_t id, raw;
                int controls = cj_get(&d, token, "controls");
                if (naction == sizeof actions / sizeof *actions ||
                    !cj_u64(&d, cj_get(&d, token, "id"), &id) || id > UINT32_MAX ||
                    !cj_u64(&d, cj_get(&d, token, "value"), &raw) ||
                    (raw != 1 && raw != UINT64_MAX) ||
                    (controls >= 0 && d.tokens[controls].children))
                    validation_fail("unsupported argument addition in this finite exporter");
                char operation[160];
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qadd_inplace\",\"id\":%u,\"value\":%d}",
                         (unsigned)id,
                         raw == 1 ? -1 : 1);
                actions[naction].id = (unsigned)id;
                size_t length = strlen(operation) + 1;
                actions[naction].inverse = malloc(length);
                if (!actions[naction].inverse)
                    validation_fail("addition inverse allocation failed");
                memcpy(actions[naction++].inverse, operation, length);
            }
            if (!qc_apply_token(c, &d, token, 0, 0, &value))
                validation_fail("%s", qc_error(c));
            if (cj_equal(&d, op, "qcompute")) {
                uint64_t id;
                if (!cj_u64(&d, cj_get(&d, token, "id"), &id) || id > UINT32_MAX ||
                    return_nsaved == sizeof return_saved / sizeof *return_saved)
                    validation_fail("definition recording budget or ID");
                qc_register *r = qc_lookup(c, (unsigned)id);
                if (!r || !r->definition)
                    validation_fail("computed register has no return_saved definition");
                size_t length = strlen(r->definition) + 1;
                return_saved[return_nsaved].id = (unsigned)id;
                return_saved[return_nsaved].width = r->width;
                return_saved[return_nsaved].expression = malloc(length);
                if (!return_saved[return_nsaved].expression)
                    validation_fail("definition recording allocation failed");
                memcpy(return_saved[return_nsaved++].expression, r->definition, length);
            }
        }
        qc_register *result = qc_lookup(c, current);
        if (!result || !result->alive || c->measure || c->reset)
            validation_fail("missing current value or implicit channel");

        if (profile == 0 || profile == 2) {
            memory_return_word_action(c, profile, current);
            for (unsigned i = 0; i < naction; ++i)
                free(actions[i].inverse);
            if (c->physical > *maximum)
                *maximum = c->physical;
            qc_destroy(c);
            for (unsigned i = 0; i < return_nsaved; ++i)
                free(return_saved[i].expression);
            continue;
        }
        memory_return_saved_action(c, profile, current, 0);
        // Reverse semantic creation/write order: consumers of a known in-place
        // write must be inverted before that write's original basis is restored.
        while (naction) {
            --naction;
            qc_register *r = qc_lookup(c, actions[naction].id);
            if (!r || !r->alive) {
                free(actions[naction].inverse);
                continue;
            }
            if (actions[naction].inverse) {
                // A scope may have uncomputed a decision before this prefix ends.
                // Rebuild it from its actual return_saved definition while undoing its
                // writes, then clean that temporary again. No observation is used.
                cj_doc inverse = {0};
                if (!cj_parse(&inverse, actions[naction].inverse))
                    validation_fail("invalid inverse operation");
                int controls = cj_get(&inverse, 0, "controls");
                unsigned rebuilt[16], nrebuilt = 0;
                if (controls >= 0)
                    for (int token = controls + 1; token < inverse.tokens[controls].next;
                         token = inverse.tokens[token].next) {
                        uint64_t id;
                        if (!cj_u64(&inverse, cj_get(&inverse, token, "id"), &id) ||
                            id > UINT32_MAX)
                            validation_fail("invalid return_saved control");
                        if (return_restore_guard(c, (unsigned)id)) {
                            if (nrebuilt == sizeof rebuilt / sizeof *rebuilt)
                                validation_fail("control reconstruction budget exceeded");
                            rebuilt[nrebuilt++] = (unsigned)id;
                        }
                    }
                if (!qc_apply(c, actions[naction].inverse, NULL))
                    validation_fail("%s", qc_error(c));
                while (nrebuilt)
                    return_invert_definition(c, rebuilt[--nrebuilt]);
                cj_free(&inverse);
                free(actions[naction].inverse);
                continue;
            }
            if (!r->definition)
                continue;
            return_invert_definition(c, r->id);
        }
        for (size_t i = 0; i < c->nregisters; ++i)
            if (c->registers[i].alive && c->registers[i].definition)
                validation_fail("unrecorded live return_saved definition remains");

        memory_return_saved_action(c, profile, current, 1);
        if (c->physical > *maximum)
            *maximum = c->physical;
        qc_destroy(c);
        for (unsigned i = 0; i < return_nsaved; ++i)
            free(return_saved[i].expression);
    }
    cj_free(&d);
    return 2;
}
