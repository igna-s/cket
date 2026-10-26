// Ordered private computations and retained scalar updates shared by
// memory, library and indexed-value lowering. No host placeholder is read.

tree missing_string_terminator(tree type, location_t location);

tree quantum_value_capture(tree call,
                           tree expression,
                           const char *name,
                           std::vector<tree> &prefix,
                           tree value_type = unsigned_char_type_node) {
    tree type = build_qualified_type(value_type, TYPE_QUAL_VOLATILE);
    std::string identifier = std::string(name) + std::to_string(next_id);
    tree decl = build_decl(at(call), VAR_DECL, get_identifier(identifier.c_str()), type);
    DECL_CONTEXT(decl) = active_function;
    DECL_ATTRIBUTES(decl) = tree_cons(get_identifier("cket_quant"), NULL_TREE, NULL_TREE);
    TREE_THIS_VOLATILE(decl) = TREE_SIDE_EFFECTS(decl) = 1;
    DECL_INITIAL(decl) = build_zero_cst(type);
    // The zero preparation is separate from the circuit-domain expression,
    // including when that expression uses only captured CPU bytes.
    record_decl(decl, nullptr);
    variables.at(decl).private_capture = true;
    prefix.push_back(build1(DECL_EXPR, void_type_node, decl));
    prefix.push_back(build2_loc(at(call), MODIFY_EXPR, type, decl, fold_convert(type, expression)));
    return fold_convert(value_type, decl);
}

tree make_quantum_value_program(tree call,
                                const std::vector<std::pair<tree, tree>> &words,
                                tree defined,
                                std::vector<tree> prefix = {},
                                tree result_value = NULL_TREE,
                                const std::map<tree, tree> &bit_masks = {},
                                const std::map<tree, tree> &quantum_masks = {}) {
    if (!quantum_masks.empty() || words.empty()) {
        // Check the whole call before any destination write, including calls
        // whose first destination mask is zero on some amplitudes.
        tree checked =
            fold_build3_loc(at(call),
                            COND_EXPR,
                            unsigned_char_type_node,
                            defined,
                            build_zero_cst(unsigned_char_type_node),
                            missing_string_terminator(unsigned_char_type_node, at(call)));
        quantum_value_capture(call, checked, "__cket_string_domain_", prefix);
        defined = boolean_true_node;
    }
    QuantumValueProgram operation;
    operation.prefix = std::move(prefix);
    operation.result = result_value;
    std::vector<tree> arguments;
    for (tree step : operation.prefix)
        if (TREE_CODE(step) != DECL_EXPR)
            arguments.push_back(step);
    for (const auto &word : words) {
        tree type = TREE_TYPE(word.first), next = fold_convert(type, word.second);
        if (operation.updates.empty())
            next = fold_build3_loc(at(call),
                                   COND_EXPR,
                                   type,
                                   defined,
                                   next,
                                   missing_string_terminator(type, at(call)));
        tree update = build2_loc(at(call), MODIFY_EXPR, type, word.first, next);
        operation.updates.push_back(update);
        auto mask = bit_masks.find(word.first);
        if (mask != bit_masks.end()) {
            operation.bit_masks.emplace(update, mask->second);
            // Keep metadata-only mask trees reachable by GCC's collector.
            arguments.push_back(mask->second);
        }
        auto selector = quantum_masks.find(word.first);
        if (selector != quantum_masks.end()) {
            operation.quantum_masks.emplace(update, selector->second);
            arguments.push_back(selector->second);
        }
        arguments.push_back(update);
    }
    if (result_value)
        arguments.push_back(result_value);
    tree marker =
        build_fn_decl("__cket_value_program", build_function_type(TREE_TYPE(call), NULL_TREE));
    tree result =
        build_call_expr_loc_array(at(call), marker, (int)arguments.size(), arguments.data());
    quantum_value_programs.emplace(result, std::move(operation));
    return result;
}
