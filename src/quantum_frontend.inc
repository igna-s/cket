// Composition and fixed products on GCC trees. This is not a C parser.
// Quantum function definitions are templates whose effects are checked again
// at each caller by the ordinary quantum semantic pass.
tree expand_quantum_program(tree value);
bool standard_integer_division(tree fn);
std::map<tree, std::map<std::string, tree>> quantum_products;

struct QuantumReference {
    std::vector<tree> elements;
    int64_t offset = 0;
    tree element_type = NULL_TREE;
};

std::map<tree, QuantumReference> quantum_references;
std::set<tree> quantum_division_products;
bool resolve_quantum_reference(tree value, QuantumReference &reference);
tree reference_element(tree value);

bool gcc_quantum_sanitizer_guard(tree value) {
    if (!value || TREE_CODE(value) != COND_EXPR)
        return false;
    tree check = TREE_OPERAND(value, 1), absent = TREE_OPERAND(value, 2);
    if (!check || TREE_CODE(check) != CALL_EXPR || !absent || TREE_CODE(absent) != VOID_CST)
        return false;
    tree fn = get_callee_fndecl(check);
    return fn && (fndecl_built_in_p(fn, BUILT_IN_UBSAN_HANDLE_DIVREM_OVERFLOW) ||
                  fndecl_built_in_p(fn, BUILT_IN_UBSAN_HANDLE_DIVREM_OVERFLOW_ABORT) ||
                  fndecl_built_in_p(fn, BUILT_IN_UBSAN_HANDLE_SHIFT_OUT_OF_BOUNDS) ||
                  fndecl_built_in_p(fn, BUILT_IN_UBSAN_HANDLE_SHIFT_OUT_OF_BOUNDS_ABORT));
}

bool quantum_pure_expression(tree value) {
    if (!value)
        return true;
    if (quantum(value) || CONSTANT_CLASS_P(value) || TYPE_P(value))
        return true;
    if (!has_quantum(value) && INTEGRAL_TYPE_P(TREE_TYPE(value)) && !TREE_SIDE_EFFECTS(value) &&
        !TREE_THIS_VOLATILE(value) && !TYPE_VOLATILE(TREE_TYPE(value)))
        return true;
    if (DECL_P(value))
        return TREE_CODE(value) == VAR_DECL && !TREE_THIS_VOLATILE(value);
    auto code = TREE_CODE(value);
    if (code == COMPOUND_EXPR && gcc_quantum_sanitizer_guard(TREE_OPERAND(value, 0)))
        return quantum_pure_expression(TREE_OPERAND(value, 1));
    if (code == CALL_EXPR || code == MODIFY_EXPR || code == INIT_EXPR || code == COMPOUND_EXPR ||
        code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR ||
        code == POSTDECREMENT_EXPR || code == ADDR_EXPR)
        return false;
    if (!EXPR_P(value))
        return false;
    for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
        if (!quantum_pure_expression(TREE_OPERAND(value, i)))
            return false;
    return true;
}

tree clone_quantum_tree(tree value, std::map<tree, tree> &bindings) {
    if (!value)
        return value;
    auto bound = bindings.find(value);
    if (bound != bindings.end())
        return bound->second;
    if (DECL_P(value) || TYPE_P(value) || CONSTANT_CLASS_P(value))
        return value;
    if (TREE_CODE(value) == STATEMENT_LIST) {
        tree result = alloc_stmt_list();
        for (auto it = tsi_start(value); !tsi_end_p(it); tsi_next(&it))
            append(&result, clone_quantum_tree(tsi_stmt(it), bindings));
        return result;
    }
    if (TREE_CODE(value) == BIND_EXPR) {
        tree result = copy_node(value), first = NULL_TREE, last = NULL_TREE;
        for (tree var = BIND_EXPR_VARS(value); var; var = DECL_CHAIN(var)) {
            tree fresh = copy_node(var);
            DECL_CONTEXT(fresh) = active_function;
            DECL_CHAIN(fresh) = NULL_TREE;
            bindings[var] = fresh;
            if (!first)
                first = fresh;
            else
                DECL_CHAIN(last) = fresh;
            last = fresh;
        }
        for (tree var = BIND_EXPR_VARS(value); var; var = DECL_CHAIN(var))
            if (DECL_INITIAL(var))
                DECL_INITIAL(bindings[var]) = clone_quantum_tree(DECL_INITIAL(var), bindings);
        BIND_EXPR_VARS(result) = first;
        BIND_EXPR_BODY(result) = clone_quantum_tree(BIND_EXPR_BODY(value), bindings);
        BIND_EXPR_BLOCK(result) = NULL_TREE;
        return result;
    }
    tree result = copy_node(value);
    // GCC shares SAVE_EXPR to evaluate a CPU operand once. Preserve that
    // identity across all references within this function/iteration clone.
    if (TREE_CODE(value) == SAVE_EXPR)
        bindings[value] = result;
    if (TREE_CODE(value) == DECL_EXPR) {
        tree original = DECL_EXPR_DECL(value);
        if (!bindings.count(original)) {
            tree fresh = copy_node(original);
            DECL_CONTEXT(fresh) = active_function;
            DECL_CHAIN(fresh) = NULL_TREE;
            bindings[original] = fresh;
            if (DECL_INITIAL(original))
                DECL_INITIAL(fresh) = clone_quantum_tree(DECL_INITIAL(original), bindings);
        }
        DECL_EXPR_DECL(result) = bindings[original];
        return result;
    }
    if (TREE_CODE(value) == CONSTRUCTOR) {
        vec<constructor_elt, va_gc> *elts = NULL;
        unsigned ix;
        tree index, element;
        FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(value), ix, index, element)
        CONSTRUCTOR_APPEND_ELT(elts, index, clone_quantum_tree(element, bindings));
        CONSTRUCTOR_ELTS(result) = elts;
        return result;
    }
    if (EXPR_P(value)) {
        for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
            TREE_OPERAND(result, i) = clone_quantum_tree(TREE_OPERAND(value, i), bindings);
        return result;
    }
    return result;
}

tree returned_expression(tree body, bool &valid, unsigned &returns) {
    if (!body)
        return NULL_TREE;
    if (TREE_CODE(body) == BIND_EXPR) {
        if (BIND_EXPR_VARS(body))
            valid = false;
        return returned_expression(BIND_EXPR_BODY(body), valid, returns);
    }
    if (TREE_CODE(body) == STATEMENT_LIST) {
        tree result = NULL_TREE;
        for (auto it = tsi_start(body); !tsi_end_p(it); tsi_next(&it)) {
            tree found = returned_expression(tsi_stmt(it), valid, returns);
            if (found)
                result = found;
        }
        return result;
    }
    if (TREE_CODE(body) == RETURN_EXPR) {
        ++returns;
        tree expr = TREE_OPERAND(body, 0);
        if (expr && (TREE_CODE(expr) == MODIFY_EXPR || TREE_CODE(expr) == INIT_EXPR))
            expr = TREE_OPERAND(expr, 1);
        return expr;
    }
    if (TREE_CODE(body) == COND_EXPR && TREE_TYPE(body) == void_type_node) {
        unsigned yes_returns = 0, no_returns = 0;
        tree yes = returned_expression(TREE_OPERAND(body, 1), valid, yes_returns);
        tree no = returned_expression(TREE_OPERAND(body, 2), valid, no_returns);
        if (!yes || !no || yes_returns != 1 || no_returns != 1) {
            valid = false;
            return NULL_TREE;
        }
        ++returns;
        return build3(COND_EXPR, TREE_TYPE(yes), TREE_OPERAND(body, 0), yes, no);
    }
    if (!(TREE_CODE(body) == NOP_EXPR && TREE_TYPE(body) == void_type_node))
        valid = false;
    return NULL_TREE;
}

