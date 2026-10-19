#include "internal.h"
#include <math.h>
#include <string.h>

/* Independent validation of serialized IR, before synthesis or execution.
 * This checks all expression arms structurally; domain evaluation retains
 * C's short-circuit semantics for arithmetic definedness. */
static uint64_t mask_bits(unsigned width) {
    return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
}

static int fields(qc_context *c, const cj_doc *d, int obj, const char *const *allowed) {
    if (obj < 0 || obj >= d->count || d->tokens[obj].kind != '{')
        return qc_fail(c, "Quantum IR requires an object");
    for (int key = obj + 1; key < d->tokens[obj].next;) {
        int accepted = 0;
        for (unsigned j = 0; allowed[j]; ++j)
            accepted |= cj_equal(d, key, allowed[j]);
        if (!accepted)
            return qc_fail(c, "unknown field in Quantum IR object");
        for (int prior = obj + 1; prior < key;) {
            size_t n = d->tokens[key].end - d->tokens[key].begin;
            if (n == d->tokens[prior].end - d->tokens[prior].begin &&
                !memcmp(d->text + d->tokens[key].begin, d->text + d->tokens[prior].begin, n))
                return qc_fail(c, "duplicate field in Quantum IR object");
            prior = d->tokens[d->tokens[prior].next].next;
        }
        key = d->tokens[d->tokens[key].next].next;
    }
    return 1;
}

static int unsigned_field(qc_context *c, const cj_doc *d, int obj, const char *key, uint64_t *out) {
    int t = cj_get(d, obj, key);
    if (t < 0 || d->tokens[t].kind < '0' || d->tokens[t].kind > '9' || !cj_u64(d, t, out))
        return qc_fail(c, "missing or invalid unsigned integer: %s", key);
    return 1;
}

static int boolean_field(qc_context *c, const cj_doc *d, int obj, const char *key, int optional) {
    int t = cj_get(d, obj, key);
    if (t < 0 && optional)
        return 1;
    return (t >= 0 && (d->tokens[t].kind == 't' || d->tokens[t].kind == 'f')) ||
           qc_fail(c, "missing or invalid boolean: %s", key);
}

static int string_field(qc_context *c, const cj_doc *d, int obj, const char *key, int optional) {
    int t = cj_get(d, obj, key);
    return (t < 0 && optional) || (t >= 0 && d->tokens[t].kind == '"') ||
           qc_fail(c, "missing or invalid string: %s", key);
}

static int same_type(const cj_doc *d, int a, int b) {
    uint64_t ab = 0, bb = 0;
    return cj_u64(d, cj_get(d, a, "bits"), &ab) && cj_u64(d, cj_get(d, b, "bits"), &bb) &&
           ab == bb && cj_bool(d, cj_get(d, a, "signed")) == cj_bool(d, cj_get(d, b, "signed"));
}

static int truth_valued(const cj_doc *d, int e) {
    uint64_t bits = 0;
    cj_u64(d, cj_get(d, e, "bits"), &bits);
    if (bits == 1)
        return 1;
    int op = cj_get(d, e, "op");
    if (cj_equal(d, op, "cast")) {
        int args = cj_get(d, e, "args");
        if (args >= 0 && d->tokens[args].kind == '[' && d->tokens[args].children == 1)
            return truth_valued(d, args + 1);
    }
    const char *ops[] = {"eq", "ne", "lt", "le", "gt", "ge", "not", "land", "lor", NULL};
    for (unsigned i = 0; ops[i]; ++i)
        if (cj_equal(d, op, ops[i]))
            return 1;
    return 0;
}

