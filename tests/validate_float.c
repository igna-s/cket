#include "engine/quantum.h"
#include <complex.h>
#include <fenv.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double k_pi = 3.14159265358979323846264338327950288;

static int checks = 0;

static void check(int ok, const char *what, const char *detail) {
    if (!ok) {
        fprintf(stderr, "validate_float failed (%s): %s\n", what, detail ? detail : "");
        exit(1);
    }
    ++checks;
}

static uint32_t f32_bits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

static float bits_f32(uint32_t u) {
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static uint64_t f64_bits(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof u);
    return u;
}

static double bits_f64(uint64_t u) {
    double d;
    memcpy(&d, &u, sizeof d);
    return d;
}

static uint64_t read_wire_range(const uint64_t *words, unsigned start, unsigned width) {
    uint64_t v = 0;
    for (unsigned b = 0; b < width; ++b) {
        unsigned w = start + b;
        v |= ((words[w >> 6] >> (w & 63u)) & 1ULL) << b;
    }
    return v;
}

static uint64_t read_wire_bits(const uint64_t *words, unsigned width) {
    return read_wire_range(words, 0, width);
}

static void run_op(qc_context *c, const char *ir, uint64_t *res, const char *label) {
    int ok = qc_apply(c, ir, res);
    check(ok, label, qc_error(c));
}

static void expect_fail(const char *ops[], size_t nops, const char *label) {
    qc_context *c = qc_create(0, 1);
    check(c != NULL, label, "alloc ctx");
    int ok = 1;
    for (size_t i = 0; i < nops && ok; ++i)
        ok = qc_apply(c, ops[i], NULL);
    check(!ok, label, "expected rejection but succeeded");
    qc_destroy(c);
}

static void
test_binary_f32(const char *op, uint32_t a, uint32_t b, uint32_t expected, const char *label) {
    qc_context *c = qc_create_sparse(7, QC_SPARSE_MAX_STATES);
    char buf[1024];
    check(c != NULL, label, "create sparse");
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false,\"float\":true}",
           NULL,
           label);
    snprintf(buf,
             sizeof buf,
             "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu32 ",\"upper\":%" PRIu32 "}",
             a,
             a);
    run_op(c, buf, NULL, label);
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":2,\"width\":32,\"signed\":false,\"float\":true}",
           NULL,
           label);
    snprintf(buf,
             sizeof buf,
             "{\"op\":\"qprepare\",\"id\":2,\"lower\":%" PRIu32 ",\"upper\":%" PRIu32 "}",
             b,
             b);
    run_op(c, buf, NULL, label);
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":3,\"width\":32,\"signed\":false,\"float\":true}",
           NULL,
           label);
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"%s\",\"bits\":32,\"signed\":false,"
        "\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":32,\"signed\":false,\"float\":true,\"id\":1},"
        "{\"op\":\"reg\",\"bits\":32,\"signed\":false,\"float\":true,\"id\":2}]}}",
        op);
    run_op(c, buf, NULL, label);
    check(qc_sparse_size(c) == 1, label, "expected deterministic basis");
    uint64_t w[16] = {0};
    double complex amp = 0;
    check(qc_sparse_entry_words(c, 0, w, 16, &amp), label, qc_error(c));
    check((w[1] >> 32) == 0, label, "dirty ancilla in w[1]");
    for (size_t u = 2; u < 16; ++u)
        check(w[u] == 0, label, "dirty ancilla upper word");
    uint32_t got = (uint32_t)read_wire_range(w, 64, 32);
    if (got != expected) {
        fprintf(stderr,
                "f32 %s mismatch (a=0x%08x b=0x%08x): got=0x%08x want=0x%08x\n",
                op,
                a,
                b,
                got,
                expected);
        exit(1);
    }
    ++checks;
    run_op(c, "{\"op\":\"quncompute\",\"id\":3,\"saved\":true}", NULL, label);
    run_op(c, "{\"op\":\"qrelease\",\"id\":3}", NULL, label);
    run_op(c, "{\"op\":\"qreset\",\"id\":2}", NULL, label);
    run_op(c, "{\"op\":\"qrelease\",\"id\":2}", NULL, label);
    run_op(c, "{\"op\":\"qreset\",\"id\":1}", NULL, label);
    run_op(c, "{\"op\":\"qrelease\",\"id\":1}", NULL, label);
    check(qc_sparse_size(c) == 1, label, "dirty sparse state after release");
    qc_destroy(c);
}

