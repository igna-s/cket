// Semantic analysis of genuine GCC C trees. No source/token parser exists here.
std::map<unsigned, unsigned> remaining;
std::set<tree> counted_saves;

struct SavedExpression {
    std::string text;
    std::set<unsigned> sources, parameters;
};

std::map<tree, SavedExpression> expression_saves;
std::map<tree, unsigned> cpu_parameter_ids;
std::vector<tree> cpu_parameter_values;
std::set<unsigned> active_cpu_parameters;
std::set<tree> cpu_library_saves;
unsigned coherent_expression_depth = 0;
std::vector<std::pair<unsigned, unsigned>> controls;
unsigned dynamic_depth = 0;

struct CpuBranch {
    std::vector<std::string> operations;
    std::vector<tree> prefix;
    std::set<unsigned> incoming, allocations, retained;
};

std::vector<CpuBranch *> cpu_branches;
size_t embedded_module_bytes = 0;
tree apply_fn = NULL_TREE, apply_arg_fn = NULL_TREE;
tree apply_values_fn = NULL_TREE;
tree apply_phase_fn = NULL_TREE;
tree apply_index_fn = NULL_TREE;
tree lower_node(tree value);
bool gcc_quantum_sanitizer_guard(tree value);
bool quantum_pure_expression(tree value);

tree selected_storage(tree value) {
    value = strip(value);
    while (value && TREE_CODE(value) == SAVE_EXPR)
        value = strip(TREE_OPERAND(value, 0));
    return value && quantum_indexed_accesses.count(value) ? value : NULL_TREE;
}

bool same_selected_storage(tree value, tree selection) {
    tree other = selected_storage(value);
    if (!other)
        return false;
    if (other == selection)
        return true;
    const auto &a = quantum_indexed_accesses.at(other), &b = quantum_indexed_accesses.at(selection);
    tree ai = a.index, bi = b.index;
    // Independent pure index evaluations can select the same storage. GCC
    // deliberately treats distinct SAVE_EXPR nodes as unequal; inspect their
    // operands only when doing so cannot merge observable CPU effects.
    while (TREE_CODE(ai) == SAVE_EXPR && !TREE_SIDE_EFFECTS(TREE_OPERAND(ai, 0)))
        ai = TREE_OPERAND(ai, 0);
    while (TREE_CODE(bi) == SAVE_EXPR && !TREE_SIDE_EFFECTS(TREE_OPERAND(bi, 0)))
        bi = TREE_OPERAND(bi, 0);
    return a.elements == b.elements && operand_equal_p(ai, bi, 0);
}

tree indexed_delta(tree value, tree selection, unsigned width, bool xor_update) {
    if (TREE_CODE(value) == SAVE_EXPR)
        return indexed_delta(TREE_OPERAND(value, 0), selection, width, xor_update);
    if (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR) {
        if (!INTEGRAL_TYPE_P(TREE_TYPE(value)) || TYPE_PRECISION(TREE_TYPE(value)) < width)
            return NULL_TREE;
        tree inner = indexed_delta(TREE_OPERAND(value, 0), selection, width, xor_update);
        // These conversions preserve all target bits. Keep the offset in
        // unsigned arithmetic so reassociation cannot introduce signed UB.
        return inner;
    }
    if (same_selected_storage(value, selection))
        return build_zero_cst(long_long_unsigned_type_node);
    auto code = TREE_CODE(value);
    if (xor_update ? code != BIT_XOR_EXPR : code != PLUS_EXPR && code != MINUS_EXPR)
        return NULL_TREE;
    tree a = TREE_OPERAND(value, 0), b = TREE_OPERAND(value, 1),
         delta = indexed_delta(a, selection, width, xor_update);
    if (delta && !has_quantum(b))
        return fold_build2(code,
                           long_long_unsigned_type_node,
                           delta,
                           fold_convert(long_long_unsigned_type_node, b));
    if (code == MINUS_EXPR || has_quantum(a))
        return NULL_TREE;
    delta = indexed_delta(b, selection, width, xor_update);
    return delta ? fold_build2(code,
                               long_long_unsigned_type_node,
                               fold_convert(long_long_unsigned_type_node, a),
                               delta)
                 : NULL_TREE;
}

std::string control_json() {
    std::string result = ",\"controls\":[";
    for (size_t i = 0; i < controls.size(); ++i) {
        if (i)
            result += ',';
        result += "{\"id\":" + std::to_string(controls[i].first) +
                  ",\"value\":" + std::to_string(controls[i].second) + '}';
    }
    return result + ']';
}

tree emit_operation(std::string operation, location_t loc, tree argument = NULL_TREE) {
    if (!active_cpu_parameters.empty()) {
        std::set<unsigned> parameters;
        parameters.swap(active_cpu_parameters);
        if (argument || parameters.size() > 128) {
            error_at(loc, "|c>: CPU expression binding exceeds its checked operation budget");
            return build_zero_cst(long_long_unsigned_type_node);
        }
        std::string descriptors = "[";
        std::vector<tree> values;
        for (unsigned id : parameters) {
            tree value = cpu_parameter_values.at(id);
            if (!values.empty())
                descriptors += ',';
            descriptors += "{\"id\":" + std::to_string(id) +
                           ",\"bits\":" + std::to_string(TYPE_PRECISION(TREE_TYPE(value))) +
                           ",\"signed\":" + (TYPE_UNSIGNED(TREE_TYPE(value)) ? "false" : "true") +
                           '}';
            values.push_back(fold_convert(long_long_unsigned_type_node, lower_node(value)));
        }
        operation = "{\"op\":\"qbind\",\"instruction\":" + operation +
                    ",\"parameters\":" + descriptors + "]}";
        instructions.push_back(operation);
        if (!apply_values_fn) {
            apply_values_fn =
                build_fn_decl("__cket_apply_values",
                              build_varargs_function_type_list(long_long_unsigned_type_node,
                                                               build_pointer_type(char_type_node),
                                                               long_long_unsigned_type_node,
                                                               NULL_TREE));
            TREE_PUBLIC(apply_values_fn) = DECL_EXTERNAL(apply_values_fn) =
                DECL_ARTIFICIAL(apply_values_fn) = 1;
        }
        std::vector<tree> arguments = {
            build_string_literal(operation.size() + 1, operation.c_str()), constant(values.size())};
        arguments.insert(arguments.end(), values.begin(), values.end());
        return build_call_expr_loc_array(loc, apply_values_fn, arguments.size(), arguments.data());
    }
    instructions.push_back(operation);
    tree &fn = argument ? apply_arg_fn : apply_fn;
    if (!fn) {
        tree args = tree_cons(NULL_TREE, void_type_node, NULL_TREE);
        if (argument)
            args = tree_cons(NULL_TREE, long_long_unsigned_type_node, args);
        args = tree_cons(NULL_TREE, build_pointer_type(char_type_node), args);
        fn = build_fn_decl(argument ? "__cket_apply_arg" : "__cket_apply",
                           build_function_type(long_long_unsigned_type_node, args));
        TREE_PUBLIC(fn) = DECL_EXTERNAL(fn) = DECL_ARTIFICIAL(fn) = 1;
    }
    tree text = build_string_literal(operation.size() + 1, operation.c_str());
    return argument ? build_call_expr_loc(
                          loc, fn, 2, text, fold_convert(long_long_unsigned_type_node, argument))
                    : build_call_expr_loc(loc, fn, 1, text);
}

// Storage for either CPU arm exists cleanly before dispatch. Only the selected
// arm computes values; inactive storage is never a measurement or reset.
tree emit_cpu_storage(std::string operation, location_t loc, unsigned id, bool allocation) {
    if (cpu_branches.empty() || dynamic_depth > cpu_branches.size())
        return emit_operation(operation, loc);
    auto &branch = *cpu_branches.front();
    // A nested join creates its owner only on the executed CPU path. Retain
    // that owner in the branch instead of using it before dispatch.
    if (!allocation && !branch.incoming.count(id) && !branch.allocations.count(id))
        return emit_operation(operation, loc);
    if (!(allocation ? branch.allocations : branch.retained).insert(id).second)
        return NULL_TREE;
    std::set<unsigned> parameters;
    parameters.swap(active_cpu_parameters);
    tree call = emit_operation(operation, loc);
    instructions.pop_back();
    branch.operations.push_back(operation);
    branch.prefix.push_back(call);
    if (allocation) {
        std::string prepare =
            "{\"op\":\"qprepare\",\"id\":" + std::to_string(id) + ",\"lower\":0,\"upper\":0}";
        tree zero = emit_operation(prepare, loc);
        instructions.pop_back();
        branch.operations.push_back(prepare);
        branch.prefix.push_back(zero);
    }
    active_cpu_parameters.swap(parameters);
    return NULL_TREE;
}

void append(tree *list, tree value) {
    if (value)
        append_to_statement_list(value, list);
}

tree quantum_use(tree *node, int *walk, void *) {
    if (quantum_indexed_accesses.count(*node) || quantum_memory_copies.count(*node))
        return *node;
    if (quantum(*node))
        return *node;
    if (DECL_P(*node) || TYPE_P(*node) ||
        (TREE_CODE(*node) == CALL_EXPR &&
         (intrinsic(*node) == "measure" || intrinsic(*node) == "search")))
        *walk = 0;
    return NULL_TREE;
}

bool has_quantum(tree value) {
    return value && walk_tree(&value, quantum_use, nullptr, nullptr);
}

tree count_uses(tree *node, int *walk, void *) {
    tree value = *node;
    if (TREE_CODE(value) == SAVE_EXPR && !counted_saves.insert(value).second) {
        // GCC shares this node to guarantee exactly one evaluation. Counting
        // each reference would keep already-reset storage artificially live.
        *walk = 0;
    } else if (TREE_CODE(value) == BIND_EXPR) {
        *walk = 0;
        walk_tree(&BIND_EXPR_BODY(value), count_uses, nullptr, nullptr);
    } else if (TREE_CODE(value) == DECL_EXPR) {
        *walk = 0;
        tree decl = DECL_EXPR_DECL(value);
        if (DECL_INITIAL(decl))
            walk_tree(&DECL_INITIAL(decl), count_uses, nullptr, nullptr);
    } else if (quantum_indexed_accesses.count(value)) {
        for (tree storage : quantum_indexed_accesses.at(value).elements)
            ++remaining[variables.at(storage).id];
        *walk = 0;
        tree index = quantum_indexed_accesses.at(value).index;
        walk_tree(&index, count_uses, nullptr, nullptr);
    } else if (quantum(value)) {
        ++remaining[variables.at(value).id];
        *walk = 0;
    } else if (DECL_P(value) || TYPE_P(value))
        *walk = 0;
    return NULL_TREE;
}

