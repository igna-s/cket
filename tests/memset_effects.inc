/* Independent finite C value/phase model for sequenced value arguments.
 * All retained inputs and current outputs are checked before inversion.
 * Intermediate history may be live there; saved inverses must restore the
 * original state and make every private wire zero. */
static size_t memset_effect_bits(const qc_register *r, uint64_t value) {
    if (!r || !r->alive)
        validation_fail("missing live memset value-effect register");
    size_t result = 0;
    for (unsigned i = 0; i < r->width; ++i)
        result |= (size_t)((value >> i) & 1) << r->wires[i];
    return result;
}

static unsigned memset_effect_value(size_t basis, const qc_register *r) {
    if (!r || !r->alive || r->width > 8)
        validation_fail("invalid memset value-effect output register");
    unsigned value = 0;
    for (unsigned i = 0; i < r->width; ++i)
        value |= ((basis >> r->wires[i]) & 1) << i;
    return value;
}

static void memset_effect_reference(
    unsigned profile, unsigned input, unsigned flag, unsigned *updated, uint8_t bytes[2]) {
    int b = input, fill = 0;
    bytes[0] = profile == 5 ? 17 : 5;
    bytes[1] = 34;
    switch (profile) {
    case 0: // b++ returns the old value; conversion to _Bool saturates the update.
        fill = b;
        b = (_Bool)(b + 1);
        break;
    case 1:
        b = (_Bool)(b + 1);
        fill = b;
        break;
    case 2:
    case 5:
        fill = b;
        b = (_Bool)(b - 1);
        break;
    case 3:
        b = fill = 0;
        break;
    case 4:
        b = (_Bool)(b + 1);
        break;
    case 6:
        if (flag) {
            fill = b;
            b = (_Bool)(b - 1);
        }
        break;
    case 7:
        fill = b;
        b = (_Bool)(flag ? b + 1 : b - 1);
        break;
    default:
        validation_fail("invalid memset value-effect profile");
    }
    if (profile != 6 || flag)
        memset(bytes, fill, profile == 4 ? 0 : profile == 5 ? 2 : 1);
    *updated = (unsigned)b;
}

static void
memset_effect_compare(qc_context *c, unsigned profile, const unsigned observed[4], int inverse) {
    unsigned controlled = profile >= 6, count = controlled ? 4 : 2, seen = 0;
    qc_register *old = qc_lookup(c, 0);
    qc_register *flag = controlled ? qc_lookup(c, 1) : NULL;
    unsigned target = controlled ? 2 : 1;
    qc_register *first = qc_lookup(c, inverse ? target : observed[0]);
    qc_register *second = profile == 5 ? qc_lookup(c, inverse ? target + 1 : observed[1]) : NULL;
    qc_register *current = inverse ? old : qc_lookup(c, observed[profile == 5 ? 2 : 1]);
    if (!old || !old->alive || old->width != 1 || (!inverse && !old->history) || !first ||
        !current || (controlled && !flag))
        validation_fail("memset value-effect input/history invariant failed");
    for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
        double complex amplitude = qc_amplitude(c, basis);
        double re = creal(amplitude), im = cimag(amplitude);
        if (!isfinite(re) || !isfinite(im))
            validation_fail("nonfinite memset value-effect amplitude");
        if (cabs(amplitude) < 1e-12)
            continue;
        unsigned input = memset_effect_value(basis, old);
        unsigned control = controlled ? memset_effect_value(basis, flag) : 0;
        unsigned label = input | (control << 1), updated;
        uint8_t bytes[2];
        memset_effect_reference(profile, input, control, &updated, bytes);
        double angle = .31 * input + .19 * control + (bytes[0] == 1 ? .17 : 0);
        if (seen & (1u << label) || cabs(amplitude - cexp(I * angle) / sqrt(count)) > 1e-10)
            validation_fail("memset value-effect support, norm or relative phase mismatch");
        seen |= 1u << label;
        if (inverse) {
            size_t wanted =
                memset_effect_bits(old, input) | memset_effect_bits(first, profile == 5 ? 17 : 5);
            if (flag)
                wanted |= memset_effect_bits(flag, control);
            if (second)
                wanted |= memset_effect_bits(second, 34);
            if (basis != wanted)
                validation_fail(
                    "memset value-effect inverse left private garbage or changed an input");
        } else if (memset_effect_value(basis, first) != bytes[0] ||
                   memset_effect_value(basis, current) != updated ||
                   (second && memset_effect_value(basis, second) != bytes[1]))
            validation_fail("memset value-effect outputs disagree with the finite C model");
    }
    if (seen != (1u << count) - 1 || c->measure || c->reset)
        validation_fail("incomplete memset value-effect state or implicit observation/reset");
}

