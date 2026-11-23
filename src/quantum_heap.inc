// Classical pointers name runtime quantum heaps. No host load reads their payload.
tree unscaled_quantum_pointer_index(tree bytes, uint64_t stride);
tree expand_heap_transfer(tree call, bool move);

void defer_heap_decisions() {
    // A native heap write can preserve a decision about its previous payload
    // as history. Runtime disposal uses the actual saved inverse or root.
    for (const auto &control : controls)
        for (auto &entry : variables)
            if (entry.second.id == control.first && entry.second.derived)
                entry.second.optional_definition = entry.second.runtime_history = true;
}

std::string heap_allocator(tree fn) {
    if (!fn || DECL_SAVED_TREE(fn))
        return "";
    const char *names[] = {
        "malloc", "calloc", "realloc", "free", "aligned_alloc", "__cket_aligned_alloc"};

    enum built_in_function codes[] = {BUILT_IN_MALLOC,
                                      BUILT_IN_CALLOC,
                                      BUILT_IN_REALLOC,
                                      BUILT_IN_FREE,
                                      BUILT_IN_NONE,
                                      BUILT_IN_NONE};

    for (unsigned i = 0; i < 6; ++i)
        if ((codes[i] != BUILT_IN_NONE && fndecl_built_in_p(fn, codes[i])) ||
            (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) &&
             in_system_header_at(DECL_SOURCE_LOCATION(fn)) && name_of(fn) == names[i]))
            return i >= 4 ? "aligned_alloc" : names[i];
    return "";
}

bool heap_pointer(tree value, tree &element, bool &readonly) {
    if (!value || !TREE_TYPE(value) || TREE_CODE(TREE_TYPE(value)) != POINTER_TYPE)
        return false;
    readonly |= TYPE_READONLY(TREE_TYPE(TREE_TYPE(value)));
    auto found = quantum_heap_pointers.find(value);
    if (found != quantum_heap_pointers.end()) {
        element = found->second;
        readonly |= TYPE_READONLY(element);
        return true;
    }
    if (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR ||
        TREE_CODE(value) == SAVE_EXPR)
        return heap_pointer(TREE_OPERAND(value, 0), element, readonly);
    if (TREE_CODE(value) == POINTER_PLUS_EXPR && !has_quantum(TREE_OPERAND(value, 1)))
        return heap_pointer(TREE_OPERAND(value, 0), element, readonly);
    if (TREE_CODE(value) == COMPOUND_EXPR)
        return heap_pointer(TREE_OPERAND(value, 1), element, readonly);
    if (quantum_native_memory.count(value))
        return heap_pointer(quantum_native_memory.at(value).destination, element, readonly);
    bool move = false;
    if (TREE_CODE(value) == CALL_EXPR && call_expr_nargs(value) == 3 &&
        (standard_memory_set(get_callee_fndecl(value)) ||
         standard_memory_copy(get_callee_fndecl(value), move)))
        return heap_pointer(CALL_EXPR_ARG(value, 0), element, readonly);
    return false;
}

bool heap_pointer(tree value) {
    tree element = NULL_TREE;
    bool readonly = false;
    return heap_pointer(value, element, readonly);
}

bool heap_byte_layout(tree element) {
    return element && INTEGRAL_TYPE_P(element) && CHAR_BIT == 8 && TYPE_PRECISION(element) &&
           TYPE_PRECISION(element) % CHAR_BIT == 0;
}

bool heap_borrowed_pointer(tree value) {
    if (!value)
        return false;
    if (quantum_heap_borrows.count(value))
        return true;
    if (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR ||
        TREE_CODE(value) == SAVE_EXPR || TREE_CODE(value) == POINTER_PLUS_EXPR)
        return heap_borrowed_pointer(TREE_OPERAND(value, 0));
    if (TREE_CODE(value) == COMPOUND_EXPR)
        return heap_borrowed_pointer(TREE_OPERAND(value, 1));
    if (quantum_native_memory.count(value))
        return heap_borrowed_pointer(quantum_native_memory.at(value).destination);
    bool move = false;
    if (TREE_CODE(value) == CALL_EXPR && call_expr_nargs(value) == 3 &&
        (standard_memory_set(get_callee_fndecl(value)) ||
         standard_memory_copy(get_callee_fndecl(value), move)))
        return heap_borrowed_pointer(CALL_EXPR_ARG(value, 0));
    return false;
}

