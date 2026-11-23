// |c> frontend extension. All trees below are produced by GCC's real C parser.
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "c-family/c-common.h"
#include "stringpool.h"
#include "tree-iterator.h"
#include "diagnostic.h"
#include "attribs.h"
#include "print-tree.h"
#include "tree-pretty-print.h"
#include "fold-const.h"
#include "ggc.h"
#include "gtype-desc.h"
#include "langhooks.h"
#include "real.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <climits>

int plugin_is_GPL_compatible;

namespace {
struct Variable {
    tree decl;
    unsigned id, width;
    bool is_signed, is_float = false, lowered = false, measured = false;
    uint64_t lower = 0, upper = 0;
    std::string name, c_type;
    tree original_init = NULL_TREE;
    bool expression_init = false, classical_init = false, derived = false, released = false,
         moved = false, clean = false;
    uint64_t range_max = 0;
    unsigned epoch = 0;
    std::string creation;
    std::set<unsigned> dependencies;
    bool basis_known = false;
    bool history = false;
    bool optional_definition = false;
    bool runtime_history = false;
    bool private_capture = false;
    bool pending_destruction = false;
    bool pristine_uniform = false;
};

std::map<tree, Variable> variables;

struct QuantumIndexedAccess {
    std::vector<tree> elements;
    tree index;
    bool readonly;
};

std::map<tree, QuantumIndexedAccess> quantum_indexed_accesses;
std::map<tree, QuantumIndexedAccess> quantum_coherent_accesses;
// Value reads retain an index proof separate from the existing CPU lvalue path.
std::map<tree, tree> quantum_indexed_read_indices;

struct QuantumHeapAccess {
    tree pointer, index, type;
    bool readonly;
};

struct QuantumHeapUpdate {
    QuantumHeapAccess target;
    tree value;
    bool postfix;
};

std::map<tree, tree> quantum_heap_pointers;
std::map<tree, QuantumHeapAccess> quantum_heap_accesses;
std::map<tree, QuantumHeapUpdate> quantum_heap_updates;

struct QuantumNativeMemory {
    tree destination, source, count;
    std::string mode;
    std::vector<tree> bytes{};
    tree fixed_output = NULL_TREE;
    uint64_t limit = 64;
    uint64_t overlap = UINT64_MAX;
};

std::map<tree, QuantumNativeMemory> quantum_native_memory;
std::map<tree, std::pair<tree, uint64_t>> quantum_heap_byte_reads;
std::set<tree> quantum_memory_cpu_branches;
std::set<tree> procedure_cpu_flags, procedure_cpu_branches, cpu_argument_captures;
std::set<tree> coherent_cpu_locals;
std::map<tree, tree> coherent_cpu_updates;
std::set<tree> coherent_cpu_branches, coherent_cpu_loops;
std::set<tree> cpu_guarded_captures;
std::set<tree> quantum_heap_borrows;
std::set<tree> quantum_heap_queries;

struct QuantumMemoryCopy {
    std::vector<std::pair<tree, tree>> elements; // Destination, source, in read-before-write order.
};

std::map<tree, QuantumMemoryCopy> quantum_memory_copies;

struct QuantumValueProgram {
    std::vector<tree> prefix, updates;
    std::map<tree, tree> bit_masks;
    std::map<tree, tree> quantum_masks;
    tree result = NULL_TREE;
};

std::map<tree, QuantumValueProgram> quantum_value_programs;
tree decimal_space_fn = NULL_TREE;
bool value_program_prefix = false;

struct QuantumMemorySet {
    tree fill;
    std::vector<std::pair<tree, uint64_t>> elements; // Destination and selected object-bit mask.
};

std::map<tree, QuantumMemorySet> quantum_memory_sets;
std::map<tree, tree> indexed_expansions;
std::set<tree> readonly_quantum_borrows;

struct QuantumFunction {
    tree decl, body;
    bool result;
    bool owned_result = false;
};

std::map<std::string, QuantumFunction> quantum_functions;
std::map<std::string, unsigned> expanding_functions;
std::set<tree> quantum_fn_pointer_vars;
std::map<tree, std::vector<tree>> quantum_fn_pointer_targets;
std::set<tree> directly_observed_quantum_outputs;
std::set<tree> quantum_owned_subexprs;
std::set<tree> auto_clean_zeros;
std::map<tree, tree> quantum_byte_pun_pointers;
std::map<tree, uint64_t> known_cpu_constants;
std::vector<tree> file_scope_quantum_vars;
tree strtok_last_address = NULL_TREE;
uint64_t strtok_next_offset = 0;
bool compile_only_mode = false;

bool quantum_function(tree fn) {
    if (!fn || TREE_CODE(fn) != FUNCTION_DECL)
        return false;
    if (lookup_attribute("cket_quant", DECL_ATTRIBUTES(fn)))
        return true;
    for (tree p = DECL_ARGUMENTS(fn); p; p = DECL_CHAIN(p))
        if (lookup_attribute("cket_quant", DECL_ATTRIBUTES(p)))
            return true;
    return quantum_functions.count(DECL_NAME(fn) ? IDENTIFIER_POINTER(DECL_NAME(fn)) : "");
}

std::set<tree> recorded_declarations;
std::set<tree> expanded_save_exprs;
std::map<tree, tree> expanded_save_expr_map;
std::vector<std::string> symbols, instructions;
std::string ir_path, ast_path;
tree prepare_fn, measure_fn;
tree active_function = NULL_TREE;
bool has_quantum(tree value);
FILE *ast_file = nullptr;
bool saw_measure = false;
bool saw_return = false;
bool saw_main_function = false;
bool lowering_standalone_module = false;
unsigned next_id = 0, next_measurement = 0;

std::string json(const std::string &value) {
    std::ostringstream stream;
    stream << '"';
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\')
            stream << '\\' << ch;
        else if (ch == '\n')
            stream << "\\n";
        else if (ch < 32)
            stream << "\\u00" << "0123456789abcdef"[ch >> 4] << "0123456789abcdef"[ch & 15];
        else
            stream << ch;
    }
    stream << '"';
    return stream.str();
}