static void
test_binary_f64(const char *op, uint64_t a, uint64_t b, uint64_t expected, const char *label) {
    qc_context *c = qc_create_sparse(11, QC_SPARSE_MAX_STATES);
    char buf[1024];
    check(c != NULL, label, "create sparse");
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false,\"float\":true}",
           NULL,
           label);
    snprintf(buf,
             sizeof buf,
             "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
             a,
             a);
    run_op(c, buf, NULL, label);
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":false,\"float\":true}",
           NULL,
           label);
    snprintf(buf,
             sizeof buf,
             "{\"op\":\"qprepare\",\"id\":2,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
             b,
             b);
    run_op(c, buf, NULL, label);
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":3,\"width\":64,\"signed\":false,\"float\":true}",
           NULL,
           label);
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"%s\",\"bits\":64,\"signed\":false,"
        "\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":1},"
        "{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":2}]}}",
        op);
    run_op(c, buf, NULL, label);
    check(qc_sparse_size(c) == 1, label, "expected deterministic basis");
    uint64_t w[16] = {0};
    double complex amp = 0;
    check(qc_sparse_entry_words(c, 0, w, 16, &amp), label, qc_error(c));
    for (size_t u = 3; u < 16; ++u)
        check(w[u] == 0, label, "dirty ancilla upper word");
    uint64_t got = w[2];
    if (got != expected) {
        fprintf(stderr,
                "f64 %s mismatch (a=0x%016" PRIx64 " b=0x%016" PRIx64 "): got=0x%016" PRIx64
                " want=0x%016" PRIx64 "\n",
                op,
                a,
                b,
                got,
                expected);
        exit(1);
    }
    ++checks;
    run_op(c, "{\"op\":\"quncompute\",\"id\":3,\"saved\":true}", NULL, label);
    run_op(c, "{\"op\":\"qrelease\",\"id\":3}", NULL, label);
    run_op(c, "{\"op\":\"qreset\",\"id\":2}", NULL, label);
    run_op(c, "{\"op\":\"qrelease\",\"id\":2}", NULL, label);
    run_op(c, "{\"op\":\"qreset\",\"id\":1}", NULL, label);
    run_op(c, "{\"op\":\"qrelease\",\"id\":1}", NULL, label);
    check(qc_sparse_size(c) == 1, label, "dirty sparse state after release");
    qc_destroy(c);
}

static void test_cmp_f32(uint32_t a, uint32_t b, const char *label) {
    volatile float fa = bits_f32(a), fb = bits_f32(b);
    const char *ops[] = {"feq", "fne", "flt", "fle", "fgt", "fge"};
    int wants[6] = {fa == fb, fa != fb, fa<fb, fa <= fb, fa> fb, fa >= fb};
    for (int k = 0; k < 6; ++k) {
        qc_context *c = qc_create_sparse(13, QC_SPARSE_MAX_STATES);
        char buf[1024];
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false,\"float\":true}",
               NULL,
               label);
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu32 ",\"upper\":%" PRIu32 "}",
                 a,
                 a);
        run_op(c, buf, NULL, label);
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":2,\"width\":32,\"signed\":false,\"float\":true}",
               NULL,
               label);
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":2,\"lower\":%" PRIu32 ",\"upper\":%" PRIu32 "}",
                 b,
                 b);
        run_op(c, buf, NULL, label);
        run_op(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}", NULL, label);
        snprintf(
            buf,
            sizeof buf,
            "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"%s\",\"bits\":1,\"signed\":false,"
            "\"args\":[{\"op\":\"reg\",\"bits\":32,\"signed\":false,\"float\":true,\"id\":1},"
            "{\"op\":\"reg\",\"bits\":32,\"signed\":false,\"float\":true,\"id\":2}]}}",
            ops[k]);
        run_op(c, buf, NULL, label);
        check(qc_sparse_size(c) == 1, label, ops[k]);
        uint64_t w[16] = {0};
        double complex amp = 0;
        check(qc_sparse_entry_words(c, 0, w, 16, &amp), label, qc_error(c));
        check((w[1] >> 1) == 0, label, "dirty cmp ancilla");
        for (size_t u = 2; u < 16; ++u)
            check(w[u] == 0, label, "dirty cmp upper word");
        check((int)(w[1] & 1u) == wants[k], label, ops[k]);
        run_op(c, "{\"op\":\"quncompute\",\"id\":3,\"saved\":true}", NULL, label);
        run_op(c, "{\"op\":\"qrelease\",\"id\":3}", NULL, label);
        qc_destroy(c);
    }
}