tree heap_runtime_call(const char *name,
                       tree result,
                       const std::vector<tree> &arguments,
                       const std::vector<tree> &types,
                       location_t loc) {
    tree signature = tree_cons(NULL_TREE, void_type_node, NULL_TREE);
    for (auto type = types.rbegin(); type != types.rend(); ++type)
        signature = tree_cons(NULL_TREE, *type, signature);
    tree fn = build_fn_decl(name, build_function_type(result, signature));
    TREE_PUBLIC(fn) = DECL_EXTERNAL(fn) = DECL_ARTIFICIAL(fn) = 1;
    if (std::string(name) == "__cket_heap_identity" || std::string(name) == "__cket_heap_offset" ||
        std::string(name) == "__cket_heap_borrow_check" ||
        std::string(name) == "__cket_heap_byte_identity" ||
        std::string(name) == "__cket_heap_byte_offset")
        quantum_heap_queries.insert(fn);
    std::vector<tree> values;
    for (size_t i = 0; i < arguments.size(); ++i)
        values.push_back(fold_convert(types[i], arguments[i]));
    return build_call_expr_loc_array(loc, fn, values.size(), values.data());
}

bool heap_struct_layout(tree element,
                        tree &field_type,
                        uint64_t &field_count,
                        uint64_t &field_stride) {
    if (!element || TREE_CODE(element) != RECORD_TYPE || !TYPE_SIZE_UNIT(element) ||
        !tree_fits_uhwi_p(TYPE_SIZE_UNIT(element)))
        return false;
    field_type = NULL_TREE;
    field_count = 0;
    for (tree f = TYPE_FIELDS(element); f; f = DECL_CHAIN(f)) {
        if (TREE_CODE(f) != FIELD_DECL)
            continue;
        if (DECL_BIT_FIELD(f) || !INTEGRAL_TYPE_P(TREE_TYPE(f)) || !TYPE_PRECISION(TREE_TYPE(f)) ||
            TYPE_PRECISION(TREE_TYPE(f)) > 64 || !TYPE_SIZE_UNIT(TREE_TYPE(f)) ||
            !tree_fits_uhwi_p(TYPE_SIZE_UNIT(TREE_TYPE(f))))
            return false;
        if (!field_type)
            field_type = TREE_TYPE(f);
        else if (TYPE_MAIN_VARIANT(TREE_TYPE(f)) != TYPE_MAIN_VARIANT(field_type) ||
                 TYPE_PRECISION(TREE_TYPE(f)) != TYPE_PRECISION(field_type) ||
                 TYPE_UNSIGNED(TREE_TYPE(f)) != TYPE_UNSIGNED(field_type))
            return false;
        ++field_count;
    }
    if (!field_type || !field_count)
        return false;
    field_stride = tree_to_uhwi(TYPE_SIZE_UNIT(field_type));
    return field_stride == static_cast<uint64_t>((TYPE_PRECISION(field_type) + 7) / 8) &&
           tree_to_uhwi(TYPE_SIZE_UNIT(element)) == field_count * field_stride;
}

tree heap_allocate_call(tree call, tree pointer_type) {
    tree fn = get_callee_fndecl(call), element = TREE_TYPE(pointer_type);
    std::string operation = heap_allocator(fn);
    unsigned arity = operation == "malloc" ? 1 : 2;
    if ((operation != "malloc" && operation != "calloc" && operation != "realloc" &&
         operation != "aligned_alloc") ||
        (unsigned)call_expr_nargs(call) != arity)
        return NULL_TREE;
    tree scalar = element;
    uint64_t field_count = 1, stride = 0;
    if (TREE_CODE(element) == RECORD_TYPE) {
        if (!heap_struct_layout(element, scalar, field_count, stride))
            return NULL_TREE;
    } else {
        if (!INTEGRAL_TYPE_P(element) || !TYPE_PRECISION(element) || TYPE_PRECISION(element) > 64 ||
            !TYPE_SIZE_UNIT(element) || !tree_fits_uhwi_p(TYPE_SIZE_UNIT(element)))
            return NULL_TREE;
        stride = tree_to_uhwi(TYPE_SIZE_UNIT(element));
        if (stride != static_cast<uint64_t>((TYPE_PRECISION(element) + 7) / 8)) {
            error_at(at(call), "|c>: quantum heap requires padding-free scalar elements");
            return build_zero_cst(pointer_type);
        }
    }
    if (operation == "aligned_alloc") {
        tree align = strip(fold(CALL_EXPR_ARG(call, 0)));
        if (!align || TREE_CODE(align) != INTEGER_CST || !tree_fits_uhwi_p(align) ||
            tree_to_uhwi(align) == 0 || (tree_to_uhwi(align) & (tree_to_uhwi(align) - 1)) != 0) {
            error_at(at(call),
                     "|c>: quantum aligned_alloc requires a constant power-of-two alignment");
            return build_zero_cst(pointer_type);
        }
    }
    tree ull = long_long_unsigned_type_node;
    std::vector<tree> args, types;
    if (operation == "realloc") {
        if (heap_borrowed_pointer(CALL_EXPR_ARG(call, 0))) {
            error_at(at(call), "|c>: a borrowed quantum heap cannot be reallocated by its callee");
            return build_zero_cst(pointer_type);
        }
        if (!heap_pointer(CALL_EXPR_ARG(call, 0)) &&
            !integer_zerop(strip(CALL_EXPR_ARG(call, 0)))) {
            error_at(at(call), "|c>: quantum realloc requires a quantum heap or null pointer");
            return build_zero_cst(pointer_type);
        }
        args.push_back(CALL_EXPR_ARG(call, 0));
        types.push_back(ptr_type_node);
    }
    for (unsigned i = (operation == "realloc" || operation == "aligned_alloc") ? 1 : 0; i < arity;
         ++i) {
        if (has_quantum(CALL_EXPR_ARG(call, i))) {
            error_at(at(call), "|c>: quantum heap capacity must be a classical integer");
            return build_zero_cst(pointer_type);
        }
        args.push_back(expand_quantum_program(CALL_EXPR_ARG(call, i)));
        types.push_back(ull);
    }
    args.push_back(build_int_cstu(ull, stride));
    args.push_back(build_int_cstu(ull, TYPE_PRECISION(scalar)));
    args.push_back(build_int_cstu(ull, !TYPE_UNSIGNED(scalar)));
    types.insert(types.end(), 3, ull);
    const char *name = operation == "realloc"  ? "__cket_heap_realloc"
                       : operation == "calloc" ? "__cket_heap_calloc"
                                               : "__cket_heap_malloc";
    instructions.push_back("{\"op\":\"cheap\"}");
    return fold_convert(pointer_type,
                        heap_runtime_call(name, ptr_type_node, args, types, at(call)));
}

