bool same_search_object(const QuantumByteRange &a, const QuantumByteRange &b) {
    if (a.quantum || b.quantum)
        return a.quantum && b.quantum && a.elements == b.elements && a.stride == b.stride;
    // Equal byte values do not establish pointer provenance: distinct arrays
    // may have identical contents while their subtraction remains undefined.
    return a.cpu_object && a.cpu_object == b.cpu_object;
}

tree search_pointer_call(tree pointer) {
    pointer = strip(pointer);
    while (pointer && TREE_CODE(pointer) == SAVE_EXPR)
        pointer = strip(TREE_OPERAND(pointer, 0));
    if (quantum_search_pointers.count(pointer))
        return pointer;
    return pointer && TREE_CODE(pointer) == CALL_EXPR &&
                   !standard_string_search(get_callee_fndecl(pointer)).empty()
               ? pointer
               : NULL_TREE;
}

[[maybe_unused]] tree
search_absolute_position(const QuantumStringSearch &search, tree type, location_t location) {
    return fold_build2_loc(location,
                           PLUS_EXPR,
                           type,
                           fold_convert(type, search.position),
                           build_int_cstu(type, search.source.offset));
}

static bool search_read_pointer_full(tree value,
                                     QuantumStringSearch &search,
                                     int64_t &delta,
                                     tree &dynamic_delta,
                                     tree &dynamic_defined,
                                     bool &had_error);

static bool search_append_offset_step(location_t loc,
                                      tree orig_arg,
                                      bool from_pointer_plus,
                                      const QuantumStringSearch &search,
                                      int64_t &delta,
                                      tree &dynamic_delta,
                                      tree &dynamic_defined,
                                      bool &had_error,
                                      const char *kind_label) {
    (void)search;
    auto fail = [&](const char *msg) {
        error_at(loc, "%s", msg);
        had_error = true;
        return true;
    };
    tree folded = strip(fold(orig_arg));
    if (folded && TREE_CODE(folded) == INTEGER_CST && TYPE_PRECISION(TREE_TYPE(folded)) <= 64) {
        if (!from_pointer_plus && !tree_fits_shwi_p(folded)) {
            dynamic_defined = boolean_false_node;
            return true;
        }
        int64_t step = from_pointer_plus ? static_cast<int64_t>(TREE_INT_CST_LOW(folded))
                                         : tree_to_shwi(folded);
        if ((step > 0 && delta > INT64_MAX - step) || (step < 0 && delta < INT64_MIN - step)) {
            std::string msg = std::string("|c>: search-pointer ") + kind_label +
                              " overflows its checked representation";
            return fail(msg.c_str());
        }
        delta += step;
        return true;
    }
    tree raw_step = expand_quantum_program(orig_arg);
    if (!raw_step || !INTEGRAL_TYPE_P(TREE_TYPE(raw_step)) ||
        TYPE_PRECISION(TREE_TYPE(raw_step)) > 64 || !quantum_pure_expression(raw_step)) {
        std::string msg = std::string("|c>: search-pointer ") + kind_label +
                          " requires a pure integer expression";
        return fail(msg.c_str());
    }
    bool negate_after = false;
    tree inner = raw_step;
    while (TREE_CODE(inner) == SAVE_EXPR && TREE_OPERAND(inner, 0))
        inner = TREE_OPERAND(inner, 0);
    if (from_pointer_plus && TREE_CODE(inner) == NEGATE_EXPR && TYPE_UNSIGNED(TREE_TYPE(inner))) {
        negate_after = true;
        inner = TREE_OPERAND(inner, 0);
    }
    if (from_pointer_plus && (TREE_CODE(inner) == NOP_EXPR || TREE_CODE(inner) == CONVERT_EXPR) &&
        (TREE_TYPE(inner) == sizetype || TYPE_MAIN_VARIANT(TREE_TYPE(inner)) == size_type_node)) {
        tree under = TREE_OPERAND(inner, 0);
        if (under && INTEGRAL_TYPE_P(TREE_TYPE(under)) && TYPE_PRECISION(TREE_TYPE(under)) <= 64)
            inner = under;
    }
    if (!has_quantum(inner) && TREE_CODE(strip(inner)) != INTEGER_CST) {
        inner = save_expr(inner);
        cpu_library_saves.insert(inner);
    }
    tree i64_type = long_long_integer_type_node;
    tree inner_type = TREE_TYPE(inner);
    unsigned prec = TYPE_PRECISION(inner_type);
    const int64_t max_step = int64_t{1} << 48;
    tree step_ok = boolean_true_node;
    if (prec >= 48) {
        if (TYPE_UNSIGNED(inner_type)) {
            step_ok = fold_build2_loc(loc,
                                      LE_EXPR,
                                      boolean_type_node,
                                      inner,
                                      build_int_cstu(inner_type, static_cast<uint64_t>(max_step)));
        } else {
            tree lo = fold_build2_loc(
                loc, GE_EXPR, boolean_type_node, inner, build_int_cst(inner_type, -max_step));
            tree hi = fold_build2_loc(
                loc, LE_EXPR, boolean_type_node, inner, build_int_cst(inner_type, max_step));
            step_ok = fold_build2_loc(loc, TRUTH_ANDIF_EXPR, boolean_type_node, lo, hi);
        }
    }
    tree step_i64 = fold_convert(i64_type, inner);
    if (negate_after)
        step_i64 = fold_build1_loc(loc, NEGATE_EXPR, i64_type, step_i64);
    dynamic_defined =
        fold_build2_loc(loc, TRUTH_ANDIF_EXPR, boolean_type_node, dynamic_defined, step_ok);
    dynamic_delta = dynamic_delta
                        ? fold_build2_loc(loc, PLUS_EXPR, i64_type, dynamic_delta, step_i64)
                        : step_i64;
    return true;
}

