tree string_length_expression(QuantumByteRange &range,
                              uint64_t reads,
                              tree fallback,
                              location_t location) {
    tree length = fallback;
    for (uint64_t i = reads; i; --i) {
        tree zero = fold_build2_loc(location,
                                    EQ_EXPR,
                                    boolean_type_node,
                                    object_byte(range, i - 1, location),
                                    build_zero_cst(integer_type_node));
        length = fold_build3_loc(location,
                                 COND_EXPR,
                                 unsigned_char_type_node,
                                 zero,
                                 build_int_cstu(unsigned_char_type_node, i - 1),
                                 length);
    }
    return length;
}

tree expand_string_concat(tree call) {
    tree fn = get_callee_fndecl(call);
    bool bounded = fndecl_built_in_p(fn, BUILT_IN_STRNCAT) || name_of(fn) == "strncat" ||
                   name_of(fn) == "__cket_strncat";
    if (call_expr_nargs(call) != (bounded ? 3 : 2))
        return NULL_TREE;
    QuantumByteRange destination, source;
    bool resolved_destination = resolve_byte_range(CALL_EXPR_ARG(call, 0), destination);
    bool resolved_source = resolve_byte_range(CALL_EXPR_ARG(call, 1), source);
    if (!destination.quantum && !source.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)) &&
        !has_quantum(CALL_EXPR_ARG(call, 1)))
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!destination.quantum)
        return reject("|c>: quantum concatenation cannot write unmeasured data into CPU storage");
    if (!resolved_destination || !resolved_source)
        return reject(
            !resolved_destination && destination.error ? destination.error
            : !resolved_source && source.error
                ? source.error
                : "|c>: quantum concatenation needs checked integer destination bytes and checked quantum/CPU source bytes");
    if (TYPE_READONLY(destination.element_type))
        return reject("|c>: quantum concatenation requires mutable destination storage");
    uint64_t count = destination.size() - destination.offset;
    uint64_t available_source = source.size() - source.offset;
    uint64_t limit = available_source;
    if (bounded) {
        tree length = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!length || TREE_CODE(length) != INTEGER_CST || !tree_fits_uhwi_p(length))
            return reject("|c>: quantum strncat requires a constant checked append count");
        limit = tree_to_uhwi(length);
    }
    if (!count || (!bounded && !available_source) || (limit && !available_source))
        return reject(
            "|c>: quantum concatenation needs checked string storage and space for its terminating NUL");
    if (count > 64 || limit > 64)
        return reject("|c>: quantum concatenation exceeds the 64-byte expression synthesis bound");
    uint64_t reads = std::min(limit, available_source);
    std::map<tree, uint64_t> source_masks;
    if (source.quantum)
        for (uint64_t i = 0; i < reads; ++i) {
            uint64_t position = source.offset + i;
            source_masks[source.elements[(size_t)(position / source.stride)]] |=
                string_byte_mask(source, position);
        }
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t position = destination.offset + i;
        tree target = destination.elements[(size_t)(position / destination.stride)];
        if (readonly_quantum_borrows.count(target))
            return reject("|c>: quantum concatenation cannot write through a read-only borrow");
        auto overlap = source_masks.find(target);
        if (overlap != source_masks.end() &&
            (overlap->second & string_byte_mask(destination, position)))
            return reject("|c>: potentially overlapping strcat/strncat byte views are unsupported");
    }
    source.stop_at_zero = true;
    tree invalid = missing_string_terminator(unsigned_char_type_node, at(call));
    tree destination_length = string_length_expression(destination, count, invalid, at(call));
    tree source_fallback =
        bounded && reads == limit ? build_int_cstu(unsigned_char_type_node, limit) : invalid;
    tree source_length = string_length_expression(source, reads, source_fallback, at(call));
    std::vector<tree> prefix;
    tree start = fold_convert(
        integer_type_node,
        quantum_value_capture(call, destination_length, "__cket_concat_start_", prefix));
    tree copied =
        fold_convert(integer_type_node,
                     quantum_value_capture(call, source_length, "__cket_concat_count_", prefix));
    tree end = fold_build2_loc(at(call), PLUS_EXPR, integer_type_node, start, copied);
    tree defined = fold_build2_loc(
        at(call), LT_EXPR, boolean_type_node, end, build_int_cstu(integer_type_node, count));
    tree fixed_length = strip(fold(source_length));
    if (fixed_length && TREE_CODE(fixed_length) == INTEGER_CST && integer_zerop(fixed_length))
        // An empty append validates the destination string but writes no
        // bytes. It needs neither a word mask nor an old-value history.
        return make_quantum_value_program(call, {}, defined, std::move(prefix));
    std::vector<std::pair<tree, tree>> words;
    std::map<tree, tree> bit_masks, mask_values;
    for (uint64_t i = 0; i < count; ++i) {
        tree next = build_zero_cst(integer_type_node);
        for (uint64_t j = 0; j < reads; ++j) {
            tree after_start = fold_build2_loc(at(call),
                                               PLUS_EXPR,
                                               integer_type_node,
                                               start,
                                               build_int_cstu(integer_type_node, j));
            tree here = fold_build2_loc(at(call),
                                        EQ_EXPR,
                                        boolean_type_node,
                                        after_start,
                                        build_int_cstu(integer_type_node, i));
            tree in_prefix = fold_build2_loc(
                at(call), GT_EXPR, boolean_type_node, copied, build_int_cstu(integer_type_node, j));
            tree selected =
                fold_build2_loc(at(call), TRUTH_ANDIF_EXPR, boolean_type_node, here, in_prefix);
            next = fold_build3_loc(at(call),
                                   COND_EXPR,
                                   integer_type_node,
                                   selected,
                                   object_byte(source, j, at(call)),
                                   next);
        }
        tree position = build_int_cstu(integer_type_node, i);
        tree after_start = fold_build2_loc(at(call), LE_EXPR, boolean_type_node, start, position);
        tree through_end = fold_build2_loc(at(call), LE_EXPR, boolean_type_node, position, end);
        tree nonempty = fold_build2_loc(
            at(call), NE_EXPR, boolean_type_node, copied, build_zero_cst(integer_type_node));
        tree enabled = fold_build2_loc(
            at(call),
            TRUTH_ANDIF_EXPR,
            boolean_type_node,
            nonempty,
            fold_build2_loc(
                at(call), TRUTH_ANDIF_EXPR, boolean_type_node, after_start, through_end));
        uint64_t offset = destination.offset + i;
        tree target = destination.elements[(size_t)(offset / destination.stride)];
        tree type = unsigned_type_for(TREE_TYPE(target));
        uint64_t mask = string_byte_mask(destination, offset);
        auto previous_bits = bit_masks.find(target);
        bit_masks[target] = build_int_cstu(
            long_long_unsigned_type_node,
            mask | (previous_bits == bit_masks.end() ? 0 : tree_to_uhwi(previous_bits->second)));
        tree selected = fold_build3_loc(
            at(call), COND_EXPR, type, enabled, build_int_cstu(type, mask), build_zero_cst(type));
        auto previous_mask = mask_values.find(target);
        mask_values[target] =
            previous_mask == mask_values.end()
                ? selected
                : fold_build2_loc(at(call), BIT_IOR_EXPR, type, previous_mask->second, selected);
        string_word_byte(destination, i, next, words, at(call));
    }
    std::map<tree, tree> quantum_masks;
    for (const auto &entry : mask_values)
        quantum_masks[entry.first] =
            quantum_value_capture(call,
                                  entry.second,
                                  "__cket_concat_mask_",
                                  prefix,
                                  unsigned_type_for(TREE_TYPE(entry.first)));
    return make_quantum_value_program(
        call, words, defined, std::move(prefix), NULL_TREE, bit_masks, quantum_masks);
}
