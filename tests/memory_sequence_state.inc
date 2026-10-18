/* Complete basis action after a checked one-H preparation. The reversible
 * suffix contains X/phase gates only. Both labeled inputs and their complex
 * amplitudes are checked before and after the saved result/write inverses. */
static qc_mask sequence_bits(const qc_register *r, unsigned value) {
    if (!r || !r->alive || r->width > 8)
        validation_fail("memory sequence register missing");
    qc_mask result = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        result |= (qc_mask)((value >> bit) & 1) << r->wires[bit];
    return result;
}

static void sequence_reference(unsigned profile, unsigned b, uint8_t *x, uint8_t *y) {
    *x = 5;
    *y = profile == 1 ? 7 : 0;
    switch (profile) {
    case 0:
        *y = (memset(x, (int)b, 1), *x + 1);
        break;
    case 1:
        memset(x, (memset(y, (int)b, 1), *y + 1), 1);
        break;
    case 2:
        *y = b ? (memset(x, 17, 1), *x) : *x;
        break;
    case 3:
        *y = b && (memset(x, 17, 1), *x == 17);
        break;
    case 4:
        *y = b || (memset(x, 17, 1), *x == 17);
        break;
    default:
        validation_fail("invalid memory sequence profile");
    }
}

static void sequence_compare(qc_context *c, unsigned profile, int inverse) {
    qc_register *input = qc_lookup(c, 0), *x = qc_lookup(c, 1);
    qc_register *y = !inverse || profile == 1 ? qc_lookup(c, 2) : NULL;
    if (!input || !input->alive || input->width != 1 || input->wires[0] != 0 || !x ||
        x->width != 8 || ((!inverse || profile == 1) && !y) ||
        (y && (!y->alive || y->width != (profile < 3 ? 8u : 1u))))
        validation_fail("memory sequence source/result invariant failed");
    qc_mask states[2] = {0, 1};
    double complex amplitudes[2] = {1 / sqrt(2.0), 1 / sqrt(2.0)};
    int prepared = 0, suffix = 0;
    for (size_t i = 0; i < c->ngates; ++i) {
        const qc_gate *g = &c->gates[i];
        if (g->kind == QC_H) {
            if (prepared || suffix || g->mask || g->target != 0)
                validation_fail("memory sequence requires canonical one-H preparation");
            prepared = 1;
            continue;
        }
        if (!prepared || (g->kind != QC_X && g->kind != QC_PHASE))
            validation_fail("unexpected memory sequence preparation, suffix or channel");
        suffix = 1;
        double complex phase = cexp(I * g->angle);
        for (unsigned b = 0; b < 2; ++b)
            if ((states[b] & g->mask) == g->pattern) {
                if (g->kind == QC_X)
                    states[b] ^= QC_WIRE(g->target);
                else
                    amplitudes[b] *= phase;
            }
    }
    if (!prepared || c->measure || c->reset)
        validation_fail("missing memory sequence preparation or implicit channel");
    for (unsigned b = 0; b < 2; ++b) {
        uint8_t first, second;
        sequence_reference(profile, b, &first, &second);
        unsigned tagged = profile == 1   ? first == 2
                          : profile < 2  ? second == 2
                          : profile == 2 ? second == 17
                                         : !!second;
        double angle = .31 * b + .17 * tagged;
        qc_mask wanted = sequence_bits(input, b) | sequence_bits(x, inverse ? 5 : first);
        if (y)
            wanted |= sequence_bits(y, inverse ? 7 : second);
        unsigned guards = 0;
        for (size_t i = 0; i < c->nregisters; ++i) {
            const qc_register *r = &c->registers[i];
            if (!r->alive || r->id < 3)
                continue;
            if (inverse || profile < 2 || ++guards > 1 || r->width != 1 || !r->ancillary ||
                !r->definition)
                validation_fail("unexpected live memory sequence auxiliary");
            wanted |= sequence_bits(r, b);
        }
        if (states[b] != wanted || cabs(amplitudes[b] - cexp(I * angle) / sqrt(2.0)) > 1e-10)
            validation_fail("memory sequence input/result/phase/private-wire mismatch");
    }
}

static void sequence_uncompute(qc_context *c, unsigned id) {
    qc_register *r = qc_lookup(c, id);
    if (!r || !r->alive || !r->definition)
        validation_fail("memory sequence result has no saved inverse");
    size_t capacity = strlen(r->definition) + 128;
    char *operation = malloc(capacity);
    if (!operation)
        validation_fail("memory sequence inverse allocation failed");
    validation_path(
        operation, capacity, "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}", id, r->definition);
    if (!qc_apply(c, operation, NULL))
        validation_fail("memory sequence inverse: %s", qc_error(c));
    validation_path(operation, capacity, "{\"op\":\"qrelease\",\"id\":%u}", id);
    if (!qc_apply(c, operation, NULL))
        validation_fail("memory sequence inverse release: %s", qc_error(c));
    free(operation);
}

static unsigned sequence_state_probe(const char *ir, unsigned profile, unsigned *maximum) {
    cj_doc d = {0};
    if (!cj_parse(&d, ir))
        validation_fail("invalid memory sequence IR");
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(0, 7);
        if (!c || !qc_verify_document(c, &d))
            validation_fail("memory sequence verification failed: %s",
                            c ? qc_error(c) : "allocation");
        qc_allow_tables(c, tables);
        int list = cj_get(&d, 0, "instructions");
        char *writes[16];
        unsigned nwrite = 0, current = UINT32_MAX;
        for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next) {
            int op = cj_get(&d, token, "op");
            uint64_t id, ignored;
            if (cj_equal(&d, op, "qmeasure")) {
                if (!cj_u64(&d, cj_get(&d, token, "id"), &id) || id > UINT32_MAX)
                    validation_fail("invalid memory sequence observation");
                current = (unsigned)id;
                break;
            }
            if (cj_equal(&d, op, "qxor_inplace")) {
                if (nwrite == sizeof writes / sizeof *writes)
                    validation_fail("memory sequence write recording budget exceeded");
                writes[nwrite++] = cj_copy(&d, token);
            }
            if (!qc_apply_token(c, &d, token, 0, 0, &ignored))
                validation_fail("memory sequence prefix failed: %s", qc_error(c));
        }
        if (current != (profile == 1 ? 1u : 2u))
            validation_fail("memory sequence result mapping changed");
        sequence_compare(c, profile, 0);
        // Consumers of the written bytes must be inverted before those writes;
        // their controlling snapshots remain live until the writes are undone.
        if (profile != 1)
            sequence_uncompute(c, current);
        while (nwrite) {
            char *operation = writes[--nwrite];
            if (!qc_apply(c, operation, NULL))
                validation_fail("memory sequence write inverse failed: %s", qc_error(c));
            free(operation);
        }
        for (size_t i = c->nregisters; i; --i) {
            qc_register *r = &c->registers[i - 1];
            if (r->alive && r->definition)
                sequence_uncompute(c, r->id);
        }
        sequence_compare(c, profile, 1);
        if (c->physical > *maximum)
            *maximum = c->physical;
        qc_destroy(c);
    }
    cj_free(&d);
    return 2;
}
