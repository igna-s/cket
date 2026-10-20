/* Register circuits, with compute/copy/uncompute. No input truth table.
 * A recorded forward plan makes the cleanup exact for arbitrary amplitudes. */
#define QC_ARITH_WORK 2000000u

typedef struct {
    qc_mask wire;
    int inverted;
} ar_bit;

typedef struct {
    unsigned width;
    int sign;
    ar_bit bit[64];
} ar_value;

typedef struct {
    __int128 lo, hi;
    unsigned width;
    int sign, valid, nonzero;
} ar_range;

typedef struct ar_fact {
    int expression, wanted;
    const struct ar_fact *next;
} ar_fact;

typedef struct ar_branch {
    int expression;
    ar_range *owner, *arm[2];
    struct ar_branch *next;
} ar_branch;

typedef struct {
    qc_context *c;
    const cj_doc *d;
    ar_range *ranges;
    qc_gate *gates;
    size_t count, capacity;
    uint32_t ids[512];
    unsigned nids, work;
    const ar_fact *facts;
    ar_branch *branches;
    size_t branch_bytes;
} ar_builder;

static ar_bit ar_at(const ar_value *v, unsigned b) {
    return b < v->width ? v->bit[b] : v->sign ? v->bit[v->width - 1] : (ar_bit){0, 0};
}

static __int128 ar_abs(__int128 value) {
    return value < 0 ? -value : value;
}

static int ar_nonzero(ar_builder *p, int e, const ar_range *r) {
    if (r->nonzero || r->lo > 0 || r->hi < 0)
        return 1;
    for (unsigned b = 0; b < r->width; ++b) {
        bit_poly bit;
        qc_mask wire;
        int inverted;
        if (expr_bit(p->c, p->d, e, b, &bit, 0) && poly_literal(&bit, &wire, &inverted) && !wire &&
            inverted)
            return 1;
    }
    return 0;
}

/* Facts describe pure values on a branch, never measurements. Strip only
 * conversions preserving the zero/nonzero distinction; narrowing does not. */
static int ar_truth_node(ar_builder *p, int e) {
    for (unsigned depth = 0; depth < 128; ++depth) {
        if (++p->work > QC_ARITH_WORK) {
            qc_fail(p->c, "arithmetic proof work budget exceeded");
            return e;
        }
        if (!cj_equal(p->d, cj_get(p->d, e, "op"), "cast"))
            break;
        int nodes[3];
        expr_nodes(p->d, e, nodes);
        uint64_t to, from;
        cj_u64(p->d, cj_get(p->d, e, "bits"), &to);
        cj_u64(p->d, cj_get(p->d, nodes[0], "bits"), &from);
        if (to != 1 && to < from)
            break;
        e = nodes[0];
    }
    return e;
}

static int ar_same(ar_builder *p, int a, int b, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "arithmetic proof work budget exceeded");
    if (a == b)
        return 1;
    int ao = cj_get(p->d, a, "op"), bo = cj_get(p->d, b, "op");
    cj_token at = p->d->tokens[ao], bt = p->d->tokens[bo];
    if (at.end - at.begin != bt.end - bt.begin ||
        memcmp(p->d->text + at.begin, p->d->text + bt.begin, at.end - at.begin))
        return 0;
    uint64_t aw, bw;
    cj_u64(p->d, cj_get(p->d, a, "bits"), &aw);
    cj_u64(p->d, cj_get(p->d, b, "bits"), &bw);
    if (aw != bw ||
        cj_bool(p->d, cj_get(p->d, a, "signed")) != cj_bool(p->d, cj_get(p->d, b, "signed")))
        return 0;
    if (ctype_profile(p->d, ao) && !ctype_same_profile(p->c, p->d, a, b))
        return 0;
    if (cj_equal(p->d, ao, "reg") || cj_equal(p->d, ao, "const")) {
        const char *field = cj_equal(p->d, ao, "reg") ? "id" : "value";
        uint64_t av, bv;
        cj_u64(p->d, cj_get(p->d, a, field), &av);
        cj_u64(p->d, cj_get(p->d, b, field), &bv);
        return cj_equal(p->d, ao, "reg")
                   ? av == bv
                   : (av & bits_mask((unsigned)aw)) == (bv & bits_mask((unsigned)bw));
    }
    int aa[3], bb[3];
    unsigned n = expr_nodes(p->d, a, aa);
    if (n != expr_nodes(p->d, b, bb))
        return 0;
    for (unsigned j = 0; j < n; ++j)
        if (!ar_same(p, aa[j], bb[j], depth + 1))
            return 0;
    return 1;
}

/* A saved, unconditional Boolean definition can certify an identity or its
 * complement. Its live dependency epochs must still match; partial controls
 * and general functions do not receive this certificate. */
static int ar_register_boolean_alias(
    ar_builder *p, uint32_t id, uint32_t *source, int *inverted, unsigned depth);

static int ar_boolean_alias(
    ar_builder *p, const cj_doc *d, int e, uint32_t *id, int *inverted, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "Boolean alias proof work/depth budget exceeded");
    int op = cj_get(d, e, "op"), nodes[3];
    unsigned n = expr_nodes(d, e, nodes);
    uint64_t width;
    if (!cj_u64(d, cj_get(d, e, "bits"), &width))
        return 0;
    if (cj_equal(d, op, "cast") && n == 1) {
        uint64_t from;
        if (!cj_u64(d, cj_get(d, nodes[0], "bits"), &from) || (width != 1 && width < from))
            return 0;
        return ar_boolean_alias(p, d, nodes[0], id, inverted, depth + 1);
    }
    if ((cj_equal(d, op, "not") || (cj_equal(d, op, "bnot") && width == 1)) && n == 1) {
        *inverted ^= 1;
        return ar_boolean_alias(p, d, nodes[0], id, inverted, depth + 1);
    }
    if ((cj_equal(d, op, "eq") || cj_equal(d, op, "ne")) && n == 2) {
        for (unsigned side = 0; side < 2; ++side) {
            uint64_t value, bits;
            if (cj_equal(d, cj_get(d, nodes[side], "op"), "const") &&
                cj_u64(d, cj_get(d, nodes[side], "value"), &value) &&
                cj_u64(d, cj_get(d, nodes[side], "bits"), &bits) &&
                !(value & bits_mask((unsigned)bits))) {
                *inverted ^= cj_equal(d, op, "eq");
                return ar_boolean_alias(p, d, nodes[!side], id, inverted, depth + 1);
            }
        }
        return 0;
    }
    uint64_t value;
    if (width != 1 || !cj_equal(d, op, "reg") || !cj_u64(d, cj_get(d, e, "id"), &value))
        return 0;
    return ar_register_boolean_alias(p, (uint32_t)value, id, inverted, depth + 1);
}

static int ar_register_boolean_alias(
    ar_builder *p, uint32_t id, uint32_t *source, int *inverted, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "Boolean alias proof work/depth budget exceeded");
    qc_register *r = reg(p->c, id);
    if (!r || !r->alive || !r->prepared || r->width != 1)
        return 0;
    *source = r->id;
    if (!r->definition || r->definition_mask)
        return 1;
    for (unsigned j = 0; j < r->ndeps; ++j) {
        qc_register *input = reg(p->c, r->deps[j]);
        if (!input || !input->alive || input->epoch != r->epochs[j])
            return 0;
    }
    cj_doc local = {0};
    const cj_doc *saved = parsed_operation(p->c, r->definition, &local);
    uint32_t original = r->id;
    int polarity = *inverted;
    int found = saved && ar_boolean_alias(p, saved, 0, &original, &polarity, depth + 1);
    cj_free(&local);
    if (found) {
        *source = original;
        *inverted = polarity;
    }
    return !*p->c->error;
}

static int ar_truth_fact(ar_builder *p, int e, const ar_fact *fact, int *wanted) {
    *wanted = fact->wanted;
    if (ar_same(p, e, fact->expression, 0))
        return 1;
    uint32_t value_id, fact_id;
    int value_inverted = 0, fact_inverted = 0;
    if (!ar_boolean_alias(p, p->d, e, &value_id, &value_inverted, 0) ||
        !ar_boolean_alias(p, p->d, fact->expression, &fact_id, &fact_inverted, 0) ||
        value_id != fact_id)
        return 0;
    *wanted ^= value_inverted ^ fact_inverted;
    return 1;
}

