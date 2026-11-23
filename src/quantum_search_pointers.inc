// Local search pointers retain a basis-value selector and object provenance.
// They never contain a host address for superposed storage. The checked
// candidate count is the missing-match sentinel, after every valid position.
bool expand_search_pointer_borrow(tree decl, tree &prefix) {
    tree initial_raw = DECL_INITIAL(decl), initial = strip(initial_raw), type = TREE_TYPE(decl);
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
    tree alias_key = search_pointer_call(initial_raw);
    auto alias_it =
        alias_key ? quantum_search_pointers.find(alias_key) : quantum_search_pointers.end();
    if (alias_it != quantum_search_pointers.end()) {
        // These local borrows preserve read-only qualification from any source
        // pointer/borrow and share the selector lifetime cleanly.
        if (TREE_CODE(alias_key) == VAR_DECL && TREE_TYPE(TREE_TYPE(alias_key)) &&
            TYPE_READONLY(TREE_TYPE(TREE_TYPE(alias_key))))
            search.source.element_type =
                build_qualified_type(search.source.element_type, TYPE_QUAL_CONST);
        quantum_search_pointers[decl] = search;
        DECL_INITIAL(decl) = NULL_TREE;
        prefix = build_empty_stmt(at(decl));
        return true;
    }
    unsigned pos_bits = 8;
    while (pos_bits < 32 && (uint64_t{1} << pos_bits) <= search.positions)
        ++pos_bits;
    tree pos_type =
        pos_bits <= 8 ? unsigned_char_type_node : build_nonstandard_integer_type(pos_bits, true);
    tree encoded = fold_build3_loc(at(decl),
                                   COND_EXPR,
                                   pos_type,
                                   search.found,
                                   fold_convert(pos_type, search.position),
                                   build_int_cstu(pos_type, search.positions));
    encoded = string_search_domain(search, encoded, at(decl));
    tree handle = build_decl(at(decl),
                             VAR_DECL,
                             get_identifier((name_of(decl) + ".__search_position").c_str()),
                             build_qualified_type(pos_type, TYPE_QUAL_VOLATILE));
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

static bool search_read_pointer_full(tree value,
                                     QuantumStringSearch &search,
                                     int64_t &delta,
                                     tree &dynamic_delta,
                                     tree &dynamic_defined,
                                     bool &had_error) {
    value = strip(value);
    while (value && TREE_CODE(value) == SAVE_EXPR)
        value = strip(TREE_OPERAND(value, 0));
    if (!value)
        return false;
    if (TREE_CODE(value) == POINTER_PLUS_EXPR) {
        if (!search_read_pointer_full(
                TREE_OPERAND(value, 0), search, delta, dynamic_delta, dynamic_defined, had_error))
            return false;
        if (had_error)
            return true;
        return search_append_offset_step(at(value),
                                         TREE_OPERAND(value, 1),
                                         true,
                                         search,
                                         delta,
                                         dynamic_delta,
                                         dynamic_defined,
                                         had_error,
                                         "byte offset");
    }
    return expand_string_search(value, search, true);
}

[[maybe_unused]] bool
search_read_pointer(tree value, QuantumStringSearch &search, int64_t &delta, tree &dynamic_delta) {
    tree dynamic_defined = boolean_true_node;
    bool had_error = false;
    bool ok =
        search_read_pointer_full(value, search, delta, dynamic_delta, dynamic_defined, had_error);
    if (ok && dynamic_defined != boolean_true_node)
        search.defined = fold_build2_loc(
            at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.defined, dynamic_defined);
    return ok;
}

static bool search_read_reference_full(tree value,
                                       QuantumStringSearch &search,
                                       int64_t &delta,
                                       tree &dynamic_delta,
                                       tree &dynamic_defined,
                                       bool &had_error) {
    if (TREE_CODE(value) == INDIRECT_REF)
        return search_read_pointer_full(
            TREE_OPERAND(value, 0), search, delta, dynamic_delta, dynamic_defined, had_error);
    if (TREE_CODE(value) != ARRAY_REF ||
        TREE_CODE(TREE_TYPE(TREE_OPERAND(value, 0))) != POINTER_TYPE)
        return false;
    if (!search_read_pointer_full(
            TREE_OPERAND(value, 0), search, delta, dynamic_delta, dynamic_defined, had_error))
        return false;
    if (had_error)
        return true;
    return search_append_offset_step(at(value),
                                     TREE_OPERAND(value, 1),
                                     false,
                                     search,
                                     delta,
                                     dynamic_delta,
                                     dynamic_defined,
                                     had_error,
                                     "character index");
}

bool search_read_reference(tree value,
                           QuantumStringSearch &search,
                           int64_t &delta,
                           tree &dynamic_delta) {
    tree dynamic_defined = boolean_true_node;
    bool had_error = false;
    bool ok =
        search_read_reference_full(value, search, delta, dynamic_delta, dynamic_defined, had_error);
    if (ok && dynamic_defined != boolean_true_node)
        search.defined = fold_build2_loc(
            at(value), TRUTH_ANDIF_EXPR, boolean_type_node, search.defined, dynamic_defined);
    return ok;
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
    if (dynamic_delta || (search.defined != boolean_true_node && dynamic_delta)) {
        tree i64_type = long_long_integer_type_node;
        tree in_bounds = boolean_false_node;
        tree effective_pos = search_signed_position(
            at(value), search, delta, dynamic_delta, boolean_true_node, 0, in_bounds);
        QuantumByteRange safe_src = search.source;
        safe_src.cpu_bytes.clear();
        current.cpu_guard = NULL_TREE;
        for (uint64_t address = current.size(); address; --address) {
            uint64_t addr = address - 1;
            tree selected = build2_loc(at(value),
                                       EQ_EXPR,
                                       boolean_type_node,
                                       effective_pos,
                                       build_int_cst(i64_type, static_cast<int64_t>(addr)));
            tree byte_val;
            if (current.cpu_address && search.source.stop_at_zero && addr >= origin) {
                byte_val = object_byte(safe_src, addr - origin, at(value));
            } else if (current.cpu_address && !search.source.stop_at_zero &&
                       addr >= origin + search.positions) {
                continue;
            } else {
                byte_val = object_byte(current, addr, at(value));
            }
            tree byte = fold_convert(type, byte_val);
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
