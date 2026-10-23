// A finite multiplexer over checked owned registers. Index capture and bounds
// checks belong to the circuit; host placeholders never supply selected data.
tree dynamic_product_access(tree value);

// Recover the element index before GCC's pointer-sized byte scaling. Dividing
// the scaled offset would accept huge indices whose multiplication wrapped.
// Keep the conversions on the original index: they are part of C semantics.
tree unscaled_quantum_pointer_index(tree bytes, uint64_t stride) {
    tree scaled = bytes;
    while (scaled && (TREE_CODE(scaled) == NON_LVALUE_EXPR || CONVERT_EXPR_P(scaled))) {
        tree operand = TREE_OPERAND(scaled, 0);
        if (TREE_CODE(scaled) != NON_LVALUE_EXPR &&
            (!INTEGRAL_TYPE_P(TREE_TYPE(operand)) || !TYPE_UNSIGNED(TREE_TYPE(scaled)) ||
             !TYPE_UNSIGNED(TREE_TYPE(operand)) ||
             TYPE_PRECISION(TREE_TYPE(scaled)) != TYPE_PRECISION(TREE_TYPE(operand))))
            break;
        scaled = operand;
    }
    tree type = TREE_TYPE(scaled);
    if (!INTEGRAL_TYPE_P(type) || !TYPE_UNSIGNED(type) ||
        TYPE_PRECISION(type) != TYPE_PRECISION(sizetype) || TYPE_PRECISION(type) > 64)
        return NULL_TREE;
    if (stride == 1)
        return bytes;
    if (TREE_CODE(scaled) == MULT_EXPR) {
        for (unsigned side = 0; side < 2; ++side) {
            tree factor = TREE_OPERAND(scaled, side);
            if (TREE_CODE(factor) == INTEGER_CST && tree_fits_uhwi_p(factor) &&
                tree_to_uhwi(factor) == stride)
                return TREE_OPERAND(scaled, 1 - side);
        }
    }
    if (TREE_CODE(scaled) == LSHIFT_EXPR) {
        tree shift = TREE_OPERAND(scaled, 1);
        if (TREE_CODE(shift) == INTEGER_CST && tree_fits_uhwi_p(shift) &&
            tree_to_uhwi(shift) < 64 && (uint64_t{1} << tree_to_uhwi(shift)) == stride)
            return TREE_OPERAND(scaled, 0);
    }
    return NULL_TREE;
}

tree coherent_indexed_access(tree original,
                             const std::vector<tree> &elements,
                             tree index,
                             bool readonly) {
    if (elements.empty() || elements.size() > 64 || !INTEGRAL_TYPE_P(TREE_TYPE(index)) ||
        TYPE_PRECISION(TREE_TYPE(index)) > 64 || !quantum_pure_expression(index)) {
        error_at(
            at(original),
            "|c>: coherent indexing requires 1..64 integer elements and a pure integer index of at most 64 bits");
        return build_zero_cst(TREE_TYPE(original));
    }
    tree marker = build_fn_decl("__cket_coherent_storage",
                                build_function_type(TREE_TYPE(original), NULL_TREE));
    tree result = build_call_expr_loc(at(original), marker, 1, index);
    quantum_coherent_accesses.emplace(result, QuantumIndexedAccess{elements, index, readonly});
    return result;
}

tree capture_coherent_index(tree original,
                            const QuantumIndexedAccess &access,
                            std::vector<tree> &prefix) {
    tree type = TYPE_MAIN_VARIANT(TREE_TYPE(access.index));
    tree promoted =
        TYPE_PRECISION(type) < TYPE_PRECISION(integer_type_node) ? integer_type_node : type;
    tree index = fold_convert(promoted, access.index);
    tree valid = fold_build2_loc(at(original),
                                 LT_EXPR,
                                 boolean_type_node,
                                 index,
                                 build_int_cstu(promoted, access.elements.size()));
    if (!TYPE_UNSIGNED(promoted))
        valid = fold_build2_loc(
            at(original),
            TRUTH_ANDIF_EXPR,
            boolean_type_node,
            fold_build2_loc(
                at(original), GE_EXPR, boolean_type_node, index, build_zero_cst(promoted)),
            valid);
    unsigned bits = 1;
    while ((uint64_t{1} << bits) < access.elements.size())
        ++bits;
    tree saved_type = build_nonstandard_integer_type(bits, true);
    tree guarded = fold_build3_loc(at(original),
                                   COND_EXPR,
                                   saved_type,
                                   valid,
                                   fold_convert(saved_type, index),
                                   missing_string_terminator(saved_type, at(original)));
    return quantum_value_capture(original, guarded, "__cket_index_", prefix, saved_type);
}

tree selected_coherent_value(tree original, const QuantumIndexedAccess &access, tree index) {
    tree type = TYPE_MAIN_VARIANT(TREE_TYPE(original));
    tree result = build_zero_cst(type);
    for (size_t i = access.elements.size(); i; --i) {
        tree selected = fold_build2_loc(at(original),
                                        EQ_EXPR,
                                        boolean_type_node,
                                        index,
                                        build_int_cstu(TREE_TYPE(index), i - 1));
        result = fold_build3_loc(at(original),
                                 COND_EXPR,
                                 type,
                                 selected,
                                 fold_convert(type, access.elements[i - 1]),
                                 result);
    }
    return result;
}