static int ar_register_boolean_constant(ar_builder *p, uint32_t id, int *value, unsigned depth);

/* Certify a fixed truth value without evaluating the statevector. Definitions
 * remain conditional on every recorded input epoch, even for constants. */
static int ar_boolean_constant(ar_builder *p, const cj_doc *d, int e, int *value, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "Boolean constant proof work/depth budget exceeded");
    int op = cj_get(d, e, "op"), nodes[3];
    unsigned n = expr_nodes(d, e, nodes);
    uint64_t width, raw;
    if (!cj_u64(d, cj_get(d, e, "bits"), &width))
        return 0;
    if (cj_equal(d, op, "const") && cj_u64(d, cj_get(d, e, "value"), &raw)) {
        *value = (raw & bits_mask((unsigned)width)) != 0;
        return 1;
    }
    if (cj_equal(d, op, "reg") && width == 1 && cj_u64(d, cj_get(d, e, "id"), &raw))
        return ar_register_boolean_constant(p, (uint32_t)raw, value, depth + 1);
    if (cj_equal(d, op, "cast") && n == 1) {
        uint64_t from;
        if (!cj_u64(d, cj_get(d, nodes[0], "bits"), &from) || (width != 1 && width < from))
            return 0;
        return ar_boolean_constant(p, d, nodes[0], value, depth + 1);
    }
    if ((cj_equal(d, op, "not") || (cj_equal(d, op, "bnot") && width == 1)) && n == 1) {
        if (!ar_boolean_constant(p, d, nodes[0], value, depth + 1))
            return 0;
        *value ^= 1;
        return 1;
    }
    if (cj_equal(d, op, "select") && n == 3) {
        int decision;
        if (!ar_boolean_constant(p, d, nodes[0], &decision, depth + 1))
            return 0;
        return ar_boolean_constant(p, d, nodes[decision ? 1 : 2], value, depth + 1);
    }
    uint32_t source;
    int inverted = 0;
    if (!ar_boolean_alias(p, d, e, &source, &inverted, depth + 1))
        return 0;
    qc_register *original = reg(p->c, source);
    if (!original || !original->basis_known)
        return 0;
    *value = (original->basis_value != 0) ^ inverted;
    return 1;
}

static int ar_register_boolean_constant(ar_builder *p, uint32_t id, int *value, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "Boolean constant proof work/depth budget exceeded");
    qc_register *r = reg(p->c, id);
    if (!r || !r->alive || !r->prepared || r->width != 1)
        return 0;
    if (r->basis_known) {
        *value = r->basis_value != 0;
        return 1;
    }
    if (!r->definition || r->definition_mask)
        return 0;
    for (unsigned j = 0; j < r->ndeps; ++j) {
        qc_register *input = reg(p->c, r->deps[j]);
        if (!input || !input->alive || input->epoch != r->epochs[j])
            return 0;
    }
    cj_doc local = {0};
    const cj_doc *saved = parsed_operation(p->c, r->definition, &local);
    int found = saved && ar_boolean_constant(p, saved, 0, value, depth + 1);
    cj_free(&local);
    return found;
}

static int ar_zero_constant(ar_builder *p, int e) {
    uint64_t value, width;
    return cj_equal(p->d, cj_get(p->d, e, "op"), "const") &&
           cj_u64(p->d, cj_get(p->d, e, "value"), &value) &&
           cj_u64(p->d, cj_get(p->d, e, "bits"), &width) && !(value & bits_mask((unsigned)width));
}

static int ar_push_truth(ar_builder *p,
                         int e,
                         int wanted,
                         ar_fact *facts,
                         unsigned *count,
                         const ar_fact **head,
                         unsigned depth) {
    if (depth > 128 || *count == 128)
        return qc_fail(p->c, "arithmetic branch fact budget exceeded");
    e = ar_truth_node(p, e);
    facts[*count] = (ar_fact){e, wanted, *head};
    *head = &facts[(*count)++];
    int op = cj_get(p->d, e, "op"), nodes[3];
    unsigned n = expr_nodes(p->d, e, nodes);
    if (cj_equal(p->d, op, "not"))
        return ar_push_truth(p, nodes[0], !wanted, facts, count, head, depth + 1);
    if (cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne")) {
        int other = ar_zero_constant(p, nodes[1])   ? nodes[0]
                    : ar_zero_constant(p, nodes[0]) ? nodes[1]
                                                    : -1;
        if (other >= 0)
            return ar_push_truth(
                p, other, wanted ^ cj_equal(p->d, op, "eq"), facts, count, head, depth + 1);
    }
    if (n == 2 &&
        ((wanted && cj_equal(p->d, op, "land")) || (!wanted && cj_equal(p->d, op, "lor"))))
        return ar_push_truth(p, nodes[0], wanted, facts, count, head, depth + 1) &&
               ar_push_truth(p, nodes[1], wanted, facts, count, head, depth + 1);
    return !*p->c->error;
}

static int ar_known_truth(ar_range value) {
    return value.nonzero || value.lo > 0 || value.hi < 0 ? 1
           : value.lo == 0 && value.hi == 0              ? 0
                                                         : -1;
}

/* Order facts retain the exact typed expression. Unlike truth facts, casts
 * changing signedness cannot simply be stripped for a numeric inequality. */
static int ar_order_fact(ar_builder *p, int e, ar_range *range, const ar_fact *fact) {
    static const char *const ops[] = {"eq", "ne", "lt", "le", "gt", "ge"};
    int op = cj_get(p->d, fact->expression, "op"), relation = -1, nodes[3];
    for (unsigned j = 0; j < 6; ++j)
        if (cj_equal(p->d, op, ops[j]))
            relation = (int)j;
    if (relation < 0)
        return 1;
    expr_nodes(p->d, fact->expression, nodes);
    unsigned constant_side = cj_equal(p->d, cj_get(p->d, nodes[1], "op"), "const") ? 1 : 0;
    if (!cj_equal(p->d, cj_get(p->d, nodes[constant_side], "op"), "const") ||
        !ar_same(p, e, nodes[!constant_side], 0))
        return !*p->c->error;
    uint64_t value, width;
    cj_u64(p->d, cj_get(p->d, nodes[constant_side], "value"), &value);
    cj_u64(p->d, cj_get(p->d, nodes[constant_side], "bits"), &width);
    __int128 bound = numeric(
        value, (unsigned)width, cj_bool(p->d, cj_get(p->d, nodes[constant_side], "signed")));
    if (!constant_side && relation >= 2)
        relation = relation < 4 ? relation + 2 : relation - 2;
    if (!fact->wanted) {
        static const unsigned opposite[] = {1, 0, 5, 4, 3, 2};
        relation = (int)opposite[relation];
    }
    if (relation == 0) {
        if (range->lo < bound)
            range->lo = bound;
        if (range->hi > bound)
            range->hi = bound;
    } else if (relation == 1) {
        if (range->lo == bound)
            ++range->lo;
        if (range->hi == bound)
            --range->hi;
        if (!bound)
            range->nonzero = 1;
    } else if (relation == 2 || relation == 3) {
        __int128 high = bound - (relation == 2);
        if (range->hi > high)
            range->hi = high;
    } else {
        __int128 low = bound + (relation == 4);
        if (range->lo < low)
            range->lo = low;
    }
    return range->lo <= range->hi;
}

static int ar_bound(ar_builder *p, int e, unsigned depth);

static int ar_bound_arm(ar_builder *p,
                        ar_branch *branch,
                        unsigned arm,
                        int condition,
                        int wanted,
                        int value,
                        unsigned depth) {
    size_t bytes = (size_t)p->d->count * sizeof *p->ranges;
    if (bytes > 16u * 1024u * 1024u - p->branch_bytes)
        return qc_fail(p->c, "arithmetic branch proof memory budget exceeded");
    branch->arm[arm] = calloc((size_t)p->d->count, sizeof *p->ranges);
    if (!branch->arm[arm])
        return qc_fail(p->c, "out of memory analyzing arithmetic branch");
    p->branch_bytes += bytes;
    ar_fact facts[128];
    unsigned count = 0;
    const ar_fact *saved_facts = p->facts, *head = p->facts;
    if (!ar_push_truth(p, condition, wanted, facts, &count, &head, 0))
        return 0;
    ar_range *saved_ranges = p->ranges;
    p->ranges = branch->arm[arm];
    p->facts = head;
    int ok = ar_bound(p, value, depth + 1);
    p->ranges = saved_ranges;
    p->facts = saved_facts;
    return ok;
}

