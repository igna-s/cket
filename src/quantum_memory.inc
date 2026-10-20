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
             name_of(fn) == "strcmp" || name_of(fn) == "__cket_strcmp"));
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
           (DECL_EXTERNAL(fn) && TREE_PUBLIC(fn) && in_system_header_at(DECL_SOURCE_LOCATION(fn)) &&
            (name_of(fn) == "strcpy" || name_of(fn) == "__cket_strcpy" ||
             name_of(fn) == "strncpy" || name_of(fn) == "__cket_strncpy"));
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
        range.element_type = reference.element_type;
        if (!byte_range_layout(range))
            return false;
        range.offset = (uint64_t)reference.offset * range.stride;
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

tree expand_memory_compare(tree call, enum tree_code relation = ERROR_MARK) {
    tree fn = get_callee_fndecl(call);
    bool unbounded = fn && (fndecl_built_in_p(fn, BUILT_IN_STRCMP) || name_of(fn) == "strcmp" ||
                            name_of(fn) == "__cket_strcmp");
    if (call_expr_nargs(call) != (unbounded ? 2 : 3))
        return NULL_TREE;
    bool stop_at_zero =
        unbounded || (fn && (fndecl_built_in_p(fn, BUILT_IN_STRNCMP) || name_of(fn) == "strncmp" ||
                             name_of(fn) == "__cket_strncmp"));
    QuantumByteRange left, right;
    bool a = resolve_byte_range(CALL_EXPR_ARG(call, 0), left);
    bool b = resolve_byte_range(CALL_EXPR_ARG(call, 1), right);
    if (!left.quantum && !right.quantum)
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
    if (unbounded) {
        uint64_t left_count = left.size() - left.offset, right_count = right.size() - right.offset;
        if (!left_count || !right_count)
            return reject("|c>: quantum strcmp needs a NUL inside both checked storage views");
        if (left_count > 64 || right_count > 64)
            return reject("|c>: quantum strcmp exceeds the 64-byte expression synthesis bound");
        count = std::min(left_count, right_count);
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
        tree x = object_byte(left, i - 1, at(call));
        tree y = object_byte(right, i - 1, at(call));
        if (i == count && relation != EQ_EXPR && relation != NE_EXPR) {
            result =
                fold_build2_loc(at(call), predicate ? relation : MINUS_EXPR, result_type, x, y);
            continue;
        }
        // strncmp stops at an equal NUL; unequal bytes still select their
        // own relation/difference before this suffix can contribute.
        if (stop_at_zero && i < count) {
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
        !has_quantum(CALL_EXPR_ARG(call, 1)))
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
        return reject("|c>: quantum memset currently requires a constant checked byte count");
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