static void test_cmp_f64(uint64_t a, uint64_t b, const char *label) {
    volatile double da = bits_f64(a), db = bits_f64(b);
    const char *ops[] = {"feq", "fne", "flt", "fle", "fgt", "fge"};
    int wants[6] = {da == db, da != db, da<db, da <= db, da> db, da >= db};
    for (int k = 0; k < 6; ++k) {
        qc_context *c = qc_create_sparse(13, QC_SPARSE_MAX_STATES);
        char buf[1024];
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               label);
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
                 a,
                 a);
        run_op(c, buf, NULL, label);
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               label);
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":2,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
                 b,
                 b);
        run_op(c, buf, NULL, label);
        run_op(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}", NULL, label);
        snprintf(
            buf,
            sizeof buf,
            "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"%s\",\"bits\":1,\"signed\":false,"
            "\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":1},"
            "{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":2}]}}",
            ops[k]);
        run_op(c, buf, NULL, label);
        check(qc_sparse_size(c) == 1, label, ops[k]);
        uint64_t w[16] = {0};
        double complex amp = 0;
        check(qc_sparse_entry_words(c, 0, w, 16, &amp), label, qc_error(c));
        check((w[2] >> 1) == 0, label, "dirty cmp64 ancilla");
        for (size_t u = 3; u < 16; ++u)
            check(w[u] == 0, label, "dirty cmp64 upper word");
        check((int)(w[2] & 1u) == wants[k], label, ops[k]);
        run_op(c, "{\"op\":\"quncompute\",\"id\":3,\"saved\":true}", NULL, label);
        run_op(c, "{\"op\":\"qrelease\",\"id\":3}", NULL, label);
        qc_destroy(c);
    }
}

static void test_f32_arithmetic(void) {
    uint32_t vals[] = {
        0x00000000u, /* +0.0 */
        0x80000000u, /* -0.0 */
        0x3f800000u, /* 1.0 */
        0xbf800000u, /* -1.0 */
        0x40400000u, /* 3.0 */
        0x3f800001u, /* 1 + 2^-23 */
        0x34000000u, /* 2^-23 */
        0x33800000u, /* 2^-24 (half ulp) */
        0x33000000u, /* 2^-25 */
        0x32800000u, /* 2^-26 */
        0x33800001u, /* > half ulp */
        0x00000001u, /* min subnormal */
        0x007fffffu, /* max subnormal */
        0x00800000u, /* min normal */
        0x7f7fffffu, /* FLT_MAX */
        0xff7fffffu, /* -FLT_MAX */
        0x7f800000u, /* +Inf */
        0xff800000u, /* -Inf */
        0x7fc00001u, /* qNaN */
        0xff800002u, /* -sNaN */
    };
    size_t n = sizeof vals / sizeof vals[0];
    for (size_t i = 0; i < n; ++i) {
        volatile float fa = bits_f32(vals[i]);
        for (size_t j = 0; j < n; ++j) {
            volatile float fb = bits_f32(vals[j]);
            float sum = fa + fb;
            float diff = fa - fb;
            float prod = fa * fb;
            float quot = fa / fb;
            test_binary_f32("fadd", vals[i], vals[j], f32_bits(sum), "f32 fadd");
            test_binary_f32("fsub", vals[i], vals[j], f32_bits(diff), "f32 fsub");
            if ((i + j) % 3 == 0) {
                test_binary_f32("fmul", vals[i], vals[j], f32_bits(prod), "f32 fmul");
                test_binary_f32("fdiv", vals[i], vals[j], f32_bits(quot), "f32 fdiv");
            }
            if ((i + j) % 4 == 0)
                test_cmp_f32(vals[i], vals[j], "f32 cmp");
        }
    }
    /* Specific subtle cases for fmul/fdiv/subnormals */
    test_binary_f32("fmul",
                    0x00000001u,
                    0x4e800000u,
                    f32_bits(bits_f32(0x00000001u) * bits_f32(0x4e800000u)),
                    "sub*scale");
    test_binary_f32(
        "fmul", 0x00800000u, 0x3f000000u, f32_bits(bits_f32(0x00800000u) * 0.5f), "minnorm*0.5");
    test_binary_f32("fmul",
                    0x00000003u,
                    0x3f000000u,
                    f32_bits(bits_f32(0x00000003u) * 0.5f),
                    "tie-even-odd sub");
    test_binary_f32("fmul",
                    0x00000002u,
                    0x3f000000u,
                    f32_bits(bits_f32(0x00000002u) * 0.5f),
                    "tie-even-even sub");
    test_binary_f32("fdiv", 0x3f800000u, 0x40400000u, f32_bits(1.0f / 3.0f), "1/3 f32");
    test_binary_f32(
        "fdiv", 0x3f800000u, 0x00000003u, f32_bits(1.0f / bits_f32(0x00000003u)), "1/sub");
    test_binary_f32(
        "fdiv", 0x00000003u, 0x40000000u, f32_bits(bits_f32(0x00000003u) / 2.0f), "sub/2 tie");
}

