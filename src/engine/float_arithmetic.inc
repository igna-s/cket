/* Gate-level reversible IEEE-754 binary32 (float) and binary64 (double)
 * arithmetic, comparisons, and casts. Zero measurements or truth tables. */
#include <fenv.h>
#include <math.h>

typedef struct {
    unsigned bits, exp_bits, frac_bits, ew;
    int bias;
    uint64_t exp_max, qnan_bits;
} fl_spec;

static fl_spec fl_spec_for(unsigned bits) {
    if (bits == 32)
        return (fl_spec){32, 8, 23, 10, 127, 255u, UINT64_C(0xffc00000)};
    return (fl_spec){64, 11, 52, 13, 1023, 2047u, UINT64_C(0xfff8000000000000)};
}

static float fl_bits_to_f32(uint64_t raw) {
    uint32_t u = (uint32_t)raw;
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static uint64_t fl_f32_to_bits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return (uint64_t)u;
}

static double fl_bits_to_f64(uint64_t raw) {
    double d;
    memcpy(&d, &raw, sizeof d);
    return d;
}

static uint64_t fl_f64_to_bits(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof u);
    return u;
}

static int float_is_op(const cj_doc *d, int op) {
    static const char *const ops[] = {"fneg",
                                      "fabs",
                                      "fadd",
                                      "fsub",
                                      "fmul",
                                      "fdiv",
                                      "feq",
                                      "fne",
                                      "flt",
                                      "fle",
                                      "fgt",
                                      "fge",
                                      NULL};
    for (unsigned i = 0; ops[i]; ++i)
        if (cj_equal(d, op, ops[i]))
            return 1;
    return 0;
}

static int float_expr_uses_float(const cj_doc *d, int e) {
    for (int t = e; t < d->tokens[e].next; ++t) {
        if (d->tokens[t].kind == '{') {
            if (cj_bool(d, cj_get(d, t, "float")))
                return 1;
            int op = cj_get(d, t, "op");
            if (op >= 0 && float_is_op(d, op))
                return 1;
        }
    }
    return 0;
}

static int float_cond_truth(const cj_doc *d, int node, uint64_t val) {
    uint64_t b = 64;
    cj_u64(d, cj_get(d, node, "bits"), &b);
    if (cj_bool(d, cj_get(d, node, "float")))
        return (val & bits_mask((unsigned)b - 1)) != 0;
    return (val & bits_mask((unsigned)b)) != 0;
}

static int float_evaluate(qc_context *c,
                          const cj_doc *d,
                          int e,
                          int op,
                          unsigned bits,
                          int sign,
                          int is_float,
                          const int *nodes,
                          unsigned n,
                          uint64_t av,
                          uint64_t bv,
                          uint64_t *out) {
    (void)e;
    int saved_round = fegetround();
    fesetround(FE_TONEAREST);
    uint64_t abits = 0;
    if (!number(c, d, nodes[0], "bits", &abits)) {
        fesetround(saved_round);
        return 0;
    }
    int asign = cj_bool(d, cj_get(d, nodes[0], "signed"));
    int afloat = cj_bool(d, cj_get(d, nodes[0], "float"));

    if (cj_equal(d, op, "not")) {
        *out = (uint64_t)!float_cond_truth(d, nodes[0], av);
        fesetround(saved_round);
        return 1;
    }
    if (cj_equal(d, op, "fneg")) {
        *out = av ^ (UINT64_C(1) << (bits - 1));
        fesetround(saved_round);
        return 1;
    }
    if (cj_equal(d, op, "fabs")) {
        *out = av & ~(UINT64_C(1) << (bits - 1));
        fesetround(saved_round);
        return 1;
    }
    if (cj_equal(d, op, "fadd") || cj_equal(d, op, "fsub") || cj_equal(d, op, "fmul") ||
        cj_equal(d, op, "fdiv")) {
        if (n != 2) {
            fesetround(saved_round);
            return qc_fail(c, "floating-point binary arithmetic requires two operands");
        }
        if (bits == 32) {
            volatile float fa = fl_bits_to_f32(av);
            volatile float fb = fl_bits_to_f32(bv);
            volatile float fr = cj_equal(d, op, "fadd")   ? (fa + fb)
                                : cj_equal(d, op, "fsub") ? (fa - fb)
                                : cj_equal(d, op, "fmul") ? (fa * fb)
                                                          : (fa / fb);
            float tmp = fr;
            *out = fl_f32_to_bits(tmp);
        } else {
            volatile double da = fl_bits_to_f64(av);
            volatile double db = fl_bits_to_f64(bv);
            volatile double dr = cj_equal(d, op, "fadd")   ? (da + db)
                                 : cj_equal(d, op, "fsub") ? (da - db)
                                 : cj_equal(d, op, "fmul") ? (da * db)
                                                           : (da / db);
            double tmp = dr;
            *out = fl_f64_to_bits(tmp);
        }
        fesetround(saved_round);
        return 1;
    }
    if (cj_equal(d, op, "feq") || cj_equal(d, op, "fne") || cj_equal(d, op, "flt") ||
        cj_equal(d, op, "fle") || cj_equal(d, op, "fgt") || cj_equal(d, op, "fge")) {
        int res = 0;
        if (abits == 32) {
            volatile float fa = fl_bits_to_f32(av);
            volatile float fb = fl_bits_to_f32(bv);
            res = cj_equal(d, op, "feq")   ? (fa == fb)
                  : cj_equal(d, op, "fne") ? (fa != fb)
                  : cj_equal(d, op, "flt") ? (fa < fb)
                  : cj_equal(d, op, "fle") ? (fa <= fb)
                  : cj_equal(d, op, "fgt") ? (fa > fb)
                                           : (fa >= fb);
        } else {
            volatile double da = fl_bits_to_f64(av);
            volatile double db = fl_bits_to_f64(bv);
            res = cj_equal(d, op, "feq")   ? (da == db)
                  : cj_equal(d, op, "fne") ? (da != db)
                  : cj_equal(d, op, "flt") ? (da < db)
                  : cj_equal(d, op, "fle") ? (da <= db)
                  : cj_equal(d, op, "fgt") ? (da > db)
                                           : (da >= db);
        }
        *out = (uint64_t)(res != 0);
        fesetround(saved_round);
        return 1;
    }
    if (cj_equal(d, op, "cast")) {
        if (afloat && is_float) {
            if (abits == bits) {
                *out = av;
            } else if (abits == 32 && bits == 64) {
                volatile float fa = fl_bits_to_f32(av);
                volatile double dr = (double)fa;
                double tmp = dr;
                *out = fl_f64_to_bits(tmp);
            } else {
                volatile double da = fl_bits_to_f64(av);
                volatile float fr = (float)da;
                float tmp = fr;
                *out = fl_f32_to_bits(tmp);
            }
            fesetround(saved_round);
            return 1;
        }
        if (!afloat && is_float) {
            if (asign) {
                int64_t sa = (int64_t)numeric(av, (unsigned)abits, 1);
                if (bits == 32) {
                    volatile float fr = (float)sa;
                    float tmp = fr;
                    *out = fl_f32_to_bits(tmp);
                } else {
                    volatile double dr = (double)sa;
                    double tmp = dr;
                    *out = fl_f64_to_bits(tmp);
                }
            } else {
                uint64_t ua = av & bits_mask((unsigned)abits);
                if (bits == 32) {
                    volatile float fr = (float)ua;
                    float tmp = fr;
                    *out = fl_f32_to_bits(tmp);
                } else {
                    volatile double dr = (double)ua;
                    double tmp = dr;
                    *out = fl_f64_to_bits(tmp);
                }
            }
            fesetround(saved_round);
            return 1;
        }
        if (afloat && !is_float) {
            if (bits == 1) {
                *out = (uint64_t)float_cond_truth(d, nodes[0], av);
                fesetround(saved_round);
                return 1;
            }
            double dv = abits == 32 ? (double)fl_bits_to_f32(av) : fl_bits_to_f64(av);
            if (isnan(dv) || isinf(dv)) {
                fesetround(saved_round);
                return qc_fail(c, "undefined C conversion from non-finite float to integer");
            }
            double tv = trunc(dv);
            if (sign) {
                double lo = -ldexp(1.0, (int)bits - 1);
                double hi = ldexp(1.0, (int)bits - 1);
                if (!(tv >= lo && tv < hi)) {
                    fesetround(saved_round);
                    return qc_fail(c, "undefined signed floating-point to integer overflow");
                }
                int64_t iv = (int64_t)tv;
                *out = (uint64_t)iv & bits_mask(bits);
            } else {
                double hi = ldexp(1.0, (int)bits);
                if (!(tv >= 0.0 && tv < hi)) {
                    fesetround(saved_round);
                    return qc_fail(c, "undefined unsigned floating-point to integer out of range");
                }
                uint64_t uv = (uint64_t)tv;
                *out = uv & bits_mask(bits);
            }
            fesetround(saved_round);
            return 1;
        }
    }
    fesetround(saved_round);
    return qc_fail(c, "unsupported floating-point operator");
}

static ar_bit fl_bit0(void) {
    return (ar_bit){qc_mask_zero(), 0};
}

static ar_bit fl_bit1(void) {
    return (ar_bit){qc_mask_zero(), 1};
}

static ar_bit fl_not(ar_bit b) {
    return (ar_bit){b.wire, !b.inverted};
}

static ar_value fl_const_val(unsigned width, int sign, uint64_t v) {
    ar_value out = {.width = width, .sign = sign};
    for (unsigned b = 0; b < width; ++b)
        out.bit[b] = (ar_bit){qc_mask_zero(), !!((v >> b) & 1)};
    return out;
}

typedef struct {
    size_t begin_gates;
    unsigned begin_ids;
} fl_scope;

static fl_scope fl_scope_enter(ar_builder *p) {
    return (fl_scope){p->count, p->nids};
}

static int fl_scope_leave_before(ar_builder *p, fl_scope sc, size_t gate_limit) {
    for (size_t j = gate_limit; j > sc.begin_gates;) {
        qc_gate g = p->gates[--j];
        if (!ar_gate(p, g.target, g.mask, g.pattern))
            return 0;
    }
    while (p->nids > sc.begin_ids) {
        qc_register *r = reg(p->c, p->ids[--p->nids]);
        r->clean = 1;
        if (!release(p->c, r))
            return 0;
    }
    return 1;
}

