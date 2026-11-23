// Checked object-byte views. Comparisons read integer expressions; writes
// use reversible updates with retained information when needed. No CPU
// load or store accesses a quantum placeholder's object representation.
struct QuantumByteRange {
    std::vector<tree> elements;
    tree element_type = NULL_TREE;
    uint64_t stride = 0, offset = 0;
    bool quantum = false;
    const char *error = nullptr;
    tree cpu_address = NULL_TREE;
    tree cpu_object = NULL_TREE;
    tree cpu_guard = NULL_TREE;
    uint64_t cpu_size = 0;
    std::map<uint64_t, tree> cpu_bytes;
    bool stop_at_zero = false;

    uint64_t size() const {
        return cpu_address ? cpu_size : elements.size() * stride;
    }
};

bool standard_memory_compare(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return false;
    return fndecl_built_in_p(fn, BUILT_IN_MEMCMP) || fndecl_built_in_p(fn, BUILT_IN_STRNCMP) ||
           fndecl_built_in_p(fn, BUILT_IN_STRCMP) ||
           (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn)) &&
            (name_of(fn) == "memcmp" || name_of(fn) == "__cket_memcmp" ||
             name_of(fn) == "strncmp" || name_of(fn) == "__cket_strncmp" ||
             name_of(fn) == "strcmp" || name_of(fn) == "__cket_strcmp" ||
             name_of(fn) == "strcoll" || name_of(fn) == "__cket_strcoll"));
}

bool standard_string_length(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return false;
    return fndecl_built_in_p(fn, BUILT_IN_STRLEN) ||
           (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn)) &&
            (name_of(fn) == "strlen" || name_of(fn) == "__cket_strlen"));
}

bool standard_memory_set(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return false;
    return fndecl_built_in_p(fn, BUILT_IN_MEMSET) ||
           (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn)) &&
            (name_of(fn) == "memset" || name_of(fn) == "__cket_memset"));
}

bool standard_string_span(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return false;
    return fndecl_built_in_p(fn, BUILT_IN_STRSPN) || fndecl_built_in_p(fn, BUILT_IN_STRCSPN) ||
           (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn)) &&
            (name_of(fn) == "strspn" || name_of(fn) == "__cket_strspn" ||
             name_of(fn) == "strcspn" || name_of(fn) == "__cket_strcspn"));
}

bool standard_string_copy(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return false;
    return fndecl_built_in_p(fn, BUILT_IN_STRCPY) || fndecl_built_in_p(fn, BUILT_IN_STRNCPY) ||
           fndecl_built_in_p(fn, BUILT_IN_STRCAT) || fndecl_built_in_p(fn, BUILT_IN_STRNCAT) ||
           (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn)) &&
            (name_of(fn) == "strcpy" || name_of(fn) == "__cket_strcpy" ||
             name_of(fn) == "strncpy" || name_of(fn) == "__cket_strncpy" ||
             name_of(fn) == "strcat" || name_of(fn) == "__cket_strcat" ||
             name_of(fn) == "strncat" || name_of(fn) == "__cket_strncat"));
}

std::string standard_string_search(tree fn) {
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return "";
    if (fndecl_built_in_p(fn, BUILT_IN_MEMCHR))
        return "memchr";
    if (fndecl_built_in_p(fn, BUILT_IN_STRCHR))
        return "strchr";
    if (fndecl_built_in_p(fn, BUILT_IN_STRRCHR))
        return "strrchr";
    if (fndecl_built_in_p(fn, BUILT_IN_STRPBRK))
        return "strpbrk";
    if (fndecl_built_in_p(fn, BUILT_IN_STRSTR))
        return "strstr";
    if (!DECL_EXTERNAL(fn) || !TREE_PUBLIC(fn) || !in_system_header_at(DECL_SOURCE_LOCATION(fn)))
        return "";
    std::string name = name_of(fn);
    if (name.compare(0, 7, "__cket_") == 0)
        name.erase(0, 7);
    return name == "memchr" || name == "strchr" || name == "strrchr" || name == "strpbrk" ||
                   name == "strstr"
               ? name
               : "";
}

bool byte_range_layout(QuantumByteRange &range) {
    tree type = range.element_type, size = TYPE_SIZE_UNIT(type);
    if (BITS_PER_UNIT != 8 || !INTEGRAL_TYPE_P(type) || !size || !tree_fits_uhwi_p(size) ||
        !tree_to_uhwi(size) || tree_to_uhwi(size) > 8 ||
        TYPE_PRECISION(type) != tree_to_uhwi(size) * BITS_PER_UNIT) {
        range.error =
            "|c>: quantum byte comparison requires integer object bytes without padding; bool, floating and aggregate layouts need further implementation";
        return false;
    }
    range.stride = tree_to_uhwi(size);
    return true;
}

bool byte_snapshot_has_effects(tree type, std::set<tree> &visited) {
    if (TYPE_VOLATILE(type) || TYPE_ATOMIC(type))
        return true;
    if (!visited.insert(type).second)
        return false;
    if (TREE_CODE(type) == ARRAY_TYPE)
        return byte_snapshot_has_effects(TREE_TYPE(type), visited);
    if (TREE_CODE(type) == RECORD_TYPE || TREE_CODE(type) == UNION_TYPE)
        for (tree field = TYPE_FIELDS(type); field; field = DECL_CHAIN(field))
            if (TREE_CODE(field) == FIELD_DECL &&
                byte_snapshot_has_effects(TREE_TYPE(field), visited))
                return true;
    return false;
}

bool constant_byte_range(tree storage, uint64_t index, QuantumByteRange &range) {
    tree initial = NULL_TREE, type = TREE_TYPE(storage);
    bool array = TREE_CODE(type) == ARRAY_TYPE;
    uint64_t count = 1;
    if (TREE_CODE(storage) == STRING_CST)
        initial = storage;
    else if (TREE_CODE(storage) == VAR_DECL &&
             (TREE_READONLY(storage) || (array && TYPE_READONLY(TREE_TYPE(type)))))
        initial = DECL_INITIAL(storage);
    if (!initial)
        return false;
    if (array) {
        tree domain = TYPE_DOMAIN(type), maximum = domain ? TYPE_MAX_VALUE(domain) : NULL_TREE;
        if (!maximum || !tree_fits_uhwi_p(maximum) || tree_to_uhwi(maximum) >= 4096)
            return false;
        count = tree_to_uhwi(maximum) + 1;
        type = TREE_TYPE(type);
    } else if (index)
        return false;
    range.element_type = type;
    if (!byte_range_layout(range) || index > count)
        return false;
    if (array && TREE_CODE(initial) != STRING_CST && TREE_CODE(initial) != CONSTRUCTOR)
        return false;
    for (uint64_t i = 0; i < count; ++i) {
        tree element = initial;
        if (array && TREE_CODE(initial) == STRING_CST) {
            if (range.stride != 1)
                return false;
            element = build_int_cstu(type,
                                     i < (uint64_t)TREE_STRING_LENGTH(initial)
                                         ? (unsigned char)TREE_STRING_POINTER(initial)[i]
                                         : 0);
        } else if (array) {
            element = constructor_element(initial, NULL_TREE, i, true);
            if (!element)
                element = build_zero_cst(type);
        }
        element = strip(fold(element));
        if (!element || TREE_CODE(element) != INTEGER_CST)
            return false;
        range.elements.push_back(fold_convert(type, element));
    }
    range.offset = index * range.stride;
    range.cpu_object = storage;
    return true;
}