std::string name_of(tree decl) {
    return DECL_NAME(decl) ? IDENTIFIER_POINTER(DECL_NAME(decl)) : "<anonymous>";
}

bool supported_quantum_scalar(tree type) {
    if (!type)
        return false;
    if (INTEGRAL_TYPE_P(type))
        return TYPE_PRECISION(type) <= 64;
    return SCALAR_FLOAT_TYPE_P(type) && (TYPE_PRECISION(type) == 32 || TYPE_PRECISION(type) == 64);
}

uint64_t encode_float_bits(tree cst, unsigned width) {
    unsigned char buf[16]{};
    int len = native_encode_expr(cst, buf, sizeof(buf));
    uint64_t bits = 0;
    if (len == 4 && width == 32) {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        bits = v;
    } else if (len == 8 && width == 64) {
        std::memcpy(&bits, buf, 8);
    }
    return bits;
}

uint64_t mask_to_width(uint64_t value, unsigned width) {
    return width >= 64 ? value : (value & ((uint64_t{1} << width) - 1));
}

std::string interval_bound_json(uint64_t value, unsigned width, bool is_signed) {
    if (!is_signed)
        return std::to_string(mask_to_width(value, width));
    unsigned shift = 64 - width;
    int64_t signed_value = static_cast<int64_t>(value << shift) >> shift;
    return std::to_string(signed_value);
}

bool quantum(tree decl) {
    return decl && (TREE_CODE(decl) == VAR_DECL || TREE_CODE(decl) == PARM_DECL) &&
           supported_quantum_scalar(TREE_TYPE(decl)) &&
           lookup_attribute("cket_quant", DECL_ATTRIBUTES(decl));
}

tree strip(tree value) {
    while (value) {
        if (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR ||
            TREE_CODE(value) == PAREN_EXPR)
            value = TREE_OPERAND(value, 0);
        else if (TREE_CODE(value) == C_MAYBE_CONST_EXPR && !TREE_OPERAND(value, 0))
            value = C_MAYBE_CONST_EXPR_EXPR(value);
        else
            break;
    }
    return value;
}

std::string intrinsic(tree call) {
    if (!call || TREE_CODE(call) != CALL_EXPR)
        return "";
    tree fn = get_callee_fndecl(call);
    if (!fn)
        return "";
    tree attr = lookup_attribute("cket_intrinsic", DECL_ATTRIBUTES(fn));
    if (!attr)
        return "";
    tree value = TREE_VALUE(TREE_VALUE(attr));
    return TREE_CODE(value) == STRING_CST ? TREE_STRING_POINTER(value) : "";
}

location_t at(tree node) {
    if (DECL_P(node))
        return DECL_SOURCE_LOCATION(node);
    return EXPR_HAS_LOCATION(node) ? EXPR_LOCATION(node) : input_location;
}

bool gcc_quantum_sanitizer_guard(tree value);

bool array_element_count(tree type, uint64_t &count_out) {
    if (!type || TREE_CODE(type) != ARRAY_TYPE)
        return false;
    tree domain = TYPE_DOMAIN(type);
    tree max_val = domain ? TYPE_MAX_VALUE(domain) : NULL_TREE;
    if (!max_val)
        return false;
    if (tree_fits_uhwi_p(max_val)) {
        uint64_t n = tree_to_uhwi(max_val) + 1;
        if (!n || n > 4096)
            return false;
        count_out = n;
        return true;
    }
    auto subst = [](tree node, auto &self) -> tree {
        if (!node)
            return node;
        if (TREE_CODE(node) == SAVE_EXPR)
            return self(TREE_OPERAND(node, 0), self);
        if (TREE_CODE(node) == COMPOUND_EXPR && gcc_quantum_sanitizer_guard(TREE_OPERAND(node, 0)))
            return self(TREE_OPERAND(node, 1), self);
        if (CONVERT_EXPR_P(node) || TREE_CODE(node) == NON_LVALUE_EXPR) {
            tree inner = self(TREE_OPERAND(node, 0), self);
            return inner ? fold_convert(TREE_TYPE(node), inner) : node;
        }
        if (TREE_CODE(node) == VAR_DECL) {
            if (known_cpu_constants.count(node))
                return build_int_cstu(TREE_TYPE(node), known_cpu_constants.at(node));
            if (!active_function && !TREE_THIS_VOLATILE(node) && DECL_INITIAL(node) &&
                TREE_CODE(DECL_INITIAL(node)) == INTEGER_CST)
                return fold_convert(TREE_TYPE(node), DECL_INITIAL(node));
        }
        if (EXPR_P(node) && TREE_OPERAND_LENGTH(node) == 2) {
            tree a = self(TREE_OPERAND(node, 0), self);
            tree b = self(TREE_OPERAND(node, 1), self);
            return fold_build2(TREE_CODE(node), TREE_TYPE(node), a, b);
        }
        return node;
    };
    tree folded = strip(fold(subst(max_val, subst)));
    if (folded && TREE_CODE(folded) == INTEGER_CST && tree_fits_uhwi_p(folded)) {
        uint64_t n = tree_to_uhwi(folded) + 1;
        if (n >= 1 && n <= 256) {
            count_out = n;
            return true;
        }
    }
    return false;
}

bool union_byte_size(tree type, uint64_t &bytes_out) {
    if (!type || TREE_CODE(type) != UNION_TYPE)
        return false;
    tree sz = TYPE_SIZE_UNIT(type);
    if (!sz || !tree_fits_uhwi_p(sz))
        return false;
    uint64_t n = tree_to_uhwi(sz);
    if (!n || n > 8)
        return false;
    bytes_out = n;
    return true;
}