static int fl_and2(ar_builder *p, ar_bit a, ar_bit b, ar_bit *out) {
    if (!qc_mask_any(a.wire)) {
        *out = a.inverted ? b : fl_bit0();
        return 1;
    }
    if (!qc_mask_any(b.wire)) {
        *out = b.inverted ? a : fl_bit0();
        return 1;
    }
    if (qc_mask_eq(a.wire, b.wire)) {
        *out = a.inverted == b.inverted ? a : fl_bit0();
        return 1;
    }
    ar_value t;
    if (!ar_temp(p, 1, &t))
        return 0;
    ar_bit ctrls[2] = {a, b};
    int wanted[2] = {1, 1};
    if (!ar_condition(p, ar_wire(t.bit[0]), ctrls, wanted, 2))
        return 0;
    *out = t.bit[0];
    return 1;
}

static int fl_or2(ar_builder *p, ar_bit a, ar_bit b, ar_bit *out) {
    ar_bit neg;
    if (!fl_and2(p, fl_not(a), fl_not(b), &neg))
        return 0;
    *out = fl_not(neg);
    return 1;
}

static int fl_xor2(ar_builder *p, ar_bit a, ar_bit b, ar_bit *out) {
    if (!qc_mask_any(a.wire)) {
        *out = a.inverted ? fl_not(b) : b;
        return 1;
    }
    if (!qc_mask_any(b.wire)) {
        *out = b.inverted ? fl_not(a) : a;
        return 1;
    }
    if (qc_mask_eq(a.wire, b.wire)) {
        *out = (ar_bit){qc_mask_zero(), a.inverted ^ b.inverted};
        return 1;
    }
    ar_value t;
    if (!ar_temp(p, 1, &t))
        return 0;
    int one = 1;
    if (!ar_condition(p, ar_wire(t.bit[0]), &a, &one, 1) ||
        !ar_condition(p, ar_wire(t.bit[0]), &b, &one, 1))
        return 0;
    *out = t.bit[0];
    return 1;
}

static int fl_bits_nz(ar_builder *p, const ar_bit *bits, unsigned n, ar_bit *out) {
    ar_value v = {.width = n};
    for (unsigned i = 0; i < n; ++i)
        v.bit[i] = bits[i];
    return ar_truth(p, &v, out);
}

static int fl_all_ones(ar_builder *p, const ar_bit *bits, unsigned n, ar_bit *out) {
    ar_bit zeros_or;
    ar_value inv = {.width = n};
    for (unsigned i = 0; i < n; ++i)
        inv.bit[i] = fl_not(bits[i]);
    if (!ar_truth(p, &inv, &zeros_or))
        return 0;
    *out = fl_not(zeros_or);
    return 1;
}

/* Reversible carry-chain ripple adder: acc += ctrl * x (or acc -= ctrl * x),
 * allocating and immediately uncomputing n-1 temporary carry wires. */
static int
fl_cadd(ar_builder *p, ar_value *acc, const ar_value *x, unsigned width, ar_bit ctrl, int sub) {
    if (!qc_mask_any(ctrl.wire) && !ctrl.inverted)
        return 1;
    if (width == 1) {
        ar_bit c[2] = {ctrl, ar_at(x, 0)};
        int w[2] = {1, 1};
        return ar_condition(p, ar_wire(acc->bit[0]), c, w, 2);
    }
    unsigned saved_nids = p->nids;
    ar_value carry;
    if (!ar_temp(p, width - 1, &carry))
        return 0;
    for (unsigned i = 0; i < width; ++i) {
        ar_bit xi = sub ? fl_not(ar_at(x, i)) : ar_at(x, i);
        ar_bit ci = i == 0 ? (sub ? fl_bit1() : fl_bit0()) : carry.bit[i - 1];
        if (i < width - 1) {
            ar_bit c1[3] = {ctrl, xi, acc->bit[i]};
            int w1[3] = {1, 1, 1};
            if (!ar_condition(p, ar_wire(carry.bit[i]), c1, w1, 3))
                return 0;
        }
        ar_bit c2[2] = {ctrl, xi};
        int w2[2] = {1, 1};
        if (!ar_condition(p, ar_wire(acc->bit[i]), c2, w2, 2))
            return 0;
        if (i < width - 1) {
            ar_bit c3[3] = {ctrl, ci, acc->bit[i]};
            int w3[3] = {1, 1, 1};
            if (!ar_condition(p, ar_wire(carry.bit[i]), c3, w3, 3))
                return 0;
        }
    }
    for (unsigned step = 0; step < width; ++step) {
        unsigned i = width - 1 - step;
        ar_bit xi = sub ? fl_not(ar_at(x, i)) : ar_at(x, i);
        ar_bit ci = i == 0 ? (sub ? fl_bit1() : fl_bit0()) : carry.bit[i - 1];
        if (i < width - 1) {
            ar_bit c4[3] = {ctrl, ci, acc->bit[i]};
            int w4[3] = {1, 1, 1};
            if (!ar_condition(p, ar_wire(carry.bit[i]), c4, w4, 3))
                return 0;
            ar_bit c5[3] = {ctrl, xi, acc->bit[i]};
            int w5[3] = {1, 1, 0};
            if (!ar_condition(p, ar_wire(carry.bit[i]), c5, w5, 3))
                return 0;
        }
        ar_bit c6[2] = {ctrl, ci};
        int w6[2] = {1, 1};
        if (!ar_condition(p, ar_wire(acc->bit[i]), c6, w6, 2))
            return 0;
    }
    while (p->nids > saved_nids) {
        qc_register *sc = reg(p->c, p->ids[--p->nids]);
        sc->clean = 1;
        if (!release(p->c, sc))
            return 0;
    }
    return 1;
}

static int fl_ctz_pow2(ar_builder *p,
                       const ar_bit *bits_in,
                       unsigned nbits,
                       unsigned pow2_w,
                       ar_value *ctz_out,
                       ar_bit *all_zero) {
    unsigned lg = pow2_w == 32 ? 5u : 6u;
    ar_value az;
    if (!ar_temp(p, lg, ctz_out) || !ar_temp(p, 1, &az))
        return 0;
    fl_scope sc = fl_scope_enter(p);
    ar_bit cur[64];
    for (unsigned i = 0; i < pow2_w; ++i)
        cur[i] = i < nbits ? bits_in[i] : fl_bit0();
    ar_bit ctz_ans[6];
    for (unsigned lvl = lg - 1; lvl >= 1; --lvl) {
        unsigned half = 1u << lvl;
        ar_bit any_lo;
        if (!fl_bits_nz(p, cur, half, &any_lo))
            return 0;
        ctz_ans[lvl] = fl_not(any_lo);
        ar_value lo = {.width = half}, hi = {.width = half}, nxt;
        for (unsigned i = 0; i < half; ++i) {
            lo.bit[i] = cur[i];
            hi.bit[i] = cur[half + i];
        }
        if (!ar_select_view(p, &lo, &hi, any_lo, half, 0, &nxt))
            return 0;
        for (unsigned i = 0; i < half; ++i)
            cur[i] = nxt.bit[i];
    }
    ctz_ans[0] = fl_not(cur[0]);
    ar_bit ztmp;
    if (!fl_and2(p, fl_not(cur[0]), fl_not(cur[1]), &ztmp))
        return 0;
    size_t after_tree = p->count;
    int one = 1;
    for (unsigned i = 0; i < lg; ++i)
        if (!ar_condition(p, ar_wire(ctz_out->bit[i]), &ctz_ans[i], &one, 1))
            return 0;
    if (!ar_condition(p, ar_wire(az.bit[0]), &ztmp, &one, 1))
        return 0;
    if (!fl_scope_leave_before(p, sc, after_tree))
        return 0;
    *all_zero = az.bit[0];
    return 1;
}

static int fl_clz(ar_builder *p,
                  const ar_bit *bits_in,
                  unsigned nbits,
                  unsigned pow2_w,
                  ar_value *clz_out,
                  ar_bit *all_zero) {
    ar_bit rev[64];
    for (unsigned i = 0; i < nbits; ++i)
        rev[i] = bits_in[nbits - 1 - i];
    return fl_ctz_pow2(p, rev, nbits, pow2_w, clz_out, all_zero);
}

typedef struct {
    ar_value raw, exp, sig;
    ar_bit sign, is_zero, is_inf, is_nan;
} fl_unpacked;

static int fl_unpack(ar_builder *p, fl_spec sp, const ar_value *raw, int norm_sub, fl_unpacked *u) {
    u->raw = *raw;
    u->sign = raw->bit[sp.bits - 1];
    ar_bit exp_nz, exp_max, frac_nz, unit_one;
    if (!fl_bits_nz(p, &raw->bit[sp.frac_bits], sp.exp_bits, &exp_nz) ||
        !fl_all_ones(p, &raw->bit[sp.frac_bits], sp.exp_bits, &exp_max) ||
        !fl_bits_nz(p, raw->bit, sp.frac_bits, &frac_nz))
        return 0;
    ar_bit exp_zero = fl_not(exp_nz);
    if (!fl_and2(p, exp_zero, fl_not(frac_nz), &u->is_zero) ||
        !fl_and2(p, exp_max, fl_not(frac_nz), &u->is_inf) ||
        !fl_and2(p, exp_max, frac_nz, &u->is_nan) ||
        !fl_and2(p, exp_nz, fl_not(exp_max), &unit_one))
        return 0;

    if (!ar_temp(p, sp.ew, &u->exp))
        return 0;
    u->exp.sign = 1;
    int one = 1;
    for (unsigned i = 0; i < sp.exp_bits; ++i)
        if (!ar_condition(p, ar_wire(u->exp.bit[i]), &raw->bit[sp.frac_bits + i], &one, 1))
            return 0;
    if (!ar_condition(p, ar_wire(u->exp.bit[0]), &exp_zero, &one, 1))
        return 0;
    ar_value bias_v = fl_const_val(sp.ew, 1, (uint64_t)sp.bias);
    if (!fl_cadd(p, &u->exp, &bias_v, sp.ew, fl_bit1(), 1))
        return 0;

    ar_value base_sig = {.width = sp.frac_bits + 1};
    for (unsigned i = 0; i < sp.frac_bits; ++i)
        base_sig.bit[i] = raw->bit[i];
    base_sig.bit[sp.frac_bits] = unit_one;
    if (!norm_sub) {
        u->sig = base_sig;
        return 1;
    }

    ar_value clz;
    ar_bit sig_zero;
    unsigned pow2_w = sp.bits == 32 ? 32u : 64u;
    if (!fl_clz(p, base_sig.bit, sp.frac_bits + 1, pow2_w, &clz, &sig_zero))
        return 0;
    if (!fl_cadd(p, &u->exp, &clz, sp.ew, fl_bit1(), 1))
        return 0;
    if (!ar_temp(p, sp.frac_bits + 1, &u->sig))
        return 0;
    ar_bit sbits[8];
    int swanted[8];
    for (unsigned i = 0; i < clz.width; ++i)
        sbits[i] = clz.bit[i];
    for (unsigned d = 0; d <= sp.frac_bits; ++d) {
        for (unsigned i = 0; i < clz.width; ++i)
            swanted[i] = !!((d >> i) & 1u);
        for (unsigned b = d; b <= sp.frac_bits; ++b) {
            sbits[clz.width] = base_sig.bit[b - d];
            swanted[clz.width] = 1;
            if (!ar_condition(p, ar_wire(u->sig.bit[b]), sbits, swanted, clz.width + 1))
                return 0;
        }
    }
    return 1;
}