static void test_f64_arithmetic(void) {
    uint64_t vals[] = {
        UINT64_C(0x0000000000000000), /* +0.0 */
        UINT64_C(0x8000000000000000), /* -0.0 */
        UINT64_C(0x3ff0000000000000), /* 1.0 */
        UINT64_C(0xbff0000000000000), /* -1.0 */
        UINT64_C(0x4008000000000000), /* 3.0 */
        UINT64_C(0x3cb0000000000000), /* 2^-52 */
        UINT64_C(0x3ca0000000000000), /* 2^-53 */
        UINT64_C(0x3c90000000000000), /* 2^-54 */
        UINT64_C(0x3c80000000000000), /* 2^-55 */
        UINT64_C(0x0000000000000001), /* min sub */
        UINT64_C(0x000fffffffffffff), /* max sub */
        UINT64_C(0x0010000000000000), /* min norm */
        UINT64_C(0x7fefffffffffffff), /* DBL_MAX */
        UINT64_C(0x7ff0000000000000), /* +Inf */
        UINT64_C(0xfff0000000000000), /* -Inf */
        UINT64_C(0x7ff0000000000001), /* +sNaN */
    };
    size_t n = sizeof vals / sizeof vals[0];
    for (size_t i = 0; i < n; ++i) {
        volatile double da = bits_f64(vals[i]);
        for (size_t j = 0; j < n; ++j) {
            volatile double db = bits_f64(vals[j]);
            if ((i + j) % 3 == 0 || i == 2) {
                double s = da + db, d = da - db;
                test_binary_f64("fadd", vals[i], vals[j], f64_bits(s), "f64 fadd");
                test_binary_f64("fsub", vals[i], vals[j], f64_bits(d), "f64 fsub");
            }
            if ((i + j) % 5 == 0 ||
                (i == 2 && (j == 0 || j == 4 || j == 9 || j == 13 || j == 15))) {
                double p = da * db, q = da / db;
                test_binary_f64("fmul", vals[i], vals[j], f64_bits(p), "f64 fmul");
                test_binary_f64("fdiv", vals[i], vals[j], f64_bits(q), "f64 fdiv");
            }
            if (i <= 3 && j <= 15 && (i + j) % 5 == 0)
                test_cmp_f64(vals[i], vals[j], "f64 cmp");
        }
    }
    test_cmp_f64(UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000), "f64 cmp +0 -0");
    test_cmp_f64(UINT64_C(0x7ff0000000000001), UINT64_C(0x3ff0000000000000), "f64 cmp nan");
    test_cmp_f64(UINT64_C(0xbff0000000000000), UINT64_C(0x3ff0000000000000), "f64 cmp neg pos");
}