int qc_verify_ctype_profile(qc_context *c, const cj_doc *d, int e, int mapping) {
    int profile = cj_get(d, e, "profile");
    if (profile < 0 || cj_equal(d, profile, "current"))
        return 1;
    if (d->tokens[profile].kind != '[' || d->tokens[profile].children > 256)
        return qc_fail(c, "ctype profile requires at most 256 immutable intervals");
    unsigned previous = 0, count = 0;
    for (int row = profile + 1; row < d->tokens[profile].next; row = d->tokens[row].next) {
        if (d->tokens[row].kind != '[' || d->tokens[row].children != 3)
            return qc_fail(c, "ctype profile interval requires lower, upper and value");
        uint64_t values[3];
        int field = row + 1;
        for (unsigned i = 0; i < 3; ++i) {
            if (d->tokens[field].kind < '0' || d->tokens[field].kind > '9' ||
                !cj_u64(d, field, &values[i]))
                return qc_fail(c, "ctype profile interval requires unsigned integers");
            field = d->tokens[field].next;
        }
        if (values[0] > values[1] || values[1] > 255 || (count && values[0] <= previous) ||
            !values[2] || values[2] > (mapping ? 255u : 2147483647u))
            return qc_fail(c, "ctype profile intervals must be sorted, disjoint and representable");
        previous = (unsigned)values[1];
        ++count;
    }
    return 1;
}

