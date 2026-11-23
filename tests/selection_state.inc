/* Finite gate-action checks, not dense simulations of the wide registers.
 * Each case checks every byte and both control labels against C bit patterns,
 * then checks source-dependent phases and zero workspace after the inverse. */
static const char *selection_constant(unsigned width, int sign, uint64_t value) {
    if (sign)
        return constant(width, 1, expression_signed(value, width));
    char text[160];
    validation_path(text,
                    sizeof text,
                    "{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":%" PRIu64 "}",
                    width,
                    value);
    return keep(text);
}

static void
selection_case(unsigned width, int sign, unsigned profile, unsigned reverse, unsigned tables) {
    qc_context *c = qc_create(0, 7);
    if (!c)
        validation_fail("selection context allocation failed");
    qc_allow_tables(c, tables);
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":255}");
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
    char operation[16000];
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qalloc\",\"id\":2,\"width\":%u,\"signed\":%s}",
                    width,
                    sign ? "true" : "false");
    apply(c, operation);
    size_t begin = c->ngates;
    const char *raw = "{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false}";
    const char *control = "{\"op\":\"reg\",\"id\":1,\"bits\":1,\"signed\":false}";
    const char *condition = reverse ? node("not", 1, 0, control, NULL, NULL) : control;
    const char *x =
        node("cast", width, sign, sign ? node("cast", 8, 1, raw, NULL, NULL) : raw, NULL, NULL);
    const char *complement = node("bnot", width, sign, x, NULL, NULL);
    uint64_t mask = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
    uint64_t low = sign ? UINT64_C(1) << (width - 1) : 0;
    uint64_t high = sign ? (UINT64_C(1) << (width - 1)) - 1 : mask;
    const char *minimum = selection_constant(width, sign, low);
    const char *maximum = selection_constant(width, sign, high);
    const char *zero = constant(width, sign, 0), *yes, *no;
    switch (profile) {
    case 0:
        yes = minimum;
        no = maximum;
        break;
    case 1:
        yes = x;
        no = x;
        break;
    case 2:
        yes = x;
        no = zero;
        break;
    case 3:
        yes = zero;
        no = x;
        break;
    case 4:
        yes = x;
        no = complement;
        break;
    case 5:
        yes = complement;
        no = x;
        break;
    case 6:
        yes = x;
        no = node("bxor", width, sign, x, selection_constant(width, sign, 0xaa), NULL);
        break;
    case 7:
        yes = maximum;
        no = x;
        break;
    case 8:
        yes = x;
        no = minimum;
        break;
    case 9:
        yes = node("band", width, sign, x, selection_constant(width, sign, 0xf0), NULL);
        no = node("bxor", width, sign, x, selection_constant(width, sign, 0xf0), NULL);
        break;
    case 10: {
        unsigned promoted = width < 32 ? 32 : width;
        int signed_operation = width < 32 || sign;
        const char *input = width < 32 ? node("cast", 32, 1, x, NULL, NULL) : x;
        yes = node("cast",
                   width,
                   sign,
                   node("mod",
                        promoted,
                        signed_operation,
                        input,
                        constant(promoted, signed_operation, 3),
                        NULL),
                   NULL,
                   NULL);
        no = node("cast",
                  width,
                  sign,
                  node("mod",
                       promoted,
                       signed_operation,
                       input,
                       constant(promoted, signed_operation, 5),
                       NULL),
                  NULL,
                  NULL);
        break;
    }
    default: {
        const char *odd = node("band", 8, 0, raw, constant(8, 0, 1), NULL);
        yes = node("select", width, sign, odd, x, zero);
        no = node("select", width, sign, odd, zero, complement);
        break;
    }
    }
    const char *expr = node("select", width, sign, condition, yes, no);
    validation_path(
        operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s}", expr);
    if (!qc_apply(c, operation, NULL))
        validation_fail("selection expression rejected width=%u sign=%d profile=%u reverse=%u "
                        "tables=%u: %s",
                        width,
                        sign,
                        profile,
                        reverse,
                        tables,
                        qc_error(c));
    size_t forward = c->ngates;
    qc_register *result = qc_lookup(c, 2);
    /* Nine public input wires plus at most31 private columns/guards in these
     * profiles, including both nested arms. A full64-bit mux word cannot fit. */
    if (qc_qubits(c) > width + 40)
        validation_fail("selection budget width=%u sign=%d profile=%u reverse=%u tables=%u "
                        "qubits=%u",
                        width,
                        sign,
                        profile,
                        reverse,
                        tables,
                        qc_qubits(c));
    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(result->wires[0]), QC_WIRE(result->wires[0]), .11) ||
        !qc_emit(c,
                 QC_PHASE,
                 0,
                 QC_WIRE(result->wires[width - 1]),
                 QC_WIRE(result->wires[width - 1]),
                 .29))
        validation_fail("selection result phase failed");
    validation_path(
        operation, sizeof operation, "{\"op\":\"quncompute\",\"id\":2,\"expr\":%s}", expr);
    apply(c, operation);
    qc_register *source = qc_lookup(c, 0), *guard = qc_lookup(c, 1);
    result = qc_lookup(c, 2);
    for (unsigned raw_value = 0; raw_value < 256; ++raw_value) {
        int64_t signed_value = raw_value < 128 ? (int64_t)raw_value : (int64_t)raw_value - 256;
        uint64_t value = sign ? (uint64_t)signed_value & mask : raw_value;
        uint64_t a, b;
        switch (profile) {
        case 0:
            a = low;
            b = high;
            break;
        case 1:
            a = value;
            b = value;
            break;
        case 2:
            a = value;
            b = 0;
            break;
        case 3:
            a = 0;
            b = value;
            break;
        case 4:
            a = value;
            b = (~value) & mask;
            break;
        case 5:
            a = (~value) & mask;
            b = value;
            break;
        case 6:
            a = value;
            b = value ^ 0xaa;
            break;
        case 7:
            a = high;
            b = value;
            break;
        case 8:
            a = value;
            b = low;
            break;
        case 9:
            a = value & 0xf0;
            b = value ^ 0xf0;
            break;
        case 10:
            a = sign ? (uint64_t)(signed_value % 3) & mask : raw_value % 3;
            b = sign ? (uint64_t)(signed_value % 5) & mask : raw_value % 5;
            break;
        default:
            a = raw_value & 1 ? value : 0;
            b = raw_value & 1 ? 0 : (~value) & mask;
            break;
        }
        for (unsigned control_value = 0; control_value < 2; ++control_value) {
            uint64_t selected = (control_value ^ reverse) ? a : b;
            qc_mask original = qc_mask_or(expression_basis(source, raw_value),
                                          expression_basis(guard, control_value));
            qc_mask basis = original;
            double angle = 0;
            expression_walk(c, begin, forward, &basis, &angle);
            if (!qc_mask_eq(basis, qc_mask_or(original, expression_basis(result, selected))) ||
                fabs(angle) > 1e-10)
                validation_fail("selection forward width=%u sign=%d profile=%u reverse=%u "
                                "tables=%u raw=%u control=%u",
                                width,
                                sign,
                                profile,
                                reverse,
                                tables,
                                raw_value,
                                control_value);
            expression_walk(c, forward, c->ngates, &basis, &angle);
            double expected_angle = .11 * (selected & 1) + .29 * ((selected >> (width - 1)) & 1);
            if (!qc_mask_eq(basis, original) || fabs(angle - expected_angle) > 1e-10)
                validation_fail("selection inverse left garbage or changed relative phase");
        }
    }
    if (c->measure || c->reset)
        validation_fail("selection introduced observation or reset");
    apply(c, "{\"op\":\"qrelease\",\"id\":2}");
    qc_destroy(c);
    clear();
    ++checks;
}

static void selection_states(void) {
    const unsigned widths[] = {8, 32, 64};
    for (unsigned tables = 0; tables < 2; ++tables)
        for (unsigned index = 0; index < 3; ++index)
            for (int sign = 0; sign < 2; ++sign)
                for (unsigned reverse = 0; reverse < 2; ++reverse)
                    for (unsigned profile = 0; profile < 12; ++profile)
                        selection_case(widths[index], sign, profile, reverse, tables);
    puts(
        "PASS 288 selection basis-action domains: every byte, both control labels and polarities, "
        "signed/unsigned 8/32/64-bit results, shared/complement columns, nested selections, "
        "arithmetic arms, bounded workspace and clean phase-preserving inverse; no dense wide-state claim");
}
