// Decimal parsing is a bounded sequence of typed reversible computations.
// Phase 0 skips locale whitespace, phase 1 accepts digits after an optional
// sign, and phase 2 keeps the first completed result. Old state is retained.
bool standard_decimal_conversion(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))) ||
        !DECL_EXTERNAL(fn) || !TREE_PUBLIC(fn) || !in_system_header_at(DECL_SOURCE_LOCATION(fn)))
        return false;
    std::string name = name_of(fn);
    return name == "atoi" || name == "atol" || name == "atoll" || name == "__cket_atoi" ||
           name == "__cket_atol" || name == "__cket_atoll" || name == "strtol" ||
           name == "strtoll" || name == "strtoimax" || name == "__strtol_internal" ||
           name == "__strtoll_internal";
}

tree expand_decimal_conversion(tree call) {
    int nargs = call_expr_nargs(call);
    if (nargs != 1 && nargs != 3 && nargs != 4)
        return NULL_TREE;
    QuantumByteRange source;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), source);
    if (!source.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)))
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    tree end_decl = NULL_TREE;
    uint64_t radix = 10;
    if (nargs >= 3) {
        tree endptr = strip(CALL_EXPR_ARG(call, 1));
        if (!endptr)
            return reject("|c>: quantum integer conversion requires a checked endptr argument");
        if (!integer_zerop(endptr)) {
            if (TREE_CODE(endptr) != ADDR_EXPR)
                return reject(
                    "|c>: quantum integer conversion endptr requires NULL or the address of a local character/void pointer");
            tree target = strip(TREE_OPERAND(endptr, 0));
            if (!target || TREE_CODE(target) != VAR_DECL ||
                TREE_CODE(TREE_TYPE(target)) != POINTER_TYPE)
                return reject(
                    "|c>: quantum integer conversion endptr requires NULL or the address of a local character/void pointer");
            tree ptr_type = TREE_TYPE(target), pointed = TREE_TYPE(ptr_type);
            bool character = TYPE_MAIN_VARIANT(pointed) == char_type_node ||
                             TYPE_MAIN_VARIANT(pointed) == signed_char_type_node ||
                             TYPE_MAIN_VARIANT(pointed) == unsigned_char_type_node;
            if ((TREE_CODE(pointed) != VOID_TYPE && !character) || TYPE_VOLATILE(pointed) ||
                TYPE_ATOMIC(pointed) || TYPE_VOLATILE(ptr_type) || TYPE_ATOMIC(ptr_type) ||
                TREE_STATIC(target) || DECL_EXTERNAL(target) || quantum(target) ||
                lookup_attribute("cket_quant", DECL_ATTRIBUTES(target)))
                return reject(
                    "|c>: quantum integer conversion endptr requires a local nonvolatile character/void pointer");
            if (quantum_search_pointers.count(target) || quantum_references.count(target) ||
                quantum_pointer_aliases.count(target) || quantum_heap_pointers.count(target))
                return reject(
                    "|c>: quantum integer conversion endptr cannot rebind an existing quantum pointer");
            end_decl = target;
        }
        tree base_arg = strip(CALL_EXPR_ARG(call, 2));
        if (!base_arg || TREE_CODE(base_arg) != INTEGER_CST || !tree_fits_shwi_p(base_arg))
            return reject("|c>: quantum integer conversion requires a constant radix in 2..36");
        int64_t base_val = tree_to_shwi(base_arg);
        if (base_val < 2 || base_val > 36)
            return reject("|c>: quantum integer conversion requires a constant radix in 2..36");
        if (nargs == 4 && !integer_zerop(strip(CALL_EXPR_ARG(call, 3))))
            return reject("|c>: quantum integer conversion does not support locale grouping");
        radix = static_cast<uint64_t>(base_val);
    }
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
    uint64_t max_digit = radix - 1;
    tree phase_type = build_nonstandard_integer_type(2, true);
    tree phase = build_zero_cst(integer_type_node), negative = boolean_false_node;
    tree magnitude = build_zero_cst(unsigned_type);
    tree end_offset = end_decl ? build_zero_cst(integer_type_node) : NULL_TREE;
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
        int dec_max = radix <= 10 ? static_cast<int>('0' + radix - 1) : '9';
        tree dec_digit = both(binary(GE_EXPR, byte, build_int_cst(integer_type_node, '0')),
                              binary(LE_EXPR, byte, build_int_cst(integer_type_node, dec_max)));
        tree digit = dec_digit;
        tree raw_digit_value = fold_build2_loc(
            location, MINUS_EXPR, integer_type_node, byte, build_int_cst(integer_type_node, '0'));
        if (radix > 10) {
            int alpha_span = static_cast<int>(radix - 11);
            tree upper_digit =
                both(binary(GE_EXPR, byte, build_int_cst(integer_type_node, 'A')),
                     binary(LE_EXPR, byte, build_int_cst(integer_type_node, 'A' + alpha_span)));
            tree lower_digit =
                both(binary(GE_EXPR, byte, build_int_cst(integer_type_node, 'a')),
                     binary(LE_EXPR, byte, build_int_cst(integer_type_node, 'a' + alpha_span)));
            digit = either(dec_digit, either(upper_digit, lower_digit));
            tree upper_val = fold_build2_loc(location,
                                             MINUS_EXPR,
                                             integer_type_node,
                                             byte,
                                             build_int_cst(integer_type_node, 'A' - 10));
            tree lower_val = fold_build2_loc(location,
                                             MINUS_EXPR,
                                             integer_type_node,
                                             byte,
                                             build_int_cst(integer_type_node, 'a' - 10));
            raw_digit_value =
                choose(dec_digit, raw_digit_value, choose(upper_digit, upper_val, lower_val));
        }
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
        tree digit_value = fold_convert(unsigned_type, raw_digit_value);
        tree remainder = choose(negative,
                                build_int_cstu(unsigned_type, negative_limit % radix),
                                build_int_cstu(unsigned_type, positive_limit % radix));
        tree threshold = build_int_cstu(unsigned_type, positive_limit / radix);
        tree safe = either(
            binary(LT_EXPR, magnitude, threshold),
            both(binary(EQ_EXPR, magnitude, threshold), binary(LE_EXPR, digit_value, remainder)));
        tree next = fold_build2_loc(location,
                                    PLUS_EXPR,
                                    unsigned_type,
                                    fold_build2_loc(location,
                                                    MULT_EXPR,
                                                    unsigned_type,
                                                    magnitude,
                                                    build_int_cstu(unsigned_type, radix)),
                                    digit_value);
        next = choose(take,
                      choose(safe, next, missing_string_terminator(unsigned_type, location)),
                      magnitude);
        if (i + 1 == count)
            next = choose(byte_range_terminated(source, location),
                          next,
                          missing_string_terminator(unsigned_type, location));
        bound = bound > (negative_limit - max_digit) / radix ? negative_limit
                                                             : bound * radix + max_digit;
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
        if (end_decl) {
            tree next_end = choose(take, build_int_cstu(integer_type_node, i + 1), end_offset);
            unsigned end_bits = 1;
            while ((uint64_t{1} << end_bits) <= i + 1)
                ++end_bits;
            tree end_step_type = build_nonstandard_integer_type(end_bits, true);
            end_offset =
                fold_convert(integer_type_node,
                             quantum_value_capture(
                                 call, next_end, "__cket_decimal_end_", prefix, end_step_type));
        }
        phase = fold_convert(
            integer_type_node,
            quantum_value_capture(call, next_phase, "__cket_decimal_phase_", prefix, phase_type));
        negative = fold_convert(boolean_type_node, next_negative_value);
        magnitude = fold_convert(unsigned_type, next_magnitude);
    }
    if (end_decl) {
        QuantumStringSearch search;
        search.source = source;
        search.source.stop_at_zero = true;
        search.source.cpu_bytes.clear();
        search.positions = count;
        search.position = fold_convert(size_type_node, end_offset);
        search.found = boolean_true_node;
        search.defined = boolean_true_node;
        quantum_search_pointers[end_decl] = search;
        DECL_INITIAL(end_decl) = NULL_TREE;
        procedure_cpu_flags.insert(end_decl);
    }
    tree signed_value = fold_convert(type, magnitude);
    tree negated = choose(binary(EQ_EXPR, magnitude, build_int_cstu(unsigned_type, negative_limit)),
                          TYPE_MIN_VALUE(type),
                          fold_build1_loc(location, NEGATE_EXPR, type, signed_value));
    tree result = choose(negative, negated, signed_value);
    return make_quantum_value_program(call, {}, boolean_true_node, std::move(prefix), result);
}