bool expand_heap_declaration(tree decl) {
    tree type = TREE_TYPE(decl), initial = DECL_INITIAL(decl), init = strip(initial);
    if (TREE_CODE(type) != POINTER_TYPE)
        return false;
    bool annotated = lookup_attribute("cket_quant", DECL_ATTRIBUTES(decl));
    bool borrowed = heap_pointer(initial);
    bool reallocation = init && TREE_CODE(init) == CALL_EXPR &&
                        heap_allocator(get_callee_fndecl(init)) == "realloc" &&
                        heap_pointer(CALL_EXPR_ARG(init, 0));
    if (!annotated && !borrowed && !reallocation)
        return false;
    tree element = TREE_TYPE(type), scalar = NULL_TREE;
    uint64_t field_count = 1, stride = 0;
    if (init && TREE_CODE(init) == CALL_EXPR) {
        tree allocated = heap_allocate_call(init, type);
        if (allocated) {
            quantum_heap_pointers[decl] = element;
            DECL_INITIAL(decl) = allocated;
            return true;
        }
    }
    if (init && integer_zerop(init) && annotated &&
        (INTEGRAL_TYPE_P(element) || heap_struct_layout(element, scalar, field_count, stride))) {
        quantum_heap_pointers[decl] = element;
        instructions.push_back("{\"op\":\"cheap\"}");
        return true;
    }
    tree source_type = NULL_TREE;
    bool readonly = false;
    if (!borrowed || !heap_pointer(initial, source_type, readonly))
        return false;
    bool byte_view =
        VOID_TYPE_P(element) || (INTEGRAL_TYPE_P(element) && TYPE_PRECISION(element) == CHAR_BIT);
    if ((!byte_view && TYPE_MAIN_VARIANT(source_type) != TYPE_MAIN_VARIANT(element)) ||
        (readonly && !TYPE_READONLY(element))) {
        error_at(at(decl), "|c>: quantum heap alias must preserve its C type and read-only borrow");
        return true;
    }
    quantum_heap_pointers[decl] =
        readonly ? build_qualified_type(source_type, TYPE_QUAL_CONST) : source_type;
    if (heap_borrowed_pointer(initial))
        quantum_heap_borrows.insert(decl);
    // A returned memory pointer carries an effect. Expand it exactly once;
    // ordinary address aliases deliberately bypass scalar escape expansion.
    if (init && (TREE_CODE(init) == CALL_EXPR || TREE_CODE(init) == COMPOUND_EXPR))
        DECL_INITIAL(decl) = expand_quantum_program(initial);
    return true;
}

tree expand_heap_pointer_update(tree value) {
    if (TREE_CODE(value) != MODIFY_EXPR)
        return NULL_TREE;
    tree target = strip(TREE_OPERAND(value, 0));
    if (!quantum_heap_pointers.count(target))
        return NULL_TREE;
    tree rhs = strip(TREE_OPERAND(value, 1)), type = TREE_TYPE(target);
    if (rhs && TREE_CODE(rhs) == CALL_EXPR) {
        tree allocation = heap_allocate_call(rhs, type);
        if (allocation) {
            TREE_OPERAND(value, 1) = allocation;
            return value;
        }
    }
    tree element = NULL_TREE;
    bool readonly = false;
    if (!integer_zerop(rhs) &&
        (!heap_pointer(rhs, element, readonly) ||
         TYPE_MAIN_VARIANT(element) != TYPE_MAIN_VARIANT(quantum_heap_pointers.at(target)) ||
         (readonly && !TYPE_READONLY(TREE_TYPE(type))))) {
        error_at(at(value),
                 "|c>: quantum heap pointer assignment requires a compatible heap or null");
        return value;
    }
    if (heap_borrowed_pointer(rhs))
        quantum_heap_borrows.insert(target);
    if (rhs && (TREE_CODE(rhs) == CALL_EXPR || TREE_CODE(rhs) == COMPOUND_EXPR))
        TREE_OPERAND(value, 1) = expand_quantum_program(TREE_OPERAND(value, 1));
    return value;
}

