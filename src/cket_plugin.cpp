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
#include "real.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    bool is_signed, lowered = false, measured = false;
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
std::set<tree> quantum_heap_borrows;
std::set<tree> quantum_heap_queries;

struct QuantumMemoryCopy {
    std::vector<std::pair<tree, tree>> elements; // Destination, source, in read-before-write order.
};

std::map<tree, QuantumMemoryCopy> quantum_memory_copies;

struct QuantumValueProgram {
    std::vector<tree> prefix, updates;
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
std::set<std::string> expanding_functions;

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
std::vector<std::string> symbols, instructions;
std::string ir_path, ast_path;
tree prepare_fn, measure_fn;
tree active_function = NULL_TREE;
bool has_quantum(tree value);
FILE *ast_file = nullptr;
bool saw_measure = false;
bool saw_return = false;
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

bool quantum(tree decl) {
    return decl && (TREE_CODE(decl) == VAR_DECL || TREE_CODE(decl) == PARM_DECL) &&
           INTEGRAL_TYPE_P(TREE_TYPE(decl)) &&
           lookup_attribute("cket_quant", DECL_ATTRIBUTES(decl));
}

tree strip(tree value) {
    while (value && (CONVERT_EXPR_P(value) || TREE_CODE(value) == NON_LVALUE_EXPR))
        value = TREE_OPERAND(value, 0);
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

bool quantum_product_type(tree type) {
    if (!type || TYPE_READONLY(type))
        return false;
    if (INTEGRAL_TYPE_P(type))
        return TYPE_PRECISION(type) <= 64;
    if (TREE_CODE(type) == ARRAY_TYPE) {
        tree domain = TYPE_DOMAIN(type);
        return domain && TYPE_MAX_VALUE(domain) && tree_fits_uhwi_p(TYPE_MAX_VALUE(domain)) &&
               tree_to_uhwi(TYPE_MAX_VALUE(domain)) < 4096 && quantum_product_type(TREE_TYPE(type));
    }
    if (TREE_CODE(type) == RECORD_TYPE) {
        for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f))
            if (TREE_CODE(f) == FIELD_DECL &&
                (DECL_BIT_FIELD(f) || !quantum_product_type(TREE_TYPE(f))))
                return false;
        return true;
    }
    return false;
}