bool quantum_product_type(tree type) {
    if (!type)
        return false;
    type = TYPE_MAIN_VARIANT(type);
    if (supported_quantum_scalar(type))
        return true;
    if (TREE_CODE(type) == ARRAY_TYPE) {
        uint64_t count = 0;
        return array_element_count(type, count) && quantum_product_type(TREE_TYPE(type));
    }
    if (TREE_CODE(type) == RECORD_TYPE) {
        for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f))
            if (TREE_CODE(f) == FIELD_DECL) {
                tree ft = TREE_TYPE(f);
                if (DECL_BIT_FIELD(f)) {
                    if (!INTEGRAL_TYPE_P(ft) || !TYPE_PRECISION(ft) || TYPE_PRECISION(ft) > 64)
                        return false;
                    continue;
                }
                if (TREE_CODE(ft) == POINTER_TYPE) {
                    if ((!lookup_attribute("cket_quant", DECL_ATTRIBUTES(f)) &&
                         !lookup_attribute("cket_quant", TYPE_ATTRIBUTES(ft)) &&
                         !lookup_attribute("cket_quant", TYPE_ATTRIBUTES(TREE_TYPE(ft)))) ||
                        !supported_quantum_scalar(TYPE_MAIN_VARIANT(TREE_TYPE(ft))))
                        return false;
                    continue;
                }
                if (!quantum_product_type(ft))
                    return false;
            }
        return true;
    }
    if (TREE_CODE(type) == UNION_TYPE) {
        uint64_t bytes = 0;
        if (!union_byte_size(type, bytes))
            return false;
        bool has_field = false;
        for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f))
            if (TREE_CODE(f) == FIELD_DECL) {
                has_field = true;
                if (!quantum_product_type(TREE_TYPE(f)))
                    return false;
            }
        return has_field;
    }
    return false;
}

tree quantum_attribute(tree *node, tree, tree, int, bool *no_add) {
    if (TYPE_P(*node)) {
        tree type = *node;
        if (TREE_CODE(type) == POINTER_TYPE)
            *node = build_pointer_type(build_qualified_type(
                TREE_TYPE(type), TYPE_QUALS(TREE_TYPE(type)) | TYPE_QUAL_VOLATILE));
        else if (TREE_CODE(type) != VOID_TYPE)
            *node = build_qualified_type(type, TYPE_QUALS(type) | TYPE_QUAL_VOLATILE);
        return NULL_TREE;
    }
    tree decl = *node;
    if (TREE_CODE(decl) == FUNCTION_DECL) {
        tree result = TREE_TYPE(TREE_TYPE(decl));
        bool valid_result =
            result == void_type_node || supported_quantum_scalar(result) ||
            (TREE_CODE(result) == RECORD_TYPE && quantum_product_type(TYPE_MAIN_VARIANT(result)));
        if (!valid_result) {
            error_at(at(decl),
                     "|c>: quantum function results currently require a C scalar or struct type");
            *no_add = true;
        }
        return NULL_TREE;
    }
    if (TREE_CODE(decl) == FIELD_DECL) {
        tree ft = TREE_TYPE(decl);
        if (TREE_CODE(ft) == POINTER_TYPE &&
            supported_quantum_scalar(TYPE_MAIN_VARIANT(TREE_TYPE(ft)))) {
            TREE_TYPE(decl) = build_pointer_type(build_qualified_type(
                TREE_TYPE(ft), TYPE_QUALS(TREE_TYPE(ft)) | TYPE_QUAL_VOLATILE));
            return NULL_TREE;
        }
        if (quantum_product_type(TYPE_MAIN_VARIANT(ft)))
            return NULL_TREE;
        error_at(at(decl),
                 "|c>: quant field requires a scalar, quantum pointer, array or structure");
        *no_add = true;
        return NULL_TREE;
    }
    if (TREE_CODE(decl) == TYPE_DECL) {
        tree type = TREE_TYPE(decl);
        if (TREE_CODE(type) == POINTER_TYPE && TREE_CODE(TREE_TYPE(type)) == FUNCTION_TYPE) {
            return NULL_TREE;
        }
        if (TREE_CODE(type) == POINTER_TYPE &&
            (supported_quantum_scalar(TYPE_MAIN_VARIANT(TREE_TYPE(type))) ||
             (TREE_CODE(TREE_TYPE(type)) == RECORD_TYPE &&
              quantum_product_type(TYPE_MAIN_VARIANT(TREE_TYPE(type)))))) {
            tree target = build_qualified_type(TREE_TYPE(type),
                                               TYPE_QUALS(TREE_TYPE(type)) | TYPE_QUAL_VOLATILE);
            tree ptr = build_pointer_type(target);
            if (TYPE_NAME(type) == decl && TYPE_NAME(ptr) != decl) {
                ptr = build_variant_type_copy(ptr);
                TYPE_NAME(ptr) = decl;
            }
            TREE_TYPE(decl) = ptr;
            return NULL_TREE;
        }
        if (!quantum_product_type(TYPE_MAIN_VARIANT(type))) {
            error_at(
                at(decl),
                "|c>: quant requires integers/bools, float/double, fixed arrays or structures");
            *no_add = true;
            return NULL_TREE;
        }
        tree qualified = build_qualified_type(type, TYPE_QUALS(type) | TYPE_QUAL_VOLATILE);
        if (TYPE_NAME(type) == decl && TYPE_NAME(qualified) != decl) {
            qualified = build_variant_type_copy(qualified);
            TYPE_NAME(qualified) = decl;
        }
        TREE_TYPE(decl) = qualified;
        return NULL_TREE;
    }
    if (TREE_CODE(decl) == PARM_DECL) {
        tree type = TREE_TYPE(decl);
        if (TREE_CODE(type) == POINTER_TYPE &&
            (supported_quantum_scalar(TYPE_MAIN_VARIANT(TREE_TYPE(type))) ||
             (TREE_CODE(TREE_TYPE(type)) == RECORD_TYPE &&
              quantum_product_type(TYPE_MAIN_VARIANT(TREE_TYPE(type)))))) {
            TREE_TYPE(decl) = build_pointer_type(build_qualified_type(
                TREE_TYPE(type), TYPE_QUALS(TREE_TYPE(type)) | TYPE_QUAL_VOLATILE));
            return NULL_TREE;
        }
        if (!supported_quantum_scalar(type)) {
            error_at(
                at(decl),
                "|c>: quantum parameters currently borrow scalar registers; pointer/array parameters need an explicit borrow contract");
            *no_add = true;
            return NULL_TREE;
        }
        TREE_TYPE(decl) = build_qualified_type(type, TYPE_QUALS(type) | TYPE_QUAL_VOLATILE);
        TREE_THIS_VOLATILE(decl) = TREE_SIDE_EFFECTS(decl) = 1;
        return NULL_TREE;
    }
    tree type = TREE_TYPE(decl);
    if (TREE_CODE(decl) == VAR_DECL && TREE_CODE(type) == POINTER_TYPE &&
        TREE_CODE(TREE_TYPE(type)) == FUNCTION_TYPE) {
        quantum_fn_pointer_vars.insert(decl);
        return NULL_TREE;
    }
    tree context = DECL_P(decl) && DECL_CONTEXT(decl) ? DECL_CONTEXT(decl) : current_function_decl;
    if (TREE_CODE(decl) != VAR_DECL || !context || TREE_CODE(context) != FUNCTION_DECL ||
        TREE_STATIC(decl) || DECL_EXTERNAL(decl)) {
        if (TREE_CODE(decl) == VAR_DECL && type != error_mark_node &&
            TREE_CODE(type) != POINTER_TYPE && quantum_product_type(type)) {
            TREE_TYPE(decl) = build_qualified_type(type, TYPE_QUALS(type) | TYPE_QUAL_VOLATILE);
            TREE_THIS_VOLATILE(decl) = 1;
            TREE_SIDE_EFFECTS(decl) = 1;
            if (!DECL_EXTERNAL(decl) &&
                std::find(file_scope_quantum_vars.begin(), file_scope_quantum_vars.end(), decl) ==
                    file_scope_quantum_vars.end())
                file_scope_quantum_vars.push_back(decl);
            return NULL_TREE;
        }
        error_at(
            at(decl),
            TREE_CODE(decl) == VAR_DECL && TREE_CODE(type) == POINTER_TYPE
                ? "|c>: quantum pointer requires a checked local borrow; global escape is unsupported"
                : "|c>: quant currently requires an automatic local variable");
        *no_add = true;
        return NULL_TREE;
    }
    if (TREE_CODE(type) == POINTER_TYPE &&
        (supported_quantum_scalar(TYPE_MAIN_VARIANT(TREE_TYPE(type))) ||
         (TREE_CODE(TREE_TYPE(type)) == RECORD_TYPE &&
          quantum_product_type(TYPE_MAIN_VARIANT(TREE_TYPE(type)))))) {
        TREE_TYPE(decl) = build_pointer_type(build_qualified_type(
            TREE_TYPE(type), TYPE_QUALS(TREE_TYPE(type)) | TYPE_QUAL_VOLATILE));
        return NULL_TREE;
    }
    if (!quantum_product_type(type)) {
        error_at(at(decl),
                 "|c>: quant requires integers/bools, float/double, fixed arrays or structures");
        *no_add = true;
        return NULL_TREE;
    }
    // Retain all accesses until the quantum semantic pass. Otherwise GCC may
    // fold x*0 or x-x before the pass can diagnose an illegal quantum read.
    TREE_TYPE(decl) = build_qualified_type(type, TYPE_QUALS(type) | TYPE_QUAL_VOLATILE);
    TREE_THIS_VOLATILE(decl) = 1;
    TREE_SIDE_EFFECTS(decl) = 1;
    return NULL_TREE;
}