tree expand_heap_pointer_relation(tree value) {
    if (TREE_OPERAND_LENGTH(value) != 2 ||
        (!heap_pointer(TREE_OPERAND(value, 0)) && !heap_pointer(TREE_OPERAND(value, 1))))
        return NULL_TREE;
    if (TREE_CODE(value) == EQ_EXPR || TREE_CODE(value) == NE_EXPR ||
        TREE_CODE(value) == POINTER_DIFF_EXPR) {
        // Calls returning an address still perform their memory effect once.
        for (int i = 0; i < 2; ++i) {
            tree operand = TREE_OPERAND(value, i), call = strip(operand);
            if (call && TREE_CODE(call) == CALL_EXPR && heap_pointer(call))
                TREE_OPERAND(value, i) =
                    fold_convert(TREE_TYPE(operand), expand_quantum_program(call));
        }
        return value; // These classical addresses expose no quantum data.
    }
    error_at(at(value),
             "|c>: ordered quantum heap pointer relations need checked common-object provenance");
    return build_zero_cst(TREE_TYPE(value));
}

bool heap_access(tree value, QuantumHeapAccess &access) {
    auto known = quantum_heap_accesses.find(strip(value));
    if (known != quantum_heap_accesses.end()) {
        access = known->second;
        return true;
    }
    tree pointer = NULL_TREE, index = NULL_TREE;
    if (TREE_CODE(value) == COMPONENT_REF) {
        tree container = strip(TREE_OPERAND(value, 0)), field = TREE_OPERAND(value, 1);
        tree struct_type = container ? TREE_TYPE(container) : NULL_TREE;
        tree field_type = NULL_TREE;
        uint64_t field_count = 0, field_stride = 0;
        if (!struct_type || !heap_struct_layout(struct_type, field_type, field_count, field_stride))
            return false;
        tree base_ptr = NULL_TREE, elem_idx = NULL_TREE;
        if (TREE_CODE(container) == ARRAY_REF) {
            base_ptr = TREE_OPERAND(container, 0);
            elem_idx = TREE_OPERAND(container, 1);
        } else if (TREE_CODE(container) == INDIRECT_REF) {
            tree p = TREE_OPERAND(container, 0);
            if (TREE_CODE(strip(p)) == POINTER_PLUS_EXPR) {
                tree sum = strip(p);
                base_ptr = TREE_OPERAND(sum, 0);
                tree bytes = TREE_OPERAND(sum, 1);
                uint64_t s = tree_to_uhwi(TYPE_SIZE_UNIT(struct_type));
                if (TREE_CODE(strip(bytes)) == INTEGER_CST && tree_fits_uhwi_p(strip(bytes)) &&
                    tree_to_uhwi(strip(bytes)) % s == 0)
                    elem_idx = build_int_cstu(long_long_unsigned_type_node,
                                              tree_to_uhwi(strip(bytes)) / s);
                else
                    elem_idx = unscaled_quantum_pointer_index(bytes, s);
                if (!elem_idx)
                    return false;
            } else {
                base_ptr = p;
                elem_idx = build_zero_cst(long_long_unsigned_type_node);
            }
        } else {
            return false;
        }
        tree element = NULL_TREE;
        bool readonly = false;
        if (!heap_pointer(base_ptr, element, readonly) ||
            TYPE_MAIN_VARIANT(element) != TYPE_MAIN_VARIANT(struct_type))
            return false;
        uint64_t ord = 0;
        bool found_field = false;
        for (tree f = TYPE_FIELDS(struct_type); f; f = DECL_CHAIN(f))
            if (TREE_CODE(f) == FIELD_DECL) {
                if (f == field) {
                    found_field = true;
                    break;
                }
                ++ord;
            }
        if (!found_field || TYPE_MAIN_VARIANT(TREE_TYPE(value)) != TYPE_MAIN_VARIANT(field_type))
            return false;
        tree expanded_elem = expand_quantum_program(elem_idx);
        tree ull = long_long_unsigned_type_node;
        tree flat = fold_build2_loc(at(value),
                                    PLUS_EXPR,
                                    ull,
                                    fold_build2_loc(at(value),
                                                    MULT_EXPR,
                                                    ull,
                                                    fold_convert(ull, expanded_elem),
                                                    build_int_cstu(ull, field_count)),
                                    build_int_cstu(ull, ord));
        access = {base_ptr, save_expr(flat), TREE_TYPE(value), readonly};
        return true;
    }
    if (TREE_CODE(value) == INDIRECT_REF) {
        pointer = TREE_OPERAND(value, 0);
        if (TREE_CODE(strip(pointer)) == POINTER_PLUS_EXPR) {
            tree sum = strip(pointer);
            tree element = NULL_TREE;
            bool readonly = false;
            if (!heap_pointer(TREE_OPERAND(sum, 0), element, readonly))
                return false;
            pointer = TREE_OPERAND(sum, 0);
            tree bytes = TREE_OPERAND(sum, 1);
            uint64_t stride = tree_to_uhwi(TYPE_SIZE_UNIT(element));
            if (TREE_CODE(strip(bytes)) == INTEGER_CST && tree_fits_uhwi_p(strip(bytes)) &&
                tree_to_uhwi(strip(bytes)) % stride == 0)
                index = build_int_cstu(long_long_unsigned_type_node,
                                       tree_to_uhwi(strip(bytes)) / stride);
            else
                index = unscaled_quantum_pointer_index(bytes, stride);
            if (!index) {
                error_at(at(value),
                         "|c>: heap access requires the element index before byte scaling");
                return false;
            }
        } else
            index = build_zero_cst(long_long_unsigned_type_node);
    } else if (TREE_CODE(value) == ARRAY_REF) {
        pointer = TREE_OPERAND(value, 0);
        index = TREE_OPERAND(value, 1);
    } else
        return false;
    tree element = NULL_TREE;
    bool readonly = false;
    if (!heap_pointer(pointer, element, readonly))
        return false;
    if (TYPE_MAIN_VARIANT(element) != TYPE_MAIN_VARIANT(TREE_TYPE(value))) {
        error_at(at(value), "|c>: quantum heap access must preserve the allocated C element type");
        return false;
    }
    access = {pointer, save_expr(expand_quantum_program(index)), TREE_TYPE(value), readonly};
    return true;
}

