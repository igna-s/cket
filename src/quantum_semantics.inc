// Semantic analysis of genuine GCC C trees. No source/token parser exists here.
std::map<unsigned, unsigned> remaining;
std::set<tree> counted_saves;
std::map<tree, std::pair<std::string, std::set<unsigned>>> expression_saves;
std::vector<std::pair<unsigned, unsigned>> controls;
unsigned dynamic_depth = 0;
size_t embedded_module_bytes=0;
tree apply_fn = NULL_TREE, apply_arg_fn = NULL_TREE;
tree apply_phase_fn = NULL_TREE;
tree apply_index_fn = NULL_TREE;
tree selected_storage(tree value) {
    value = strip(value);
    while (value && TREE_CODE(value) == SAVE_EXPR) value = strip(TREE_OPERAND(value, 0));
    return value && quantum_indexed_accesses.count(value) ? value : NULL_TREE;
}
bool same_selected_storage(tree value, tree selection) {
    tree other = selected_storage(value);
    if (!other) return false;
    if (other == selection) return true;
    const auto &a = quantum_indexed_accesses.at(other), &b = quantum_indexed_accesses.at(selection);
    tree ai = a.index, bi = b.index;
    // Independent pure index evaluations can select the same storage. GCC
    // deliberately treats distinct SAVE_EXPR nodes as unequal; inspect their
    // operands only when doing so cannot merge observable CPU effects.
    while (TREE_CODE(ai) == SAVE_EXPR && !TREE_SIDE_EFFECTS(TREE_OPERAND(ai, 0))) ai = TREE_OPERAND(ai, 0);
    while (TREE_CODE(bi) == SAVE_EXPR && !TREE_SIDE_EFFECTS(TREE_OPERAND(bi, 0))) bi = TREE_OPERAND(bi, 0);
    return a.elements == b.elements && operand_equal_p(ai, bi, 0);
}
tree indexed_delta(tree value, tree selection, unsigned width, bool xor_update) {
    if (TREE_CODE(value) == SAVE_EXPR) return indexed_delta(TREE_OPERAND(value, 0), selection, width, xor_update);
    if (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR) {
        if (!INTEGRAL_TYPE_P(TREE_TYPE(value)) || TYPE_PRECISION(TREE_TYPE(value)) < width) return NULL_TREE;
        tree inner = indexed_delta(TREE_OPERAND(value, 0), selection, width, xor_update);
        // These conversions preserve all target bits. Keep the offset in
        // unsigned arithmetic so reassociation cannot introduce signed UB.
        return inner;
    }
    if (same_selected_storage(value, selection)) return build_zero_cst(long_long_unsigned_type_node);
    auto code = TREE_CODE(value);
    if (xor_update ? code != BIT_XOR_EXPR : code != PLUS_EXPR && code != MINUS_EXPR) return NULL_TREE;
    tree a = TREE_OPERAND(value, 0), b = TREE_OPERAND(value, 1), delta = indexed_delta(a, selection, width, xor_update);
    if (delta && !has_quantum(b)) return fold_build2(code, long_long_unsigned_type_node, delta, fold_convert(long_long_unsigned_type_node, b));
    if (code == MINUS_EXPR || has_quantum(a)) return NULL_TREE;
    delta = indexed_delta(b, selection, width, xor_update);
    return delta ? fold_build2(code, long_long_unsigned_type_node, fold_convert(long_long_unsigned_type_node, a), delta) : NULL_TREE;
}