bool resolve_byte_range(tree value, QuantumByteRange &range) {
    value = strip(value);
    // Pure helper arguments may already share a once-only address evaluation.
    while (value && TREE_CODE(value) == SAVE_EXPR)
        value = strip(TREE_OPERAND(value, 0));
    if (!value)
        return false;
    auto alias = quantum_pointer_aliases.find(value);
    if (alias != quantum_pointer_aliases.end()) {
        if (!resolve_byte_range(alias->second, range))
            return false;
        if (TYPE_READONLY(TREE_TYPE(TREE_TYPE(value))))
            range.element_type = build_qualified_type(
                range.element_type, TYPE_QUALS(range.element_type) | TYPE_QUAL_CONST);
        return true;
    }
    if (TREE_CODE(value) == POINTER_PLUS_EXPR) {
        if (!resolve_byte_range(TREE_OPERAND(value, 0), range))
            return false;
        tree bytes = strip(fold(TREE_OPERAND(value, 1)));
        if (!bytes || TREE_CODE(bytes) != INTEGER_CST || TYPE_PRECISION(TREE_TYPE(bytes)) > 64) {
            range.error =
                "|c>: quantum byte comparison byte offsets currently require a constant checked address";
            return false;
        }
        int64_t delta = static_cast<int64_t>(TREE_INT_CST_LOW(bytes));
        if ((delta < 0 && (uint64_t)(-(delta + 1)) + 1 > range.offset) ||
            (delta >= 0 && (uint64_t)delta > range.size() - range.offset)) {
            range.error =
                "|c>: quantum byte comparison byte view is outside its checked storage bounds";
            return false;
        }
        range.offset = delta < 0 ? range.offset - ((uint64_t)(-(delta + 1)) + 1)
                                 : range.offset + (uint64_t)delta;
        return true;
    }
    QuantumReference reference;
    if (resolve_quantum_reference(value, reference)) {
        range.quantum = true;
        range.elements = reference.elements;
        if (!reference.product_root) {
            range.element_type = reference.element_type;
            if (!byte_range_layout(range))
                return false;
            range.offset = (uint64_t)reference.offset * range.stride;
            return range.offset <= range.size();
        }
        if (reference.elements.empty() || !reference.leaves_per_element) {
            range.error =
                "|c>: quantum byte comparison requires integer object bytes without padding; bool, floating and aggregate layouts need further implementation";
            return false;
        }
        tree leaf_type = TYPE_MAIN_VARIANT(TREE_TYPE(reference.elements.front()));
        for (tree elem : reference.elements)
            if (TYPE_MAIN_VARIANT(TREE_TYPE(elem)) != leaf_type) {
                range.error =
                    "|c>: quantum byte comparison requires integer object bytes without padding; bool, floating and aggregate layouts need further implementation";
                return false;
            }
        bool ro =
            TYPE_READONLY(reference.element_type) || (TREE_CODE(TREE_TYPE(value)) == POINTER_TYPE &&
                                                      TYPE_READONLY(TREE_TYPE(TREE_TYPE(value))));
        if (ro)
            leaf_type = build_qualified_type(leaf_type, TYPE_QUALS(leaf_type) | TYPE_QUAL_CONST);
        range.element_type = leaf_type;
        if (!byte_range_layout(range))
            return false;
        tree elem_size = TYPE_SIZE_UNIT(reference.element_type);
        if (!elem_size || !tree_fits_uhwi_p(elem_size) ||
            tree_to_uhwi(elem_size) != reference.leaves_per_element * range.stride) {
            range.error =
                "|c>: quantum byte comparison requires integer object bytes without padding; bool, floating and aggregate layouts need further implementation";
            return false;
        }
        range.offset = (uint64_t)reference.offset * reference.leaves_per_element * range.stride;
        return range.offset <= range.size();
    }
    // Only addresses with known CPU storage enter this path. Unknown pointers
    // cannot be assumed to point to CPU bytes rather than quantum placeholders.
    if (TREE_CODE(value) != ADDR_EXPR || has_quantum(value))
        return false;
    tree storage = strip(TREE_OPERAND(value, 0));
    uint64_t index = 0;
    if (TREE_CODE(storage) == ARRAY_REF) {
        tree subscript = strip(fold(TREE_OPERAND(storage, 1)));
        if (!subscript || TREE_CODE(subscript) != INTEGER_CST || !tree_fits_uhwi_p(subscript))
            return false;
        index = tree_to_uhwi(subscript);
        storage = strip(TREE_OPERAND(storage, 0));
    }
    if (TREE_CODE(storage) != VAR_DECL && TREE_CODE(storage) != PARM_DECL &&
        TREE_CODE(storage) != STRING_CST)
        return false;
    if (quantum_products.count(storage) || quantum_references.count(storage) ||
        (DECL_P(storage) && lookup_attribute("cket_quant", DECL_ATTRIBUTES(storage)))) {
        range.quantum = true;
        range.error =
            "|c>: quantum byte comparison requires an implemented quantum object-byte layout";
        return false;
    }
    tree type = TREE_TYPE(storage);
    std::set<tree> visited;
    if (TREE_THIS_VOLATILE(storage) || byte_snapshot_has_effects(type, visited)) {
        range.error =
            "|c>: quantum byte comparison needs a separate volatile/atomic CPU access contract";
        return false;
    }
    QuantumByteRange constant;
    if (constant_byte_range(storage, index, constant)) {
        range = std::move(constant);
        return true;
    }
    tree size = TYPE_SIZE_UNIT(type);
    if (BITS_PER_UNIT != 8 || !size || !tree_fits_uhwi_p(size))
        return false;
    uint64_t extent = tree_to_uhwi(size), offset = 0;
    if (TREE_CODE(type) == ARRAY_TYPE) {
        tree element_size = TYPE_SIZE_UNIT(TREE_TYPE(type));
        if (!element_size || !tree_fits_uhwi_p(element_size) || !tree_to_uhwi(element_size))
            return false;
        uint64_t stride = tree_to_uhwi(element_size);
        if (index > extent / stride) {
            range.error =
                "|c>: quantum byte comparison CPU byte view is outside its checked storage bounds";
            return false;
        }
        offset = index * stride;
    } else if (index)
        return false;
    // Read the actual current object representation, including floating or
    // aggregate CPU storage, through C's unsigned-character aliasing rule.
    // No typed scalar load, padding interpretation or quantum load is used.
    range.cpu_address =
        fold_convert(build_pointer_type(unsigned_char_type_node), build_fold_addr_expr(storage));
    range.cpu_object = storage;
    range.cpu_size = extent;
    range.offset = offset;
    return true;
}