static tree search_signed_position(location_t loc,
                                   const QuantumStringSearch &search,
                                   int64_t delta,
                                   tree dynamic_delta,
                                   tree dynamic_defined,
                                   uint64_t inclusive_end,
                                   tree &in_bounds_out) {
    tree i64_type = long_long_integer_type_node;
    if (delta > INT64_MAX - static_cast<int64_t>(search.source.size()) ||
        delta < INT64_MIN + static_cast<int64_t>(search.source.size()) + 1) {
        in_bounds_out = boolean_false_node;
        return build_zero_cst(i64_type);
    }
    int64_t base_offset = static_cast<int64_t>(search.source.offset) + delta;
    tree pos = fold_build2_loc(loc,
                               PLUS_EXPR,
                               i64_type,
                               fold_convert(i64_type, search.position),
                               build_int_cst(i64_type, base_offset));
    if (dynamic_delta)
        pos = fold_build2_loc(loc, PLUS_EXPR, i64_type, pos, dynamic_delta);
    tree ge_zero = fold_build2_loc(loc, GE_EXPR, boolean_type_node, pos, build_zero_cst(i64_type));
    tree le_end =
        fold_build2_loc(loc,
                        inclusive_end ? LE_EXPR : LT_EXPR,
                        boolean_type_node,
                        pos,
                        build_int_cst(i64_type, static_cast<int64_t>(search.source.size())));
    in_bounds_out =
        fold_build2_loc(loc,
                        TRUTH_ANDIF_EXPR,
                        boolean_type_node,
                        dynamic_defined ? dynamic_defined : boolean_true_node,
                        fold_build2_loc(loc, TRUTH_ANDIF_EXPR, boolean_type_node, ge_zero, le_end));
    return pos;
}