static unsigned memset_effect_probe(const char *ir, unsigned profile) {
    cj_doc d = {0};
    if (!cj_parse(&d, ir))
        validation_fail("invalid memset value-effect IR");
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(1, 7);
        if (!c || !qc_verify_document(c, &d))
            validation_fail("memset value-effect IR verification failed: %s",
                            c ? qc_error(c) : "allocation");
        qc_allow_tables(c, tables);
        int list = cj_get(&d, 0, "instructions"), observing = 0;
        unsigned observed[4], count = 0, nwrite = 0;
        char *writes[16];
        for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next) {
            int op = cj_get(&d, token, "op");
            if (cj_equal(&d, op, "qmeasure")) {
                uint64_t id;
                if (count == 4 || !cj_u64(&d, cj_get(&d, token, "id"), &id) || id > UINT32_MAX)
                    validation_fail("invalid memset value-effect observation mapping");
                observed[count++] = (unsigned)id;
                observing = 1;
            }
            if (observing)
                continue; // Compare the complete unitary prefix before terminal observations.
            if (cj_equal(&d, op, "qxor_inplace")) {
                if (nwrite == sizeof writes / sizeof *writes)
                    validation_fail("memset value-effect inverse recording budget exceeded");
                writes[nwrite++] = cj_copy(&d, token);
            }
            uint64_t ignored;
            if (!qc_apply_token(c, &d, token, 0, 0, &ignored))
                validation_fail("memset value-effect execution failed: %s", qc_error(c));
        }
        if (count != (profile == 5 || profile >= 6 ? 3u : 2u))
            validation_fail("incomplete memset value-effect observation mapping");
        memset_effect_compare(c, profile, observed, 0);
        while (nwrite) {
            char *operation = writes[--nwrite];
            if (!qc_apply(c, operation, NULL))
                validation_fail("memset value-effect write inverse failed: %s", qc_error(c));
            free(operation);
        }
        // Derived history is allocated after its inputs. Invert all still-live
        // definitions in reverse allocation order; already-cleaned phase and
        // zero-count argument temporaries need no second inverse.
        for (size_t i = c->nregisters; i; --i) {
            qc_register *r = &c->registers[i - 1];
            if (!r->alive || !r->definition)
                continue;
            unsigned id = r->id;
            size_t size = strlen(r->definition) + 128;
            char *operation = malloc(size);
            if (!operation)
                validation_fail("memset value-effect inverse allocation failed");
            validation_path(operation,
                            size,
                            "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}",
                            id,
                            r->definition);
            if (!qc_apply(c, operation, NULL))
                validation_fail("memset value-effect saved inverse failed: %s", qc_error(c));
            validation_path(operation, size, "{\"op\":\"qrelease\",\"id\":%u}", id);
            if (!qc_apply(c, operation, NULL))
                validation_fail("memset value-effect inverse release failed: %s", qc_error(c));
            free(operation);
        }
        memset_effect_compare(c, profile, observed, 1);
        qc_destroy(c);
    }
    cj_free(&d);
    return 2;
}
