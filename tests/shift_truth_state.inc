/* Four explicit source labels, every valid count of the promoted word, six
 * truth forms, both synthesis policies and both coherent control branches. */
static uint64_t shift_truth_mask(unsigned width) {
    return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
}

static unsigned shift_truth_reference(unsigned seed,
                                      unsigned width,
                                      unsigned promoted,
                                      int sign,
                                      int left,
                                      int narrow,
                                      unsigned form,
                                      unsigned distance) {
    uint64_t source_mask = shift_truth_mask(width), mask = shift_truth_mask(promoted);
    uint64_t raw = seed < 2 ? seed : source_mask - (3 - seed);
    if (sign && width < promoted && (raw & (UINT64_C(1) << (width - 1))))
        raw |= mask ^ source_mask;
    uint64_t value = left ? (raw << distance) & mask : raw >> distance;
    if (!left && sign && distance && (raw & (UINT64_C(1) << (promoted - 1))))
        value |= mask ^ ((UINT64_C(1) << (promoted - distance)) - 1);
    if (narrow)
        value &= 255;
    unsigned truth = value != 0;
    return form >= 1 && form <= 3 ? !truth : truth;
}

static void shift_truth_state(qc_context *c,
                              unsigned width,
                              int sign,
                              int left,
                              int narrow,
                              unsigned form,
                              unsigned active,
                              int inverse) {
    unsigned promoted = width < 32 ? 32 : width;
    size_t states = (size_t)4 * promoted * 2;
    unsigned char seen[512] = {0};
    if (qc_sparse_size(c) != states)
        validation_fail("shift truth support changed");
    qc_mask allowed = 0;
    for (unsigned id = 0; id < 5; ++id) {
        qc_register *r = qc_lookup(c, id);
        for (unsigned bit = 0; bit < r->width; ++bit)
            allowed |= QC_WIRE(r->wires[bit]);
    }
    for (size_t row = 0; row < states; ++row) {
        uint64_t low, high;
        double complex amplitude;
        if (!qc_sparse_entry(c, row, &low, &high, &amplitude))
            validation_fail("shift truth enumeration");
        qc_mask basis = (qc_mask)low | (qc_mask)high << 64;
        unsigned seed = (unsigned)shift_read(basis, qc_lookup(c, 4));
        unsigned count = (unsigned)shift_read(basis, qc_lookup(c, 1));
        unsigned control = (unsigned)shift_read(basis, qc_lookup(c, 3));
        uint64_t raw = seed < 2 ? seed : shift_truth_mask(width) - (3 - seed);
        if (seed > 3 || count >= promoted || (basis & ~allowed) ||
            shift_read(basis, qc_lookup(c, 0)) != raw)
            validation_fail("shift truth input or private wire mismatch");
        unsigned truth =
            control == active
                ? shift_truth_reference(seed, width, promoted, sign, left, narrow, form, count)
                : 0;
        if (shift_read(basis, qc_lookup(c, 2)) != (inverse ? 0 : truth))
            validation_fail(
                "shift truth value mismatch width=%u sign=%d left=%d narrow=%d form=%u seed=%u count=%u control=%u inverse=%d actual=%llu expected=%u",
                width,
                sign,
                left,
                narrow,
                form,
                seed,
                count,
                control,
                inverse,
                (unsigned long long)shift_read(basis, qc_lookup(c, 2)),
                inverse ? 0 : truth);
        double angle = .003 * seed + .007 * count + .011 * control + (inverse && truth ? .37 : 0);
        if (cabs(amplitude - cexp(I * angle) / sqrt((double)states)) > 2e-11)
            validation_fail("shift truth phase mismatch");
        size_t label = (size_t)seed * promoted * 2 + count * 2 + control;
        if (seen[label])
            validation_fail("duplicate shift truth state");
        seen[label] = 1;
    }
}