static int ar_bound(ar_builder *p, int e, unsigned depth) {
    if (depth > 128 || ++p->work > QC_ARITH_WORK)
        return 0;
    ar_range *r = &p->ranges[e];
    if (r->valid)
        return 1;
    uint64_t w;
    if (!cj_u64(p->d, cj_get(p->d, e, "bits"), &w))
        return 0;
    r->width = (unsigned)w;
    r->sign = cj_bool(p->d, cj_get(p->d, e, "signed"));
    __int128 low = r->sign ? -((__int128)1 << (w - 1)) : 0;
    __int128 high = r->sign ? ((__int128)1 << (w - 1)) - 1 : (__int128)bits_mask((unsigned)w);
    int op = cj_get(p->d, e, "op"), nodes[3];
    unsigned n = expr_nodes(p->d, e, nodes);
    if (cj_equal(p->d, op, "const")) {
        uint64_t v;
        cj_u64(p->d, cj_get(p->d, e, "value"), &v);
        r->lo = r->hi = numeric(v, (unsigned)w, r->sign);
    } else if (cj_equal(p->d, op, "reg")) {
        uint64_t id;
        cj_u64(p->d, cj_get(p->d, e, "id"), &id);
        qc_register *s = reg(p->c, (uint32_t)id);
        if (s->basis_known)
            r->lo = r->hi = numeric(s->basis_value, s->width, r->sign);
        else {
            r->lo = low;
            r->hi = high;
            if (!r->sign || (s->zero_mask & (UINT64_C(1) << (s->width - 1)))) {
                r->lo = 0;
                r->hi = (__int128)(bits_mask(s->width) & ~s->zero_mask);
            }
            int fixed;
            if (s->width == 1 && ar_register_boolean_constant(p, s->id, &fixed, 0))
                r->lo = r->hi = numeric((uint64_t)fixed, 1, r->sign);
            if (*p->c->error)
                return 0;
        }
    } else {
        if (!n)
            return 0;
        const qc_ctype_profile *ctype = ctype_profile(p->d, op);
        if (ctype) {
            if (!ar_bound(p, nodes[0], depth + 1))
                return 0;
            ar_range input = p->ranges[nodes[0]];
            if (input.lo < EOF || input.hi > UCHAR_MAX)
                return qc_fail(
                    p->c, "ctype argument must be proven EOF or representable as unsigned char");
            qc_ctype_data data;
            if (!ctype_read(p->c, p->d, e, &data))
                return 0;
            if (input.lo == input.hi) {
                r->lo = r->hi = ctype_data_value(&data, (int)input.lo);
            } else if (data.mapping) {
                r->lo = input.lo < 0 ? -1 : 0;
                r->hi = 255;
            } else {
                r->lo = 0;
                r->hi = 0;
                for (unsigned i = 0; i < data.intervals; ++i)
                    if (data.bounds[i][2] > r->hi)
                        r->hi = data.bounds[i][2];
            }
            goto facts;
        }
        int lazy =
            cj_equal(p->d, op, "select") || cj_equal(p->d, op, "land") || cj_equal(p->d, op, "lor");
        if (lazy) {
            if (!ar_bound(p, nodes[0], depth + 1))
                return 0;
            int known = ar_known_truth(p->ranges[nodes[0]]), select = cj_equal(p->d, op, "select"),
                is_or = cj_equal(p->d, op, "lor");
            if (!select && known == is_or) {
                r->lo = r->hi = is_or;
                goto facts;
            }
            ar_branch *branch = calloc(1, sizeof *branch);
            if (!branch)
                return qc_fail(p->c, "out of memory recording arithmetic branch");
            branch->expression = e;
            branch->owner = p->ranges;
            branch->next = p->branches;
            p->branches = branch;
            if (select) {
                for (unsigned arm = 0; arm < 2; ++arm)
                    if (known < 0 || known == (int)arm)
                        if (!ar_bound_arm(
                                p, branch, arm, nodes[0], (int)arm, nodes[arm ? 1 : 2], depth))
                            return 0;
                ar_range yes = branch->arm[1] ? branch->arm[1][nodes[1]] : branch->arm[0][nodes[2]];
                ar_range no = branch->arm[0] ? branch->arm[0][nodes[2]] : yes;
                r->lo = yes.lo < no.lo ? yes.lo : no.lo;
                r->hi = yes.hi > no.hi ? yes.hi : no.hi;
                r->nonzero = yes.nonzero && no.nonzero;
            } else {
                unsigned arm = !is_or;
                if (!ar_bound_arm(p, branch, arm, nodes[0], (int)arm, nodes[1], depth))
                    return 0;
                int right = ar_known_truth(branch->arm[arm][nodes[1]]);
                r->lo = known >= 0 ? (right == 1) : is_or && right == 1;
                r->hi = known >= 0 ? (right != 0) : is_or || right != 0;
            }
            goto facts;
        }
        for (unsigned j = 0; j < n; ++j)
            if (!ar_bound(p, nodes[j], depth + 1))
                return 0;
        ar_range a = p->ranges[nodes[0]], b = n > 1 ? p->ranges[nodes[1]] : a;
        if (cj_equal(p->d, op, "cast")) {
            r->lo = a.lo;
            r->hi = a.hi;
            if (w == 1) {
                r->lo = 0;
                r->hi = 1;
            } else if (r->lo < low || r->hi > high) {
                r->lo = low;
                r->hi = high;
            }
        } else if (cj_equal(p->d, op, "sequence")) {
            r->lo = b.lo;
            r->hi = b.hi;
        } else if (cj_equal(p->d, op, "abs") || cj_equal(p->d, op, "uabs")) {
            if (r->sign && a.lo <= low)
                return 0; // Signed output cannot represent its own minimum's magnitude.
            r->lo = a.lo > 0 ? a.lo : a.hi < 0 ? -a.hi : 0;
            r->hi = ar_abs(a.lo) > ar_abs(a.hi) ? ar_abs(a.lo) : ar_abs(a.hi);
        } else if (cj_equal(p->d, op, "neg")) {
            r->lo = -a.hi;
            r->hi = -a.lo;
        } else if (cj_equal(p->d, op, "bnot")) {
            r->lo = r->sign ? -a.hi - 1 : high - a.hi;
            r->hi = r->sign ? -a.lo - 1 : high - a.lo;
        } else if (cj_equal(p->d, op, "add") || cj_equal(p->d, op, "sub")) {
            int sub = cj_equal(p->d, op, "sub");
            r->lo = a.lo + (sub ? -b.hi : b.lo);
            r->hi = a.hi + (sub ? -b.lo : b.hi);
        } else if (cj_equal(p->d, op, "mul")) {
            if (r->sign) {
                /* Signed 64-bit endpoints multiply safely in signed 128 bits. */
                __int128 products[4] = {a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi};
                r->lo = r->hi = products[0];
                for (unsigned j = 1; j < 4; ++j) {
                    if (products[j] < r->lo)
                        r->lo = products[j];
                    if (products[j] > r->hi)
                        r->hi = products[j];
                }
            } else {
                /* Avoid overflowing the range analyzer itself at 64 bits. */
                unsigned __int128 product = (unsigned __int128)a.hi * (unsigned __int128)b.hi;
                if (product > (unsigned __int128)high) {
                    r->lo = 0;
                    r->hi = high;
                } else {
                    r->lo = a.lo * b.lo;
                    r->hi = (__int128)product;
                }
            }
        } else if (cj_equal(p->d, op, "shl") || cj_equal(p->d, op, "shr")) {
            /* Count is checked against the promoted left type, never against
             * the eventual destination width. No input enumeration is used. */
            if (b.lo < 0 || b.hi >= a.width)
                return 0;
            unsigned first = (unsigned)b.lo, last = (unsigned)b.hi;
            if (cj_equal(p->d, op, "shl")) {
                if (r->sign && (a.lo < 0 || a.hi > (high >> last)))
                    return 0;
                if (!r->sign && a.hi > (high >> last)) {
                    r->lo = 0;
                    r->hi = high;
                } else {
                    r->lo = a.lo << first;
                    r->hi = a.hi << last;
                }
            } else {
                /* This backend's declared GCC/two's-complement profile uses
                 * arithmetic right shift for negative signed integers. */
                __int128 endpoints[4] = {a.lo >> first, a.lo >> last, a.hi >> first, a.hi >> last};
                r->lo = r->hi = endpoints[0];
                for (unsigned j = 1; j < 4; ++j) {
                    if (endpoints[j] < r->lo)
                        r->lo = endpoints[j];
                    if (endpoints[j] > r->hi)
                        r->hi = endpoints[j];
                }
            }
        } else if (cj_equal(p->d, op, "div") || cj_equal(p->d, op, "mod")) {
            if (!ar_nonzero(p, nodes[1], &b))
                return 0;
            if (r->sign && a.lo <= low && b.lo <= -1 && b.hi >= -1)
                return 0;
            __int128 amax = ar_abs(a.lo) > ar_abs(a.hi) ? ar_abs(a.lo) : ar_abs(a.hi);
            __int128 bmax = ar_abs(b.lo) > ar_abs(b.hi) ? ar_abs(b.lo) : ar_abs(b.hi);
            __int128 bmin = b.lo > 0 ? b.lo : b.hi < 0 ? -b.hi : 1;
            int divide = cj_equal(p->d, op, "div");
            __int128 magnitude = divide ? amax / bmin : (amax < bmax - 1 ? amax : bmax - 1);
            int negative = divide ? ((a.lo < 0 && b.hi > 0) || (a.hi > 0 && b.lo < 0)) : a.lo < 0;
            int positive = divide ? ((a.lo < 0 && b.lo < 0) || (a.hi > 0 && b.hi > 0)) : a.hi > 0;
            r->lo = negative ? -magnitude : 0;
            r->hi = positive ? magnitude : 0;
            /* A branch can prove both operands constant without measuring
             * them. Keep that exact value instead of allocating a divider. */
            if (a.lo == a.hi && b.lo == b.hi)
                r->lo = r->hi = divide ? a.lo / b.lo : a.lo % b.lo;
            /* MIN/-1 was excluded above, so these clamps are proven safe. */
            if (r->lo < low)
                r->lo = low;
            if (r->hi > high)
                r->hi = high;
        } else if (cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne") ||
                   cj_equal(p->d, op, "lt") || cj_equal(p->d, op, "le") ||
                   cj_equal(p->d, op, "gt") || cj_equal(p->d, op, "ge") ||
                   cj_equal(p->d, op, "not")) {
            r->lo = 0;
            r->hi = 1;
            if (cj_equal(p->d, op, "not")) {
                int truth = ar_known_truth(a);
                if (truth >= 0)
                    r->lo = r->hi = !truth;
            } else if (a.lo == a.hi && b.lo == b.hi) {
                int result = cj_equal(p->d, op, "eq")   ? a.lo == b.lo
                             : cj_equal(p->d, op, "ne") ? a.lo != b.lo
                             : cj_equal(p->d, op, "lt") ? a.lo < b.lo
                             : cj_equal(p->d, op, "le") ? a.lo <= b.lo
                             : cj_equal(p->d, op, "gt") ? a.lo > b.lo
                                                        : a.lo >= b.lo;
                r->lo = r->hi = result;
            } else {
                int result = -1;
                if (a.hi < b.lo || a.lo > b.hi) {
                    if (cj_equal(p->d, op, "eq"))
                        result = 0;
                    else if (cj_equal(p->d, op, "ne"))
                        result = 1;
                }
                if (cj_equal(p->d, op, "lt"))
                    result = a.hi < b.lo ? 1 : a.lo >= b.hi ? 0 : -1;
                if (cj_equal(p->d, op, "le"))
                    result = a.hi <= b.lo ? 1 : a.lo > b.hi ? 0 : -1;
                if (cj_equal(p->d, op, "gt"))
                    result = a.lo > b.hi ? 1 : a.hi <= b.lo ? 0 : -1;
                if (cj_equal(p->d, op, "ge"))
                    result = a.lo >= b.hi ? 1 : a.hi < b.lo ? 0 : -1;
                if (result >= 0)
                    r->lo = r->hi = result;
            }
        } else if (cj_equal(p->d, op, "band") || cj_equal(p->d, op, "bor") ||
                   cj_equal(p->d, op, "bxor")) {
            r->lo = low;
            r->hi = high;
            if (a.lo >= 0 && b.lo >= 0) {
                r->lo = 0;
                unsigned bits = 0;
                unsigned __int128 top = (unsigned __int128)(a.hi > b.hi ? a.hi : b.hi);
                while (top) {
                    ++bits;
                    top >>= 1;
                }
                r->hi = (__int128)bits_mask(bits);
                if (cj_equal(p->d, op, "band"))
                    r->hi = a.hi < b.hi ? a.hi : b.hi;
                if (cj_equal(p->d, op, "bor")) {
                    if (a.lo == a.hi)
                        r->lo = a.lo;
                    if (b.lo == b.hi && b.lo > r->lo)
                        r->lo = b.lo;
                }
            }
        } else
            return 0;
        if (r->lo < low || r->hi > high) {
            if (r->sign && !cj_equal(p->d, op, "cast"))
                return 0;
            if (w == 1) {
                r->lo = r->lo > 0 || r->hi < 0;
                r->hi = 1;
            } else if ((r->lo >> w) == (r->hi >> w)) {
                r->lo = (uint64_t)r->lo & bits_mask((unsigned)w);
                r->hi = (uint64_t)r->hi & bits_mask((unsigned)w);
            } else {
                r->lo = low;
                r->hi = high;
            }
        }
    }
facts:
    if (r->lo < low || r->hi > high) {
        if (r->sign && !cj_equal(p->d, op, "cast"))
            return 0;
        if (w == 1) {
            r->lo = r->lo > 0 || r->hi < 0;
            r->hi = 1;
        } else if ((r->lo >> w) == (r->hi >> w)) {
            r->lo = (uint64_t)r->lo & bits_mask((unsigned)w);
            r->hi = (uint64_t)r->hi & bits_mask((unsigned)w);
        } else {
            r->lo = low;
            r->hi = high;
        }
    }
    r->nonzero |= r->lo > 0 || r->hi < 0;
    if (p->facts) {
        int key = ar_truth_node(p, e);
        for (const ar_fact *fact = p->facts; fact; fact = fact->next) {
            if (!ar_order_fact(p, e, r, fact))
                return 0;
            int wanted;
            if (!ar_truth_fact(p, key, fact, &wanted))
                continue;
            if (wanted) {
                if (r->lo == 0 && r->hi == 0)
                    return 0;
                r->nonzero = 1;
                if (r->lo == 0 && r->hi > 0)
                    r->lo = 1;
                else if (r->hi == 0 && r->lo < 0)
                    r->hi = -1;
            } else {
                if (r->nonzero)
                    return 0;
                r->lo = r->hi = 0;
            }
        }
        if (*p->c->error)
            return 0;
    }
    r->valid = 1;
    return 1;
}