static void test_casts_and_unary(void) {
    /* float <-> double */
    uint32_t f_cases[] = {
        0x00000000u, 0x80000000u, 0x00000001u, 0x3f800000u, 0x7f800000u, 0x7f800001u, 0xffc00002u};
    for (size_t i = 0; i < sizeof f_cases / sizeof f_cases[0]; ++i) {
        qc_context *c = qc_create_sparse(17, QC_SPARSE_MAX_STATES);
        char buf[512];
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false,\"float\":true}",
               NULL,
               "f2d");
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu32 ",\"upper\":%" PRIu32 "}",
                 f_cases[i],
                 f_cases[i]);
        run_op(c, buf, NULL, "f2d");
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               "f2d");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":64,\"signed\":false,\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":32,\"signed\":false,\"float\":true,\"id\":1}]}}",
            NULL,
            "f2d");
        uint64_t got = 0;
        run_op(c, "{\"op\":\"qmeasure\",\"id\":2,\"result\":\"r\"}", &got, "f2d");
        volatile float src = bits_f32(f_cases[i]);
        volatile double want_d = (double)src;
        double wd = want_d;
        check(got == f64_bits(wd), "cast f32->f64", "bit mismatch");
        qc_destroy(c);
    }

    /* double -> float including subnormal single rounding and huge-exponent clip */
    uint64_t d_cases[] = {
        UINT64_C(0x8000000000000000),
        UINT64_C(0x3690000000000000), /* 2^-150 -> +0.0 */
        UINT64_C(0x3690000000000001), /* 2^-150 + eps -> min subnormal */
        UINT64_C(0x36a0000000000000), /* 2^-149 -> min subnormal */
        UINT64_C(0x47effffff0000000), /* rounds to +Inf */
        f64_bits(1e200),              /* huge +exp -> +Inf */
        f64_bits(-1e200),             /* huge +exp -> -Inf */
        f64_bits(1e-200),             /* huge -exp -> +0.0 */
        f64_bits(-1e-200),            /* huge -exp -> -0.0 */
        UINT64_C(0x7ff0000000000001), /* sNaN */
    };
    for (size_t i = 0; i < sizeof d_cases / sizeof d_cases[0]; ++i) {
        qc_context *c = qc_create_sparse(19, QC_SPARSE_MAX_STATES);
        char buf[512];
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               "d2f");
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
                 d_cases[i],
                 d_cases[i]);
        run_op(c, buf, NULL, "d2f");
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":2,\"width\":32,\"signed\":false,\"float\":true}",
               NULL,
               "d2f");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":false,\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":1}]}}",
            NULL,
            "d2f");
        uint64_t got = 0;
        run_op(c, "{\"op\":\"qmeasure\",\"id\":2,\"result\":\"r\"}", &got, "d2f");
        volatile double src = bits_f64(d_cases[i]);
        volatile float want_f = (float)src;
        float wf = want_f;
        check((uint32_t)got == f32_bits(wf), "cast f64->f32", "bit mismatch");
        qc_destroy(c);
    }

    /* uint64 -> float single-round check & int64_min -> double */
    {
        uint64_t u64 = (UINT64_C(1) << 63) + (UINT64_C(1) << 40) + UINT64_C(1);
        qc_context *c = qc_create_sparse(23, QC_SPARSE_MAX_STATES);
        char buf[512];
        run_op(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false}", NULL, "u64->f32");
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
                 u64,
                 u64);
        run_op(c, buf, NULL, "u64->f32");
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":2,\"width\":32,\"signed\":false,\"float\":true}",
               NULL,
               "u64->f32");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":false,\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"id\":1}]}}",
            NULL,
            "u64->f32");
        uint64_t got = 0;
        run_op(c, "{\"op\":\"qmeasure\",\"id\":2,\"result\":\"r\"}", &got, "u64->f32");
        volatile float wf = (float)u64;
        float wf_v = wf;
        check((uint32_t)got == f32_bits(wf_v), "u64->f32 single-round", "mismatch");
        qc_destroy(c);
    }
    {
        qc_context *c = qc_create_sparse(25, QC_SPARSE_MAX_STATES);
        char buf[512];
        int64_t imin = INT64_MIN;
        run_op(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":true}", NULL, "i64min->f64");
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRId64 ",\"upper\":%" PRId64 "}",
                 imin,
                 imin);
        run_op(c, buf, NULL, "i64min->f64");
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               "i64min->f64");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":64,\"signed\":false,\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":true,\"id\":1}]}}",
            NULL,
            "i64min->f64");
        uint64_t got = 0;
        run_op(c, "{\"op\":\"qmeasure\",\"id\":2,\"result\":\"r\"}", &got, "i64min->f64");
        volatile double wd = (double)imin;
        double wd_v = wd;
        check(got == f64_bits(wd_v), "INT64_MIN->f64", "mismatch");
        run_op(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":32,\"signed\":true}", NULL, "f64->i32");
        snprintf(
            buf,
            sizeof buf,
            "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64
            "}]}}",
            f64_bits(-42.75));
        run_op(c, buf, NULL, "f64->i32");
        run_op(c, "{\"op\":\"qmeasure\",\"id\":3,\"result\":\"ri\"}", &got, "f64->i32");
        check((int32_t)(uint32_t)got == -42, "-42.75 -> int32 == -42", "mismatch");
        qc_destroy(c);
    }

    /* double -> int64 & -0.9 -> uint32 & not(-0.0) & float UB rejection */
    {
        qc_context *c = qc_create_sparse(29, QC_SPARSE_MAX_STATES);
        char buf[512];
        uint64_t dneg = f64_bits(-0.9);
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               "d->u32");
        snprintf(buf,
                 sizeof buf,
                 "{\"op\":\"qprepare\",\"id\":1,\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
                 dneg,
                 dneg);
        run_op(c, buf, NULL, "d->u32");
        run_op(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":32,\"signed\":false}", NULL, "d->u32");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":1}]}}",
            NULL,
            "d->u32");
        uint64_t got = 99;
        run_op(c, "{\"op\":\"qmeasure\",\"id\":2,\"result\":\"r\"}", &got, "d->u32");
        check(got == 0, "-0.9 -> uint32 == 0", "mismatch");
        run_op(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}", NULL, "bool cast");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"not\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":32,\"signed\":false,\"float\":true,\"value\":2147483648}]}}",
            NULL,
            "!-0.0f");
        run_op(c, "{\"op\":\"qmeasure\",\"id\":3,\"result\":\"b\"}", &got, "!-0.0f");
        check(got == 1, "!(-0.0f) == 1", "mismatch");
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":4,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               "fneg");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":4,\"expr\":{\"op\":\"fneg\",\"bits\":64,\"signed\":false,\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":1}]}}",
            NULL,
            "fneg");
        run_op(c, "{\"op\":\"qmeasure\",\"id\":4,\"result\":\"fn\"}", &got, "fneg");
        check(got == f64_bits(0.9), "fneg(-0.9) == 0.9", "mismatch");
        run_op(c,
               "{\"op\":\"qalloc\",\"id\":5,\"width\":64,\"signed\":false,\"float\":true}",
               NULL,
               "fabs");
        run_op(
            c,
            "{\"op\":\"qcompute\",\"id\":5,\"expr\":{\"op\":\"fabs\",\"bits\":64,\"signed\":false,\"float\":true,\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":1}]}}",
            NULL,
            "fabs");
        run_op(c, "{\"op\":\"qmeasure\",\"id\":5,\"result\":\"fa\"}", &got, "fabs");
        check(got == f64_bits(0.9), "fabs(-0.9) == 0.9", "mismatch");
        qc_destroy(c);
    }

    /* Strict C11 UB rejections */
    {
        const char *nan_to_i32[] = {
            "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":true}",
            "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"const\",\"bits\":32,\"signed\":false,\"float\":true,\"value\":2143289344}]}}",
        };
        const char *neg_to_u32[] = {
            "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false}",
            "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":32,\"signed\":false,\"float\":true,\"value\":3212836864}]}}",
        };
        const char *ovf_i32[] = {
            "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":true}",
            "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":4746794007248502784}]}}", /* 2^31 */
        };
        const char *ovf_u64[] = {
            "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false}",
            "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":64,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":4895412794951729152}]}}", /* 2^64 */
        };
        expect_fail(nan_to_i32, 2, "NaN -> int32 UB");
        expect_fail(neg_to_u32, 2, "-1.0 -> uint32 UB");
        expect_fail(ovf_i32, 2, "2^31 -> int32 UB");
        expect_fail(ovf_u64, 2, "2^64 -> uint64 UB");
    }
}

