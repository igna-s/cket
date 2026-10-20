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
    return pointer && TREE_CODE(pointer) == CALL_EXPR &&
                   !standard_string_search(get_callee_fndecl(pointer)).empty()
               ? pointer
               : NULL_TREE;
}

// GCC supplies a byte difference here and divides by the pointed-to element
// size outside this node. Preserve that separation for casts to wider types.
tree expand_search_pointer_difference(tree value) {
    tree left = strip(TREE_OPERAND(value, 0)), right = strip(TREE_OPERAND(value, 1));
    tree call = search_pointer_call(left);
    bool reverse = false;
    if (!call) {
        call = search_pointer_call(right);
        reverse = true;
    }
    if (!call)
        return NULL_TREE;
    QuantumStringSearch search;
    if (!expand_string_search(call, search, true))
        return NULL_TREE;
    auto reject = [value](const char *message) -> tree {
        error_at(at(value), "%s", message);
        return build_zero_cst(TREE_TYPE(value));
    };
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
    tree absolute = fold_build2_loc(at(value),
                                    PLUS_EXPR,
                                    type,
                                    fold_convert(type, search.position),
                                    build_int_cstu(type, search.source.offset));
    tree difference =
        fold_build2_loc(at(value), MINUS_EXPR, type, absolute, build_int_cstu(type, base.offset));
    if (reverse)
        difference = fold_build1_loc(at(value), NEGATE_EXPR, type, difference);
    // NULL subtraction is undefined. A surrounding conditional can keep that
    // branch unevaluated; a failed search must never become offset zero.
    return string_search_domain(search, difference, at(value), true);
}

tree expand_search_pointer_comparison(tree value) {
    tree left = strip(TREE_OPERAND(value, 0)), right = strip(TREE_OPERAND(value, 1));
    tree call = search_pointer_call(left);
    bool reverse = false;
    if (!call) {
        call = search_pointer_call(right);
        reverse = true;
    }
    if (!call || !left || !right || integer_zerop(reverse ? left : right))
        return NULL_TREE; // The existence-only adapter handles NULL.
    QuantumStringSearch search;
    if (!expand_string_search(call, search, true))
        return NULL_TREE;
    QuantumByteRange base;
    if (!resolve_byte_range(reverse ? left : right, base) ||
        !same_search_object(search.source, base)) {
        error_at(at(value),
                 "|c>: quantum search address comparison requires the same checked storage object");
        return build_zero_cst(TREE_TYPE(value));
    }
    tree absolute = fold_build2_loc(at(value),
                                    PLUS_EXPR,
                                    size_type_node,
                                    search.position,
                                    build_int_cstu(size_type_node, search.source.offset));
    tree address = build_int_cstu(size_type_node, base.offset);
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