static int ar_gate(ar_builder *p, unsigned target, qc_mask mask, qc_mask pattern) {
    if (++p->work > QC_ARITH_WORK)
        return qc_fail(p->c, "structured arithmetic work budget exceeded");
    if (p->count == p->capacity) {
        size_t size = p->capacity ? p->capacity * 2 : 128;
        qc_gate *g = realloc(p->gates, size * sizeof *g);
        if (!g)
            return qc_fail(p->c, "out of memory recording arithmetic plan");
        p->gates = g;
        p->capacity = size;
    }
    p->gates[p->count++] = (qc_gate){QC_X, target, mask, pattern, 0};
    return qc_emit(p->c, QC_X, target, mask, pattern, 0);
}

static int
ar_condition(ar_builder *p, unsigned target, const ar_bit *bits, const int *wanted, unsigned n) {
    qc_mask mask = 0, pattern = 0;
    int possible = 1;
    for (unsigned j = 0; j < n; ++j)
        constrain(bits[j].wire, bits[j].inverted, wanted[j], &mask, &pattern, &possible);
    return !possible || ar_gate(p, target, mask, pattern);
}

static int ar_temp(ar_builder *p, unsigned width, ar_value *v) {
    if (p->nids == 512)
        return qc_fail(p->c, "arithmetic temporary budget exceeded");
    uint32_t id = UINT32_MAX;
    while (reg(p->c, id) && reg(p->c, id)->alive)
        --id;
    if (!allocate(p->c, id, width, 1, 0))
        return 0;
    p->ids[p->nids++] = id;
    qc_register *r = reg(p->c, id);
    *v = (ar_value){.width = width};
    for (unsigned b = 0; b < width; ++b)
        v->bit[b].wire = QC_WIRE(r->wires[b]);
    return 1;
}

