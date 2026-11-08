// Local search pointers retain a basis-value selector and object provenance.
// They never contain a host address for superposed storage. The checked
// candidate count is the missing-match sentinel, after every valid position.
bool expand_search_pointer_borrow(tree decl, tree &prefix) {
    tree initial = strip(DECL_INITIAL(decl)), type = TREE_TYPE(decl);
    if (!initial || TREE_CODE(type) != POINTER_TYPE || !search_pointer_call(initial))
        return false;
    QuantumStringSearch search;
    if (!expand_string_search(initial, search, true))
        return false;
    tree pointed = TREE_TYPE(type);
    bool character = TYPE_MAIN_VARIANT(pointed) == char_type_node ||
                     TYPE_MAIN_VARIANT(pointed) == signed_char_type_node ||
                     TYPE_MAIN_VARIANT(pointed) == unsigned_char_type_node;
    if ((TREE_CODE(pointed) != VOID_TYPE && !character) || TYPE_VOLATILE(pointed) ||
        TYPE_ATOMIC(pointed) || TYPE_VOLATILE(type) || TYPE_ATOMIC(type) || TREE_STATIC(decl) ||
        DECL_EXTERNAL(decl)) {
        error_at(
            at(decl),
            "|c>: a saved quantum search pointer requires a local nonvolatile character/void pointer");
        prefix = build_empty_stmt(at(decl));
        return true;
    }
    if (quantum_search_pointers.count(strip(initial))) {
        // These local borrows are immutable. Aliasing an existing selector
        // shares its lifetime rather than copying an unknown quantum state.
        quantum_search_pointers[decl] = search;
        DECL_INITIAL(decl) = NULL_TREE;
        prefix = build_empty_stmt(at(decl));
        return true;
    }
    tree encoded = fold_build3_loc(at(decl),
                                   COND_EXPR,
                                   unsigned_char_type_node,
                                   search.found,
                                   fold_convert(unsigned_char_type_node, search.position),
                                   build_int_cstu(unsigned_char_type_node, search.positions));
    encoded = string_search_domain(search, encoded, at(decl));
    tree handle = build_decl(at(decl),
                             VAR_DECL,
                             get_identifier((name_of(decl) + ".__search_position").c_str()),
                             build_qualified_type(unsigned_char_type_node, TYPE_QUAL_VOLATILE));
    DECL_CONTEXT(handle) = active_function;
    DECL_ATTRIBUTES(handle) = tree_cons(get_identifier("cket_quant"), NULL_TREE, NULL_TREE);
    TREE_THIS_VOLATILE(handle) = TREE_SIDE_EFFECTS(handle) = 1;
    DECL_INITIAL(handle) = encoded;
    search.position = fold_convert(size_type_node, handle);
    search.found = fold_build2_loc(at(decl),
                                   LT_EXPR,
                                   boolean_type_node,
                                   handle,
                                   build_int_cstu(TREE_TYPE(handle), search.positions));
    search.defined = boolean_true_node; // Checked once by the saved initializer.
    // Captured CPU bytes belong only to that initialization. A later load
    // through the saved address must observe the object's current contents.
    quantum_search_pointers[decl] = search;
    DECL_INITIAL(decl) = NULL_TREE;
    prefix = build3(
        BIND_EXPR, void_type_node, handle, build1(DECL_EXPR, void_type_node, handle), NULL_TREE);
    return true;
}

