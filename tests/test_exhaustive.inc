/* Independent ordinary-C reference functions, checked on every uint8 input.
 * These are compiled by GCC as classical CPU code, not evaluated by the QIR
 * expression evaluator. All selected expressions have defined C behavior. */
#define EXPECT(name, expr)                    \
    static uint8_t expect_##name(uint8_t x) { \
        return (uint8_t)(expr);               \
    }
EXPECT(add, x + 5)
EXPECT(sub, x - 2)
EXPECT(mul, x * 3)
EXPECT(div, x / 2)
EXPECT(mod, x % 3)
EXPECT(band, x & 3)
EXPECT(bor, x | 2)
EXPECT(bxor, x ^ 5)
EXPECT(bnot, ~x)
EXPECT(shl, x << 1)
EXPECT(shr, x >> 1)
EXPECT(eq, x == 7)
EXPECT(ne, x != 7)
EXPECT(lt, x < 9)
EXPECT(le, x <= 7)
EXPECT(gt, x > 8)
EXPECT(ge, x >= 7)
EXPECT(not, !x)
EXPECT(land, x && 2)
EXPECT(lor, x || 0)
EXPECT(select, x ? 255 / x : 7)
#undef EXPECT

static void exhaustive_cases(void) {

    const struct {
        const char *expression;
        uint8_t (*expected)(uint8_t);
    } expressions[] = {{"x+5", expect_add},   {"x-2", expect_sub},  {"x*3", expect_mul},
                       {"x/2", expect_div},   {"x%3", expect_mod},  {"x&3", expect_band},
                       {"x|2", expect_bor},   {"x^5", expect_bxor}, {"~x", expect_bnot},
                       {"x<<1", expect_shl},  {"x>>1", expect_shr}, {"x==7", expect_eq},
                       {"x!=7", expect_ne},   {"x<9", expect_lt},   {"x<=7", expect_le},
                       {"x>8", expect_gt},    {"x>=7", expect_ge},  {"!x", expect_not},
                       {"x&&2", expect_land}, {"x||0", expect_lor}, {"x?255/x:7", expect_select}};

    for (unsigned test = 0; test < sizeof expressions / sizeof *expressions; ++test) {
        char source[512], name[128];
        snprintf(source,
                 sizeof source,
                 "quant uint8_t x=superposition(0,255);quant uint8_t y=%s;measure(y);measure(x);",
                 expressions[test].expression);
        qc_context *c = state(source);
        int ok = c && qc_qubits(c) == 16;
        if (c)
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                unsigned x = (unsigned)i & 255, y = (unsigned)(i >> 8) & 255;
                double amplitude = y == expressions[test].expected((uint8_t)x) ? 1.0 / 16 : 0;
                ok &= cabs(qc_amplitude(c, i) - amplitude) < 1e-10;
            }
        snprintf(name,
                 sizeof name,
                 "every uint8 input, C reference and amplitudes: %s",
                 expressions[test].expression);
        report(name, ok);
        qc_destroy(c);
    }
    // Exhaust all base-state pairs and both directions for several widths,
    // including unequal widths and modular wraparound. This checks mapping,
    // whereas a uniform state alone cannot distinguish different permutations.
    int ok = 1;
    unsigned checked = 0;
    for (unsigned target_width = 1; target_width <= 5; ++target_width) {
        if (target_width == 1)
            continue; // bool += is deliberately not modular.
        for (unsigned source_width = 1; source_width <= 5; ++source_width) {
            for (unsigned x = 0; x < (1u << target_width); ++x)
                for (unsigned y = 0; y < (1u << source_width); ++y) {
                    qc_context *c = qc_create(1, 7);
                    char operation[192];
                    uint64_t value = 0, source = 0;
                    int pass = c != NULL;
                    snprintf(operation,
                             sizeof operation,
                             "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",
                             target_width);
                    pass = pass && qc_apply(c, operation, NULL);
                    snprintf(operation,
                             sizeof operation,
                             "{\"op\":\"qprepare\",\"id\":0,\"lower\":%u,\"upper\":%u}",
                             x,
                             x);
                    pass = pass && qc_apply(c, operation, NULL);
                    snprintf(operation,
                             sizeof operation,
                             "{\"op\":\"qalloc\",\"id\":1,\"width\":%u,\"signed\":false}",
                             source_width);
                    pass = pass && qc_apply(c, operation, NULL);
                    snprintf(operation,
                             sizeof operation,
                             "{\"op\":\"qprepare\",\"id\":1,\"lower\":%u,\"upper\":%u}",
                             y,
                             y);
                    pass = pass && qc_apply(c, operation, NULL);
                    pass = pass &&
                           qc_apply(c, "{\"op\":\"qadd_register\",\"id\":0,\"source\":1}", NULL);
                    if (pass) {
                        unsigned expected = (x + y) & ((1u << target_width) - 1);
                        size_t basis = expected | ((size_t)y << target_width);
                        for (size_t i = 0; i < qc_state_size(c); ++i)
                            pass &= cabs(qc_amplitude(c, i) - (i == basis ? 1 : 0)) < 1e-10;
                    }
                    pass = pass &&
                           qc_apply(
                               c,
                               "{\"op\":\"qadd_register\",\"id\":0,\"source\":1,\"subtract\":true}",
                               NULL) &&
                           qc_apply(c, "{\"op\":\"qmeasure\",\"id\":0}", &value) &&
                           qc_apply(c, "{\"op\":\"qmeasure\",\"id\":1}", &source) && value == x &&
                           source == y;
                    ok &= pass;
                    ++checked;
                    qc_destroy(c);
                }
        }
    }
    char name[128];
    snprintf(name,
             sizeof name,
             "exhaustive register addition/subtraction and inverse: %u basis pairs",
             checked);
    report(name, ok);
}