bool quantum_return_is_pure(tree value) {
    if (!value)
        return true;
    if (DECL_P(value))
        return TREE_CODE(value) == PARM_DECL ||
               (TREE_CODE(value) == VAR_DECL && !TREE_THIS_VOLATILE(value));
    if (CONSTANT_CLASS_P(value) || TYPE_P(value))
        return true;
    if (TREE_CODE(value) == COMPOUND_EXPR && gcc_quantum_sanitizer_guard(TREE_OPERAND(value, 0)))
        return quantum_return_is_pure(TREE_OPERAND(value, 1));
    if (TREE_CODE(value) == CALL_EXPR) {
        if (!quantum_function(get_callee_fndecl(value)))
            return false;
        for (int i = 0; i < call_expr_nargs(value); ++i)
            if (!quantum_return_is_pure(CALL_EXPR_ARG(value, i)))
                return false;
        return true;
    }
    if (!EXPR_P(value) || TREE_CODE(value) == MODIFY_EXPR || TREE_CODE(value) == INIT_EXPR ||
        TREE_CODE(value) == COMPOUND_EXPR || TREE_CODE(value) == ADDR_EXPR ||
        TREE_CODE(value) == PREINCREMENT_EXPR || TREE_CODE(value) == POSTINCREMENT_EXPR ||
        TREE_CODE(value) == PREDECREMENT_EXPR || TREE_CODE(value) == POSTDECREMENT_EXPR)
        return false;
    for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
        if (!quantum_return_is_pure(TREE_OPERAND(value, i)))
            return false;
    return true;
}

bool initializer_consumes_borrow(tree value) {
    if (!value)
        return false;
    value = strip(value);
    if (TREE_CODE(value) == PARM_DECL && lookup_attribute("cket_quant", DECL_ATTRIBUTES(value)))
        return true;
    if (TREE_CODE(value) == INDIRECT_REF || TREE_CODE(value) == ARRAY_REF) {
        tree base = TREE_OPERAND(value, 0);
        auto borrowed_parameter = [](tree *node, int *walk, void *) -> tree {
            if (TREE_CODE(*node) == PARM_DECL &&
                lookup_attribute("cket_quant", DECL_ATTRIBUTES(*node)))
                return *node;
            if (DECL_P(*node) || TYPE_P(*node))
                *walk = 0;
            return NULL_TREE;
        };
        if (walk_tree(&base, borrowed_parameter, nullptr, nullptr))
            return true;
    }
    if (TREE_CODE(value) == CONSTRUCTOR) {
        unsigned i;
        tree index, element;
        FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(value), i, index, element)
        if (initializer_consumes_borrow(element))
            return true;
        (void)index;
    }
    return false;
}

tree *terminal_return_slot(tree *body) {
    while (*body && TREE_CODE(*body) == BIND_EXPR)
        body = &BIND_EXPR_BODY(*body);
    if (*body && TREE_CODE(*body) == STATEMENT_LIST) {
        auto last = tsi_last(*body);
        if (tsi_end_p(last))
            return nullptr;
        body = tsi_stmt_ptr(last);
    }
    return *body && TREE_CODE(*body) == RETURN_EXPR ? body : nullptr;
}

void capture_quantum_function(tree fn) {
    if (name_of(fn) == "main") {
        error_at(
            at(fn),
            "|c>: main must have a classical C return; declare quant storage inside it and measure explicitly");
        return;
    }
    tree result = TREE_TYPE(TREE_TYPE(fn)), body = DECL_SAVED_TREE(fn);
    bool quantum_result =
        lookup_attribute("cket_quant", DECL_ATTRIBUTES(fn)) && result != void_type_node;
    if (result != void_type_node && !quantum_result) {
        error_at(
            at(fn),
            "|c>: a function returning an unmeasured quantum expression must declare a quant result");
        return;
    }
    bool owned_result = false;
    if (quantum_result) {
        bool valid = true;
        unsigned returns = 0;
        tree expr = returned_expression(body, valid, returns);
        if (!valid) {
            tree copy = body;
            tree *slot = terminal_return_slot(&copy);
            if (slot && returns == 1) {
                expr = TREE_OPERAND(*slot, 0);
                if (expr && (TREE_CODE(expr) == MODIFY_EXPR || TREE_CODE(expr) == INIT_EXPR))
                    expr = TREE_OPERAND(expr, 1);
                owned_result = expr && INTEGRAL_TYPE_P(result);
            }
        }
        if ((!valid && !owned_result) || returns != 1 || !expr ||
            (!owned_result && !quantum_return_is_pure(expr))) {
            error_at(
                at(fn),
                "|c>: quantum value functions currently require one pure return expression; statement bodies use void procedures");
            return;
        }
        tree direct = strip(expr);
        if (initializer_consumes_borrow(direct)) {
            error_at(
                at(fn),
                "|c>: returning a borrowed quantum parameter directly would ambiguously copy or consume it; compute an expression instead");
            return;
        }
    }
    quantum_functions[name_of(fn)] = {fn, body, quantum_result, owned_result};
    // No CPU entry point may execute the template with classical integer bits.
    // All direct calls are expanded; addresses and indirect calls are rejected.
    tree operand = result == void_type_node
                       ? NULL_TREE
                       : build2(MODIFY_EXPR, result, DECL_RESULT(fn), build_zero_cst(result));
    DECL_SAVED_TREE(fn) = build1(RETURN_EXPR, void_type_node, operand);
}