static void test_superposition_phase_nested_and_binding(void) {
    /* Entangled superposition: (a + b) * c - d in 64-bit double with pi/3 relative phase,
     * staged within QC_MAX_PHYSICAL = 1024, and exact unitary inverse via quncompute! */
    qc_context *c = qc_create_sparse(31, QC_SPARSE_MAX_STATES);
    check(c != NULL, "superposition", "create sparse");
    run_op(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}", NULL, "ctrl");
    run_op(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}", NULL, "H ctrl");
    run_op(c,
           "{\"op\":\"qphase\",\"angle\":1.0471975511965976,\"controls\":[{\"id\":1,\"value\":1}]}",
           NULL,
           "phase");

    double a0 = 1.25, a1 = -3.5, b0 = 2.5, b1 = 0.75, k0 = 4.0, d0 = 0.5;
    char buf[4096];
    run_op(
        c, "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":false,\"float\":true}", NULL, "a");
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"select\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":1},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "}]}}",
        f64_bits(a1),
        f64_bits(a0));
    run_op(c, buf, NULL, "compute a");

    run_op(
        c, "{\"op\":\"qalloc\",\"id\":3,\"width\":64,\"signed\":false,\"float\":true}", NULL, "b");
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"select\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":1},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "}]}}",
        f64_bits(b1),
        f64_bits(b0));
    run_op(c, buf, NULL, "compute b");

    /* Bound qprepare / qcompute with float parameter */
    run_op(
        c, "{\"op\":\"qalloc\",\"id\":4,\"width\":64,\"signed\":false,\"float\":true}", NULL, "k");
    uint64_t bind_val = f64_bits(k0);
    check(
        qc_apply_bound(
            c,
            "{\"op\":\"qbind\",\"parameters\":[{\"id\":99,\"bits\":64,\"signed\":false,\"float\":true}],"
            "\"instruction\":{\"op\":\"qcompute\",\"id\":4,\"expr\":{\"op\":\"parameter\",\"id\":99,\"bits\":64,\"signed\":false,\"float\":true}}}",
            &bind_val,
            1,
            NULL),
        "qbind float param",
        qc_error(c));

    /* res = (a + b) * k - d0 */
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":5,\"width\":64,\"signed\":false,\"float\":true}",
           NULL,
           "res");
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":5,\"expr\":{\"op\":\"fsub\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"fmul\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"fadd\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":2},"
        "{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":3}]},"
        "{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":4}]},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "}]}}",
        f64_bits(d0));
    run_op(c, buf, NULL, "nested double expr");

    check(qc_qubits(c) <= 1024, "physical qubit budget", "exceeded 1024");
    check(qc_sparse_size(c) == 2, "entangled branch count", "expected 2");

    uint64_t w0[16] = {0}, w1[16] = {0};
    double complex amp0 = 0, amp1 = 0;
    check(qc_sparse_entry_words(c, 0, w0, 16, &amp0), "entry 0", qc_error(c));
    check(qc_sparse_entry_words(c, 1, w1, 16, &amp1), "entry 1", qc_error(c));
    /* Ensure zero garbage on all wires >= 257 (id1=1, id2=64, id3=64, id4=64, id5=64 => 257 wires)
     */
    check((w0[4] & ~UINT64_C(1)) == 0 && (w1[4] & ~UINT64_C(1)) == 0,
          "garbage upper bits in word 4",
          "nonzero ancilla");
    for (size_t u = 5; u < 16; ++u) {
        check(w0[u] == 0 && w1[u] == 0, "garbage upper words", "nonzero ancilla");
    }
    unsigned b_ctrl0 = (unsigned)(w0[0] & 1u);
    volatile double want0_val = (a0 + b0) * k0 - d0;
    volatile double want1_val = (a1 + b1) * k0 - d0;
    uint64_t res0_got = read_wire_range(w0, 193, 64);
    uint64_t res1_got = read_wire_range(w1, 193, 64);
    check(res0_got == f64_bits(b_ctrl0 ? want1_val : want0_val), "branch 0 nested f64", "mismatch");
    check(res1_got == f64_bits(b_ctrl0 ? want0_val : want1_val), "branch 1 nested f64", "mismatch");
    /* Check relative phase pi/3 between ctrl=0 and ctrl=1 branches */
    double complex ratio = b_ctrl0 == 0 ? (amp1 / amp0) : (amp0 / amp1);
    check(fabs(carg(ratio) - (k_pi / 3.0)) < 1e-9 && fabs(cabs(ratio) - 1.0) < 1e-9,
          "relative phase preservation",
          "phase drift");
    check(read_wire_bits(w0, 1) != read_wire_bits(w1, 1), "ctrl bits", "same");

    /* Exact unitary uncompute back to |a, b, k, 0> */
    run_op(c, "{\"op\":\"quncompute\",\"id\":5,\"saved\":true}", NULL, "uncompute res");
    run_op(c, "{\"op\":\"qrelease\",\"id\":5}", NULL, "release res");
    run_op(c, "{\"op\":\"quncompute\",\"id\":4,\"saved\":true}", NULL, "uncompute k");
    run_op(c, "{\"op\":\"qrelease\",\"id\":4}", NULL, "release k");
    run_op(c, "{\"op\":\"quncompute\",\"id\":3,\"saved\":true}", NULL, "uncompute b");
    run_op(c, "{\"op\":\"qrelease\",\"id\":3}", NULL, "release b");
    run_op(c, "{\"op\":\"quncompute\",\"id\":2,\"saved\":true}", NULL, "uncompute a");
    run_op(c, "{\"op\":\"qrelease\",\"id\":2}", NULL, "release a");
    /* Verify result amplitude is still (|0> + e^{i*pi/3}|1>)/sqrt(2) on wire 0! */
    check(qc_sparse_size(c) == 2, "uncomputed state count", "expected 2");
    check(qc_sparse_entry_words(c, 0, w0, 16, &amp0), "post entry 0", qc_error(c));
    check(qc_sparse_entry_words(c, 1, w1, 16, &amp1), "post entry 1", qc_error(c));
    ratio = (w0[0] & 1u) == 0 ? (amp1 / amp0) : (amp0 / amp1);
    check(fabs(carg(ratio) - (k_pi / 3.0)) < 1e-9, "post uncompute phase", "phase corrupted");
    uint64_t m_ctrl = 0;
    run_op(c, "{\"op\":\"qmeasure\",\"id\":1,\"result\":\"c\"}", &m_ctrl, "measure ctrl");
    /* Check res expected arithmetic independently */
    volatile double exp0 = (a0 + b0) * k0 - d0;
    volatile double exp1 = (a1 + b1) * k0 - d0;
    (void)exp0;
    (void)exp1;
    /* Recompute res directly on measured branch to confirm bit-exact nested result too */
    run_op(
        c, "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":false,\"float\":true}", NULL, "a2");
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"select\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":1},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "}]}}",
        f64_bits(a1),
        f64_bits(a0));
    run_op(c, buf, NULL, "a2");
    run_op(
        c, "{\"op\":\"qalloc\",\"id\":3,\"width\":64,\"signed\":false,\"float\":true}", NULL, "b2");
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"select\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":1},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "}]}}",
        f64_bits(b1),
        f64_bits(b0));
    run_op(c, buf, NULL, "b2");
    run_op(
        c, "{\"op\":\"qalloc\",\"id\":4,\"width\":64,\"signed\":false,\"float\":true}", NULL, "k2");
    check(
        qc_apply_bound(
            c,
            "{\"op\":\"qbind\",\"parameters\":[{\"id\":99,\"bits\":64,\"signed\":false,\"float\":true}],"
            "\"instruction\":{\"op\":\"qcompute\",\"id\":4,\"expr\":{\"op\":\"parameter\",\"id\":99,\"bits\":64,\"signed\":false,\"float\":true}}}",
            &bind_val,
            1,
            NULL),
        "bind k2",
        qc_error(c));
    run_op(c,
           "{\"op\":\"qalloc\",\"id\":5,\"width\":64,\"signed\":false,\"float\":true}",
           NULL,
           "res2");
    snprintf(
        buf,
        sizeof buf,
        "{\"op\":\"qcompute\",\"id\":5,\"expr\":{\"op\":\"fsub\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"fmul\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"fadd\",\"bits\":64,\"signed\":false,\"float\":true,"
        "\"args\":[{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":2},"
        "{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":3}]},"
        "{\"op\":\"reg\",\"bits\":64,\"signed\":false,\"float\":true,\"id\":4}]},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"float\":true,\"value\":%" PRIu64 "}]}}",
        f64_bits(d0));
    run_op(c, buf, NULL, "res2");
    uint64_t got_res = 0;
    run_op(c, "{\"op\":\"qmeasure\",\"id\":5,\"result\":\"r\"}", &got_res, "measure res2");
    double want_branch = m_ctrl ? exp1 : exp0;
    check(got_res == f64_bits(want_branch), "nested f64 result parity", "mismatch");
    qc_destroy(c);
}