// GCC supplies a byte difference here and divides by the pointed-to element
// size outside this node. Preserve that separation for casts to wider types.
tree expand_search_pointer_difference(tree value) {
    tree left = strip(TREE_OPERAND(value, 0)), right = strip(TREE_OPERAND(value, 1));
    QuantumStringSearch search, other;
    int64_t delta = 0, other_delta = 0;
    tree dyn = NULL_TREE, other_dyn = NULL_TREE;
    tree def = boolean_true_node, other_def = boolean_true_node;
    bool err = false, other_err = false;
    bool reverse = false;
    if (!search_read_pointer_full(left, search, delta, dyn, def, err)) {
        if (!search_read_pointer_full(right, search, delta, dyn, def, err))
            return NULL_TREE;
        reverse = true;
    }
    auto reject = [value](const char *message) -> tree {
        error_at(at(value), "%s", message);
        return build_zero_cst(TREE_TYPE(value));
    };
    if (err)
        return build_zero_cst(TREE_TYPE(value));
    tree other_pointer = reverse ? left : right;
    if (search_read_pointer_full(
            other_pointer, other, other_delta, other_dyn, other_def, other_err)) {
        if (other_err)
            return build_zero_cst(TREE_TYPE(value));
        tree type = TREE_TYPE(value);
        if (!same_search_object(search.source, other.source) || !INTEGRAL_TYPE_P(type) ||
            TYPE_UNSIGNED(type) || !TYPE_PRECISION(type) || TYPE_PRECISION(type) > 64)
            return reject(
                "|c>: saved quantum search subtraction needs the same checked object and a signed difference type");
        uint64_t maximum = TYPE_PRECISION(type) == 64
                               ? static_cast<uint64_t>(INT64_MAX)
                               : (uint64_t{1} << (TYPE_PRECISION(type) - 1)) - 1;
        if (search.source.size() > maximum || other.source.size() > maximum)
            return reject(
                "|c>: quantum search subtraction exceeds its signed difference representation");
        tree ok_a = NULL_TREE, ok_b = NULL_TREE;
        tree a = search_signed_position(at(value), search, delta, dyn, def, 1, ok_a);
        tree b =
            search_signed_position(at(value), other, other_delta, other_dyn, other_def, 1, ok_b);
        tree both_ok = fold_build2_loc(at(value), TRUTH_ANDIF_EXPR, boolean_type_node, ok_a, ok_b);
        search.defined = fold_build2_loc(
            at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.defined, both_ok);
        tree difference = fold_convert(type,
                                       fold_build2_loc(at(value),
                                                       MINUS_EXPR,
                                                       long_long_integer_type_node,
                                                       reverse ? b : a,
                                                       reverse ? a : b));
        difference = string_search_domain(other, difference, at(value), true);
        return string_search_domain(search, difference, at(value), true);
    }
    QuantumByteRange base;
    if (!resolve_byte_range(reverse ? left : right, base) ||
        !same_search_object(search.source, base))
        return reject(
            "|c>: quantum search pointer subtraction requires the same checked storage object");
    tree type = TREE_TYPE(value);
    if (!INTEGRAL_TYPE_P(type) || TYPE_UNSIGNED(type) || !TYPE_PRECISION(type) ||
        TYPE_PRECISION(type) > 64)
        return reject(
            "|c>: quantum search subtraction requires a supported signed pointer-difference type");
    uint64_t maximum = TYPE_PRECISION(type) == 64 ? (uint64_t)INT64_MAX
                                                  : (uint64_t{1} << (TYPE_PRECISION(type) - 1)) - 1;
    if (search.source.size() > maximum || base.offset > maximum)
        return reject(
            "|c>: quantum search subtraction exceeds its signed difference representation");
    tree ok_a = NULL_TREE;
    tree abs_a = search_signed_position(at(value), search, delta, dyn, def, 1, ok_a);
    search.defined =
        fold_build2_loc(at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.defined, ok_a);
    tree difference = fold_build2_loc(
        at(value),
        MINUS_EXPR,
        long_long_integer_type_node,
        abs_a,
        build_int_cst(long_long_integer_type_node, static_cast<int64_t>(base.offset)));
    if (reverse)
        difference =
            fold_build1_loc(at(value), NEGATE_EXPR, long_long_integer_type_node, difference);
    difference = fold_convert(type, difference);
    // NULL subtraction is undefined. A surrounding conditional can keep that
    // branch unevaluated; a failed search must never become offset zero.
    return string_search_domain(search, difference, at(value), true);
}