bool search_read_pointer(tree value,
                         QuantumStringSearch &search,
                         int64_t &delta,
                         tree &dynamic_delta) {
    value = strip(value);
    if (!value)
        return false;
    if (TREE_CODE(value) == POINTER_PLUS_EXPR) {
        if (!search_read_pointer(TREE_OPERAND(value, 0), search, delta, dynamic_delta))
            return false;
        tree raw_offset = expand_quantum_program(TREE_OPERAND(value, 1));
        tree offset = strip(fold(raw_offset));
        if (offset && TREE_CODE(offset) == INTEGER_CST && TYPE_PRECISION(TREE_TYPE(offset)) <= 64) {
            int64_t step = static_cast<int64_t>(TREE_INT_CST_LOW(offset));
            if ((step > 0 && delta > INT64_MAX - step) || (step < 0 && delta < INT64_MIN - step)) {
                error_at(at(value),
                         "|c>: search-pointer byte offset overflows its checked representation");
                return true;
            }
            delta += step;
            return true;
        }
        if (!raw_offset || !INTEGRAL_TYPE_P(TREE_TYPE(raw_offset)) ||
            TYPE_PRECISION(TREE_TYPE(raw_offset)) > 64 || !quantum_pure_expression(raw_offset)) {
            error_at(at(value),
                     "|c>: search-pointer byte offset requires a pure integer expression");
            return true;
        }
        tree step_expr = fold_convert(size_type_node, raw_offset);
        if (!has_quantum(step_expr) && TREE_CODE(strip(step_expr)) != INTEGER_CST) {
            step_expr = save_expr(step_expr);
            cpu_library_saves.insert(step_expr);
        }
        dynamic_delta =
            dynamic_delta
                ? fold_build2_loc(at(value), PLUS_EXPR, size_type_node, dynamic_delta, step_expr)
                : step_expr;
        return true;
    }
    return expand_string_search(value, search, true);
}

bool search_read_reference(tree value,
                           QuantumStringSearch &search,
                           int64_t &delta,
                           tree &dynamic_delta) {
    if (TREE_CODE(value) == INDIRECT_REF)
        return search_read_pointer(TREE_OPERAND(value, 0), search, delta, dynamic_delta);
    if (TREE_CODE(value) != ARRAY_REF ||
        TREE_CODE(TREE_TYPE(TREE_OPERAND(value, 0))) != POINTER_TYPE)
        return false;
    if (!search_read_pointer(TREE_OPERAND(value, 0), search, delta, dynamic_delta))
        return false;
    tree raw_index = expand_quantum_program(TREE_OPERAND(value, 1));
    tree index = strip(fold(raw_index));
    if (index && TREE_CODE(index) == INTEGER_CST && tree_fits_shwi_p(index)) {
        int64_t step = tree_to_shwi(index);
        if ((step > 0 && delta > INT64_MAX - step) || (step < 0 && delta < INT64_MIN - step)) {
            error_at(at(value),
                     "|c>: search-pointer character index overflows its checked representation");
            return true;
        }
        delta += step;
        return true;
    }
    if (!raw_index || !INTEGRAL_TYPE_P(TREE_TYPE(raw_index)) ||
        TYPE_PRECISION(TREE_TYPE(raw_index)) > 64 || !quantum_pure_expression(raw_index)) {
        error_at(at(value),
                 "|c>: search-pointer character index requires a pure integer expression");
        return true;
    }
    tree step_expr = fold_convert(size_type_node, raw_index);
    if (!has_quantum(step_expr) && TREE_CODE(strip(step_expr)) != INTEGER_CST) {
        step_expr = save_expr(step_expr);
        cpu_library_saves.insert(step_expr);
    }
    dynamic_delta =
        dynamic_delta
            ? fold_build2_loc(at(value), PLUS_EXPR, size_type_node, dynamic_delta, step_expr)
            : step_expr;
    return true;
}

