/* Finite basis-action checks for wide registers. The source preparation is
 * excluded from this interpreter; these are not dense 32/64-bit simulations.
 * Exhaustive byte inputs and selected wide boundaries use host C as oracle. */
static void
small_remainder_case(unsigned width, int sign, int divisor, unsigned projection, unsigned tables) {
    qc_context *c = qc_create(0, 7);
    if (!c)
        validation_fail("small remainder context allocation failed");
    qc_allow_tables(c, tables);
    char operation[12000], input[256];
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":%s}",
                    width,
                    sign ? "true" : "false");
    apply(c, operation);
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}");
    apply(c, "{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}");
    unsigned result_width = projection == 0 ? 1 : projection == 1 ? 8 : 32;
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qalloc\",\"id\":1,\"width\":%u,\"signed\":false}",
                    result_width);
    apply(c, operation);
    apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":2,\"lower\":0,\"upper\":1}");
    size_t begin = c->ngates;
    validation_path(input,
                    sizeof input,
                    "{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":%s}",
                    width,
                    sign ? "true" : "false");
    unsigned promoted = width < 32 ? 32 : width;
    int signed_operation = width < 32 || sign;
    const char *a = width < 32 ? node("cast", 32, 1, keep(input), NULL, NULL) : keep(input);
    const char *expr = node("cast",
                            result_width,
                            0,
                            node("mod",
                                 promoted,
                                 signed_operation,
                                 a,
                                 constant(promoted, signed_operation, divisor),
                                 NULL),
                            NULL,
                            NULL);
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s,"
                    "\"controls\":[{\"id\":2,\"value\":1}]}",
                    expr);
    apply(c, operation);
    size_t forward = c->ngates;
    qc_register *source = qc_lookup(c, 0), *result = qc_lookup(c, 1), *control = qc_lookup(c, 2);
    if (!source || !result || !control || qc_qubits(c) > QC_MAX_PHYSICAL)
        validation_fail("small remainder lost its registers or exceeded its wire budget");
    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(result->wires[0]), QC_WIRE(result->wires[0]), .37))
        validation_fail("small remainder result phase failed");
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"quncompute\",\"id\":1,\"expr\":%s,"
                    "\"controls\":[{\"id\":2,\"value\":1}]}",
                    expr);
    apply(c, operation);
    /* Lookup again: arithmetic scratch allocation can relocate register storage. */
    source = qc_lookup(c, 0);
    result = qc_lookup(c, 1);
    control = qc_lookup(c, 2);
    uint64_t mask = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
    uint64_t top = UINT64_C(1) << (width - 1);
    uint64_t edges[] = {0,
                        1,
                        2,
                        3,
                        7,
                        15,
                        16,
                        17,
                        top - 1,
                        top,
                        top + 1,
                        mask - 1,
                        mask,
                        UINT64_C(0xaaaaaaaaaaaaaaaa) & mask,
                        UINT64_C(0x5555555555555555) & mask};
    unsigned labels = width == 8 ? 256 : sizeof edges / sizeof edges[0] + 32;
    uint64_t random = UINT64_C(0x83a6210ce2516df7);
    for (unsigned i = 0; i < labels; ++i) {
        random ^= random << 13;
        random ^= random >> 7;
        random ^= random << 17;
        uint64_t raw = width == 8                           ? i
                       : i < sizeof edges / sizeof edges[0] ? edges[i]
                                                            : random & mask;
        int64_t value = signed_operation
                            ? (sign ? expression_signed(raw, width) : (int64_t)raw) % divisor
                            : (int64_t)(raw % (unsigned)divisor);
        uint64_t projected =
            result_width == 1 ? value != 0 : (uint64_t)value & ((UINT64_C(1) << result_width) - 1);
        for (unsigned active = 0; active < 2; ++active) {
            qc_mask original =
                qc_mask_or(expression_basis(source, raw), expression_basis(control, active));
            qc_mask basis = original;
            double angle = 0;
            expression_walk(c, begin, forward, &basis, &angle);
            qc_mask expected =
                qc_mask_or(original, expression_basis(result, active ? projected : 0));
            if (!qc_mask_eq(basis, expected) || fabs(angle) > 1e-10)
                validation_fail("remainder forward width=%u sign=%d d=%d projection=%u raw=%" PRIu64
                                " active=%u tables=%u",
                                width,
                                sign,
                                divisor,
                                projection,
                                raw,
                                active,
                                tables);
            expression_walk(c, forward, c->ngates, &basis, &angle);
            if (!qc_mask_eq(basis, original) ||
                fabs(angle - (active && (projected & 1) ? .37 : 0)) > 1e-10)
                validation_fail("remainder inverse left garbage or lost a relative phase");
        }
    }
    if (c->measure || c->reset)
        validation_fail("small remainder introduced measurement or reset");
    apply(c, "{\"op\":\"qrelease\",\"id\":1}");
    qc_destroy(c);
    clear();
    ++checks;
}

static void small_remainder_states(void) {
    const unsigned widths[] = {8, 32, 64};
    for (unsigned tables = 0; tables < 2; ++tables)
        for (unsigned index = 0; index < 3; ++index)
            for (int sign = 0; sign < 2; ++sign)
                for (int divisor = 2; divisor <= 16; ++divisor)
                    for (unsigned projection = 0; projection < (index == 2 ? 2u : 3u);
                         ++projection) {
                        small_remainder_case(widths[index], sign, divisor, projection, tables);
                        if (sign || widths[index] < 32)
                            small_remainder_case(widths[index], sign, -divisor, projection, tables);
                    }
    puts("PASS small constant remainder: exhaustive byte and finite 32/64-bit basis actions, "
         "C promotions, signed limits, Boolean/narrow/full-int results, coherent controls, "
         "relative phases and clean inverse; no dense wide-state claim");
}