tree instantiate_quantum_call(tree call, tree destination = NULL_TREE) {
    tree fn = get_callee_fndecl(call);
    auto found = quantum_functions.find(name_of(fn));
    if (found == quantum_functions.end()) {
        error_at(
            at(call),
            "|c>: quantum function definition must be available before use in this translation unit");
        return call;
    }
    if (expanding_functions.size() >= 64 || !expanding_functions.insert(name_of(fn)).second) {
        error_at(at(call),
                 "|c>: recursive quantum composition requires a proven finite expansion bound");
        return call;
    }
    auto &function = found->second;
    if (function.owned_result && !destination) {
        expanding_functions.erase(name_of(fn));
        error_at(at(call),
                 "|c>: owned quantum function results require a new scalar quant initializer");
        return call;
    }
    auto previous_readonly = readonly_quantum_borrows;
    std::map<tree, tree> bindings;
    std::set<tree> borrowed;
    tree prefix = alloc_stmt_list();
    unsigned index = 0;
    bool valid = true;
    for (tree param = DECL_ARGUMENTS(function.decl); param; param = DECL_CHAIN(param), ++index) {
        if (index >= (unsigned)call_expr_nargs(call)) {
            valid = false;
            break;
        }
        tree argument = CALL_EXPR_ARG(call, index);
        if (lookup_attribute("cket_quant", DECL_ATTRIBUTES(param)) &&
            TREE_CODE(TREE_TYPE(param)) == POINTER_TYPE) {
            QuantumReference reference;
            if (!resolve_quantum_reference(argument, reference) ||
                TYPE_MAIN_VARIANT(TREE_TYPE(TREE_TYPE(param))) !=
                    TYPE_MAIN_VARIANT(reference.element_type) ||
                (TYPE_READONLY(reference.element_type) &&
                 !TYPE_READONLY(TREE_TYPE(TREE_TYPE(param))))) {
                error_at(
                    at(call),
                    "|c>: quantum pointer parameter requires a checked borrow of compatible quantum storage");
                valid = false;
                break;
            }
            if (!function.result || function.owned_result)
                for (size_t i = (size_t)reference.offset; i < reference.elements.size(); ++i)
                    if (!borrowed.insert(reference.elements[i]).second) {
                        error_at(
                            at(call),
                            "|c>: mutable quantum pointer parameters cannot overlap other borrowed storage");
                        valid = false;
                    }
            for (size_t i = (size_t)reference.offset; i < reference.elements.size(); ++i) {
                tree storage = reference.elements[i];
                if ((!function.result || function.owned_result) &&
                    !TYPE_READONLY(TREE_TYPE(TREE_TYPE(param))) &&
                    readonly_quantum_borrows.count(storage)) {
                    error_at(
                        at(call),
                        "|c>: read-only quantum borrow cannot be passed to a mutable parameter");
                    valid = false;
                }
                if (TYPE_READONLY(TREE_TYPE(TREE_TYPE(param))))
                    readonly_quantum_borrows.insert(storage);
            }
            reference.element_type = TREE_TYPE(TREE_TYPE(param));
            tree marker = build_decl(
                at(call), VAR_DECL, get_identifier("__cket_reference"), TREE_TYPE(param));
            DECL_CONTEXT(marker) = active_function;
            quantum_references[marker] = reference;
            bindings[param] = marker;
        } else if (quantum(param)) {
            tree storage = strip(argument);
            if (!quantum(storage) ||
                TYPE_MAIN_VARIANT(TREE_TYPE(param)) != TYPE_MAIN_VARIANT(TREE_TYPE(storage))) {
                error_at(
                    at(call),
                    "|c>: quantum argument must borrow storage of the same C type without conversion");
                valid = false;
                break;
            }
            if ((!function.result || function.owned_result) && !borrowed.insert(storage).second) {
                error_at(at(call),
                         "|c>: mutable quantum parameters cannot alias the same register");
                valid = false;
                break;
            }
            if ((!function.result || function.owned_result) && !TYPE_READONLY(TREE_TYPE(param)) &&
                readonly_quantum_borrows.count(storage)) {
                error_at(at(call),
                         "|c>: read-only quantum borrow cannot be passed to a mutable parameter");
                valid = false;
            }
            if (TYPE_READONLY(TREE_TYPE(param)))
                readonly_quantum_borrows.insert(storage);
            bindings[param] = storage;
        } else {
            if (has_quantum(argument)) {
                error_at(at(call),
                         "|c>: classical parameter cannot implicitly read a quantum argument");
                valid = false;
                break;
            }
            if (TREE_SIDE_EFFECTS(argument) && function.result && !function.owned_result) {
                error_at(at(call), "|c>: quantum value functions require pure classical arguments");
                valid = false;
                break;
            }
            if (TREE_CODE(strip(argument)) == INTEGER_CST)
                bindings[param] = argument;
            else {
                tree saved = save_expr(argument);
                bindings[param] = saved;
                append(&prefix, saved); // Evaluate CPU arguments once, before effects.
            }
        }
    }
    if (index != (unsigned)call_expr_nargs(call))
        valid = false;
    tree result = call;
    if (valid) {
        if (function.result && !function.owned_result) {
            bool pure = true;
            unsigned returns = 0;
            tree expr = returned_expression(function.body, pure, returns);
            result = expand_quantum_program(clone_quantum_tree(expr, bindings));
        } else {
            // Borrowed parameters cannot be consumed by local moves/find.
            tree body = function.body;
            auto inspect = [](tree *node, int *walk, void *data) -> tree {
                if (TREE_CODE(*node) == DECL_EXPR) {
                    tree var = DECL_EXPR_DECL(*node), init = strip(DECL_INITIAL(var));
                    if (lookup_attribute("cket_quant", DECL_ATTRIBUTES(var)) &&
                        initializer_consumes_borrow(init))
                        error_at(
                            at(var),
                            "|c>: a borrowed parameter cannot be consumed by an ownership move");
                    if (init && (intrinsic(init) == "find" || intrinsic(init) == "find_known")) {
                        tree source = strip(CALL_EXPR_ARG(init, 0));
                        if (initializer_consumes_borrow(source))
                            error_at(at(var),
                                     "|c>: find cannot consume a borrowed function parameter");
                    }
                }
                if ((intrinsic(*node) == "find" || intrinsic(*node) == "find_known")) {
                    tree source = strip(CALL_EXPR_ARG(*node, 0));
                    if (initializer_consumes_borrow(source))
                        error_at(at(*node),
                                 "|c>: find cannot consume a borrowed function parameter");
                }
                if (!*static_cast<bool *>(data) && TREE_CODE(*node) == RETURN_EXPR &&
                    TREE_OPERAND(*node, 0))
                    error_at(at(*node), "|c>: quantum procedures must return void");
                if (DECL_P(*node) || TYPE_P(*node))
                    *walk = 0;
                return NULL_TREE;
            };
            walk_tree(&body, inspect, &function.owned_result, nullptr);
            tree expanded = clone_quantum_tree(body, bindings);
            if (function.owned_result) {
                tree *slot = terminal_return_slot(&expanded);
                tree result = TREE_OPERAND(*slot, 0);
                if (TREE_CODE(result) == MODIFY_EXPR || TREE_CODE(result) == INIT_EXPR)
                    result = TREE_OPERAND(result, 1);
                DECL_INITIAL(destination) = fold_convert(TREE_TYPE(destination), result);
                *slot = build1(DECL_EXPR, void_type_node, destination);
            }
            tree *terminal_slot = &expanded;
            while (TREE_CODE(*terminal_slot) == BIND_EXPR)
                terminal_slot = &BIND_EXPR_BODY(*terminal_slot);
            tree terminal_body = *terminal_slot;
            if (TREE_CODE(terminal_body) == STATEMENT_LIST) {
                auto last = tsi_last(terminal_body);
                if (!tsi_end_p(last) && TREE_CODE(tsi_stmt(last)) == RETURN_EXPR &&
                    !TREE_OPERAND(tsi_stmt(last), 0))
                    *tsi_stmt_ptr(last) = build_empty_stmt(at(tsi_stmt(last)));
            } else if (TREE_CODE(terminal_body) == RETURN_EXPR && !TREE_OPERAND(terminal_body, 0)) {
                *terminal_slot = build_empty_stmt(at(terminal_body));
            }
            // A final return; ends the procedure, not its caller.
            auto remove_return = [](tree *node, int *walk, void *) -> tree {
                if (TREE_CODE(*node) == RETURN_EXPR) {
                    if (TREE_OPERAND(*node, 0))
                        error_at(at(*node), "|c>: unexpected value in quantum procedure return");
                    *node = build_empty_stmt(at(*node));
                    *walk = 0;
                } else if (DECL_P(*node) || TYPE_P(*node))
                    *walk = 0;
                return NULL_TREE;
            };
            // Early/conditional returns are rejected rather than discarded.
            auto check_return = [](tree *node, int *walk, void *) -> tree {
                if (TREE_CODE(*node) == RETURN_EXPR)
                    error_at(
                        at(*node),
                        "|c>: early or conditional returns in quantum procedures need control-flow analysis");
                if (DECL_P(*node) || TYPE_P(*node))
                    *walk = 0;
                return NULL_TREE;
            };
            walk_tree(&expanded, check_return, nullptr, nullptr);
            walk_tree(&expanded, remove_return, nullptr, nullptr);
            expanded = expand_quantum_program(expanded);
            auto check_consumption = [](tree *node, int *walk, void *data) -> tree {
                auto &loans = *static_cast<std::set<tree> *>(data);
                if (TREE_CODE(*node) == DECL_EXPR) {
                    tree var = DECL_EXPR_DECL(*node), init = strip(DECL_INITIAL(var));
                    if (quantum(var) && init && loans.count(init))
                        error_at(
                            at(var),
                            "|c>: ownership move cannot consume borrowed quantum storage through an alias");
                }
                if ((intrinsic(*node) == "find" || intrinsic(*node) == "find_known") &&
                    loans.count(strip(CALL_EXPR_ARG(*node, 0))))
                    error_at(at(*node),
                             "|c>: find cannot consume borrowed quantum storage through an alias");
                if (DECL_P(*node) || TYPE_P(*node))
                    *walk = 0;
                return NULL_TREE;
            };
            walk_tree(&expanded, check_consumption, &borrowed, nullptr);
            append(&prefix, expanded);
            result = prefix;
        }
    } else
        error_at(at(call), "|c>: invalid quantum function arguments");
    expanding_functions.erase(name_of(fn));
    readonly_quantum_borrows = previous_readonly;
    return result;
}

// Fixed arrays and structures become independently owned scalar registers.
unsigned expanded_product_leaves = 0;

tree classical_product_constructor(tree type, tree value, unsigned &nodes) {
    if (++nodes > 4096) {
        error_at(at(value), "|c>: classical product preparation exceeds 4096 nodes");
        return NULL_TREE;
    }
    if (TREE_CODE(type) != ARRAY_TYPE && TREE_CODE(type) != RECORD_TYPE)
        return value;
    vec<constructor_elt, va_gc> *elements = NULL;
    if (TREE_CODE(type) == ARRAY_TYPE) {
        uint64_t count = tree_to_uhwi(TYPE_MAX_VALUE(TYPE_DOMAIN(type))) + 1;
        for (uint64_t i = 0; i < count; ++i) {
            tree index = size_int(i);
            tree selected = build4(ARRAY_REF, TREE_TYPE(type), value, index, NULL_TREE, NULL_TREE);
            tree child = classical_product_constructor(TREE_TYPE(type), selected, nodes);
            if (!child)
                return NULL_TREE;
            CONSTRUCTOR_APPEND_ELT(elements, index, child);
        }
    } else {
        for (tree field = TYPE_FIELDS(type); field; field = DECL_CHAIN(field))
            if (TREE_CODE(field) == FIELD_DECL) {
                tree selected = build3(COMPONENT_REF, TREE_TYPE(field), value, field, NULL_TREE);
                tree child = classical_product_constructor(TREE_TYPE(field), selected, nodes);
                if (!child)
                    return NULL_TREE;
                CONSTRUCTOR_APPEND_ELT(elements, field, child);
            }
    }
    return build_constructor(type, elements);
}

