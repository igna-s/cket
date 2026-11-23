/* Complete finite sparse states: all byte labels and all selected shift
 * positions, including phased wide words. The source domain is stated below;
 * these are not all 2^32/2^64 input values or dense wide-state simulations. */
static uint64_t shift_read(qc_mask basis, const qc_register *r) {
    uint64_t result = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        if (qc_mask_test(basis, r->wires[bit]))
            result |= UINT64_C(1) << bit;
    return result;
}

static void shift_sparse_state(qc_context *c,
                               unsigned width,
                               unsigned count_bits,
                               unsigned output_bits,
                               uint64_t base,
                               int left,
                               int sign,
                               unsigned active,
                               int inverse) {
    qc_register *input = qc_lookup(c, 0), *count = qc_lookup(c, 1), *output = qc_lookup(c, 2),
                *control = qc_lookup(c, 3);
    unsigned positions = 1u << count_bits;
    size_t expected_states = (size_t)256 * positions * 2;
    unsigned char *seen = calloc(expected_states, 1);
    if (!seen || qc_sparse_size(c) != expected_states)
        validation_fail("shift sparse support size");
    qc_mask allowed = qc_mask_zero();
    for (unsigned id = 0; id < 4; ++id) {
        qc_register *r = qc_lookup(c, id);
        for (unsigned bit = 0; bit < r->width; ++bit)
            allowed = qc_mask_or(allowed, QC_WIRE(r->wires[bit]));
    }
    for (size_t row = 0; row < expected_states; ++row) {
        uint64_t low, high;
        double complex amplitude;
        if (!qc_sparse_entry(c, row, &low, &high, &amplitude))
            validation_fail("shift sparse enumeration");
        qc_mask basis = qc_mask_from_u128(low, high);
        uint64_t raw = shift_read(basis, input), distance = shift_read(basis, count);
        unsigned flag = (unsigned)shift_read(basis, control);
        uint64_t byte = raw ^ base;
        if (byte > 255 || distance >= positions || qc_mask_any(qc_mask_andnot(basis, allowed)))
            validation_fail("shift changed an input or left private garbage");
        uint64_t shifted = left ? raw << distance : raw >> distance;
        if (!left && sign && distance && (raw & (UINT64_C(1) << (width - 1)))) {
            uint64_t mask = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
            shifted |= mask ^ ((UINT64_C(1) << (width - distance)) - 1);
        }
        if (flag != active)
            shifted = 0;
        shifted &= output_bits == 64 ? UINT64_MAX : (UINT64_C(1) << output_bits) - 1;
        if (shift_read(basis, output) != (inverse ? 0 : shifted))
            validation_fail("shift sparse numeric result width=%u distance=%llu",
                            width,
                            (unsigned long long)distance);
        double angle = .003 * byte + .007 * distance + .011 * flag;
        if (inverse && (shifted & 1))
            angle += .37;
        double complex expected = cexp(I * angle) / sqrt((double)expected_states);
        if (cabs(amplitude - expected) > 2e-11)
            validation_fail("shift sparse complex phase mismatch");
        size_t label = (size_t)(byte * positions * 2 + distance * 2 + flag);
        if (seen[label])
            validation_fail("duplicate joint shift state");
        seen[label] = 1;
    }
    free(seen);
}

static void wide_sparse_states(void) {
    const struct {
        unsigned width, output, count_bits;
        int left, sign;
    } profiles[] = {{8, 8, 3, 1, 0},
                    {8, 8, 3, 0, 0},
                    {8, 8, 3, 0, 1},
                    {32, 32, 5, 1, 0},
                    {32, 32, 5, 0, 0},
                    {32, 32, 5, 0, 1},
                    {64, 8, 6, 0, 0},
                    {64, 8, 6, 0, 1}};

    for (unsigned profile = 0; profile < sizeof profiles / sizeof *profiles; ++profile)
        for (unsigned tables = 0; tables < 2; ++tables) {
            unsigned width = profiles[profile].width, output = profiles[profile].output,
                     count_bits = profiles[profile].count_bits, active = profile & 1;
            int left = profiles[profile].left, sign = profiles[profile].sign;
            qc_context *c = qc_create_sparse(7, 65536);
            if (!c)
                validation_fail("sparse shift context");
            qc_allow_tables(c, tables);
            char operation[2200], expression[1800], leaf[350], input[600];
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",
                     width);
            apply(c, operation);
            apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":255}");
            uint64_t base = width == 8 ? 0 : (UINT64_C(1) << (width - 1));
            if (base) {
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qxor_inplace\",\"id\":0,\"value\":%llu}",
                         (unsigned long long)base);
                apply(c, operation);
            }
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":1,\"width\":%u,\"signed\":false}",
                     count_bits);
            apply(c, operation);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":%u}",
                     (1u << count_bits) - 1);
            apply(c, operation);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":2,\"width\":%u,\"signed\":false}",
                     output);
            apply(c, operation);
            apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":3,\"lower\":0,\"upper\":1}");
            for (unsigned id = 0; id < 4; ++id) {
                if (id == 2)
                    continue;
                qc_register *r = qc_lookup(c, id);
                unsigned bits = id == 0 ? 8 : r->width;
                for (unsigned bit = 0; bit < bits; ++bit) {
                    double angle = (id == 0 ? .003 : id == 1 ? .007 : .011) * (1u << bit);
                    if (!qc_emit(
                            c, QC_PHASE, 0, QC_WIRE(r->wires[bit]), QC_WIRE(r->wires[bit]), angle))
                        validation_fail("shift input phase");
                }
            }
            snprintf(
                leaf, sizeof leaf, "{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":false}", width);
            if (sign)
                snprintf(input,
                         sizeof input,
                         "{\"op\":\"cast\",\"bits\":%u,\"signed\":true,\"args\":[%s]}",
                         width,
                         leaf);
            else
                snprintf(input, sizeof input, "%s", leaf);
            unsigned promoted = width < 32 ? 32 : width;
            snprintf(
                expression,
                sizeof expression,
                "{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[%s]},{\"op\":\"reg\",\"id\":1,\"bits\":%u,\"signed\":false}]}]}",
                output,
                left ? "shl" : "shr",
                promoted,
                (sign || width < 32) ? "true" : "false",
                promoted,
                (sign || width < 32) ? "true" : "false",
                input,
                count_bits);
            snprintf(
                operation,
                sizeof operation,
                "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s,\"controls\":[{\"id\":3,\"value\":%u}]}",
                expression,
                active);
            apply(c, operation);
            if (qc_qubits(c) > width + count_bits + 2 * output + 1)
                validation_fail("shift allocated stage-sized temporary registers");
            shift_sparse_state(c, width, count_bits, output, base, left, sign, active, 0);
            qc_register *result = qc_lookup(c, 2);
            if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(result->wires[0]), QC_WIRE(result->wires[0]), .37))
                validation_fail("shift result phase");
            snprintf(
                operation,
                sizeof operation,
                "{\"op\":\"quncompute\",\"id\":2,\"expr\":%s,\"controls\":[{\"id\":3,\"value\":%u}]}",
                expression,
                active);
            apply(c, operation);
            shift_sparse_state(c, width, count_bits, output, base, left, sign, active, 1);
            if (c->measure || c->reset)
                validation_fail("shift introduced a hidden channel");
            apply(c, "{\"op\":\"qrelease\",\"id\":2}");
            qc_destroy(c);
            ++checks;
        }
    puts(
        "PASS 16 sparse joint-state shift domains: all bytes/counts, selected wide words, signed extension, phases, controls, bounded workspace and clean inverse");
}
