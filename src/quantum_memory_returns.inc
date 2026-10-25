// Checked memory-return addresses and ordered quantum data reads.
tree quantum_memory_return(tree value) {
    value = strip(value);
    if (!value || !TREE_TYPE(value) || TREE_CODE(TREE_TYPE(value)) != POINTER_TYPE)
        return NULL_TREE;
    if (quantum_native_memory.count(value))
        return value;
    bool move = false;
    if (TREE_CODE(value) != CALL_EXPR || call_expr_nargs(value) != 3 ||
        (!standard_memory_set(get_callee_fndecl(value)) &&
         !standard_memory_copy(get_callee_fndecl(value), move)))
        return NULL_TREE;
    QuantumByteRange destination;
    resolve_byte_range(CALL_EXPR_ARG(value, 0), destination);
    return destination.quantum || heap_pointer(CALL_EXPR_ARG(value, 0)) ? value : NULL_TREE;
}

tree memory_return_in_address(tree address, bool any = false) {
    if (any) {
        auto find = [](tree *node, int *walk, void *) -> tree {
            if (DECL_P(*node) || TYPE_P(*node))
                *walk = 0;
            return quantum_memory_return(*node);
        };
        return walk_tree_without_duplicates(&address, find, nullptr);
    }
    for (unsigned depth = 0; address && depth < 128; ++depth) {
        address = strip(address);
        tree call = quantum_memory_return(address);
        if (call)
            return call;
        if (TREE_CODE(address) == SAVE_EXPR || TREE_CODE(address) == POINTER_PLUS_EXPR)
            address = TREE_OPERAND(address, 0);
        else
            break;
    }
    return NULL_TREE;
}

tree expand_memory_return_read(tree value) {
    tree pointer = TREE_OPERAND(value, 0), call = memory_return_in_address(pointer);
    if (!call) {
        if (memory_return_in_address(pointer, true)) {
            error_at(at(value),
                     "|c>: conditional memory-return data access requires a checked local borrow");
            return build_zero_cst(TREE_TYPE(value));
        }
        return NULL_TREE;
    }
    tree destination = quantum_native_memory.count(call)
                           ? quantum_native_memory.at(call).destination
                           : CALL_EXPR_ARG(call, 0);
    std::map<tree, tree> bindings{{call, destination}};
    tree checked = copy_node(value);
    TREE_OPERAND(checked, 0) = clone_quantum_tree(pointer, bindings);
    tree read = expand_quantum_program(checked);
    if (!has_quantum(read)) {
        error_at(at(value),
                 "|c>: memory-return data access requires a compatible checked quantum borrow");
        return build_zero_cst(TREE_TYPE(value));
    }
    // Sequence the actual memory effect before the logical register read.
    // A CPU consumer then requires explicit observation; no host byte is read.
    tree effect = expand_quantum_program(call);
    return build2_loc(at(value), COMPOUND_EXPR, TREE_TYPE(value), effect, read);
}

bool memory_return_address_value(tree value) {
    value = strip(value);
    if (!value)
        return false;
    if (quantum_memory_return(value))
        return true;
    auto code = TREE_CODE(value);
    if (code == COMPOUND_EXPR)
        return memory_return_address_value(TREE_OPERAND(value, 1));
    if (code == COND_EXPR)
        return memory_return_address_value(TREE_OPERAND(value, 1)) ||
               memory_return_address_value(TREE_OPERAND(value, 2));
    if (code == CONSTRUCTOR) {
        unsigned i;
        tree index, element;
        FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(value), i, index, element)
        if (memory_return_address_value(element))
            return true;
        (void)index;
        return false;
    }
    if (DECL_P(value) || TYPE_P(value) || code == CALL_EXPR || code == INDIRECT_REF ||
        code == ARRAY_REF || code == COMPONENT_REF || TREE_CODE_CLASS(code) == tcc_comparison)
        return false;
    if (EXPR_P(value))
        for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
            if (memory_return_address_value(TREE_OPERAND(value, i)))
                return true;
    return false;
}