tree coherent_index_read(tree selection) {
    auto found = quantum_coherent_accesses.find(selection);
    if (found == quantum_coherent_accesses.end())
        return selection;
    std::vector<tree> prefix;
    tree index = capture_coherent_index(selection, found->second, prefix);
    tree result = selected_coherent_value(selection, found->second, index);
    return make_quantum_value_program(selection, {}, boolean_true_node, std::move(prefix), result);
}

std::string cpu_indexed_expression(tree selection, std::set<unsigned> &deps) {
    const auto &access = quantum_indexed_accesses.at(selection);
    auto proof = quantum_indexed_read_indices.find(selection);
    if (access.elements.empty() || access.elements.size() > 64 ||
        proof == quantum_indexed_read_indices.end() || !proof->second ||
        !quantum_pure_expression(proof->second)) {
        error_at(
            at(selection),
            "|c>: indexed quantum reads require 1..64 scalar elements and a pure CPU element index with checked pointer-byte scaling");
        return "{}";
    }
    tree index = proof->second;
    tree valid = fold_build2_loc(at(selection),
                                 LT_EXPR,
                                 boolean_type_node,
                                 index,
                                 build_int_cstu(TREE_TYPE(index), access.elements.size()));
    tree type = TYPE_MAIN_VARIANT(TREE_TYPE(selection));
    tree guarded = fold_build3_loc(at(selection),
                                   COND_EXPR,
                                   type,
                                   valid,
                                   selected_coherent_value(selection, access, index),
                                   missing_string_terminator(type, at(selection)));
    // Domain checks and inactive arithmetic belong to typed synthesis. Only
    // actual CPU parameter values are captured; no quantum selector is needed.
    ++coherent_expression_depth;
    std::string result = expression(guarded, deps);
    --coherent_expression_depth;
    return result;
}

tree expand_coherent_index_write(tree value) {
    enum tree_code code = TREE_CODE(value);
    bool increment = code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR ||
                     code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR;
    if (code != MODIFY_EXPR && code != INIT_EXPR && !increment)
        return NULL_TREE;
    tree target = strip(TREE_OPERAND(value, 0));
    tree selection = dynamic_product_access(target);
    auto found = quantum_coherent_accesses.find(selection);
    if (found == quantum_coherent_accesses.end())
        return NULL_TREE;
    auto reject = [value](const char *message) -> tree {
        error_at(at(value), "%s", message);
        return TREE_TYPE(value) == void_type_node ? build_empty_stmt(at(value))
                                                  : build_zero_cst(TREE_TYPE(value));
    };
    const auto &access = found->second;
    tree type = TYPE_MAIN_VARIANT(TREE_TYPE(target));
    if (access.readonly || TYPE_READONLY(TREE_TYPE(target)) || TYPE_ATOMIC(TREE_TYPE(target)))
        return reject("|c>: coherent indexed writes require mutable non-atomic storage");
    for (tree element : access.elements)
        if (readonly_quantum_borrows.count(element) || TYPE_READONLY(TREE_TYPE(element)))
            return reject("|c>: coherent indexed write cannot modify a read-only borrow");
    std::vector<tree> prefix;
    tree index = capture_coherent_index(value, access, prefix);
    tree storage_type = unsigned_type_for(type);
    tree previous = NULL_TREE, assigned;
    if (increment) {
        previous = quantum_value_capture(
            value,
            fold_convert(storage_type, selected_coherent_value(target, access, index)),
            "__cket_index_previous_",
            prefix,
            storage_type);
        tree promoted =
            TYPE_PRECISION(type) < TYPE_PRECISION(integer_type_node) ? integer_type_node : type;
        assigned = fold_build2_loc(
            at(value),
            code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR ? MINUS_EXPR : PLUS_EXPR,
            promoted,
            fold_convert(promoted, fold_convert(type, previous)),
            build_one_cst(promoted));
    } else {
        assigned = expand_quantum_program(TREE_OPERAND(value, 1));
        if (!quantum_pure_expression(assigned))
            return reject(
                "|c>: coherent indexed assignment requires an ordered pure value; sequence other effects before the store");
    }
    assigned = fold_convert(storage_type, fold_convert(type, assigned));
    tree captured =
        quantum_value_capture(value, assigned, "__cket_index_write_", prefix, storage_type);
    std::vector<std::pair<tree, tree>> words;
    for (size_t i = 0; i < access.elements.size(); ++i) {
        tree selected = fold_build2_loc(
            at(value), EQ_EXPR, boolean_type_node, index, build_int_cstu(TREE_TYPE(index), i));
        tree next = fold_build3_loc(at(value),
                                    COND_EXPR,
                                    type,
                                    selected,
                                    fold_convert(type, captured),
                                    fold_convert(type, access.elements[i]));
        words.emplace_back(access.elements[i], next);
    }
    bool postfix = code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR;
    tree result = fold_convert(TREE_TYPE(value), fold_convert(type, postfix ? previous : captured));
    return make_quantum_value_program(value, words, boolean_true_node, std::move(prefix), result);
}