void consume(Variable &var) {
    if (remaining[var.id])
        --remaining[var.id];
}

void taint_uniform(unsigned id, std::set<unsigned> &visited) {
    if (!visited.insert(id).second)
        return;
    for (auto &entry : variables)
        if (entry.second.id == id) {
            entry.second.pristine_uniform = false;
            for (unsigned parent : entry.second.dependencies)
                taint_uniform(parent, visited);
        }
}

void taint_uniform(unsigned id) {
    std::set<unsigned> visited;
    taint_uniform(id, visited);
}

std::string type_json(tree type) {
    if (!type || !INTEGRAL_TYPE_P(type))
        return ",\"bits\":1,\"signed\":false";
    return ",\"bits\":" + std::to_string(TYPE_PRECISION(type)) +
           ",\"signed\":" + (TYPE_UNSIGNED(type) ? "false" : "true");
}

std::string cpu_parameter(tree value) {
    auto found = cpu_parameter_ids.find(value);
    if (found == cpu_parameter_ids.end()) {
        unsigned id = cpu_parameter_values.size();
        cpu_parameter_values.push_back(value);
        found = cpu_parameter_ids.emplace(value, id).first;
    }
    active_cpu_parameters.insert(found->second);
    return "{\"op\":\"parameter\",\"id\":" + std::to_string(found->second) +
           type_json(TREE_TYPE(value)) + '}';
}

std::string register_expression(const Variable &var) {
    unsigned active = 0;
    for (uint64_t value = var.range_max; value; value >>= 1)
        ++active;
    return "{\"op\":\"reg\",\"id\":" + std::to_string(var.id) +
           ",\"bits\":" + std::to_string(var.width) +
           ",\"signed\":" + (var.is_signed ? "true" : "false") +
           ",\"active\":" + std::to_string(active) +
           (var.basis_known ? ",\"known\":" + std::to_string(var.lower) : "") + '}';
}

std::string expression(tree value, std::set<unsigned> &deps) {
    if (!value)
        return "{}";
    auto code = TREE_CODE(value);
    // GCC inserts scalar UBSan handlers before this pass. In a coherent arm,
    // pure arithmetic belongs to the circuit, whose backend checks definedness.
    // Executing a host handler would eagerly evaluate an inactive branch.
    if (coherent_expression_depth && code == COMPOUND_EXPR &&
        gcc_quantum_sanitizer_guard(TREE_OPERAND(value, 0)) &&
        quantum_pure_expression(TREE_OPERAND(value, 1)))
        return expression(TREE_OPERAND(value, 1), deps);
    if (code != INTEGER_CST && !has_quantum(value) && INTEGRAL_TYPE_P(TREE_TYPE(value))) {
        tree inner = value;
        while (CONVERT_EXPR_P(inner) || TREE_CODE(inner) == NON_LVALUE_EXPR)
            inner = TREE_OPERAND(inner, 0);
        tree effect_source = inner;
        while (TREE_CODE(effect_source) == SAVE_EXPR)
            effect_source = TREE_OPERAND(effect_source, 0);
        bool effect = TREE_SIDE_EFFECTS(effect_source) || TREE_THIS_VOLATILE(effect_source);
        if (coherent_expression_depth && quantum_pure_expression(value))
            effect = false;
        bool captured = cpu_library_saves.count(inner);
        if (effect && (!captured || !controls.empty() || coherent_expression_depth)) {
            error_at(
                at(value),
                "|c>: CPU effects in quantum expressions require once-only capture outside coherent control");
            return "{}";
        }
        tree leaf = inner;
        while (TREE_CODE(leaf) == SAVE_EXPR)
            leaf = TREE_OPERAND(leaf, 0);
        // Pure arithmetic in a coherent arm is lowered lazily on the quantum
        // side. Eager CPU evaluation could otherwise execute inactive UB.
        if (TREE_CODE(inner) != INTEGER_CST &&
            (!coherent_expression_depth || TREE_CODE(leaf) == VAR_DECL)) {
            if (TYPE_PRECISION(TREE_TYPE(value)) > 64) {
                error_at(at(value),
                         "|c>: CPU integer expression bindings currently require at most 64 bits");
                return "{}";
            }
            return cpu_parameter(value);
        }
    }
    if (code == SAVE_EXPR) {
        auto cached = expression_saves.find(value);
        if (cached == expression_saves.end()) {
            std::set<unsigned> sources;
            std::set<unsigned> previous;
            previous.swap(active_cpu_parameters);
            std::string result = expression(TREE_OPERAND(value, 0), sources);
            SavedExpression saved{result, sources, active_cpu_parameters};
            active_cpu_parameters.insert(previous.begin(), previous.end());
            cached = expression_saves.emplace(value, saved).first;
        }
        deps.insert(cached->second.sources.begin(), cached->second.sources.end());
        active_cpu_parameters.insert(cached->second.parameters.begin(),
                                     cached->second.parameters.end());
        return cached->second.text;
    }
    if (code == NON_LVALUE_EXPR)
        return expression(TREE_OPERAND(value, 0), deps);
    if (CONVERT_EXPR_P(value) && TREE_TYPE(value) == void_type_node)
        return expression(TREE_OPERAND(value, 0), deps);
    if (code == C_MAYBE_CONST_EXPR)
        return expression(C_MAYBE_CONST_EXPR_EXPR(value), deps);
    if (code == INTEGER_CST) {
        std::string val = TYPE_UNSIGNED(TREE_TYPE(value))
                              ? std::to_string(static_cast<uint64_t>(TREE_INT_CST_LOW(value)))
                              : std::to_string(static_cast<int64_t>(TREE_INT_CST_LOW(value)));
        return "{\"op\":\"const\",\"value\":" + val + type_json(TREE_TYPE(value)) + '}';
    }
    if (quantum(value)) {
        auto &var = variables.at(value);
        consume(var);
        deps.insert(var.id);
        if (!var.lowered || var.released || var.moved)
            error_at(at(value), "|c>: use of unavailable, released or moved quantum storage");
        // Range information is valid only until an update/measurement invalidates it.
        return register_expression(var);
    }
    if (code == ABSU_EXPR) {
        // GCC uses this distinct unsigned-result operation when a promoted
        // absolute value is narrowed. Its signed input minimum is representable
        // in the unsigned result (e.g. labs((long)INT_MIN) on this target).
        unsigned width = TYPE_PRECISION(TREE_TYPE(value));
        std::string input = expression(TREE_OPERAND(value, 0), deps);
        input = "{\"op\":\"cast\",\"bits\":" + std::to_string(width) +
                ",\"signed\":true,\"args\":[" + input + "]}";
        return "{\"op\":\"uabs\"" + type_json(TREE_TYPE(value)) + ",\"args\":[" + input + "]}";
    }
    if (code == ABS_EXPR) {
        tree operand = TREE_OPERAND(value, 0);
        unsigned width = std::max((unsigned)TYPE_PRECISION(TREE_TYPE(operand)),
                                  (unsigned)TYPE_PRECISION(integer_type_node));
        // GCC can narrow abs before this pass. Restore integer promotion for
        // a small signed source; abs(INT8_MIN) is defined after promotion.
        std::string type = ",\"bits\":" + std::to_string(width) + ",\"signed\":true";
        std::string input = expression(operand, deps);
        input = "{\"op\":\"cast\"" + type + ",\"args\":[" + input + "]}";
        std::string result = "{\"op\":\"abs\"" + type + ",\"args\":[" + input + "]}";
        return "{\"op\":\"cast\"" + type_json(TREE_TYPE(value)) + ",\"args\":[" + result + "]}";
    }
    const char *op = nullptr;
    int arity = 2;
    if (code == TRUTH_XOR_EXPR) {
        // Logical XOR compares truth values, including non-Boolean operands.
        // Normalize each operand once; reuse existing typed IR operations.
        std::string type = type_json(TREE_TYPE(value));
        std::string left = expression(TREE_OPERAND(value, 0), deps);
        std::string right = expression(TREE_OPERAND(value, 1), deps);
        return "{\"op\":\"ne\"" + type + ",\"args\":[{\"op\":\"not\"" + type + ",\"args\":[" +
               left + "]},{\"op\":\"not\"" + type + ",\"args\":[" + right + "]}]}";
    }
    if (code == MULT_EXPR && TYPE_UNSIGNED(TREE_TYPE(value)) &&
        TYPE_PRECISION(TREE_TYPE(value)) < TYPE_PRECISION(integer_type_node)) {
        unsigned __int128 maximum[2];
        for (unsigned j = 0; j < 2; ++j) {
            tree operand = strip(TREE_OPERAND(value, j));
            if (quantum(operand))
                maximum[j] = variables.at(operand).range_max;
            else if (TREE_CODE(operand) == INTEGER_CST && tree_fits_uhwi_p(operand))
                maximum[j] = tree_to_uhwi(operand);
            else
                maximum[j] = TYPE_PRECISION(TREE_TYPE(operand)) >= 64
                                 ? UINT64_MAX
                                 : ((uint64_t{1} << TYPE_PRECISION(TREE_TYPE(operand))) - 1);
        }
        if (maximum[0] * maximum[1] >
            ((uint64_t{1} << (TYPE_PRECISION(integer_type_node) - 1)) - 1)) {
            error_at(
                at(value),
                "|c>: GCC narrowed a multiplication before the quantum pass; compute a full unsigned result before narrowing to preserve the arithmetic contract");
            return "{}";
        }
    }
    switch (code) {
    case NOP_EXPR:
    case CONVERT_EXPR:
    case VIEW_CONVERT_EXPR:
        op = "cast";
        arity = 1;
        break;
    case PLUS_EXPR:
        op = "add";
        break;
    case MINUS_EXPR:
        op = "sub";
        break;
    case MULT_EXPR:
        op = "mul";
        break;
    case TRUNC_DIV_EXPR:
        op = "div";
        break;
    case TRUNC_MOD_EXPR:
        op = "mod";
        break;
    case BIT_AND_EXPR:
        op = "band";
        break;
    case BIT_IOR_EXPR:
        op = "bor";
        break;
    case BIT_XOR_EXPR:
        op = "bxor";
        break;
    case LSHIFT_EXPR:
        op = "shl";
        break;
    case RSHIFT_EXPR:
        op = "shr";
        break;
    case EQ_EXPR:
        op = "eq";
        break;
    case NE_EXPR:
        op = "ne";
        break;
    case LT_EXPR:
        op = "lt";
        break;
    case LE_EXPR:
        op = "le";
        break;
    case GT_EXPR:
        op = "gt";
        break;
    case GE_EXPR:
        op = "ge";
        break;
    case TRUTH_ANDIF_EXPR:
    case TRUTH_AND_EXPR:
        op = "land";
        break;
    case TRUTH_ORIF_EXPR:
    case TRUTH_OR_EXPR:
        op = "lor";
        break;
    case TRUTH_NOT_EXPR:
        op = "not";
        arity = 1;
        break;
    case COND_EXPR:
        op = "select";
        arity = 3;
        break;
    case COMPOUND_EXPR:
        op = "sequence";
        break;
    case BIT_NOT_EXPR:
        op = "bnot";
        arity = 1;
        break;
    case NEGATE_EXPR:
        op = "neg";
        arity = 1;
        break;
    default:
        error_at(at(value),
                 "|c>: unsupported GCC quantum expression node %qs",
                 get_tree_code_name(code));
        return "{}";
    }
    std::string result = "{\"op\":" + json(op) + type_json(TREE_TYPE(value)) + ",\"args\":[";
    for (int i = 0; i < arity; ++i) {
        if (i)
            result += ',';
        bool coherent_arm =
            i && ((code == COND_EXPR || code == TRUTH_ANDIF_EXPR || code == TRUTH_ORIF_EXPR ||
                   code == TRUTH_AND_EXPR || code == TRUTH_OR_EXPR) &&
                  has_quantum(TREE_OPERAND(value, 0)));
        coherent_expression_depth += coherent_arm;
        result += expression(TREE_OPERAND(value, i), deps);
        coherent_expression_depth -= coherent_arm;
    }
    return result + "]}";
}