tree constructor_element(tree init, tree key, uint64_t number, bool array) {
    if (!init || TREE_CODE(init) != CONSTRUCTOR)
        return NULL_TREE;
    unsigned i;
    tree index, element;
    FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(init), i, index, element) {
        if ((!array && index == key) ||
            (array && index && TREE_CODE(index) == INTEGER_CST && tree_to_uhwi(index) == number))
            return element;
        if (array && index && TREE_CODE(index) == RANGE_EXPR) {
            tree lo = TREE_OPERAND(index, 0), hi = TREE_OPERAND(index, 1);
            if (tree_fits_uhwi_p(lo) && tree_fits_uhwi_p(hi) && number >= tree_to_uhwi(lo) &&
                number <= tree_to_uhwi(hi))
                return element;
        }
    }
    return NULL_TREE;
}

void product_leaves(
    tree original, tree type, tree init, const std::string &path, tree *list, tree *vars) {
    if (++expanded_product_leaves > 4096) {
        error_at(at(original), "|c>: fixed quantum product expansion exceeds 4096 nodes");
        return;
    }
    if (TREE_CODE(type) == RECORD_TYPE && init && TREE_CODE(init) != CONSTRUCTOR) {
        tree call = strip(init);
        if (call && TREE_CODE(call) == CALL_EXPR &&
            standard_integer_division(get_callee_fndecl(call)) && has_quantum(call))
            init = expand_quantum_program(call);
    }
    if (TREE_CODE(type) == RECORD_TYPE && init && TREE_CODE(init) != CONSTRUCTOR) {
        if (has_quantum(init)) {
            error_at(at(original),
                     "|c>: quantum product preparation cannot implicitly copy quantum storage");
            return;
        }
        tree temporary = build_decl(at(original),
                                    VAR_DECL,
                                    get_identifier("__cket_product_input"),
                                    TYPE_MAIN_VARIANT(type));
        DECL_CONTEXT(temporary) = active_function;
        DECL_INITIAL(temporary) = init;
        DECL_CHAIN(temporary) = *vars;
        *vars = temporary;
        append(list, build1(DECL_EXPR, void_type_node, temporary));
        unsigned nodes = 0;
        init = classical_product_constructor(type, temporary, nodes);
        if (!init)
            return;
    }
    if (TREE_CODE(type) == ARRAY_TYPE) {
        uint64_t count = tree_to_uhwi(TYPE_MAX_VALUE(TYPE_DOMAIN(type))) + 1;
        for (uint64_t i = 0; i < count; ++i)
            product_leaves(original,
                           TREE_TYPE(type),
                           constructor_element(init, NULL_TREE, i, true),
                           path + "[" + std::to_string(i) + "]",
                           list,
                           vars);
    } else if (TREE_CODE(type) == RECORD_TYPE) {
        for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f))
            if (TREE_CODE(f) == FIELD_DECL) {
                if (!DECL_NAME(f)) {
                    error_at(at(original),
                             "|c>: anonymous quantum fields need explicit access paths");
                    continue;
                }
                product_leaves(original,
                               TREE_TYPE(f),
                               constructor_element(init, f, 0, false),
                               path + "." + name_of(f),
                               list,
                               vars);
            }
    } else {
        tree decl = build_decl(at(original),
                               VAR_DECL,
                               get_identifier((name_of(original) + path).c_str()),
                               build_qualified_type(type, TYPE_QUAL_VOLATILE));
        DECL_CONTEXT(decl) = active_function;
        DECL_ATTRIBUTES(decl) = tree_cons(get_identifier("cket_quant"), NULL_TREE, NULL_TREE);
        TREE_THIS_VOLATILE(decl) = TREE_SIDE_EFFECTS(decl) = 1;
        DECL_INITIAL(decl) = init ? init : build_zero_cst(TREE_TYPE(decl));
        DECL_CHAIN(decl) = *vars;
        *vars = decl;
        quantum_products[original][path] = decl;
        append(list, build1(DECL_EXPR, void_type_node, decl));
    }
}

bool product_path(tree node, tree &root, std::string &path) {
    if (!node)
        return false;
    if (TREE_CODE(node) == VAR_DECL) {
        root = node;
        return quantum_products.count(root);
    }
    if (TREE_CODE(node) == COMPONENT_REF) {
        if (!product_path(TREE_OPERAND(node, 0), root, path))
            return false;
        path += "." + name_of(TREE_OPERAND(node, 1));
        return true;
    }
    if (TREE_CODE(node) == ARRAY_REF) {
        if (!product_path(TREE_OPERAND(node, 0), root, path))
            return false;
        tree index = strip(fold(TREE_OPERAND(node, 1)));
        if (TREE_CODE(index) != INTEGER_CST || !tree_fits_uhwi_p(index)) {
            error_at(
                at(node),
                "|c>: quantum array access currently requires a constant index or a statically bounded for loop");
            return false;
        }
        path += "[" + std::to_string(tree_to_uhwi(index)) + "]";
        return true;
    }
    return false;
}

bool resolve_quantum_reference(tree value, QuantumReference &reference) {
    value = strip(value);
    if (!value)
        return false;
    auto existing = quantum_references.find(value);
    if (existing != quantum_references.end()) {
        reference = existing->second;
        return true;
    }
    if (TREE_CODE(value) == POINTER_PLUS_EXPR) {
        if (!resolve_quantum_reference(TREE_OPERAND(value, 0), reference))
            return false;
        tree bytes = strip(fold(TREE_OPERAND(value, 1))),
             size = TYPE_SIZE_UNIT(reference.element_type);
        if (!size || !tree_fits_uhwi_p(size) || !tree_to_uhwi(size) ||
            TREE_CODE(bytes) != INTEGER_CST)
            return false;
        int64_t delta = static_cast<int64_t>(TREE_INT_CST_LOW(bytes));
        uint64_t stride = tree_to_uhwi(size);
        if (delta % (int64_t)stride)
            return false;
        int64_t steps = delta / (int64_t)stride;
        if (steps > 4096 || steps < -4096)
            return false;
        reference.offset += steps;
        return reference.offset >= 0 && reference.offset <= (int64_t)reference.elements.size();
    }
    if (TREE_CODE(value) != ADDR_EXPR)
        return false;
    tree storage = strip(TREE_OPERAND(value, 0));
    if (TREE_CODE(storage) == VAR_DECL && quantum_products.count(storage) &&
        TREE_CODE(TREE_TYPE(storage)) == ARRAY_TYPE &&
        INTEGRAL_TYPE_P(TREE_TYPE(TREE_TYPE(storage)))) {
        reference.element_type = TREE_TYPE(TREE_TYPE(storage));
        reference.offset = 0;
        uint64_t size = tree_to_uhwi(TYPE_MAX_VALUE(TYPE_DOMAIN(TREE_TYPE(storage)))) + 1;
        for (uint64_t i = 0; i < size; ++i) {
            auto leaf = quantum_products[storage].find("[" + std::to_string(i) + "]");
            if (leaf == quantum_products[storage].end())
                return false;
            reference.elements.push_back(leaf->second);
        }
        return true;
    }
    if (TREE_CODE(storage) == ARRAY_REF) {
        tree array = strip(TREE_OPERAND(storage, 0)), index = strip(fold(TREE_OPERAND(storage, 1)));
        if (TREE_CODE(array) == VAR_DECL && quantum_products.count(array) &&
            TREE_CODE(TREE_TYPE(array)) == ARRAY_TYPE &&
            INTEGRAL_TYPE_P(TREE_TYPE(TREE_TYPE(array))) && TREE_CODE(index) == INTEGER_CST &&
            tree_fits_uhwi_p(index)) {
            uint64_t size = tree_to_uhwi(TYPE_MAX_VALUE(TYPE_DOMAIN(TREE_TYPE(array)))) + 1;
            uint64_t offset = tree_to_uhwi(index);
            if (offset > size)
                return false;
            reference.element_type = TREE_TYPE(TREE_TYPE(array));
            reference.offset = (int64_t)offset;
            for (uint64_t i = 0; i < size; ++i) {
                auto leaf = quantum_products[array].find("[" + std::to_string(i) + "]");
                if (leaf == quantum_products[array].end())
                    return false;
                reference.elements.push_back(leaf->second);
            }
            return true;
        }
    }
    if (TREE_CODE(storage) == INDIRECT_REF)
        return resolve_quantum_reference(TREE_OPERAND(storage, 0), reference);
    storage = expand_quantum_program(storage);
    if (!quantum(storage))
        return false;
    reference.elements = {storage};
    reference.offset = 0;
    reference.element_type = TREE_TYPE(storage);
    return true;
}