tree object_byte(QuantumByteRange &range, uint64_t index, location_t location) {
    index += range.offset;
    if (range.cpu_address) {
        auto found = range.cpu_bytes.find(index);
        if (found == range.cpu_bytes.end()) {
            tree previous = NULL_TREE;
            if (range.stop_at_zero && index > range.offset) {
                // Either an unequal quantum byte or an equal NUL ends the
                // comparison at a classical NUL. Its CPU tail must not be read,
                // even when it is within the declared object's byte extent.
                previous = object_byte(range, index - range.offset - 1, location);
            }
            tree address = fold_build2_loc(location,
                                           POINTER_PLUS_EXPR,
                                           TREE_TYPE(range.cpu_address),
                                           range.cpu_address,
                                           size_int(index));
            if (!read_byte_fn) {
                tree pointer =
                    build_pointer_type(build_qualified_type(void_type_node, TYPE_QUAL_CONST));
                read_byte_fn = build_fn_decl(
                    "__cket_read_byte",
                    build_function_type_list(
                        unsigned_char_type_node, pointer, unsigned_char_type_node, NULL_TREE));
                TREE_PUBLIC(read_byte_fn) = DECL_EXTERNAL(read_byte_fn) =
                    DECL_ARTIFICIAL(read_byte_fn) = 1;
            }
            tree enabled = previous ? fold_build2_loc(location,
                                                      NE_EXPR,
                                                      boolean_type_node,
                                                      previous,
                                                      build_zero_cst(integer_type_node))
                                    : boolean_true_node;
            if (range.cpu_guard)
                enabled = fold_build2_loc(
                    location, TRUTH_ANDIF_EXPR, boolean_type_node, enabled, range.cpu_guard);
            // A conditional load in this caller can be speculated by GCC from
            // the known enclosing object size. The private runtime query keeps
            // disabled addresses unread, including protected pages after NUL.
            tree pointer_type = TREE_VALUE(TYPE_ARG_TYPES(TREE_TYPE(read_byte_fn)));
            tree value =
                save_expr(build_call_expr_loc(location,
                                              read_byte_fn,
                                              2,
                                              fold_convert(pointer_type, address),
                                              fold_convert(unsigned_char_type_node, enabled)));
            cpu_byte_saves.insert(value);
            found = range.cpu_bytes.emplace(index, value).first;
        }
        return fold_convert(integer_type_node, found->second);
    }
    tree element = range.elements[(size_t)(index / range.stride)];
    tree unsigned_type = unsigned_type_for(range.element_type);
    tree value = fold_convert(unsigned_type, element);
    unsigned byte = (unsigned)(index % range.stride);
    unsigned shift = 8 * (BYTES_BIG_ENDIAN ? (unsigned)range.stride - byte - 1 : byte);
    if (shift)
        value = fold_build2_loc(
            location, RSHIFT_EXPR, unsigned_type, value, build_int_cst(integer_type_node, shift));
    return fold_convert(integer_type_node, fold_convert(unsigned_char_type_node, value));
}

// An unreachable division-by-zero expression encodes a domain failure in the
// existing typed IR. It is never executed on the CPU. The backend checks this
// leaf only if the selected string has no NUL inside its checked object view.
tree missing_string_terminator(tree type, location_t location) {
    return build2_loc(location, TRUNC_DIV_EXPR, type, build_one_cst(type), build_zero_cst(type));
}

tree byte_range_terminated(QuantumByteRange &range, location_t location) {
    tree terminated = boolean_false_node;
    for (uint64_t i = range.size() - range.offset; i; --i) {
        tree zero = fold_build2_loc(location,
                                    EQ_EXPR,
                                    boolean_type_node,
                                    object_byte(range, i - 1, location),
                                    build_zero_cst(integer_type_node));
        terminated =
            fold_build2_loc(location, TRUTH_ORIF_EXPR, boolean_type_node, zero, terminated);
    }
    if (range.cpu_address) {
        // The combined predicate is a once-only read of the checked CPU
        // snapshot, just like its individual byte captures. Keeping its SAVE
        // identity also preserves the exact predicate for a saved inverse.
        terminated = save_expr(terminated);
        cpu_byte_saves.insert(terminated);
    }
    return terminated;
}

tree expand_string_length(tree call) {
    if (call_expr_nargs(call) != 1)
        return NULL_TREE;
    QuantumByteRange range;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), range);
    if (!range.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)))
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!resolved)
        return reject(range.error
                          ? range.error
                          : "|c>: quantum strlen requires checked integer object-byte storage");
    uint64_t count = range.size() - range.offset;
    if (!count)
        return reject("|c>: quantum strlen needs a NUL inside its checked storage bounds");
    if (count > 64)
        return reject("|c>: quantum strlen exceeds the 64-byte expression synthesis bound");
    range.stop_at_zero = true;
    tree type = TREE_TYPE(call);
    tree result = missing_string_terminator(type, at(call));
    for (uint64_t i = count; i; --i) {
        tree zero = fold_build2_loc(at(call),
                                    EQ_EXPR,
                                    boolean_type_node,
                                    object_byte(range, i - 1, at(call)),
                                    build_zero_cst(integer_type_node));
        result =
            fold_build3_loc(at(call), COND_EXPR, type, zero, build_int_cstu(type, i - 1), result);
    }
    return result;
}

#include "quantum_string_span.inc"
#include "quantum_string_search.inc"
#include "quantum_string_copy.inc"
#include "quantum_string_concat.inc"

