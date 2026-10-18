/* Immutable ctype profiles are semantic inputs to branch/range proofs. */
#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("profile proof operation failed: %s", qc_error(c));
}

static void expression(char *out,
                       size_t size,
                       const char *name,
                       const char *left,
                       const char *right,
                       unsigned relation) {
    const char *input = "{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":["
                        "{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}]}";
    char condition[4096], divisor[2048];
    validation_path(condition,
                    sizeof condition,
                    "{\"op\":\"cast\",\"bits\":2,\"signed\":false,\"args\":["
                    "{\"op\":\"%s\",\"bits\":32,\"signed\":true%s,\"args\":[%s]}]}",
                    name,
                    left,
                    input);
    validation_path(divisor,
                    sizeof divisor,
                    "{\"op\":\"cast\",\"bits\":2,\"signed\":false,\"args\":["
                    "{\"op\":\"%s\",\"bits\":32,\"signed\":true%s,\"args\":[%s]}]}",
                    name,
                    right,
                    input);
    if (relation) {
        char previous[4096];
        validation_path(previous, sizeof previous, "%s", condition);
        validation_path(condition,
                        sizeof condition,
                        "{\"op\":\"%s\",\"bits\":1,\"signed\":false,\"args\":[%s,"
                        "{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":0}]}",
                        relation == 1 ? "ne" : "gt",
                        previous);
    }
    validation_path(out,
                    size,
                    "{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":["
                    "{\"op\":\"select\",\"bits\":2,\"signed\":false,\"args\":[%s,"
                    "{\"op\":\"div\",\"bits\":2,\"signed\":false,\"args\":["
                    "{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":1},%s]},"
                    "{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":0}]}]}",
                    condition,
                    divisor);
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1)
        validation_fail("usage: validate_profiles");

    const struct {
        const char *name, *left, *right;
        int valid, output_on_one;
    } cases[] = {
        {"ctype_isalpha", ",\"profile\":[[0,0,1]]", ",\"profile\":[[1,1,1]]", 0, 0},
        {"ctype_isalpha", ",\"profile\":[[1,1,1]]", ",\"profile\":[[0,0,1]]", 0, 0},
        {"ctype_tolower", ",\"profile\":[[0,1,1]]", "", 0, 0},
        {"ctype_toupper", "", ",\"profile\":[[0,1,1]]", 0, 0},
        {"ctype_isalpha", ",\"profile\":[[0,0,1]]", ",\"profile\": [ [ 0, 0, 1 ] ]", 1, 0},
        {"ctype_isalpha", ",\"profile\":[[1,1,1]]", ",\"profile\":[[1,1,1]]", 1, 1},
        {"ctype_toupper", "", ",\"profile\":[[97,122,32]]", 1, 1},
        {"ctype_tolower", ",\"profile\":[[65,90,32]]", "", 1, 1},
    };

    unsigned checks = 0;
    for (unsigned tables = 0; tables < 2; ++tables)
        for (unsigned simulate = 0; simulate < 2; ++simulate)
            for (unsigned relation = 0; relation < 3; ++relation)
                for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
                    qc_context *c = qc_create((int)simulate, 7);
                    if (!c)
                        validation_fail("profile proof allocation failed");
                    qc_allow_tables(c, tables);
                    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
                    apply(
                        c,
                        "{\"op\":\"qphase\",\"angle\":0.31,\"controls\":[{\"id\":0,\"value\":1}]}");
                    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
                    char expr[8192], operation[9000];
                    expression(
                        expr, sizeof expr, cases[i].name, cases[i].left, cases[i].right, relation);
                    validation_path(operation,
                                    sizeof operation,
                                    "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}",
                                    expr);
                    int ok = qc_apply(c, operation, NULL);
                    if (!cases[i].valid) {
                        if (ok || !(strstr(qc_error(c), "division by zero") ||
                                    strstr(qc_error(c), "truth tables are disabled")))
                            validation_fail(
                                "profile proof accepted reachable UB: case=%u relation=%u tables=%u simulate=%u: %s",
                                i,
                                relation,
                                tables,
                                simulate,
                                qc_error(c));
                    } else {
                        if (!ok)
                            validation_fail(
                                "equal-profile guarded division rejected: case=%u relation=%u tables=%u simulate=%u: %s",
                                i,
                                relation,
                                tables,
                                simulate,
                                qc_error(c));
                        if (simulate)
                            for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                                size_t zero = cases[i].output_on_one ? 0 : 2;
                                size_t one = cases[i].output_on_one ? 3 : 1;
                                double complex expected = basis == zero  ? 1 / sqrt(2.)
                                                          : basis == one ? cexp(I * .31) / sqrt(2.)
                                                                         : 0;
                                if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                                    validation_fail(
                                        "guarded profile division changed the joint state");
                            }
                        validation_path(operation,
                                        sizeof operation,
                                        "{\"op\":\"quncompute\",\"id\":1,\"expr\":%s}",
                                        expr);
                        apply(c, operation);
                        apply(c, "{\"op\":\"qrelease\",\"id\":1}");
                        if (simulate)
                            for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                                double complex expected = basis == 0   ? 1 / sqrt(2.)
                                                          : basis == 1 ? cexp(I * .31) / sqrt(2.)
                                                                       : 0;
                                if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                                    validation_fail("guarded profile inverse left private garbage");
                            }
                        if (c->measure || c->reset)
                            validation_fail("profile proof introduced a hidden channel");
                    }
                    qc_destroy(c);
                    ++checks;
                }
    printf(
        "%u immutable profile proof checks passed; distinct locale functions cannot share branch facts.\n",
        checks);
    return 0;
}