std::string inverse_operation(unsigned id, const std::string &expression) {
    return "{\"op\":\"quncompute\",\"id\":" + std::to_string(id) +
           (expression.find("\"op\":\"parameter\"") != std::string::npos
                ? ",\"saved\":true}"
                : ",\"expr\":" + expression + '}');
}

std::string cast_result(std::string expr, Variable &var) {
    return "{\"op\":\"cast\",\"bits\":" + std::to_string(var.width) +
           ",\"signed\":" + (var.is_signed ? "true" : "false") + ",\"args\":[" + expr + "]}";
}

bool dependent(unsigned id) {
    for (auto &entry : variables) {
        auto &var = entry.second;
        if (var.lowered && var.derived && !var.released &&
            (!var.measured || var.pending_destruction) && !var.moved && var.dependencies.count(id))
            return true;
    }
    return false;
}

void cleanup(tree *list) {
    if (dynamic_depth || !controls.empty())
        return;
    bool changed;
    do {
        changed = false;
        for (auto &entry : variables) {
            auto &var = entry.second;
            if (var.lowered && var.optional_definition && !var.released && !var.moved &&
                !var.measured && !var.pending_destruction && !remaining[var.id]) {
                append(list,
                       emit_operation("{\"op\":\"qdispose\",\"id\":" + std::to_string(var.id) + '}',
                                      at(var.decl)));
                var.released = true;
                changed = true;
                continue;
            }
            if (var.lowered && var.history && !var.derived && !var.released && !var.moved &&
                !remaining[var.id] && !dependent(var.id)) {
                append(list,
                       emit_operation("{\"op\":\"qhistory_end\",\"id\":" + std::to_string(var.id) +
                                          '}',
                                      at(var.decl)));
                var.released = true;
                changed = true;
                continue;
            }
            if (var.lowered && var.measured && var.clean && !var.released && !var.moved &&
                !remaining[var.id] && !dependent(var.id)) {
                append(list,
                       emit_operation("{\"op\":\"qrelease\",\"id\":" + std::to_string(var.id) + '}',
                                      at(var.decl)));
                var.released = true;
                changed = true;
                continue;
            }
            if (!var.lowered || !var.derived || var.released || var.measured || var.moved ||
                remaining[var.id] || dependent(var.id))
                continue;
            if (var.optional_definition)
                append(list,
                       emit_operation("{\"op\":\"qdispose\",\"id\":" + std::to_string(var.id) + '}',
                                      at(var.decl)));
            else {
                append(list, emit_operation(inverse_operation(var.id, var.creation), at(var.decl)));
                append(list,
                       emit_operation("{\"op\":\"qrelease\",\"id\":" + std::to_string(var.id) + '}',
                                      at(var.decl)));
            }
            var.released = true;
            changed = true;
        }
    } while (changed);
}

int affine_parity(tree value, unsigned width, tree &source, unsigned depth = 0) {
    if (!value || depth > 128)
        return -1;
    if (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR) {
        if (INTEGRAL_TYPE_P(TREE_TYPE(value)) && TYPE_PRECISION(TREE_TYPE(value)) < width)
            return -1;
        return affine_parity(TREE_OPERAND(value, 0), width, source, depth + 1);
    }
    if (TREE_CODE(value) == INTEGER_CST)
        return 0;
    if (quantum(value)) {
        if (source && source != value)
            return -1;
        source = value;
        return 1;
    }
    auto code = TREE_CODE(value);
    if (code == NEGATE_EXPR || code == BIT_NOT_EXPR)
        return affine_parity(TREE_OPERAND(value, 0), width, source, depth + 1);
    if (code == PLUS_EXPR || code == MINUS_EXPR) {
        int a = affine_parity(TREE_OPERAND(value, 0), width, source, depth + 1);
        int b = affine_parity(TREE_OPERAND(value, 1), width, source, depth + 1);
        return a < 0 || b < 0 ? -1 : a ^ b;
    }
    if (code == MULT_EXPR) {
        tree a = strip(TREE_OPERAND(value, 0)), b = strip(TREE_OPERAND(value, 1));
        tree literal = TREE_CODE(a) == INTEGER_CST ? a : b;
        if (TREE_CODE(literal) != INTEGER_CST)
            return -1;
        int coefficient =
            affine_parity(literal == a ? TREE_OPERAND(value, 1) : TREE_OPERAND(value, 0),
                          width,
                          source,
                          depth + 1);
        return coefficient < 0 ? -1 : coefficient & (TREE_INT_CST_LOW(literal) & 1);
    }
    return -1;
}

bool lossy_assignment(tree value, unsigned width, unsigned depth = 0) {
    if (depth > 128)
        return true;
    while (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR) {
        if (INTEGRAL_TYPE_P(TREE_TYPE(value)) && TYPE_PRECISION(TREE_TYPE(value)) < width)
            return true;
        value = TREE_OPERAND(value, 0);
    }
    auto code = TREE_CODE(value);
    tree affine_source = NULL_TREE;
    int parity = width > 1 ? affine_parity(value, width, affine_source) : -1;
    if (parity >= 0)
        return parity == 0;
    if (width == 1 && dynamic_depth == cpu_branches.size()) {
        if (code == TRUTH_NOT_EXPR ||
            (code == BIT_NOT_EXPR && TYPE_PRECISION(TREE_TYPE(value)) == 1) || quantum(value))
            return false;
        if (code == EQ_EXPR || code == NE_EXPR) {
            tree a = strip(TREE_OPERAND(value, 0)), b = strip(TREE_OPERAND(value, 1));
            tree literal = TREE_CODE(a) == INTEGER_CST ? a : b;
            tree source = literal == a ? b : a;
            bool boolean_view = quantum(source) || (TREE_CODE(source) == BIT_NOT_EXPR &&
                                                    TYPE_PRECISION(TREE_TYPE(source)) == 1 &&
                                                    quantum(strip(TREE_OPERAND(source, 0))));
            if (boolean_view && TYPE_PRECISION(TREE_TYPE(source)) == 1 &&
                TREE_CODE(literal) == INTEGER_CST && tree_fits_uhwi_p(literal) &&
                tree_to_uhwi(literal) <= 1)
                return false;
        }
        return true;
    }
    if (width == 1 && (code == PLUS_EXPR || code == MINUS_EXPR))
        return true;
    if (code == TRUNC_DIV_EXPR || code == LSHIFT_EXPR || code == RSHIFT_EXPR) {
        tree amount = strip(TREE_OPERAND(value, 1));
        if (TREE_CODE(amount) == INTEGER_CST &&
            (code == TRUNC_DIV_EXPR ? integer_onep(amount) : integer_zerop(amount)))
            return false;
        return true;
    }
    if (code == TRUNC_MOD_EXPR)
        return true;
    if (code == BIT_AND_EXPR || code == BIT_IOR_EXPR) {
        tree a = strip(TREE_OPERAND(value, 0)), b = strip(TREE_OPERAND(value, 1));
        tree literal = TREE_CODE(a) == INTEGER_CST ? a : b;
        uint64_t mask = width == 64 ? UINT64_MAX : ((uint64_t{1} << width) - 1);
        if (TREE_CODE(literal) == INTEGER_CST &&
            (code == BIT_AND_EXPR
                 ? (static_cast<uint64_t>(TREE_INT_CST_LOW(literal)) & mask) == mask
                 : integer_zerop(literal)))
            return false;
        return !has_quantum(TREE_OPERAND(value, 0)) || !has_quantum(TREE_OPERAND(value, 1));
    }
    if (code == MULT_EXPR) {
        tree a = strip(TREE_OPERAND(value, 0)), b = strip(TREE_OPERAND(value, 1));
        tree coefficient = TREE_CODE(a) == INTEGER_CST ? a : b;
        return TREE_CODE(coefficient) != INTEGER_CST ||
               (width == 1 ? integer_zerop(coefficient) : !(TREE_INT_CST_LOW(coefficient) & 1));
    }
    if (code == PLUS_EXPR || code == MINUS_EXPR || code == NEGATE_EXPR || code == BIT_NOT_EXPR ||
        code == BIT_XOR_EXPR)
        for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
            if (has_quantum(TREE_OPERAND(value, i)) &&
                lossy_assignment(TREE_OPERAND(value, i), width, depth + 1))
                return true;
    return false;
}

bool references_storage(tree value, tree storage) {
    auto inspect = [](tree *node, int *walk, void *data) -> tree {
        if (*node == static_cast<tree>(data))
            return *node;
        if (DECL_P(*node) || TYPE_P(*node))
            *walk = 0;
        return NULL_TREE;
    };
    return value && walk_tree(&value, inspect, storage, nullptr);
}

std::string select_controls(std::string value,
                            const std::string &inactive,
                            unsigned width,
                            bool is_signed,
                            std::set<unsigned> &deps) {
    for (auto control = controls.rbegin(); control != controls.rend(); ++control) {
        deps.insert(control->first);
        std::string flag = "{\"op\":\"reg\",\"id\":" + std::to_string(control->first) +
                           ",\"bits\":1,\"signed\":false}";
        value = "{\"op\":\"select\",\"bits\":" + std::to_string(width) +
                ",\"signed\":" + (is_signed ? "true" : "false") + ",\"args\":[" + flag + ',' +
                (control->second ? value : inactive) + ',' + (control->second ? inactive : value) +
                "]}";
    }
    return value;
}