tree reference_element(tree value) {
    if (TREE_CODE(value) != INDIRECT_REF)
        return NULL_TREE;
    QuantumReference reference;
    if (!resolve_quantum_reference(TREE_OPERAND(value, 0), reference))
        return NULL_TREE;
    if (TYPE_MAIN_VARIANT(TREE_TYPE(value)) != TYPE_MAIN_VARIANT(reference.element_type)) {
        error_at(
            at(value),
            "|c>: reinterpreting quantum storage through a different pointer element type needs a byte-access contract");
        return build_zero_cst(TREE_TYPE(value));
    }
    if (reference.offset >= (int64_t)reference.elements.size()) {
        error_at(at(value), "|c>: quantum pointer dereference is outside borrowed storage");
        return build_zero_cst(TREE_TYPE(value));
    }
    tree element = reference.elements[(size_t)reference.offset];
    if (TYPE_READONLY(TREE_TYPE(value)))
        return fold_convert(TREE_TYPE(value), element);
    return element;
}

// C library calls are recognized by declaration identity, rather than spelling
// alone. A user definition with the same name retains its ordinary C behavior.
bool standard_memory_copy(tree fn, bool &move) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return false;
    bool copy = fndecl_built_in_p(fn, BUILT_IN_MEMCPY);
    move = fndecl_built_in_p(fn, BUILT_IN_MEMMOVE);
    if (!copy && !move && DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) &&
        in_system_header_at(DECL_SOURCE_LOCATION(fn))) {
        copy = name_of(fn) == "memcpy";
        move = name_of(fn) == "memmove";
    }
    return copy || move;
}

bool standard_integer_division(tree fn) {
    if (!fn || !DECL_EXTERNAL(fn) || !TREE_PUBLIC(fn) ||
        !in_system_header_at(DECL_SOURCE_LOCATION(fn)))
        return false;
    std::string name = name_of(fn);
    return name == "div" || name == "ldiv" || name == "lldiv" || name == "imaxdiv";
}

tree snapshot_classical_leaves(tree value, std::map<tree, tree> &captures) {
    if (!value || CONSTANT_CLASS_P(value) || TYPE_P(value))
        return value;
    auto code = TREE_CODE(value);
    if (!has_quantum(value) && INTEGRAL_TYPE_P(TREE_TYPE(value)) &&
        (code == VAR_DECL || code == ARRAY_REF || code == COMPONENT_REF || code == INDIRECT_REF)) {
        auto found = captures.find(value);
        if (found == captures.end()) {
            tree saved = save_expr(value);
            cpu_library_saves.insert(saved);
            found = captures.emplace(value, saved).first;
        }
        return found->second;
    }
    if (DECL_P(value) || !EXPR_P(value))
        return value;
    tree result = copy_node(value);
    for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
        TREE_OPERAND(result, i) = snapshot_classical_leaves(TREE_OPERAND(value, i), captures);
    return result;
}

tree expand_integer_division(tree call) {
    if (call_expr_nargs(call) != 2 ||
        (!has_quantum(CALL_EXPR_ARG(call, 0)) && !has_quantum(CALL_EXPR_ARG(call, 1))))
        return NULL_TREE;
    std::map<tree, tree> captures;
    tree arguments[2];
    for (unsigned i = 0; i < 2; ++i) {
        tree argument = CALL_EXPR_ARG(call, i);
        if (has_quantum(argument) && !quantum_pure_expression(argument)) {
            error_at(
                at(call),
                "|c>: quantum integer-division arguments currently require pure quantum expressions; effectful quantum arguments need sequenced capture");
            return call;
        }
        if (has_quantum(argument))
            arguments[i] = snapshot_classical_leaves(argument, captures);
        else {
            arguments[i] = save_expr(argument);
            cpu_library_saves.insert(arguments[i]);
        }
    }
    tree type = TREE_TYPE(call);
    if (TREE_CODE(type) != RECORD_TYPE) {
        error_at(at(call), "|c>: integer-division library result requires a checked product type");
        return call;
    }
    vec<constructor_elt, va_gc> *elements = NULL;
    unsigned fields = 0;
    for (tree field = TYPE_FIELDS(type); field; field = DECL_CHAIN(field)) {
        if (TREE_CODE(field) != FIELD_DECL)
            continue;
        std::string name = name_of(field);
        tree field_type = TREE_TYPE(field);
        if ((name != "quot" && name != "rem") || !INTEGRAL_TYPE_P(field_type) ||
            TYPE_PRECISION(field_type) > 64 || TYPE_UNSIGNED(field_type) || DECL_BIT_FIELD(field)) {
            error_at(at(call), "|c>: integer-division library product layout is unsupported");
            return call;
        }
        tree numerator = fold_convert(field_type, arguments[0]),
             denominator = fold_convert(field_type, arguments[1]);
        // Do not fold remainder by minus one to zero: the complete C call is
        // undefined when its quotient is not representable, even if unused.
        tree value = build2_loc(at(call),
                                name == "quot" ? TRUNC_DIV_EXPR : TRUNC_MOD_EXPR,
                                field_type,
                                numerator,
                                denominator);
        CONSTRUCTOR_APPEND_ELT(elements, field, value);
        ++fields;
    }
    if (fields != 2) {
        error_at(at(call), "|c>: integer-division library product must contain quot and rem");
        return call;
    }
    tree result = build_constructor(type, elements);
    quantum_division_products.insert(result);
    return result;
}