static void shift_truth_states(void) {
    const struct {
        unsigned width;
        int sign, left, narrow;
    } profiles[] = {{8, 0, 1, 0},
                    {8, 0, 0, 0},
                    {8, 1, 0, 0},
                    {32, 0, 1, 0},
                    {32, 0, 0, 0},
                    {32, 1, 0, 0},
                    {64, 0, 1, 0},
                    {64, 0, 0, 0},
                    {64, 1, 0, 0},
                    {64, 0, 1, 1},
                    {64, 0, 0, 1}};

    for (unsigned profile = 0; profile < sizeof profiles / sizeof *profiles; ++profile)
        for (unsigned form = 0; form < 6; ++form)
            for (unsigned tables = 0; tables < 2; ++tables) {
                unsigned width = profiles[profile].width, promoted = width < 32 ? 32 : width;
                unsigned count_bits = promoted == 64 ? 6 : 5, active = profile & 1;
                int sign = profiles[profile].sign, left = profiles[profile].left,
                    narrow = profiles[profile].narrow;
                qc_context *c = qc_create_sparse(7, 1024);
                if (!c)
                    validation_fail("shift truth context");
                qc_allow_tables(c, tables);
                char operation[3600], source[1800], input[400], value[1800], consumed[2200];
                char predicate[2700], expression[3000];
                apply(c, "{\"op\":\"qalloc\",\"id\":4,\"width\":2,\"signed\":false}");
                apply(c, "{\"op\":\"qprepare\",\"id\":4,\"lower\":0,\"upper\":3}");
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":%s}",
                         width,
                         sign ? "true" : "false");
                apply(c, operation);
                snprintf(
                    source,
                    sizeof source,
                    "{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":4,\"bits\":2,\"signed\":false}]}]}",
                    width,
                    sign ? "true" : "false");
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qcompute\",\"id\":0,\"expr\":%s}",
                         source);
                apply(c, operation);
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qalloc\",\"id\":1,\"width\":%u,\"signed\":false}",
                         count_bits);
                apply(c, operation);
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":%u}",
                         promoted - 1);
                apply(c, operation);
                apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false}");
                apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}");
                apply(c, "{\"op\":\"qprepare\",\"id\":3,\"lower\":0,\"upper\":1}");
                for (unsigned id = 1; id < 5; ++id) {
                    if (id == 2)
                        continue;
                    qc_register *r = qc_lookup(c, id);
                    for (unsigned bit = 0; bit < r->width; ++bit) {
                        double angle = (id == 4 ? .003 : id == 1 ? .007 : .011) * (1u << bit);
                        if (!qc_emit(c,
                                     QC_PHASE,
                                     0,
                                     QC_WIRE(r->wires[bit]),
                                     QC_WIRE(r->wires[bit]),
                                     angle))
                            validation_fail("shift truth input phase");
                    }
                }
                snprintf(
                    input,
                    sizeof input,
                    "{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":%s}]}",
                    promoted,
                    sign ? "true" : "false",
                    width,
                    sign ? "true" : "false");
                snprintf(
                    value,
                    sizeof value,
                    "{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[%s,{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":%u,\"signed\":false}]}]}",
                    left ? "shl" : "shr",
                    promoted,
                    sign ? "true" : "false",
                    input,
                    count_bits);
                if (narrow)
                    snprintf(consumed,
                             sizeof consumed,
                             "{\"op\":\"cast\",\"bits\":8,\"signed\":false,\"args\":[%s]}",
                             value);
                else
                    snprintf(consumed, sizeof consumed, "%s", value);
                if (form == 0)
                    snprintf(predicate, sizeof predicate, "%s", consumed);
                else if (form == 1)
                    snprintf(predicate,
                             sizeof predicate,
                             "{\"op\":\"not\",\"bits\":32,\"signed\":true,\"args\":[%s]}",
                             consumed);
                else {
                    char operand[2400], zero[100];
                    if (narrow)
                        snprintf(operand,
                                 sizeof operand,
                                 "{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[%s]}",
                                 consumed);
                    else
                        snprintf(operand, sizeof operand, "%s", consumed);
                    snprintf(zero,
                             sizeof zero,
                             "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":0}",
                             narrow ? 32 : promoted,
                             (narrow || sign) ? "true" : "false");
                    int reverse = form == 3 || form == 5;
                    if (reverse)
                        snprintf(predicate,
                                 sizeof predicate,
                                 "{\"op\":\"%s\",\"bits\":32,\"signed\":true,\"args\":[%s,%s]}",
                                 form < 4 ? "eq" : "ne",
                                 zero,
                                 operand);
                    else
                        snprintf(predicate,
                                 sizeof predicate,
                                 "{\"op\":\"%s\",\"bits\":32,\"signed\":true,\"args\":[%s,%s]}",
                                 form < 4 ? "eq" : "ne",
                                 operand,
                                 zero);
                }
                snprintf(expression,
                         sizeof expression,
                         "{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":[%s]}",
                         predicate);
                snprintf(
                    operation,
                    sizeof operation,
                    "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s,\"controls\":[{\"id\":3,\"value\":%u}]}",
                    expression,
                    active);
                apply(c, operation);
                if (qc_qubits(c) > width + count_bits + 16)
                    validation_fail("truth/cast allocated full-width shift scratch");
                shift_truth_state(c, width, sign, left, narrow, form, active, 0);
                qc_register *out = qc_lookup(c, 2);
                if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(out->wires[0]), QC_WIRE(out->wires[0]), .37))
                    validation_fail("shift truth result phase");
                snprintf(
                    operation,
                    sizeof operation,
                    "{\"op\":\"quncompute\",\"id\":2,\"expr\":%s,\"controls\":[{\"id\":3,\"value\":%u}]}",
                    expression,
                    active);
                apply(c, operation);
                shift_truth_state(c, width, sign, left, narrow, form, active, 1);
                if (c->measure || c->reset)
                    validation_fail("shift truth introduced a hidden channel");
                apply(c, "{\"op\":\"qrelease\",\"id\":2}");
                qc_destroy(c);
                ++checks;
            }
    puts(
        "PASS 132 shift-truth/cast sparse domains: six truth forms, selected8/32/64-bit labels, every promoted count, phases, controls and clean inverse");
}