tree intrinsic_attribute(tree *node, tree, tree args, int, bool *no_add) {
    if (TREE_CODE(*node) != FUNCTION_DECL || TREE_CODE(TREE_VALUE(args)) != STRING_CST) {
        error("|c>: invalid compiler intrinsic declaration");
        *no_add = true;
    }
    return NULL_TREE;
}

const attribute_spec quant_spec = {
    "cket_quant", 0, 0, false, false, false, false, quantum_attribute, nullptr};
const attribute_spec intrinsic_spec = {
    "cket_intrinsic", 1, 1, true, false, false, false, intrinsic_attribute, nullptr};

void register_attributes(void *, void *) {
    register_attribute(&quant_spec);
    register_attribute(&intrinsic_spec);
}

bool type_has_quant_typedef(tree type, bool allow_pointer = true, unsigned depth = 0) {
    if (!type || type == error_mark_node || !TYPE_P(type) || depth > 8)
        return false;
    if (lookup_attribute("cket_quant", TYPE_ATTRIBUTES(type)))
        return true;
    if (TYPE_NAME(type) && TREE_CODE(TYPE_NAME(type)) == TYPE_DECL &&
        lookup_attribute("cket_quant", DECL_ATTRIBUTES(TYPE_NAME(type))))
        return true;
    if (TREE_CODE(type) == POINTER_TYPE)
        return allow_pointer && type_has_quant_typedef(TREE_TYPE(type), false, depth + 1);
    if (TREE_CODE(type) == ARRAY_TYPE)
        return type_has_quant_typedef(TREE_TYPE(type), false, depth + 1);
    if (TREE_CODE(type) == RECORD_TYPE) {
        for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f))
            if (TREE_CODE(f) == FIELD_DECL &&
                type_has_quant_typedef(TREE_TYPE(f), false, depth + 1))
                return true;
    }
    return false;
}

void propagate_typedef_quant(tree decl) {
    if (!decl || decl == error_mark_node || !DECL_P(decl) ||
        lookup_attribute("cket_quant", DECL_ATTRIBUTES(decl)))
        return;
    bool match = false;
    if (TREE_CODE(decl) == FUNCTION_DECL && TREE_TYPE(decl))
        match = type_has_quant_typedef(TREE_TYPE(TREE_TYPE(decl)));
    else if (TREE_CODE(decl) == VAR_DECL || TREE_CODE(decl) == PARM_DECL)
        match = type_has_quant_typedef(TREE_TYPE(decl));
    if (!match)
        return;
    bool no_add = false;
    quantum_attribute(&decl, get_identifier("cket_quant"), NULL_TREE, 0, &no_add);
    if (!no_add)
        DECL_ATTRIBUTES(decl) =
            tree_cons(get_identifier("cket_quant"), NULL_TREE, DECL_ATTRIBUTES(decl));
}