tree expand_heap_read(tree value) {
    QuantumHeapAccess access;
    if (!heap_access(value, access))
        return NULL_TREE;
    tree marker =
        build_fn_decl("__cket_heap_read",
                      build_function_type_list(
                          TREE_TYPE(value), ptr_type_node, TREE_TYPE(access.index), NULL_TREE));
    tree result = build_call_expr_loc(
        at(value), marker, 2, fold_convert(ptr_type_node, access.pointer), access.index);
    quantum_heap_accesses.emplace(result, access);
    return result;
}

tree expand_heap_write(tree value) {
    auto code = TREE_CODE(value);
    bool increment = code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR ||
                     code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR;
    if (code != MODIFY_EXPR && !increment)
        return NULL_TREE;
    QuantumHeapAccess access;
    if (!heap_access(TREE_OPERAND(value, 0), access))
        return NULL_TREE;
    if (access.readonly) {
        error_at(at(value), "|c>: quantum heap store requires a mutable borrow");
        return build_zero_cst(TREE_TYPE(value));
    }
    if (!quantum_pure_expression(access.index)) {
        error_at(at(value), "|c>: quantum heap writes require a pure element index");
        return build_zero_cst(TREE_TYPE(value));
    }
    tree rhs;
    if (increment) {
        tree read = expand_heap_read(TREE_OPERAND(value, 0));
        tree promoted = TYPE_PRECISION(access.type) < TYPE_PRECISION(integer_type_node)
                            ? integer_type_node
                            : TYPE_MAIN_VARIANT(access.type);
        rhs = build2_loc(at(value),
                         code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR ? MINUS_EXPR
                                                                                 : PLUS_EXPR,
                         promoted,
                         fold_convert(promoted, read),
                         build_int_cst(promoted, 1));
    } else {
        rhs = expand_quantum_program(TREE_OPERAND(value, 1));
        auto index_accesses = quantum_accesses(access.index, true);
        auto assigned_accesses = quantum_accesses(rhs, true);
        if (conflicting_accesses(index_accesses, assigned_accesses)) {
            error_at(
                at(value),
                ambiguous_heap_accesses(index_accesses, assigned_accesses)
                    ? "|c>: unsequenced heap index/value accesses require a proven disjoint-address contract"
                    : "|c>: unsequenced quantum heap index and assigned value have no defined C11 result");
            return build_zero_cst(TREE_TYPE(value));
        }
    }
    tree marker =
        build_fn_decl("__cket_heap_store",
                      build_function_type_list(
                          TREE_TYPE(value), TREE_TYPE(access.index), TREE_TYPE(rhs), NULL_TREE));
    tree result = build_call_expr_loc(at(value), marker, 2, access.index, rhs);
    quantum_heap_updates.emplace(
        result,
        QuantumHeapUpdate{access, rhs, code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR});
    return result;
}

