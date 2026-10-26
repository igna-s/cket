// Cross the fixed/heap boundary using actual register expressions. CPU source
// bytes require a known object; a heap placeholder never enters that path.
tree heap_byte_read_marker(tree pointer, uint64_t index, location_t loc) {
    tree fn = build_fn_decl("__cket_heap_byte_read",
                            build_function_type_list(unsigned_char_type_node, NULL_TREE));
    tree result = build_call_expr_loc(loc, fn, 0);
    quantum_heap_byte_reads.emplace(result, std::make_pair(pointer, index));
    return result;
}

tree heap_transfer_address(tree value, unsigned depth = 0) {
    if (!value)
        return value;
    if (depth > 128) {
        error_at(at(value), "|c>: heap transfer address exceeds its checked alias depth");
        return build_zero_cst(TREE_TYPE(value));
    }
    tree key = strip(value);
    auto alias = quantum_pointer_aliases.find(key);
    if (alias != quantum_pointer_aliases.end())
        return fold_convert(TREE_TYPE(value), heap_transfer_address(alias->second, depth + 1));
    auto reference = quantum_references.find(key);
    if (reference != quantum_references.end() && reference->second.address)
        return fold_convert(TREE_TYPE(value),
                            heap_transfer_address(reference->second.address, depth + 1));
    if (DECL_P(value) || TYPE_P(value) || CONSTANT_CLASS_P(value))
        return value;
    tree result = copy_node(value);
    if (EXPR_P(value))
        for (int i = 0; i < TREE_OPERAND_LENGTH(value); ++i)
            TREE_OPERAND(result, i) = heap_transfer_address(TREE_OPERAND(value, i), depth + 1);
    return result;
}

tree expand_heap_transfer(tree call, bool move) {
    tree destination = CALL_EXPR_ARG(call, 0), source = CALL_EXPR_ARG(call, 1),
         count = CALL_EXPR_ARG(call, 2), ull = long_long_unsigned_type_node;
    bool to_heap = heap_pointer(destination);
    tree heap_type = NULL_TREE;
    bool readonly = false;
    heap_pointer(to_heap ? destination : source, heap_type, readonly);
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!heap_byte_layout(heap_type))
        return reject("|c>: heap byte transfer requires a whole-byte integer heap layout");
    if (to_heap && readonly)
        return reject("|c>: heap byte transfer requires a mutable destination");
    tree heap = to_heap ? destination : source;
    if (has_quantum(count) || !quantum_pure_expression(count) || !quantum_pure_expression(heap))
        return reject("|c>: heap byte addresses and length require pure classical expressions");
    QuantumByteRange fixed;
    if (!resolve_byte_range(to_heap ? source : destination, fixed))
        return reject(
            fixed.error
                ? fixed.error
                : "|c>: heap transfer requires a checked fixed quantum buffer or known CPU source object");
    if (!to_heap && !fixed.quantum)
        return reject("|c>: heap transfer cannot write unmeasured data into CPU storage");
    if (!to_heap && TYPE_READONLY(fixed.element_type))
        return reject("|c>: heap byte transfer requires mutable fixed destination storage");
    uint64_t capacity = std::min<uint64_t>(64, fixed.size() - fixed.offset);
    tree length = save_expr(fold_convert(ull, count));
    QuantumNativeMemory operation{save_expr(heap_transfer_address(destination)),
                                  save_expr(source),
                                  length,
                                  to_heap ? "bytes" : "check"};
    operation.limit = capacity;
    std::vector<tree> arguments;
    if (to_heap) {
        for (uint64_t i = 0; i < capacity; ++i) {
            fixed.cpu_guard = fold_build2_loc(
                at(call), LT_EXPR, boolean_type_node, build_int_cstu(ull, i), length);
            tree byte = object_byte(fixed, i, at(call));
            operation.bytes.push_back(byte);
            arguments.push_back(byte);
        }
    } else {
        std::vector<std::pair<tree, tree>> words;
        std::map<tree, tree> bit_masks;
        for (uint64_t i = 0; i < capacity; ++i) {
            uint64_t position = fixed.offset + i;
            tree target = fixed.elements[(size_t)(position / fixed.stride)];
            if (readonly_quantum_borrows.count(target))
                return reject("|c>: heap transfer cannot write through a read-only borrow");
            tree selected = fold_build2_loc(
                at(call), LT_EXPR, boolean_type_node, build_int_cstu(ull, i), length);
            tree byte =
                fold_build3_loc(at(call),
                                COND_EXPR,
                                integer_type_node,
                                selected,
                                fold_convert(integer_type_node,
                                             heap_byte_read_marker(operation.source, i, at(call))),
                                object_byte(fixed, i, at(call)));
            string_word_byte(fixed, i, byte, words, at(call));
            tree mask = fold_build3_loc(at(call),
                                        COND_EXPR,
                                        ull,
                                        selected,
                                        build_int_cstu(ull, string_byte_mask(fixed, position)),
                                        build_zero_cst(ull));
            auto prior = bit_masks.find(target);
            bit_masks[target] =
                prior == bit_masks.end()
                    ? mask
                    : fold_build2_loc(at(call), BIT_IOR_EXPR, ull, prior->second, mask);
        }
        operation.fixed_output = alloc_stmt_list();
        for (const auto &word : words) {
            auto found = std::find(fixed.elements.begin(), fixed.elements.end(), word.first);
            uint64_t first = (uint64_t)(found - fixed.elements.begin()) * fixed.stride;
            first = first < fixed.offset ? 0 : first - fixed.offset;
            tree active = fold_build2_loc(
                at(call), GT_EXPR, boolean_type_node, length, build_int_cstu(ull, first));
            tree program = make_quantum_value_program(
                call, {word}, boolean_true_node, {}, NULL_TREE, bit_masks);
            tree branch = build3_loc(
                at(call), COND_EXPR, void_type_node, active, program, build_empty_stmt(at(call)));
            quantum_memory_cpu_branches.insert(branch);
            append(&operation.fixed_output, branch);
            arguments.push_back(program);
        }
    }
    tree fn =
        build_fn_decl("__cket_heap_transfer", build_function_type(TREE_TYPE(call), NULL_TREE));
    tree result = build_call_expr_loc_array(at(call), fn, arguments.size(), arguments.data());
    quantum_native_memory.emplace(result, std::move(operation));
    return result;
}
