// Decimal parsing is a bounded sequence of typed reversible computations.
// Phase 0 skips locale whitespace, phase 1 accepts digits after an optional
// sign, and phase 2 keeps the first completed result. Old state is retained.
bool standard_decimal_conversion(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))) ||
        !DECL_EXTERNAL(fn) || !TREE_PUBLIC(fn) || !in_system_header_at(DECL_SOURCE_LOCATION(fn)))
        return false;
    std::string name = name_of(fn);
    return name == "atoi" || name == "atol" || name == "atoll" || name == "__cket_atoi" ||
           name == "__cket_atol" || name == "__cket_atoll";
}

tree expand_decimal_conversion(tree call) {
    if (call_expr_nargs(call) != 1)
        return NULL_TREE;
    QuantumByteRange source;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), source);
    if (!source.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)))
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!resolved)
        return reject(source.error
                          ? source.error
                          : "|c>: quantum decimal conversion needs checked integer object bytes");
    tree type = TREE_TYPE(call);
    unsigned width = TYPE_PRECISION(type);
    if (!INTEGRAL_TYPE_P(type) || TYPE_UNSIGNED(type) || width < 8 || width > 64 || CHAR_BIT != 8 ||
        '0' != 48 || '9' != 57)
        return reject(
            "|c>: quantum decimal conversion needs the ASCII signed integer target profile");
    uint64_t count = source.size() - source.offset;
    if (!count || count > 64)
        return reject("|c>: quantum decimal conversion requires a NUL within 1..64 checked bytes");
    if (!decimal_space_fn) {
        decimal_space_fn = build_fn_decl(
            "__cket_decimal_isspace",
            build_function_type_list(integer_type_node, integer_type_node, NULL_TREE));
        TREE_PUBLIC(decimal_space_fn) = DECL_EXTERNAL(decimal_space_fn) =
            DECL_ARTIFICIAL(decimal_space_fn) = 1;
    }
    location_t location = at(call);
    auto binary = [location](enum tree_code code, tree a, tree b) {
        return fold_build2_loc(location, code, boolean_type_node, a, b);
    };
    auto both = [&](tree a, tree b) { return binary(TRUTH_ANDIF_EXPR, a, b); };
    auto either = [&](tree a, tree b) { return binary(TRUTH_ORIF_EXPR, a, b); };
    auto choose = [location](tree condition, tree a, tree b) {
        return fold_build3_loc(location, COND_EXPR, TREE_TYPE(a), condition, a, b);
    };
    tree unsigned_type = unsigned_type_for(type);
    uint64_t negative_limit = uint64_t{1} << (width - 1), positive_limit = negative_limit - 1;
    tree phase_type = build_nonstandard_integer_type(2, true);
    tree phase = build_zero_cst(integer_type_node), negative = boolean_false_node;
    tree magnitude = build_zero_cst(unsigned_type);
    std::vector<tree> prefix;
    uint64_t bound = 0;
    for (uint64_t i = 0; i < count; ++i) {
        tree byte = object_byte(source, i, location);
        tree leading = binary(EQ_EXPR, phase, build_zero_cst(integer_type_node));
        tree running = binary(NE_EXPR, phase, build_int_cst(integer_type_node, 2));
        tree space = build_call_expr_loc(location, decimal_space_fn, 1, byte);
        space = binary(NE_EXPR, space, build_zero_cst(integer_type_node));
        tree skip = both(leading, space);
        tree available =
            both(running, fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, skip));
        tree digit = both(binary(GE_EXPR, byte, build_int_cst(integer_type_node, '0')),
                          binary(LE_EXPR, byte, build_int_cst(integer_type_node, '9')));
        tree take = both(available, digit);
        tree minus = binary(EQ_EXPR, byte, build_int_cst(integer_type_node, '-'));
        tree sign =
            both(both(leading, available),
                 either(minus, binary(EQ_EXPR, byte, build_int_cst(integer_type_node, '+'))));
        tree next_negative = choose(sign, minus, negative);
        tree next_phase = choose(
            either(take, sign),
            build_int_cst(integer_type_node, 1),
            choose(skip, build_zero_cst(integer_type_node), build_int_cst(integer_type_node, 2)));
        tree digit_value = fold_convert(unsigned_type,
                                        fold_build2_loc(location,
                                                        MINUS_EXPR,
                                                        integer_type_node,
                                                        byte,
                                                        build_int_cst(integer_type_node, '0')));
        tree remainder = choose(negative,
                                build_int_cstu(unsigned_type, negative_limit % 10),
                                build_int_cstu(unsigned_type, positive_limit % 10));
        tree threshold = build_int_cstu(unsigned_type, positive_limit / 10);
        tree safe = either(
            binary(LT_EXPR, magnitude, threshold),
            both(binary(EQ_EXPR, magnitude, threshold), binary(LE_EXPR, digit_value, remainder)));
        tree next = fold_build2_loc(
            location,
            PLUS_EXPR,
            unsigned_type,
            fold_build2_loc(
                location, MULT_EXPR, unsigned_type, magnitude, build_int_cstu(unsigned_type, 10)),
            digit_value);
        next = choose(take,
                      choose(safe, next, missing_string_terminator(unsigned_type, location)),
                      magnitude);
        if (i + 1 == count)
            next = choose(byte_range_terminated(source, location),
                          next,
                          missing_string_terminator(unsigned_type, location));
        bound = bound > (negative_limit - 9) / 10 ? negative_limit : bound * 10 + 9;
        unsigned bits = 1;
        while (bits < width && (bound >> bits))
            ++bits;
        tree next_magnitude = quantum_value_capture(call,
                                                    next,
                                                    "__cket_decimal_value_",
                                                    prefix,
                                                    build_nonstandard_integer_type(bits, true));
        tree next_negative_value = quantum_value_capture(
            call, next_negative, "__cket_decimal_sign_", prefix, boolean_type_node);
        phase = fold_convert(
            integer_type_node,
            quantum_value_capture(call, next_phase, "__cket_decimal_phase_", prefix, phase_type));
        negative = fold_convert(boolean_type_node, next_negative_value);
        magnitude = fold_convert(unsigned_type, next_magnitude);
    }
    tree signed_value = fold_convert(type, magnitude);
    tree negated = choose(binary(EQ_EXPR, magnitude, build_int_cstu(unsigned_type, negative_limit)),
                          TYPE_MIN_VALUE(type),
                          fold_build1_loc(location, NEGATE_EXPR, type, signed_value));
    tree result = choose(negative, negated, signed_value);
    return make_quantum_value_program(call, {}, boolean_true_node, std::move(prefix), result);
}