static int bounded_expression(qc_context *c, const cj_doc *d, int e, unsigned depth) {
    if (depth > 128 || d->count > 100000)
        return qc_fail(c, "quantum expression complexity budget exceeded");
    static const char *const leaf[] = {"op", "bits", "signed", "value", NULL};
    static const char *const source[] = {"op", "bits", "signed", "id", "active", "known", NULL};
    static const char *const composite[] = {"op", "bits", "signed", "args", NULL};
    static const char *const character[] = {"op", "bits", "signed", "args", "profile", NULL};
    if (e < 0 || e >= d->count || d->tokens[e].kind != '{')
        return qc_fail(c, "invalid typed quantum expression");
    int op = cj_get(d, e, "op");
    uint64_t bits;
    if (!string_field(c, d, e, "op", 0) || !unsigned_field(c, d, e, "bits", &bits) || !bits ||
        bits > 64 || !boolean_field(c, d, e, "signed", 0))
        return qc_fail(c, "invalid quantum expression type");
    int sign = cj_bool(d, cj_get(d, e, "signed"));
    if (bits == 1 && sign)
        return qc_fail(c, "one-bit quantum values use unsigned bool representation");
    if (cj_equal(d, op, "const")) {
        uint64_t v;
        int t = cj_get(d, e, "value");
        if (!fields(c, d, e, leaf) || t < 0 || !cj_u64(d, t, &v))
            return qc_fail(c, "invalid quantum constant");
        if (d->tokens[t].kind == '-') {
            uint64_t minimum = (uint64_t)(-((__int128)1 << ((unsigned)bits - 1)));
            if (!sign || v < minimum)
                return qc_fail(c, "negative constant is outside its signed C type");
        } else if ((sign && v >= (UINT64_C(1) << ((unsigned)bits - 1))) ||
                   (!sign && v > mask_bits((unsigned)bits)))
            return qc_fail(c, "constant is outside its C type");
        return 1;
    }
    if (cj_equal(d, op, "reg")) {
        uint64_t id, active = bits, known;
        if (!fields(c, d, e, source) || !unsigned_field(c, d, e, "id", &id))
            return 0;
        if (id > UINT32_MAX)
            return qc_fail(c, "quantum source identifier is out of range");
        qc_register *r = qc_lookup(c, (uint32_t)id);
        if (!r || !r->alive || r->width != bits || (r->signed_type >= 0 && r->signed_type != sign))
            return qc_fail(c, "unavailable or mistyped quantum source");
        if (cj_get(d, e, "active") >= 0 && !unsigned_field(c, d, e, "active", &active))
            return 0;
        if (active > bits ||
            ((mask_bits((unsigned)bits) ^ mask_bits((unsigned)active)) & ~r->zero_mask))
            return qc_fail(c, "inactive source bits are not independently proven zero");
        if (cj_get(d, e, "known") >= 0) {
            if (!unsigned_field(c, d, e, "known", &known))
                return 0;
            if (!r->basis_known || known != r->basis_value)
                return qc_fail(c, "claimed known basis value is not independently proven");
        }
        return 1;
    }
    int character_op = 0;
    const char *ctype_names[] = {"ctype_isalnum",
                                 "ctype_isalpha",
                                 "ctype_isblank",
                                 "ctype_iscntrl",
                                 "ctype_isdigit",
                                 "ctype_isgraph",
                                 "ctype_islower",
                                 "ctype_isprint",
                                 "ctype_ispunct",
                                 "ctype_isspace",
                                 "ctype_isupper",
                                 "ctype_isxdigit",
                                 "ctype_tolower",
                                 "ctype_toupper",
                                 NULL};
    for (unsigned i = 0; ctype_names[i]; ++i)
        if (cj_equal(d, op, ctype_names[i])) {
            character_op = 1;
            if (!qc_verify_ctype_profile(c, d, e, i >= 12))
                return 0;
        }
    if (!fields(c, d, e, character_op ? character : composite))
        return 0;
    int arity = 0, matching = 0, comparison = 0, shift = 0;
    int ctype = 0;
    const char *unary[] = {"cast",          "neg",           "bnot",
                           "not",           "abs",           "uabs",
                           "ctype_isalnum", "ctype_isalpha", "ctype_isblank",
                           "ctype_iscntrl", "ctype_isdigit", "ctype_isgraph",
                           "ctype_islower", "ctype_isprint", "ctype_ispunct",
                           "ctype_isspace", "ctype_isupper", "ctype_isxdigit",
                           "ctype_tolower", "ctype_toupper", NULL};
    const char *binary[] = {"add",  "sub", "mul",  "div", "mod",      "band", "bor",
                            "bxor", "shl", "shr",  "eq",  "ne",       "lt",   "le",
                            "gt",   "ge",  "land", "lor", "sequence", NULL};
    for (unsigned i = 0; unary[i]; ++i)
        if (cj_equal(d, op, unary[i])) {
            arity = 1;
            matching = i == 1 || i == 2 || i == 4;
            ctype = i >= 6;
            if (ctype && (bits != 32 || !sign))
                return qc_fail(c, "ctype result requires the declared signed int representation");
        }
    for (unsigned i = 0; binary[i]; ++i)
        if (cj_equal(d, op, binary[i])) {
            arity = 2;
            matching = i < 8;
            shift = i == 8 || i == 9;
            comparison = i >= 10 && i < 16;
        }
    if (cj_equal(d, op, "select"))
        arity = 3;
    if (!arity)
        return qc_fail(c, "unsupported operator in typed Quantum IR");
    if (cj_equal(d, op, "abs") && !sign)
        return qc_fail(c, "absolute value requires a signed C integer type");
    int args = cj_get(d, e, "args"), nodes[3], n = 0;
    if (args < 0 || d->tokens[args].kind != '[')
        return qc_fail(c, "missing quantum expression operands");
    for (int a = args + 1; a < d->tokens[args].next; a = d->tokens[a].next) {
        if (n == 3)
            return qc_fail(c, "invalid quantum expression arity");
        nodes[n++] = a;
    }
    if (n != arity)
        return qc_fail(c, "invalid quantum expression arity");
    for (int j = 0; j < n; ++j)
        if (!bounded_expression(c, d, nodes[j], depth + 1))
            return 0;
    if (ctype && !same_type(d, e, nodes[0]))
        return qc_fail(c, "ctype argument requires an explicit conversion to signed int");
    if (matching)
        for (int j = 0; j < n; ++j)
            if (!same_type(d, e, nodes[j]))
                return qc_fail(c, "arithmetic operand type requires an explicit C conversion");
    if (cj_equal(d, op, "uabs")) {
        uint64_t input_bits;
        if (sign || !cj_bool(d, cj_get(d, nodes[0], "signed")) ||
            !cj_u64(d, cj_get(d, nodes[0], "bits"), &input_bits) || input_bits != bits)
            return qc_fail(c, "unsigned absolute value requires a corresponding signed input type");
    }
    if (shift && !same_type(d, e, nodes[0]))
        return qc_fail(c, "shift result type differs from its left operand");
    if (comparison && !same_type(d, nodes[0], nodes[1]))
        return qc_fail(c, "comparison operands require a common C type");
    if (arity == 3 && (!same_type(d, e, nodes[1]) || !same_type(d, e, nodes[2])))
        return qc_fail(c, "conditional expression arms have incompatible types");
    if (cj_equal(d, op, "sequence") && !same_type(d, e, nodes[1]))
        return qc_fail(c, "sequence result type differs from its final expression");
    return 1;
}