void versioned_assignment(Variable &var,
                          const std::string &expr,
                          const std::set<unsigned> &deps,
                          location_t loc,
                          tree *list) {
    if (dynamic_depth > cpu_branches.size()) {
        error_at(
            loc,
            "|c>: versioned assignment needs implemented CPU branch merges or a static per-iteration expansion; CPU-loop history is pending");
        return;
    }
    unsigned old_id = var.id, new_id = next_id++;
    std::set<unsigned> sources = deps;
    std::string value = expr;
    if (!controls.empty()) {
        sources.insert(old_id);
        std::string inactive = register_expression(var);
        value = select_controls(value, inactive, var.width, var.is_signed, sources);
    }
    Variable previous = var;
    previous.history = true;
    previous.name += ".history." + std::to_string(old_id);
    previous.decl =
        build_decl(loc, VAR_DECL, get_identifier(previous.name.c_str()), TREE_TYPE(var.decl));
    variables.emplace(previous.decl, previous);
    remaining[new_id] = remaining[old_id];
    remaining[old_id] = 0;
    std::set<unsigned> parameters;
    parameters.swap(active_cpu_parameters);
    append(list,
           emit_cpu_storage(
               "{\"op\":\"qretain\",\"id\":" + std::to_string(old_id) + '}', loc, old_id, false));
    append(list,
           emit_cpu_storage("{\"op\":\"qalloc\",\"id\":" + std::to_string(new_id) +
                                ",\"width\":" + std::to_string(var.width) + ",\"signed\":" +
                                (var.is_signed ? "true" : "false") + ",\"ancilla\":true}",
                            loc,
                            new_id,
                            true));
    active_cpu_parameters.swap(parameters);
    append(list,
           emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(new_id) +
                              ",\"expr\":" + value + '}',
                          loc));
    var.id = new_id;
    var.derived = true;
    var.optional_definition = !cpu_branches.empty();
    var.history = false;
    var.dependencies = sources;
    var.creation = value;
    var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
    var.measured = var.clean = var.basis_known = var.pristine_uniform = false;
    ++var.epoch;
}

tree lower_node(tree value);

void invalidate_cpu_ranges() {
    for (auto &entry : variables) {
        auto &v = entry.second;
        if (!v.lowered || v.released || v.moved || v.derived)
            continue;
        v.basis_known = v.pristine_uniform = false;
        v.range_max = v.width == 64 ? UINT64_MAX : ((uint64_t{1} << v.width) - 1);
    }
}

void merge_cpu_paths(const std::map<tree, Variable> &left,
                     const std::map<tree, Variable> &right,
                     location_t loc) {
    variables = left;
    for (auto &entry : variables) {
        auto other = right.find(entry.first);
        auto &a = entry.second;
        if (other == right.end()) {
            error_at(loc, "|c>: quantum storage must have a compatible owner on every CPU path");
            continue;
        }
        const auto &b = other->second;
        if (a.id != b.id || a.lowered != b.lowered || a.moved != b.moved ||
            a.released != b.released || a.derived != b.derived || a.dependencies != b.dependencies)
            error_at(loc, "|c>: CPU paths disagree on quantum ownership or inverse dependencies");
        a.measured = a.measured && b.measured;
        a.clean = a.clean && b.clean;
        a.basis_known = a.basis_known && b.basis_known && a.lower == b.lower;
        a.pristine_uniform = a.pristine_uniform && b.pristine_uniform && a.epoch == b.epoch;
        a.range_max = std::max(a.range_max, b.range_max);
        a.epoch = std::max(a.epoch, b.epoch);
    }
}

std::map<unsigned, unsigned> branch_uses(tree value) {
    auto saved_remaining = remaining;
    auto saved_saves = counted_saves;
    remaining.clear();
    counted_saves.clear();
    if (value)
        walk_tree(&value, count_uses, nullptr, nullptr);
    auto result = remaining;
    remaining = std::move(saved_remaining);
    counted_saves = std::move(saved_saves);
    return result;
}

void merge_cpu_versions(const std::map<tree, Variable> &incoming,
                        const std::map<tree, Variable> &left,
                        const std::map<tree, Variable> &right,
                        const std::map<unsigned, unsigned> &after,
                        tree choice,
                        location_t loc,
                        tree *join) {
    variables = incoming;
    bool transferred = false;
    for (auto &entry : variables) {
        auto l = left.find(entry.first), r = right.find(entry.first);
        if (l == left.end() || r == right.end()) {
            error_at(loc, "|c>: CPU paths require compatible scalar owners");
            continue;
        }
        const auto &a = l->second, &b = r->second;
        unsigned uses = after.count(entry.second.id) ? after.at(entry.second.id) : 0;
        if (a.lowered != b.lowered || a.moved != b.moved || a.released != b.released ||
            a.width != b.width || a.is_signed != b.is_signed) {
            error_at(loc, "|c>: CPU paths disagree on quantum ownership");
            continue;
        }
        auto &merged = entry.second;
        if (a.id == b.id) {
            if (a.derived != b.derived || a.dependencies != b.dependencies) {
                error_at(loc, "|c>: CPU paths disagree on inverse dependencies");
                continue;
            }
            merged = a;
            merged.measured &= b.measured;
            merged.clean &= b.clean;
            merged.basis_known &= b.basis_known && a.lower == b.lower;
            merged.pristine_uniform &= b.pristine_uniform && a.epoch == b.epoch;
            merged.range_max = std::max(a.range_max, b.range_max);
            merged.epoch = std::max(a.epoch, b.epoch);
        } else {
            transferred = true;
            if (!a.lowered || a.moved || a.released || a.measured || b.measured) {
                error_at(loc, "|c>: CPU version joins require unmeasured available scalar owners");
                continue;
            }
            unsigned id = next_id++;
            std::string predicate = cpu_parameter(choice);
            append(join,
                   emit_operation("{\"op\":\"qphi\",\"id\":" + std::to_string(id) +
                                      ",\"width\":" + std::to_string(a.width) +
                                      ",\"signed\":" + (a.is_signed ? "true" : "false") +
                                      ",\"sources\":[" + std::to_string(a.id) + ',' +
                                      std::to_string(b.id) + "],\"condition\":" + predicate + '}',
                                  loc));
            merged = a;
            merged.id = id;
            merged.creation.clear();
            merged.dependencies.insert(b.dependencies.begin(), b.dependencies.end());
            merged.derived = a.derived || b.derived;
            merged.optional_definition = true;
            merged.history = false;
            merged.measured = merged.clean = merged.basis_known = merged.pristine_uniform = false;
            merged.range_max = a.width == 64 ? UINT64_MAX : ((uint64_t{1} << a.width) - 1);
            merged.epoch = std::max(a.epoch, b.epoch) + 1;
        }
        remaining[merged.id] = uses;
    }
    std::set<unsigned> owners;
    for (const auto &entry : variables)
        if (!entry.second.moved && !entry.second.released)
            owners.insert(entry.second.id);
    std::map<unsigned, Variable> history;
    for (const auto *path : {&left, &right})
        for (const auto &entry : *path) {
            const auto &var = entry.second;
            if (!var.lowered || var.moved || var.released || owners.count(var.id))
                continue;
            auto found = history.find(var.id);
            if (found == history.end())
                history.emplace(var.id, var);
            else
                found->second.dependencies.insert(var.dependencies.begin(), var.dependencies.end());
        }
    for (auto &entry : history) {
        auto &var = entry.second;
        var.history = true;
        var.optional_definition = true;
        var.name += ".cpu.history." + std::to_string(var.id);
        var.decl = build_decl(loc, VAR_DECL, get_identifier(var.name.c_str()), TREE_TYPE(var.decl));
        variables.emplace(var.decl, var);
        remaining[var.id] = 0;
        const auto &branch = *cpu_branches.front();
        if (branch.incoming.count(var.id) || branch.allocations.count(var.id))
            append(join,
                   emit_cpu_storage("{\"op\":\"qretain\",\"id\":" + std::to_string(var.id) + '}',
                                    loc,
                                    var.id,
                                    false));
    }
    // A transfer can relabel inputs of existing saved definitions. Runtime
    // cleanup follows those exact definitions, including branch-local ones.
    if (transferred)
        for (auto &entry : variables)
            if (entry.second.lowered && !entry.second.moved)
                entry.second.optional_definition = true;
}

