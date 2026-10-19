bool phase_integer_subtree(tree value) {
    auto inspect = [](tree *node, int *walk, void *) -> tree {
        if (TREE_TYPE(*node) && TREE_CODE(TREE_TYPE(*node)) == REAL_TYPE)
            return *node;
        if (DECL_P(*node) || TYPE_P(*node))
            *walk = 0;
        return NULL_TREE;
    };
    return !walk_tree(&value, inspect, nullptr, nullptr);
}

// Pure classical arithmetic for a deferred phase angle. Integer subexpressions
// keep the ordinary typed IR and its C definedness checks. Real nodes retain
// each binary32/binary64 conversion instead of reassociating across it.
bool phase_angle_expression(tree value, std::string &result, tree *prefix, unsigned depth = 0) {
    if (!value || depth > 128 || has_quantum(value) || !quantum_pure_expression(value))
        return false;
    // GCC may wrap an integer subexpression converted to double in its own
    // DIVREM/SHIFT UBSan guard. Serialize the pure operation lazily; its typed
    // integer IR retains those definedness checks in the selected domain.
    // Unsupported expressions still fall back with the original tree intact.
    if (TREE_CODE(value) == COMPOUND_EXPR && gcc_quantum_sanitizer_guard(TREE_OPERAND(value, 0)) &&
        quantum_pure_expression(TREE_OPERAND(value, 1)))
        return phase_angle_expression(TREE_OPERAND(value, 1), result, prefix, depth + 1);
    tree type = TREE_TYPE(value);
    if (INTEGRAL_TYPE_P(type)) {
        if (TYPE_PRECISION(type) > 64 || !phase_integer_subtree(value))
            return false;
        std::set<unsigned> dependencies;
        std::string integer = evaluate_expression(value, dependencies, prefix);
        if (!dependencies.empty())
            return false;
        result = "{\"op\":\"real_from_int\",\"bits\":64,\"args\":[" + integer + "]}";
        return true;
    }
    if (TREE_CODE(type) != REAL_TYPE || (TYPE_PRECISION(type) != 32 && TYPE_PRECISION(type) != 64))
        return false;
    unsigned width = TYPE_PRECISION(type);
    auto code = TREE_CODE(value);
    if (code == REAL_CST) {
        char number[160];
        real_to_decimal(number, TREE_REAL_CST_PTR(value), sizeof number, 0, 1);
        if (!std::isfinite(std::strtod(number, nullptr)))
            return false;
        result = "{\"op\":\"real_const\",\"bits\":" + std::to_string(width) +
                 ",\"value\":" + number + '}';
        return true;
    }
    if (DECL_P(value) || TREE_CODE_CLASS(code) == tcc_reference) {
        tree integer_type = width == 32 ? unsigned_type_node : long_long_unsigned_type_node;
        tree representation = build1_loc(at(value), VIEW_CONVERT_EXPR, integer_type, value);
        result = "{\"op\":\"real_bits\",\"bits\":" + std::to_string(width) + ",\"args\":[" +
                 cpu_parameter(representation) + "]}";
        return true;
    }
    const char *operation = nullptr;
    unsigned arity = 0;
    if (code == FLOAT_EXPR) {
        tree input = TREE_OPERAND(value, 0);
        if (!INTEGRAL_TYPE_P(TREE_TYPE(input)) || TYPE_PRECISION(TREE_TYPE(input)) > 64 ||
            !phase_integer_subtree(input))
            return false;
        std::set<unsigned> dependencies;
        std::string integer = evaluate_expression(input, dependencies, prefix);
        if (!dependencies.empty())
            return false;
        result = "{\"op\":\"real_from_int\",\"bits\":" + std::to_string(width) + ",\"args\":[" +
                 integer + "]}";
        return true;
    }
    if (CONVERT_EXPR_P(value) || code == NON_LVALUE_EXPR) {
        operation = "real_cast";
        arity = 1;
    } else if (code == NEGATE_EXPR || code == ABS_EXPR) {
        operation = code == NEGATE_EXPR ? "real_neg" : "real_abs";
        arity = 1;
    } else if (code == PLUS_EXPR || code == MINUS_EXPR || code == MULT_EXPR || code == RDIV_EXPR) {
        operation = code == PLUS_EXPR    ? "real_add"
                    : code == MINUS_EXPR ? "real_sub"
                    : code == MULT_EXPR  ? "real_mul"
                                         : "real_div";
        arity = 2;
    } else if (code == COND_EXPR) {
        operation = "real_select";
        arity = 3;
    } else if (code == SAVE_EXPR) {
        return phase_angle_expression(TREE_OPERAND(value, 0), result, prefix, depth + 1);
    } else
        return false;
    result = "{\"op\":" + json(operation) + ",\"bits\":" + std::to_string(width) + ",\"args\":[";
    for (unsigned i = 0; i < arity; ++i) {
        if (i)
            result += ',';
        std::string operand;
        if (code == COND_EXPR && i == 0) {
            std::set<unsigned> dependencies;
            if (!phase_integer_subtree(TREE_OPERAND(value, i)))
                return false;
            operand = evaluate_expression(TREE_OPERAND(value, i), dependencies, prefix);
            if (!dependencies.empty())
                return false;
        } else if (!phase_angle_expression(TREE_OPERAND(value, i), operand, prefix, depth + 1))
            return false;
        result += operand;
    }
    result += "]}";
    return true;
}
