// A null comparison asks only whether a search succeeds. Lower that result
// directly to a Boolean circuit; no host or quantum pointer is materialized.
tree expand_string_search_relation(tree value) {
    enum tree_code relation = TREE_CODE(value);
    if (relation != EQ_EXPR && relation != NE_EXPR)
        return NULL_TREE;
    tree call = strip(TREE_OPERAND(value, 0)), zero = strip(TREE_OPERAND(value, 1));
    if (call && integer_zerop(call))
        std::swap(call, zero);
    if (!call || TREE_CODE(call) != CALL_EXPR || !zero || !integer_zerop(zero))
        return NULL_TREE;
    std::string operation = standard_string_search(get_callee_fndecl(call));
    if (operation.empty())
        return NULL_TREE;
    bool memory = operation == "memchr";
    bool set_search = operation == "strpbrk", substring = operation == "strstr";
    if (call_expr_nargs(call) != (memory ? 3 : 2))
        return NULL_TREE;

    QuantumByteRange string, pattern;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), string);
    bool resolved_pattern =
        !(set_search || substring) || resolve_byte_range(CALL_EXPR_ARG(call, 1), pattern);
    if (!string.quantum && !pattern.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)) &&
        !has_quantum(CALL_EXPR_ARG(call, 1)))
        return NULL_TREE;
    auto reject = [value](const char *message) -> tree {
        error_at(at(value), "%s", message);
        return build_zero_cst(TREE_TYPE(value));
    };
    if (!resolved || !resolved_pattern)
        return reject(
            !resolved && string.error ? string.error
            : !resolved_pattern && pattern.error
                ? pattern.error
                : "|c>: quantum search needs checked quantum bytes or known CPU object storage");
    uint64_t count = string.size() - string.offset;
    uint64_t pattern_count = set_search || substring ? pattern.size() - pattern.offset : 0;
    if (memory) {
        tree length = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!length || TREE_CODE(length) != INTEGER_CST || !tree_fits_uhwi_p(length))
            return reject("|c>: quantum memchr requires a constant checked byte count");
        uint64_t requested = tree_to_uhwi(length);
        if (requested > count)
            return reject("|c>: quantum memchr exceeds its checked storage bounds");
        count = requested;
    } else if (!count || ((set_search || substring) && !pattern_count))
        return reject("|c>: quantum string search needs a NUL inside each checked string view");
    if (count > 64 || pattern_count > 64)
        return reject("|c>: quantum search exceeds the 64-byte expression synthesis bound");
    string.stop_at_zero = !memory;
    pattern.stop_at_zero = true;
    tree character = NULL_TREE;
    if (!set_search && !substring) {
        // memchr converts to unsigned char; strchr/strrchr convert to char.
        // Compare the resulting object byte so signed target chars keep their
        // implementation-defined integer conversion before the byte view.
        tree argument = expand_quantum_program(CALL_EXPR_ARG(call, 1));
        if (!quantum_pure_expression(argument))
            return reject(
                "|c>: quantum character search requires a pure character argument; sequence CPU effects before the call");
        character = fold_convert(
            integer_type_node,
            fold_convert(
                unsigned_char_type_node,
                fold_convert(memory ? unsigned_char_type_node : char_type_node, argument)));
    }
    location_t location = at(call);
    auto both = [location](tree a, tree b) {
        return fold_build2_loc(location, TRUTH_ANDIF_EXPR, boolean_type_node, a, b);
    };
    auto either = [location](tree a, tree b) {
        return fold_build2_loc(location, TRUTH_ORIF_EXPR, boolean_type_node, a, b);
    };
    auto nonzero = [location](tree byte) {
        return fold_build2_loc(
            location, NE_EXPR, boolean_type_node, byte, build_zero_cst(integer_type_node));
    };
    tree found = boolean_false_node;
    for (uint64_t i = count; i; --i) {
        tree byte = object_byte(string, i - 1, location), match;
        if (substring) {
            match = boolean_false_node;
            for (uint64_t j = pattern_count; j; --j) {
                tree wanted = object_byte(pattern, j - 1, location);
                tree empty =
                    fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, nonzero(wanted));
                if (j <= count - (i - 1)) {
                    tree actual = object_byte(string, i + j - 2, location);
                    tree equal =
                        fold_build2_loc(location, EQ_EXPR, boolean_type_node, actual, wanted);
                    match = either(empty, both(nonzero(actual), both(equal, match)));
                } else
                    match = empty;
            }
        } else if (set_search)
            match = byte_range_member(pattern, byte, location);
        else
            match = fold_build2_loc(location, EQ_EXPR, boolean_type_node, byte, character);
        if (memory)
            found = either(match, found);
        else if (set_search || substring)
            found = both(nonzero(byte), either(match, found));
        else
            found = either(match, both(nonzero(byte), found));
    }
    if (substring) {
        tree first = object_byte(pattern, 0, location);
        tree empty = fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, nonzero(first));
        found = either(empty, found);
    }
    if (!memory) {
        tree terminated = byte_range_terminated(string, location);
        if (set_search || substring)
            terminated = both(terminated, byte_range_terminated(pattern, location));
        found =
            fold_build3_loc(location,
                            COND_EXPR,
                            boolean_type_node,
                            terminated,
                            found,
                            fold_convert(boolean_type_node,
                                         missing_string_terminator(integer_type_node, location)));
    }
    if (relation == EQ_EXPR)
        found = fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, found);
    return fold_convert(TREE_TYPE(value), found);
}