tree expand_search_pointer_comparison(tree value) {
    tree left = strip(TREE_OPERAND(value, 0)), right = strip(TREE_OPERAND(value, 1));
    if (!left || !right || integer_zerop(left) || integer_zerop(right))
        return NULL_TREE; // The existence-only adapter handles NULL.
    QuantumStringSearch search, other;
    int64_t delta = 0, other_delta = 0;
    tree dyn = NULL_TREE, other_dyn = NULL_TREE;
    tree def = boolean_true_node, other_def = boolean_true_node;
    bool err = false, other_err = false;
    bool reverse = false;
    if (!search_read_pointer_full(left, search, delta, dyn, def, err)) {
        if (!search_read_pointer_full(right, search, delta, dyn, def, err))
            return NULL_TREE;
        reverse = true;
    }
    if (err)
        return build_zero_cst(TREE_TYPE(value));
    tree other_pointer = reverse ? left : right;
    if (search_read_pointer_full(
            other_pointer, other, other_delta, other_dyn, other_def, other_err)) {
        if (other_err)
            return build_zero_cst(TREE_TYPE(value));
        if (!same_search_object(search.source, other.source)) {
            error_at(
                at(value),
                "|c>: saved quantum search comparison requires the same checked storage object");
            return build_zero_cst(TREE_TYPE(value));
        }
        enum tree_code relation = TREE_CODE(value);
        bool equality = relation == EQ_EXPR || relation == NE_EXPR;
        tree ok_a = NULL_TREE, ok_b = NULL_TREE;
        tree a = search_signed_position(at(value), search, delta, dyn, def, 1, ok_a);
        tree b =
            search_signed_position(at(value), other, other_delta, other_dyn, other_def, 1, ok_b);
        search.defined = fold_build2_loc(
            at(value),
            TRUTH_ANDIF_EXPR,
            boolean_type_node,
            search.defined,
            fold_build2_loc(at(value), TRUTH_ANDIF_EXPR, boolean_type_node, ok_a, ok_b));
        tree result = fold_build2_loc(at(value),
                                      equality ? EQ_EXPR : relation,
                                      boolean_type_node,
                                      reverse ? b : a,
                                      reverse ? a : b);
        if (equality) {
            tree both_found = fold_build2_loc(
                at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.found, other.found);
            tree either_found = fold_build2_loc(
                at(value), TRUTH_ORIF_EXPR, boolean_type_node, search.found, other.found);
            tree both_missing =
                fold_build1_loc(at(value), TRUTH_NOT_EXPR, boolean_type_node, either_found);
            result =
                fold_build2_loc(at(value), TRUTH_ANDIF_EXPR, boolean_type_node, both_found, result);
            result = fold_build2_loc(
                at(value), TRUTH_ORIF_EXPR, boolean_type_node, both_missing, result);
            if (relation == NE_EXPR)
                result = fold_build1_loc(at(value), TRUTH_NOT_EXPR, boolean_type_node, result);
        }
        result = string_search_domain(other, result, at(value), !equality);
        return fold_convert(TREE_TYPE(value),
                            string_search_domain(search, result, at(value), !equality));
    }
    QuantumByteRange base;
    if (!resolve_byte_range(reverse ? left : right, base) ||
        !same_search_object(search.source, base)) {
        error_at(at(value),
                 "|c>: quantum search address comparison requires the same checked storage object");
        return build_zero_cst(TREE_TYPE(value));
    }
    tree ok_a = NULL_TREE;
    tree absolute = search_signed_position(at(value), search, delta, dyn, def, 1, ok_a);
    search.defined =
        fold_build2_loc(at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.defined, ok_a);
    tree address = build_int_cst(long_long_integer_type_node, static_cast<int64_t>(base.offset));
    enum tree_code relation = TREE_CODE(value);
    tree result = fold_build2_loc(at(value),
                                  relation,
                                  boolean_type_node,
                                  reverse ? address : absolute,
                                  reverse ? absolute : address);
    bool equality = relation == EQ_EXPR || relation == NE_EXPR;
    if (equality) {
        tree found = search.found;
        if (relation == NE_EXPR)
            found = fold_build1_loc(at(value), TRUTH_NOT_EXPR, boolean_type_node, found);
        result = fold_build2_loc(at(value),
                                 relation == EQ_EXPR ? TRUTH_ANDIF_EXPR : TRUTH_ORIF_EXPR,
                                 boolean_type_node,
                                 found,
                                 result);
    }
    return fold_convert(TREE_TYPE(value),
                        string_search_domain(search, result, at(value), !equality));
}