static unsigned ar_wire(ar_bit b) {
    return (uint64_t)b.wire ? (unsigned)__builtin_ctzll((uint64_t)b.wire)
                            : 64 + (unsigned)__builtin_ctzll((uint64_t)(b.wire >> 64));
}

static int ar_copy(ar_builder *p, ar_value *out, const ar_value *in) {
    int one = 1;
    for (unsigned b = 0; b < out->width; ++b) {
        ar_bit bit = ar_at(in, b);
        if (!ar_condition(p, ar_wire(out->bit[b]), &bit, &one, 1))
            return 0;
    }
    return 1;
}

/* Zero testing reads all active bits directly. No width-sized XOR scratch. */
static int ar_truth(ar_builder *p, const ar_value *value, ar_bit *out) {
    if (value->width == 1) {
        *out = value->bit[0];
        return 1;
    }
    ar_bit bits[64];
    int zeros[64] = {0};
    int wires = 0;
    for (unsigned j = 0; j < value->width; ++j) {
        bits[j] = value->bit[j];
        if (!bits[j].wire && bits[j].inverted) {
            *out = (ar_bit){0, 1};
            return 1;
        }
        wires |= !!bits[j].wire;
    }
    if (!wires) {
        *out = (ar_bit){0, 0};
        return 1;
    }
    ar_value result;
    if (!ar_temp(p, 1, &result) || !ar_gate(p, ar_wire(result.bit[0]), 0, 0) ||
        !ar_condition(p, ar_wire(result.bit[0]), bits, zeros, value->width))
        return 0;
    *out = result.bit[0];
    return 1;
}

static int ar_literal_value(const ar_value *value) {
    for (unsigned j = 0; j < value->width; ++j)
        if (value->bit[j].wire)
            return 0;
    return 1;
}

/* Controlled increment/decrement of a suffix; O(n) logical MCX. */
static int ar_power(ar_builder *p,
                    ar_value *out,
                    unsigned offset,
                    const ar_bit *control,
                    unsigned n,
                    int subtract) {
    if (offset >= out->width)
        return 1;
    for (unsigned step = 0; step < out->width - offset; ++step) {
        unsigned b = subtract ? offset + step : out->width - 1 - step;
        ar_bit bits[66];
        int wanted[66];
        unsigned count = 0;
        for (unsigned j = 0; j < n; ++j) {
            bits[count] = control[j];
            wanted[count++] = 1;
        }
        for (unsigned k = offset; k < b; ++k) {
            bits[count] = out->bit[k];
            wanted[count++] = 1;
        }
        if (!ar_condition(p, ar_wire(out->bit[b]), bits, wanted, count))
            return 0;
    }
    return 1;
}

static int ar_add(ar_builder *p, ar_value *out, const ar_value *in, int subtract, ar_bit extra) {
    for (unsigned b = 0; b < out->width; ++b) {
        ar_bit controls[2] = {ar_at(in, b), extra};
        if (!ar_power(p, out, b, controls, 2, subtract))
            return 0;
    }
    return 1;
}

static int ar_negate_if(ar_builder *p, ar_value *out, ar_bit condition) {
    int one = 1;
    for (unsigned b = 0; b < out->width; ++b)
        if (!ar_condition(p, ar_wire(out->bit[b]), &condition, &one, 1))
            return 0;
    return ar_power(p, out, 0, &condition, 1, 0);
}

static int ar_magnitude(ar_builder *p, ar_value *value, ar_bit *negative) {
    *negative = value->sign ? value->bit[value->width - 1] : (ar_bit){0, 0};
    if (!value->sign)
        return 1;
    uint64_t constant = 0;
    int literal = 1;
    for (unsigned b = 0; b < value->width; ++b) {
        literal &= !value->bit[b].wire;
        if (value->bit[b].inverted)
            constant |= UINT64_C(1) << b;
    }
    if (literal) {
        if (negative->inverted)
            constant = (~constant + 1) & bits_mask(value->width);
        for (unsigned b = 0; b < value->width; ++b)
            value->bit[b] = (ar_bit){0, !!(constant & (UINT64_C(1) << b))};
        value->sign = 0;
        return 1;
    }
    ar_value magnitude;
    if (!ar_temp(p, value->width, &magnitude) || !ar_copy(p, &magnitude, value) ||
        !ar_negate_if(p, &magnitude, *negative))
        return 0;
    *value = magnitude;
    return 1;
}

/* Compare using a reversible XOR difference and disjoint lexical prefixes. */
static int ar_compare(ar_builder *p,
                      const ar_value *a,
                      const ar_value *b,
                      unsigned width,
                      int sign,
                      int relation,
                      ar_value *out) {
    if (ar_literal_value(a) || ar_literal_value(b)) {
        const ar_value *literal = ar_literal_value(b) ? b : a;
        const ar_value *value = literal == b ? a : b;
        ar_bit bits[66];
        int wanted[66];
        unsigned count = 0;
        if (relation == 0 || relation == 1 || relation == 3 || relation == 5) {
            for (unsigned j = 0; j < width; ++j) {
                bits[count] = ar_at(value, j);
                wanted[count++] = ar_at(literal, j).inverted;
            }
            if (!ar_condition(p, ar_wire(out->bit[0]), bits, wanted, count))
                return 0;
        }
        if (relation == 1 && !ar_gate(p, ar_wire(out->bit[0]), 0, 0))
            return 0;
        if (relation >= 2)
            for (unsigned step = 0; step < width; ++step) {
                unsigned j = width - step - 1;
                count = 0;
                for (unsigned k = j + 1; k < width; ++k) {
                    bits[count] = ar_at(value, k);
                    wanted[count++] = ar_at(literal, k).inverted;
                }
                bits[count] = ar_at(a, j);
                wanted[count++] = (relation >= 4) ^ (sign && j == width - 1);
                bits[count] = ar_at(b, j);
                wanted[count++] = (relation < 4) ^ (sign && j == width - 1);
                if (!ar_condition(p, ar_wire(out->bit[0]), bits, wanted, count))
                    return 0;
            }
        return 1;
    }
    ar_value diff;
    if (!ar_temp(p, width, &diff))
        return 0;
    size_t begin = p->count;
    if (!ar_copy(p, &diff, a) || !ar_copy(p, &diff, b))
        return 0;
    size_t copied = p->count;
    ar_bit bits[66];
    int wanted[66];
    unsigned count = 0;
    for (unsigned j = 0; j < width; ++j) {
        bits[count] = diff.bit[j];
        wanted[count++] = 0;
    }
    if (relation == 0 || relation == 1 || relation == 3 || relation == 5)
        if (!ar_condition(p, ar_wire(out->bit[0]), bits, wanted, count))
            return 0;
    if (relation == 1 && !ar_gate(p, ar_wire(out->bit[0]), 0, 0))
        return 0;
    if (relation >= 2)
        for (unsigned step = 0; step < width; ++step) {
            unsigned j = width - 1 - step;
            count = 0;
            for (unsigned k = j + 1; k < width; ++k) {
                bits[count] = diff.bit[k];
                wanted[count++] = 0;
            }
            bits[count] = ar_at(a, j);
            wanted[count++] = (relation >= 4) ^ (sign && j == width - 1);
            bits[count] = ar_at(b, j);
            wanted[count++] = (relation < 4) ^ (sign && j == width - 1);
            if (!ar_condition(p, ar_wire(out->bit[0]), bits, wanted, count))
                return 0;
        }
    for (size_t j = copied; j > begin;) {
        qc_gate g = p->gates[--j];
        if (!ar_gate(p, g.target, g.mask, g.pattern))
            return 0;
    }
    qc_register *scratch = reg(p->c, p->ids[--p->nids]);
    scratch->clean = 1;
    return release(p->c, scratch);
}