tree lower_quantum_decl(Variable &var) {
    if (!controls.empty() || dynamic_depth) {
        error_at(
            at(var.decl),
            "|c>: quantum allocation inside a controlled branch or loop needs a proven lifetime and is not supported yet");
        return NULL_TREE;
    }
    if (saw_return) {
        error_at(at(var.decl), "|c>: quantum preparation after return is unreachable");
        return NULL_TREE;
    }
    tree list = alloc_stmt_list();
    tree init = strip(var.original_init);
    // A change of integer representation computes a value and preserves its
    // source. Only a compatible representation can transfer the owner.
    if (var.expression_init && quantum(init) && variables.at(init).width == var.width &&
        variables.at(init).is_signed == var.is_signed) {
        auto &src = variables.at(init);
        if (!src.lowered || src.released || src.moved ||
            TYPE_MAIN_VARIANT(TREE_TYPE(src.decl)) != TYPE_MAIN_VARIANT(TREE_TYPE(var.decl))) {
            error_at(at(var.decl),
                     "|c>: quantum move requires available storage with the same C type");
            return list;
        }
        consume(src);
        unsigned unused_id = var.id;
        remaining[src.id] += remaining[unused_id];
        remaining[unused_id] = 0;
        var.id = src.id;
        var.lowered = true;
        var.range_max = src.range_max;
        var.derived = src.derived;
        var.creation = src.creation;
        var.dependencies = src.dependencies;
        var.lower = src.lower;
        var.upper = src.upper;
        var.epoch = src.epoch;
        var.basis_known = src.basis_known;
        var.pristine_uniform = src.pristine_uniform;
        var.measured = src.measured;
        var.clean = src.clean;
        src.moved = true;
        append(&list,
               emit_operation("{\"op\":\"qmove\",\"id\":" + std::to_string(var.id) +
                                  ",\"from\":" + json(src.name) + ",\"to\":" + json(var.name) + '}',
                              at(var.decl)));
        DECL_INITIAL(var.decl) = build_zero_cst(TREE_TYPE(var.decl));
        append(&list, build1(DECL_EXPR, void_type_node, var.decl));
        return list;
    }
    // find transfers ownership of its input register: no allocation/copy.
    if (var.expression_init && (intrinsic(init) == "find" || intrinsic(init) == "find_known")) {
        tree source = strip(CALL_EXPR_ARG(init, 0));
        auto found = variables.find(source);
        if (found == variables.end() || !found->second.lowered ||
            TYPE_MAIN_VARIANT(TREE_TYPE(found->second.decl)) !=
                TYPE_MAIN_VARIANT(TREE_TYPE(var.decl)) ||
            dependent(found->second.id)) {
            error_at(at(var.decl),
                     "|c>: find requires an independent quantum input of the same C type");
            return list;
        }
        auto &src = found->second;
        consume(src);
        std::set<unsigned> deps;
        std::string predicate = expression(CALL_EXPR_ARG(init, 1), deps);
        if (deps != std::set<unsigned>{src.id} || src.lower != 0 || src.epoch ||
            !src.pristine_uniform) {
            error_at(
                at(var.decl),
                "|c>: find currently requires an unchanged uniform [0,2^k-1] input and a pure predicate on that input");
            return list;
        }
        unsigned unused_id = var.id;
        remaining[src.id] += remaining[unused_id];
        remaining[unused_id] = 0;
        var.id = src.id;
        var.lowered = true;
        var.range_max = src.range_max;
        src.moved = true;
        uint64_t solutions = 1;
        if (intrinsic(init) == "find_known") {
            tree count = strip(CALL_EXPR_ARG(init, 2));
            if (TREE_CODE(count) != INTEGER_CST || !tree_fits_uhwi_p(count) ||
                !(solutions = tree_to_uhwi(count)) || solutions > src.upper + 1)
                error_at(at(init), "|c>: find_known requires a constant solution count in 1..N");
        }
        append(&list,
               emit_operation("{\"op\":\"qfind\",\"id\":" + std::to_string(var.id) +
                                  ",\"upper\":" + std::to_string(src.upper) + ",\"solutions\":" +
                                  std::to_string(solutions) + ",\"predicate\":" + predicate + '}',
                              at(var.decl)));
        DECL_INITIAL(var.decl) = build_zero_cst(TREE_TYPE(var.decl));
        append(&list, build1(DECL_EXPR, void_type_node, var.decl));
        return list;
    }
    append(&list,
           emit_operation("{\"op\":\"qalloc\",\"id\":" + std::to_string(var.id) + ",\"name\":" +
                              json(var.name) + ",\"width\":" + std::to_string(var.width) +
                              ",\"signed\":" + (var.is_signed ? "true" : "false") +
                              ",\"ancilla\":" + (var.expression_init ? "true" : "false") + '}',
                          at(var.decl)));
    var.lowered = true;
    if (var.expression_init) {
        var.creation = cast_result(expression(var.original_init, var.dependencies), var);
        if (var.dependencies.empty())
            error_at(
                at(var.decl),
                "|c>: quantum initializer must be constant, superposition, or a supported quantum expression");
        var.derived = true;
        append(&list,
               emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(var.id) +
                                  ",\"expr\":" + var.creation + '}',
                              at(var.decl)));
    } else if (var.classical_init) {
        std::string value = cpu_parameter(var.original_init);
        append(&list,
               emit_operation("{\"op\":\"qprepare\",\"id\":" + std::to_string(var.id) +
                                  ",\"expr\":" + value + '}',
                              at(var.decl)));
    } else {
        append(&list,
               emit_operation("{\"op\":\"qprepare\",\"id\":" + std::to_string(var.id) +
                                  ",\"lower\":" + std::to_string(var.lower) +
                                  ",\"upper\":" + std::to_string(var.upper) + '}',
                              at(var.decl)));
    }
    DECL_INITIAL(var.decl) = build_zero_cst(TREE_TYPE(var.decl));
    append(&list, build1(DECL_EXPR, void_type_node, var.decl));
    return list;
}

tree lower_indexed_operation(
    tree selection, const char *operation, tree argument, bool inverse, location_t loc) {
    const auto &access = quantum_indexed_accesses.at(selection);
    // Operand measurements happen before the selected operation. Record that
    // order in the IR and update ownership after those operand effects.
    tree lowered_index = save_expr(lower_node(access.index));
    tree lowered_argument = argument ? save_expr(lower_node(argument)) : constant(0);
    tree list = alloc_stmt_list();
    append(&list, lowered_index);
    if (argument)
        append(&list, lowered_argument);
    cleanup(&list);
    if (!controls.empty() || saw_return || access.readonly) {
        error_at(
            loc,
            "|c>: indexed operation requires classical control and a compatible mutable borrow");
        return constant(0);
    }
    std::string ids = "[";
    for (tree storage : access.elements) {
        auto &var = variables.at(storage);
        if (!var.lowered || var.moved || var.released || var.derived || dependent(var.id) ||
            readonly_quantum_borrows.count(storage)) {
            error_at(
                loc,
                "|c>: indexed access needs available storage without pending cleanup dependencies");
            return constant(0);
        }
        consume(var);
        taint_uniform(var.id);
        if (ids.size() > 1)
            ids += ',';
        ids += std::to_string(var.id);
        var.clean = var.basis_known = var.pristine_uniform = false;
        if (std::string(operation) != "qmeasure" && std::string(operation) != "qreset")
            var.measured = false;
        var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
        ++var.epoch;
    }
    ids += ']';
    bool dynamic = argument != NULL_TREE;
    std::string action = "{\"op\":" + json(operation) +
                         ",\"id\":" + std::to_string(variables.at(access.elements.front()).id);
    if (dynamic)
        action += ",\"dynamic\":true";
    if (std::string(operation) == "qfourier")
        action += std::string(",\"inverse\":") + (inverse ? "true" : "false");
    action += '}';
    std::string text = "{\"op\":\"qindexed\",\"ids\":" + ids + ",\"action\":" + action + '}';
    instructions.push_back(text);
    if (!apply_index_fn) {
        apply_index_fn = build_fn_decl("__cket_apply_index",
                                       build_function_type_list(long_long_unsigned_type_node,
                                                                build_pointer_type(char_type_node),
                                                                long_long_unsigned_type_node,
                                                                long_long_unsigned_type_node,
                                                                long_long_unsigned_type_node,
                                                                NULL_TREE));
        TREE_PUBLIC(apply_index_fn) = DECL_EXTERNAL(apply_index_fn) =
            DECL_ARTIFICIAL(apply_index_fn) = 1;
    }
    tree call = build_call_expr_loc(
        loc,
        apply_index_fn,
        4,
        build_string_literal(text.size() + 1, text.c_str()),
        lowered_index,
        constant(dynamic ? 1 : 0),
        dynamic ? fold_convert(long_long_unsigned_type_node, lowered_argument) : constant(0));
    tree sequence = call;
    std::vector<tree> prefix;
    for (auto it = tsi_start(list); !tsi_end_p(it); tsi_next(&it))
        prefix.push_back(tsi_stmt(it));
    for (auto it = prefix.rbegin(); it != prefix.rend(); ++it)
        sequence = build2(COMPOUND_EXPR, TREE_TYPE(call), *it, sequence);
    return sequence;
}