static int expression(qc_context *c, const cj_doc *d, int e) {
    return bounded_expression(c, d, e, 0);
}

int qc_verify_expression(qc_context *c, const cj_doc *d, int e) {
    return expression(c, d, e);
}

int qc_verify_document(qc_context *c, const cj_doc *d) {
    static const char *const allowed[] = {
        "format", "version", "frontend", "symbols", "instructions", NULL};
    uint64_t version;
    if (!fields(c, d, 0, allowed) || !string_field(c, d, 0, "format", 0) ||
        !cj_equal(d, cj_get(d, 0, "format"), "cket.qir") ||
        !unsigned_field(c, d, 0, "version", &version) || version != 1)
        return qc_fail(c, "unsupported |c> Quantum IR format/version");
    if (!string_field(c, d, 0, "frontend", 1))
        return 0;
    int instructions = cj_get(d, 0, "instructions"), symbols = cj_get(d, 0, "symbols");
    if (instructions < 0 || d->tokens[instructions].kind != '[' ||
        (symbols >= 0 && d->tokens[symbols].kind != '['))
        return qc_fail(c, "Quantum IR instructions/symbols require arrays");
    int nesting[256];
    unsigned depth = 0;
    for (int t = instructions + 1; t < d->tokens[instructions].next; t = d->tokens[t].next) {
        int op = cj_get(d, t, "op"), begin = 0, end = 0;
        if (cj_equal(d, op, "cloop_begin"))
            begin = 1;
        else if (cj_equal(d, op, "ccontrol_begin"))
            begin = 2;
        else if (cj_equal(d, op, "cloop_end"))
            end = 1;
        else if (cj_equal(d, op, "ccontrol_end"))
            end = 2;
        else if (cj_equal(d, op, "ccontrol_else")) {
            if (!depth || nesting[depth - 1] != 2)
                return qc_fail(c, "CPU else marker requires a matching first arm");
            nesting[depth - 1] = 3;
        }
        if (begin) {
            if (depth == 256)
                return qc_fail(c, "too many nested CPU control regions");
            nesting[depth++] = begin;
        } else if (end) {
            if (!depth || (nesting[depth - 1] != end && !(end == 2 && nesting[depth - 1] == 3)))
                return qc_fail(c, "unbalanced CPU control markers");
            --depth;
        }
    }
    return !depth || qc_fail(c, "unterminated CPU control region");
}

#include "phase_verify.inc"