static int fl_round_and_pack(ar_builder *p,
                             fl_spec sp,
                             ar_bit res_sign,
                             const ar_value *exp_in,
                             const ar_value *m_in,
                             ar_value *out) {
    ar_bit is_sub = fl_not(m_in->bit[sp.frac_bits + 3]);
    ar_bit any_rs_lsb, round_up;
    ar_bit guard_bits[3] = {m_in->bit[1], m_in->bit[0], m_in->bit[3]};
    if (!fl_bits_nz(p, guard_bits, 3, &any_rs_lsb) ||
        !fl_and2(p, m_in->bit[2], any_rs_lsb, &round_up))
        return 0;

    ar_value sig_r;
    if (!ar_temp(p, sp.frac_bits + 2, &sig_r))
        return 0;
    int one = 1;
    for (unsigned i = 0; i <= sp.frac_bits; ++i)
        if (!ar_condition(p, ar_wire(sig_r.bit[i]), &m_in->bit[3 + i], &one, 1))
            return 0;
    if (!ar_power(p, &sig_r, 0, &round_up, 1, 0))
        return 0;

    ar_value exp_pre;
    if (!ar_temp(p, sp.ew, &exp_pre))
        return 0;
    exp_pre.sign = 1;
    ar_value bias_v = fl_const_val(sp.ew, 1, (uint64_t)sp.bias);
    ar_bit not_sub = fl_not(is_sub);
    if (!fl_cadd(p, &exp_pre, exp_in, sp.ew, not_sub, 0) ||
        !fl_cadd(p, &exp_pre, &bias_v, sp.ew, not_sub, 0))
        return 0;

    ar_bit sub_prom, norm_carry, inc_exp;
    if (!fl_and2(p, is_sub, sig_r.bit[sp.frac_bits], &sub_prom) ||
        !fl_and2(p, not_sub, sig_r.bit[sp.frac_bits + 1], &norm_carry) ||
        !fl_or2(p, sub_prom, norm_carry, &inc_exp))
        return 0;
    if (!ar_power(p, &exp_pre, 0, &inc_exp, 1, 0))
        return 0;

    ar_value emax_v = fl_const_val(sp.ew, 1, sp.exp_max);
    ar_value overflow;
    if (!ar_temp(p, 1, &overflow) ||
        !ar_compare(p, &exp_pre, &emax_v, sp.ew, 1, 5 /* >= */, &overflow))
        return 0;

    if (!ar_temp(p, sp.bits, out))
        return 0;
    ar_bit ov = overflow.bit[0], not_ov = fl_not(ov);
    for (unsigned i = 0; i < sp.frac_bits; ++i) {
        ar_bit ctrls[2] = {not_ov, sig_r.bit[i]};
        int w[2] = {1, 1};
        if (!ar_condition(p, ar_wire(out->bit[i]), ctrls, w, 2))
            return 0;
    }
    for (unsigned i = 0; i < sp.exp_bits; ++i) {
        ar_bit ctrls[2] = {not_ov, exp_pre.bit[i]};
        int w[2] = {1, 1};
        if (!ar_condition(p, ar_wire(out->bit[sp.frac_bits + i]), ctrls, w, 2))
            return 0;
        if (!ar_condition(p, ar_wire(out->bit[sp.frac_bits + i]), &ov, &one, 1))
            return 0;
    }
    return ar_condition(p, ar_wire(out->bit[sp.bits - 1]), &res_sign, &one, 1);
}

static int fl_underflow_shift(
    ar_builder *p, fl_spec sp, const ar_value *norm_m, const ar_value *exp_in, ar_value *m_out) {
    unsigned mw = sp.frac_bits + 4;
    if (!ar_temp(p, mw, m_out))
        return 0;
    fl_scope sc = fl_scope_enter(p);
    int64_t e_min = 1 - sp.bias;
    ar_value emin_v = fl_const_val(sp.ew, 1, (uint64_t)e_min);
    ar_value is_uf;
    if (!ar_temp(p, 1, &is_uf) || !ar_compare(p, exp_in, &emin_v, sp.ew, 1, 2 /* < */, &is_uf))
        return 0;
    ar_bit uf = is_uf.bit[0];
    ar_value sub_delta;
    if (!ar_temp(p, sp.ew, &sub_delta) || !fl_cadd(p, &sub_delta, &emin_v, sp.ew, uf, 0) ||
        !fl_cadd(p, &sub_delta, exp_in, sp.ew, uf, 1))
        return 0;
    ar_value lsb_pos;
    ar_bit m_zero;
    unsigned pow2_w = sp.bits == 32 ? 32u : 64u;
    if (!fl_ctz_pow2(p, norm_m->bit, mw, pow2_w, &lsb_pos, &m_zero))
        return 0;
    ar_value lsb_ext = {.width = sp.ew};
    for (unsigned i = 0; i < sp.ew; ++i)
        lsb_ext.bit[i] = i < lsb_pos.width ? lsb_pos.bit[i] : fl_bit0();
    ar_value sticky_hit;
    if (!ar_temp(p, 1, &sticky_hit) ||
        !ar_compare(p, &lsb_ext, &sub_delta, sp.ew, 0, 3 /* <= */, &sticky_hit))
        return 0;

    size_t after_helpers = p->count;
    ar_bit not_uf = fl_not(uf);
    for (unsigned pos = 0; pos < mw; ++pos) {
        ar_bit ctrls[2] = {not_uf, norm_m->bit[pos]};
        int wanted[2] = {1, 1};
        if (!ar_condition(p, ar_wire(m_out->bit[pos]), ctrls, wanted, 2))
            return 0;
    }
    ar_bit sbits[16];
    int swanted[16];
    sbits[0] = uf;
    swanted[0] = 1;
    for (unsigned d = 1; d <= sp.frac_bits + 2; ++d) {
        for (unsigned j = 0; j < sp.ew; ++j) {
            sbits[1 + j] = sub_delta.bit[j];
            swanted[1 + j] = !!((d >> j) & 1u);
        }
        for (unsigned pos = 1; pos < mw; ++pos) {
            if (pos + d >= mw)
                break;
            sbits[1 + sp.ew] = norm_m->bit[pos + d];
            swanted[1 + sp.ew] = 1;
            if (!ar_condition(p, ar_wire(m_out->bit[pos]), sbits, swanted, 2 + sp.ew))
                return 0;
        }
    }
    ar_bit st_c[3] = {uf, fl_not(m_zero), sticky_hit.bit[0]};
    int st_w[3] = {1, 1, 1};
    if (!ar_condition(p, ar_wire(m_out->bit[0]), st_c, st_w, 3))
        return 0;
    return fl_scope_leave_before(p, sc, after_helpers);
}

static int fl_quiet_nan(fl_spec sp, const ar_value *nan_src, ar_value *out) {
    *out = (ar_value){.width = sp.bits};
    for (unsigned i = 0; i < sp.bits; ++i)
        out->bit[i] = i == sp.frac_bits - 1 ? fl_bit1() : nan_src->bit[i];
    return 1;
}

static int fl_copy_bits(ar_builder *p, const ar_value *dst, const ar_bit *src, unsigned n) {
    int one = 1;
    for (unsigned i = 0; i < n; ++i)
        if (!ar_condition(p, ar_wire(dst->bit[i]), &src[i], &one, 1))
            return 0;
    return 1;
}

static int fl_apply_specials(ar_builder *p,
                             fl_spec sp,
                             const ar_value *a_raw,
                             ar_bit a_is_nan,
                             const ar_value *b_raw,
                             ar_bit b_is_nan,
                             ar_bit new_qnan,
                             ar_bit sp_inf,
                             ar_bit sp_inf_sign,
                             ar_bit sp_zero,
                             ar_bit sp_zero_sign,
                             const ar_value *normal_val,
                             ar_value *out) {
    if (!ar_temp(p, sp.bits, out))
        return 0;
    fl_scope sc = fl_scope_enter(p);
    ar_bit not_a = fl_not(a_is_nan), not_b = fl_not(b_is_nan);
    ar_bit not_q = fl_not(new_qnan), not_inf = fl_not(sp_inf), not_z = fl_not(sp_zero);
    ar_bit sel_bnan, p2, sel_qnan, p3, sel_inf, p4, sel_zero, sel_norm;
    if (!fl_and2(p, not_a, b_is_nan, &sel_bnan) || !fl_and2(p, not_a, not_b, &p2) ||
        !fl_and2(p, p2, new_qnan, &sel_qnan) || !fl_and2(p, p2, not_q, &p3) ||
        !fl_and2(p, p3, sp_inf, &sel_inf) || !fl_and2(p, p3, not_inf, &p4) ||
        !fl_and2(p, p4, sp_zero, &sel_zero) || !fl_and2(p, p4, not_z, &sel_norm))
        return 0;
    size_t after_sel = p->count;

    ar_value zero_v = {.width = sp.bits};
    zero_v.bit[sp.bits - 1] = sp_zero_sign;
    ar_value inf_v = fl_const_val(sp.bits, 0, sp.exp_max << sp.frac_bits);
    inf_v.bit[sp.bits - 1] = sp_inf_sign;
    ar_value qnan_v = fl_const_val(sp.bits, 0, sp.qnan_bits);
    ar_value anan_v, bnan_v;
    if (!fl_quiet_nan(sp, a_raw, &anan_v) || !fl_quiet_nan(sp, b_raw, &bnan_v))
        return 0;

    const ar_value *vals[6] = {&anan_v, &bnan_v, &qnan_v, &inf_v, &zero_v, normal_val};
    ar_bit sels[6] = {a_is_nan, sel_bnan, sel_qnan, sel_inf, sel_zero, sel_norm};
    for (unsigned k = 0; k < 6; ++k) {
        for (unsigned i = 0; i < sp.bits; ++i) {
            ar_bit ctrls[2] = {sels[k], vals[k]->bit[i]};
            int wanted[2] = {1, 1};
            if (!ar_condition(p, ar_wire(out->bit[i]), ctrls, wanted, 2))
                return 0;
        }
    }
    return fl_scope_leave_before(p, sc, after_sel);
}

