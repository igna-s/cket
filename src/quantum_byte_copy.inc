// Object-byte copies share the checked string-write machinery, without NUL
// semantics. Schedule each source read before any other word overwrites it.
// A self-dependent partial-word write retains its old owner for the inverse.
tree expand_byte_copy(tree call, bool move) {
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
        return reject("|c>: quantum byte copy cannot write unmeasured data into CPU storage");
    if (!resolved_destination || !resolved_source)
        return reject(
            !resolved_destination && destination.error ? destination.error
            : !resolved_source && source.error
                ? source.error
                : "|c>: quantum byte copy needs checked integer destination bytes and known quantum/CPU source bytes");
    if (TYPE_READONLY(destination.element_type))
        return reject("|c>: quantum byte copy requires mutable destination storage");
    tree length = strip(fold(CALL_EXPR_ARG(call, 2)));
    if (!length || TREE_CODE(length) != INTEGER_CST || !tree_fits_uhwi_p(length))
        return expand_dynamic_memory(call, move, false);
    uint64_t count = tree_to_uhwi(length);
    if (count > destination.size() - destination.offset || count > source.size() - source.offset)
        return reject("|c>: quantum byte copy exceeds its checked storage bounds");
    if (count > 64)
        return reject(
            "|c>: quantum object-byte copy exceeds the 64-byte expression synthesis bound");
    if (move && source.quantum && destination.elements == source.elements &&
        destination.stride == source.stride && destination.offset == source.offset)
        return make_quantum_value_program(call, {}, boolean_true_node);
    std::map<tree, uint64_t> source_masks;
    if (source.quantum)
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t position = source.offset + i;
            source_masks[source.elements[(size_t)(position / source.stride)]] |=
                string_byte_mask(source, position);
        }
    std::vector<std::pair<tree, tree>> words;
    std::map<tree, tree> bit_masks;
    std::map<tree, std::set<tree>> inputs;
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t position = destination.offset + i;
        tree target = destination.elements[(size_t)(position / destination.stride)];
        if (readonly_quantum_borrows.count(target))
            return reject("|c>: quantum byte copy cannot write through a read-only borrow");
        auto overlap = source_masks.find(target);
        if (!move && overlap != source_masks.end() &&
            (overlap->second & string_byte_mask(destination, position)))
            return reject("|c>: overlapping quantum memcpy has undefined C behavior; use memmove");
        string_word_byte(destination, i, object_byte(source, i, at(call)), words, at(call));
        uint64_t mask = string_byte_mask(destination, position);
        auto prior = bit_masks.find(target);
        if (prior != bit_masks.end())
            mask |= tree_to_uhwi(prior->second);
        bit_masks[target] = build_int_cstu(long_long_unsigned_type_node, mask);
        if (source.quantum)
            inputs[target].insert(source.elements[(size_t)((source.offset + i) / source.stride)]);
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
        return reject("|c>: cyclic quantum byte-copy dependencies need a retained snapshot");
    return make_quantum_value_program(call, ordered, boolean_true_node, {}, NULL_TREE, bit_masks);
}