tree expand_memory_copy(tree call, bool move) {
    if (call_expr_nargs(call) != 3)
        return NULL_TREE;
    QuantumReference destination, source;
    bool quantum_destination = resolve_quantum_reference(CALL_EXPR_ARG(call, 0), destination);
    bool quantum_source = resolve_quantum_reference(CALL_EXPR_ARG(call, 1), source);
    if (!quantum_destination && !quantum_source)
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!quantum_destination || !quantum_source)
        return reject(
            "|c>: mixed CPU/quantum memory copy needs an implemented preparation contract or explicit measurement; byte copying cannot cross that boundary implicitly");
    tree bytes = strip(fold(CALL_EXPR_ARG(call, 2)));
    if (!bytes || TREE_CODE(bytes) != INTEGER_CST || !tree_fits_uhwi_p(bytes))
        return reject(
            "|c>: quantum memory copy currently needs a constant byte count; dynamic extents need bounded memory synthesis");
    tree target_size = TYPE_SIZE_UNIT(destination.element_type),
         source_size = TYPE_SIZE_UNIT(source.element_type);
    if (!target_size || !source_size || !tree_fits_uhwi_p(target_size) ||
        !tree_fits_uhwi_p(source_size) || !tree_to_uhwi(target_size) ||
        tree_to_uhwi(target_size) != tree_to_uhwi(source_size) ||
        !INTEGRAL_TYPE_P(destination.element_type) || !INTEGRAL_TYPE_P(source.element_type) ||
        TYPE_PRECISION(destination.element_type) != tree_to_uhwi(target_size) * BITS_PER_UNIT ||
        TYPE_PRECISION(source.element_type) != tree_to_uhwi(source_size) * BITS_PER_UNIT)
        return reject(
            "|c>: quantum byte copy currently requires equal-size integer elements without padding; byte views, bool padding and product layouts need further implementation");
    uint64_t extent = tree_to_uhwi(bytes), stride = tree_to_uhwi(target_size);
    if (extent % stride)
        return reject(
            "|c>: partial-element quantum byte copy needs an implemented byte-access contract");
    uint64_t count = extent / stride;
    if (destination.offset < 0 || source.offset < 0 ||
        (uint64_t)destination.offset > destination.elements.size() ||
        (uint64_t)source.offset > source.elements.size() ||
        count > destination.elements.size() - (uint64_t)destination.offset ||
        count > source.elements.size() - (uint64_t)source.offset)
        return reject("|c>: quantum memory copy exceeds its checked storage bounds");
    if (count > 4096)
        return reject("|c>: quantum memory copy exceeds the 4096-element synthesis bound");
    if (TYPE_READONLY(destination.element_type))
        return reject("|c>: quantum memory copy requires mutable destination storage");
    std::vector<std::pair<tree, tree>> pairs;
    std::map<tree, unsigned> source_positions;
    for (unsigned i = 0; i < count; ++i) {
        tree target = destination.elements[(size_t)destination.offset + i],
             input = source.elements[(size_t)source.offset + i];
        if (readonly_quantum_borrows.count(target))
            return reject("|c>: quantum memory copy cannot write through a read-only borrow");
        pairs.emplace_back(target, input);
        source_positions.emplace(input, i);
    }
    // If a write aliases another source, read that source first. For contiguous
    // ranges this reproduces memmove's forward/backward ordering without a
    // classical temporary or an ancilla holding an uncleared copy.
    std::vector<unsigned> indegree(count, 0), next(count, (unsigned)count), ready;
    for (unsigned i = 0; i < count; ++i) {
        auto overlap = source_positions.find(pairs[i].first);
        if (overlap == source_positions.end())
            continue;
        if (!move)
            return reject("|c>: overlapping quantum memcpy has undefined C behavior; use memmove");
        unsigned reader = overlap->second;
        if (reader != i) {
            next[reader] = i;
            ++indegree[i];
        }
    }
    for (unsigned i = 0; i < count; ++i)
        if (!indegree[i])
            ready.push_back(i);
    QuantumMemoryCopy operation;
    for (size_t i = 0; i < ready.size(); ++i) {
        unsigned index = ready[i];
        operation.elements.push_back(pairs[index]);
        if (next[index] != count && !--indegree[next[index]])
            ready.push_back(next[index]);
    }
    if (operation.elements.size() != count)
        return reject("|c>: cyclic quantum memory aliases need retained snapshot storage");
    tree marker =
        build_fn_decl("__cket_memory_copy", build_function_type(TREE_TYPE(call), NULL_TREE));
    std::vector<tree> arguments;
    for (const auto &pair : operation.elements) {
        arguments.push_back(pair.first);
        arguments.push_back(pair.second);
    }
    tree result =
        build_call_expr_loc_array(at(call), marker, (int)arguments.size(), arguments.data());
    quantum_memory_copies.emplace(result, std::move(operation));
    return result;
}

tree indexed_access(tree original, const std::vector<tree> &elements, tree index, bool readonly) {
    index = expand_quantum_program(index);
    if (has_quantum(index)) {
        error_at(
            at(original),
            "|c>: a quantum index needs a coherent memory-access transformation; measure explicitly for CPU selection");
        return build_zero_cst(TREE_TYPE(original));
    }
    tree marker = build_fn_decl(
        "__cket_indexed_storage",
        build_function_type_list(TREE_TYPE(original), long_long_unsigned_type_node, NULL_TREE));
    tree saved = save_expr(fold_convert(long_long_unsigned_type_node, index));
    tree result = build_call_expr_loc(at(original), marker, 1, saved);
    quantum_indexed_accesses.emplace(result, QuantumIndexedAccess{elements, saved, readonly});
    return result;
}

bool readonly_quantum_access(tree value) {
    if (!value)
        return false;
    tree storage = strip(value);
    if (readonly_quantum_borrows.count(storage))
        return true;
    if (quantum_indexed_accesses.count(storage))
        return quantum_indexed_accesses.at(storage).readonly;
    if (TREE_CODE(storage) == INDIRECT_REF || TREE_CODE(storage) == ARRAY_REF) {
        QuantumReference reference;
        tree pointer = TREE_OPERAND(storage, 0);
        if (resolve_quantum_reference(pointer, reference))
            return TYPE_READONLY(reference.element_type);
    }
    return quantum(storage) && TYPE_READONLY(TREE_TYPE(value));
}

tree dynamic_product_access(tree value) {
    auto existing = indexed_expansions.find(value);
    if (existing != indexed_expansions.end())
        return existing->second;
    if (TREE_CODE(value) == ARRAY_REF) {
        tree array = strip(TREE_OPERAND(value, 0)), type = TREE_TYPE(array);
        tree index = strip(fold(TREE_OPERAND(value, 1)));
        if (TREE_CODE(array) != VAR_DECL || !quantum_products.count(array) ||
            TREE_CODE(type) != ARRAY_TYPE || !INTEGRAL_TYPE_P(TREE_TYPE(type)) ||
            TREE_CODE(index) == INTEGER_CST)
            return NULL_TREE;
        std::vector<tree> elements;
        uint64_t count = tree_to_uhwi(TYPE_MAX_VALUE(TYPE_DOMAIN(type))) + 1;
        for (uint64_t i = 0; i < count; ++i)
            elements.push_back(quantum_products.at(array).at("[" + std::to_string(i) + "]"));
        tree selected = indexed_access(value, elements, TREE_OPERAND(value, 1), false);
        indexed_expansions[value] = selected;
        return selected;
    }
    if (TREE_CODE(value) == INDIRECT_REF) {
        tree pointer = strip(TREE_OPERAND(value, 0));
        if (TREE_CODE(pointer) != POINTER_PLUS_EXPR)
            return NULL_TREE;
        QuantumReference reference;
        if (!resolve_quantum_reference(TREE_OPERAND(pointer, 0), reference))
            return NULL_TREE;
        if (TYPE_MAIN_VARIANT(TREE_TYPE(value)) != TYPE_MAIN_VARIANT(reference.element_type)) {
            error_at(
                at(value),
                "|c>: indexed quantum pointer access must preserve the borrowed C element type");
            return build_zero_cst(TREE_TYPE(value));
        }
        tree bytes = TREE_OPERAND(pointer, 1), size = TYPE_SIZE_UNIT(reference.element_type);
        if (TREE_CODE(strip(fold(bytes))) == INTEGER_CST || !size || !tree_fits_uhwi_p(size))
            return NULL_TREE;
        tree index = fold_build2(TRUNC_DIV_EXPR,
                                 long_long_integer_type_node,
                                 fold_convert(long_long_integer_type_node, bytes),
                                 build_int_cst(long_long_integer_type_node, tree_to_uhwi(size)));
        index = fold_build2(PLUS_EXPR,
                            long_long_integer_type_node,
                            index,
                            build_int_cst(long_long_integer_type_node, reference.offset));
        tree selected =
            indexed_access(value, reference.elements, index, TYPE_READONLY(reference.element_type));
        indexed_expansions[value] = selected;
        return selected;
    }
    return NULL_TREE;
}