tree expand_heap_call(tree call) {
    tree fn = get_callee_fndecl(call);
    std::string operation = heap_allocator(fn);
    bool move = false;
    bool set = standard_memory_set(fn), copy = standard_memory_copy(fn, move);
    if ((set || copy) && call_expr_nargs(call) == 3 &&
        (heap_pointer(CALL_EXPR_ARG(call, 0)) || (copy && heap_pointer(CALL_EXPR_ARG(call, 1))))) {
        tree destination = CALL_EXPR_ARG(call, 0), source = CALL_EXPR_ARG(call, 1),
             count = CALL_EXPR_ARG(call, 2), element = NULL_TREE;
        if (copy && (!heap_pointer(destination) || !heap_pointer(source)))
            return expand_heap_transfer(call, move);
        bool readonly = false;
        if (!heap_pointer(destination, element, readonly) || readonly ||
            (!set && !heap_pointer(source))) {
            error_at(
                at(call),
                "|c>: heap byte copy currently requires two heap views and a mutable destination");
            return build_zero_cst(TREE_TYPE(call));
        }
        tree source_type = NULL_TREE;
        bool source_readonly = false;
        if (!set)
            heap_pointer(source, source_type, source_readonly);
        if (!heap_byte_layout(element) || (!set && !heap_byte_layout(source_type))) {
            error_at(at(call), "|c>: heap byte operations require whole-byte integer layouts");
            return build_zero_cst(TREE_TYPE(call));
        }
        if (has_quantum(count) || !quantum_pure_expression(count) ||
            !quantum_pure_expression(destination) || (!set && !quantum_pure_expression(source))) {
            error_at(at(call),
                     "|c>: heap byte addresses and length require pure classical expressions");
            return build_zero_cst(TREE_TYPE(call));
        }
        if (set) {
            source = expand_quantum_program(source);
            if (!has_quantum(source)) {
                // Capture the ordinary C value argument once, even when the
                // byte range is empty or its write proves to be an identity.
                source = save_expr(source);
                cpu_library_saves.insert(source);
            }
        }
        tree marker =
            build_fn_decl("__cket_heap_memory",
                          build_function_type_list(TREE_TYPE(call), TREE_TYPE(source), NULL_TREE));
        tree result = build_call_expr_loc(at(call), marker, 1, source);
        quantum_native_memory.emplace(result,
                                      QuantumNativeMemory{save_expr(destination),
                                                          source,
                                                          count,
                                                          set    ? "set"
                                                          : move ? "move"
                                                                 : "copy"});
        return result;
    }
    if (operation == "free" && call_expr_nargs(call) == 1 && heap_pointer(CALL_EXPR_ARG(call, 0))) {
        if (heap_borrowed_pointer(CALL_EXPR_ARG(call, 0))) {
            error_at(at(call), "|c>: a borrowed quantum heap cannot be freed by its callee");
            return build_empty_stmt(at(call));
        }
        return heap_runtime_call("__cket_heap_free",
                                 void_type_node,
                                 {CALL_EXPR_ARG(call, 0)},
                                 {ptr_type_node},
                                 at(call));
    }
    if (quantum_function(fn))
        return NULL_TREE;
    if (!intrinsic(call).empty())
        return NULL_TREE;
    for (int i = 0; i < call_expr_nargs(call); ++i)
        if (heap_pointer(CALL_EXPR_ARG(call, i))) {
            error_at(at(call),
                     "|c>: this CPU call has no implemented quantum heap buffer contract");
            return build_zero_cst(TREE_TYPE(call));
        }
    return NULL_TREE;
}

std::string heap_address(const QuantumHeapAccess &access) {
    tree pointer = save_expr(access.pointer), ull = long_long_unsigned_type_node;
    std::string handle = cpu_parameter(heap_runtime_call(
        "__cket_heap_identity", ull, {pointer}, {const_ptr_type_node}, at(pointer)));
    std::string offset = cpu_parameter(heap_runtime_call(
        "__cket_heap_offset", ull, {pointer}, {const_ptr_type_node}, at(pointer)));
    return ",\"heap\":" + handle + ",\"offset\":" + offset;
}

std::string heap_expression(tree selection, std::set<unsigned> &deps) {
    const auto &access = quantum_heap_accesses.at(selection);
    if (!quantum_pure_expression(access.index)) {
        error_at(at(selection), "|c>: quantum heap reads require a pure element index");
        return "{}";
    }
    std::string address = heap_address(access);
    ++coherent_expression_depth;
    std::string index = expression(access.index, deps);
    --coherent_expression_depth;
    return "{\"op\":\"heap_read\"" + type_json(access.type) + address + ",\"index\":" + index + '}';
}

