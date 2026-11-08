// Runtime CPU lengths select a checked prefix of fixed integer object bytes.
// Bounds and memcpy overlap are checked before captures or destination writes.
tree expand_dynamic_memory(tree call, bool move, bool set) {
    QuantumByteRange destination, source;
    bool dst_ok = resolve_byte_range(CALL_EXPR_ARG(call, 0), destination);
    bool src_ok = set || resolve_byte_range(CALL_EXPR_ARG(call, 1), source);
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!dst_ok || !destination.quantum || !src_ok)
        return reject(
            destination.error ? destination.error
            : source.error
                ? source.error
                : "|c>: runtime memory length requires checked fixed quantum destination and quantum/CPU source bytes");
    if (TYPE_READONLY(destination.element_type))
        return reject("|c>: runtime memory length requires mutable destination storage");
    tree count = CALL_EXPR_ARG(call, 2), ull = long_long_unsigned_type_node;
    if (has_quantum(count)) {
        tree q_count = expand_quantum_program(count);
        if (!q_count || !INTEGRAL_TYPE_P(TREE_TYPE(q_count)) ||
            TYPE_PRECISION(TREE_TYPE(q_count)) > 64 || !quantum_pure_expression(q_count))
            return reject(
                "|c>: quantum memory length requires a pure integer expression of at most 64 bits");
        uint64_t limit = destination.size() - destination.offset;
        if (!set)
            limit = std::min(limit, source.size() - source.offset);
        if (limit > 64)
            return reject(
                "|c>: quantum memory length exceeds the 64-byte expression synthesis bound");
        uint64_t max_valid = limit;
        if (!set && !move && source.quantum)
            for (uint64_t i = 0; i < limit; ++i) {
                uint64_t dst = destination.offset + i;
                tree target = destination.elements[(size_t)(dst / destination.stride)];
                for (uint64_t j = 0; j < limit; ++j) {
                    uint64_t src = source.offset + j;
                    if (target == source.elements[(size_t)(src / source.stride)] &&
                        (string_byte_mask(destination, dst) & string_byte_mask(source, src)))
                        max_valid = std::min(max_valid, std::max(i, j));
                }
            }
        std::vector<tree> prefix;
        tree fill = NULL_TREE;
        if (set) {
            fill = expand_quantum_program(CALL_EXPR_ARG(call, 1));
            if (!has_quantum(fill)) {
                fill = save_expr(fill);
                cpu_library_saves.insert(fill);
            }
            fill = quantum_value_capture(call, fill, "__cket_dynamic_fill_", prefix);
        }
        tree q_u = fold_convert(ull, q_count);
        tree defined = fold_build2_loc(
            at(call), LE_EXPR, boolean_type_node, q_u, build_int_cstu(ull, max_valid));
        if (!TYPE_UNSIGNED(TREE_TYPE(q_count)))
            defined = fold_build2_loc(at(call),
                                      TRUTH_ANDIF_EXPR,
                                      boolean_type_node,
                                      fold_build2_loc(at(call),
                                                      GE_EXPR,
                                                      boolean_type_node,
                                                      q_count,
                                                      build_zero_cst(TREE_TYPE(q_count))),
                                      defined);
        tree captured_len = quantum_value_capture(
            call, fold_convert(unsigned_char_type_node, q_count), "__cket_qmem_len_", prefix);
        bool identity = move && source.quantum && destination.elements == source.elements &&
                        destination.offset == source.offset && destination.stride == source.stride;
        std::vector<std::pair<tree, tree>> words;
        std::map<tree, tree> bit_masks, mask_values;
        std::map<tree, std::set<tree>> inputs;
        for (uint64_t i = 0; i < limit && !identity; ++i) {
            uint64_t dst = destination.offset + i;
            tree target = destination.elements[(size_t)(dst / destination.stride)];
            if (readonly_quantum_borrows.count(target))
                return reject("|c>: runtime memory cannot write through a read-only borrow");
            tree selected = fold_build2_loc(at(call),
                                            GT_EXPR,
                                            boolean_type_node,
                                            captured_len,
                                            build_int_cstu(unsigned_char_type_node, i));
            tree next = set ? fill : object_byte(source, i, at(call));
            string_word_byte(
                destination, i, fold_convert(integer_type_node, next), words, at(call));
            tree type = unsigned_type_for(TREE_TYPE(target));
            uint64_t mask = string_byte_mask(destination, dst);
            auto prior_bits = bit_masks.find(target);
            bit_masks[target] = build_int_cstu(
                ull, mask | (prior_bits == bit_masks.end() ? 0 : tree_to_uhwi(prior_bits->second)));
            tree enabled = fold_build3_loc(at(call),
                                           COND_EXPR,
                                           type,
                                           selected,
                                           build_int_cstu(type, mask),
                                           build_zero_cst(type));
            auto prior_mask = mask_values.find(target);
            mask_values[target] =
                prior_mask == mask_values.end()
                    ? enabled
                    : fold_build2_loc(at(call), BIT_IOR_EXPR, type, prior_mask->second, enabled);
            if (!set && source.quantum)
                inputs[target].insert(
                    source.elements[(size_t)((source.offset + i) / source.stride)]);
        }
        std::map<tree, unsigned> writers;
        for (unsigned i = 0; i < words.size(); ++i)
            writers.emplace(words[i].first, i);
        std::vector<unsigned> indegree(words.size(), 0), ready;
        std::vector<std::set<unsigned>> next(words.size());
        for (unsigned reader = 0; reader < words.size(); ++reader)
            for (tree input : inputs[words[reader].first]) {
                auto writer = writers.find(input);
                if (writer != writers.end() && writer->second != reader &&
                    next[reader].insert(writer->second).second)
                    ++indegree[writer->second];
            }
        for (unsigned i = 0; i < words.size(); ++i)
            if (!indegree[i])
                ready.push_back(i);
        std::vector<std::pair<tree, tree>> ordered;
        for (size_t i = 0; i < ready.size(); ++i) {
            unsigned index = ready[i];
            ordered.push_back(words[index]);
            for (unsigned writer : next[index])
                if (!--indegree[writer])
                    ready.push_back(writer);
        }
        if (ordered.size() != words.size())
            return reject("|c>: cyclic runtime byte-copy dependencies need a retained snapshot");
        std::map<tree, tree> quantum_masks;
        for (const auto &entry : mask_values)
            quantum_masks[entry.first] =
                quantum_value_capture(call,
                                      entry.second,
                                      "__cket_qmem_mask_",
                                      prefix,
                                      unsigned_type_for(TREE_TYPE(entry.first)));
        return make_quantum_value_program(
            call, ordered, defined, std::move(prefix), NULL_TREE, bit_masks, quantum_masks);
    }
    if (!quantum_pure_expression(count))
        return reject(
            "|c>: runtime fixed memory length requires a pure classical integer expression");
    tree length = save_expr(fold_convert(ull, count));
    QuantumNativeMemory operation{
        save_expr(heap_transfer_address(CALL_EXPR_ARG(call, 0))), NULL_TREE, length, "fixed"};
    operation.limit = std::min<uint64_t>(64, destination.size() - destination.offset);
    if (!set)
        operation.limit = std::min(operation.limit, source.size() - source.offset);
    std::vector<tree> prefix;
    tree fill = NULL_TREE;
    if (set) {
        fill = expand_quantum_program(CALL_EXPR_ARG(call, 1));
        if (!has_quantum(fill)) {
            fill = save_expr(fill);
            cpu_library_saves.insert(fill);
        }
        fill = quantum_value_capture(call, fill, "__cket_dynamic_fill_", prefix);
    }
    bool identity = move && source.quantum && destination.elements == source.elements &&
                    destination.offset == source.offset && destination.stride == source.stride;
    std::vector<std::pair<tree, tree>> words;
    std::map<tree, tree> bit_masks;
    std::map<tree, std::set<tree>> inputs;
    for (uint64_t i = 0; i < operation.limit && !identity; ++i) {
        uint64_t dst = destination.offset + i;
        tree target = destination.elements[(size_t)(dst / destination.stride)];
        if (readonly_quantum_borrows.count(target))
            return reject("|c>: runtime memory cannot write through a read-only borrow");
        tree selected =
            fold_build2_loc(at(call), LT_EXPR, boolean_type_node, build_int_cstu(ull, i), length);
        source.cpu_guard = selected;
        tree next = set ? fill : object_byte(source, i, at(call));
        tree byte = fold_build3_loc(at(call),
                                    COND_EXPR,
                                    integer_type_node,
                                    selected,
                                    fold_convert(integer_type_node, next),
                                    object_byte(destination, i, at(call)));
        string_word_byte(destination, i, byte, words, at(call));
        tree mask = fold_build3_loc(at(call),
                                    COND_EXPR,
                                    ull,
                                    selected,
                                    build_int_cstu(ull, string_byte_mask(destination, dst)),
                                    build_zero_cst(ull));
        auto prior = bit_masks.find(target);
        bit_masks[target] = prior == bit_masks.end()
                                ? mask
                                : fold_build2_loc(at(call), BIT_IOR_EXPR, ull, prior->second, mask);
        if (!set && source.quantum) {
            inputs[target].insert(source.elements[(size_t)((source.offset + i) / source.stride)]);
            if (!move)
                for (uint64_t j = 0; j < operation.limit; ++j) {
                    uint64_t src = source.offset + j;
                    if (target == source.elements[(size_t)(src / source.stride)] &&
                        (string_byte_mask(destination, dst) & string_byte_mask(source, src)))
                        operation.overlap = std::min(operation.overlap, std::max(i, j));
                }
        }
    }
    std::map<tree, unsigned> writers;
    for (unsigned i = 0; i < words.size(); ++i)
        writers.emplace(words[i].first, i);
    std::vector<unsigned> indegree(words.size(), 0), ready;
    std::vector<std::set<unsigned>> next(words.size());
    for (unsigned reader = 0; reader < words.size(); ++reader)
        for (tree input : inputs[words[reader].first]) {
            auto writer = writers.find(input);
            if (writer != writers.end() && writer->second != reader &&
                next[reader].insert(writer->second).second)
                ++indegree[writer->second];
        }
    for (unsigned i = 0; i < words.size(); ++i)
        if (!indegree[i])
            ready.push_back(i);
    for (size_t i = 0; i < ready.size(); ++i)
        for (unsigned writer : next[ready[i]])
            if (!--indegree[writer])
                ready.push_back(writer);
    if (ready.size() != words.size())
        return reject("|c>: cyclic runtime byte-copy dependencies need a retained snapshot");
    operation.fixed_output = alloc_stmt_list();
    // The fill argument is evaluated once even for a zero-length call.
    if (!prefix.empty())
        append(&operation.fixed_output,
               make_quantum_value_program(call, {}, boolean_true_node, prefix));
    for (unsigned index : ready) {
        const auto &word = words[index];
        auto found =
            std::find(destination.elements.begin(), destination.elements.end(), word.first);
        uint64_t first = (uint64_t)(found - destination.elements.begin()) * destination.stride;
        first = first < destination.offset ? 0 : first - destination.offset;
        tree active = fold_build2_loc(
            at(call), GT_EXPR, boolean_type_node, length, build_int_cstu(ull, first));
        tree program =
            make_quantum_value_program(call, {word}, boolean_true_node, {}, NULL_TREE, bit_masks);
        tree branch = build3_loc(
            at(call), COND_EXPR, void_type_node, active, program, build_empty_stmt(at(call)));
        quantum_memory_cpu_branches.insert(branch);
        append(&operation.fixed_output, branch);
    }
    std::vector<tree> arguments{operation.fixed_output};
    tree fn = build_fn_decl("__cket_fixed_memory", build_function_type(TREE_TYPE(call), NULL_TREE));
    tree result = build_call_expr_loc_array(at(call), fn, arguments.size(), arguments.data());
    quantum_native_memory.emplace(result, std::move(operation));
    return result;
}
