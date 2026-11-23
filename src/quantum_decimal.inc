// Decimal and radix integer parsing is a bounded sequence of typed reversible
// computations. States: 0 = leading whitespace, 1 = post-sign prefix,
// 2 = immediately after leading '0' (checking optional 0x/0X hex prefix),
// 3 = digits, 4 = finished. Old state is retained.
bool standard_decimal_conversion(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))) ||
        !DECL_EXTERNAL(fn) || !TREE_PUBLIC(fn) || !in_system_header_at(DECL_SOURCE_LOCATION(fn)))
        return false;
    std::string name = name_of(fn);
    return name == "atoi" || name == "atol" || name == "atoll" || name == "__cket_atoi" ||
           name == "__cket_atol" || name == "__cket_atoll" || name == "strtol" ||
           name == "__cket_strtol" || name == "strtoll" || name == "__cket_strtoll" ||
           name == "strtoimax" || name == "__cket_strtoimax" || name == "strtoul" ||
           name == "__cket_strtoul" || name == "strtoull" || name == "__cket_strtoull" ||
           name == "strtoumax" || name == "__cket_strtoumax" || name == "__strtol_internal" ||
           name == "__strtoll_internal" || name == "__strtoul_internal" ||
           name == "__strtoull_internal";
}

tree expand_decimal_conversion(tree call) {
    int nargs = call_expr_nargs(call);
    if (nargs != 1 && nargs != 3 && nargs != 4)
        return NULL_TREE;
    tree fn = get_callee_fndecl(call);
    std::string fn_name = fn ? name_of(fn) : "";
    bool is_ato = fn_name == "atoi" || fn_name == "__cket_atoi" || fn_name == "atol" ||
                  fn_name == "__cket_atol" || fn_name == "atoll" || fn_name == "__cket_atoll";
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (nargs >= 3 && has_quantum(CALL_EXPR_ARG(call, 2)))
        return reject("|c>: quantum integer conversion requires a constant radix 0 or in 2..36");
    QuantumByteRange source;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), source);
    if (!source.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)))
        return NULL_TREE;
    tree end_decl = NULL_TREE;
    int64_t base_val = 10;
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
            return reject(
                "|c>: quantum integer conversion requires a constant radix 0 or in 2..36");
        base_val = tree_to_shwi(base_arg);
        if (base_val != 0 && (base_val < 2 || base_val > 36))
            return reject(
                "|c>: quantum integer conversion requires a constant radix 0 or in 2..36");
        if (nargs == 4 && !integer_zerop(strip(CALL_EXPR_ARG(call, 3))))
            return reject("|c>: quantum integer conversion does not support locale grouping");
    }
    if (!resolved)
        return reject(source.error
                          ? source.error
                          : "|c>: quantum decimal conversion needs checked integer object bytes");
    tree type = TREE_TYPE(call);
    bool is_unsigned = TYPE_UNSIGNED(type);
    unsigned width = TYPE_PRECISION(type);
    if (!INTEGRAL_TYPE_P(type) || (is_ato && is_unsigned) || width < 8 || width > 64 ||
        CHAR_BIT != 8 || '0' != 48 || '9' != 57)
        return reject("|c>: quantum decimal conversion needs an ASCII integer target profile");
    uint64_t count = source.size() - source.offset;
    if (!count || count > 128)
        return reject("|c>: quantum decimal conversion requires a NUL within 1..128 checked bytes");
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
    auto negate = [&](tree a) {
        return fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, a);
    };
    auto choose = [location](tree condition, tree a, tree b) {
        return fold_build3_loc(location, COND_EXPR, TREE_TYPE(a), condition, a, b);
    };
    auto digit_decode = [&](tree byte_val, uint64_t r, tree &is_valid, tree &val) {
        int dec_max = r <= 10 ? static_cast<int>('0' + r - 1) : '9';
        tree dec_d = both(binary(GE_EXPR, byte_val, build_int_cst(integer_type_node, '0')),
                          binary(LE_EXPR, byte_val, build_int_cst(integer_type_node, dec_max)));
        tree d_val = fold_build2_loc(location,
                                     MINUS_EXPR,
                                     integer_type_node,
                                     byte_val,
                                     build_int_cst(integer_type_node, '0'));
        if (r > 10) {
            int alpha_span = static_cast<int>(r - 11);
            tree upper_d =
                both(binary(GE_EXPR, byte_val, build_int_cst(integer_type_node, 'A')),
                     binary(LE_EXPR, byte_val, build_int_cst(integer_type_node, 'A' + alpha_span)));
            tree lower_d =
                both(binary(GE_EXPR, byte_val, build_int_cst(integer_type_node, 'a')),
                     binary(LE_EXPR, byte_val, build_int_cst(integer_type_node, 'a' + alpha_span)));
            is_valid = either(dec_d, either(upper_d, lower_d));
            tree u_val = fold_build2_loc(location,
                                         MINUS_EXPR,
                                         integer_type_node,
                                         byte_val,
                                         build_int_cst(integer_type_node, 'A' - 10));
            tree l_val = fold_build2_loc(location,
                                         MINUS_EXPR,
                                         integer_type_node,
                                         byte_val,
                                         build_int_cst(integer_type_node, 'a' - 10));
            val = choose(dec_d, d_val, choose(upper_d, u_val, l_val));
        } else {
            is_valid = dec_d;
            val = d_val;
        }
    };

    tree unsigned_type = unsigned_type_for(type);
    uint64_t negative_limit = uint64_t{1} << (width - 1), positive_limit = negative_limit - 1;
    uint64_t unsigned_limit = width == 64 ? ~uint64_t{0} : (uint64_t{1} << width) - 1;
    uint64_t limit = is_unsigned ? unsigned_limit : negative_limit;
    bool auto_base = (base_val == 0);
    uint64_t fixed_radix = auto_base ? 10 : static_cast<uint64_t>(base_val);
    bool allow_hex_prefix = (base_val == 0 || base_val == 16);

    // State values: 0 = space, 1 = sign seen / prefix start, 2 = post-'0', 3 = digits, 4 = done.
    tree phase_type = build_nonstandard_integer_type(3, true);
    tree phase = build_zero_cst(integer_type_node);
    // For base == 0: radix_mode 0 = base 10, 1 = base 8, 2 = base 16.
    tree radix_mode_type = build_nonstandard_integer_type(2, true);
    tree radix_mode = build_zero_cst(integer_type_node);
    tree negative = boolean_false_node;
    tree overflow = boolean_false_node;
    tree magnitude = build_zero_cst(unsigned_type);
    tree end_offset = end_decl ? build_zero_cst(integer_type_node) : NULL_TREE;
    std::vector<tree> prefix;
    uint64_t bound = 0;

    for (uint64_t i = 0; i < count; ++i) {
        tree byte = object_byte(source, i, location);
        tree st0 = binary(EQ_EXPR, phase, build_zero_cst(integer_type_node));
        tree st1 = binary(EQ_EXPR, phase, build_int_cst(integer_type_node, 1));
        tree st2 = binary(EQ_EXPR, phase, build_int_cst(integer_type_node, 2));
        tree st_done = binary(EQ_EXPR, phase, build_int_cst(integer_type_node, 4));
        tree running = negate(st_done);

        tree space = build_call_expr_loc(location, decimal_space_fn, 1, byte);
        space = binary(NE_EXPR, space, build_zero_cst(integer_type_node));
        tree skip = both(st0, space);

        tree minus = binary(EQ_EXPR, byte, build_int_cst(integer_type_node, '-'));
        tree is_sign = either(minus, binary(EQ_EXPR, byte, build_int_cst(integer_type_node, '+')));
        tree take_sign = both(st0, both(negate(space), is_sign));
        tree at_prefix = either(st1, both(st0, both(negate(space), negate(is_sign))));

        tree is_zero_ch = binary(EQ_EXPR, byte, build_int_cst(integer_type_node, '0'));
        tree start_zero = both(at_prefix, is_zero_ch);
        if (!allow_hex_prefix)
            start_zero = boolean_false_node;

        tree consume_x = boolean_false_node;
        if (allow_hex_prefix && i + 1 < count) {
            tree is_x = either(binary(EQ_EXPR, byte, build_int_cst(integer_type_node, 'x')),
                               binary(EQ_EXPR, byte, build_int_cst(integer_type_node, 'X')));
            tree next_b = object_byte(source, i + 1, location);
            tree next_is_hex = boolean_false_node, unused_next_val = NULL_TREE;
            digit_decode(next_b, 16, next_is_hex, unused_next_val);
            consume_x = both(st2, both(is_x, next_is_hex));
        }

        // Radix / digit decode for the active radix at this step.
        tree is_digit = boolean_false_node, raw_digit_value = build_zero_cst(integer_type_node);
        tree active_radix = NULL_TREE, active_thresh = NULL_TREE, active_rem = NULL_TREE;
        if (auto_base) {
            tree mode_oct = binary(EQ_EXPR, radix_mode, build_int_cst(integer_type_node, 1));
            tree mode_hex = binary(EQ_EXPR, radix_mode, build_int_cst(integer_type_node, 2));
            tree d_dec = NULL_TREE, v_dec = NULL_TREE;
            tree d_oct = NULL_TREE, v_oct = NULL_TREE;
            tree d_hex = NULL_TREE, v_hex = NULL_TREE;
            digit_decode(byte, 10, d_dec, v_dec);
            digit_decode(byte, 8, d_oct, v_oct);
            digit_decode(byte, 16, d_hex, v_hex);
            is_digit = choose(mode_hex, d_hex, choose(mode_oct, d_oct, d_dec));
            raw_digit_value = choose(mode_hex, v_hex, choose(mode_oct, v_oct, v_dec));

            auto make_thresh = [&](uint64_t r) -> tree {
                if (is_unsigned)
                    return build_int_cstu(unsigned_type, unsigned_limit / r);
                return choose(negative,
                              build_int_cstu(unsigned_type, negative_limit / r),
                              build_int_cstu(unsigned_type, positive_limit / r));
            };
            auto make_rem = [&](uint64_t r) -> tree {
                if (is_unsigned)
                    return build_int_cstu(unsigned_type, unsigned_limit % r);
                return choose(negative,
                              build_int_cstu(unsigned_type, negative_limit % r),
                              build_int_cstu(unsigned_type, positive_limit % r));
            };
            active_radix = choose(mode_hex,
                                  build_int_cstu(unsigned_type, 16),
                                  choose(mode_oct,
                                         build_int_cstu(unsigned_type, 8),
                                         build_int_cstu(unsigned_type, 10)));
            active_thresh = choose(
                mode_hex, make_thresh(16), choose(mode_oct, make_thresh(8), make_thresh(10)));
            active_rem =
                choose(mode_hex, make_rem(16), choose(mode_oct, make_rem(8), make_rem(10)));
        } else {
            digit_decode(byte, fixed_radix, is_digit, raw_digit_value);
            active_radix = build_int_cstu(unsigned_type, fixed_radix);
            if (is_unsigned) {
                active_thresh = build_int_cstu(unsigned_type, unsigned_limit / fixed_radix);
                active_rem = build_int_cstu(unsigned_type, unsigned_limit % fixed_radix);
            } else {
                active_thresh = choose(negative,
                                       build_int_cstu(unsigned_type, negative_limit / fixed_radix),
                                       build_int_cstu(unsigned_type, positive_limit / fixed_radix));
                active_rem = choose(negative,
                                    build_int_cstu(unsigned_type, negative_limit % fixed_radix),
                                    build_int_cstu(unsigned_type, positive_limit % fixed_radix));
            }
        }

        // Only consume digit if running, not skipping whitespace, not taking sign, not consuming
        // 'x'/'X'.
        tree try_digit =
            both(running, both(negate(skip), both(negate(take_sign), negate(consume_x))));
        tree take_digit = both(try_digit, is_digit);

        tree next_negative = choose(take_sign, minus, negative);
        tree next_radix_mode = radix_mode;
        if (auto_base) {
            next_radix_mode =
                choose(consume_x,
                       build_int_cst(integer_type_node, 2),
                       choose(start_zero, build_int_cst(integer_type_node, 1), radix_mode));
        }

        tree next_phase = choose(skip,
                                 build_zero_cst(integer_type_node),
                                 choose(take_sign,
                                        build_int_cst(integer_type_node, 1),
                                        choose(start_zero,
                                               build_int_cst(integer_type_node, 2),
                                               choose(either(consume_x, take_digit),
                                                      build_int_cst(integer_type_node, 3),
                                                      build_int_cst(integer_type_node, 4)))));

        tree digit_value = fold_convert(unsigned_type, raw_digit_value);
        tree safe = either(binary(LT_EXPR, magnitude, active_thresh),
                           both(binary(EQ_EXPR, magnitude, active_thresh),
                                binary(LE_EXPR, digit_value, active_rem)));
        tree next = fold_build2_loc(
            location,
            PLUS_EXPR,
            unsigned_type,
            fold_build2_loc(location, MULT_EXPR, unsigned_type, magnitude, active_radix),
            digit_value);
        tree next_overflow = boolean_false_node;
        if (is_ato) {
            next = choose(take_digit,
                          choose(safe, next, missing_string_terminator(unsigned_type, location)),
                          magnitude);
        } else {
            next_overflow = either(overflow, both(take_digit, negate(safe)));
            tree keep_update = both(take_digit, both(safe, negate(overflow)));
            next = choose(keep_update, next, magnitude);
        }

        uint64_t step_radix = auto_base ? 16 : fixed_radix;
        uint64_t step_max_digit = step_radix - 1;
        bound = bound > (limit - step_max_digit) / step_radix ? limit
                                                              : bound * step_radix + step_max_digit;
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
        if (!is_ato) {
            overflow = fold_convert(
                boolean_type_node,
                quantum_value_capture(
                    call, next_overflow, "__cket_decimal_ovf_", prefix, boolean_type_node));
        }
        if (auto_base) {
            radix_mode = fold_convert(
                integer_type_node,
                quantum_value_capture(
                    call, next_radix_mode, "__cket_decimal_radix_", prefix, radix_mode_type));
        }
        if (end_decl) {
            tree next_end =
                choose(take_digit, build_int_cstu(integer_type_node, i + 1), end_offset);
            unsigned end_bits = 1;
            while (end_bits < 64 && (uint64_t{1} << end_bits) <= i + 1)
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

    tree terminated = byte_range_terminated(source, location);
    if (end_decl) {
        QuantumStringSearch search;
        search.source = source;
        search.source.stop_at_zero = true;
        search.source.cpu_bytes.clear();
        search.positions = count;
        search.position = fold_convert(size_type_node, end_offset);
        search.found = boolean_true_node;
        search.defined = terminated;
        quantum_search_pointers[end_decl] = search;
        DECL_INITIAL(end_decl) = NULL_TREE;
        procedure_cpu_flags.insert(end_decl);
    }

    tree result = NULL_TREE;
    if (is_unsigned) {
        tree mag_in_target = fold_convert(type, magnitude);
        tree negated = fold_build1_loc(location, NEGATE_EXPR, type, mag_in_target);
        result = choose(overflow, TYPE_MAX_VALUE(type), choose(negative, negated, mag_in_target));
    } else {
        tree signed_value = fold_convert(type, magnitude);
        tree negated =
            choose(binary(EQ_EXPR, magnitude, build_int_cstu(unsigned_type, negative_limit)),
                   TYPE_MIN_VALUE(type),
                   fold_build1_loc(location, NEGATE_EXPR, type, signed_value));
        result = choose(negative, negated, signed_value);
        if (!is_ato) {
            tree sat = choose(negative, TYPE_MIN_VALUE(type), TYPE_MAX_VALUE(type));
            result = choose(overflow, sat, result);
        }
    }
    result = choose(terminated, result, missing_string_terminator(type, location));
    return make_quantum_value_program(call, {}, boolean_true_node, std::move(prefix), result);
}