/* A controlled bit multiplexer enumerates at most 64 shift positions, not
 * dividend values. No full-width predecessor registers are needed. Typed
 * definedness is checked before lowering; unreachable invalid counts retain
 * a unitary XOR extension. The declared promoted width bounds the positions,
 * even when the active source view or requested output is narrower. */
static int ar_shift(ar_builder *p,
                    const ar_value *source,
                    const ar_value *count,
                    unsigned promoted_width,
                    int left,
                    ar_value *out) {
    unsigned positions = promoted_width;
    if (count->width < 6 && positions > (1u << count->width))
        positions = 1u << count->width;
    ar_bit bits[65];
    int wanted[65];
    for (unsigned j = 0; j < count->width; ++j)
        bits[j] = count->bit[j];
    for (unsigned distance = 0; distance < positions; ++distance) {
        for (unsigned j = 0; j < count->width; ++j)
            wanted[j] = !!((uint64_t)distance & (UINT64_C(1) << j));
        for (unsigned j = 0; j < out->width; ++j) {
            if (left && j < distance)
                continue;
            bits[count->width] = ar_at(source, left ? j - distance : j + distance);
            wanted[count->width] = 1;
            if (!ar_condition(p, ar_wire(out->bit[j]), bits, wanted, count->width + 1))
                return 0;
        }
    }
    return 1;
}

static int ar_lower(ar_builder *p, int e, unsigned requested, ar_value *out);

/* Powers of two do not need a long-divider remainder/quotient workspace.
 * Signed division rounds toward zero; signed remainder keeps the dividend's
 * sign. Typed definedness was checked before this lowering. */
static int ar_power_division(ar_builder *p,
                             const ar_value *a,
                             const ar_value *b,
                             int divide,
                             unsigned width,
                             int sign,
                             ar_value *out) {
    if (!ar_literal_value(b))
        return 0;
    uint64_t raw = 0;
    for (unsigned j = 0; j < b->width; ++j)
        if (b->bit[j].inverted)
            raw |= UINT64_C(1) << j;
    int negative = b->sign && !!(raw & (UINT64_C(1) << (b->width - 1)));
    uint64_t magnitude = negative ? (~raw + 1) & bits_mask(b->width) : raw;
    if (!magnitude || (magnitude & (magnitude - 1)))
        return 0;
    unsigned shift = 0;
    while ((magnitude >> shift) > 1)
        ++shift;
    ar_value value = {.width = width, .sign = sign};
    for (unsigned j = 0; j < width; ++j)
        value.bit[j] = divide ? ar_at(a, j + shift) : j < shift ? ar_at(a, j) : (ar_bit){0, 0};
    if (!a->sign && (!divide || !negative)) {
        *out = value;
        return 1;
    }
    if (!ar_temp(p, width, out) || !ar_copy(p, out, &value))
        return -1;
    out->sign = sign;
    ar_value low = {.width = shift};
    for (unsigned j = 0; j < shift; ++j)
        low.bit[j] = ar_at(a, j);
    ar_bit nonzero;
    if (!ar_truth(p, &low, &nonzero))
        return -1;
    ar_bit controls[2] = {a->sign ? a->bit[a->width - 1] : (ar_bit){0, 0}, nonzero};
    if (divide) {
        // Arithmetic shift is floor division. Increment negative, nonexact
        // results to obtain C truncation toward zero.
        if (!ar_power(p, out, 0, controls, 2, 0) ||
            (negative && !ar_negate_if(p, out, (ar_bit){0, 1})))
            return -1;
    } else {
        int ones[2] = {1, 1};
        for (unsigned j = shift; j < width; ++j)
            if (!ar_condition(p, ar_wire(out->bit[j]), controls, ones, 2))
                return -1;
    }
    return 1;
}

#include "small_remainder.inc"
#include "truth_views.inc"
#include "small_division.inc"
#include "select_views.inc"

static int
ar_lower_arm(ar_builder *p, int e, unsigned arm, int value, unsigned requested, ar_value *out) {
    ar_branch *branch = p->branches;
    while (branch && (branch->expression != e || branch->owner != p->ranges))
        branch = branch->next;
    if (!branch || !branch->arm[arm])
        return qc_fail(p->c, "missing arithmetic branch proof");
    ar_range *saved = p->ranges;
    p->ranges = branch->arm[arm];
    int ok = ar_lower(p, value, requested, out);
    p->ranges = saved;
    return ok;
}

/* Cover an interval by disjoint binary prefixes. One MCX per prefix toggles
 * the output; the cover is O(byte width), with no per-input enumeration. */
static int ar_ctype_interval(
    ar_builder *p, const ar_value *source, unsigned lo, unsigned hi, unsigned target) {
    while (lo <= hi) {
        unsigned power = 0;
        while (power < 8 && !(lo & ((1u << (power + 1)) - 1)) && lo + (1u << (power + 1)) - 1 <= hi)
            ++power;
        ar_bit bits[9];
        int values[9];
        unsigned count = 0;
        for (unsigned bit = power; bit < 8; ++bit) {
            bits[count] = ar_at(source, bit);
            values[count++] = !!(lo & (1u << bit));
        }
        // EOF's low byte is 255 too; its sign bit must exclude it from
        // byte intervals even when the locale classifies character 255.
        bits[count] = ar_at(source, 31);
        values[count++] = 0;
        if (!ar_condition(p, target, bits, values, count))
            return 0;
        lo += 1u << power;
    }
    return 1;
}