static int fl_finish_arith(ar_builder *p,
                           fl_spec sp,
                           const ar_value *a_raw,
                           const ar_value *b_raw,
                           const ar_value *flags_v,
                           const ar_value *exp_val,
                           const ar_value *m_in,
                           int do_underflow,
                           ar_value *out) {
    ar_value m_shifted;
    const ar_value *m_use = m_in;
    if (do_underflow) {
        if (!fl_underflow_shift(p, sp, m_in, exp_val, &m_shifted))
            return 0;
        m_use = &m_shifted;
    }
    ar_value packed;
    if (!fl_round_and_pack(p, sp, flags_v->bit[6], exp_val, m_use, &packed))
        return 0;
    return fl_apply_specials(p,
                             sp,
                             a_raw,
                             flags_v->bit[0],
                             b_raw,
                             flags_v->bit[1],
                             flags_v->bit[2],
                             flags_v->bit[3],
                             flags_v->bit[4],
                             flags_v->bit[5],
                             flags_v->bit[6],
                             &packed,
                             out);
}

static int fl_synth_add_sub(ar_builder *p,
                            fl_spec sp,
                            const ar_value *a_raw,
                            const ar_value *b_raw,
                            int is_sub,
                            ar_value *out) {
    unsigned mw = sp.frac_bits + 4;
    ar_value flags_v, exp_norm, m_norm;
    if (!ar_temp(p, 7, &flags_v) || !ar_temp(p, sp.ew, &exp_norm) || !ar_temp(p, mw, &m_norm))
        return 0;
    exp_norm.sign = 1;

    {
        fl_scope sc_core = fl_scope_enter(p);
        fl_unpacked a, b;
        if (!fl_unpack(p, sp, a_raw, 0, &a) || !fl_unpack(p, sp, b_raw, 0, &b))
            return 0;
        ar_bit eff_b_sign = is_sub ? fl_not(b.sign) : b.sign;

        ar_value swap_v;
        if (!ar_temp(p, 1, &swap_v) ||
            !ar_compare(p, &a.raw, &b.raw, sp.bits - 1, 0, 2 /* < */, &swap_v))
            return 0;
        ar_bit swap = swap_v.bit[0];

        ar_bit big_sign, eff_sub;
        ar_value bs_a = {.width = 1, .bit = {a.sign}}, bs_b = {.width = 1, .bit = {eff_b_sign}},
                 bs_v;
        if (!ar_select_view(p, &bs_b, &bs_a, swap, 1, 0, &bs_v) ||
            !fl_xor2(p, a.sign, eff_b_sign, &eff_sub))
            return 0;
        big_sign = bs_v.bit[0];

        ar_value big_exp, small_exp, big_sig, small_sig;
        if (!ar_select_view(p, &b.exp, &a.exp, swap, sp.ew, 1, &big_exp) ||
            !ar_select_view(p, &a.exp, &b.exp, swap, sp.ew, 1, &small_exp) ||
            !ar_select_view(p, &b.sig, &a.sig, swap, sp.frac_bits + 1, 0, &big_sig) ||
            !ar_select_view(p, &a.sig, &b.sig, swap, sp.frac_bits + 1, 0, &small_sig))
            return 0;

        ar_value delta;
        if (!ar_temp(p, sp.ew, &delta) || !fl_cadd(p, &delta, &big_exp, sp.ew, fl_bit1(), 0) ||
            !fl_cadd(p, &delta, &small_exp, sp.ew, fl_bit1(), 1))
            return 0;

        ar_value small_al;
        if (!ar_temp(p, mw, &small_al))
            return 0;
        {
            fl_scope sc = fl_scope_enter(p);
            ar_value lsb_pos;
            ar_bit small_z;
            unsigned pow2_w = sp.bits == 32 ? 32u : 64u;
            if (!fl_ctz_pow2(p, small_sig.bit, sp.frac_bits + 1, pow2_w, &lsb_pos, &small_z))
                return 0;
            ar_value lsb_p3;
            ar_value three = fl_const_val(lsb_pos.width + 1, 0, 3);
            if (!ar_temp(p, lsb_pos.width + 1, &lsb_p3) ||
                !fl_cadd(p, &lsb_p3, &lsb_pos, lsb_pos.width + 1, fl_bit1(), 0) ||
                !fl_cadd(p, &lsb_p3, &three, lsb_pos.width + 1, fl_bit1(), 0))
                return 0;
            ar_value sticky_hit;
            if (!ar_temp(p, 1, &sticky_hit) ||
                !ar_compare(p, &lsb_p3, &delta, sp.ew, 0, 3 /* <= */, &sticky_hit))
                return 0;
            size_t after_helpers = p->count;
            ar_bit abits[16];
            int awanted[16];
            for (unsigned d = 0; d <= sp.frac_bits + 2; ++d) {
                for (unsigned j = 0; j < sp.ew; ++j) {
                    abits[j] = delta.bit[j];
                    awanted[j] = !!((d >> j) & 1u);
                }
                for (unsigned pos = 1; pos < mw; ++pos) {
                    int idx = (int)pos - 3 + (int)d;
                    if (idx < 0 || idx > (int)sp.frac_bits)
                        continue;
                    abits[sp.ew] = small_sig.bit[idx];
                    awanted[sp.ew] = 1;
                    if (!ar_condition(p, ar_wire(small_al.bit[pos]), abits, awanted, sp.ew + 1))
                        return 0;
                }
            }
            ar_bit sc_bits[2] = {fl_not(small_z), sticky_hit.bit[0]};
            int sc_w[2] = {1, 1};
            if (!ar_condition(p, ar_wire(small_al.bit[0]), sc_bits, sc_w, 2))
                return 0;
            if (!fl_scope_leave_before(p, sc, after_helpers))
                return 0;
        }

        ar_value sum;
        if (!ar_temp(p, mw + 1, &sum))
            return 0;
        int one = 1;
        for (unsigned i = 0; i <= sp.frac_bits; ++i)
            if (!ar_condition(p, ar_wire(sum.bit[3 + i]), &big_sig.bit[i], &one, 1))
                return 0;
        if (!fl_cadd(p, &sum, &small_al, mw + 1, fl_not(eff_sub), 0) ||
            !fl_cadd(p, &sum, &small_al, mw + 1, eff_sub, 1))
            return 0;

        ar_bit carry_out = sum.bit[mw], not_co = fl_not(carry_out);
        ar_value clz;
        ar_bit sum_z, res_zero;
        unsigned pow2_w = sp.bits == 32 ? 32u : 64u;
        if (!fl_clz(p, sum.bit, mw, pow2_w, &clz, &sum_z) || !fl_and2(p, not_co, sum_z, &res_zero))
            return 0;

        ar_value max_left, clz_le;
        ar_value bias_m1 = fl_const_val(sp.ew, 1, (uint64_t)(sp.bias - 1));
        if (!ar_temp(p, sp.ew, &max_left) ||
            !fl_cadd(p, &max_left, &big_exp, sp.ew, fl_bit1(), 0) ||
            !fl_cadd(p, &max_left, &bias_m1, sp.ew, fl_bit1(), 0) || !ar_temp(p, 1, &clz_le))
            return 0;
        ar_value clz_ext = {.width = sp.ew};
        for (unsigned i = 0; i < clz.width; ++i)
            clz_ext.bit[i] = clz.bit[i];
        if (!ar_compare(p, &clz_ext, &max_left, sp.ew, 1, 3 /* <= */, &clz_le))
            return 0;

        ar_value left_sh;
        if (!ar_select_view(p, &clz_ext, &max_left, clz_le.bit[0], clz.width, 0, &left_sh))
            return 0;

        ar_value exp_tmp;
        if (!ar_temp(p, sp.ew, &exp_tmp))
            return 0;
        ar_value v_one = fl_const_val(sp.ew, 1, 1);
        if (!fl_cadd(p, &exp_tmp, &big_exp, sp.ew, fl_bit1(), 0) ||
            !fl_cadd(p, &exp_tmp, &v_one, sp.ew, carry_out, 0) ||
            !fl_cadd(p, &exp_tmp, &left_sh, sp.ew, not_co, 1))
            return 0;

        ar_bit co_or_rs;
        if (!fl_or2(p, sum.bit[0], sum.bit[1], &co_or_rs))
            return 0;

        ar_bit both_zero, zero_sign, nz_sign, t0, t1, res_sign;
        ar_bit both_inf_diff, any_inf, sp_inf, sp_zero;
        ar_value s_a = {.width = 1, .bit = {a.sign}}, s_b = {.width = 1, .bit = {eff_b_sign}},
                 s_inf;
        if (!fl_and2(p, a.is_zero, b.is_zero, &both_zero) ||
            !fl_and2(p, a.sign, eff_b_sign, &zero_sign) ||
            !fl_and2(p, fl_not(res_zero), big_sign, &nz_sign) ||
            !fl_and2(p, both_zero, zero_sign, &t0) ||
            !fl_and2(p, fl_not(both_zero), nz_sign, &t1) || !fl_or2(p, t0, t1, &res_sign) ||
            !fl_and2(p, a.is_inf, b.is_inf, &any_inf) ||
            !fl_and2(p, any_inf, eff_sub, &both_inf_diff) ||
            !fl_or2(p, a.is_inf, b.is_inf, &any_inf) ||
            !fl_and2(p, any_inf, fl_not(both_inf_diff), &sp_inf) ||
            !ar_select_view(p, &s_a, &s_b, a.is_inf, 1, 0, &s_inf) ||
            !fl_or2(p, both_zero, res_zero, &sp_zero))
            return 0;

        size_t after_core = p->count;
        ar_bit exp_bits_src[16];
        for (unsigned i = 0; i < sp.ew; ++i)
            exp_bits_src[i] = exp_tmp.bit[i];
        if (!fl_copy_bits(p, &exp_norm, exp_bits_src, sp.ew))
            return 0;

        ar_bit fsrc[7] = {
            a.is_nan, b.is_nan, both_inf_diff, sp_inf, s_inf.bit[0], sp_zero, res_sign};
        if (!fl_copy_bits(p, &flags_v, fsrc, 7))
            return 0;

        ar_bit c0[2] = {carry_out, co_or_rs};
        int w0[2] = {1, 1};
        if (!ar_condition(p, ar_wire(m_norm.bit[0]), c0, w0, 2))
            return 0;
        for (unsigned pos = 1; pos < mw; ++pos) {
            ar_bit cp[2] = {carry_out, sum.bit[pos + 1]};
            if (!ar_condition(p, ar_wire(m_norm.bit[pos]), cp, w0, 2))
                return 0;
        }
        ar_bit lbits[10];
        int lwanted[10];
        lbits[0] = not_co;
        lwanted[0] = 1;
        for (unsigned d = 0; d < mw; ++d) {
            for (unsigned j = 0; j < left_sh.width; ++j) {
                lbits[1 + j] = left_sh.bit[j];
                lwanted[1 + j] = !!((d >> j) & 1u);
            }
            for (unsigned pos = d; pos < mw; ++pos) {
                lbits[1 + left_sh.width] = sum.bit[pos - d];
                lwanted[1 + left_sh.width] = 1;
                if (!ar_condition(p, ar_wire(m_norm.bit[pos]), lbits, lwanted, 2 + left_sh.width))
                    return 0;
            }
        }
        if (!fl_scope_leave_before(p, sc_core, after_core))
            return 0;
    }

    return fl_finish_arith(p, sp, a_raw, b_raw, &flags_v, &exp_norm, &m_norm, 0, out);
}