tree expand_memory_compare(tree call, enum tree_code relation = ERROR_MARK) {
    tree fn = get_callee_fndecl(call);
    bool unbounded = fn && (fndecl_built_in_p(fn, BUILT_IN_STRCMP) || name_of(fn) == "strcmp" ||
                            name_of(fn) == "__cket_strcmp" || name_of(fn) == "strcoll" ||
                            name_of(fn) == "__cket_strcoll");
    if (call_expr_nargs(call) != (unbounded ? 2 : 3))
        return NULL_TREE;
    bool stop_at_zero =
        unbounded || (fn && (fndecl_built_in_p(fn, BUILT_IN_STRNCMP) || name_of(fn) == "strncmp" ||
                             name_of(fn) == "__cket_strncmp"));
    QuantumByteRange left, right;
    bool a = resolve_byte_range(CALL_EXPR_ARG(call, 0), left);
    bool b = resolve_byte_range(CALL_EXPR_ARG(call, 1), right);
    if (!left.quantum && !right.quantum && (unbounded || !has_quantum(CALL_EXPR_ARG(call, 2))))
        return NULL_TREE;
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!a || !b)
        return reject(
            !a && left.error ? left.error
            : !b && right.error
                ? right.error
                : "|c>: quantum byte comparison requires checked quantum storage or known CPU object storage; general pointers and dynamic layouts remain pending");
    uint64_t count;
    tree quantum_bytes = NULL_TREE;
    if (unbounded) {
        uint64_t left_count = left.size() - left.offset, right_count = right.size() - right.offset;
        if (!left_count || !right_count)
            return reject("|c>: quantum strcmp needs a NUL inside both checked storage views");
        if (left_count > 64 || right_count > 64)
            return reject("|c>: quantum strcmp exceeds the 64-byte expression synthesis bound");
        count = std::min(left_count, right_count);
    } else if (has_quantum(CALL_EXPR_ARG(call, 2))) {
        quantum_bytes = expand_quantum_program(CALL_EXPR_ARG(call, 2));
        if (!quantum_bytes || !INTEGRAL_TYPE_P(TREE_TYPE(quantum_bytes)) ||
            TYPE_PRECISION(TREE_TYPE(quantum_bytes)) > 64 ||
            !quantum_pure_expression(quantum_bytes))
            return reject(
                "|c>: quantum byte comparison requires a pure integer byte count of at most 64 bits");
        count = std::min(left.size() - left.offset, right.size() - right.offset);
    } else {
        tree bytes = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!bytes || TREE_CODE(bytes) != INTEGER_CST || !tree_fits_uhwi_p(bytes))
            return reject(
                "|c>: quantum byte comparison currently requires a constant byte count; bounded dynamic extents remain pending");
        count = tree_to_uhwi(bytes);
    }
    if (count > left.size() - left.offset || count > right.size() - right.offset)
        return reject("|c>: quantum byte comparison exceeds its checked storage bounds");
    if (count > 64)
        return reject(
            "|c>: quantum byte comparison exceeds the 64-byte expression synthesis bound");
    if (unbounded && left.quantum && right.quantum && left.elements == right.elements &&
        left.stride == right.stride && left.offset == right.offset) {
        // Equal views compare equal on every defined string input. This is a
        // constant identity, so it needs no read, measurement or workspace.
        // Like ordinary C folding, it makes no diagnostic promise for a call
        // whose input violates strcmp's string precondition.
        return build_int_cst(relation == ERROR_MARK ? TREE_TYPE(call) : boolean_type_node,
                             relation == EQ_EXPR || relation == LE_EXPR || relation == GE_EXPR);
    }
    left.stop_at_zero = right.stop_at_zero = stop_at_zero;
    // C11 specifies the sign of the first differing unsigned bytes. This target
    // extension returns their exact difference, within [-255,255]. All inputs
    // are preserved; the complete computation is |a,b,0> -> |a,b,memcmp(a,b,n)>.
    bool predicate = relation != ERROR_MARK;
    tree result_type = predicate ? boolean_type_node : integer_type_node;
    tree result = build_int_cst(result_type,
                                relation == LE_EXPR || relation == GE_EXPR || relation == EQ_EXPR);
    tree equal_result = result;
    for (uint64_t i = count; i; --i) {
        tree active_byte = NULL_TREE;
        if (quantum_bytes)
            active_byte = fold_build2_loc(at(call),
                                          GT_EXPR,
                                          boolean_type_node,
                                          fold_convert(long_long_unsigned_type_node, quantum_bytes),
                                          build_int_cstu(long_long_unsigned_type_node, i - 1));
        tree x = object_byte(left, i - 1, at(call));
        tree y = object_byte(right, i - 1, at(call));
        if (!quantum_bytes && i == count && relation != EQ_EXPR && relation != NE_EXPR) {
            result =
                fold_build2_loc(at(call), predicate ? relation : MINUS_EXPR, result_type, x, y);
            continue;
        }
        // strncmp stops at an equal NUL; unequal bytes still select their
        // own relation/difference before this suffix can contribute.
        if (stop_at_zero && (i < count || quantum_bytes)) {
            tree zero = fold_build2_loc(
                at(call), EQ_EXPR, boolean_type_node, x, build_zero_cst(integer_type_node));
            result = fold_build3_loc(at(call), COND_EXPR, result_type, zero, equal_result, result);
        }
        tree different = fold_build2_loc(at(call), NE_EXPR, boolean_type_node, x, y);
        if (relation == EQ_EXPR || relation == NE_EXPR) {
            tree comparison =
                relation == EQ_EXPR
                    ? fold_build1_loc(at(call), TRUTH_NOT_EXPR, boolean_type_node, different)
                    : different;
            result = fold_build2_loc(at(call),
                                     relation == EQ_EXPR ? TRUTH_ANDIF_EXPR : TRUTH_ORIF_EXPR,
                                     boolean_type_node,
                                     comparison,
                                     result);
        } else {
            tree difference =
                predicate ? fold_build2_loc(at(call),
                                            relation == LT_EXPR || relation == LE_EXPR ? LT_EXPR
                                                                                       : GT_EXPR,
                                            boolean_type_node,
                                            x,
                                            y)
                          : fold_build2_loc(at(call), MINUS_EXPR, integer_type_node, x, y);
            result =
                fold_build3_loc(at(call), COND_EXPR, result_type, different, difference, result);
        }
        if (active_byte)
            result = fold_build3_loc(
                at(call), COND_EXPR, result_type, active_byte, result, equal_result);
    }
    if (unbounded) {
        tree terminated = fold_build2_loc(at(call),
                                          TRUTH_ANDIF_EXPR,
                                          boolean_type_node,
                                          byte_range_terminated(left, at(call)),
                                          byte_range_terminated(right, at(call)));
        // Use an int domain leaf before Boolean conversion: bool(1/0) must not
        // become an operation whose arithmetic type is the one-bit predicate.
        tree invalid =
            fold_convert(result_type, missing_string_terminator(integer_type_node, at(call)));
        result = fold_build3_loc(at(call), COND_EXPR, result_type, terminated, result, invalid);
    } else if (quantum_bytes) {
        tree q_type = TREE_TYPE(quantum_bytes);
        tree in_bounds = fold_build2_loc(at(call),
                                         LE_EXPR,
                                         boolean_type_node,
                                         fold_convert(long_long_unsigned_type_node, quantum_bytes),
                                         build_int_cstu(long_long_unsigned_type_node, count));
        if (!TYPE_UNSIGNED(q_type))
            in_bounds = fold_build2_loc(
                at(call),
                TRUTH_ANDIF_EXPR,
                boolean_type_node,
                fold_build2_loc(
                    at(call), GE_EXPR, boolean_type_node, quantum_bytes, build_zero_cst(q_type)),
                in_bounds);
        tree invalid =
            fold_convert(result_type, missing_string_terminator(integer_type_node, at(call)));
        result = fold_build3_loc(at(call), COND_EXPR, result_type, in_bounds, result, invalid);
    }
    return predicate ? result : fold_convert(TREE_TYPE(call), result);
}