tree runtime_function(const char *name, unsigned argc) {
    tree args = NULL_TREE;
    args = tree_cons(NULL_TREE, void_type_node, args);
    for (unsigned i = 0; i < argc; ++i)
        args = tree_cons(NULL_TREE, long_long_unsigned_type_node, args);
    tree fn = build_fn_decl(name, build_function_type(long_long_unsigned_type_node, args));
    TREE_PUBLIC(fn) = 1;
    DECL_EXTERNAL(fn) = 1;
    DECL_ARTIFICIAL(fn) = 1;
    return fn;
}

tree constant(uint64_t value) {
    return build_int_cstu(long_long_unsigned_type_node, value);
}

void record_decl(void *event, void *) {
    tree decl = static_cast<tree>(event);
    bool file_scope_global =
        TREE_CODE(decl) == VAR_DECL && TREE_STATIC(decl) &&
        (!DECL_CONTEXT(decl) || TREE_CODE(DECL_CONTEXT(decl)) != FUNCTION_DECL);
    tree context =
        DECL_P(decl) && DECL_CONTEXT(decl) && TREE_CODE(DECL_CONTEXT(decl)) == FUNCTION_DECL
            ? DECL_CONTEXT(decl)
            : active_function;
    if (TREE_CODE(decl) != VAR_DECL || !context || TREE_CODE(context) != FUNCTION_DECL ||
        !DECL_NAME(decl))
        return;
    if (!recorded_declarations.insert(decl).second)
        return;
    if (!DECL_CONTEXT(decl))
        DECL_CONTEXT(decl) = context;
    bool is_quantum = quantum(decl);
    tree type = TYPE_MAIN_VARIANT(TREE_TYPE(decl));
    char *spelling = print_generic_expr_to_str(type);
    std::string c_type = spelling ? spelling : "unknown";
    std::free(spelling);
    unsigned width = supported_quantum_scalar(type) ? TYPE_PRECISION(type) : 0;
    symbols.push_back("{\"name\":" + json(name_of(decl)) + ",\"function\":" +
                      json(name_of(file_scope_global ? active_function : DECL_CONTEXT(decl))) +
                      ",\"type\":{\"c_type\":" + json(c_type) +
                      ",\"quantum\":" + (is_quantum ? "true" : "false") +
                      ",\"bits\":" + std::to_string(width) + "}}");
    if (!is_quantum)
        return;
    if (name_of(context) != "main" && !lowering_standalone_module) {
        error_at(
            at(decl),
            "|c>: quantum locals require main or expansion through a declared quantum procedure");
        return;
    }
    Variable var{};
    var.decl = decl;
    var.id = next_id++;
    var.width = width;
    var.is_float = SCALAR_FLOAT_TYPE_P(type);
    var.is_signed = !var.is_float && !TYPE_UNSIGNED(type);
    var.name = name_of(decl);
    var.c_type = c_type;
    tree init = strip(DECL_INITIAL(decl));
    if (init && var.is_float && TREE_CODE(init) != REAL_CST && TREE_CODE(init) != CALL_EXPR) {
        tree folded = strip(fold(DECL_INITIAL(decl)));
        if (folded && TREE_CODE(folded) == REAL_CST)
            init = folded;
    }
    // C converts uint64_t -> _Bool as a comparison with zero.
    if (TREE_CODE(type) == BOOLEAN_TYPE && init && TREE_CODE(init) == NE_EXPR &&
        integer_zerop(TREE_OPERAND(init, 1)))
        init = strip(TREE_OPERAND(init, 0));
    if (var.is_float) {
        if (init && TREE_CODE(init) == REAL_CST) {
            var.lower = var.upper = encode_float_bits(init, var.width);
        } else if (init && intrinsic(init) == "superposition") {
            error_at(at(decl), "|c>: uniform preparation requires an integer quantum register");
            return;
        } else if (init) {
            var.classical_init = !has_quantum(init);
            var.expression_init = !var.classical_init;
        }
        var.original_init = DECL_INITIAL(decl);
        var.range_max = width == 64 ? UINT64_MAX : UINT32_MAX;
        var.clean = !var.expression_init && !var.classical_init && var.lower == 0;
        var.basis_known = !var.expression_init && !var.classical_init;
        var.pristine_uniform = false;
        variables.emplace(decl, std::move(var));
        return;
    }
    if (init && TREE_CODE(init) == INTEGER_CST) {
        var.lower = var.upper = mask_to_width(static_cast<uint64_t>(TREE_INT_CST_LOW(init)), width);
    } else if (init && intrinsic(init) == "superposition" && call_expr_nargs(init) == 2) {
        tree lower = strip(fold(CALL_EXPR_ARG(init, 0)));
        tree upper = strip(fold(CALL_EXPR_ARG(init, 1)));
        if (TREE_CODE(lower) != INTEGER_CST || TREE_CODE(upper) != INTEGER_CST) {
            error_at(at(decl), "|c>: superposition bounds must be integer constants in this MVP");
            return;
        }
        uint64_t raw_lower = static_cast<uint64_t>(TREE_INT_CST_LOW(lower));
        uint64_t raw_upper = static_cast<uint64_t>(TREE_INT_CST_LOW(upper));
        uint64_t span = 0;
        if (var.is_signed) {
            int64_t s_lower = static_cast<int64_t>(raw_lower);
            int64_t s_upper = static_cast<int64_t>(raw_upper);
            int64_t min_s = width == 64 ? INT64_MIN : -(int64_t)(uint64_t{1} << (width - 1));
            int64_t max_s =
                width == 64 ? INT64_MAX : static_cast<int64_t>((uint64_t{1} << (width - 1)) - 1);
            if (s_lower > s_upper || s_lower < min_s || s_upper > max_s) {
                error_at(at(decl), "|c>: superposition interval does not fit the quantum register");
                return;
            }
            span = static_cast<uint64_t>(s_upper) - static_cast<uint64_t>(s_lower);
            var.lower = static_cast<uint64_t>(s_lower);
            var.upper = static_cast<uint64_t>(s_upper);
        } else {
            if (raw_lower > raw_upper || (width < 64 && raw_upper >= (uint64_t{1} << width))) {
                error_at(at(decl), "|c>: superposition interval does not fit the quantum register");
                return;
            }
            span = raw_upper - raw_lower;
            var.lower = raw_lower;
            var.upper = raw_upper;
        }
        if (var.lower != var.upper && span != UINT64_MAX && (span & (span + 1)) != 0) {
            error_at(
                at(decl),
                "|c>: uniform preparation currently supports [a, a + 2^k - 1] or a single basis value");
            return;
        }
    } else if (init) {
        var.classical_init = !has_quantum(init);
        var.expression_init = !var.classical_init;
    }
    var.original_init = DECL_INITIAL(decl);
    if (var.expression_init || var.classical_init)
        var.range_max = mask_to_width(UINT64_MAX, width);
    else if (var.is_signed &&
             ((static_cast<int64_t>(var.lower << (64 - width)) >> (64 - width)) < 0))
        var.range_max = mask_to_width(UINT64_MAX, width);
    else
        var.range_max = mask_to_width(var.upper, width);
    var.clean = !var.expression_init && !var.classical_init && var.lower == 0 && var.upper == 0;
    var.basis_known = !var.expression_init && !var.classical_init && var.lower == var.upper;
    var.pristine_uniform = !var.expression_init && !var.classical_init && var.lower != var.upper;
    variables.emplace(decl, std::move(var));
}