static int ar_lower(ar_builder *p, int e, unsigned requested, ar_value *out) {
    ar_range range = p->ranges[e];
    unsigned width = range.width < requested ? range.width : requested;
    if (range.lo >= 0) {
        unsigned active = 1;
        unsigned __int128 high = (unsigned __int128)range.hi;
        while (high >> active && active < 64)
            ++active;
        if (width > active)
            width = active;
    } else {
        unsigned active = 1;
        while (active < 64 && (range.lo < -((__int128)1 << (active - 1)) ||
                               range.hi > ((__int128)1 << (active - 1)) - 1))
            ++active;
        if (width > active)
            width = active;
    }
    int op = cj_get(p->d, e, "op"), nodes[3];
    unsigned n = expr_nodes(p->d, e, nodes);
    /* Pure values proved constant on this support need no arithmetic plan.
     * Branch-local constants are selected only by the already computed guard. */
    if (range.lo == range.hi) {
        uint64_t value = (uint64_t)range.lo;
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        for (unsigned j = 0; j < width; ++j)
            out->bit[j] = (ar_bit){0, !!(value & (UINT64_C(1) << j))};
        return 1;
    }
    if (cj_equal(p->d, op, "const") || cj_equal(p->d, op, "reg")) {
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        uint64_t v = 0, id = 0;
        qc_register *r = NULL;
        if (cj_equal(p->d, op, "reg")) {
            cj_u64(p->d, cj_get(p->d, e, "id"), &id);
            r = reg(p->c, (uint32_t)id);
            if (r->basis_known)
                v = r->basis_value;
        } else
            cj_u64(p->d, cj_get(p->d, e, "value"), &v);
        for (unsigned j = 0; j < width; ++j)
            out->bit[j] = r && !r->basis_known && !(r->zero_mask & (UINT64_C(1) << j))
                              ? (ar_bit){QC_WIRE(r->wires[j]), 0}
                              : (ar_bit){0, !!(v & (UINT64_C(1) << j))};
        return 1;
    }
    if (cj_equal(p->d, op, "cast") && range.width != 1) {
        ar_value source;
        // Only the requested cast bits survive. The original typed operation
        // was already checked for definedness before narrowing its output.
        if (!ar_lower(p, nodes[0], width, &source))
            return 0;
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        for (unsigned j = 0; j < width; ++j)
            out->bit[j] = ar_at(&source, j);
        return 1;
    }
    int shifted_truth = ar_shift_truth(p, e, out);
    if (shifted_truth)
        return shifted_truth > 0;
    int quotient_truth = ar_small_division_truth(p, e, out);
    if (quotient_truth)
        return quotient_truth > 0;
    const qc_ctype_profile *ctype = ctype_profile(p->d, op);
    if (ctype) {
        ar_value source;
        qc_ctype_data data;
        if (!ctype_read(p->c, p->d, e, &data) || !ar_lower(p, nodes[0], 32, &source))
            return 0;
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        if (data.mapping)
            for (unsigned bit = 0; bit < width; ++bit)
                out->bit[bit] = ar_at(&source, bit);
        unsigned seen[256], groups = 0;
        for (unsigned interval = 0; interval < data.intervals; ++interval) {
            unsigned value = data.bounds[interval][2], previous = 0;
            for (unsigned group = 0; group < groups; ++group)
                previous |= seen[group] == value;
            if (previous)
                continue;
            seen[groups++] = value;
            ar_value flag;
            if (!ar_temp(p, 1, &flag))
                return 0;
            for (unsigned j = interval; j < data.intervals; ++j)
                if (data.bounds[j][2] == value &&
                    !ar_ctype_interval(
                        p, &source, data.bounds[j][0], data.bounds[j][1], ar_wire(flag.bit[0])))
                    return 0;
            for (unsigned bit = 0; bit < width; ++bit) {
                if (!(value & (UINT32_C(1) << bit)))
                    continue;
                ar_bit before = out->bit[bit];
                if (!before.wire && !before.inverted)
                    out->bit[bit] = flag.bit[0];
                else {
                    ar_value mapped;
                    int one = 1;
                    if (!ar_temp(p, 1, &mapped) ||
                        !ar_condition(p, ar_wire(mapped.bit[0]), &before, &one, 1) ||
                        !ar_condition(p, ar_wire(mapped.bit[0]), &flag.bit[0], &one, 1))
                        return 0;
                    out->bit[bit] = mapped.bit[0];
                }
            }
        }
        return 1;
    }
    if (cj_equal(p->d, op, "sequence")) {
        ar_value discarded;
        if (!ar_lower(p, nodes[0], 64, &discarded))
            return 0;
        return ar_lower(p, nodes[1], requested, out);
    }
    if (cj_equal(p->d, op, "bnot")) {
        ar_value source;
        if (!ar_lower(p, nodes[0], width, &source))
            return 0;
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        for (unsigned j = 0; j < width; ++j) {
            out->bit[j] = ar_at(&source, j);
            out->bit[j].inverted ^= 1;
        }
        return 1;
    }
    if (cj_equal(p->d, op, "select") || cj_equal(p->d, op, "land") || cj_equal(p->d, op, "lor")) {
        int known = ar_known_truth(p->ranges[nodes[0]]), select = cj_equal(p->d, op, "select"),
            is_or = cj_equal(p->d, op, "lor");
        if (!select && known == is_or) {
            *out = (ar_value){.width = 1, .bit = {{0, is_or}}};
            return 1;
        }
        if (select && known >= 0)
            return ar_lower_arm(p, e, (unsigned)known, nodes[known ? 1 : 2], requested, out);
        ar_bit condition = {0, known == 1};
        ar_value first;
        if (known < 0 && (!ar_lower(p, nodes[0], 64, &first) || !ar_truth(p, &first, &condition)))
            return 0;
        if (select) {
            ar_value yes, no;
            if (!ar_lower_arm(p, e, 1, nodes[1], width, &yes) ||
                !ar_lower_arm(p, e, 0, nodes[2], width, &no))
                return 0;
            return ar_select_view(p, &yes, &no, condition, width, range.sign && range.lo < 0, out);
        }
        if (!ar_temp(p, width, out))
            return 0;
        out->sign = range.sign && range.lo < 0;
        ar_value right;
        ar_bit truth;
        if (!ar_lower_arm(p, e, (unsigned)!is_or, nodes[1], 64, &right) ||
            !ar_truth(p, &right, &truth))
            return 0;
        if (is_or) {
            int one = 1;
            if (!ar_condition(p, ar_wire(out->bit[0]), &condition, &one, 1))
                return 0;
        }
        ar_bit bits[2] = {condition, truth};
        int wanted[2] = {!is_or, 1};
        return ar_condition(p, ar_wire(out->bit[0]), bits, wanted, 2);
    }
    if (cj_equal(p->d, op, "band") || cj_equal(p->d, op, "bor") || cj_equal(p->d, op, "bxor")) {
        ar_value literals = {.width = width, .sign = range.sign && range.lo < 0};
        int supported = 1;
        for (unsigned j = 0; j < width; ++j) {
            bit_poly bit;
            if (!expr_bit(p->c, p->d, e, j, &bit, 0) ||
                !poly_literal(&bit, &literals.bit[j].wire, &literals.bit[j].inverted)) {
                supported = 0;
                break;
            }
        }
        if (supported) {
            *out = literals;
            return 1;
        }
    }
    int comparison = cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne") ||
                     cj_equal(p->d, op, "lt") || cj_equal(p->d, op, "le") ||
                     cj_equal(p->d, op, "gt") || cj_equal(p->d, op, "ge");
    int division = cj_equal(p->d, op, "div") || cj_equal(p->d, op, "mod");
    int absolute = cj_equal(p->d, op, "abs") || cj_equal(p->d, op, "uabs");
    int shift = cj_equal(p->d, op, "shl") || cj_equal(p->d, op, "shr");
    int logical_not = cj_equal(p->d, op, "not");
    ar_value a, b;
    // Absolute value must inspect the original sign, even if the destination
    // narrows its magnitude; truncating the input first changes that sign.
    if (!ar_lower(p,
                  nodes[0],
                  comparison || division || absolute || shift || logical_not || range.width == 1
                      ? 64
                      : width,
                  &a))
        return 0;
    if (n > 1 && !ar_lower(p, nodes[1], comparison || division || shift ? 64 : width, &b))
        return 0;
    if (division) {
        int optimized = ar_power_division(
            p, &a, &b, cj_equal(p->d, op, "div"), width, range.sign && range.lo < 0, out);
        if (optimized)
            return optimized > 0;
        optimized = cj_equal(p->d, op, "mod")
                        ? ar_small_remainder(p, &a, &b, width, range.sign && range.lo < 0, out)
                        : ar_small_division(p, &a, &b, width, range.sign && range.lo < 0, out);
        if (optimized)
            return optimized > 0;
    }
    // A fixed shift selects existing wires or sign/zero constants. Definedness
    // was checked by ar_bound; no barrel-shifter output register is needed.
    if (shift && ar_literal_value(&b)) {
        uint64_t distance = 0;
        for (unsigned j = 0; j < b.width; ++j)
            distance |= (uint64_t)b.bit[j].inverted << j;
        int left = cj_equal(p->d, op, "shl");
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        for (unsigned j = 0; j < width; ++j)
            out->bit[j] = left ? (j < distance ? (ar_bit){0, 0} : ar_at(&a, j - (unsigned)distance))
                               : ar_at(&a, j + (unsigned)distance);
        return 1;
    }
    // Constant masks are bit views even when the other operand is a locale
    // mapping or another expression not representable by the polynomial path.
    if ((cj_equal(p->d, op, "band") || cj_equal(p->d, op, "bor") || cj_equal(p->d, op, "bxor")) &&
        (ar_literal_value(&a) || ar_literal_value(&b))) {
        const ar_value *literal = ar_literal_value(&b) ? &b : &a;
        const ar_value *value = literal == &b ? &a : &b;
        *out = (ar_value){.width = width, .sign = range.sign && range.lo < 0};
        for (unsigned bit = 0; bit < width; ++bit) {
            ar_bit constant = ar_at(literal, bit), input = ar_at(value, bit);
            if (cj_equal(p->d, op, "band"))
                out->bit[bit] = constant.inverted ? input : (ar_bit){0, 0};
            else if (cj_equal(p->d, op, "bor"))
                out->bit[bit] = constant.inverted ? (ar_bit){0, 1} : input;
            else {
                input.inverted ^= constant.inverted;
                out->bit[bit] = input;
            }
        }
        return 1;
    }
    if (!ar_temp(p, width, out))
        return 0;
    out->sign = range.sign && range.lo < 0;
    if (shift)
        return ar_shift(p, &a, &b, p->ranges[nodes[0]].width, cj_equal(p->d, op, "shl"), out);
    if (absolute)
        return ar_copy(p, out, &a) &&
               ar_negate_if(p, out, a.sign ? a.bit[a.width - 1] : (ar_bit){0, 0});
    if (cj_equal(p->d, op, "neg"))
        return ar_copy(p, out, &a) && ar_negate_if(p, out, (ar_bit){0, 1});
    if (comparison) {
        if ((cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne")) &&
            (ar_literal_value(&a) || ar_literal_value(&b))) {
            const ar_value *literal = ar_literal_value(&b) ? &b : &a,
                           *value = literal == &b ? &a : &b;
            unsigned common = p->ranges[nodes[0]].width;
            ar_bit bits[64];
            int wanted[64];
            for (unsigned j = 0; j < common; ++j) {
                bits[j] = ar_at(value, j);
                wanted[j] = ar_at(literal, j).inverted;
            }
            if (!ar_condition(p, ar_wire(out->bit[0]), bits, wanted, common))
                return 0;
            return !cj_equal(p->d, op, "ne") || ar_gate(p, ar_wire(out->bit[0]), 0, 0);
        }
        int signed_order = p->ranges[nodes[0]].sign && (a.sign || b.sign);
        unsigned awidth = a.width + (signed_order && !a.sign),
                 bwidth = b.width + (signed_order && !b.sign);
        unsigned common = awidth > bwidth ? awidth : bwidth;
        int relation = cj_equal(p->d, op, "eq")   ? 0
                       : cj_equal(p->d, op, "ne") ? 1
                       : cj_equal(p->d, op, "lt") ? 2
                       : cj_equal(p->d, op, "le") ? 3
                       : cj_equal(p->d, op, "gt") ? 4
                                                  : 5;
        return ar_compare(p, &a, &b, common, signed_order, relation, out);
    }
    if (logical_not || (cj_equal(p->d, op, "cast") && range.width == 1)) {
        ar_bit truth;
        if (!ar_truth(p, &a, &truth))
            return 0;
        truth.inverted ^= logical_not;
        int one = 1;
        return ar_condition(p, ar_wire(out->bit[0]), &truth, &one, 1);
    }
    if (division) {
        ar_bit a_negative, b_negative;
        if (!ar_magnitude(p, &a, &a_negative) || !ar_magnitude(p, &b, &b_negative))
            return 0;
        unsigned nbits = a.width > b.width ? a.width : b.width;
        if (nbits == 64)
            return qc_fail(p->c,
                           "divider needs a 65-bit work register; physical backend limit exceeded");
        ar_value remainder, quotient;
        if (!ar_temp(p, nbits + 1, &remainder))
            return 0;
        int reused = cj_equal(p->d, op, "div") && out->width == nbits;
        if (reused)
            quotient = *out;
        else if (!ar_temp(p, nbits, &quotient))
            return 0;
        for (unsigned step = 0; step < nbits; ++step) {
            /* A wire permutation shifts a remainder whose top bit is zero. */
            ar_bit top = remainder.bit[nbits];
            for (unsigned j = nbits; j; --j)
                remainder.bit[j] = remainder.bit[j - 1];
            remainder.bit[0] = top;
            ar_bit next = ar_at(&a, nbits - 1 - step);
            int one = 1;
            if (!ar_condition(p, ar_wire(remainder.bit[0]), &next, &one, 1))
                return 0;
            ar_value flag = {.width = 1, .bit = {quotient.bit[nbits - 1 - step]}};
            if (!ar_compare(p, &remainder, &b, nbits + 1, 0, 5, &flag))
                return 0;
            if (!ar_add(p, &remainder, &b, 1, flag.bit[0]))
                return 0;
        }
        int divide = cj_equal(p->d, op, "div");
        if (!reused && !ar_copy(p, out, divide ? &quotient : &remainder))
            return 0;
        if (!ar_negate_if(p, out, a_negative))
            return 0;
        return !divide || ar_negate_if(p, out, b_negative);
    }
    if (cj_equal(p->d, op, "add") && range.width == 1) {
        ar_bit bits[2] = {ar_at(&a, 0), ar_at(&b, 0)};
        int wanted[2] = {1, 1};
        unsigned wire = ar_wire(out->bit[0]);
        return ar_condition(p, wire, bits, wanted, 1) &&
               ar_condition(p, wire, bits + 1, wanted, 1) && ar_condition(p, wire, bits, wanted, 2);
    }
    if (cj_equal(p->d, op, "add") || cj_equal(p->d, op, "sub"))
        return ar_copy(p, out, &a) && ar_add(p, out, &b, cj_equal(p->d, op, "sub"), (ar_bit){0, 1});
    if (cj_equal(p->d, op, "mul")) {
        for (unsigned i = 0; i < (a.sign ? width : a.width) && i < width; ++i)
            for (unsigned j = 0; j < (b.sign ? width : b.width) && i + j < width; ++j) {
                ar_bit controls[2] = {ar_at(&a, i), ar_at(&b, j)};
                if (!ar_power(p, out, i + j, controls, 2, 0))
                    return 0;
            }
        return 1;
    }
    if (cj_equal(p->d, op, "band") || cj_equal(p->d, op, "bor") || cj_equal(p->d, op, "bxor")) {
        for (unsigned j = 0; j < width; ++j) {
            ar_bit bits[2] = {ar_at(&a, j), ar_at(&b, j)};
            int ones[2] = {1, 1};
            unsigned wire = ar_wire(out->bit[j]);
            if (!cj_equal(p->d, op, "band") && (!ar_condition(p, wire, bits, ones, 1) ||
                                                !ar_condition(p, wire, bits + 1, ones, 1)))
                return 0;
            if (!cj_equal(p->d, op, "bxor") && !ar_condition(p, wire, bits, ones, 2))
                return 0;
        }
        return 1;
    }
    return qc_fail(p->c, "unsupported structured arithmetic operator");
}

static void ar_free(ar_builder *p) {
    while (p->branches) {
        ar_branch *branch = p->branches;
        p->branches = branch->next;
        free(branch->arm[0]);
        free(branch->arm[1]);
        free(branch);
    }
    free(p->ranges);
    free(p->gates);
}

static int arithmetic_compute(
    qc_context *c, const cj_doc *d, int e, qc_register *target, qc_mask outer, qc_mask pattern) {
    if (d->count > 100000)
        return qc_fail(c, "expression token budget exceeded"), -1;
    ar_builder p = {.c = c, .d = d};
    p.ranges = calloc((size_t)d->count, sizeof *p.ranges);
    if (!p.ranges)
        return qc_fail(c, "out of memory analyzing arithmetic"), -1;
    if (!ar_bound(&p, e, 0)) {
        int failure = *c->error ? -1 : 0;
        ar_free(&p);
        return failure;
    }
    /* Allocation can relocate the register array: retain only the identity. */
    uint32_t id = target->id;
    unsigned width = target->width;
    ar_value value;
    int ok = ar_lower(&p, e, width, &value);
    if (ok) {
        target = reg(c, id);
        for (unsigned b = 0; b < width && ok; ++b) {
            ar_bit bit = ar_at(&value, b);
            qc_mask mask = outer, state = pattern;
            int possible = 1;
            constrain(bit.wire, bit.inverted, 1, &mask, &state, &possible);
            if (possible)
                ok = qc_emit(c, QC_X, target->wires[b], mask, state, 0);
        }
    }
    if (ok)
        for (size_t j = p.count; j && ok;) {
            qc_gate g = p.gates[--j];
            ok = qc_emit(c, g.kind, g.target, g.mask, g.pattern, g.angle);
        }
    if (ok)
        for (unsigned j = p.nids; j && ok;) {
            qc_register *r = reg(c, p.ids[--j]);
            r->clean = 1;
            ok = release(c, r);
        }
    ar_free(&p);
    return ok ? 1 : -1;
}