tree quantum_attribute(tree *node, tree, tree, int, bool *no_add) {
    tree decl = *node;
    if (TREE_CODE(decl) == FUNCTION_DECL) {
        tree result = TREE_TYPE(TREE_TYPE(decl));
        if (result != void_type_node && (!INTEGRAL_TYPE_P(result) || TYPE_PRECISION(result) > 64)) {
            error_at(at(decl),
                     "|c>: quantum function results currently require a C integer or bool");
            *no_add = true;
        }
        return NULL_TREE;
    }
    if (TREE_CODE(decl) == PARM_DECL) {
        tree type = TREE_TYPE(decl);
        if (TREE_CODE(type) == POINTER_TYPE && INTEGRAL_TYPE_P(TREE_TYPE(type)) &&
            TYPE_PRECISION(TREE_TYPE(type)) <= 64) {
            TREE_TYPE(decl) = build_pointer_type(build_qualified_type(
                TREE_TYPE(type), TYPE_QUALS(TREE_TYPE(type)) | TYPE_QUAL_VOLATILE));
            return NULL_TREE;
        }
        if (!INTEGRAL_TYPE_P(type) || TYPE_PRECISION(type) > 64) {
            error_at(
                at(decl),
                "|c>: quantum parameters currently borrow scalar integer registers; pointer/array parameters need an explicit borrow contract");
            *no_add = true;
            return NULL_TREE;
        }
        TREE_TYPE(decl) = build_qualified_type(type, TYPE_QUALS(type) | TYPE_QUAL_VOLATILE);
        TREE_THIS_VOLATILE(decl) = TREE_SIDE_EFFECTS(decl) = 1;
        return NULL_TREE;
    }
    tree context = DECL_P(decl) && DECL_CONTEXT(decl) ? DECL_CONTEXT(decl) : current_function_decl;
    if (TREE_CODE(decl) != VAR_DECL || !context || TREE_CODE(context) != FUNCTION_DECL ||
        TREE_STATIC(decl) || DECL_EXTERNAL(decl)) {
        error_at(at(decl), "|c>: quant currently requires an automatic local integer variable");
        *no_add = true;
        return NULL_TREE;
    }
    tree type = TREE_TYPE(decl);
    if (TREE_CODE(type) == POINTER_TYPE && INTEGRAL_TYPE_P(TREE_TYPE(type)) &&
        TYPE_PRECISION(TREE_TYPE(type)) <= 64) {
        TREE_TYPE(decl) = build_pointer_type(build_qualified_type(
            TREE_TYPE(type), TYPE_QUALS(TREE_TYPE(type)) | TYPE_QUAL_VOLATILE));
        return NULL_TREE;
    }
    if (!quantum_product_type(type)) {
        error_at(
            at(decl),
            "|c>: quant requires integers/bools, fixed arrays or integer structures; pointers, unions, bitfields and floating point need further quantum semantics");
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
    "cket_quant", 0, 0, true, false, false, false, quantum_attribute, nullptr};
const attribute_spec intrinsic_spec = {
    "cket_intrinsic", 1, 1, true, false, false, false, intrinsic_attribute, nullptr};

void register_attributes(void *, void *) {
    register_attribute(&quant_spec);
    register_attribute(&intrinsic_spec);
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
    tree context = DECL_P(decl) && DECL_CONTEXT(decl) ? DECL_CONTEXT(decl) : current_function_decl;
    if (!context)
        context = active_function;
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
    unsigned width = INTEGRAL_TYPE_P(type) ? TYPE_PRECISION(type) : 0;
    symbols.push_back("{\"name\":" + json(name_of(decl)) + ",\"function\":" +
                      json(name_of(DECL_CONTEXT(decl))) + ",\"type\":{\"c_type\":" + json(c_type) +
                      ",\"quantum\":" + (is_quantum ? "true" : "false") +
                      ",\"bits\":" + std::to_string(width) + "}}");
    if (!is_quantum)
        return;
    if (name_of(DECL_CONTEXT(decl)) != "main") {
        error_at(
            at(decl),
            "|c>: quantum locals require main or expansion through a declared quantum procedure");
        return;
    }
    Variable var{};
    var.decl = decl;
    var.id = next_id++;
    var.width = width;
    var.is_signed = !TYPE_UNSIGNED(type);
    var.name = name_of(decl);
    var.c_type = c_type;
    tree init = strip(DECL_INITIAL(decl));
    // C converts uint64_t -> _Bool as a comparison with zero.
    if (TREE_CODE(type) == BOOLEAN_TYPE && init && TREE_CODE(init) == NE_EXPR &&
        integer_zerop(TREE_OPERAND(init, 1)))
        init = strip(TREE_OPERAND(init, 0));
    if (init && TREE_CODE(init) == INTEGER_CST) {
        var.lower = var.upper = static_cast<uint64_t>(TREE_INT_CST_LOW(init));
        if (width < 64)
            var.lower = var.upper = var.lower & ((uint64_t{1} << width) - 1);
    } else if (init && intrinsic(init) == "superposition" && call_expr_nargs(init) == 2) {
        tree lower = strip(CALL_EXPR_ARG(init, 0)), upper = strip(CALL_EXPR_ARG(init, 1));
        if (TREE_CODE(lower) != INTEGER_CST || TREE_CODE(upper) != INTEGER_CST) {
            error_at(at(decl), "|c>: superposition bounds must be integer constants in this MVP");
            return;
        }
        var.lower = static_cast<uint64_t>(TREE_INT_CST_LOW(lower));
        var.upper = static_cast<uint64_t>(TREE_INT_CST_LOW(upper));
        if (var.lower > var.upper || (width < 64 && var.upper >= (uint64_t{1} << width))) {
            error_at(at(decl), "|c>: superposition interval does not fit the quantum register");
            return;
        }
        if (var.lower != var.upper && (var.lower != 0 || (var.upper & (var.upper + 1)) != 0)) {
            error_at(
                at(decl),
                "|c>: uniform preparation currently supports [0, 2^k-1] or a single basis value");
            return;
        }
        if (var.is_signed && var.upper > ((uint64_t{1} << (width - 1)) - 1)) {
            error_at(
                at(decl),
                "|c>: superposition bounds must be nonnegative values representable by the signed C type");
            return;
        }
    } else if (init) {
        var.classical_init = !has_quantum(init);
        var.expression_init = !var.classical_init;
    }
    var.original_init = DECL_INITIAL(decl);
    var.range_max = var.expression_init || var.classical_init
                        ? (width == 64 ? UINT64_MAX : ((uint64_t{1} << width) - 1))
                        : var.upper;
    var.clean = !var.expression_init && !var.classical_init && var.upper == 0;
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
}

void finish_function(void *event, void *) {
    tree fn = static_cast<tree>(event);
    active_function = fn;
    recorded_declarations.clear();
    if (!DECL_SAVED_TREE(fn))
        return;
    if (lookup_attribute("cket_intrinsic", DECL_ATTRIBUTES(fn))) {
        error_at(at(fn), "|c>: compiler intrinsics must not have C definitions");
        return;
    }
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
    DECL_SAVED_TREE(fn) = expand_quantum_program(DECL_SAVED_TREE(fn));
    DECL_SAVED_TREE(fn) = normalize_quantum_saves(DECL_SAVED_TREE(fn));
    walk_tree_without_duplicates(&DECL_SAVED_TREE(fn), discover_declarations, nullptr);
    bool has_quantum = false;
    for (const auto &item : variables)
        has_quantum |= DECL_CONTEXT(item.first) == fn;
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
    } else
        DECL_SAVED_TREE(fn) = lower_node(DECL_SAVED_TREE(fn));
    for (const auto &item : variables) {
        const auto &var = item.second;
        if (DECL_CONTEXT(var.decl) == fn &&
            (!var.lowered || (!var.measured && !var.released && !var.moved)))
            error_at(
                at(var.decl),
                "|c>: live quantum storage must be explicitly measured/reset, or safely uncomputed before leaving main");
    }
    active_function = NULL_TREE;
    // Per-call analysis is discarded; saved function templates are explicitly
    // marked through PLUGIN_GGC_MARKING. The emitted IR contains no GCC trees.
    variables.clear();
    observed_quantum_outputs.clear();
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
    quantum_heap_borrows.clear();
    quantum_heap_queries.clear();
    indexed_expansions.clear();
    readonly_quantum_borrows.clear();
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

void finish(void *, void *) {
    if (ast_file) {
        fclose(ast_file);
        ast_file = nullptr;
    }
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
    register_callback(info->base_name, PLUGIN_PRE_GENERICIZE, finish_function, nullptr);
    register_callback(info->base_name, PLUGIN_GGC_MARKING, mark_quantum_trees, nullptr);
    register_callback(info->base_name, PLUGIN_FINISH, finish, nullptr);
    return 0;
}