tree discover_declarations(tree *node, int *walk, void *) {
    if (TREE_CODE(*node) == DECL_EXPR) {
        if (ast_file) {
            tree decl = DECL_EXPR_DECL(*node);
            fprintf(ast_file,
                    "\n;; Semantic declaration %s quant=%d\n",
                    name_of(decl).c_str(),
                    quantum(decl));
            print_node(ast_file, "declaration", decl, 0, false);
        }
        record_decl(DECL_EXPR_DECL(*node), nullptr);
        *walk = 0;
    } else if (DECL_P(*node) || TYPE_P(*node))
        *walk = 0;
    return NULL_TREE;
}

#include "quantum_semantics.inc"
#include "quantum_frontend.inc"

void mark_quantum_trees(void *, void *) {
    for (const auto &entry : quantum_functions) {
        gt_ggc_mx_tree_node(entry.second.decl);
        gt_ggc_mx_tree_node(entry.second.body);
    }
    for (tree g : file_scope_quantum_vars)
        gt_ggc_mx_tree_node(g);
}

void clear_function_state() {
    active_function = NULL_TREE;
    variables.clear();
    observed_quantum_outputs.clear();
    directly_observed_quantum_outputs.clear();
    quantum_snapshot_updates.clear();
    quantum_products.clear();
    quantum_division_products.clear();
    quantum_references.clear();
    quantum_pointer_aliases.clear();
    quantum_search_pointers.clear();
    quantum_memory_copies.clear();
    quantum_value_programs.clear();
    decimal_space_fn = NULL_TREE;
    value_program_prefix = false;
    quantum_memory_sets.clear();
    quantum_indexed_accesses.clear();
    quantum_coherent_accesses.clear();
    quantum_indexed_read_indices.clear();
    quantum_heap_pointers.clear();
    quantum_heap_accesses.clear();
    quantum_heap_updates.clear();
    quantum_native_memory.clear();
    quantum_heap_byte_reads.clear();
    quantum_memory_cpu_branches.clear();
    procedure_cpu_flags.clear();
    procedure_cpu_branches.clear();
    cpu_argument_captures.clear();
    coherent_cpu_locals.clear();
    coherent_cpu_updates.clear();
    coherent_cpu_branches.clear();
    coherent_cpu_loops.clear();
    cpu_guarded_captures.clear();
    quantum_heap_borrows.clear();
    quantum_heap_queries.clear();
    indexed_expansions.clear();
    readonly_quantum_borrows.clear();
    expanded_save_exprs.clear();
    expanded_save_expr_map.clear();
    expanding_functions.clear();
    quantum_fn_pointer_targets.clear();
    quantum_owned_subexprs.clear();
    auto_clean_zeros.clear();
    quantum_byte_pun_pointers.clear();
    known_cpu_constants.clear();
    cpu_patch_locals = NULL_TREE;
    strtok_last_address = NULL_TREE;
    strtok_next_offset = 0;
    expanded_product_leaves = 0;
    counted_saves.clear();
    expression_saves.clear();
    expression_frames.clear();
    cpu_parameter_ids.clear();
    cpu_parameter_values.clear();
    active_cpu_parameters.clear();
    cpu_library_saves.clear();
    cpu_byte_saves.clear();
    coherent_expression_depth = 0;
    lazy_effect_depth = 0;
    prepare_fn = measure_fn = NULL_TREE;
    apply_fn = apply_arg_fn = NULL_TREE;
    apply_values_fn = NULL_TREE;
    read_byte_fn = NULL_TREE;
    apply_phase_fn = NULL_TREE;
    apply_index_fn = NULL_TREE;
    observe_fn = NULL_TREE;
}