static void test_schema_rejections(void) {
    const char *bad_width[] = {
        "{\"op\":\"qalloc\",\"id\":1,\"width\":16,\"signed\":false,\"float\":true}"};
    const char *bad_signed[] = {
        "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":true,\"float\":true}"};
    const char *bad_prep_range[] = {
        "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false,\"float\":true}",
        "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}",
    };
    const char *bad_qft[] = {
        "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false,\"float\":true}",
        "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":0}",
        "{\"op\":\"qfourier\",\"id\":1,\"inverse\":false}",
    };
    const char *bad_int_add[] = {
        "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false,\"float\":true}",
        "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":0}",
        "{\"op\":\"qalloc\",\"id\":2,\"width\":32,\"signed\":false}",
        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"add\",\"bits\":32,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":32,\"signed\":false,\"float\":true,\"id\":1},{\"op\":\"const\",\"bits\":32,\"signed\":false,\"value\":1}]}}",
    };
    expect_fail(bad_width, 1, "bad float width 16");
    expect_fail(bad_signed, 1, "bad signed float");
    expect_fail(bad_prep_range, 2, "bad float interval");
    expect_fail(bad_qft, 3, "bad float qfourier");
    expect_fail(bad_int_add, 4, "bad int add on float");
}

int main(void) {
    fesetround(FE_TONEAREST);
    test_f32_arithmetic();
    test_f64_arithmetic();
    test_casts_and_unary();
    test_superposition_phase_nested_and_binding();
    test_schema_rejections();
    printf("%d floating-point checks passed\n", checks);
    return 0;
}
