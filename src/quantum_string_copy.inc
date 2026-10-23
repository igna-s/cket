// String copies become ordinary typed quantum updates. Existing overwrite
// lowering retains old owners when necessary; no unknown input is erased.
tree expand_string_concat(tree call);

uint64_t string_byte_mask(const QuantumByteRange &range, uint64_t position) {
    unsigned byte = (unsigned)(position % range.stride);
    unsigned shift = 8 * (BYTES_BIG_ENDIAN ? (unsigned)range.stride - byte - 1 : byte);
    return uint64_t{255} << shift;
}

void string_word_byte(QuantumByteRange &destination,
                      uint64_t index,
                      tree byte,
                      std::vector<std::pair<tree, tree>> &words,
                      location_t location) {
    uint64_t position = destination.offset + index;
    tree target = destination.elements[(size_t)(position / destination.stride)];
    tree type = unsigned_type_for(TREE_TYPE(target));
    if (words.empty() || words.back().first != target)
        words.emplace_back(target, fold_convert(type, target));
    uint64_t full =
        TYPE_PRECISION(type) == 64 ? UINT64_MAX : (uint64_t{1} << TYPE_PRECISION(type)) - 1;
    tree kept =
        fold_build2_loc(location,
                        BIT_AND_EXPR,
                        type,
                        words.back().second,
                        build_int_cstu(type, full ^ string_byte_mask(destination, position)));
    tree part = fold_convert(type, fold_convert(unsigned_char_type_node, byte));
    unsigned shift = 8 * (BYTES_BIG_ENDIAN ? (unsigned)destination.stride -
                                                 (unsigned)(position % destination.stride) - 1
                                           : (unsigned)(position % destination.stride));
    if (shift)
        part = fold_build2_loc(
            location, LSHIFT_EXPR, type, part, build_int_cstu(integer_type_node, shift));
    words.back().second = fold_build2_loc(location, BIT_IOR_EXPR, type, kept, part);
}

tree string_write_marker(tree call,
                         const std::vector<std::pair<tree, tree>> &words,
                         tree defined,
                         std::vector<tree> prefix = {},
                         tree result_value = NULL_TREE) {
    QuantumStringWrite operation;
    operation.prefix = std::move(prefix);
    operation.result = result_value;
    std::vector<tree> arguments;
    for (tree step : operation.prefix)
        if (TREE_CODE(step) != DECL_EXPR)
            arguments.push_back(step);
    for (const auto &word : words) {
        tree type = TREE_TYPE(word.first), next = fold_convert(type, word.second);
        if (operation.updates.empty())
            next = fold_build3_loc(at(call),
                                   COND_EXPR,
                                   type,
                                   defined,
                                   next,
                                   missing_string_terminator(type, at(call)));
        tree update = build2_loc(at(call), MODIFY_EXPR, type, word.first, next);
        operation.updates.push_back(update);
        arguments.push_back(update);
    }
    if (result_value)
        arguments.push_back(result_value);
    tree marker =
        build_fn_decl("__cket_string_copy", build_function_type(TREE_TYPE(call), NULL_TREE));
    tree result =
        build_call_expr_loc_array(at(call), marker, (int)arguments.size(), arguments.data());
    quantum_string_copies.emplace(result, std::move(operation));
    return result;
}

tree expand_string_copy(tree call) {
    tree fn = get_callee_fndecl(call);
    if (fndecl_built_in_p(fn, BUILT_IN_STRCAT) || fndecl_built_in_p(fn, BUILT_IN_STRNCAT) ||
        name_of(fn) == "strcat" || name_of(fn) == "__cket_strcat" || name_of(fn) == "strncat" ||
        name_of(fn) == "__cket_strncat")
        return expand_string_concat(call);
    bool bounded = fndecl_built_in_p(fn, BUILT_IN_STRNCPY) || name_of(fn) == "strncpy" ||
                   name_of(fn) == "__cket_strncpy";
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
        return reject("|c>: quantum string copy cannot write an unmeasured value into CPU storage");
    if (!resolved_destination || !resolved_source)
        return reject(
            !resolved_destination && destination.error ? destination.error
            : !resolved_source && source.error
                ? source.error
                : "|c>: quantum string copy needs checked integer destination bytes and checked quantum/CPU source bytes");
    if (TYPE_READONLY(destination.element_type))
        return reject("|c>: quantum string copy requires mutable destination storage");
    uint64_t available = destination.size() - destination.offset;
    uint64_t source_count = source.size() - source.offset;
    uint64_t count = std::min(available, source_count);
    if (bounded) {
        tree length = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!length || TREE_CODE(length) != INTEGER_CST || !tree_fits_uhwi_p(length))
            return reject("|c>: quantum strncpy requires a constant checked byte count");
        count = tree_to_uhwi(length);
        if (count > available)
            return reject("|c>: quantum strncpy exceeds its checked destination bounds");
    } else if (!count)
        return reject("|c>: quantum strcpy needs a source NUL that fits the checked destination");
    if (count > 64 || (!bounded && source_count > 64))
        return reject("|c>: quantum string copy exceeds the 64-byte expression synthesis bound");
    uint64_t reads = std::min(count, source_count);
    if (count && !reads)
        return reject("|c>: quantum string copy has no readable source byte");
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
            return reject("|c>: quantum string copy cannot write through a read-only borrow");
        auto overlap = source_masks.find(target);
        if (overlap != source_masks.end() &&
            (overlap->second & string_byte_mask(destination, position)))
            return reject(
                "|c>: overlapping strcpy/strncpy byte ranges are unsupported; overlapping copies have undefined C behavior");
    }
    source.stop_at_zero = true;
    tree running = boolean_true_node;
    tree terminated = boolean_false_node;
    std::vector<std::pair<tree, tree>> words;
    for (uint64_t i = 0; i < count; ++i) {
        tree byte =
            i < reads ? object_byte(source, i, at(call)) : build_zero_cst(integer_type_node);
        tree zero = fold_build2_loc(
            at(call), EQ_EXPR, boolean_type_node, byte, build_zero_cst(integer_type_node));
        if (i < reads)
            terminated =
                fold_build2_loc(at(call), TRUTH_ORIF_EXPR, boolean_type_node, terminated, zero);
        tree tail =
            bounded ? build_zero_cst(integer_type_node) : object_byte(destination, i, at(call));
        tree selected =
            fold_build3_loc(at(call), COND_EXPR, integer_type_node, running, byte, tail);
        running =
            fold_build2_loc(at(call),
                            TRUTH_ANDIF_EXPR,
                            boolean_type_node,
                            running,
                            fold_build1_loc(at(call), TRUTH_NOT_EXPR, boolean_type_node, zero));
        string_word_byte(destination, i, selected, words, at(call));
    }
    tree defined = !bounded || count > source_count ? terminated : boolean_true_node;
    return string_write_marker(call, words, defined);
}