tree expand_memory_relation(tree value) {
    auto relation = TREE_CODE(value);
    if (relation != EQ_EXPR && relation != NE_EXPR && relation != LT_EXPR && relation != LE_EXPR &&
        relation != GT_EXPR && relation != GE_EXPR)
        return NULL_TREE;
    tree call = TREE_OPERAND(value, 0), zero = TREE_OPERAND(value, 1);
    if (integer_zerop(call)) {
        std::swap(call, zero);
        relation = relation == LT_EXPR   ? GT_EXPR
                   : relation == LE_EXPR ? GE_EXPR
                   : relation == GT_EXPR ? LT_EXPR
                   : relation == GE_EXPR ? LE_EXPR
                                         : relation;
    }
    // Do not strip casts here. Narrowing or unsigned conversion of memcmp's
    // result can change its sign, and therefore requires the full integer value.
    if (!zero || !integer_zerop(zero) || !call || TREE_CODE(call) != CALL_EXPR ||
        !standard_memory_compare(get_callee_fndecl(call)))
        return NULL_TREE;
    tree result = expand_memory_compare(call, relation);
    return result ? fold_convert(TREE_TYPE(value), result) : NULL_TREE;
}

tree expand_memory_set(tree call) {
    if (call_expr_nargs(call) != 3)
        return NULL_TREE;
    QuantumByteRange range;
    bool resolved = resolve_byte_range(CALL_EXPR_ARG(call, 0), range);
    if (!range.quantum && !has_quantum(CALL_EXPR_ARG(call, 0)) &&
        !has_quantum(CALL_EXPR_ARG(call, 1)) && !has_quantum(CALL_EXPR_ARG(call, 2)))
        return NULL_TREE; // Ordinary CPU storage and values keep their libc call.
    auto reject = [call](const char *message) -> tree {
        error_at(at(call), "%s", message);
        return build_zero_cst(TREE_TYPE(call));
    };
    if (!range.quantum)
        return reject(
            "|c>: quantum memset cannot write an unmeasured value into CPU storage; use explicit measurement");
    if (!resolved) {
        std::string message =
            range.error ? range.error
                        : "|c>: quantum memset requires checked integer object-byte storage";
        const std::string shared = "quantum byte comparison";
        size_t position = message.find(shared);
        if (position != std::string::npos)
            message.replace(position, shared.size(), "quantum memset");
        return reject(message.c_str());
    }
    tree bytes = strip(fold(CALL_EXPR_ARG(call, 2)));
    if (!bytes || TREE_CODE(bytes) != INTEGER_CST || !tree_fits_uhwi_p(bytes))
        return expand_dynamic_memory(call, false, true);
    uint64_t count = tree_to_uhwi(bytes);
    if (count > range.size() - range.offset)
        return reject("|c>: quantum memset exceeds its checked storage bounds");
    if (count > 64)
        return reject("|c>: quantum memset exceeds the 64-byte expression synthesis bound");
    if (TYPE_READONLY(range.element_type))
        return reject("|c>: quantum memset requires mutable destination storage");
    tree fill = expand_quantum_program(CALL_EXPR_ARG(call, 1));
    if (!has_quantum(fill)) {
        fill = save_expr(fill);
        cpu_library_saves.insert(fill);
    }
    QuantumMemorySet operation;
    operation.fill = fold_convert(unsigned_char_type_node, fill);
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t position = range.offset + i;
        tree target = range.elements[(size_t)(position / range.stride)];
        if (readonly_quantum_borrows.count(target))
            return reject("|c>: quantum memset cannot write through a read-only borrow");
        unsigned byte = (unsigned)(position % range.stride);
        unsigned shift = 8 * (BYTES_BIG_ENDIAN ? (unsigned)range.stride - byte - 1 : byte);
        if (operation.elements.empty() || operation.elements.back().first != target)
            operation.elements.emplace_back(target, 0);
        operation.elements.back().second |= uint64_t{255} << shift;
    }
    std::vector<tree> arguments{operation.fill};
    for (const auto &entry : operation.elements)
        arguments.push_back(entry.first);
    tree marker =
        build_fn_decl("__cket_memory_set", build_function_type(TREE_TYPE(call), NULL_TREE));
    tree result =
        build_call_expr_loc_array(at(call), marker, (int)arguments.size(), arguments.data());
    quantum_memory_sets.emplace(result, std::move(operation));
    return result;
}

bool extract_string_literal(tree arg, std::string &out) {
    arg = strip(arg);
    while (arg && (TREE_CODE(arg) == ADDR_EXPR || TREE_CODE(arg) == ARRAY_REF))
        arg = strip(TREE_OPERAND(arg, 0));
    if (!arg || TREE_CODE(arg) != STRING_CST)
        return false;
    const char *p = TREE_STRING_POINTER(arg);
    int len = TREE_STRING_LENGTH(arg);
    out.assign(p, len > 0 ? strnlen(p, (size_t)len) : 0);
    return true;
}

int static_zero_or_nonzero_byte(tree expr, unsigned depth = 0) {
    if (!expr || depth > 16)
        return -1;
    expr = strip(expr);
    if (TREE_CODE(expr) == INTEGER_CST)
        return integer_zerop(expr) ? 0 : 1;
    if (TREE_CODE(expr) == COND_EXPR) {
        int a = static_zero_or_nonzero_byte(TREE_OPERAND(expr, 1), depth + 1);
        int b = static_zero_or_nonzero_byte(TREE_OPERAND(expr, 2), depth + 1);
        return (a >= 0 && a == b) ? a : -1;
    }
    if (TREE_CODE(expr) == VAR_DECL && DECL_INITIAL(expr))
        return static_zero_or_nonzero_byte(DECL_INITIAL(expr), depth + 1);
    return -1;
}