tree expand_static_for_block(tree block) {
    tree counter = BIND_EXPR_VARS(block), body = BIND_EXPR_BODY(block);
    if (!counter || DECL_CHAIN(counter) || TREE_CODE(counter) != VAR_DECL || quantum(counter) ||
        !DECL_INITIAL(counter) || TREE_CODE(strip(DECL_INITIAL(counter))) != INTEGER_CST ||
        TREE_CODE(body) != STATEMENT_LIST)
        return NULL_TREE;
    std::vector<tree> stmts;
    for (auto it = tsi_start(body); !tsi_end_p(it); tsi_next(&it))
        stmts.push_back(tsi_stmt(it));
    if (stmts.size() != 2 || TREE_CODE(stmts[0]) != DECL_EXPR ||
        DECL_EXPR_DECL(stmts[0]) != counter || TREE_CODE(stmts[1]) != FOR_STMT)
        return NULL_TREE;
    tree loop = stmts[1], cond = FOR_COND(loop), step = FOR_EXPR(loop);
    if (FOR_INIT_STMT(loop) || !cond || !step || !COMPARISON_CLASS_P(cond) ||
        strip(TREE_OPERAND(cond, 0)) != counter ||
        TREE_CODE(strip(TREE_OPERAND(cond, 1))) != INTEGER_CST)
        return NULL_TREE;
    auto operation = TREE_CODE(step);
    int increment = operation == PREINCREMENT_EXPR || operation == POSTINCREMENT_EXPR   ? 1
                    : operation == PREDECREMENT_EXPR || operation == POSTDECREMENT_EXPR ? -1
                                                                                        : 0;
    if (!increment || strip(TREE_OPERAND(step, 0)) != counter)
        return NULL_TREE;
    // Unroll only when it enables currently unsupported product/control lives.
    bool useful = false, safe = true;
    tree content = FOR_BODY(loop);

    struct Scan {
        tree counter;
        bool *useful, *safe;
    } scan{counter, &useful, &safe};

    auto inspect = [](tree *node, int *walk, void *data) -> tree {
        auto &s = *static_cast<Scan *>(data);
        tree value = *node;
        auto code = TREE_CODE(value);
        if (code == ARRAY_REF || code == INDIRECT_REF ||
            (code == DECL_EXPR &&
             lookup_attribute("cket_quant", DECL_ATTRIBUTES(DECL_EXPR_DECL(value)))) ||
            (code == COND_EXPR && has_quantum(TREE_OPERAND(value, 0))))
            *s.useful = true;
        if (code == CALL_EXPR) {
            bool move = false;
            tree fn = get_callee_fndecl(value);
            if (standard_memory_copy(fn, move) || quantum_function(fn)) {
                auto quantum_storage = [](tree *node, int *walk, void *) -> tree {
                    if (quantum(*node) || quantum_products.count(*node) ||
                        quantum_references.count(*node))
                        return *node;
                    if (DECL_P(*node) || TYPE_P(*node))
                        *walk = 0;
                    return NULL_TREE;
                };
                tree call = value;
                if (walk_tree(&call, quantum_storage, nullptr, nullptr))
                    *s.useful = true;
            }
        }
        if (code == BREAK_STMT || code == CONTINUE_STMT || code == GOTO_EXPR || code == RETURN_EXPR)
            *s.safe = false;
        if ((code == MODIFY_EXPR || code == INIT_EXPR || code == PREINCREMENT_EXPR ||
             code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR ||
             code == POSTDECREMENT_EXPR || code == ADDR_EXPR) &&
            strip(TREE_OPERAND(value, 0)) == s.counter)
            *s.safe = false;
        if (DECL_P(value) || TYPE_P(value))
            *walk = 0;
        return NULL_TREE;
    };
    walk_tree(&content, inspect, &scan, nullptr);
    if (!useful || !safe || !tree_fits_shwi_p(strip(DECL_INITIAL(counter))) ||
        !tree_fits_shwi_p(strip(TREE_OPERAND(cond, 1))))
        return NULL_TREE;
    tree counter_type = TREE_TYPE(counter);
    if (!INTEGRAL_TYPE_P(counter_type) || TYPE_PRECISION(counter_type) > 64)
        return NULL_TREE;
    unsigned precision = TYPE_PRECISION(counter_type);
    int64_t minimum = TYPE_UNSIGNED(counter_type) ? 0
                      : precision == 64           ? INT64_MIN
                                                  : -(int64_t{1} << (precision - 1));
    int64_t maximum = precision == 64 || (TYPE_UNSIGNED(counter_type) && precision == 63)
                          ? INT64_MAX
                          : (int64_t{1} << (precision - (TYPE_UNSIGNED(counter_type) ? 0 : 1))) - 1;
    int64_t start = tree_to_shwi(strip(DECL_INITIAL(counter))),
            stop = tree_to_shwi(strip(TREE_OPERAND(cond, 1)));
    auto test = [cond, stop](int64_t i) {
        switch (TREE_CODE(cond)) {
        case LT_EXPR:
            return i < stop;
        case LE_EXPR:
            return i <= stop;
        case GT_EXPR:
            return i > stop;
        case GE_EXPR:
            return i >= stop;
        default:
            return false;
        }
    };
    if (TREE_CODE(cond) != LT_EXPR && TREE_CODE(cond) != LE_EXPR && TREE_CODE(cond) != GT_EXPR &&
        TREE_CODE(cond) != GE_EXPR)
        return NULL_TREE;
    std::vector<int64_t> indices;
    for (int64_t i = start; test(i); i += increment) {
        if (indices.size() == 4096 || i < minimum || i > maximum ||
            (increment > 0 ? i == maximum : i == minimum)) {
            error_at(at(loop),
                     "|c>: static quantum loop expansion exceeds its proven finite bound");
            return loop;
        }
        indices.push_back(i);
    }
    tree result = alloc_stmt_list();
    for (int64_t index : indices) {
        std::map<tree, tree> bindings;
        bindings[counter] = build_int_cst(TREE_TYPE(counter), index);
        append(&result, expand_quantum_program(clone_quantum_tree(FOR_BODY(loop), bindings)));
    }
    return result;
}