tree expand_search_pointer_read_resolved(tree value,
                                         QuantumStringSearch search,
                                         int64_t delta,
                                         tree dynamic_delta) {
    tree type = TREE_TYPE(value);
    if (!INTEGRAL_TYPE_P(type) || TYPE_PRECISION(type) != 8 || TYPE_VOLATILE(type) ||
        TYPE_ATOMIC(type)) {
        error_at(
            at(value),
            "|c>: search-pointer dereference currently reads nonvolatile character bytes only");
        return build_zero_cst(type);
    }
    QuantumByteRange current = search.source;
    uint64_t count = search.positions;
    uint64_t origin = current.offset;
    current.offset = 0;
    current.stop_at_zero = false;
    current.cpu_bytes.clear();
    tree result = fold_convert(type, missing_string_terminator(integer_type_node, at(value)));
    if (dynamic_delta) {
        tree base_pos =
            fold_build2_loc(at(value),
                            PLUS_EXPR,
                            size_type_node,
                            search.position,
                            build_int_cstu(size_type_node, origin + static_cast<uint64_t>(delta)));
        tree effective_pos =
            build2_loc(at(value), PLUS_EXPR, size_type_node, base_pos, dynamic_delta);
        current.cpu_guard = NULL_TREE;
        for (uint64_t address = current.size(); address; --address) {
            uint64_t addr = address - 1;
            tree selected = build2_loc(at(value),
                                       EQ_EXPR,
                                       boolean_type_node,
                                       effective_pos,
                                       build_int_cstu(size_type_node, addr));
            tree byte = fold_convert(type, object_byte(current, addr, at(value)));
            result = build3_loc(at(value), COND_EXPR, type, selected, byte, result);
        }
        return string_search_domain(search, result, at(value), true);
    }
    for (uint64_t i = count; i; --i) {
        uint64_t relative = i - 1, address = origin + relative;
        if ((delta < 0 && (uint64_t)(-(delta + 1)) + 1 > address) ||
            (delta >= 0 && (uint64_t)delta >= current.size() - address))
            continue;
        address = delta < 0 ? address - ((uint64_t)(-(delta + 1)) + 1) : address + (uint64_t)delta;
        tree selected = fold_build2_loc(at(value),
                                        EQ_EXPR,
                                        boolean_type_node,
                                        search.position,
                                        build_int_cstu(size_type_node, relative));
        if (current.cpu_address && search.source.stop_at_zero && relative) {
            tree previous = object_byte(search.source, relative - 1, at(value));
            current.cpu_guard = fold_build2_loc(
                at(value), NE_EXPR, boolean_type_node, previous, build_zero_cst(integer_type_node));
        } else
            current.cpu_guard = NULL_TREE;
        tree byte = fold_convert(type, object_byte(current, address, at(value)));
        result = fold_build3_loc(at(value), COND_EXPR, type, selected, byte, result);
    }
    return string_search_domain(search, result, at(value), true);
}

tree expand_search_pointer_read(tree value) {
    QuantumStringSearch search;
    int64_t delta = 0;
    tree dynamic_delta = NULL_TREE;
    if (!search_read_reference(value, search, delta, dynamic_delta))
        return NULL_TREE;
    return expand_search_pointer_read_resolved(value, search, delta, dynamic_delta);
}

tree expand_search_byte_write(
    tree value, tree target, QuantumStringSearch search, int64_t delta, tree dynamic_delta);

tree expand_search_pointer_write(tree value) {
    enum tree_code code = TREE_CODE(value);
    if (code != MODIFY_EXPR && code != INIT_EXPR && code != PREINCREMENT_EXPR &&
        code != POSTINCREMENT_EXPR && code != PREDECREMENT_EXPR && code != POSTDECREMENT_EXPR)
        return NULL_TREE;
    tree target = strip(TREE_OPERAND(value, 0));
    QuantumStringSearch search;
    int64_t delta = 0;
    tree dynamic_delta = NULL_TREE;
    bool rebind = quantum_search_pointers.count(target);
    if (!rebind && !search_read_reference(target, search, delta, dynamic_delta))
        return NULL_TREE;
    if (!rebind)
        return expand_search_byte_write(value, target, search, delta, dynamic_delta);
    error_at(at(value),
             "|c>: saved quantum search pointer rebinding requires retained pointer history");
    return TREE_TYPE(value) == void_type_node ? build_empty_stmt(at(value))
                                              : build_zero_cst(TREE_TYPE(value));
}