std::string control_json() {
    std::string result = ",\"controls\":[";
    for (size_t i = 0; i < controls.size(); ++i) {
        if (i) result += ',';
        result += "{\"id\":" + std::to_string(controls[i].first) + ",\"value\":" + std::to_string(controls[i].second) + '}';
    }
    return result + ']';
}
tree emit_operation(std::string operation, location_t loc, tree argument = NULL_TREE) {
    instructions.push_back(operation);
    tree &fn = argument ? apply_arg_fn : apply_fn;
    if (!fn) {
        tree args = tree_cons(NULL_TREE, void_type_node, NULL_TREE);
        if (argument) args = tree_cons(NULL_TREE, long_long_unsigned_type_node, args);
        args = tree_cons(NULL_TREE, build_pointer_type(char_type_node), args);
        fn = build_fn_decl(argument ? "__cket_apply_arg" : "__cket_apply", build_function_type(long_long_unsigned_type_node, args));
        TREE_PUBLIC(fn) = DECL_EXTERNAL(fn) = DECL_ARTIFICIAL(fn) = 1;
    }
    tree text = build_string_literal(operation.size() + 1, operation.c_str());
    return argument ? build_call_expr_loc(loc, fn, 2, text, fold_convert(long_long_unsigned_type_node, argument))
                    : build_call_expr_loc(loc, fn, 1, text);
}
void append(tree *list, tree value) { if (value) append_to_statement_list(value, list); }
tree quantum_use(tree *node, int *walk, void *) {
    if (quantum_indexed_accesses.count(*node)) return *node;
    if (quantum(*node)) return *node;
    if (DECL_P(*node) || TYPE_P(*node) || (TREE_CODE(*node) == CALL_EXPR && (intrinsic(*node) == "measure" || intrinsic(*node) == "search"))) *walk = 0;
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
        *walk = 0; walk_tree(&BIND_EXPR_BODY(value), count_uses, nullptr, nullptr);
    } else if (TREE_CODE(value) == DECL_EXPR) {
        *walk = 0; tree decl = DECL_EXPR_DECL(value);
        if (DECL_INITIAL(decl)) walk_tree(&DECL_INITIAL(decl), count_uses, nullptr, nullptr);
    } else if (quantum_indexed_accesses.count(value)) {
        for (tree storage : quantum_indexed_accesses.at(value).elements) ++remaining[variables.at(storage).id];
        *walk = 0;
        tree index = quantum_indexed_accesses.at(value).index;
        walk_tree(&index, count_uses, nullptr, nullptr);
    } else if (quantum(value)) {
        ++remaining[variables.at(value).id]; *walk = 0;
    } else if (DECL_P(value) || TYPE_P(value)) *walk = 0;
    return NULL_TREE;
}
void consume(Variable &var) { if (remaining[var.id]) --remaining[var.id]; }
void taint_uniform(unsigned id, std::set<unsigned> &visited) {
    if (!visited.insert(id).second) return;
    for (auto &entry : variables) if (entry.second.id == id) {
        entry.second.pristine_uniform = false;
        for (unsigned parent : entry.second.dependencies) taint_uniform(parent, visited);
    }
}
void taint_uniform(unsigned id) { std::set<unsigned> visited; taint_uniform(id, visited); }
std::string type_json(tree type) {
    if (!type || !INTEGRAL_TYPE_P(type)) return ",\"bits\":1,\"signed\":false";
    return ",\"bits\":" + std::to_string(TYPE_PRECISION(type)) + ",\"signed\":" + (TYPE_UNSIGNED(type) ? "false" : "true");
}
std::string expression(tree value, std::set<unsigned> &deps) {
    if (!value) return "{}";
    auto code = TREE_CODE(value);
    if (code == SAVE_EXPR) {
        auto cached = expression_saves.find(value);
        if (cached == expression_saves.end()) {
            std::set<unsigned> sources;
            std::string result = expression(TREE_OPERAND(value, 0), sources);
            cached = expression_saves.emplace(value, std::make_pair(result, sources)).first;
        }
        deps.insert(cached->second.second.begin(), cached->second.second.end());
        return cached->second.first;
    }
    if (code == NON_LVALUE_EXPR) return expression(TREE_OPERAND(value, 0), deps);
    if(CONVERT_EXPR_P(value) && TREE_TYPE(value)==void_type_node) return expression(TREE_OPERAND(value,0),deps);
    if (code == C_MAYBE_CONST_EXPR) return expression(C_MAYBE_CONST_EXPR_EXPR(value), deps);
    if (code == INTEGER_CST) {
        std::string val = TYPE_UNSIGNED(TREE_TYPE(value)) ? std::to_string(static_cast<uint64_t>(TREE_INT_CST_LOW(value)))
                                                        : std::to_string(static_cast<int64_t>(TREE_INT_CST_LOW(value)));
        return "{\"op\":\"const\",\"value\":" + val + type_json(TREE_TYPE(value)) + '}';
    }
    if (quantum(value)) {
        auto &var = variables.at(value); consume(var); deps.insert(var.id);
        if (!var.lowered || var.released || var.moved) error_at(at(value), "|c>: use of unavailable, released or moved quantum storage");
        unsigned active = 0; for (uint64_t n = var.range_max; n; n >>= 1) ++active;
        // Range information is valid only until an update/measurement invalidates it.
        return "{\"op\":\"reg\",\"id\":" + std::to_string(var.id) + type_json(TREE_TYPE(value)) +
               ",\"active\":" + std::to_string(active) + (var.basis_known ? ",\"known\":" + std::to_string(var.lower) : "") + '}';
    }
    const char *op = nullptr; int arity = 2;
    if(code==MULT_EXPR && TYPE_UNSIGNED(TREE_TYPE(value)) && TYPE_PRECISION(TREE_TYPE(value))<TYPE_PRECISION(integer_type_node)) {
        unsigned __int128 maximum[2];
        for(unsigned j=0;j<2;++j) {
            tree operand=strip(TREE_OPERAND(value,j));
            if(quantum(operand)) maximum[j]=variables.at(operand).range_max;
            else if(TREE_CODE(operand)==INTEGER_CST && tree_fits_uhwi_p(operand)) maximum[j]=tree_to_uhwi(operand);
            else maximum[j]=TYPE_PRECISION(TREE_TYPE(operand))>=64?UINT64_MAX:((uint64_t{1}<<TYPE_PRECISION(TREE_TYPE(operand)))-1);
        }
        if(maximum[0]*maximum[1]>((uint64_t{1}<<(TYPE_PRECISION(integer_type_node)-1))-1)) {
            error_at(at(value),"|c>: GCC narrowed a multiplication before the quantum pass; compute a full unsigned result before narrowing to preserve the arithmetic contract");return "{}";
        }
    }
    switch (code) {
        case NOP_EXPR: case CONVERT_EXPR: case VIEW_CONVERT_EXPR: op = "cast"; arity = 1; break;
        case PLUS_EXPR: op = "add"; break; case MINUS_EXPR: op = "sub"; break;
        case MULT_EXPR: op = "mul"; break; case TRUNC_DIV_EXPR: op = "div"; break; case TRUNC_MOD_EXPR: op = "mod"; break;
        case BIT_AND_EXPR: op = "band"; break; case BIT_IOR_EXPR: op = "bor"; break; case BIT_XOR_EXPR: op = "bxor"; break;
        case LSHIFT_EXPR: op = "shl"; break; case RSHIFT_EXPR: op = "shr"; break;
        case EQ_EXPR: op = "eq"; break; case NE_EXPR: op = "ne"; break; case LT_EXPR: op = "lt"; break;
        case LE_EXPR: op = "le"; break; case GT_EXPR: op = "gt"; break; case GE_EXPR: op = "ge"; break;
        case TRUTH_ANDIF_EXPR: case TRUTH_AND_EXPR: op = "land"; break;
        case TRUTH_ORIF_EXPR: case TRUTH_OR_EXPR: op = "lor"; break;
        case TRUTH_NOT_EXPR: op = "not"; arity = 1; break;
        case COND_EXPR: op = "select"; arity = 3; break;
        case COMPOUND_EXPR: op = "sequence"; break;
        case BIT_NOT_EXPR: op = "bnot"; arity = 1; break; case NEGATE_EXPR: op = "neg"; arity = 1; break;
        default: error_at(at(value), "|c>: unsupported GCC quantum expression node %qs",get_tree_code_name(code)); return "{}";
    }
    std::string result = "{\"op\":" + json(op) + type_json(TREE_TYPE(value)) + ",\"args\":[";
    for (int i = 0; i < arity; ++i) { if (i) result += ','; result += expression(TREE_OPERAND(value, i), deps); }
    return result + "]}";
}
std::string cast_result(std::string expr, Variable &var) {
    return "{\"op\":\"cast\",\"bits\":" + std::to_string(var.width) + ",\"signed\":" + (var.is_signed ? "true" : "false") + ",\"args\":[" + expr + "]}";
}
bool dependent(unsigned id) {
    for (auto &entry : variables) {
        auto &var = entry.second;
        if (var.lowered && var.derived && !var.released && !var.measured && !var.moved && var.dependencies.count(id)) return true;
    }
    return false;
}
void cleanup(tree *list) {
    if (dynamic_depth || !controls.empty()) return;
    bool changed;
    do {
        changed = false;
        for (auto &entry : variables) {
            auto &var = entry.second;
            if (var.lowered && var.measured && var.clean && !var.released && !var.moved && !remaining[var.id] && !dependent(var.id)) {
                append(list, emit_operation("{\"op\":\"qrelease\",\"id\":" + std::to_string(var.id) + '}', at(var.decl)));
                var.released = true; changed = true; continue;
            }
            if (!var.lowered || !var.derived || var.released || var.measured || var.moved || remaining[var.id] || dependent(var.id)) continue;
            append(list, emit_operation("{\"op\":\"quncompute\",\"id\":" + std::to_string(var.id) + ",\"expr\":" + var.creation + '}', at(var.decl)));
            append(list, emit_operation("{\"op\":\"qrelease\",\"id\":" + std::to_string(var.id) + '}', at(var.decl)));
            var.released = true; changed = true;
        }
    } while (changed);
}
tree lower_node(tree value);
void invalidate_cpu_ranges() {
    for(auto &entry:variables) {
        auto &v=entry.second;if(!v.lowered || v.released || v.moved || v.derived) continue;
        v.basis_known=v.pristine_uniform=false;
        v.range_max=v.width==64?UINT64_MAX:((uint64_t{1}<<v.width)-1);
    }
}
void merge_cpu_paths(const std::map<tree,Variable> &left,const std::map<tree,Variable> &right,location_t loc) {
    variables=left;
    for(auto &entry:variables) {
        auto other=right.find(entry.first);auto &a=entry.second;
        if(other==right.end()) {error_at(loc,"|c>: quantum storage must have a compatible owner on every CPU path");continue;}
        const auto &b=other->second;
        if(a.id!=b.id || a.lowered!=b.lowered || a.moved!=b.moved || a.released!=b.released || a.derived!=b.derived || a.dependencies!=b.dependencies)
            error_at(loc,"|c>: CPU paths disagree on quantum ownership or inverse dependencies");
        a.measured=a.measured && b.measured;a.clean=a.clean && b.clean;
        a.basis_known=a.basis_known && b.basis_known && a.lower==b.lower;
        a.pristine_uniform=a.pristine_uniform && b.pristine_uniform && a.epoch==b.epoch;
        a.range_max=std::max(a.range_max,b.range_max);a.epoch=std::max(a.epoch,b.epoch);
    }
}
tree lower_quantum_decl(Variable &var) {
    if (!controls.empty() || dynamic_depth) {
        error_at(at(var.decl), "|c>: quantum allocation inside a controlled branch or loop needs a proven lifetime and is not supported yet");
        return NULL_TREE;
    }
    if (saw_return) { error_at(at(var.decl), "|c>: quantum preparation after return is unreachable"); return NULL_TREE; }
    tree list = alloc_stmt_list();
    tree init = strip(var.original_init);
    // A change of integer representation computes a value and preserves its
    // source. Only a compatible representation can transfer the owner.
    if (var.expression_init && quantum(init) && variables.at(init).width==var.width && variables.at(init).is_signed==var.is_signed) {
        auto &src = variables.at(init);
        if (!src.lowered || src.released || src.moved ||
            TYPE_MAIN_VARIANT(TREE_TYPE(src.decl)) != TYPE_MAIN_VARIANT(TREE_TYPE(var.decl))) {
            error_at(at(var.decl), "|c>: quantum move requires available storage with the same C type"); return list;
        }
        consume(src);
        unsigned unused_id = var.id; remaining[src.id] += remaining[unused_id]; remaining[unused_id] = 0;
        var.id = src.id; var.lowered = true; var.range_max = src.range_max;
        var.derived = src.derived; var.creation = src.creation; var.dependencies = src.dependencies;
        var.lower = src.lower; var.upper = src.upper; var.epoch = src.epoch;
        var.basis_known = src.basis_known;
        var.pristine_uniform = src.pristine_uniform;
        var.measured = src.measured; var.clean = src.clean;
        src.moved = true;
        append(&list, emit_operation("{\"op\":\"qmove\",\"id\":" + std::to_string(var.id) + ",\"from\":" + json(src.name) + ",\"to\":" + json(var.name) + '}', at(var.decl)));
        DECL_INITIAL(var.decl) = build_zero_cst(TREE_TYPE(var.decl));
        append(&list, build1(DECL_EXPR, void_type_node, var.decl));
        return list;
    }
    // find transfers ownership of its input register: no allocation/copy.
    if (var.expression_init && (intrinsic(init) == "find" || intrinsic(init) == "find_known")) {
        tree source = strip(CALL_EXPR_ARG(init, 0));
        auto found = variables.find(source);
        if (found == variables.end() || !found->second.lowered ||
            TYPE_MAIN_VARIANT(TREE_TYPE(found->second.decl)) != TYPE_MAIN_VARIANT(TREE_TYPE(var.decl)) || dependent(found->second.id)) {
            error_at(at(var.decl), "|c>: find requires an independent quantum input of the same C type"); return list;
        }
        auto &src = found->second;
        consume(src);
        std::set<unsigned> deps;
        std::string predicate = expression(CALL_EXPR_ARG(init, 1), deps);
        if (deps != std::set<unsigned>{src.id} || src.lower != 0 || src.epoch || !src.pristine_uniform) {
            error_at(at(var.decl), "|c>: find currently requires an unchanged uniform [0,2^k-1] input and a pure predicate on that input"); return list;
        }
        unsigned unused_id = var.id; remaining[src.id] += remaining[unused_id]; remaining[unused_id] = 0;
        var.id = src.id; var.lowered = true; var.range_max = src.range_max;
        src.moved = true;
        uint64_t solutions=1;
        if(intrinsic(init)=="find_known") {
            tree count=strip(CALL_EXPR_ARG(init,2));
            if(TREE_CODE(count)!=INTEGER_CST || !tree_fits_uhwi_p(count) || !(solutions=tree_to_uhwi(count)) || solutions>src.upper+1)
                error_at(at(init),"|c>: find_known requires a constant solution count in 1..N");
        }
        append(&list, emit_operation("{\"op\":\"qfind\",\"id\":" + std::to_string(var.id) + ",\"upper\":" + std::to_string(src.upper) + ",\"solutions\":"+std::to_string(solutions)+",\"predicate\":" + predicate + '}', at(var.decl)));
        DECL_INITIAL(var.decl) = build_zero_cst(TREE_TYPE(var.decl));
        append(&list, build1(DECL_EXPR, void_type_node, var.decl));
        return list;
    }
    append(&list, emit_operation("{\"op\":\"qalloc\",\"id\":" + std::to_string(var.id) + ",\"name\":" + json(var.name) + ",\"width\":" + std::to_string(var.width) + ",\"signed\":" + (var.is_signed ? "true" : "false") + ",\"ancilla\":" + (var.expression_init ? "true" : "false") + '}', at(var.decl)));
    var.lowered = true;
    if (var.expression_init) {
        var.creation = cast_result(expression(var.original_init, var.dependencies), var);
        if (var.dependencies.empty()) error_at(at(var.decl), "|c>: quantum initializer must be constant, superposition, or a supported quantum expression");
        var.derived = true;
        append(&list, emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(var.id) + ",\"expr\":" + var.creation + '}', at(var.decl)));
    } else if (var.classical_init) {
        append(&list, emit_operation("{\"op\":\"qprepare\",\"id\":" + std::to_string(var.id) + ",\"dynamic\":true}",
            at(var.decl), lower_node(var.original_init)));
    } else {
        append(&list, emit_operation("{\"op\":\"qprepare\",\"id\":" + std::to_string(var.id) + ",\"lower\":" + std::to_string(var.lower) + ",\"upper\":" + std::to_string(var.upper) + '}', at(var.decl)));
    }
    DECL_INITIAL(var.decl) = build_zero_cst(TREE_TYPE(var.decl));
    append(&list, build1(DECL_EXPR, void_type_node, var.decl));
    return list;
}
tree lower_indexed_operation(tree selection, const char *operation, tree argument, bool inverse, location_t loc) {
    const auto &access = quantum_indexed_accesses.at(selection);
    // Operand measurements happen before the selected operation. Record that
    // order in the IR and update ownership after those operand effects.
    tree lowered_index = save_expr(lower_node(access.index));
    tree lowered_argument = argument ? save_expr(lower_node(argument)) : constant(0);
    tree list = alloc_stmt_list();
    append(&list, lowered_index);
    if (argument) append(&list, lowered_argument);
    cleanup(&list);
    if (!controls.empty() || saw_return || (access.readonly && std::string(operation) != "qmeasure")) {
        error_at(loc, "|c>: indexed operation requires classical control and a compatible mutable borrow"); return constant(0);
    }
    std::string ids = "[";
    for (tree storage : access.elements) {
        auto &var = variables.at(storage);
        if (!var.lowered || var.moved || var.released || var.derived || dependent(var.id) || readonly_quantum_borrows.count(storage)) {
            error_at(loc, "|c>: indexed access needs available storage without pending cleanup dependencies"); return constant(0);
        }
        consume(var); taint_uniform(var.id);
        if (ids.size() > 1) ids += ',';
        ids += std::to_string(var.id);
        var.clean = var.basis_known = var.pristine_uniform = false;
        if (std::string(operation) != "qmeasure" && std::string(operation) != "qreset") var.measured = false;
        var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1); ++var.epoch;
    }
    ids += ']';
    bool dynamic = argument != NULL_TREE;
    std::string action = "{\"op\":" + json(operation) + ",\"id\":" + std::to_string(variables.at(access.elements.front()).id);
    if (dynamic) action += ",\"dynamic\":true";
    if (std::string(operation) == "qfourier") action += std::string(",\"inverse\":") + (inverse ? "true" : "false");
    action += '}';
    std::string text = "{\"op\":\"qindexed\",\"ids\":" + ids + ",\"action\":" + action + '}';
    instructions.push_back(text);
    if (!apply_index_fn) {
        apply_index_fn = build_fn_decl("__cket_apply_index", build_function_type_list(long_long_unsigned_type_node,
            build_pointer_type(char_type_node), long_long_unsigned_type_node, long_long_unsigned_type_node,
            long_long_unsigned_type_node, NULL_TREE));
        TREE_PUBLIC(apply_index_fn) = DECL_EXTERNAL(apply_index_fn) = DECL_ARTIFICIAL(apply_index_fn) = 1;
    }
    tree call = build_call_expr_loc(loc, apply_index_fn, 4, build_string_literal(text.size() + 1, text.c_str()),
        lowered_index, constant(dynamic ? 1 : 0), dynamic ? fold_convert(long_long_unsigned_type_node, lowered_argument) : constant(0));
    tree sequence = call;
    std::vector<tree> prefix;
    for (auto it = tsi_start(list); !tsi_end_p(it); tsi_next(&it)) prefix.push_back(tsi_stmt(it));
    for (auto it = prefix.rbegin(); it != prefix.rend(); ++it) sequence = build2(COMPOUND_EXPR, TREE_TYPE(call), *it, sequence);
    return sequence;
}
tree lower_node(tree value) {
    if (!value) return value;
    auto code = TREE_CODE(value);
    if ((code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR) &&
        selected_storage(TREE_OPERAND(value, 0))) {
        tree selection = selected_storage(TREE_OPERAND(value, 0));
        tree type = TREE_TYPE(quantum_indexed_accesses.at(selection).elements.front());
        if (!TYPE_UNSIGNED(type) || TYPE_PRECISION(type) == 1) {
            error_at(at(value), "|c>: indexed increment currently requires an unsigned integer register"); return constant(0);
        }
        return lower_indexed_operation(selection, "qadd_inplace", build_int_cst(long_long_integer_type_node,
            code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR ? -1 : 1), false, at(value));
    }
    if (code == MODIFY_EXPR && selected_storage(TREE_OPERAND(value, 0))) {
        tree selection = selected_storage(TREE_OPERAND(value, 0)), rhs = strip(TREE_OPERAND(value, 1));
        auto operation = TREE_CODE(rhs);
        tree type = TREE_TYPE(quantum_indexed_accesses.at(selection).elements.front());
        tree operand = indexed_delta(TREE_OPERAND(value, 1), selection, TYPE_PRECISION(type), operation == BIT_XOR_EXPR);
        if (!operand || has_quantum(operand) || !TYPE_UNSIGNED(type) || TYPE_PRECISION(type) == 1) {
            error_at(at(value), "|c>: indexed update currently supports unsigned addition, subtraction or XOR with a classical operand"); return constant(0);
        }
        return lower_indexed_operation(selection, operation == BIT_XOR_EXPR ? "qxor_inplace" : "qadd_inplace", operand, false, at(value));
    }
    if (code == CALL_EXPR) {
        std::string kind = intrinsic(value);
        if ((kind == "measure" || kind == "reset" || kind == "fourier" || kind == "inverse_fourier") &&
            selected_storage(CALL_EXPR_ARG(value, 0)))
            return lower_indexed_operation(selected_storage(CALL_EXPR_ARG(value, 0)),
                kind == "measure" ? "qmeasure" : kind == "reset" ? "qreset" : "qfourier", NULL_TREE, kind == "inverse_fourier", at(value));
        if (quantum_indexed_accesses.count(value)) {
            error_at(at(value), "|c>: indexed quantum storage cannot be implicitly read or moved; use an implemented indexed operation or explicit measure"); return constant(0);
        }
    }
    if (CONVERT_EXPR_P(value) || code == NON_LVALUE_EXPR) {
        tree inner = strip(value); auto kind = TREE_CODE(inner);
        if (kind == PREINCREMENT_EXPR || kind == POSTINCREMENT_EXPR || kind == PREDECREMENT_EXPR || kind == POSTDECREMENT_EXPR)
            return lower_node(inner);
    }
    if (code == BIND_EXPR) { BIND_EXPR_BODY(value) = lower_node(BIND_EXPR_BODY(value)); return value; }
    if (code == STATEMENT_LIST) {
        tree result = alloc_stmt_list();
        for (auto it = tsi_start(value); !tsi_end_p(it); tsi_next(&it)) { append(&result, lower_node(tsi_stmt(it))); cleanup(&result); }
        return result;
    }
    if (code == COMPOUND_EXPR) {
        tree update = strip(TREE_OPERAND(value, 1));
        if (TREE_CODE(update) == MODIFY_EXPR && (quantum(strip(TREE_OPERAND(update, 0))) || selected_storage(TREE_OPERAND(update, 0))) &&
            !has_quantum(TREE_OPERAND(value, 0))) {
            // GCC saves a side-effecting compound-assignment operand before
            // updating the destination. Keep its shared SAVE_EXPR intact so
            // native C evaluates the operand exactly once.
            if (!controls.empty() && TREE_SIDE_EFFECTS(TREE_OPERAND(value, 0))) {
                error_at(at(value), "|c>: a classical side effect in an arithmetic operand cannot execute inside coherent quantum control");
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
        if (variables.count(decl)) return lower_quantum_decl(variables.at(decl));
        if (!controls.empty()) error_at(at(decl), "|c>: classical storage cannot be created coherently inside a quantum if");
        if (DECL_INITIAL(decl) && has_quantum(DECL_INITIAL(decl))) {
            error_at(at(decl), "|c>: classical initialization cannot read a quantum value; measure explicitly"); return value;
        }
        if (DECL_INITIAL(decl)) DECL_INITIAL(decl) = lower_node(DECL_INITIAL(decl));
        return value;
    }
    if (code == RETURN_EXPR) {
        if (has_quantum(TREE_OPERAND(value, 0))) { error_at(at(value), "|c>: returning a quantum value through the CPU requires explicit measure"); return value; }
        bool live = false;
        for (const auto &entry : variables) live |= entry.second.lowered && !entry.second.measured && !entry.second.released && !entry.second.moved;
        if (!controls.empty() || (live && dynamic_depth)) error_at(at(value), "|c>: return inside a quantum region needs proven cleanup");
        if (TREE_OPERAND(value, 0)) TREE_OPERAND(value, 0) = lower_node(TREE_OPERAND(value, 0));
        tree list = alloc_stmt_list();
        // The return expression may perform explicit measurement. Evaluate it
        // before emitting release calls, preserving actual native/IR order.
        tree operand = TREE_OPERAND(value, 0);
        if (operand && TREE_TYPE(operand) != void_type_node) {
            tree result = DECL_RESULT(active_function);
            if (TREE_CODE(operand) == MODIFY_EXPR && TREE_OPERAND(operand, 0) == result) append(&list, operand);
            else append(&list, build2(MODIFY_EXPR, TREE_TYPE(result), result, operand));
            TREE_OPERAND(value, 0) = result;
        }
        cleanup(&list); append(&list, value);
        if (!dynamic_depth) saw_return = true;
        return list;
    }
    if (code == COND_EXPR && TREE_TYPE(value) == void_type_node && has_quantum(TREE_OPERAND(value, 0))) {
        tree list = alloc_stmt_list();
        std::set<unsigned> deps;
        std::string predicate = expression(TREE_OPERAND(value, 0), deps);
        unsigned flag = next_id++;
        append(&list, emit_operation("{\"op\":\"qalloc\",\"id\":" + std::to_string(flag) + ",\"width\":1,\"signed\":false,\"ancilla\":true}", at(value)));
        append(&list, emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(flag) + ",\"expr\":" + predicate + '}', at(value)));
        // A live predicate's sources cannot be changed before its inverse.
        Variable guard{}; guard.id = flag; guard.lowered = guard.derived = true; guard.dependencies = deps;
        tree key = build_decl(at(value), VAR_DECL, get_identifier("__cket_guard"), boolean_type_node);
        variables.emplace(key, guard);
        controls.push_back({flag, 1}); append(&list, lower_node(TREE_OPERAND(value, 1))); controls.back().second = 0;
        append(&list, lower_node(TREE_OPERAND(value, 2))); controls.pop_back();
        variables.erase(key);
        append(&list, emit_operation("{\"op\":\"quncompute\",\"id\":" + std::to_string(flag) + ",\"expr\":" + predicate + '}', at(value)));
        append(&list, emit_operation("{\"op\":\"qrelease\",\"id\":" + std::to_string(flag) + '}', at(value)));
        return list;
    }
    if (code == COND_EXPR && TREE_TYPE(value) == void_type_node) {
        if (!controls.empty()) { error_at(at(value), "|c>: classical branch inside quantum control is not supported yet"); return value; }
        TREE_OPERAND(value, 0) = lower_node(TREE_OPERAND(value, 0));
        invalidate_cpu_ranges();
        auto incoming=variables;
        // Record dynamic CPU control explicitly, never flatten it for export.
        instructions.push_back("{\"op\":\"ccontrol_begin\"}"); ++dynamic_depth;
        TREE_OPERAND(value, 1) = lower_node(TREE_OPERAND(value, 1));
        auto then_state=variables;variables=incoming;
        TREE_OPERAND(value, 2) = lower_node(TREE_OPERAND(value, 2));
        auto else_state=variables;merge_cpu_paths(then_state,else_state,at(value));
        --dynamic_depth; instructions.push_back("{\"op\":\"ccontrol_end\"}"); return value;
    }
    if (code == WHILE_STMT || code == DO_STMT || code == FOR_STMT || code == LOOP_EXPR) {
        tree condition = code == WHILE_STMT ? WHILE_COND(value) : code == DO_STMT ? DO_COND(value) : code == FOR_STMT ? FOR_COND(value) : TREE_OPERAND(value, 0);
        if (has_quantum(condition) || !controls.empty()) { error_at(at(value), "|c>: quantum while/for requires a proven finite reversible iteration bound; no implicit measurement is allowed"); return value; }
        invalidate_cpu_ranges();auto incoming=variables;
        instructions.push_back("{\"op\":\"cloop_begin\"}"); ++dynamic_depth;
        if (code == WHILE_STMT) { WHILE_COND(value) = lower_node(WHILE_COND(value)); WHILE_BODY(value) = lower_node(WHILE_BODY(value)); }
        else if (code == DO_STMT) { DO_COND(value) = lower_node(DO_COND(value)); DO_BODY(value) = lower_node(DO_BODY(value)); }
        else if (code == FOR_STMT) {
            FOR_INIT_STMT(value) = lower_node(FOR_INIT_STMT(value));
            FOR_COND(value) = lower_node(FOR_COND(value));
            FOR_BODY(value) = lower_node(FOR_BODY(value)); FOR_EXPR(value) = lower_node(FOR_EXPR(value));
        } else TREE_OPERAND(value, 0) = lower_node(TREE_OPERAND(value, 0));
        auto back_edge=variables;merge_cpu_paths(incoming,back_edge,at(value));
        --dynamic_depth; instructions.push_back("{\"op\":\"cloop_end\"}"); return value;
    }
    if ((code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR) && quantum(strip(TREE_OPERAND(value, 0)))) {
        auto &var = variables.at(strip(TREE_OPERAND(value, 0)));
        // This statement synthesizes two references from one source reference.
        ++remaining[var.id];
        bool subtract = code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR;
        tree promoted = TYPE_PRECISION(TREE_TYPE(var.decl)) < TYPE_PRECISION(integer_type_node) ? integer_type_node : TYPE_MAIN_VARIANT(TREE_TYPE(var.decl));
        tree rhs = build2(subtract ? MINUS_EXPR : PLUS_EXPR, promoted, fold_convert(promoted, var.decl), build_int_cst(promoted, 1));
        tree assignment = build2(MODIFY_EXPR, TREE_TYPE(var.decl), var.decl, fold_convert(TREE_TYPE(var.decl), rhs));
        protected_set_expr_location(assignment, at(value));
        return lower_node(assignment);
    }
    if ((code == MODIFY_EXPR || code == INIT_EXPR) && quantum(strip(TREE_OPERAND(value, 0)))) {
        auto &var = variables.at(strip(TREE_OPERAND(value, 0))); consume(var);
        tree rhs = TREE_OPERAND(value, 1);
        tree list = alloc_stmt_list(); cleanup(&list);
        if (var.moved || var.released || dependent(var.id)) { error_at(at(value), "|c>: modifying this register would prevent uncomputation of a live dependent quantum value"); return list; }
        taint_uniform(var.id);
        for (const auto &control : controls) taint_uniform(control.first);
        tree simple = strip(rhs);
        tree addend = NULL_TREE;
        if (TREE_CODE(simple) == PLUS_EXPR || TREE_CODE(simple) == MINUS_EXPR) {
            if (strip(TREE_OPERAND(simple, 0)) == var.decl) addend = TREE_OPERAND(simple, 1);
            else if (TREE_CODE(simple) == PLUS_EXPR && strip(TREE_OPERAND(simple, 1)) == var.decl) addend = TREE_OPERAND(simple, 0);
        }
        tree quantum_addend = addend ? strip(addend) : NULL_TREE;
        tree xor_operand = NULL_TREE;
        if (TREE_CODE(simple) == BIT_XOR_EXPR && var.width > 1) {
            if (strip(TREE_OPERAND(simple, 0)) == var.decl) xor_operand = TREE_OPERAND(simple, 1);
            else if (strip(TREE_OPERAND(simple, 1)) == var.decl) xor_operand = TREE_OPERAND(simple, 0);
        }
        if (xor_operand) {
            consume(var);
            if (has_quantum(xor_operand)) {
                std::set<unsigned> deps;
                std::string expr = cast_result(expression(xor_operand, deps), var);
                if (deps.count(var.id)) {
                    error_at(at(value), "|c>: XOR operand cannot depend on the overwritten register"); return list;
                }
                for (unsigned source : deps) taint_uniform(source);
                append(&list, emit_operation("{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(var.id) +
                    ",\"expr\":" + expr + control_json() + '}', at(value)));
            } else {
                if (!controls.empty() && TREE_SIDE_EFFECTS(xor_operand)) {
                    error_at(at(value), "|c>: CPU effects in XOR cannot execute under coherent control"); return list;
                }
                tree operand = lower_node(xor_operand);
                if (TREE_CODE(strip(operand)) == INTEGER_CST)
                    append(&list, emit_operation("{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(var.id) +
                        ",\"value\":" + std::to_string(static_cast<int64_t>(TREE_INT_CST_LOW(strip(operand)))) + control_json() + '}', at(value)));
                else append(&list, emit_operation("{\"op\":\"qxor_inplace\",\"id\":" + std::to_string(var.id) +
                    ",\"dynamic\":true" + control_json() + '}', at(value), operand));
            }
        } else if (quantum(quantum_addend) && !var.is_signed && var.width > 1 &&
            !variables.at(quantum_addend).is_signed && quantum_addend != var.decl) {
            auto &source = variables.at(quantum_addend);
            if (!source.lowered || source.moved || source.released) {
                error_at(at(value), "|c>: quantum addition requires available borrowed source storage"); return list;
            }
            consume(var); consume(source); taint_uniform(source.id);
            append(&list, emit_operation("{\"op\":\"qadd_register\",\"id\":" + std::to_string(var.id) +
                ",\"source\":" + std::to_string(source.id) + ",\"subtract\":" +
                (TREE_CODE(simple) == MINUS_EXPR ? "true" : "false") + control_json() + '}', at(value)));
        } else if (addend && !has_quantum(addend) && !var.is_signed && var.width > 1) {
            if (!controls.empty() && TREE_SIDE_EFFECTS(addend)) {
                error_at(at(value), "|c>: a classical side effect in an arithmetic operand cannot execute inside coherent quantum control"); return list;
            }
            consume(var);
            tree argument = addend;
            if (TREE_CODE(simple) == MINUS_EXPR) argument = fold_build1(NEGATE_EXPR, TREE_TYPE(argument), argument);
            argument = lower_node(argument);
            if (TREE_CODE(strip(argument)) == INTEGER_CST) append(&list, emit_operation("{\"op\":\"qadd_inplace\",\"id\":" + std::to_string(var.id) + ",\"value\":" + std::to_string(static_cast<int64_t>(TREE_INT_CST_LOW(strip(argument)))) + control_json() + '}', at(value)));
            else append(&list, emit_operation("{\"op\":\"qadd_inplace\",\"id\":" + std::to_string(var.id) + ",\"dynamic\":true" + control_json() + '}', at(value), argument));
        } else {
            std::set<unsigned> deps;
            std::string expr = cast_result(expression(rhs, deps), var);
            if (var.clean && !var.derived && !deps.count(var.id) && controls.empty() && !dynamic_depth && !quantum(strip(rhs))) {
                var.derived = true; var.dependencies = deps; var.creation = expr;
                var.measured = var.clean = var.basis_known = false;
                var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
                append(&list, emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(var.id) + ",\"expr\":" + expr + '}', at(value)));
                return list;
            }
            if (deps != std::set<unsigned>{var.id}) { error_at(at(value), "|c>: assignment must preserve information; compute into a fresh quant variable, rather than overwriting this state"); return list; }
            append(&list, emit_operation("{\"op\":\"qinplace\",\"id\":" + std::to_string(var.id) + ",\"expr\":" + expr + control_json() + '}', at(value)));
        }
        var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1); ++var.epoch;
        var.measured = var.clean = var.basis_known = false;
        return list;
    }
    if (code == CALL_EXPR) {
        auto kind = intrinsic(value);
        if(kind=="circuit" || kind=="inverse_circuit") {
            tree filename=strip(CALL_EXPR_ARG(value,0));
            if(TREE_CODE(filename)==ADDR_EXPR) filename=TREE_OPERAND(filename,0);
            if(TREE_CODE(filename)!=STRING_CST) {error_at(at(value),"|c>: circuit module path must be a string literal");return constant(0);}
            std::string path=TREE_STRING_POINTER(filename);
            if(path.empty()) {error_at(at(value),"|c>: empty circuit module path");return constant(0);}
            if(path[0]!='/') {
                std::string source=LOCATION_FILE(at(value));size_t slash=source.find_last_of('/');
                if(slash!=std::string::npos) path=source.substr(0,slash+1)+path;
            }
            std::ifstream file(path,std::ios::binary | std::ios::ate);
            auto size=file.tellg();
            if(!file || size<=0 || size>64*1024*1024 || (size_t)size>64*1024*1024-embedded_module_bytes) {error_at(at(value),"|c>: cannot read circuit module or embedded modules exceed 64 MiB");return constant(0);}
            std::string module((size_t)size,'\0');file.seekg(0);file.read(&module[0],size);
            if(!file) {error_at(at(value),"|c>: cannot read complete circuit module");return constant(0);}
            embedded_module_bytes+=module.size();
            std::string ids="[";
            for(int j=1;j<call_expr_nargs(value);++j) {
                tree storage=strip(CALL_EXPR_ARG(value,j));auto found=variables.find(storage);
                if(found==variables.end() || !found->second.lowered || found->second.moved || found->second.released || found->second.derived || dependent(found->second.id) || readonly_quantum_borrows.count(storage)) {
                    error_at(at(value),"|c>: circuit module needs available mutable typed register borrows");return constant(0);
                }
                auto &var=found->second;consume(var);if(j>1) ids+=',';ids+=std::to_string(var.id);
                var.basis_known=var.pristine_uniform=var.measured=var.clean=false;
                var.range_max=var.width==64?UINT64_MAX:((uint64_t{1}<<var.width)-1);++var.epoch;
            }
            ids+=']';
            return emit_operation("{\"op\":\"qmodule\",\"ids\":"+ids+",\"inverse\":"+(kind=="inverse_circuit"?"true":"false")+",\"circuit\":"+module+control_json()+'}',at(value));
        }
        if(kind=="search") {
            tree storage=strip(CALL_EXPR_ARG(value,0)),count=strip(CALL_EXPR_ARG(value,2));
            auto found=variables.find(storage);
            if(!controls.empty() || found==variables.end() || !found->second.lowered || found->second.moved || found->second.released ||
                !found->second.pristine_uniform || found->second.derived || dependent(found->second.id) || readonly_quantum_borrows.count(storage)) {
                error_at(at(value),"|c>: search needs an independent pristine uniform mutable register under classical control");return constant(0);
            }
            if(TREE_CODE(count)!=INTEGER_CST || !tree_fits_uhwi_p(count) || !tree_to_uhwi(count) || tree_to_uhwi(count)>4096) {
                error_at(at(value),"|c>: search requires a constant attempt budget in 1..4096");return constant(0);
            }
            auto &var=found->second;consume(var);std::set<unsigned> deps;
            std::string predicate=expression(CALL_EXPR_ARG(value,1),deps);
            if(deps.size()!=1 || !deps.count(var.id)) error_at(at(value),"|c>: search predicate must depend only on its input");
            std::string operation="{\"op\":\"qsearch\",\"id\":"+std::to_string(var.id)+",\"upper\":"+std::to_string(var.upper)+",\"attempts\":"+std::to_string(tree_to_uhwi(count))+",\"predicate\":"+predicate+'}';
            var.measured=true;var.pristine_uniform=var.basis_known=var.clean=false;var.range_max=var.width==64?UINT64_MAX:((uint64_t{1}<<var.width)-1);++var.epoch;
            return fold_convert(boolean_type_node,emit_operation(operation,at(value)));
        }
        if (kind == "fourier" || kind == "inverse_fourier") {
            tree storage = strip(CALL_EXPR_ARG(value, 0));
            auto found = variables.find(storage);
            tree list = alloc_stmt_list(); cleanup(&list);
            if (found == variables.end() || !found->second.lowered || found->second.moved || found->second.released ||
                found->second.derived || dependent(found->second.id)) {
                error_at(at(value), "|c>: Fourier transform needs available owned storage without pending inverse dependencies");
                return list;
            }
            auto &var = found->second; consume(var);
            taint_uniform(var.id); for (const auto &control : controls) taint_uniform(control.first);
            var.basis_known = var.pristine_uniform = var.clean = var.measured = false;
            var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1); ++var.epoch;
            append(&list, emit_operation("{\"op\":\"qfourier\",\"id\":" + std::to_string(var.id) +
                ",\"inverse\":" + (kind == "inverse_fourier" ? "true" : "false") + control_json() + '}', at(value)));
            return list;
        }
        if (kind == "phase") {
            tree angle = CALL_EXPR_ARG(value, 0);
            if (has_quantum(angle) || (!controls.empty() && TREE_SIDE_EFFECTS(angle))) {
                error_at(at(value), "|c>: phase angle must be classical and cannot perform CPU effects under coherent control");
                return constant(0);
            }
            for (const auto &control : controls) taint_uniform(control.first);
            tree literal = fold(angle);
            if (TREE_CODE(literal) == REAL_CST) {
                char number[128]; real_to_decimal(number, TREE_REAL_CST_PTR(literal), sizeof number, 0, 1);
                if (!std::isfinite(std::strtod(number, nullptr))) {
                    error_at(at(value), "|c>: phase requires a finite angle"); return constant(0);
                }
                return emit_operation("{\"op\":\"qphase\",\"angle\":" + std::string(number) + control_json() + '}', at(value));
            }
            std::string operation = "{\"op\":\"qphase\",\"dynamic\":true" + control_json() + '}';
            instructions.push_back(operation);
            if (!apply_phase_fn) {
                tree args = tree_cons(NULL_TREE, void_type_node, NULL_TREE);
                args = tree_cons(NULL_TREE, double_type_node, args);
                args = tree_cons(NULL_TREE, build_pointer_type(char_type_node), args);
                apply_phase_fn = build_fn_decl("__cket_apply_phase", build_function_type(long_long_unsigned_type_node, args));
                TREE_PUBLIC(apply_phase_fn) = DECL_EXTERNAL(apply_phase_fn) = DECL_ARTIFICIAL(apply_phase_fn) = 1;
            }
            return build_call_expr_loc(at(value), apply_phase_fn, 2,
                build_string_literal(operation.size() + 1, operation.c_str()), lower_node(angle));
        }
        if (kind == "phase_flip") {
            for (const auto &control : controls) taint_uniform(control.first);
            return emit_operation("{\"op\":\"qphase\",\"angle\":3.141592653589793" + control_json() + '}', at(value));
        }
        if (kind == "measure" || kind == "reset") {
            if (!controls.empty() || saw_return) { error_at(at(value), "|c>: measurement/reset requires explicit classical control and a proven register lifetime"); return constant(0); }
            tree arg = strip(CALL_EXPR_ARG(value, 0)); auto found = variables.find(arg);
            if (found == variables.end() || !found->second.lowered || found->second.released || found->second.moved) { error_at(at(value), "|c>: measurement/reset requires available quantum storage"); return constant(0); }
            auto &var = found->second; consume(var);
            taint_uniform(var.id);
            // This explicit destruction consumes the computed output. Do not
            // automatically inverse that output before the requested measure.
            bool was_clean = var.clean;
            var.measured = true; var.clean = false;
            tree list = alloc_stmt_list(); cleanup(&list);
            var.clean = was_clean;
            if (dependent(var.id)) { error_at(at(value), "|c>: measurement/reset would invalidate a live computed quantum value; use or uncompute it first"); return constant(0); }
            tree call;
            if (kind == "measure") {
                if (!measure_fn) measure_fn = runtime_function("__cket_measure", 1);
                call = build_call_expr_loc(at(value), measure_fn, 1, constant(var.id));
                instructions.push_back("{\"op\":\"qmeasure\",\"id\":" + std::to_string(var.id) + ",\"result\":\"m" + std::to_string(next_measurement++) + "\",\"width\":" + std::to_string(var.width) + '}');
                var.measured = true;
                ++var.epoch;
            } else {
                call = emit_operation("{\"op\":\"qreset\",\"id\":" + std::to_string(var.id) + '}', at(value));
                var.measured = var.clean = var.basis_known = true;
                var.lower = var.upper = var.range_max = 0; ++var.epoch;
                var.derived = false; var.dependencies.clear(); var.creation.clear();
            }
            // GCC COMPOUND_EXPR preserves cleanup-before-measure inside printf/initializers.
            tree sequence = call;
            std::vector<tree> prefix; for (auto it = tsi_start(list); !tsi_end_p(it); tsi_next(&it)) prefix.push_back(tsi_stmt(it));
            for (auto it = prefix.rbegin(); it != prefix.rend(); ++it) sequence = build2(COMPOUND_EXPR, TREE_TYPE(call), *it, sequence);
            return sequence;
        }
        if (!kind.empty()) { error_at(at(value), "|c>: preparation/find is only valid as a quant initializer"); return constant(0); }
        if (!controls.empty()) { error_at(at(value), "|c>: classical calls and I/O cannot execute coherently inside a quantum if"); return value; }
        if (has_quantum(value)) { error_at(at(value), "|c>: classical call arguments cannot read quantum values; measure explicitly"); return value; }
    }
    if (quantum(value)) { error_at(at(value), "|c>: a quantum value cannot become a CPU value implicitly; use measure explicitly"); return value; }
    if (DECL_P(value) || TYPE_P(value)) return value;
    if (code == GOTO_EXPR && !variables.empty()) {
        error_at(at(value), "|c>: goto with quantum storage requires control-flow lifetime analysis and is not supported yet"); return value;
    }
    if (has_quantum(value)) { error_at(at(value), "|c>: this C context cannot consume an unmeasured quantum result; use quant output storage or explicit measure"); return value; }
    if (!controls.empty() && (code == MODIFY_EXPR || code == INIT_EXPR || code == GOTO_EXPR || code == BREAK_STMT || code == CONTINUE_STMT ||
        code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR || code == ASM_EXPR)) {
        error_at(at(value), "|c>: classical side effects cannot execute inside coherent quantum control"); return value;
    }
    // GCC knows how to walk constructors, lists, vectors and variable-arity
    // nodes. Skip the root once, then rewrite each child with this pass.
    bool root = true;
    auto children = [](tree *node, int *walk, void *data) -> tree {
        bool &first = *static_cast<bool *>(data);
        if (first) { first = false; return NULL_TREE; }
        *walk = 0; *node = lower_node(*node); return NULL_TREE;
    };
    walk_tree(&value, children, &root, nullptr);
    return value;
}