void finish_function(void *event, void *) {
    tree fn = static_cast<tree>(event);
    active_function = fn;
    recorded_declarations.clear();
    if (!DECL_SAVED_TREE(fn))
        return;
    if (name_of(fn) == "main")
        saw_main_function = true;
    if (lookup_attribute("cket_intrinsic", DECL_ATTRIBUTES(fn))) {
        error_at(at(fn), "|c>: compiler intrinsics must not have C definitions");
        return;
    }
    propagate_typedef_quant(fn);
    for (tree p = DECL_ARGUMENTS(fn); p; p = DECL_CHAIN(p))
        propagate_typedef_quant(p);
    auto propagate_body = [](tree *node, int *walk, void *) -> tree {
        if (TREE_CODE(*node) == DECL_EXPR)
            propagate_typedef_quant(DECL_EXPR_DECL(*node));
        else if (TREE_CODE(*node) == BIND_EXPR)
            for (tree v = BIND_EXPR_VARS(*node); v; v = DECL_CHAIN(v))
                propagate_typedef_quant(v);
        else if (DECL_P(*node) || TYPE_P(*node))
            *walk = 0;
        return NULL_TREE;
    };
    walk_tree_without_duplicates(&DECL_SAVED_TREE(fn), propagate_body, nullptr);
    if (ast_file) {
        fprintf(ast_file, "\n;; GCC C AST before |c> lowering: %s\n", name_of(fn).c_str());
        print_node(ast_file, "function", fn, 0, false);
        print_node(ast_file, "body", DECL_SAVED_TREE(fn), 0, false);
        fprintf(ast_file, "\n;; GCC GENERIC body before lowering\n");
        print_generic_stmt(ast_file, DECL_SAVED_TREE(fn), TDF_RAW);
        fputc('\n', ast_file);
    }
    if (quantum_function(fn)) {
        capture_quantum_function(fn);
        active_function = NULL_TREE;
        return;
    }
    if (compile_only_mode) {
        active_function = NULL_TREE;
        return;
    }
    if (name_of(fn) == "main" && !file_scope_quantum_vars.empty()) {
        tree prelude = alloc_stmt_list();
        for (tree gdecl : file_scope_quantum_vars)
            if (!DECL_EXTERNAL(gdecl))
                append(&prelude, build1_loc(at(gdecl), DECL_EXPR, void_type_node, gdecl));
        append(&prelude, DECL_SAVED_TREE(fn));
        DECL_SAVED_TREE(fn) = prelude;
    }
    DECL_SAVED_TREE(fn) = expand_quantum_program(DECL_SAVED_TREE(fn));
    DECL_SAVED_TREE(fn) = normalize_quantum_saves(DECL_SAVED_TREE(fn));
    DECL_SAVED_TREE(fn) = expand_cpu_loop_jumps(DECL_SAVED_TREE(fn));
    walk_tree_without_duplicates(&DECL_SAVED_TREE(fn), discover_declarations, nullptr);
    bool has_quantum = false;
    for (const auto &item : variables)
        has_quantum |= (DECL_CONTEXT(item.first) == fn || TREE_STATIC(item.first));
    saw_measure = false;
    saw_return = false;
    remaining.clear();
    counted_saves.clear();
    expression_saves.clear();
    expression_frames.clear();
    cpu_parameter_ids.clear();
    cpu_parameter_values.clear();
    active_cpu_parameters.clear();
    coherent_expression_depth = 0;
    lazy_effect_depth = 0;
    controls.clear();
    dynamic_depth = 0;
    runtime_memory_depth = 0;
    cpu_branches.clear();
    observed_quantum_outputs.clear();
    directly_observed_quantum_outputs.clear();
    if (has_quantum) {
        walk_tree(&DECL_SAVED_TREE(fn), count_uses, nullptr, nullptr);
        observed_quantum_outputs = observed_storage_dependencies(DECL_SAVED_TREE(fn));
    }
    if (name_of(fn) != "main" && !has_quantum) {
        auto quantum_effect = [](tree *node, int *walk, void *) -> tree {
            std::string effect = intrinsic(*node);
            if (!effect.empty() && effect.compare(0, 6, "ctype:") != 0)
                return *node;
            if (DECL_P(*node) || TYPE_P(*node))
                *walk = 0;
            return NULL_TREE;
        };
        tree effect = walk_tree(&DECL_SAVED_TREE(fn), quantum_effect, nullptr, nullptr);
        if (effect)
            error_at(
                at(effect),
                "|c>: a helper with quantum effects must declare a quantum procedure or parameter");
        // Ordinary helper loops/branches must not enter the quantum IR.
    } else {
        discover_coherent_cpu_locals(DECL_SAVED_TREE(fn));
        DECL_SAVED_TREE(fn) = lower_node(DECL_SAVED_TREE(fn));
    }
    if (cpu_patch_locals) {
        tree patched = alloc_stmt_list();
        for (tree v = cpu_patch_locals; v; v = DECL_CHAIN(v))
            if (!quantum(v) && DECL_INITIAL(v))
                append(&patched, build1_loc(at(v), DECL_EXPR, void_type_node, v));
        append(&patched, DECL_SAVED_TREE(fn));
        DECL_SAVED_TREE(fn) =
            build3(BIND_EXPR, void_type_node, cpu_patch_locals, patched, NULL_TREE);
        cpu_patch_locals = NULL_TREE;
    }
    for (const auto &item : variables) {
        const auto &var = item.second;
        if ((DECL_CONTEXT(var.decl) == fn || TREE_STATIC(var.decl)) &&
            (!var.lowered || (!var.measured && !var.released && !var.moved)))
            error_at(
                at(var.decl),
                "|c>: live quantum storage must be explicitly measured/reset, or safely uncomputed before leaving main");
    }
    active_function = NULL_TREE;
    // Per-call analysis is discarded; saved function templates are explicitly
    // marked through PLUGIN_GGC_MARKING. The emitted IR contains no GCC trees.
    clear_function_state();
}

