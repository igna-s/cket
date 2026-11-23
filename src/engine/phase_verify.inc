/* Deferred angles contain pure CPU values; quantum controls stay in the
 * instruction. Validate all branches structurally, including inactive ones. */
static int phase_cpu_integer(qc_context *c, const cj_doc *d, int e) {
    if (!expression(c, d, e))
        return 0;
    for (int token = e; token < d->tokens[e].next; ++token)
        if (d->tokens[token].kind == '{' && cj_equal(d, cj_get(d, token, "op"), "reg"))
            return qc_fail(c, "deferred phase arithmetic must contain only CPU values");
    return 1;
}

static int phase_expression(qc_context *c, const cj_doc *d, int e, unsigned depth) {
    if (depth > 128 || (e >= 0 && e < d->count && d->tokens[e].next - e > 100000))
        return qc_fail(c, "phase expression complexity budget exceeded");
    static const char *const leaf[] = {"op", "bits", "value", NULL};
    static const char *const composite[] = {"op", "bits", "args", NULL};
    uint64_t width;
    if (e < 0 || e >= d->count || d->tokens[e].kind != '{' || !string_field(c, d, e, "op", 0) ||
        !unsigned_field(c, d, e, "bits", &width) || (width != 32 && width != 64))
        return qc_fail(c, "phase arithmetic requires binary32 or binary64 values");
    int op = cj_get(d, e, "op");
    if (cj_equal(d, op, "real_const")) {
        int value = cj_get(d, e, "value");
        if (!fields(c, d, e, leaf) || value < 0 ||
            !(d->tokens[value].kind == '-' ||
              (d->tokens[value].kind >= '0' && d->tokens[value].kind <= '9')) ||
            !isfinite(cj_double(d, value)))
            return qc_fail(c, "phase arithmetic constant must be finite and numeric");
        return 1;
    }
    if (!fields(c, d, e, composite))
        return 0;
    unsigned arity = 0;
    const char *unary[] = {"real_from_int", "real_bits", "real_cast", "real_neg", "real_abs", NULL};
    for (unsigned i = 0; unary[i]; ++i)
        if (cj_equal(d, op, unary[i]))
            arity = 1;
    const char *binary[] = {"real_add", "real_sub", "real_mul", "real_div", NULL};
    for (unsigned i = 0; binary[i]; ++i)
        if (cj_equal(d, op, binary[i]))
            arity = 2;
    if (cj_equal(d, op, "real_select"))
        arity = 3;
    int args = cj_get(d, e, "args"), nodes[3];
    unsigned count = 0;
    if (!arity || args < 0 || d->tokens[args].kind != '[')
        return qc_fail(c, "invalid deferred phase operator or operands");
    for (int token = args + 1; token < d->tokens[args].next; token = d->tokens[token].next) {
        if (count == arity)
            return qc_fail(c, "deferred phase operand count mismatch");
        nodes[count++] = token;
    }
    if (count != arity)
        return qc_fail(c, "deferred phase operand count mismatch");
    if (cj_equal(d, op, "real_from_int") || cj_equal(d, op, "real_bits")) {
        if (!phase_cpu_integer(c, d, nodes[0]))
            return 0;
        uint64_t input;
        if (cj_equal(d, op, "real_bits") &&
            (!unsigned_field(c, d, nodes[0], "bits", &input) || input != width ||
             cj_bool(d, cj_get(d, nodes[0], "signed"))))
            return qc_fail(c, "phase bit representation requires a matching unsigned integer");
        return 1;
    }
    for (unsigned i = 0; i < count; ++i) {
        if (arity == 3 && i == 0) {
            if (!phase_cpu_integer(c, d, nodes[i]))
                return 0;
            continue;
        }
        if (!phase_expression(c, d, nodes[i], depth + 1))
            return 0;
        uint64_t input;
        if (!cj_equal(d, op, "real_cast") &&
            (!unsigned_field(c, d, nodes[i], "bits", &input) || input != width))
            return qc_fail(c, "phase arithmetic requires explicit C conversions");
    }
    return 1;
}