static int fl_synth_mul(
    ar_builder *p, fl_spec sp, const ar_value *a_raw, const ar_value *b_raw, ar_value *out) {
    unsigned mw = sp.frac_bits + 4;
    ar_value flags_v, exp_prod, m_norm;
    if (!ar_temp(p, 7, &flags_v) || !ar_temp(p, sp.ew, &exp_prod) || !ar_temp(p, mw, &m_norm))
        return 0;
    exp_prod.sign = 1;

    {
        fl_scope sc_core = fl_scope_enter(p);
        fl_unpacked a, b;
        if (!fl_unpack(p, sp, a_raw, 1, &a) || !fl_unpack(p, sp, b_raw, 1, &b))
            return 0;
        ar_bit res_sign;
        if (!fl_xor2(p, a.sign, b.sign, &res_sign))
            return 0;

        ar_value exp_sum;
        if (!ar_temp(p, sp.ew, &exp_sum))
            return 0;
        exp_sum.sign = 1;
        if (!fl_cadd(p, &exp_sum, &a.exp, sp.ew, fl_bit1(), 0) ||
            !fl_cadd(p, &exp_sum, &b.exp, sp.ew, fl_bit1(), 0))
            return 0;

        unsigned pw = 2 * sp.frac_bits + 2;
        ar_value prod;
        if (!ar_temp(p, pw, &prod))
            return 0;
        for (unsigned k = 0; k <= sp.frac_bits; ++k) {
            ar_value slice = {.width = sp.frac_bits + 2};
            for (unsigned j = 0; j < slice.width; ++j)
                slice.bit[j] = prod.bit[k + j];
            if (!fl_cadd(p, &slice, &a.sig, sp.frac_bits + 2, b.sig.bit[k], 0))
                return 0;
        }

        ar_bit p_top = prod.bit[pw - 1], not_top = fl_not(p_top);
        ar_value one_v = fl_const_val(sp.ew, 1, 1);
        if (!fl_cadd(p, &exp_sum, &one_v, sp.ew, p_top, 0))
            return 0;

        ar_bit s_top, s_bot;
        if (!fl_bits_nz(p, prod.bit, sp.frac_bits - 1, &s_top) ||
            !fl_bits_nz(p, prod.bit, sp.frac_bits - 2, &s_bot))
            return 0;

        ar_bit z_inf, inf_z, new_qnan, either_z, sp_zero, either_inf, sp_inf;
        if (!fl_and2(p, a.is_zero, b.is_inf, &z_inf) || !fl_and2(p, a.is_inf, b.is_zero, &inf_z) ||
            !fl_or2(p, z_inf, inf_z, &new_qnan) || !fl_or2(p, a.is_zero, b.is_zero, &either_z) ||
            !fl_and2(p, either_z, fl_not(new_qnan), &sp_zero) ||
            !fl_or2(p, a.is_inf, b.is_inf, &either_inf) ||
            !fl_and2(p, either_inf, fl_not(new_qnan), &sp_inf))
            return 0;

        size_t after_core = p->count;
        ar_bit exp_src[16];
        for (unsigned i = 0; i < sp.ew; ++i)
            exp_src[i] = exp_sum.bit[i];
        if (!fl_copy_bits(p, &exp_prod, exp_src, sp.ew))
            return 0;

        ar_bit fsrc[7] = {a.is_nan, b.is_nan, new_qnan, sp_inf, res_sign, sp_zero, res_sign};
        if (!fl_copy_bits(p, &flags_v, fsrc, 7))
            return 0;

        ar_bit st1[2] = {p_top, s_top}, st0[2] = {not_top, s_bot};
        int w2[2] = {1, 1};
        if (!ar_condition(p, ar_wire(m_norm.bit[0]), st1, w2, 2) ||
            !ar_condition(p, ar_wire(m_norm.bit[0]), st0, w2, 2))
            return 0;
        for (unsigned pos = 1; pos < mw; ++pos) {
            ar_bit c1[2] = {p_top, prod.bit[sp.frac_bits - 2 + pos]};
            ar_bit c0[2] = {not_top, prod.bit[sp.frac_bits - 3 + pos]};
            if (!ar_condition(p, ar_wire(m_norm.bit[pos]), c1, w2, 2) ||
                !ar_condition(p, ar_wire(m_norm.bit[pos]), c0, w2, 2))
                return 0;
        }
        if (!fl_scope_leave_before(p, sc_core, after_core))
            return 0;
    }

    return fl_finish_arith(p, sp, a_raw, b_raw, &flags_v, &exp_prod, &m_norm, 1, out);
}

static int fl_synth_div(
    ar_builder *p, fl_spec sp, const ar_value *a_raw, const ar_value *b_raw, ar_value *out) {
    unsigned mw = sp.frac_bits + 4;
    ar_value flags_v, exp_quot, m_norm;
    if (!ar_temp(p, 7, &flags_v) || !ar_temp(p, sp.ew, &exp_quot) || !ar_temp(p, mw, &m_norm))
        return 0;
    exp_quot.sign = 1;

    {
        fl_scope sc_core = fl_scope_enter(p);
        fl_unpacked a, b;
        if (!fl_unpack(p, sp, a_raw, 1, &a) || !fl_unpack(p, sp, b_raw, 1, &b))
            return 0;
        ar_bit res_sign;
        if (!fl_xor2(p, a.sign, b.sign, &res_sign))
            return 0;

        ar_value exp_diff;
        if (!ar_temp(p, sp.ew, &exp_diff))
            return 0;
        exp_diff.sign = 1;
        if (!fl_cadd(p, &exp_diff, &a.exp, sp.ew, fl_bit1(), 0) ||
            !fl_cadd(p, &exp_diff, &b.exp, sp.ew, fl_bit1(), 1))
            return 0;

        unsigned rw = sp.frac_bits + 3;
        ar_value b_safe = {.width = rw};
        for (unsigned i = 0; i < sp.frac_bits; ++i)
            b_safe.bit[i] = b.sig.bit[i];
        b_safe.bit[sp.frac_bits] = fl_bit1();

        ar_value rem, q;
        if (!ar_temp(p, rw, &rem) || !ar_temp(p, mw, &q))
            return 0;
        int one = 1;
        for (unsigned i = 0; i <= sp.frac_bits; ++i)
            if (!ar_condition(p, ar_wire(rem.bit[i]), &a.sig.bit[i], &one, 1))
                return 0;

        for (unsigned step = 0; step < mw; ++step) {
            unsigned k = mw - 1 - step;
            if (!fl_cadd(p, &rem, &b_safe, rw, fl_bit1(), 1))
                return 0;
            ar_bit ok_bit = fl_not(rem.bit[rw - 1]);
            if (!ar_condition(p, ar_wire(q.bit[k]), &ok_bit, &one, 1) ||
                !fl_cadd(p, &rem, &b_safe, rw, fl_not(q.bit[k]), 0))
                return 0;
            if (k > 0) {
                ar_bit top = rem.bit[rw - 1];
                memmove(&rem.bit[1], &rem.bit[0], (rw - 1) * sizeof rem.bit[0]);
                rem.bit[0] = top;
            }
        }

        ar_bit rem_nz;
        if (!fl_bits_nz(p, rem.bit, sp.frac_bits + 1, &rem_nz))
            return 0;

        ar_bit q_top = q.bit[mw - 1], not_qt = fl_not(q_top);
        ar_value one_v = fl_const_val(sp.ew, 1, 1);
        if (!fl_cadd(p, &exp_diff, &one_v, sp.ew, not_qt, 1))
            return 0;

        ar_bit s1_or;
        if (!fl_or2(p, q.bit[0], rem_nz, &s1_or))
            return 0;

        ar_bit zz, ii, new_qnan, div0, pre_inf, sp_inf, pre_zero, sp_zero;
        if (!fl_and2(p, a.is_zero, b.is_zero, &zz) || !fl_and2(p, a.is_inf, b.is_inf, &ii) ||
            !fl_or2(p, zz, ii, &new_qnan) || !fl_and2(p, fl_not(a.is_zero), b.is_zero, &div0) ||
            !fl_or2(p, a.is_inf, div0, &pre_inf) ||
            !fl_and2(p, pre_inf, fl_not(new_qnan), &sp_inf) ||
            !fl_or2(p, a.is_zero, b.is_inf, &pre_zero) ||
            !fl_and2(p, pre_zero, fl_not(new_qnan), &sp_zero) ||
            !fl_and2(p, sp_zero, fl_not(sp_inf), &sp_zero))
            return 0;

        size_t after_core = p->count;
        ar_bit exp_src[16];
        for (unsigned i = 0; i < sp.ew; ++i)
            exp_src[i] = exp_diff.bit[i];
        if (!fl_copy_bits(p, &exp_quot, exp_src, sp.ew))
            return 0;

        ar_bit fsrc[7] = {a.is_nan, b.is_nan, new_qnan, sp_inf, res_sign, sp_zero, res_sign};
        if (!fl_copy_bits(p, &flags_v, fsrc, 7))
            return 0;

        ar_bit cs1[2] = {q_top, s1_or}, cs0[2] = {not_qt, rem_nz};
        int w2[2] = {1, 1};
        if (!ar_condition(p, ar_wire(m_norm.bit[0]), cs1, w2, 2) ||
            !ar_condition(p, ar_wire(m_norm.bit[0]), cs0, w2, 2))
            return 0;
        for (unsigned pos = 1; pos < mw; ++pos) {
            ar_bit c1[2] = {q_top, q.bit[pos]};
            if (!ar_condition(p, ar_wire(m_norm.bit[pos]), c1, w2, 2))
                return 0;
            ar_bit c0[2] = {not_qt, q.bit[pos - 1]};
            if (!ar_condition(p, ar_wire(m_norm.bit[pos]), c0, w2, 2))
                return 0;
        }
        if (!fl_scope_leave_before(p, sc_core, after_core))
            return 0;
    }

    return fl_finish_arith(p, sp, a_raw, b_raw, &flags_v, &exp_quot, &m_norm, 1, out);
}

