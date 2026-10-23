// A saved address selects checked object bytes, never a host pointer load.
// Snapshot the assigned value before any candidate word changes. Retained
// scalar histories preserve the selector's original source and overwritten bits.
tree expand_search_byte_write(tree value, tree target, QuantumStringSearch search, int64_t delta) {
    auto reject = [value](const char *message) -> tree {
        error_at(at(value), "%s", message);
        return TREE_TYPE(value) == void_type_node ? build_empty_stmt(at(value))
                                                  : build_zero_cst(TREE_TYPE(value));
    };
    tree type = TREE_TYPE(target), plain = TYPE_MAIN_VARIANT(type);
    if ((plain != char_type_node && plain != signed_char_type_node &&
         plain != unsigned_char_type_node) ||
        TYPE_READONLY(type) || TYPE_VOLATILE(type) || TYPE_ATOMIC(type))
        return reject("|c>: search-pointer writes require mutable nonvolatile character storage");
    if (!search.source.quantum)
        return reject(
            "|c>: a quantum-selected write into CPU storage needs an explicit memory-effect boundary");
    if (TYPE_READONLY(search.source.element_type))
        return reject("|c>: search-pointer write cannot modify a read-only source object");
    enum tree_code code = TREE_CODE(value);
    bool increment = code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR ||
                     code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR;
    bool postfix = code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR;
    std::vector<tree> prefix;
    tree previous = NULL_TREE, assigned;
    if (increment) {
        previous = quantum_value_capture(
            value,
            fold_convert(unsigned_char_type_node, expand_search_pointer_read(target)),
            "__cket_search_previous_",
            prefix);
        tree promoted = fold_convert(integer_type_node, fold_convert(type, previous));
        assigned = fold_build2_loc(
            at(value),
            code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR ? MINUS_EXPR : PLUS_EXPR,
            integer_type_node,
            promoted,
            build_one_cst(integer_type_node));
    } else {
        assigned = expand_quantum_program(TREE_OPERAND(value, 1));
        if (!quantum_pure_expression(assigned))
            return reject(
                "|c>: search-pointer assignment requires a pure value; sequence other effects before the store");
    }
    assigned = fold_convert(unsigned_char_type_node, fold_convert(type, assigned));
    tree captured = quantum_value_capture(value, assigned, "__cket_search_write_", prefix);
    tree capture_assignment = prefix.back();
    QuantumByteRange current = search.source;
    uint64_t origin = current.offset;
    current.offset = 0;
    current.stop_at_zero = false;
    tree valid = boolean_false_node;
    std::vector<std::pair<tree, tree>> words;
    for (uint64_t i = 0; i < search.positions; ++i) {
        uint64_t address = origin + i;
        if ((delta < 0 && (uint64_t)(-(delta + 1)) + 1 > address) ||
            (delta >= 0 && (uint64_t)delta >= current.size() - address))
            continue;
        address = delta < 0 ? address - ((uint64_t)(-(delta + 1)) + 1) : address + (uint64_t)delta;
        tree owner = current.elements[(size_t)(address / current.stride)];
        if (readonly_quantum_borrows.count(owner))
            return reject("|c>: search-pointer write cannot modify a read-only borrow");
        tree selected = fold_build2_loc(at(value),
                                        EQ_EXPR,
                                        boolean_type_node,
                                        search.position,
                                        build_int_cstu(size_type_node, i));
        valid = fold_build2_loc(at(value), TRUTH_ORIF_EXPR, boolean_type_node, selected, valid);
        tree byte = fold_build3_loc(at(value),
                                    COND_EXPR,
                                    integer_type_node,
                                    selected,
                                    fold_convert(integer_type_node, captured),
                                    object_byte(current, address, at(value)));
        string_word_byte(current, address, byte, words, at(value));
    }
    tree defined = fold_build2_loc(
        at(value),
        TRUTH_ANDIF_EXPR,
        boolean_type_node,
        search.defined,
        fold_build2_loc(at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.found, valid));
    tree capture_type = TREE_TYPE(TREE_OPERAND(capture_assignment, 0));
    TREE_OPERAND(capture_assignment, 1) = fold_convert(
        capture_type,
        fold_build3_loc(at(value),
                        COND_EXPR,
                        unsigned_char_type_node,
                        defined,
                        assigned,
                        missing_string_terminator(unsigned_char_type_node, at(value))));
    tree result = fold_convert(
        TREE_TYPE(value), postfix ? fold_convert(type, previous) : fold_convert(type, captured));
    return make_quantum_value_program(value, words, defined, std::move(prefix), result);
}