std::string heap_byte_expression(tree selection, std::set<unsigned> &) {
    const auto &access = quantum_heap_byte_reads.at(selection);
    tree pointer = save_expr(access.first), ull = long_long_unsigned_type_node;
    std::string handle = cpu_parameter(heap_runtime_call(
        "__cket_heap_byte_identity", ull, {pointer}, {const_ptr_type_node}, at(selection)));
    std::string offset = cpu_parameter(heap_runtime_call(
        "__cket_heap_byte_offset", ull, {pointer}, {const_ptr_type_node}, at(selection)));
    return "{\"op\":\"heap_byte_read\",\"bits\":8,\"signed\":false,\"heap\":" + handle +
           ",\"offset\":" + offset +
           ",\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":" +
           std::to_string(access.second) +
           "},\"big_endian\":" + (BYTES_BIG_ENDIAN ? "true" : "false") + '}';
}

tree lower_heap_effect_capture(tree marker,
                               unsigned capture,
                               bool returns_value,
                               std::set<unsigned> *capture_dependencies = nullptr) {
    const auto &update = quantum_heap_updates.at(marker);
    const auto &access = update.target;
    tree list = alloc_stmt_list();
    std::map<tree, tree> promoted;
    tree value = restore_integer_arithmetic(update.value, promoted);
    std::set<unsigned> deps;
    std::string expr = evaluate_expression(value, deps, &list);
    expr = "{\"op\":\"cast\"" + type_json(access.type) + ",\"args\":[" + expr + "]}";
    std::string address = heap_address(access);
    bool coherent_index = has_quantum(access.index);
    std::string index;
    if (coherent_index) {
        ++coherent_expression_depth;
        index = expression(fold_convert(long_long_unsigned_type_node, access.index), deps);
        --coherent_expression_depth;
    } else
        index = cpu_parameter(fold_convert(long_long_unsigned_type_node, access.index));
    std::string saved_value;
    if (returns_value) {
        std::string result_expr = expr;
        if (update.postfix) {
            ++coherent_expression_depth;
            std::string selected = expression(access.index, deps);
            --coherent_expression_depth;
            result_expr = "{\"op\":\"heap_read\"" + type_json(access.type) + address +
                          ",\"index\":" + selected + '}';
        }
        saved_value =
            ",\"capture\":{\"id\":" + std::to_string(capture) + ",\"expr\":" + result_expr + '}';
    }
    append(&list,
           emit_operation(
               "{\"op\":\"" + std::string(coherent_index ? "qheap_scatter" : "qheap_store") +
                   "\",\"width\":" + std::to_string(TYPE_PRECISION(access.type)) +
                   ",\"signed\":" + (TYPE_UNSIGNED(access.type) ? "false" : "true") + address +
                   ",\"index\":" + index + ",\"expr\":" + expr + saved_value + control_json() + '}',
               at(marker)));
    for (unsigned source : deps)
        taint_uniform(source);
    defer_heap_decisions();
    if (capture_dependencies) {
        *capture_dependencies = deps;
        for (const auto &control : controls)
            capture_dependencies->insert(control.first);
    }
    return list;
}

tree lower_heap_effect(tree marker) {
    return lower_heap_effect_capture(marker, 0, false);
}

tree lower_native_memory(tree marker) {
    const auto &memory = quantum_native_memory.at(marker);
    tree ull = long_long_unsigned_type_node, list = alloc_stmt_list();
    if (memory.mode == "fixed") {
        std::string count = cpu_parameter(fold_convert(ull, memory.count));
        append(&list,
               emit_operation("{\"op\":\"cmemory\",\"count\":" + count +
                                  ",\"limit\":" + std::to_string(memory.limit) +
                                  (memory.overlap == UINT64_MAX
                                       ? ""
                                       : ",\"overlap\":" + std::to_string(memory.overlap)) +
                                  control_json() + '}',
                              at(marker)));
        ++runtime_memory_depth;
        append(&list, lower_node(memory.fixed_output));
        --runtime_memory_depth;
        return prefixed_value(list, fold_convert(TREE_TYPE(marker), memory.destination));
    }
    auto address = [&](tree pointer, const char *field) {
        return std::string(",\"") + field + "\":" +
               cpu_parameter(heap_runtime_call("__cket_heap_byte_identity",
                                               ull,
                                               {pointer},
                                               {const_ptr_type_node},
                                               at(marker))) +
               ",\"" + field + "_offset\":" +
               cpu_parameter(heap_runtime_call(
                   "__cket_heap_byte_offset", ull, {pointer}, {const_ptr_type_node}, at(marker)));
    };
    std::set<unsigned> deps;
    std::string source;
    if (memory.mode == "bytes") {
        source = ",\"bytes\":[";
        for (tree byte : memory.bytes) {
            if (source.back() != '[')
                source += ',';
            std::map<tree, tree> promoted;
            tree value = restore_integer_arithmetic(byte, promoted);
            source += "{\"op\":\"cast\",\"bits\":8,\"signed\":false,\"args\":[" +
                      evaluate_expression(value, deps, &list) + "]}";
        }
        source += ']';
    } else if (memory.fixed_output)
        source = ",\"limit\":" + std::to_string(memory.limit);
    else if (memory.mode == "set") {
        std::map<tree, tree> promoted;
        tree value = restore_integer_arithmetic(memory.source, promoted);
        source = ",\"expr\":{\"op\":\"cast\",\"bits\":8,\"signed\":false,\"args\":[" +
                 evaluate_expression(value, deps, &list) + "]}";
    } else
        source = address(save_expr(memory.source), "source");
    std::string destination =
        address(memory.fixed_output ? memory.source : memory.destination, "heap");
    std::string count = cpu_parameter(fold_convert(ull, memory.count));
    append(&list,
           emit_operation("{\"op\":\"qheap_memory\",\"mode\":" + json(memory.mode) + destination +
                              source + ",\"count\":" + count + ",\"big_endian\":" +
                              (BYTES_BIG_ENDIAN ? "true" : "false") + control_json() + '}',
                          at(marker)));
    for (unsigned id : deps)
        taint_uniform(id);
    for (const auto &control : controls)
        taint_uniform(control.first);
    defer_heap_decisions();
    if (memory.fixed_output)
        append(&list, lower_node(memory.fixed_output));
    return prefixed_value(list, fold_convert(TREE_TYPE(marker), memory.destination));
}