tree lower_node(tree value) {
    if (!value)
        return value;
    auto code = TREE_CODE(value);
    if (quantum_memory_copies.count(value)) {
        const auto &operation = quantum_memory_copies.at(value);
        tree list = alloc_stmt_list();
        // Check every precondition before mutating metadata or emitting a write.
        for (const auto &pair : operation.elements) {
            auto &target = variables.at(pair.first), &source = variables.at(pair.second);
            if (!target.lowered || target.moved || target.released || !source.lowered ||
                source.moved || source.released) {
                error_at(at(value), "|c>: quantum memory copy requires available checked storage");
                return build_zero_cst(TREE_TYPE(value));
            }
            if (pair.first == pair.second)
                continue; // Exact memmove aliases preserve arbitrary states.
            if (dynamic_depth) {
                error_at(
                    at(value),
                    "|c>: quantum memory copy in a CPU loop needs a per-iteration basis/history proof; use a proved static expansion or explicit storage management");
                return build_zero_cst(TREE_TYPE(value));
            }
            if ((!target.clean && !target.basis_known) || target.derived || dependent(target.id)) {
                error_at(
                    at(value),
                    "|c>: quantum memory copy needs an independent destination in a known basis state; overwriting unknown storage requires retained history");
                return build_zero_cst(TREE_TYPE(value));
            }
            for (const auto &control : controls)
                if (target.id == control.first) {
                    error_at(at(value), "|c>: quantum memory copy destination aliases its control");
                    return build_zero_cst(TREE_TYPE(value));
                }
        }
        for (const auto &pair : operation.elements) {
            auto &target = variables.at(pair.first), &source = variables.at(pair.second);
            consume(target);
            if (pair.first == pair.second) {
                consume(source);
                continue;
            }
            std::set<unsigned> dependencies;
            std::string expr = cast_result(expression(pair.second, dependencies), target);
            if (target.basis_known && target.lower)
                append(&list,
                       emit_operation(
                           "{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(target.id) +
                               ",\"value\":" + std::to_string(target.lower) + control_json() + '}',
                           at(value)));
            append(&list,
                   emit_operation("{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(target.id) +
                                      ",\"expr\":" + expr + control_json() + '}',
                                  at(value)));
            taint_uniform(source.id);
            taint_uniform(target.id);
            target.range_max =
                target.width == 64 ? UINT64_MAX : ((uint64_t{1} << target.width) - 1);
            ++target.epoch;
            target.measured = target.clean = target.basis_known = false;
        }
        if (!operation.elements.empty())
            for (const auto &control : controls)
                taint_uniform(control.first);
        // The C return pointer is currently accepted only when discarded. CPU
        // contexts detect this marker as quantum before attempting this lowering.
        tree result = build_zero_cst(TREE_TYPE(value));
        for (auto it = tsi_last(list); !tsi_end_p(it); tsi_prev(&it))
            result = build2(COMPOUND_EXPR, TREE_TYPE(value), tsi_stmt(it), result);
        return result;
    }
    if (CONVERT_EXPR_P(value) && TREE_TYPE(value) == void_type_node &&
        quantum_memory_copies.count(strip(value)))
        return lower_node(strip(value));
    if ((code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR ||
         code == POSTDECREMENT_EXPR) &&
        selected_storage(TREE_OPERAND(value, 0))) {
        tree selection = selected_storage(TREE_OPERAND(value, 0));
        tree type = TREE_TYPE(quantum_indexed_accesses.at(selection).elements.front());
        if (!TYPE_UNSIGNED(type) || TYPE_PRECISION(type) == 1) {
            error_at(at(value),
                     "|c>: indexed increment currently requires an unsigned integer register");
            return constant(0);
        }
        return lower_indexed_operation(
            selection,
            "qadd_inplace",
            build_int_cst(long_long_integer_type_node,
                          code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR ? -1 : 1),
            false,
            at(value));
    }
    if (code == MODIFY_EXPR && selected_storage(TREE_OPERAND(value, 0))) {
        tree selection = selected_storage(TREE_OPERAND(value, 0)),
             rhs = strip(TREE_OPERAND(value, 1));
        auto operation = TREE_CODE(rhs);
        tree type = TREE_TYPE(quantum_indexed_accesses.at(selection).elements.front());
        tree operand = indexed_delta(
            TREE_OPERAND(value, 1), selection, TYPE_PRECISION(type), operation == BIT_XOR_EXPR);
        if (!operand || has_quantum(operand) || !TYPE_UNSIGNED(type) || TYPE_PRECISION(type) == 1) {
            error_at(
                at(value),
                "|c>: indexed update currently supports unsigned addition, subtraction or XOR with a classical operand");
            return constant(0);
        }
        return lower_indexed_operation(selection,
                                       operation == BIT_XOR_EXPR ? "qxor_inplace" : "qadd_inplace",
                                       operand,
                                       false,
                                       at(value));
    }
    if (code == CALL_EXPR) {
        std::string kind = intrinsic(value);
        if ((kind == "measure" || kind == "reset" || kind == "fourier" ||
             kind == "inverse_fourier") &&
            selected_storage(CALL_EXPR_ARG(value, 0)))
            return lower_indexed_operation(selected_storage(CALL_EXPR_ARG(value, 0)),
                                           kind == "measure" ? "qmeasure"
                                           : kind == "reset" ? "qreset"
                                                             : "qfourier",
                                           NULL_TREE,
                                           kind == "inverse_fourier",
                                           at(value));
        if (quantum_indexed_accesses.count(value)) {
            error_at(
                at(value),
                "|c>: indexed quantum storage cannot be implicitly read or moved; use an implemented indexed operation or explicit measure");
            return constant(0);
        }
    }
    if (CONVERT_EXPR_P(value) || code == NON_LVALUE_EXPR) {
        tree inner = strip(value);
        auto kind = TREE_CODE(inner);
        if ((kind == PREINCREMENT_EXPR || kind == POSTINCREMENT_EXPR || kind == PREDECREMENT_EXPR ||
             kind == POSTDECREMENT_EXPR) &&
            (quantum(strip(TREE_OPERAND(inner, 0))) || selected_storage(TREE_OPERAND(inner, 0))))
            return lower_node(inner);
    }
    if (code == BIND_EXPR) {
        BIND_EXPR_BODY(value) = lower_node(BIND_EXPR_BODY(value));
        return value;
    }
    if (code == STATEMENT_LIST) {
        tree result = alloc_stmt_list();
        for (auto it = tsi_start(value); !tsi_end_p(it); tsi_next(&it)) {
            append(&result, lower_node(tsi_stmt(it)));
            cleanup(&result);
        }
        return result;
    }
    if (code == COMPOUND_EXPR) {
        tree update = strip(TREE_OPERAND(value, 1));
        if (TREE_CODE(update) == MODIFY_EXPR &&
            (quantum(strip(TREE_OPERAND(update, 0))) ||
             selected_storage(TREE_OPERAND(update, 0))) &&
            !has_quantum(TREE_OPERAND(value, 0))) {
            // GCC saves a side-effecting compound-assignment operand before
            // updating the destination. Keep its shared SAVE_EXPR intact so
            // native C evaluates the operand exactly once.
            if (!controls.empty() && TREE_SIDE_EFFECTS(TREE_OPERAND(value, 0))) {
                error_at(
                    at(value),
                    "|c>: a classical side effect in an arithmetic operand cannot execute inside coherent quantum control");
                return value;
            }
            tree list = alloc_stmt_list();
            append(&list, lower_node(TREE_OPERAND(value, 0)));
            append(&list, lower_node(update));
            return list;
        }
    }
    if (code == DECL_EXPR) {
        tree decl = DECL_EXPR_DECL(value);
        if (variables.count(decl))
            return lower_quantum_decl(variables.at(decl));
        if (!controls.empty())
            error_at(at(decl),
                     "|c>: classical storage cannot be created coherently inside a quantum if");
        if (DECL_INITIAL(decl) && has_quantum(DECL_INITIAL(decl))) {
            error_at(
                at(decl),
                "|c>: classical initialization cannot read a quantum value; measure explicitly");
            return value;
        }
        if (DECL_INITIAL(decl))
            DECL_INITIAL(decl) = lower_node(DECL_INITIAL(decl));
        return value;
    }
    if (code == RETURN_EXPR) {
        if (has_quantum(TREE_OPERAND(value, 0))) {
            error_at(at(value),
                     "|c>: returning a quantum value through the CPU requires explicit measure");
            return value;
        }
        bool live = false;
        for (const auto &entry : variables)
            live |= entry.second.lowered && !entry.second.measured && !entry.second.released &&
                    !entry.second.moved;
        if (!controls.empty() || (live && dynamic_depth))
            error_at(at(value), "|c>: return inside a quantum region needs proven cleanup");
        if (TREE_OPERAND(value, 0))
            TREE_OPERAND(value, 0) = lower_node(TREE_OPERAND(value, 0));
        // Ordinary C functions have no register cleanup to emit. Preserve GCC's
        // return assignment so inlining and SSA retain the actual return value.
        if (variables.empty())
            return value;
        tree list = alloc_stmt_list();
        // The return expression may perform explicit measurement. Evaluate it
        // before emitting release calls, preserving actual native/IR order.
        tree operand = TREE_OPERAND(value, 0);
        if (operand && TREE_TYPE(operand) != void_type_node) {
            tree result = DECL_RESULT(active_function);
            if (TREE_CODE(operand) == MODIFY_EXPR && TREE_OPERAND(operand, 0) == result)
                append(&list, operand);
            else
                append(&list, build2(MODIFY_EXPR, TREE_TYPE(result), result, operand));
            TREE_OPERAND(value, 0) = result;
        }
        cleanup(&list);
        append(&list, value);
        if (!dynamic_depth)
            saw_return = true;
        return list;
    }
    if (code == COND_EXPR && TREE_TYPE(value) == void_type_node &&
        has_quantum(TREE_OPERAND(value, 0))) {
        tree list = alloc_stmt_list();
        std::set<unsigned> deps;
        std::string predicate = "{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":[" +
                                expression(TREE_OPERAND(value, 0), deps) + "]}";
        predicate = select_controls(predicate,
                                    "{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":0}",
                                    1,
                                    false,
                                    deps);
        unsigned flag = next_id++;
        std::set<unsigned> parameters;
        parameters.swap(active_cpu_parameters);
        append(&list,
               emit_cpu_storage("{\"op\":\"qalloc\",\"id\":" + std::to_string(flag) +
                                    ",\"width\":1,\"signed\":false,\"ancilla\":true}",
                                at(value),
                                flag,
                                true));
        active_cpu_parameters.swap(parameters);
        append(&list,
               emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(flag) +
                                  ",\"expr\":" + predicate + '}',
                              at(value)));
        // A live predicate's sources cannot be changed before its inverse.
        Variable guard{};
        guard.id = flag;
        guard.width = 1;
        guard.range_max = 1;
        guard.lowered = guard.derived = true;
        guard.optional_definition = !cpu_branches.empty();
        guard.dependencies = deps;
        guard.creation = predicate;
        tree key =
            build_decl(at(value), VAR_DECL, get_identifier("__cket_guard"), boolean_type_node);
        guard.decl = key;
        variables.emplace(key, guard);
        controls.push_back({flag, 1});
        append(&list, lower_node(TREE_OPERAND(value, 1)));
        controls.back().second = 0;
        append(&list, lower_node(TREE_OPERAND(value, 2)));
        controls.pop_back();
        if (dependent(flag)) {
            variables.at(key).history = true;
            append(&list,
                   emit_cpu_storage("{\"op\":\"qretain\",\"id\":" + std::to_string(flag) + '}',
                                    at(value),
                                    flag,
                                    false));
        } else {
            variables.erase(key);
            append(&list, emit_operation(inverse_operation(flag, predicate), at(value)));
            append(&list,
                   emit_operation("{\"op\":\"qrelease\",\"id\":" + std::to_string(flag) + '}',
                                  at(value)));
        }
        return list;
    }
    if (code == COND_EXPR && TREE_TYPE(value) == void_type_node) {
        if (!controls.empty()) {
            error_at(at(value),
                     "|c>: classical branch inside quantum control is not supported yet");
            return value;
        }
        tree choice =
            save_expr(fold_convert(boolean_type_node, lower_node(TREE_OPERAND(value, 0))));
        TREE_OPERAND(value, 0) = choice;
        invalidate_cpu_ranges();
        auto incoming = variables;
        auto before = remaining;
        auto then_uses = branch_uses(TREE_OPERAND(value, 1));
        auto else_uses = branch_uses(TREE_OPERAND(value, 2));
        auto after = before;
        for (auto &entry : after) {
            unsigned branch = then_uses[entry.first] + else_uses[entry.first];
            entry.second = entry.second > branch ? entry.second - branch : 0;
        }
        size_t position = instructions.size();
        CpuBranch region;
        for (const auto &entry : incoming)
            if (entry.second.lowered && !entry.second.released && !entry.second.moved)
                region.incoming.insert(entry.second.id);
        cpu_branches.push_back(&region);
        // Record dynamic CPU control explicitly, never flatten it for export.
        instructions.push_back("{\"op\":\"ccontrol_begin\"}");
        ++dynamic_depth;
        remaining = after;
        for (const auto &entry : then_uses)
            remaining[entry.first] += entry.second;
        TREE_OPERAND(value, 1) = lower_node(TREE_OPERAND(value, 1));
        auto then_state = variables;
        variables = incoming;
        remaining = after;
        for (const auto &entry : else_uses)
            remaining[entry.first] += entry.second;
        instructions.push_back("{\"op\":\"ccontrol_else\"}");
        TREE_OPERAND(value, 2) = lower_node(TREE_OPERAND(value, 2));
        auto else_state = variables;
        instructions.push_back("{\"op\":\"ccontrol_end\"}");
        tree join = alloc_stmt_list();
        merge_cpu_versions(incoming, then_state, else_state, after, choice, at(value), &join);
        --dynamic_depth;
        cpu_branches.pop_back();
        tree result = alloc_stmt_list();
        if (cpu_branches.empty()) {
            // Evaluate the source condition before any storage operations and
            // keep its value for joins even if an arm mutates its CPU inputs.
            append(&result, choice);
            for (tree call : region.prefix)
                append(&result, call);
            instructions.insert(instructions.begin() + position,
                                region.operations.begin(),
                                region.operations.end());
        }
        append(&result, value);
        append(&result, join);
        if (cpu_branches.empty()) {
            for (unsigned id : region.allocations) {
                bool represented = false;
                for (const auto &entry : variables)
                    represented |=
                        entry.second.id == id && !entry.second.released && !entry.second.moved;
                if (!represented)
                    append(&result,
                           emit_operation("{\"op\":\"qdispose\",\"id\":" + std::to_string(id) + '}',
                                          at(value)));
            }
            cleanup(&result);
        }
        return result;
    }
    if (code == WHILE_STMT || code == DO_STMT || code == FOR_STMT || code == LOOP_EXPR) {
        tree condition = code == WHILE_STMT ? WHILE_COND(value)
                         : code == DO_STMT  ? DO_COND(value)
                         : code == FOR_STMT ? FOR_COND(value)
                                            : TREE_OPERAND(value, 0);
        if (has_quantum(condition) || !controls.empty()) {
            error_at(
                at(value),
                "|c>: quantum while/for requires a proven finite reversible iteration bound; no implicit measurement is allowed");
            return value;
        }
        invalidate_cpu_ranges();
        auto incoming = variables;
        instructions.push_back("{\"op\":\"cloop_begin\"}");
        ++dynamic_depth;
        if (code == WHILE_STMT) {
            WHILE_COND(value) = lower_node(WHILE_COND(value));
            WHILE_BODY(value) = lower_node(WHILE_BODY(value));
        } else if (code == DO_STMT) {
            DO_COND(value) = lower_node(DO_COND(value));
            DO_BODY(value) = lower_node(DO_BODY(value));
        } else if (code == FOR_STMT) {
            FOR_INIT_STMT(value) = lower_node(FOR_INIT_STMT(value));
            FOR_COND(value) = lower_node(FOR_COND(value));
            FOR_BODY(value) = lower_node(FOR_BODY(value));
            FOR_EXPR(value) = lower_node(FOR_EXPR(value));
        } else
            TREE_OPERAND(value, 0) = lower_node(TREE_OPERAND(value, 0));
        auto back_edge = variables;
        merge_cpu_paths(incoming, back_edge, at(value));
        --dynamic_depth;
        instructions.push_back("{\"op\":\"cloop_end\"}");
        return value;
    }
    if ((code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR ||
         code == POSTDECREMENT_EXPR) &&
        quantum(strip(TREE_OPERAND(value, 0)))) {
        auto &var = variables.at(strip(TREE_OPERAND(value, 0)));
        // This statement synthesizes two references from one source reference.
        ++remaining[var.id];
        bool subtract = code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR;
        tree promoted = TYPE_PRECISION(TREE_TYPE(var.decl)) < TYPE_PRECISION(integer_type_node)
                            ? integer_type_node
                            : TYPE_MAIN_VARIANT(TREE_TYPE(var.decl));
        tree rhs = build2(subtract ? MINUS_EXPR : PLUS_EXPR,
                          promoted,
                          fold_convert(promoted, var.decl),
                          build_int_cst(promoted, 1));
        tree assignment = build2(
            MODIFY_EXPR, TREE_TYPE(var.decl), var.decl, fold_convert(TREE_TYPE(var.decl), rhs));
        protected_set_expr_location(assignment, at(value));
        return lower_node(assignment);
    }
    if ((code == MODIFY_EXPR || code == INIT_EXPR) && quantum(strip(TREE_OPERAND(value, 0)))) {
        auto &var = variables.at(strip(TREE_OPERAND(value, 0)));
        tree rhs = TREE_OPERAND(value, 1);
        tree list = alloc_stmt_list();
        cleanup(&list);
        consume(var);
        if (var.moved || var.released) {
            error_at(
                at(value),
                "|c>: modifying this register would prevent uncomputation of a live dependent quantum value");
            return list;
        }
        taint_uniform(var.id);
        for (const auto &control : controls)
            taint_uniform(control.first);
        if (dependent(var.id) || var.derived ||
            (lossy_assignment(rhs, var.width) &&
             (!var.clean || references_storage(rhs, var.decl)))) {
            std::set<unsigned> deps;
            std::string expr = cast_result(expression(rhs, deps), var);
            versioned_assignment(var, expr, deps, at(value), &list);
            return list;
        }
        tree simple = strip(rhs);
        tree addend = NULL_TREE;
        if (TREE_CODE(simple) == PLUS_EXPR || TREE_CODE(simple) == MINUS_EXPR) {
            if (strip(TREE_OPERAND(simple, 0)) == var.decl)
                addend = TREE_OPERAND(simple, 1);
            else if (TREE_CODE(simple) == PLUS_EXPR && strip(TREE_OPERAND(simple, 1)) == var.decl)
                addend = TREE_OPERAND(simple, 0);
        }
        tree quantum_addend = addend ? strip(addend) : NULL_TREE;
        tree xor_operand = NULL_TREE;
        if (TREE_CODE(simple) == BIT_XOR_EXPR && var.width > 1) {
            if (strip(TREE_OPERAND(simple, 0)) == var.decl)
                xor_operand = TREE_OPERAND(simple, 1);
            else if (strip(TREE_OPERAND(simple, 1)) == var.decl)
                xor_operand = TREE_OPERAND(simple, 0);
        }
        if (xor_operand) {
            consume(var);
            if (has_quantum(xor_operand)) {
                std::set<unsigned> deps;
                std::string expr = cast_result(expression(xor_operand, deps), var);
                if (deps.count(var.id)) {
                    error_at(at(value),
                             "|c>: XOR operand cannot depend on the overwritten register");
                    return list;
                }
                for (unsigned source : deps)
                    taint_uniform(source);
                append(&list,
                       emit_operation("{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(var.id) +
                                          ",\"expr\":" + expr + control_json() + '}',
                                      at(value)));
            } else {
                if (!controls.empty() && TREE_SIDE_EFFECTS(xor_operand)) {
                    error_at(at(value),
                             "|c>: CPU effects in XOR cannot execute under coherent control");
                    return list;
                }
                tree operand = lower_node(xor_operand);
                if (TREE_CODE(strip(operand)) == INTEGER_CST)
                    append(&list,
                           emit_operation("{\"op\":\"qxor_inplace\",\"id\":" +
                                              std::to_string(var.id) + ",\"value\":" +
                                              std::to_string(static_cast<int64_t>(
                                                  TREE_INT_CST_LOW(strip(operand)))) +
                                              control_json() + '}',
                                          at(value)));
                else
                    append(
                        &list,
                        emit_operation("{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(var.id) +
                                           ",\"dynamic\":true" + control_json() + '}',
                                       at(value),
                                       operand));
            }
        } else if (quantum(quantum_addend) && !var.is_signed && var.width > 1 &&
                   !variables.at(quantum_addend).is_signed && quantum_addend != var.decl) {
            auto &source = variables.at(quantum_addend);
            if (!source.lowered || source.moved || source.released) {
                error_at(at(value),
                         "|c>: quantum addition requires available borrowed source storage");
                return list;
            }
            consume(var);
            consume(source);
            taint_uniform(source.id);
            append(&list,
                   emit_operation(
                       "{\"op\":\"qadd_register\",\"id\":" + std::to_string(var.id) +
                           ",\"source\":" + std::to_string(source.id) +
                           ",\"subtract\":" + (TREE_CODE(simple) == MINUS_EXPR ? "true" : "false") +
                           control_json() + '}',
                       at(value)));
        } else if (addend && !has_quantum(addend) && !var.is_signed && var.width > 1) {
            if (!controls.empty() && TREE_SIDE_EFFECTS(addend)) {
                error_at(
                    at(value),
                    "|c>: a classical side effect in an arithmetic operand cannot execute inside coherent quantum control");
                return list;
            }
            consume(var);
            tree argument = addend;
            if (TREE_CODE(simple) == MINUS_EXPR)
                argument = fold_build1(NEGATE_EXPR, TREE_TYPE(argument), argument);
            argument = lower_node(argument);
            if (TREE_CODE(strip(argument)) == INTEGER_CST)
                append(&list,
                       emit_operation("{\"op\":\"qadd_inplace\",\"id\":" + std::to_string(var.id) +
                                          ",\"value\":" +
                                          std::to_string(static_cast<int64_t>(
                                              TREE_INT_CST_LOW(strip(argument)))) +
                                          control_json() + '}',
                                      at(value)));
            else
                append(&list,
                       emit_operation("{\"op\":\"qadd_inplace\",\"id\":" + std::to_string(var.id) +
                                          ",\"dynamic\":true" + control_json() + '}',
                                      at(value),
                                      argument));
        } else {
            std::set<unsigned> deps;
            std::string expr = cast_result(expression(rhs, deps), var);
            if (var.clean && !var.derived && !deps.count(var.id) && controls.empty() &&
                !dynamic_depth) {
                var.derived = true;
                var.dependencies = deps;
                var.creation = expr;
                var.measured = var.clean = var.basis_known = false;
                var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
                append(&list,
                       emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(var.id) +
                                          ",\"expr\":" + expr + '}',
                                      at(value)));
                return list;
            }
            if (deps != std::set<unsigned>{var.id}) {
                versioned_assignment(var, expr, deps, at(value), &list);
                return list;
            }
            append(&list,
                   emit_operation("{\"op\":\"qinplace\",\"id\":" + std::to_string(var.id) +
                                      ",\"expr\":" + expr + control_json() + '}',
                                  at(value)));
        }
        var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
        ++var.epoch;
        var.measured = var.clean = var.basis_known = false;
        return list;
    }
    if (code == CALL_EXPR) {
        auto kind = intrinsic(value);
        if (kind == "circuit" || kind == "inverse_circuit") {
            tree filename = strip(CALL_EXPR_ARG(value, 0));
            if (TREE_CODE(filename) == ADDR_EXPR)
                filename = TREE_OPERAND(filename, 0);
            if (TREE_CODE(filename) != STRING_CST) {
                error_at(at(value), "|c>: circuit module path must be a string literal");
                return constant(0);
            }
            std::string path = TREE_STRING_POINTER(filename);
            if (path.empty()) {
                error_at(at(value), "|c>: empty circuit module path");
                return constant(0);
            }
            if (path[0] != '/') {
                std::string source = LOCATION_FILE(at(value));
                size_t slash = source.find_last_of('/');
                if (slash != std::string::npos)
                    path = source.substr(0, slash + 1) + path;
            }
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            auto size = file.tellg();
            if (!file || size <= 0 || size > 64 * 1024 * 1024 ||
                (size_t)size > 64 * 1024 * 1024 - embedded_module_bytes) {
                error_at(at(value),
                         "|c>: cannot read circuit module or embedded modules exceed 64 MiB");
                return constant(0);
            }
            std::string module((size_t)size, '\0');
            file.seekg(0);
            file.read(&module[0], size);
            if (!file) {
                error_at(at(value), "|c>: cannot read complete circuit module");
                return constant(0);
            }
            embedded_module_bytes += module.size();
            std::string ids = "[";
            for (int j = 1; j < call_expr_nargs(value); ++j) {
                tree storage = strip(CALL_EXPR_ARG(value, j));
                auto found = variables.find(storage);
                if (found == variables.end() || !found->second.lowered || found->second.moved ||
                    found->second.released || found->second.derived ||
                    dependent(found->second.id) || readonly_quantum_borrows.count(storage)) {
                    error_at(at(value),
                             "|c>: circuit module needs available mutable typed register borrows");
                    return constant(0);
                }
                auto &var = found->second;
                consume(var);
                if (j > 1)
                    ids += ',';
                ids += std::to_string(var.id);
                var.basis_known = var.pristine_uniform = var.measured = var.clean = false;
                var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
                ++var.epoch;
            }
            ids += ']';
            return emit_operation("{\"op\":\"qmodule\",\"ids\":" + ids + ",\"inverse\":" +
                                      (kind == "inverse_circuit" ? "true" : "false") +
                                      ",\"circuit\":" + module + control_json() + '}',
                                  at(value));
        }
        if (kind == "search") {
            tree storage = strip(CALL_EXPR_ARG(value, 0)), count = strip(CALL_EXPR_ARG(value, 2));
            auto found = variables.find(storage);
            if (!controls.empty() || found == variables.end() || !found->second.lowered ||
                found->second.moved || found->second.released || !found->second.pristine_uniform ||
                found->second.derived || dependent(found->second.id) ||
                readonly_quantum_borrows.count(storage)) {
                error_at(
                    at(value),
                    "|c>: search needs an independent pristine uniform mutable register under classical control");
                return constant(0);
            }
            if (TREE_CODE(count) != INTEGER_CST || !tree_fits_uhwi_p(count) ||
                !tree_to_uhwi(count) || tree_to_uhwi(count) > 4096) {
                error_at(at(value), "|c>: search requires a constant attempt budget in 1..4096");
                return constant(0);
            }
            auto &var = found->second;
            consume(var);
            std::set<unsigned> deps;
            std::string predicate = expression(CALL_EXPR_ARG(value, 1), deps);
            if (deps.size() != 1 || !deps.count(var.id))
                error_at(at(value), "|c>: search predicate must depend only on its input");
            std::string operation = "{\"op\":\"qsearch\",\"id\":" + std::to_string(var.id) +
                                    ",\"upper\":" + std::to_string(var.upper) +
                                    ",\"attempts\":" + std::to_string(tree_to_uhwi(count)) +
                                    ",\"predicate\":" + predicate + '}';
            var.measured = true;
            var.pristine_uniform = var.basis_known = var.clean = false;
            var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
            ++var.epoch;
            return fold_convert(boolean_type_node, emit_operation(operation, at(value)));
        }
        if (kind == "fourier" || kind == "inverse_fourier") {
            tree storage = strip(CALL_EXPR_ARG(value, 0));
            auto found = variables.find(storage);
            tree list = alloc_stmt_list();
            cleanup(&list);
            if (found == variables.end() || !found->second.lowered || found->second.moved ||
                found->second.released || found->second.derived || dependent(found->second.id)) {
                error_at(
                    at(value),
                    "|c>: Fourier transform needs available owned storage without pending inverse dependencies");
                return list;
            }
            auto &var = found->second;
            consume(var);
            taint_uniform(var.id);
            for (const auto &control : controls)
                taint_uniform(control.first);
            var.basis_known = var.pristine_uniform = var.clean = var.measured = false;
            var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
            ++var.epoch;
            append(&list,
                   emit_operation(
                       "{\"op\":\"qfourier\",\"id\":" + std::to_string(var.id) + ",\"inverse\":" +
                           (kind == "inverse_fourier" ? "true" : "false") + control_json() + '}',
                       at(value)));
            return list;
        }
        if (kind == "phase") {
            tree angle = CALL_EXPR_ARG(value, 0);
            if (has_quantum(angle) || (!controls.empty() && TREE_SIDE_EFFECTS(angle))) {
                error_at(
                    at(value),
                    "|c>: phase angle must be classical and cannot perform CPU effects under coherent control");
                return constant(0);
            }
            for (const auto &control : controls)
                taint_uniform(control.first);
            tree literal = fold(angle);
            if (TREE_CODE(literal) == REAL_CST) {
                char number[128];
                real_to_decimal(number, TREE_REAL_CST_PTR(literal), sizeof number, 0, 1);
                if (!std::isfinite(std::strtod(number, nullptr))) {
                    error_at(at(value), "|c>: phase requires a finite angle");
                    return constant(0);
                }
                return emit_operation("{\"op\":\"qphase\",\"angle\":" + std::string(number) +
                                          control_json() + '}',
                                      at(value));
            }
            std::string operation = "{\"op\":\"qphase\",\"dynamic\":true" + control_json() + '}';
            instructions.push_back(operation);
            if (!apply_phase_fn) {
                tree args = tree_cons(NULL_TREE, void_type_node, NULL_TREE);
                args = tree_cons(NULL_TREE, double_type_node, args);
                args = tree_cons(NULL_TREE, build_pointer_type(char_type_node), args);
                apply_phase_fn = build_fn_decl(
                    "__cket_apply_phase", build_function_type(long_long_unsigned_type_node, args));
                TREE_PUBLIC(apply_phase_fn) = DECL_EXTERNAL(apply_phase_fn) =
                    DECL_ARTIFICIAL(apply_phase_fn) = 1;
            }
            return build_call_expr_loc(
                at(value),
                apply_phase_fn,
                2,
                build_string_literal(operation.size() + 1, operation.c_str()),
                lower_node(angle));
        }
        if (kind == "phase_flip") {
            for (const auto &control : controls)
                taint_uniform(control.first);
            return emit_operation(
                "{\"op\":\"qphase\",\"angle\":3.141592653589793" + control_json() + '}', at(value));
        }
        if (kind == "measure" || kind == "reset") {
            if (!controls.empty() || saw_return) {
                error_at(
                    at(value),
                    "|c>: measurement/reset requires explicit classical control and a proven register lifetime");
                return constant(0);
            }
            tree arg = strip(CALL_EXPR_ARG(value, 0));
            auto found = variables.find(arg);
            if (found == variables.end() || !found->second.lowered || found->second.released ||
                found->second.moved) {
                error_at(at(value), "|c>: measurement/reset requires available quantum storage");
                return constant(0);
            }
            auto &var = found->second;
            consume(var);
            taint_uniform(var.id);
            // This explicit destruction consumes the computed output. Do not
            // automatically inverse that output before the requested measure.
            bool was_clean = var.clean;
            var.measured = true;
            var.pending_destruction = true;
            var.clean = false;
            tree list = alloc_stmt_list();
            cleanup(&list);
            var.clean = was_clean;
            if (dependent(var.id)) {
                var.pending_destruction = false;
                error_at(
                    at(value),
                    "|c>: measurement/reset would invalidate a live computed quantum value; use or uncompute it first");
                return constant(0);
            }
            tree call;
            if (kind == "measure") {
                if (!measure_fn)
                    measure_fn = runtime_function("__cket_measure", 1);
                call = build_call_expr_loc(at(value), measure_fn, 1, constant(var.id));
                instructions.push_back("{\"op\":\"qmeasure\",\"id\":" + std::to_string(var.id) +
                                       ",\"result\":\"m" + std::to_string(next_measurement++) +
                                       "\",\"width\":" + std::to_string(var.width) + '}');
                var.measured = true;
                ++var.epoch;
            } else {
                call = emit_operation("{\"op\":\"qreset\",\"id\":" + std::to_string(var.id) + '}',
                                      at(value));
                var.measured = var.clean = var.basis_known = true;
                var.lower = var.upper = var.range_max = 0;
                ++var.epoch;
                var.derived = false;
                var.dependencies.clear();
                var.creation.clear();
            }
            var.pending_destruction = false;
            // GCC COMPOUND_EXPR preserves cleanup-before-measure inside printf/initializers.
            tree sequence = call;
            std::vector<tree> prefix;
            for (auto it = tsi_start(list); !tsi_end_p(it); tsi_next(&it))
                prefix.push_back(tsi_stmt(it));
            for (auto it = prefix.rbegin(); it != prefix.rend(); ++it)
                sequence = build2(COMPOUND_EXPR, TREE_TYPE(call), *it, sequence);
            return sequence;
        }
        if (!kind.empty()) {
            error_at(at(value), "|c>: preparation/find is only valid as a quant initializer");
            return constant(0);
        }
        if (!controls.empty()) {
            error_at(at(value),
                     "|c>: classical calls and I/O cannot execute coherently inside a quantum if");
            return value;
        }
        if (has_quantum(value)) {
            error_at(
                at(value),
                "|c>: classical call arguments cannot read quantum values; measure explicitly");
            return value;
        }
    }
    if (quantum(value)) {
        error_at(
            at(value),
            "|c>: a quantum value cannot become a CPU value implicitly; use measure explicitly");
        return value;
    }
    if (DECL_P(value) || TYPE_P(value))
        return value;
    if (code == GOTO_EXPR && !variables.empty()) {
        error_at(
            at(value),
            "|c>: goto with quantum storage requires control-flow lifetime analysis and is not supported yet");
        return value;
    }
    if (has_quantum(value)) {
        error_at(
            at(value),
            "|c>: this C context cannot consume an unmeasured quantum result; use quant output storage or explicit measure");
        return value;
    }
    if (!controls.empty() &&
        (code == MODIFY_EXPR || code == INIT_EXPR || code == GOTO_EXPR || code == BREAK_STMT ||
         code == CONTINUE_STMT || code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR ||
         code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR || code == ASM_EXPR)) {
        error_at(at(value),
                 "|c>: classical side effects cannot execute inside coherent quantum control");
        return value;
    }
    // GCC knows how to walk constructors, lists, vectors and variable-arity
    // nodes. Skip the root once, then rewrite each child with this pass.
    bool root = true;
    auto children = [](tree *node, int *walk, void *data) -> tree {
        bool &first = *static_cast<bool *>(data);
        if (first) {
            first = false;
            return NULL_TREE;
        }
        *walk = 0;
        *node = lower_node(*node);
        return NULL_TREE;
    };
    walk_tree(&value, children, &root, nullptr);
    return value;
}