tree expand_quantum_program(tree value) {
    if (!value)
        return value;
    auto code = TREE_CODE(value);
    if (code == BIND_EXPR) {
        tree unrolled = expand_static_for_block(value);
        if (unrolled)
            return unrolled;
        BIND_EXPR_BODY(value) = expand_quantum_program(BIND_EXPR_BODY(value));
        return value;
    }
    if (code == STATEMENT_LIST) {
        tree result = alloc_stmt_list();
        for (auto it = tsi_start(value); !tsi_end_p(it); tsi_next(&it))
            append(&result, expand_quantum_program(tsi_stmt(it)));
        return result;
    }
    if (code == DECL_EXPR) {
        tree decl = DECL_EXPR_DECL(value), type = TREE_TYPE(decl);
        tree init = strip(DECL_INITIAL(decl));
        if (quantum(decl) && init && TREE_CODE(init) == CALL_EXPR) {
            tree fn = get_callee_fndecl(init);
            auto found = fn ? quantum_functions.find(name_of(fn)) : quantum_functions.end();
            if (found != quantum_functions.end() && found->second.owned_result)
                return instantiate_quantum_call(init, decl);
        }
        if (lookup_attribute("cket_quant", DECL_ATTRIBUTES(decl)) &&
            TREE_CODE(type) == POINTER_TYPE) {
            QuantumReference reference;
            if (!DECL_INITIAL(decl) || !resolve_quantum_reference(DECL_INITIAL(decl), reference) ||
                TYPE_MAIN_VARIANT(TREE_TYPE(type)) != TYPE_MAIN_VARIANT(reference.element_type) ||
                (TYPE_READONLY(reference.element_type) && !TYPE_READONLY(TREE_TYPE(type)))) {
                error_at(
                    at(decl),
                    "|c>: quantum pointer needs an initialized checked borrow of compatible quantum storage");
                return value;
            }
            reference.element_type = TREE_TYPE(type);
            quantum_references[decl] = reference;
            return build_empty_stmt(at(decl));
        }
        if (lookup_attribute("cket_quant", DECL_ATTRIBUTES(decl)) &&
            (TREE_CODE(type) == ARRAY_TYPE || TREE_CODE(type) == RECORD_TYPE)) {
            tree list = alloc_stmt_list(), vars = NULL_TREE;
            tree initial = DECL_INITIAL(decl);
            tree call = strip(initial);
            if (call && TREE_CODE(call) == CALL_EXPR &&
                standard_integer_division(get_callee_fndecl(call)))
                initial = expand_quantum_program(call);
            if (initial && TREE_CODE(initial) == STRING_CST && TREE_CODE(type) == ARRAY_TYPE &&
                INTEGRAL_TYPE_P(TREE_TYPE(type)) && TYPE_PRECISION(TREE_TYPE(type)) == 8) {
                vec<constructor_elt, va_gc> *elts = NULL;
                unsigned count = (unsigned)tree_to_uhwi(TYPE_MAX_VALUE(TYPE_DOMAIN(type))) + 1;
                for (unsigned i = 0; i < count; ++i)
                    CONSTRUCTOR_APPEND_ELT(
                        elts,
                        size_int(i),
                        build_int_cstu(TREE_TYPE(type),
                                       i < (unsigned)TREE_STRING_LENGTH(initial)
                                           ? (unsigned char)TREE_STRING_POINTER(initial)[i]
                                           : 0));
                initial = build_constructor(type, elts);
            }
            if (initial && TREE_CODE(initial) != CONSTRUCTOR &&
                (TREE_CODE(type) != RECORD_TYPE || has_quantum(initial))) {
                error_at(
                    at(decl),
                    "|c>: whole quantum product assignment cannot implicitly copy storage; initialize or move individual elements");
                return value;
            }
            product_leaves(decl, type, initial, "", &list, &vars);
            DECL_INITIAL(decl) = NULL_TREE;
            return expand_quantum_program(build3(BIND_EXPR, void_type_node, vars, list, NULL_TREE));
        }
        if (DECL_INITIAL(decl))
            DECL_INITIAL(decl) = expand_quantum_program(DECL_INITIAL(decl));
        return value;
    }
    if (code == CALL_EXPR) {
        tree fn = get_callee_fndecl(value);
        bool move = false;
        if (standard_memory_copy(fn, move)) {
            tree expanded = expand_memory_copy(value, move);
            if (expanded)
                return expanded;
        }
        std::string effect = intrinsic(value);
        bool destructive = effect == "measure" || effect == "reset" || effect == "fourier" ||
                           effect == "inverse_fourier" || effect == "find" ||
                           effect == "find_known" || effect == "search";
        bool module = effect == "circuit" || effect == "inverse_circuit";
        for (int i = module ? 1 : 0; (destructive || module) && i < call_expr_nargs(value); ++i) {
            if (!module && i)
                break;
            if (readonly_quantum_access(CALL_EXPR_ARG(value, i))) {
                error_at(
                    at(value),
                    "|c>: this operation requires a mutable quantum borrow; const borrows cannot measure, reset or transform storage");
                return value;
            }
        }
        // Resolve aggregate arguments before borrowing scalar storage.
        tree param = fn ? DECL_ARGUMENTS(fn) : NULL_TREE;
        for (int i = 0; i < call_expr_nargs(value); ++i) {
            bool pointer_borrow = param && lookup_attribute("cket_quant", DECL_ATTRIBUTES(param)) &&
                                  TREE_CODE(TREE_TYPE(param)) == POINTER_TYPE;
            if (!pointer_borrow)
                CALL_EXPR_ARG(value, i) = expand_quantum_program(CALL_EXPR_ARG(value, i));
            if (param)
                param = DECL_CHAIN(param);
        }
        if (quantum_function(fn))
            return instantiate_quantum_call(value);
        if (standard_integer_division(fn)) {
            tree expanded = expand_integer_division(value);
            if (expanded)
                return expanded;
        }
        // These standard integer interfaces are pure basis-value functions.
        // Recognize the actual GCC builtin or an external system declaration;
        // never rewrite a user function merely because its name matches.
        bool standard_abs =
            fn && !DECL_SAVED_TREE(fn) &&
            (fndecl_built_in_p(fn, BUILT_IN_ABS) || fndecl_built_in_p(fn, BUILT_IN_LABS) ||
             fndecl_built_in_p(fn, BUILT_IN_LLABS) || fndecl_built_in_p(fn, BUILT_IN_IMAXABS));
        if (fn && !standard_abs && DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) &&
            in_system_header_at(DECL_SOURCE_LOCATION(fn))) {
            std::string name = name_of(fn);
            standard_abs = name == "abs" || name == "labs" || name == "llabs" || name == "imaxabs";
        }
        if (standard_abs && call_expr_nargs(value) == 1 && has_quantum(CALL_EXPR_ARG(value, 0)))
            return build1_loc(at(value), ABS_EXPR, TREE_TYPE(value), CALL_EXPR_ARG(value, 0));
        return value;
    }
    if (code == INDIRECT_REF) {
        tree selected = dynamic_product_access(value);
        if (selected)
            return selected;
        tree element = reference_element(value);
        if (element)
            return element;
    }
    if (code == ARRAY_REF || code == COMPONENT_REF) {
        if (code == COMPONENT_REF) {
            tree container = strip(TREE_OPERAND(value, 0));
            while (container && TREE_CODE(container) == SAVE_EXPR)
                container = strip(TREE_OPERAND(container, 0));
            if (container && TREE_CODE(container) == CALL_EXPR &&
                standard_integer_division(get_callee_fndecl(container))) {
                tree product = expand_quantum_program(container);
                if (quantum_division_products.count(product)) {
                    tree member = constructor_element(product, TREE_OPERAND(value, 1), 0, false);
                    if (!member) {
                        error_at(at(value), "|c>: integer-division library projection is invalid");
                        return build_zero_cst(TREE_TYPE(value));
                    }
                    return member;
                }
            }
        }
        tree selected = dynamic_product_access(value);
        if (selected)
            return selected;
        tree root = NULL_TREE;
        std::string path;
        if (product_path(value, root, path)) {
            auto found = quantum_products[root].find(path);
            if (found == quantum_products[root].end()) {
                error_at(at(value),
                         "|c>: quantum product index or field is outside its declared storage");
                return build_zero_cst(TREE_TYPE(value));
            }
            return found->second;
        }
    }
    if (code == ADDR_EXPR && quantum_function(TREE_OPERAND(value, 0))) {
        error_at(at(value),
                 "|c>: quantum function pointers need a checked quantum calling convention");
        return value;
    }
    if (code == VAR_DECL && quantum_products.count(value))
        error_at(
            at(value),
            "|c>: quantum products cannot decay to CPU pointers or be read as a whole; access their elements");
    if (code == VAR_DECL && quantum_references.count(value))
        error_at(
            at(value),
            "|c>: quantum reference cannot escape to CPU memory or be rebound without a checked borrow");
    if (DECL_P(value) || TYPE_P(value))
        return value;
    if (code == CONSTRUCTOR) {
        unsigned i;
        tree index, element;
        FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(value), i, index, element)
        CONSTRUCTOR_ELT(value, i)->value = expand_quantum_program(element);
        (void)index;
        return value;
    }
    if (code == SAVE_EXPR) {
        TREE_OPERAND(value, 0) = expand_quantum_program(TREE_OPERAND(value, 0));
        if (CONSTANT_CLASS_P(TREE_OPERAND(value, 0)))
            return TREE_OPERAND(value, 0);
        if (has_quantum(TREE_OPERAND(value, 0)) && quantum_pure_expression(TREE_OPERAND(value, 0)))
            return TREE_OPERAND(value, 0);
        return value;
    }
    if (code == COMPOUND_EXPR) {
        TREE_OPERAND(value, 1) = expand_quantum_program(TREE_OPERAND(value, 1));
        // GCC's C frontend inserts scalar UBSan checks before PRE_GENERICIZE.
        // They cannot read a superposed register on the CPU. Keep the typed
        // operation: the backend checks its defined quantum input domain.
        if (gcc_quantum_sanitizer_guard(TREE_OPERAND(value, 0)) &&
            has_quantum(TREE_OPERAND(value, 1)) && quantum_pure_expression(TREE_OPERAND(value, 1)))
            return TREE_OPERAND(value, 1);
        TREE_OPERAND(value, 0) = expand_quantum_program(TREE_OPERAND(value, 0));
        if (TREE_CODE(TREE_OPERAND(value, 0)) == VOID_CST ||
            CONSTANT_CLASS_P(TREE_OPERAND(value, 0)))
            return TREE_OPERAND(value, 1);
        if (has_quantum(TREE_OPERAND(value, 0)) &&
            quantum_pure_expression(TREE_OPERAND(value, 0)) &&
            TREE_CODE(strip(TREE_OPERAND(value, 1))) == MODIFY_EXPR)
            return TREE_OPERAND(value, 1);
        return value;
    }
    if (code == COND_EXPR) {
        TREE_OPERAND(value, 0) = expand_quantum_program(TREE_OPERAND(value, 0));
        tree condition = strip(TREE_OPERAND(value, 0));
        if (TREE_CODE(condition) == INTEGER_CST)
            return expand_quantum_program(TREE_OPERAND(value, integer_zerop(condition) ? 2 : 1));
        TREE_OPERAND(value, 1) = expand_quantum_program(TREE_OPERAND(value, 1));
        TREE_OPERAND(value, 2) = expand_quantum_program(TREE_OPERAND(value, 2));
        return value;
    }
    if (EXPR_P(value))
        for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
            TREE_OPERAND(value, i) = expand_quantum_program(TREE_OPERAND(value, i));
    if (EXPR_P(value) && TREE_TYPE(value) &&
        (INTEGRAL_TYPE_P(TREE_TYPE(value)) || SCALAR_FLOAT_TYPE_P(TREE_TYPE(value))) &&
        !has_quantum(value) && quantum_pure_expression(value))
        return fold(value);
    return value;
}