void emit_standalone_module_if_needed() {
    if (errorcount || sorrycount || ir_path.empty() || !instructions.empty() || saw_main_function ||
        quantum_functions.size() != 1)
        return;
    const QuantumFunction &qfn = quantum_functions.begin()->second;
    if (qfn.result || !qfn.decl || !qfn.body || !DECL_ARGUMENTS(qfn.decl))
        return;
    for (tree p = DECL_ARGUMENTS(qfn.decl); p; p = DECL_CHAIN(p)) {
        tree pt = TREE_TYPE(p);
        bool scalar_borrow = quantum(p);
        bool ptr_borrow = lookup_attribute("cket_quant", DECL_ATTRIBUTES(p)) &&
                          TREE_CODE(pt) == POINTER_TYPE &&
                          supported_quantum_scalar(TYPE_MAIN_VARIANT(TREE_TYPE(pt)));
        if (!scalar_borrow && !ptr_borrow)
            return;
    }
    clear_function_state();
    lowering_standalone_module = true;
    active_function = qfn.decl;
    std::map<tree, tree> bindings;
    for (tree p = DECL_ARGUMENTS(qfn.decl); p; p = DECL_CHAIN(p)) {
        tree pt = TREE_TYPE(p);
        bool is_ptr = TREE_CODE(pt) == POINTER_TYPE;
        tree scalar_type = is_ptr ? TREE_TYPE(pt) : pt;
        tree base_type = TYPE_MAIN_VARIANT(scalar_type);
        bool ro = TYPE_READONLY(scalar_type);
        tree storage = build_decl(at(p),
                                  VAR_DECL,
                                  get_identifier(name_of(p).c_str()),
                                  build_qualified_type(base_type, TYPE_QUAL_VOLATILE));
        DECL_CONTEXT(storage) = qfn.decl;
        DECL_ATTRIBUTES(storage) = tree_cons(get_identifier("cket_quant"), NULL_TREE, NULL_TREE);
        TREE_THIS_VOLATILE(storage) = TREE_SIDE_EFFECTS(storage) = 1;
        Variable var{};
        var.decl = storage;
        var.id = next_id++;
        var.width = TYPE_PRECISION(base_type);
        var.is_float = SCALAR_FLOAT_TYPE_P(base_type);
        var.is_signed = !var.is_float && !TYPE_UNSIGNED(base_type);
        var.name = name_of(p);
        var.lowered = true;
        var.range_max = var.is_float ? (var.width == 64 ? UINT64_MAX : UINT32_MAX)
                                     : mask_to_width(UINT64_MAX, var.width);
        variables.emplace(storage, var);
        instructions.push_back("{\"op\":\"qborrow\",\"id\":" + std::to_string(var.id) +
                               ",\"width\":" + std::to_string(var.width) +
                               ",\"signed\":" + (var.is_signed ? "true" : "false") +
                               (var.is_float ? ",\"float\":true" : "") +
                               ",\"name\":" + json(var.name) + ",\"ancilla\":false" +
                               (ro ? ",\"readonly\":true" : "") + '}');
        if (ro)
            readonly_quantum_borrows.insert(storage);
        if (is_ptr) {
            QuantumReference ref{};
            ref.elements = {storage};
            ref.offset = 0;
            ref.leaves_per_element = 1;
            ref.element_type = scalar_type;
            tree marker = build_decl(at(p), VAR_DECL, get_identifier(name_of(p).c_str()), pt);
            DECL_CONTEXT(marker) = qfn.decl;
            quantum_references[marker] = ref;
            bindings[p] = marker;
        } else {
            bindings[p] = storage;
        }
    }
    tree body = clone_quantum_tree(qfn.body, bindings);
    body = expand_quantum_program(body);
    body = normalize_quantum_saves(body);
    body = expand_cpu_loop_jumps(body);
    walk_tree_without_duplicates(&body, discover_declarations, nullptr);
    walk_tree(&body, count_uses, nullptr, nullptr);
    observed_quantum_outputs = observed_storage_dependencies(body);
    discover_coherent_cpu_locals(body);
    body = lower_node(body);
    tree tail = alloc_stmt_list();
    cleanup(&tail);
    clear_function_state();
    lowering_standalone_module = false;
}

void finish(void *, void *) {
    if (ast_file) {
        fclose(ast_file);
        ast_file = nullptr;
    }
    emit_standalone_module_if_needed();
    if (errorcount || sorrycount || ir_path.empty())
        return;
    std::ofstream out(ir_path);
    if (!out) {
        error("|c>: cannot write Quantum IR");
        return;
    }
    out << "{\n  \"format\": \"cket.qir\",\n  \"version\": 1,\n  \"frontend\": \"GCC C\",\n  \"symbols\": [\n";
    for (size_t i = 0; i < symbols.size(); ++i)
        out << "    " << symbols[i] << (i + 1 < symbols.size() ? ",\n" : "\n");
    out << "  ],\n  \"instructions\": [\n";
    for (size_t i = 0; i < instructions.size(); ++i)
        out << "    " << instructions[i] << (i + 1 < instructions.size() ? ",\n" : "\n");
    out << "  ]\n}\n";
}
} // namespace

int plugin_init(plugin_name_args *info, plugin_gcc_version *version) {
    if (!plugin_default_version_check(version, &gcc_version)) {
        fprintf(stderr, "|c>: GCC plugin/compiler version mismatch; rebuild the plugin\n");
        return 1;
    }
    for (int i = 0; i < info->argc; ++i) {
        std::string key = info->argv[i].key;
        if (key == "ir" && info->argv[i].value)
            ir_path = info->argv[i].value;
        else if (key == "ast" && info->argv[i].value)
            ast_path = info->argv[i].value;
        else if (key == "compile-only" && info->argv[i].value)
            compile_only_mode = std::atoi(info->argv[i].value) != 0;
        else {
            fprintf(stderr, "|c>: unknown plugin option %s\n", key.c_str());
            return 1;
        }
    }
    if (!ast_path.empty()) {
        ast_file = fopen(ast_path.c_str(), "w");
        if (!ast_file) {
            fprintf(stderr, "|c>: cannot open AST output\n");
            return 1;
        }
    }
    register_callback(info->base_name, PLUGIN_ATTRIBUTES, register_attributes, nullptr);
    register_callback(
        info->base_name, PLUGIN_FINISH_DECL, record_global_function_references, nullptr);
    register_callback(info->base_name, PLUGIN_PRE_GENERICIZE, finish_function, nullptr);
    register_callback(info->base_name, PLUGIN_GGC_MARKING, mark_quantum_trees, nullptr);
    register_callback(info->base_name, PLUGIN_FINISH, finish, nullptr);
    return 0;
}