bool qsort_comparator_is_descending(tree cmp_fn) {
    if (!cmp_fn || TREE_CODE(cmp_fn) != FUNCTION_DECL || !DECL_SAVED_TREE(cmp_fn))
        return false;
    tree p0 = DECL_ARGUMENTS(cmp_fn), p1 = p0 ? DECL_CHAIN(p0) : NULL_TREE;
    if (!p0 || !p1)
        return false;
    std::map<tree, int> origins{{p0, 0}, {p1, 1}};
    tree body = DECL_SAVED_TREE(cmp_fn);
    auto collect_origins = [](tree *node, int *walk, void *data) -> tree {
        auto &orig = *static_cast<std::map<tree, int> *>(data);
        if (TREE_CODE(*node) == DECL_EXPR) {
            tree v = DECL_EXPR_DECL(*node);
            tree init = DECL_INITIAL(v);
            if (init) {
                auto check_param = [](tree *sub, int *w, void *pdata) -> tree {
                    auto &m = *static_cast<std::map<tree, int> *>(pdata);
                    if (m.count(*sub))
                        return *sub;
                    if (DECL_P(*sub) || TYPE_P(*sub))
                        *w = 0;
                    return NULL_TREE;
                };
                tree p = walk_tree(&init, check_param, &orig, nullptr);
                if (p)
                    orig[v] = orig[p];
            }
        }
        if (DECL_P(*node) || TYPE_P(*node))
            *walk = 0;
        return NULL_TREE;
    };
    walk_tree(&body, collect_origins, &origins, nullptr);

    struct DirState {
        std::map<tree, int> *origins;
        int direction;
    } dstate{&origins, 0};

    auto inspect_ret = [](tree *node, int *walk, void *data) -> tree {
        auto &st = *static_cast<DirState *>(data);
        auto &orig = *st.origins;
        if (TREE_CODE(*node) == RETURN_EXPR && TREE_OPERAND(*node, 0)) {
            tree expr = TREE_OPERAND(*node, 0);
            if (TREE_CODE(expr) == MODIFY_EXPR || TREE_CODE(expr) == INIT_EXPR)
                expr = TREE_OPERAND(expr, 1);
            expr = strip(expr);
            if (TREE_CODE(expr) == MINUS_EXPR) {
                tree l = strip(TREE_OPERAND(expr, 0)), r = strip(TREE_OPERAND(expr, 1));
                if ((TREE_CODE(l) == GT_EXPR && TREE_CODE(r) == LT_EXPR) ||
                    (TREE_CODE(l) == GE_EXPR && TREE_CODE(r) == LE_EXPR)) {
                    tree ll = strip(TREE_OPERAND(l, 0)), lr = strip(TREE_OPERAND(l, 1));
                    if (orig.count(ll) && orig.count(lr)) {
                        if (orig[ll] == 1 && orig[lr] == 0)
                            st.direction = -1;
                        else if (orig[ll] == 0 && orig[lr] == 1)
                            st.direction = 1;
                    }
                } else if (orig.count(l) && orig.count(r)) {
                    if (orig[l] == 1 && orig[r] == 0)
                        st.direction = -1;
                    else if (orig[l] == 0 && orig[r] == 1)
                        st.direction = 1;
                }
            }
        }
        if (DECL_P(*node) || TYPE_P(*node))
            *walk = 0;
        return NULL_TREE;
    };
    walk_tree(&body, inspect_ret, &dstate, nullptr);
    return dstate.direction < 0;
}