static int fl_synth_cmp(ar_builder *p,
                        fl_spec sp,
                        const cj_doc *d,
                        int op,
                        const ar_value *a_raw,
                        const ar_value *b_raw,
                        unsigned out_bits,
                        ar_value *out) {
    fl_unpacked a, b;
    if (!fl_unpack(p, sp, a_raw, 0, &a) || !fl_unpack(p, sp, b_raw, 0, &b))
        return 0;
    ar_bit either_nan, both_zero;
    if (!fl_or2(p, a.is_nan, b.is_nan, &either_nan) ||
        !fl_and2(p, a.is_zero, b.is_zero, &both_zero))
        return 0;
    ar_value mag_eq, mag_lt;
    if (!ar_temp(p, 1, &mag_eq) || !ar_temp(p, 1, &mag_lt) ||
        !ar_compare(p, a_raw, b_raw, sp.bits - 1, 0, 0 /* == */, &mag_eq) ||
        !ar_compare(p, a_raw, b_raw, sp.bits - 1, 0, 2 /* < */, &mag_lt))
        return 0;
    ar_bit same_sign, same_nz, eq_inner, res_eq;
    if (!fl_xor2(p, a.sign, fl_not(b.sign), &same_sign) ||
        !fl_and2(p, same_sign, mag_eq.bit[0], &same_nz) ||
        !fl_or2(p, both_zero, same_nz, &eq_inner) ||
        !fl_and2(p, fl_not(either_nan), eq_inner, &res_eq))
        return 0;

    ar_bit mag_gt, lt_neg_pos, lt_neg_neg, lt_pos_pos, tmp_s, any_lt, res_lt;
    if (!fl_and2(p, fl_not(mag_eq.bit[0]), fl_not(mag_lt.bit[0]), &mag_gt) ||
        !fl_and2(p, a.sign, fl_not(b.sign), &lt_neg_pos) || !fl_and2(p, a.sign, b.sign, &tmp_s) ||
        !fl_and2(p, tmp_s, mag_gt, &lt_neg_neg) ||
        !fl_and2(p, fl_not(a.sign), fl_not(b.sign), &tmp_s) ||
        !fl_and2(p, tmp_s, mag_lt.bit[0], &lt_pos_pos) ||
        !fl_or2(p, lt_neg_pos, lt_neg_neg, &any_lt) || !fl_or2(p, any_lt, lt_pos_pos, &any_lt) ||
        !fl_and2(p, fl_not(either_nan), fl_not(both_zero), &tmp_s) ||
        !fl_and2(p, tmp_s, any_lt, &res_lt))
        return 0;

    ar_bit bit0;
    if (cj_equal(d, op, "feq"))
        bit0 = res_eq;
    else if (cj_equal(d, op, "fne"))
        bit0 = fl_not(res_eq);
    else if (cj_equal(d, op, "flt"))
        bit0 = res_lt;
    else if (cj_equal(d, op, "fle")) {
        if (!fl_or2(p, res_lt, res_eq, &bit0))
            return 0;
    } else if (cj_equal(d, op, "fgt")) {
        ar_bit not_le;
        if (!fl_or2(p, res_lt, res_eq, &not_le) ||
            !fl_and2(p, fl_not(either_nan), fl_not(not_le), &bit0))
            return 0;
    } else {
        if (!fl_and2(p, fl_not(either_nan), fl_not(res_lt), &bit0))
            return 0;
    }
    *out = (ar_value){.width = out_bits};
    out->bit[0] = bit0;
    for (unsigned i = 1; i < out_bits; ++i)
        out->bit[i] = fl_bit0();
    return 1;
}

static int fl_synth_f2f(
    ar_builder *p, unsigned src_bits, unsigned dst_bits, const ar_value *src, ar_value *out) {
    if (src_bits == dst_bits) {
        *out = *src;
        return 1;
    }
    fl_spec ss = fl_spec_for(src_bits), ds = fl_spec_for(dst_bits);
    fl_unpacked u;
    if (!fl_unpack(p, ss, src, src_bits == 32 ? 1 : 0, &u))
        return 0;

    ar_value nan_v = {.width = dst_bits};
    nan_v.bit[dst_bits - 1] = u.sign;
    for (unsigned i = 0; i < ds.exp_bits; ++i)
        nan_v.bit[ds.frac_bits + i] = fl_bit1();

    if (src_bits == 32) {
        for (unsigned i = 0; i < 23; ++i)
            nan_v.bit[29 + i] = i == 22 ? fl_bit1() : src->bit[i];
        ar_value exp_d, bias_v = fl_const_val(ds.exp_bits, 0, (uint64_t)ds.bias);
        if (!ar_temp(p, ds.exp_bits, &exp_d) ||
            !fl_cadd(p, &exp_d, &bias_v, ds.exp_bits, fl_bit1(), 0) ||
            !fl_cadd(p, &exp_d, &u.exp, ds.exp_bits, fl_bit1(), 0))
            return 0;
        ar_value norm_v = {.width = 64};
        norm_v.bit[63] = u.sign;
        for (unsigned i = 0; i < 23; ++i)
            norm_v.bit[29 + i] = u.sig.bit[i];
        for (unsigned i = 0; i < ds.exp_bits; ++i)
            norm_v.bit[52 + i] = exp_d.bit[i];
        ar_value zero_v = {.width = 64}, inf_v = fl_const_val(64, 0, ds.exp_max << 52), t1, t2;
        zero_v.bit[63] = u.sign;
        inf_v.bit[63] = u.sign;
        if (!ar_select_view(p, &zero_v, &norm_v, u.is_zero, 64, 0, &t1) ||
            !ar_select_view(p, &inf_v, &t1, u.is_inf, 64, 0, &t2) ||
            !ar_select_view(p, &nan_v, &t2, u.is_nan, 64, 0, out))
            return 0;
        return 1;
    }

    for (unsigned i = 0; i < 23; ++i)
        nan_v.bit[i] = i == 22 ? fl_bit1() : src->bit[29 + i];
    ar_bit drop_nz;
    if (!fl_bits_nz(p, u.sig.bit, 27, &drop_nz))
        return 0;
    ar_value m_norm = {.width = 27};
    m_norm.bit[0] = drop_nz;
    m_norm.bit[1] = u.sig.bit[27];
    m_norm.bit[2] = u.sig.bit[28];
    for (unsigned i = 0; i < 24; ++i)
        m_norm.bit[3 + i] = u.sig.bit[29 + i];

    ar_value lim_hi = fl_const_val(ss.ew, 1, 129);
    ar_value lim_lo = fl_const_val(ss.ew, 1, (uint64_t)(int64_t)-152);
    ar_value ov_hi, uf_lo;
    ar_bit force_inf, force_zero;
    if (!ar_temp(p, 1, &ov_hi) || !ar_temp(p, 1, &uf_lo) ||
        !ar_compare(p, &u.exp, &lim_hi, ss.ew, 1, 5 /* >= */, &ov_hi) ||
        !ar_compare(p, &u.exp, &lim_lo, ss.ew, 1, 3 /* <= */, &uf_lo) ||
        !fl_or2(p, u.is_inf, ov_hi.bit[0], &force_inf) ||
        !fl_or2(p, u.is_zero, uf_lo.bit[0], &force_zero))
        return 0;

    ar_value m_shifted, packed;
    if (!fl_underflow_shift(p, ds, &m_norm, &u.exp, &m_shifted) ||
        !fl_round_and_pack(p, ds, u.sign, &u.exp, &m_shifted, &packed))
        return 0;

    ar_value zero_v = {.width = 32}, inf_v = fl_const_val(32, 0, ds.exp_max << 23), t1, t2;
    zero_v.bit[31] = u.sign;
    inf_v.bit[31] = u.sign;
    if (!ar_select_view(p, &zero_v, &packed, force_zero, 32, 0, &t1) ||
        !ar_select_view(p, &inf_v, &t1, force_inf, 32, 0, &t2) ||
        !ar_select_view(p, &nan_v, &t2, u.is_nan, 32, 0, out))
        return 0;
    return 1;
}

static int
fl_synth_i2f(ar_builder *p, unsigned dst_bits, const ar_value *src, int src_sign, ar_value *out) {
    fl_spec ds = fl_spec_for(dst_bits);
    ar_value mag = *src;
    mag.sign = src_sign;
    ar_bit neg;
    if (!ar_magnitude(p, &mag, &neg))
        return 0;
    unsigned pow2_w = mag.width <= 32 ? 32u : 64u;
    ar_value clz;
    ar_bit all_zero;
    if (!fl_clz(p, mag.bit, mag.width, pow2_w, &clz, &all_zero))
        return 0;

    ar_value norm;
    if (!ar_temp(p, pow2_w, &norm))
        return 0;
    ar_bit sbits[8];
    int swanted[8];
    for (unsigned i = 0; i < clz.width; ++i)
        sbits[i] = clz.bit[i];
    unsigned d_min = pow2_w - mag.width;
    for (unsigned d = d_min; d < pow2_w; ++d) {
        for (unsigned i = 0; i < clz.width; ++i)
            swanted[i] = !!((d >> i) & 1u);
        for (unsigned b = d; b < pow2_w; ++b) {
            sbits[clz.width] = mag.bit[b - d];
            swanted[clz.width] = 1;
            if (!ar_condition(p, ar_wire(norm.bit[b]), sbits, swanted, clz.width + 1))
                return 0;
        }
    }

    ar_value exp_v = fl_const_val(ds.ew, 1, (uint64_t)(pow2_w - 1));
    ar_value tmp_exp;
    if (!ar_temp(p, ds.ew, &tmp_exp) || !fl_cadd(p, &tmp_exp, &exp_v, ds.ew, fl_bit1(), 0) ||
        !fl_cadd(p, &tmp_exp, &clz, ds.ew, fl_bit1(), 1))
        return 0;
    tmp_exp.sign = 1;

    unsigned mw = ds.frac_bits + 4;
    ar_value m_in = {.width = mw};
    int bot_end = (int)pow2_w - (int)(ds.frac_bits + 3);
    if (bot_end > 0) {
        if (!fl_bits_nz(p, norm.bit, (unsigned)bot_end, &m_in.bit[0]))
            return 0;
    } else
        m_in.bit[0] = fl_bit0();
    for (unsigned pos = 1; pos < mw; ++pos) {
        int idx = (int)pos + (int)pow2_w - 1 - (int)(ds.frac_bits + 3);
        m_in.bit[pos] = idx >= 0 ? norm.bit[idx] : fl_bit0();
    }

    ar_value packed, zero_v = {.width = dst_bits};
    if (!fl_round_and_pack(p, ds, neg, &tmp_exp, &m_in, &packed))
        return 0;
    return ar_select_view(p, &zero_v, &packed, all_zero, dst_bits, 0, out);
}

