// Search consumers use an existence flag and a relative byte position. These
// are pure expressions; neither a host address nor a quantum pointer escapes.
struct QuantumStringSearch {
    QuantumByteRange source;
    tree found = boolean_false_node;
    tree position = build_zero_cst(size_type_node);
    tree defined = boolean_true_node;
    uint64_t positions = 0;
};

std::map<tree, QuantumStringSearch> quantum_search_pointers;

bool expand_string_search(tree call, QuantumStringSearch &search, bool position) {
    call = strip(call);
    while (call && TREE_CODE(call) == SAVE_EXPR)
        call = strip(TREE_OPERAND(call, 0));
    auto saved = quantum_search_pointers.find(call);
    if (saved != quantum_search_pointers.end()) {
        search = saved->second;
        return true;
    }
    if (!call || TREE_CODE(call) != CALL_EXPR)
        return false;
    std::string operation = standard_string_search(get_callee_fndecl(call));
    if (operation.empty())
        return false;
    bool memory = operation == "memchr";
    bool set_search = operation == "strpbrk", substring = operation == "strstr";
    if (call_expr_nargs(call) != (memory ? 3 : 2))
        return false;

    QuantumByteRange &string = search.source;
    QuantumByteRange pattern;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), string);
    bool resolved_pattern =
        !(set_search || substring) || resolve_byte_range(CALL_EXPR_ARG(call, 1), pattern);
    if (!string.quantum && !pattern.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)) &&
        !has_quantum(CALL_EXPR_ARG(call, 1)))
        return false;
    auto reject = [call](const char *message) {
        error_at(at(call), "%s", message);
        return true;
    };
    if (!resolved || !resolved_pattern)
        return reject(
            !resolved && string.error ? string.error
            : !resolved_pattern && pattern.error
                ? pattern.error
                : "|c>: quantum search needs checked quantum bytes or known CPU object storage");
    uint64_t count = string.size() - string.offset;
    uint64_t pattern_count = set_search || substring ? pattern.size() - pattern.offset : 0;
    if (memory) {
        tree length = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!length || TREE_CODE(length) != INTEGER_CST || !tree_fits_uhwi_p(length))
            return reject("|c>: quantum memchr requires a constant checked byte count");
        uint64_t requested = tree_to_uhwi(length);
        if (requested > count)
            return reject("|c>: quantum memchr exceeds its checked storage bounds");
        count = requested;
    } else if (!count || ((set_search || substring) && !pattern_count))
        return reject("|c>: quantum string search needs a NUL inside each checked string view");
    if (count > 64 || pattern_count > 64)
        return reject("|c>: quantum search exceeds the 64-byte expression synthesis bound");
    search.positions = count;
    string.stop_at_zero = !memory;
    pattern.stop_at_zero = true;
    tree character = NULL_TREE;
    if (!set_search && !substring) {
        // memchr converts to unsigned char; strchr/strrchr convert to char.
        // Compare the resulting object byte so signed target chars keep their
        // implementation-defined integer conversion before the byte view.
        tree argument = expand_quantum_program(CALL_EXPR_ARG(call, 1));
        if (!quantum_pure_expression(argument))
            return reject(
                "|c>: quantum character search requires a pure character argument; sequence CPU effects before the call");
        character = fold_convert(
            integer_type_node,
            fold_convert(
                unsigned_char_type_node,
                fold_convert(memory ? unsigned_char_type_node : char_type_node, argument)));
    }
    location_t location = at(call);
    auto both = [location](tree a, tree b) {
        return fold_build2_loc(location, TRUTH_ANDIF_EXPR, boolean_type_node, a, b);
    };
    auto either = [location](tree a, tree b) {
        return fold_build2_loc(location, TRUTH_ORIF_EXPR, boolean_type_node, a, b);
    };
    auto nonzero = [location](tree byte) {
        return fold_build2_loc(
            location, NE_EXPR, boolean_type_node, byte, build_zero_cst(integer_type_node));
    };
    tree found = boolean_false_node;
    for (uint64_t i = count; i; --i) {
        tree byte = object_byte(string, i - 1, location), match;
        if (substring) {
            match = boolean_false_node;
            for (uint64_t j = pattern_count; j; --j) {
                tree wanted = object_byte(pattern, j - 1, location);
                tree empty =
                    fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, nonzero(wanted));
                if (j <= count - (i - 1)) {
                    tree actual = object_byte(string, i + j - 2, location);
                    tree equal =
                        fold_build2_loc(location, EQ_EXPR, boolean_type_node, actual, wanted);
                    match = either(empty, both(nonzero(actual), both(equal, match)));
                } else
                    match = empty;
            }
        } else if (set_search)
            match = byte_range_member(pattern, byte, location);
        else
            match = fold_build2_loc(location, EQ_EXPR, boolean_type_node, byte, character);
        tree continuation = memory ? found : both(nonzero(byte), found);
        if (set_search || substring)
            match = both(nonzero(byte), match);
        if (position) {
            // Reverse scanning makes first/last selection explicit. A NUL
            // prevents a later match from contributing, including for strrchr.
            bool last = operation == "strrchr";
            search.position =
                fold_build3_loc(location,
                                COND_EXPR,
                                size_type_node,
                                last ? continuation : match,
                                last ? search.position : build_int_cstu(size_type_node, i - 1),
                                last ? build_int_cstu(size_type_node, i - 1) : search.position);
        }
        found = either(match, continuation);
    }
    if (substring) {
        tree first = object_byte(pattern, 0, location);
        tree empty = fold_build1_loc(location, TRUTH_NOT_EXPR, boolean_type_node, nonzero(first));
        found = either(empty, found);
        if (position)
            search.position = fold_build3_loc(location,
                                              COND_EXPR,
                                              size_type_node,
                                              empty,
                                              build_zero_cst(size_type_node),
                                              search.position);
    }
    if (!memory) {
        search.defined = byte_range_terminated(string, location);
        if (set_search || substring)
            search.defined = both(search.defined, byte_range_terminated(pattern, location));
    }
    search.found = found;
    return true;
}

tree string_search_domain(const QuantumStringSearch &search,
                          tree expression,
                          location_t location,
                          bool require_match = false) {
    tree defined = search.defined;
    if (require_match)
        defined =
            fold_build2_loc(location, TRUTH_ANDIF_EXPR, boolean_type_node, defined, search.found);
    tree type = TREE_TYPE(expression);
    return fold_build3_loc(
        location,
        COND_EXPR,
        type,
        defined,
        expression,
        fold_convert(type, missing_string_terminator(integer_type_node, location)));
}

tree expand_string_search_relation(tree value) {
    enum tree_code relation = TREE_CODE(value);
    if (relation != EQ_EXPR && relation != NE_EXPR)
        return NULL_TREE;
    tree call = strip(TREE_OPERAND(value, 0)), zero = strip(TREE_OPERAND(value, 1));
    if (call && integer_zerop(call))
        std::swap(call, zero);
    if (!zero || !integer_zerop(zero))
        return NULL_TREE;
    QuantumStringSearch search;
    if (!expand_string_search(call, search, false))
        return NULL_TREE;
    tree found = string_search_domain(search, search.found, at(value));
    if (relation == EQ_EXPR)
        found = fold_build1_loc(at(value), TRUTH_NOT_EXPR, boolean_type_node, found);
    return fold_convert(TREE_TYPE(value), found);
}

#include "quantum_search_positions.inc"
#include "quantum_search_pointers.inc"