std::string heap_effect_expression(tree marker, std::set<unsigned> &deps) {
    if (expression_frames.empty()) {
        error_at(at(marker), "|c>: heap update values require a scheduled consumer");
        return "{}";
    }
    const auto &access = quantum_heap_updates.at(marker).target;
    std::set<unsigned> previous_parameters;
    previous_parameters.swap(active_cpu_parameters);
    Variable temporary{};
    temporary.id = next_id++;
    temporary.width = TYPE_PRECISION(access.type);
    temporary.is_signed = !TYPE_UNSIGNED(access.type);
    temporary.name = "__cket_heap_value_" + std::to_string(temporary.id);
    temporary.decl =
        build_decl(at(marker), VAR_DECL, get_identifier(temporary.name.c_str()), access.type);
    temporary.lowered = temporary.derived = true;
    temporary.optional_definition = dynamic_depth != 0;
    temporary.range_max = temporary.width == 64 ? UINT64_MAX : (uint64_t{1} << temporary.width) - 1;
    // The engine resolves heap identities and keeps the exact saved recipe.
    // An empty creation requests that recipe during compiler cleanup.
    variables.emplace(temporary.decl, temporary);
    tree *prefix = expression_frames.back()->prefix;
    append(prefix,
           emit_operation("{\"op\":\"qalloc\",\"id\":" + std::to_string(temporary.id) +
                              ",\"name\":" + json(temporary.name) +
                              ",\"width\":" + std::to_string(temporary.width) + ",\"signed\":" +
                              (temporary.is_signed ? "true" : "false") + ",\"ancilla\":true}",
                          at(marker)));
    append(prefix,
           lower_heap_effect_capture(
               marker, temporary.id, true, &variables.at(temporary.decl).dependencies));
    active_cpu_parameters.insert(previous_parameters.begin(), previous_parameters.end());
    deps.insert(temporary.id);
    expression_frames.back()->reads.insert(temporary.id);
    return register_expression(variables.at(temporary.decl));
}

tree lower_heap_channel(tree call) {
    tree original = CALL_EXPR_ARG(call, 0), operand = strip(original);
    auto found = quantum_heap_accesses.find(operand);
    if (found == quantum_heap_accesses.end() || has_quantum(found->second.index))
        return NULL_TREE;
    const auto &access = found->second;
    unsigned width = TYPE_PRECISION(access.type);
    bool storage = INTEGRAL_TYPE_P(TREE_TYPE(call)) && TYPE_PRECISION(TREE_TYPE(call)) == width;
    while (CONVERT_EXPR_P(original) || TREE_CODE(original) == NON_LVALUE_EXPR) {
        storage &=
            INTEGRAL_TYPE_P(TREE_TYPE(original)) && TYPE_PRECISION(TREE_TYPE(original)) >= width;
        original = TREE_OPERAND(original, 0);
    }
    if (intrinsic(call) == "measure" && !storage)
        return NULL_TREE;
    if (!controls.empty() || access.readonly || !quantum_pure_expression(access.index)) {
        error_at(
            at(call),
            "|c>: quantum heap measure/reset needs a mutable borrow and classical control/index");
        return build_zero_cst(TREE_TYPE(call));
    }
    std::string address = heap_address(access);
    std::string index = cpu_parameter(fold_convert(long_long_unsigned_type_node, access.index));
    tree action = emit_operation(
        "{\"op\":\"qheap_" + (intrinsic(call) == "measure" ? std::string("measure") : "reset") +
            "\",\"width\":" + std::to_string(TYPE_PRECISION(access.type)) + ",\"signed\":" +
            (TYPE_UNSIGNED(access.type) ? "false" : "true") + address + ",\"index\":" + index + '}',
        at(call));
    return fold_convert(TREE_TYPE(call), action);
}