static int fl_synth_f2i(ar_builder *p,
                        unsigned src_bits,
                        const ar_value *src,
                        unsigned dst_bits,
                        int dst_sign,
                        ar_value *out) {
    if (dst_bits == 1) {
        ar_bit nz;
        if (!fl_bits_nz(p, src->bit, src_bits - 1, &nz))
            return 0;
        *out = (ar_value){.width = 1};
        out->bit[0] = nz;
        return 1;
    }
    fl_spec ss = fl_spec_for(src_bits);
    fl_unpacked u;
    if (!fl_unpack(p, ss, src, 0, &u))
        return 0;
    ar_value zero_v = fl_const_val(ss.ew, 1, 0), dst_lim = fl_const_val(ss.ew, 1, dst_bits);
    ar_value exp_ge0, exp_ltN, mag;
    if (!ar_temp(p, 1, &exp_ge0) || !ar_temp(p, 1, &exp_ltN) ||
        !ar_compare(p, &u.exp, &zero_v, ss.ew, 1, 5 /* >= */, &exp_ge0) ||
        !ar_compare(p, &u.exp, &dst_lim, ss.ew, 1, 2 /* < */, &exp_ltN) ||
        !ar_temp(p, dst_bits, &mag))
        return 0;
    ar_bit fin, ok_pos;
    if (!fl_and2(p, fl_not(u.is_nan), fl_not(u.is_inf), &fin) ||
        !fl_and2(p, fin, exp_ge0.bit[0], &ok_pos) || !fl_and2(p, ok_pos, exp_ltN.bit[0], &ok_pos))
        return 0;

    ar_bit ebits[16];
    int ewanted[16];
    ebits[0] = ok_pos;
    ewanted[0] = 1;
    for (unsigned e = 0; e < dst_bits; ++e) {
        for (unsigned j = 0; j < ss.ew; ++j) {
            ebits[1 + j] = u.exp.bit[j];
            ewanted[1 + j] = !!((e >> j) & 1u);
        }
        for (unsigned b = 0; b <= e; ++b) {
            int sidx = (int)ss.frac_bits - (int)(e - b);
            if (sidx < 0 || sidx > (int)ss.frac_bits)
                continue;
            ebits[1 + ss.ew] = u.sig.bit[sidx];
            ewanted[1 + ss.ew] = 1;
            if (!ar_condition(p, ar_wire(mag.bit[b]), ebits, ewanted, 2 + ss.ew))
                return 0;
        }
    }
    if (!dst_sign) {
        ar_value zero_i = {.width = dst_bits};
        return ar_select_view(p, &zero_i, &mag, u.sign, dst_bits, 0, out);
    }
    if (!ar_negate_if(p, &mag, u.sign))
        return 0;
    mag.sign = 1;
    *out = mag;
    return 1;
}

typedef struct {
    uint32_t id;
    size_t gate_begin, gate_end;
} fl_stage;

typedef struct {
    fl_stage *items;
    size_t count, capacity;
} fl_stage_list;

static int
fl_stage_push(qc_context *c, fl_stage_list *list, uint32_t id, size_t begin, size_t end) {
    if (list->count == list->capacity) {
        size_t cap = list->capacity ? list->capacity * 2 : 16;
        fl_stage *grown = realloc(list->items, cap * sizeof *grown);
        if (!grown)
            return qc_fail(c, "out of memory staging floating-point subexpressions");
        list->items = grown;
        list->capacity = cap;
    }
    list->items[list->count++] = (fl_stage){id, begin, end};
    return 1;
}

static int fl_reverse_gates(qc_context *c, size_t begin, size_t end) {
    for (size_t j = end; j > begin;) {
        qc_gate g = c->gates[--j];
        double angle = g.kind == QC_PHASE ? -g.angle : g.angle;
        if (!qc_emit(c, g.kind, g.target, g.mask, g.pattern, angle))
            return 0;
    }
    return 1;
}

static int fl_unwind_stages(qc_context *c, fl_stage_list *list, size_t keep_count) {
    int ok = 1;
    while (list->count > keep_count && ok) {
        fl_stage st = list->items[--list->count];
        ok = fl_reverse_gates(c, st.gate_begin, st.gate_end);
        if (ok) {
            qc_register *r = reg(c, st.id);
            r->clean = 1;
            ok = release(c, r);
        }
    }
    return ok;
}

static uint32_t fl_fresh_id(qc_context *c) {
    uint32_t id = UINT32_MAX;
    while (reg(c, id) && reg(c, id)->alive)
        --id;
    return id;
}

static int fl_check_domain_ub(
    qc_context *c, const cj_doc *d, int expr, qc_mask outer, qc_mask state, int *checked) {
    *checked = 0;
    domain dom;
    if (!collect_domain(c, d, expr, &dom))
        return 0;

    struct {
        unsigned src_idx, bit_idx;
    } free_bits[16];

    unsigned nfree = 0;
    uint64_t base_vals[QC_MAX_SOURCES];
    for (unsigned j = 0; j < dom.count; ++j) {
        qc_register *s = reg(c, dom.sources[j].id);
        base_vals[j] = s->basis_known ? s->basis_value : 0;
        if (s->basis_known)
            continue;
        for (unsigned b = 0; b < s->width; ++b) {
            if (s->zero_mask & (UINT64_C(1) << b))
                continue;
            if (qc_mask_test(outer, s->wires[b])) {
                if (qc_mask_test(state, s->wires[b]))
                    base_vals[j] |= UINT64_C(1) << b;
                continue;
            }
            if (nfree == 13)
                return 1;
            free_bits[nfree].src_idx = j;
            free_bits[nfree].bit_idx = b;
            ++nfree;
        }
    }
    if (nfree > 12)
        return 1;
    *checked = 1;
    size_t rows = (size_t)1 << nfree;
    for (size_t row = 0; row < rows; ++row) {
        uint64_t vals[QC_MAX_SOURCES];
        memcpy(vals, base_vals, dom.count * sizeof vals[0]);
        for (unsigned k = 0; k < nfree; ++k)
            if ((row >> k) & 1)
                vals[free_bits[k].src_idx] |= UINT64_C(1) << free_bits[k].bit_idx;
        uint64_t ignored;
        if (!evaluate(c, d, expr, &dom, vals, &ignored))
            return 0;
    }
    return 1;
}

static int
fl_check_all_subtrees_ub(qc_context *c, const cj_doc *d, int expr, qc_mask outer, qc_mask state) {
    int checked = 0;
    if (!fl_check_domain_ub(c, d, expr, outer, state, &checked))
        return 0;
    if (checked)
        return 1;
    int op = cj_get(d, expr, "op");
    if (cj_equal(d, op, "select") || cj_equal(d, op, "land") || cj_equal(d, op, "lor")) {
        int nodes[3];
        if (expr_nodes(d, expr, nodes) >= 1)
            return fl_check_all_subtrees_ub(c, d, nodes[0], outer, state);
        return 1;
    }
    int nodes[3];
    unsigned n = expr_nodes(d, expr, nodes);
    for (unsigned i = 0; i < n; ++i)
        if (!fl_check_all_subtrees_ub(c, d, nodes[i], qc_mask_zero(), qc_mask_zero()))
            return 0;
    return 1;
}

static int fl_compute_node(
    qc_context *c, const cj_doc *d, int expr, uint32_t target_id, qc_mask outer, qc_mask state);

static int
fl_stage_view(qc_context *c, const cj_doc *d, int node, fl_stage_list *stages, ar_value *out) {
    uint64_t bits64 = 0;
    if (!number(c, d, node, "bits", &bits64))
        return 0;
    unsigned bits = (unsigned)bits64;
    int sign = cj_bool(d, cj_get(d, node, "signed"));
    int is_float = cj_bool(d, cj_get(d, node, "float"));
    int op = cj_get(d, node, "op");

    if (cj_equal(d, op, "const")) {
        uint64_t v = 0;
        if (!number(c, d, node, "value", &v))
            return 0;
        *out = fl_const_val(bits, sign, bits == 1 ? !!v : v & bits_mask(bits));
        return 1;
    }
    if (cj_equal(d, op, "reg")) {
        uint64_t id = 0;
        if (!number(c, d, node, "id", &id))
            return 0;
        qc_register *r = reg(c, (uint32_t)id);
        *out = (ar_value){.width = bits, .sign = sign};
        for (unsigned b = 0; b < bits; ++b) {
            if (r->basis_known)
                out->bit[b] = (ar_bit){qc_mask_zero(), !!((r->basis_value >> b) & 1)};
            else if (r->zero_mask & (UINT64_C(1) << b))
                out->bit[b] = fl_bit0();
            else
                out->bit[b] = (ar_bit){QC_WIRE(r->wires[b]), 0};
        }
        return 1;
    }
    int nodes[3];
    unsigned n = expr_nodes(d, node, nodes);
    if (n == 1 && (cj_equal(d, op, "fneg") || cj_equal(d, op, "fabs"))) {
        if (!fl_stage_view(c, d, nodes[0], stages, out))
            return 0;
        if (cj_equal(d, op, "fneg"))
            out->bit[bits - 1].inverted ^= 1;
        else
            out->bit[bits - 1] = fl_bit0();
        return 1;
    }

    uint32_t tid = fl_fresh_id(c);
    if (!allocate(c, tid, bits, 1, sign))
        return 0;
    reg(c, tid)->float_type = is_float;
    size_t begin = c->ngates;
    int ok = fl_compute_node(c, d, node, tid, qc_mask_zero(), qc_mask_zero());
    size_t end = c->ngates;
    if (!ok || !fl_stage_push(c, stages, tid, begin, end)) {
        if (ok)
            fl_reverse_gates(c, begin, end);
        qc_register *tr = reg(c, tid);
        if (tr && tr->alive) {
            tr->clean = 1;
            release(c, tr);
        }
        return 0;
    }
    qc_register *tr = reg(c, tid);
    *out = (ar_value){.width = bits, .sign = sign};
    for (unsigned b = 0; b < bits; ++b)
        out->bit[b] = (ar_bit){QC_WIRE(tr->wires[b]), 0};
    return 1;
}

