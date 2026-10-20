// Build the prefix-length function as pure typed integer/Boolean expressions.
// Both strings and every captured CPU byte remain available to the inverse.
tree byte_range_member(QuantumByteRange &set, tree byte, location_t location) {
    tree member = boolean_false_node;
    uint64_t count = set.size() - set.offset;
    for (uint64_t j = count; j; --j) {
        tree candidate = object_byte(set, j - 1, location);
        tree present = fold_build2_loc(
            location, NE_EXPR, boolean_type_node, candidate, build_zero_cst(integer_type_node));
        tree equal = fold_build2_loc(location, EQ_EXPR, boolean_type_node, byte, candidate);
        tree suffix = fold_build2_loc(location, TRUTH_ORIF_EXPR, boolean_type_node, equal, member);
        member = fold_build2_loc(location, TRUTH_ANDIF_EXPR, boolean_type_node, present, suffix);
    }
    return member;
}

tree expand_string_span(tree call) {
    if (call_expr_nargs(call) != 2)
        return NULL_TREE;
    QuantumByteRange string, set;
    bool resolved_string = resolve_byte_range(CALL_EXPR_ARG(call, 0), string);
    bool resolved_set = resolve_byte_range(CALL_EXPR_ARG(call, 1), set);
    if (!string.quantum && !set.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)) &&
        !has_quantum(CALL_EXPR_ARG(call, 1)))
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!resolved_string || !resolved_set)
        return reject(
            !resolved_string && string.error ? string.error
            : !resolved_set && set.error
                ? set.error
                : "|c>: quantum string span requires checked quantum bytes or known CPU object storage");
    uint64_t string_count = string.size() - string.offset, set_count = set.size() - set.offset;
    if (!string_count || !set_count)
        return reject("|c>: quantum string span needs a NUL inside both checked storage views");
    if (string_count > 64 || set_count > 64)
        return reject("|c>: quantum string span exceeds the 64-byte expression synthesis bound");
    string.stop_at_zero = set.stop_at_zero = true;
    tree fn = get_callee_fndecl(call);
    bool complement = fndecl_built_in_p(fn, BUILT_IN_STRCSPN) || name_of(fn) == "strcspn" ||
                      name_of(fn) == "__cket_strcspn";
    tree type = TREE_TYPE(call), result = build_int_cstu(type, string_count);
    for (uint64_t i = string_count; i; --i) {
        tree byte = object_byte(string, i - 1, at(call));
        tree member = byte_range_member(set, byte, at(call));
        if (complement)
            member = fold_build1_loc(at(call), TRUTH_NOT_EXPR, boolean_type_node, member);
        tree nonzero = fold_build2_loc(
            at(call), NE_EXPR, boolean_type_node, byte, build_zero_cst(integer_type_node));
        tree keep = fold_build2_loc(at(call), TRUTH_ANDIF_EXPR, boolean_type_node, nonzero, member);
        result =
            fold_build3_loc(at(call), COND_EXPR, type, keep, result, build_int_cstu(type, i - 1));
    }
    tree terminated = fold_build2_loc(at(call),
                                      TRUTH_ANDIF_EXPR,
                                      boolean_type_node,
                                      byte_range_terminated(string, at(call)),
                                      byte_range_terminated(set, at(call)));
    return fold_build3_loc(
        at(call), COND_EXPR, type, terminated, result, missing_string_terminator(type, at(call)));
}