int qc_verify_instruction(qc_context *c, const cj_doc *d, int inst) {
    if (cj_equal(d, cj_get(d, inst, "op"), "qphi")) {
        static const char *const phi[] = {
            "op", "id", "width", "signed", "sources", "condition", NULL};
        uint64_t id, width, value, bits;
        int sources = cj_get(d, inst, "sources"), condition = cj_get(d, inst, "condition");
        if (!fields(c, d, inst, phi) || !unsigned_field(c, d, inst, "id", &id) || id > UINT32_MAX ||
            qc_lookup(c, (uint32_t)id) || !unsigned_field(c, d, inst, "width", &width) || !width ||
            width > 64 || !boolean_field(c, d, inst, "signed", 0) || sources < 0 ||
            d->tokens[sources].kind != '[' || d->tokens[sources].children != 2 ||
            !cj_equal(d, cj_get(d, condition, "op"), "const") || !expression(c, d, condition) ||
            !unsigned_field(c, d, condition, "bits", &bits) || bits != 1 ||
            !unsigned_field(c, d, condition, "value", &value) || value > 1)
            return qc_fail(c, "CPU phi requires a fresh owner and a bound Boolean condition");
        uint64_t seen = UINT64_MAX;
        unsigned index = 0;
        for (int token = sources + 1; token < d->tokens[sources].next;
             token = d->tokens[token].next, ++index) {
            uint64_t source;
            if (!cj_u64(d, token, &source) || source > UINT32_MAX || source == seen)
                return qc_fail(c, "invalid or duplicate CPU phi source");
            qc_register *r = qc_lookup(c, (uint32_t)source);
            if ((index == (value ? 0u : 1u) && (!r || !r->alive || !r->prepared || r->finished)) ||
                (r && r->alive &&
                 (r->width != width || r->signed_type != cj_bool(d, cj_get(d, inst, "signed")))))
                return qc_fail(c, "CPU phi needs available sources of the same C representation");
            seen = source;
        }
        return 1;
    }
    if (cj_equal(d, cj_get(d, inst, "op"), "qmodule")) {
        static const char *const module[] = {"op", "circuit", "ids", "inverse", "controls", NULL};
        if (!fields(c, d, inst, module) || !boolean_field(c, d, inst, "inverse", 0))
            return 0;
        int body = cj_get(d, inst, "circuit"), ids = cj_get(d, inst, "ids"),
            controls = cj_get(d, inst, "controls");
        if (body < 0 || d->tokens[body].kind != '{' || ids < 0 || d->tokens[ids].kind != '[' ||
            d->tokens[ids].children > QC_MAX_PHYSICAL ||
            (controls >= 0 &&
             (d->tokens[controls].kind != '[' || d->tokens[controls].children > QC_MAX_PHYSICAL)))
            return qc_fail(c, "invalid circuit module binding schema");
        for (int t = ids + 1; t < d->tokens[ids].next; t = d->tokens[t].next) {
            uint64_t id;
            if (!cj_u64(d, t, &id) || id > UINT32_MAX)
                return qc_fail(c, "invalid circuit module register identity");
        }
        if (controls >= 0)
            for (int t = controls + 1; t < d->tokens[controls].next; t = d->tokens[t].next) {
                static const char *const control[] = {"id", "value", NULL};
                uint64_t id, value;
                if (!fields(c, d, t, control) || !unsigned_field(c, d, t, "id", &id) ||
                    !unsigned_field(c, d, t, "value", &value) || id > UINT32_MAX || value > 1)
                    return qc_fail(c, "invalid circuit module control schema");
            }
        return 1;
    }
    static const char *const alloc[] = {"op", "id", "width", "signed", "name", "ancilla", NULL};
    static const char *const prepare[] = {"op", "id", "lower", "upper", "dynamic", "expr", NULL};
    static const char *const computation[] = {"op", "id", "expr", "controls", "saved", NULL};
    static const char *const addition[] = {
        "op", "id", "value", "argument", "dynamic", "controls", NULL};
    static const char *const register_addition[] = {
        "op", "id", "source", "subtract", "controls", NULL};
    static const char *const xor_update[] = {
        "op", "id", "expr", "value", "dynamic", "controls", NULL};
    static const char *const phase[] = {"op", "angle", "angle_expr", "dynamic", "controls", NULL};
    static const char *const fourier[] = {"op", "id", "inverse", "controls", NULL};
    static const char *const measure[] = {"op", "id", "width", "result", NULL};
    static const char *const simple[] = {"op", "id", NULL};
    static const char *const move[] = {"op", "id", "from", "to", NULL};
    static const char *const find[] = {"op", "id", "upper", "predicate", "solutions", NULL};
    static const char *const search[] = {"op", "id", "upper", "predicate", "attempts", NULL};
    static const char *const marker[] = {"op", NULL};
    static const char *const indexed[] = {"op", "ids", "action", NULL};
    if (inst < 0 || inst >= d->count || d->tokens[inst].kind != '{' ||
        !string_field(c, d, inst, "op", 0))
        return qc_fail(c, "invalid quantum instruction object");
    int op = cj_get(d, inst, "op");
    const char *const *allowed = NULL;
    if (cj_equal(d, op, "qindexed")) {
        if (!fields(c, d, inst, indexed))
            return 0;
        int ids = cj_get(d, inst, "ids"), action = cj_get(d, inst, "action"),
            kind = cj_get(d, action, "op");
        if (ids < 0 || d->tokens[ids].kind != '[' || !d->tokens[ids].children ||
            d->tokens[ids].children > 64)
            return qc_fail(c, "indexed storage requires 1..64 candidate registers");
        int integer = cj_equal(d, kind, "qadd_inplace") || cj_equal(d, kind, "qxor_inplace");
        if ((!integer && !cj_equal(d, kind, "qmeasure") && !cj_equal(d, kind, "qreset") &&
             !cj_equal(d, kind, "qfourier")) ||
            cj_get(d, action, "controls") >= 0 ||
            (integer && !cj_bool(d, cj_get(d, action, "dynamic"))))
            return qc_fail(c, "unsupported indexed action or coherent controls");
        uint64_t first = 0, action_id, seen[64];
        unsigned count = 0;
        qc_register *base = NULL;
        if (!unsigned_field(c, d, action, "id", &action_id))
            return 0;
        for (int t = ids + 1; t < d->tokens[ids].next; t = d->tokens[t].next) {
            uint64_t id;
            if (!cj_u64(d, t, &id) || id > UINT32_MAX)
                return qc_fail(c, "invalid indexed storage identifier");
            for (unsigned j = 0; j < count; ++j)
                if (seen[j] == id)
                    return qc_fail(c, "duplicate indexed storage identifier");
            seen[count++] = id;
            qc_register *candidate = qc_lookup(c, (uint32_t)id);
            if (!candidate || !candidate->alive || !candidate->prepared || candidate->definition)
                return qc_fail(
                    c,
                    "indexed candidate must be available prepared storage without a pending inverse");
            if (!base) {
                base = candidate;
                first = id;
            } else if (candidate->width != base->width ||
                       candidate->signed_type != base->signed_type)
                return qc_fail(c, "indexed candidates must have the same integer representation");
        }
        if (action_id != first)
            return qc_fail(c, "indexed action must identify the first candidate");
        return qc_verify_instruction(c, d, action);
    }
    if (cj_equal(d, op, "qalloc"))
        allowed = alloc;
    else if (cj_equal(d, op, "qborrow"))
        allowed = alloc;
    else if (cj_equal(d, op, "qprepare"))
        allowed = prepare;
    else if (cj_equal(d, op, "qcompute") || cj_equal(d, op, "quncompute") ||
             cj_equal(d, op, "qinplace"))
        allowed = computation;
    else if (cj_equal(d, op, "qadd_inplace"))
        allowed = addition;
    else if (cj_equal(d, op, "qadd_register"))
        allowed = register_addition;
    else if (cj_equal(d, op, "qxor_inplace"))
        allowed = xor_update;
    else if (cj_equal(d, op, "qphase"))
        allowed = phase;
    else if (cj_equal(d, op, "qfourier"))
        allowed = fourier;
    else if (cj_equal(d, op, "qmeasure") || cj_equal(d, op, "qobserve"))
        allowed = measure;
    else if (cj_equal(d, op, "qrelease") || cj_equal(d, op, "qreset") ||
             cj_equal(d, op, "qretain") || cj_equal(d, op, "qhistory_end") ||
             cj_equal(d, op, "qdispose"))
        allowed = simple;
    else if (cj_equal(d, op, "qmove"))
        allowed = move;
    else if (cj_equal(d, op, "qfind"))
        allowed = find;
    else if (cj_equal(d, op, "qsearch"))
        allowed = search;
    else if (cj_equal(d, op, "cloop_begin") || cj_equal(d, op, "cloop_end") ||
             cj_equal(d, op, "ccontrol_begin") || cj_equal(d, op, "ccontrol_end") ||
             cj_equal(d, op, "ccontrol_else") || cj_equal(d, op, "ctype_require") ||
             cj_equal(d, op, "ctype_capture"))
        allowed = marker;
    else
        return qc_fail(c, "unsupported quantum instruction");
    if (!fields(c, d, inst, allowed))
        return 0;
    if (allowed == marker)
        return 1;
    if (allowed == phase) {
        int angle = cj_get(d, inst, "angle");
        if (!boolean_field(c, d, inst, "dynamic", 1))
            return 0;
        if (angle >= 0 && cj_bool(d, cj_get(d, inst, "dynamic")))
            return qc_fail(c, "ambiguous dynamic/static phase angle");
        if (angle >= 0 && !isfinite(cj_double(d, angle)))
            return qc_fail(c, "invalid finite numeric phase angle");
        int expr = cj_get(d, inst, "angle_expr");
        if (expr >= 0) {
            if (angle >= 0 || cj_bool(d, cj_get(d, inst, "dynamic")))
                return qc_fail(c, "ambiguous deferred phase angle");
            if (!phase_expression(c, d, expr, 0))
                return 0;
        }
    } else {
        uint64_t id;
        if (!unsigned_field(c, d, inst, "id", &id) || id > UINT32_MAX)
            return qc_fail(c, "invalid quantum storage identifier");
        if (allowed == alloc) {
            uint64_t width;
            if (!unsigned_field(c, d, inst, "width", &width))
                return 0;
            if (!width || width > 64)
                return qc_fail(c, "quantum register width must be 1..64");
            if (!boolean_field(c, d, inst, "signed", 1) ||
                !boolean_field(c, d, inst, "ancilla", 1) || !string_field(c, d, inst, "name", 1))
                return 0;
            if (cj_equal(d, op, "qborrow") && !boolean_field(c, d, inst, "signed", 0))
                return 0;
            return !(width == 1 && cj_bool(d, cj_get(d, inst, "signed"))) ||
                   qc_fail(c, "one-bit storage uses unsigned bool representation");
        }
        qc_register *r = qc_lookup(c, (uint32_t)id);
        // An optional owner may never have been created on this CPU path.
        if (cj_equal(d, op, "qdispose"))
            return 1;
        if (!r || !r->alive)
            return qc_fail(c, "unavailable quantum storage");
        if (allowed == fourier) {
            if (!boolean_field(c, d, inst, "inverse", 1))
                return 0;
        } else if (allowed == prepare) {
            uint64_t low, high;
            if (!boolean_field(c, d, inst, "dynamic", 1))
                return 0;
            int expr = cj_get(d, inst, "expr");
            if (expr >= 0) {
                uint64_t bits;
                if (cj_get(d, inst, "lower") >= 0 || cj_get(d, inst, "upper") >= 0 ||
                    cj_bool(d, cj_get(d, inst, "dynamic")) ||
                    !cj_equal(d, cj_get(d, expr, "op"), "const") || !expression(c, d, expr) ||
                    !unsigned_field(c, d, expr, "bits", &bits) || bits != r->width ||
                    (r->signed_type >= 0 &&
                     cj_bool(d, cj_get(d, expr, "signed")) != r->signed_type))
                    return qc_fail(
                        c, "basis preparation requires a matching bound classical constant");
            } else if (cj_bool(d, cj_get(d, inst, "dynamic"))) {
                if (cj_get(d, inst, "lower") >= 0 || cj_get(d, inst, "upper") >= 0)
                    return qc_fail(c, "ambiguous runtime/static preparation");
            } else if (!unsigned_field(c, d, inst, "lower", &low) ||
                       !unsigned_field(c, d, inst, "upper", &high))
                return 0;
        } else if (allowed == computation) {
            int expr = cj_get(d, inst, "expr");
            int saved = cj_get(d, inst, "saved");
            if (saved >= 0) {
                if (!cj_equal(d, op, "quncompute") || !boolean_field(c, d, inst, "saved", 0) ||
                    !cj_bool(d, saved) || expr >= 0 || !r->definition)
                    return qc_fail(
                        c,
                        "saved inverse requires an existing definition and no explicit expression");
            } else {
                uint64_t bits;
                if (!expression(c, d, expr) || !unsigned_field(c, d, expr, "bits", &bits))
                    return 0;
                int sign = cj_bool(d, cj_get(d, expr, "signed"));
                if ((bits != r->width || (r->signed_type >= 0 && sign != r->signed_type)) &&
                    !(r->width == 1 && truth_valued(d, expr)))
                    return qc_fail(c,
                                   "quantum output requires an explicit conversion to its C type");
            }
        } else if (allowed == register_addition) {
            uint64_t source;
            if (!unsigned_field(c, d, inst, "source", &source) || source > UINT32_MAX ||
                !boolean_field(c, d, inst, "subtract", 1))
                return qc_fail(c, "invalid quantum addition source");
            qc_register *s = qc_lookup(c, (uint32_t)source);
            if (!s || !s->alive || s == r || s->signed_type != 0 || r->signed_type != 0 ||
                r->width == 1)
                return qc_fail(
                    c, "register addition requires separate explicitly unsigned integer storage");
        } else if (allowed == xor_update) {
            int expr = cj_get(d, inst, "expr"), v = cj_get(d, inst, "value");
            uint64_t value, bits;
            if (!boolean_field(c, d, inst, "dynamic", 1))
                return 0;
            int dynamic = cj_bool(d, cj_get(d, inst, "dynamic"));
            if ((expr >= 0) + (v >= 0) + dynamic != 1)
                return qc_fail(c, "XOR requires one unambiguous operand");
            if (v >= 0 && !cj_u64(d, v, &value))
                return qc_fail(c, "invalid XOR operand");
            if (expr >= 0 &&
                (!expression(c, d, expr) || !unsigned_field(c, d, expr, "bits", &bits) ||
                 bits != r->width ||
                 (r->signed_type >= 0 && cj_bool(d, cj_get(d, expr, "signed")) != r->signed_type)))
                return qc_fail(c, "XOR operand requires an explicit target-type conversion");
            if (r->width == 1)
                return qc_fail(
                    c, "boolean compound XOR requires a separately proved truth conversion");
        } else if (allowed == addition) {
            if (!boolean_field(c, d, inst, "dynamic", 1))
                return 0;
            uint64_t value;
            int v = cj_get(d, inst, "value"), a = cj_get(d, inst, "argument");
            if ((v >= 0 && !cj_u64(d, v, &value)) || (a >= 0 && !cj_u64(d, a, &value)))
                return qc_fail(c, "invalid modular addition operand");
            if (v >= 0 && a >= 0)
                return qc_fail(c, "ambiguous modular addition operands");
            if (v >= 0 && cj_bool(d, cj_get(d, inst, "dynamic")))
                return qc_fail(c, "dynamic addition cannot contain a static value");
        } else if (allowed == measure) {
            uint64_t width;
            if (cj_get(d, inst, "width") >= 0 &&
                (!unsigned_field(c, d, inst, "width", &width) || width != r->width))
                return qc_fail(c, "measurement width differs from quantum storage");
            if (!string_field(c, d, inst, "result", 1))
                return 0;
        } else if (allowed == move) {
            if (!string_field(c, d, inst, "from", 0) || !string_field(c, d, inst, "to", 0))
                return 0;
        } else if (allowed == find || allowed == search) {
            uint64_t upper;
            int predicate = cj_get(d, inst, "predicate");
            if (!unsigned_field(c, d, inst, "upper", &upper) || !expression(c, d, predicate))
                return 0;
            if (!truth_valued(d, predicate))
                return qc_fail(c, "find predicate requires a boolean C conversion");
            uint64_t budget;
            if (allowed == find && cj_get(d, inst, "solutions") >= 0 &&
                (!unsigned_field(c, d, inst, "solutions", &budget) || !budget ||
                 budget > upper + 1))
                return qc_fail(c, "invalid solution count promise");
            if (allowed == search &&
                (!unsigned_field(c, d, inst, "attempts", &budget) || !budget || budget > 4096))
                return qc_fail(c, "invalid search attempt budget");
        }
    }
    int list = cj_get(d, inst, "controls");
    if (list >= 0) {
        static const char *const control[] = {"id", "value", NULL};
        if (d->tokens[list].kind != '[')
            return qc_fail(c, "invalid quantum control list");
        for (int t = list + 1; t < d->tokens[list].next; t = d->tokens[t].next) {
            uint64_t id, v;
            if (!fields(c, d, t, control) || !unsigned_field(c, d, t, "id", &id) ||
                id > UINT32_MAX || !unsigned_field(c, d, t, "value", &v) || v > 1)
                return qc_fail(c, "invalid quantum control");
        }
    }
    return 1;
}