static int fl_emit_to_target(
    qc_context *c, uint32_t target_id, const ar_value *val, qc_mask outer, qc_mask state) {
    qc_register *target = reg(c, target_id);
    for (unsigned b = 0; b < target->width; ++b) {
        ar_bit bit = b < val->width            ? val->bit[b]
                     : val->sign && val->width ? val->bit[val->width - 1]
                                               : fl_bit0();
        qc_mask mask = outer, pat = state;
        int possible = 1;
        constrain(bit.wire, bit.inverted, 1, &mask, &pat, &possible);
        if (possible && !qc_emit(c, QC_X, target->wires[b], mask, pat, 0))
            return 0;
    }
    return 1;
}

static int fl_cleanup_builder(ar_builder *p) {
    int ok = 1;
    for (size_t j = p->count; j && ok;) {
        qc_gate g = p->gates[--j];
        double angle = g.kind == QC_PHASE ? -g.angle : g.angle;
        ok = qc_emit(p->c, g.kind, g.target, g.mask, g.pattern, angle);
    }
    while (p->nids && ok) {
        qc_register *r = reg(p->c, p->ids[--p->nids]);
        r->clean = 1;
        ok = release(p->c, r);
    }
    ar_free(p);
    return ok;
}

static int fl_compute_node(
    qc_context *c, const cj_doc *d, int expr, uint32_t target_id, qc_mask outer, qc_mask state) {
    if (!float_expr_uses_float(d, expr))
        return compute(c, d, expr, reg(c, target_id), outer, state);

    int op = cj_get(d, expr, "op");
    uint64_t bits64 = 0;
    if (!number(c, d, expr, "bits", &bits64))
        return 0;
    unsigned bits = (unsigned)bits64;
    int sign = cj_bool(d, cj_get(d, expr, "signed"));
    int nodes[3];
    unsigned n = expr_nodes(d, expr, nodes);

    fl_stage_list stages = {0};
    ar_builder p = {.c = c, .d = d};

    if (cj_equal(d, op, "const") || cj_equal(d, op, "reg") ||
        (n == 1 && (cj_equal(d, op, "fneg") || cj_equal(d, op, "fabs")))) {
        ar_value v;
        int ok = fl_stage_view(c, d, expr, &stages, &v) &&
                 fl_emit_to_target(c, target_id, &v, outer, state);
        if (!fl_unwind_stages(c, &stages, 0))
            ok = 0;
        free(stages.items);
        ar_free(&p);
        return ok;
    }

    if (cj_equal(d, op, "sequence") && n == 2) {
        ar_value disc;
        int ok = fl_stage_view(c, d, nodes[0], &stages, &disc) && fl_unwind_stages(c, &stages, 0) &&
                 fl_compute_node(c, d, nodes[1], target_id, outer, state);
        free(stages.items);
        ar_free(&p);
        return ok;
    }

    if (cj_equal(d, op, "select") && n == 3) {
        ar_value cond_v, res_val = {.width = bits, .sign = sign};
        int ok = fl_stage_view(c, d, nodes[0], &stages, &cond_v);
        if (ok) {
            ar_bit cond_bit;
            if (cj_bool(d, cj_get(d, nodes[0], "float"))) {
                ar_value cond_abs = {.width = cond_v.width - 1};
                for (unsigned i = 0; i < cond_v.width - 1; ++i)
                    cond_abs.bit[i] = cond_v.bit[i];
                ok = ar_truth(&p, &cond_abs, &cond_bit);
            } else
                ok = ar_truth(&p, &cond_v, &cond_bit);
            if (ok) {
                if (!qc_mask_any(cond_bit.wire)) {
                    size_t inner_mark = stages.count;
                    ok = fl_cleanup_builder(&p);
                    if (ok)
                        ok = fl_compute_node(
                            c, d, nodes[cond_bit.inverted ? 1 : 2], target_id, outer, state);
                    if (!fl_unwind_stages(c, &stages, 0))
                        ok = 0;
                    (void)inner_mark;
                    free(stages.items);
                    return ok;
                }
                ar_value arm_yes, arm_no;
                size_t saved_gates = p.count;
                if (ok)
                    ok = fl_stage_view(c, d, nodes[1], &stages, &arm_yes) &&
                         fl_stage_view(c, d, nodes[2], &stages, &arm_no) &&
                         ar_select_view(&p, &arm_yes, &arm_no, cond_bit, bits, sign, &res_val) &&
                         fl_emit_to_target(c, target_id, &res_val, outer, state);
                (void)saved_gates;
                if (!fl_cleanup_builder(&p))
                    ok = 0;
                if (!fl_unwind_stages(c, &stages, 0))
                    ok = 0;
                free(stages.items);
                return ok;
            }
        }
        fl_cleanup_builder(&p);
        fl_unwind_stages(c, &stages, 0);
        free(stages.items);
        return 0;
    }

    if (cj_equal(d, op, "not") && n == 1) {
        ar_value v;
        ar_bit nz;
        int ok = fl_stage_view(c, d, nodes[0], &stages, &v);
        if (ok) {
            ar_value mag = v;
            if (cj_bool(d, cj_get(d, nodes[0], "float")))
                mag.width = v.width - 1;
            ok = ar_truth(&p, &mag, &nz);
            if (ok) {
                ar_value res = fl_const_val(bits, sign, 0);
                res.bit[0] = fl_not(nz);
                ok = fl_emit_to_target(c, target_id, &res, outer, state);
            }
        }
        if (!fl_cleanup_builder(&p))
            ok = 0;
        if (!fl_unwind_stages(c, &stages, 0))
            ok = 0;
        free(stages.items);
        return ok;
    }

    if ((cj_equal(d, op, "land") || cj_equal(d, op, "lor")) && n == 2) {
        ar_value va, vb;
        ar_bit ta = fl_bit0(), tb = fl_bit0(), out_b = fl_bit0();
        int ok = fl_stage_view(c, d, nodes[0], &stages, &va) &&
                 fl_stage_view(c, d, nodes[1], &stages, &vb);
        if (ok) {
            ar_value a_check = va, b_check = vb;
            if (cj_bool(d, cj_get(d, nodes[0], "float")))
                a_check.width = va.width - 1;
            if (cj_bool(d, cj_get(d, nodes[1], "float")))
                b_check.width = vb.width - 1;
            ok = ar_truth(&p, &a_check, &ta) && ar_truth(&p, &b_check, &tb);
            if (ok) {
                ok = cj_equal(d, op, "land") ? fl_and2(&p, ta, tb, &out_b)
                                             : fl_or2(&p, ta, tb, &out_b);
                if (ok) {
                    ar_value res = fl_const_val(bits, sign, 0);
                    res.bit[0] = out_b;
                    ok = fl_emit_to_target(c, target_id, &res, outer, state);
                }
            }
        }
        if (!fl_cleanup_builder(&p))
            ok = 0;
        if (!fl_unwind_stages(c, &stages, 0))
            ok = 0;
        free(stages.items);
        return ok;
    }

    if (cj_equal(d, op, "cast") && n == 1) {
        uint64_t abits64 = 0;
        if (!number(c, d, nodes[0], "bits", &abits64)) {
            ar_free(&p);
            free(stages.items);
            return 0;
        }
        unsigned abits = (unsigned)abits64;
        int asign = cj_bool(d, cj_get(d, nodes[0], "signed"));
        int afloat = cj_bool(d, cj_get(d, nodes[0], "float"));
        int dst_float = cj_bool(d, cj_get(d, expr, "float"));
        ar_value a_v, out_v;
        int ok = fl_stage_view(c, d, nodes[0], &stages, &a_v);
        if (ok) {
            if (afloat && dst_float)
                ok = fl_synth_f2f(&p, abits, bits, &a_v, &out_v);
            else if (!afloat && dst_float)
                ok = fl_synth_i2f(&p, bits, &a_v, asign, &out_v);
            else if (afloat && !dst_float)
                ok = fl_synth_f2i(&p, abits, &a_v, bits, sign, &out_v);
            else
                out_v = a_v;
        }
        if (ok)
            ok = fl_emit_to_target(c, target_id, &out_v, outer, state);
        if (!fl_cleanup_builder(&p))
            ok = 0;
        if (!fl_unwind_stages(c, &stages, 0))
            ok = 0;
        free(stages.items);
        return ok;
    }

    if (n == 2) {
        ar_value a_v, b_v, out_v;
        uint64_t abits64 = 0;
        int ok = number(c, d, nodes[0], "bits", &abits64) &&
                 fl_stage_view(c, d, nodes[0], &stages, &a_v) &&
                 fl_stage_view(c, d, nodes[1], &stages, &b_v);
        if (ok) {
            fl_spec sp = fl_spec_for((unsigned)abits64);
            if (cj_equal(d, op, "fadd") || cj_equal(d, op, "fsub"))
                ok = fl_synth_add_sub(&p, sp, &a_v, &b_v, cj_equal(d, op, "fsub"), &out_v);
            else if (cj_equal(d, op, "fmul"))
                ok = fl_synth_mul(&p, sp, &a_v, &b_v, &out_v);
            else if (cj_equal(d, op, "fdiv"))
                ok = fl_synth_div(&p, sp, &a_v, &b_v, &out_v);
            else
                ok = fl_synth_cmp(&p, sp, d, op, &a_v, &b_v, bits, &out_v);
        }
        if (ok)
            ok = fl_emit_to_target(c, target_id, &out_v, outer, state);
        if (!fl_cleanup_builder(&p))
            ok = 0;
        if (!fl_unwind_stages(c, &stages, 0))
            ok = 0;
        free(stages.items);
        return ok;
    }

    ar_free(&p);
    fl_unwind_stages(c, &stages, 0);
    free(stages.items);
    return qc_fail(c, "unsupported floating-point expression operator");
}

static int float_arithmetic_compute(
    qc_context *c, const cj_doc *d, int expr, qc_register *target, qc_mask outer, qc_mask state) {
    if (!fl_check_all_subtrees_ub(c, d, expr, outer, state))
        return 0;
    return fl_compute_node(c, d, expr, target->id, outer, state);
}
