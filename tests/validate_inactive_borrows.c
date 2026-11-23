#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void require(int ok, const char *message, qc_context *c) {
    if (!ok) {
        fprintf(stderr, "inactive borrow: %s: %s\n", message, qc_error(c));
        exit(1);
    }
    ++checks;
}

static void apply(qc_context *c, const char *text) {
    require(qc_apply(c, text, NULL), text, c);
}

static qc_context *fixture(int flag) {
    qc_context *c = qc_create_sparse(7, 128);
    require(c != NULL, "context", c);
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":8,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
    apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":8,\"signed\":false,\"ancilla\":true}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":1,\"bits\":8,\"signed\":false}}");
    apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}");
    apply(c,
          flag ? "{\"op\":\"qprepare\",\"id\":3,\"lower\":1,\"upper\":1}"
               : "{\"op\":\"qprepare\",\"id\":3,\"lower\":0,\"upper\":0}");
    apply(c, "{\"op\":\"qalloc\",\"id\":4,\"width\":8,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":4,\"lower\":0,\"upper\":0}");
    return c;
}

static void guarded_certificate(int flag, int swap_computed) {
    qc_context *c = fixture(flag);
    size_t gates = c->ngates;
    uint64_t epoch = qc_lookup(c, 1)->epoch;
    apply(
        c,
        "{\"op\":\"qmaterialize\",\"id\":2,\"optional\":false,\"controls\":[{\"id\":3,\"value\":1}]}");
    require(!!qc_lookup(c, 2)->definition == !flag, "guarded certificate", c);
    require(c->ngates == gates, "metadata emitted a gate", c);
    char text[200];
    snprintf(text,
             sizeof text,
             "{\"op\":\"qswap\",\"id\":%u,\"other\":4,\"controls\":[{\"id\":3,\"value\":1}]}",
             swap_computed ? 2 : 1);
    apply(c, text);
    if (!flag) {
        require(c->ngates == gates && qc_lookup(c, 1)->epoch == epoch,
                "inactive swap mutated storage",
                c);
        apply(c, "{\"op\":\"quncompute\",\"id\":2,\"saved\":true}");
        apply(c, "{\"op\":\"qrelease\",\"id\":2}");
    }
    require(qc_sparse_size(c) == 2 && !c->measure && !c->reset, "support/channel", c);
    qc_destroy(c);
}

static void invalid_controls(void) {
    const char *guards[] = {"{}",
                            "[{\"id\":1,\"value\":1}]",
                            "[{\"id\":3,\"value\":2}]",
                            "[{\"id\":99,\"value\":0}]",
                            "[{\"id\":3,\"value\":0},{\"id\":3,\"value\":1}]",
                            "[{\"id\":3,\"value\":0,\"extra\":0}]"};
    for (unsigned i = 0; i < sizeof guards / sizeof *guards; ++i) {
        qc_context *c = fixture(0);
        char text[300];
        snprintf(text,
                 sizeof text,
                 "{\"op\":\"qmaterialize\",\"id\":2,\"optional\":false,\"controls\":%s}",
                 guards[i]);
        size_t gates = c->ngates;
        require(!qc_apply(c, text, NULL) && *qc_error(c), "malformed guard accepted", c);
        require(qc_lookup(c, 2)->definition && c->ngates == gates,
                "rejection changed certificate/state",
                c);
        qc_destroy(c);
    }
    qc_context *c = fixture(1);
    size_t gates = c->ngates;
    require(
        !qc_apply(c,
                  "{\"op\":\"qswap\",\"id\":2,\"other\":4,\"controls\":[{\"id\":3,\"value\":1}]}",
                  NULL),
        "active computed swap accepted",
        c);
    require(
        c->ngates == gates && qc_lookup(c, 2)->definition, "active rejection mutated storage", c);
    qc_destroy(c);
}

static void stale_guard(void) {
    qc_context *c = fixture(1);
    apply(c, "{\"op\":\"qalloc\",\"id\":5,\"width\":1,\"signed\":false,\"ancilla\":true}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":5,\"expr\":{\"op\":\"reg\",\"id\":3,\"bits\":1,\"signed\":false}}");
    qc_register *original = qc_lookup(c, 3), *saved = qc_lookup(c, 5);
    require(qc_emit(c, QC_X, original->wires[0], qc_mask_zero(), qc_mask_zero(), 0),
            "fixture mutation",
            c);
    ++original->epoch;
    original->basis_value = 0;
    original->zero_mask = 1;
    saved->basis_known = 0;
    saved->zero_mask = 0;
    size_t gates = c->ngates;
    apply(
        c,
        "{\"op\":\"qmaterialize\",\"id\":2,\"optional\":false,\"controls\":[{\"id\":5,\"value\":1}]}");
    require(!qc_lookup(c, 2)->definition && c->ngates == gates,
            "stale guard skipped a possible active write",
            c);
    qc_destroy(c);
}

int main(void) {
    guarded_certificate(0, 0);
    guarded_certificate(0, 1);
    guarded_certificate(1, 0);
    invalid_controls();
    stale_guard();
    printf(
        "%u inactive borrow checks passed; guarded certificates, inactive swaps, stale epochs and schema rejection have separate scopes.\n",
        checks);
    return 0;
}