tree expand_extra_stdlib_call(tree call) {
    if (!call || TREE_CODE(call) != CALL_EXPR)
        return NULL_TREE;
    tree fn = get_callee_fndecl(call);
    if (!fn || (DECL_SAVED_TREE(fn) && !in_system_header_at(DECL_SOURCE_LOCATION(fn))))
        return NULL_TREE;
    std::string name = name_of(fn);
    bool external_sys =
        DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn));

    if (external_sys && (name == "qsort" || name == "__cket_qsort") && call_expr_nargs(call) == 4) {
        QuantumReference ref;
        if (!resolve_quantum_reference(CALL_EXPR_ARG(call, 0), ref))
            return NULL_TREE;
        tree count_cst = strip(fold(CALL_EXPR_ARG(call, 1)));
        tree size_cst = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!count_cst || TREE_CODE(count_cst) != INTEGER_CST || !tree_fits_uhwi_p(count_cst) ||
            !size_cst || TREE_CODE(size_cst) != INTEGER_CST || !tree_fits_uhwi_p(size_cst)) {
            error_at(at(call), "|c>: quantum qsort requires constant element count and size");
            return build_empty_stmt(at(call));
        }
        uint64_t n = tree_to_uhwi(count_cst);
        uint64_t sz = tree_to_uhwi(size_cst);
        tree elem_size = TYPE_SIZE_UNIT(ref.element_type);
        if (!elem_size || !tree_fits_uhwi_p(elem_size) || sz != tree_to_uhwi(elem_size) ||
            ref.leaves_per_element != 1 || !supported_quantum_scalar(ref.element_type)) {
            error_at(at(call), "|c>: quantum qsort requires supported scalar elements");
            return build_empty_stmt(at(call));
        }
        if (TYPE_READONLY(ref.element_type) || ref.offset < 0 ||
            (uint64_t)ref.offset + n > ref.elements.size()) {
            error_at(at(call), "|c>: quantum qsort requires mutable storage within bounds");
            return build_empty_stmt(at(call));
        }
        if (n > 16) {
            error_at(at(call), "|c>: quantum qsort exceeds 16-element sorting network bound");
            return build_empty_stmt(at(call));
        }
        if (n <= 1)
            return build_empty_stmt(at(call));
        tree cmp_arg = strip(CALL_EXPR_ARG(call, 3));
        if (TREE_CODE(cmp_arg) == ADDR_EXPR)
            cmp_arg = strip(TREE_OPERAND(cmp_arg, 0));
        bool descending = qsort_comparator_is_descending(cmp_arg);
        tree list = alloc_stmt_list();
        tree etype = TYPE_MAIN_VARIANT(ref.element_type);
        uint64_t qsort_id = 0;
        auto compare_swap = [&](uint64_t ia, uint64_t ib) {
            tree a = ref.elements[(size_t)ref.offset + ia];
            tree b = ref.elements[(size_t)ref.offset + ib];
            if (readonly_quantum_borrows.count(a) || readonly_quantum_borrows.count(b)) {
                error_at(at(call), "|c>: quantum qsort cannot modify read-only borrowed elements");
                return;
            }
            tree cond =
                fold_build2_loc(at(call), descending ? GT_EXPR : LT_EXPR, boolean_type_node, a, b);
            tree lo_init = fold_build3_loc(at(call), COND_EXPR, etype, cond, a, b);
            tree hi_init = fold_build3_loc(at(call), COND_EXPR, etype, cond, b, a);
            auto make_tmp = [&](tree initial, const char *tag) {
                std::string tname = "__cket_qsort_" + std::string(tag) + "_" +
                                    std::to_string(auto_clean_zeros.size()) + "_" +
                                    std::to_string(qsort_id++);
                tree decl = build_decl(at(call),
                                       VAR_DECL,
                                       get_identifier(tname.c_str()),
                                       build_qualified_type(etype, TYPE_QUAL_VOLATILE));
                DECL_CONTEXT(decl) = active_function;
                DECL_ATTRIBUTES(decl) =
                    tree_cons(get_identifier("cket_quant"), NULL_TREE, NULL_TREE);
                DECL_ARTIFICIAL(decl) = TREE_THIS_VOLATILE(decl) = TREE_SIDE_EFFECTS(decl) = 1;
                DECL_INITIAL(decl) = fold_convert(TREE_TYPE(decl), initial);
                auto_clean_zeros.insert(decl);
                DECL_CHAIN(decl) = cpu_patch_locals;
                cpu_patch_locals = decl;
                return decl;
            };
            tree lo = make_tmp(lo_init, "lo");
            tree hi = make_tmp(hi_init, "hi");
            append(&list, build1_loc(at(call), DECL_EXPR, void_type_node, lo));
            append(&list, build1_loc(at(call), DECL_EXPR, void_type_node, hi));
            append(
                &list,
                build2_loc(at(call), MODIFY_EXPR, TREE_TYPE(a), a, fold_convert(TREE_TYPE(a), lo)));
            append(
                &list,
                build2_loc(at(call), MODIFY_EXPR, TREE_TYPE(b), b, fold_convert(TREE_TYPE(b), hi)));
        };
        for (uint64_t p = 1; p < n; p <<= 1)
            for (uint64_t k = p; k >= 1; k >>= 1) {
                uint64_t j_start = k % p;
                for (uint64_t j = j_start; j + k < n; j += 2 * k)
                    for (uint64_t i = 0; i < k && i + j + k < n; ++i)
                        if ((i + j) / (p * 2) == (i + j + k) / (p * 2))
                            compare_swap(i + j, i + j + k);
                if (k == 1)
                    break;
            }
        return list;
    }

    if (external_sys && (name == "strxfrm" || name == "__cket_strxfrm") &&
        call_expr_nargs(call) == 3) {
        QuantumByteRange dst, src;
        bool dst_q = resolve_byte_range(CALL_EXPR_ARG(call, 0), dst) && dst.quantum;
        bool src_q = resolve_byte_range(CALL_EXPR_ARG(call, 1), src) && src.quantum;
        if (!dst_q && !src_q)
            return NULL_TREE;
        tree count_cst = strip(fold(CALL_EXPR_ARG(call, 2)));
        if (!count_cst || TREE_CODE(count_cst) != INTEGER_CST || !tree_fits_uhwi_p(count_cst)) {
            error_at(at(call), "|c>: quantum strxfrm requires a constant destination buffer bound");
            return build_zero_cst(TREE_TYPE(call));
        }
        uint64_t limit = tree_to_uhwi(count_cst);
        if (!src_q)
            return NULL_TREE;
        uint64_t src_avail = src.size() - src.offset;
        int64_t known_len = -1;
        for (uint64_t k = 0; k < src_avail; ++k) {
            uint64_t pos = src.offset + k;
            tree elem = src.elements[(size_t)(pos / src.stride)];
            int st = src.stride == 1 ? static_zero_or_nonzero_byte(elem) : -1;
            if (st == 0) {
                known_len = (int64_t)k;
                break;
            }
            if (st < 0)
                break;
        }
        if (known_len < 0) {
            error_at(at(call),
                     "|c>: quantum strxfrm requires a source string with a statically bounded NUL");
            return build_zero_cst(TREE_TYPE(call));
        }
        tree effect = build_empty_stmt(at(call));
        if (limit > 0) {
            if (!dst_q) {
                error_at(at(call),
                         "|c>: quantum strxfrm cannot write quantum bytes into CPU storage");
                return build_zero_cst(TREE_TYPE(call));
            }
            uint64_t to_copy = std::min<uint64_t>(limit, (uint64_t)known_len + 1);
            tree memcpy_decl = build_fn_decl(
                "__cket_memcpy",
                build_function_type_list(
                    ptr_type_node, ptr_type_node, const_ptr_type_node, size_type_node, NULL_TREE));
            DECL_EXTERNAL(memcpy_decl) = TREE_PUBLIC(memcpy_decl) = 1;
            tree mc = build_call_expr_loc(at(call),
                                          memcpy_decl,
                                          3,
                                          CALL_EXPR_ARG(call, 0),
                                          CALL_EXPR_ARG(call, 1),
                                          build_int_cstu(size_type_node, to_copy));
            effect = expand_memory_copy(mc, false);
            if (!effect)
                effect = build_empty_stmt(at(call));
        }
        tree list = alloc_stmt_list();
        append(&list, effect);
        return build2_loc(at(call),
                          COMPOUND_EXPR,
                          TREE_TYPE(call),
                          list,
                          build_int_cstu(TREE_TYPE(call), (uint64_t)known_len));
    }

    bool is_snprintf_chk =
        fndecl_built_in_p(fn, BUILT_IN_SNPRINTF_CHK) || (external_sys && name == "__snprintf_chk");
    bool is_snprintf = is_snprintf_chk || fndecl_built_in_p(fn, BUILT_IN_SNPRINTF) ||
                       (external_sys && (name == "snprintf" || name == "__cket_snprintf"));
    if (is_snprintf) {
        int fmt_idx = is_snprintf_chk ? 4 : 2;
        if (call_expr_nargs(call) <= fmt_idx)
            return NULL_TREE;
        QuantumReference buf_ref;
        bool buf_q = resolve_quantum_reference(CALL_EXPR_ARG(call, 0), buf_ref);
        bool any_q = buf_q;
        for (int i = fmt_idx + 1; i < call_expr_nargs(call); ++i)
            any_q |= has_quantum(CALL_EXPR_ARG(call, i));
        if (!any_q)
            return NULL_TREE;
        std::string fmt;
        if (!buf_q || !extract_string_literal(CALL_EXPR_ARG(call, fmt_idx), fmt)) {
            error_at(at(call),
                     "|c>: quantum snprintf requires checked buffer storage and a literal format");
            return build_zero_cst(TREE_TYPE(call));
        }
        if (TYPE_READONLY(buf_ref.element_type) || buf_ref.leaves_per_element != 1 ||
            !INTEGRAL_TYPE_P(buf_ref.element_type) || TYPE_PRECISION(buf_ref.element_type) != 8) {
            error_at(at(call), "|c>: quantum snprintf requires mutable char storage");
            return build_zero_cst(TREE_TYPE(call));
        }
        tree cap_cst = strip(fold(CALL_EXPR_ARG(call, 1)));
        if (!cap_cst || TREE_CODE(cap_cst) != INTEGER_CST || !tree_fits_uhwi_p(cap_cst)) {
            error_at(at(call), "|c>: quantum snprintf requires a constant capacity");
            return build_zero_cst(TREE_TYPE(call));
        }
        uint64_t cap = tree_to_uhwi(cap_cst);
        size_t pct = fmt.find('%');
        if (pct == std::string::npos || pct + 1 >= fmt.size() ||
            (fmt[pct + 1] != 'u' && fmt[pct + 1] != 'd') ||
            fmt.find('%', pct + 2) != std::string::npos || call_expr_nargs(call) != fmt_idx + 2) {
            error_at(at(call),
                     "%s",
                     "|c>: quantum snprintf currently supports a single %u or %d integer field");
            return build_zero_cst(TREE_TYPE(call));
        }
        std::string prefix_lit = fmt.substr(0, pct), suffix_lit = fmt.substr(pct + 2);
        uint64_t needed = prefix_lit.size() + 1 + suffix_lit.size() + 1;
        if (cap < needed || buf_ref.offset < 0 ||
            (uint64_t)buf_ref.offset + needed > buf_ref.elements.size()) {
            error_at(at(call), "|c>: quantum snprintf exceeds buffer capacity or bounds");
            return build_zero_cst(TREE_TYPE(call));
        }
        tree val = expand_quantum_program(CALL_EXPR_ARG(call, fmt_idx + 1));
        tree ctype = TYPE_MAIN_VARIANT(buf_ref.element_type);
        tree list = alloc_stmt_list();
        size_t pos = (size_t)buf_ref.offset;
        auto emit_byte = [&](tree bval) {
            tree dst = buf_ref.elements[pos++];
            append(&list,
                   expand_quantum_program(build2_loc(at(call),
                                                     MODIFY_EXPR,
                                                     TREE_TYPE(dst),
                                                     dst,
                                                     fold_convert(TREE_TYPE(dst), bval))));
        };
        for (char ch : prefix_lit)
            emit_byte(build_int_cst(ctype, (unsigned char)ch));
        tree digit =
            fold_build2_loc(at(call),
                            PLUS_EXPR,
                            ctype,
                            fold_convert(ctype, build_int_cst(unsigned_char_type_node, '0')),
                            fold_convert(ctype, val));
        emit_byte(digit);
        for (char ch : suffix_lit)
            emit_byte(build_int_cst(ctype, (unsigned char)ch));
        emit_byte(build_zero_cst(ctype));
        return build2_loc(at(call),
                          COMPOUND_EXPR,
                          TREE_TYPE(call),
                          list,
                          build_int_cst(TREE_TYPE(call), (int)(needed - 1)));
    }

    if ((fndecl_built_in_p(fn, BUILT_IN_SSCANF) ||
         (external_sys && (name == "sscanf" || name == "__cket_sscanf"))) &&
        call_expr_nargs(call) >= 3) {
        QuantumReference buf_ref;
        bool buf_q = resolve_quantum_reference(CALL_EXPR_ARG(call, 0), buf_ref);
        if (!buf_q)
            return NULL_TREE;
        std::string fmt;
        if (!extract_string_literal(CALL_EXPR_ARG(call, 1), fmt)) {
            error_at(at(call), "|c>: quantum sscanf requires a literal format");
            return build_zero_cst(TREE_TYPE(call));
        }
        size_t pct = fmt.find('%');
        if (pct == std::string::npos || pct + 1 >= fmt.size() ||
            (fmt[pct + 1] != 'u' && fmt[pct + 1] != 'd') ||
            fmt.find('%', pct + 2) != std::string::npos || call_expr_nargs(call) != 3) {
            error_at(at(call),
                     "%s",
                     "|c>: quantum sscanf currently supports a single %u or %d output field");
            return build_zero_cst(TREE_TYPE(call));
        }
        QuantumReference out_ref;
        if (!resolve_quantum_reference(CALL_EXPR_ARG(call, 2), out_ref) || out_ref.offset < 0 ||
            (size_t)out_ref.offset >= out_ref.elements.size() ||
            TYPE_READONLY(out_ref.element_type)) {
            error_at(at(call), "|c>: quantum sscanf requires a mutable scalar output borrow");
            return build_zero_cst(TREE_TYPE(call));
        }
        std::string prefix_lit = fmt.substr(0, pct);
        if (buf_ref.offset < 0 ||
            (uint64_t)buf_ref.offset + prefix_lit.size() + 1 > buf_ref.elements.size() ||
            buf_ref.leaves_per_element != 1 || !INTEGRAL_TYPE_P(buf_ref.element_type) ||
            TYPE_PRECISION(buf_ref.element_type) != 8) {
            error_at(at(call), "|c>: quantum sscanf input buffer bounds are too small");
            return build_zero_cst(TREE_TYPE(call));
        }
        tree match = boolean_true_node;
        for (size_t k = 0; k < prefix_lit.size(); ++k) {
            tree ch_elem = buf_ref.elements[(size_t)buf_ref.offset + k];
            tree eq =
                fold_build2_loc(at(call),
                                EQ_EXPR,
                                boolean_type_node,
                                fold_convert(integer_type_node, ch_elem),
                                build_int_cst(integer_type_node, (unsigned char)prefix_lit[k]));
            match = fold_build2_loc(at(call), TRUTH_ANDIF_EXPR, boolean_type_node, match, eq);
        }
        tree digit_elem = buf_ref.elements[(size_t)buf_ref.offset + prefix_lit.size()];
        tree digit_i = fold_convert(integer_type_node, digit_elem);
        tree ge0 = fold_build2_loc(
            at(call), GE_EXPR, boolean_type_node, digit_i, build_int_cst(integer_type_node, '0'));
        tree le9 = fold_build2_loc(
            at(call), LE_EXPR, boolean_type_node, digit_i, build_int_cst(integer_type_node, '9'));
        match = fold_build2_loc(
            at(call),
            TRUTH_ANDIF_EXPR,
            boolean_type_node,
            match,
            fold_build2_loc(at(call), TRUTH_ANDIF_EXPR, boolean_type_node, ge0, le9));
        tree parsed = fold_build2_loc(
            at(call),
            MINUS_EXPR,
            TREE_TYPE(out_ref.elements[(size_t)out_ref.offset]),
            fold_convert(TREE_TYPE(out_ref.elements[(size_t)out_ref.offset]), digit_elem),
            build_int_cst(TREE_TYPE(out_ref.elements[(size_t)out_ref.offset]), '0'));
        tree out_var = out_ref.elements[(size_t)out_ref.offset];
        tree write =
            fold_build3_loc(at(call), COND_EXPR, TREE_TYPE(out_var), match, parsed, out_var);
        tree stmt = expand_quantum_program(
            build2_loc(at(call), MODIFY_EXPR, TREE_TYPE(out_var), out_var, write));
        tree list = alloc_stmt_list();
        append(&list, stmt);
        tree count_val = fold_build3_loc(at(call),
                                         COND_EXPR,
                                         TREE_TYPE(call),
                                         match,
                                         build_int_cst(TREE_TYPE(call), 1),
                                         build_zero_cst(TREE_TYPE(call)));
        return build2_loc(at(call), COMPOUND_EXPR, TREE_TYPE(call), list, count_val);
    }

    return NULL_TREE;
}

tree expand_extra_stdlib_relation(tree) {
    return NULL_TREE;
}
