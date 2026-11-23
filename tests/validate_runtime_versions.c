#include "../src/engine/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int condition, const char *message, qc_context *c) {
    if (!condition) {
        fprintf(stderr, "%s: %s\n", message, c ? qc_error(c) : "no context");
        exit(1);
    }
}

static void apply(qc_context *c, const char *operation) {
    uint64_t result;
    require(qc_apply(c, operation, &result), operation, c);
}

static qc_context *prepared(void) {
    qc_context *c = qc_create_sparse(7, 1024);
    require(c != NULL, "context allocation", c);
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
    return c;
}

static void schemas(void) {
    const char *bad[] = {
        "{\"op\":\"qassign\",\"id\":99,\"expr\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":0}}",
        "{\"op\":\"qassign\",\"id\":0,\"extra\":0,\"expr\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":0}}",
        "{\"op\":\"qassign\",\"id\":0,\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":0}}",
        "{\"op\":\"qassign\",\"id\":0,\"expr\":{\"op\":\"reg\",\"id\":99,\"bits\":1,\"signed\":false}}",
        "{\"op\":\"qslot\",\"id\":0,\"width\":0,\"signed\":false,\"ancilla\":true}",
        "{\"op\":\"qslot\",\"id\":0,\"width\":65,\"signed\":false,\"ancilla\":true}",
        "{\"op\":\"qslot\",\"id\":0,\"width\":1,\"signed\":true,\"ancilla\":true}",
        "{\"op\":\"qslot\",\"id\":0,\"width\":1,\"signed\":false}",
        "{\"op\":\"qslot\",\"id\":0,\"width\":8,\"signed\":false,\"ancilla\":true}",
        "{\"op\":\"qslot\",\"id\":4294967296,\"width\":1,\"signed\":false,\"ancilla\":true}",
    };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) {
        qc_context *c = prepared();
        unsigned physical = c->physical;
        size_t registers = c->nregisters, gates = c->ngates;
        uint64_t result;
        require(!qc_apply(c, bad[i], &result), "malformed version accepted", c);
        require(c->physical == physical && c->nregisters == registers && c->ngates == gates &&
                    qc_lookup(c, 0)->alive,
                "malformed version changed storage or gates",
                c);
        qc_destroy(c);
    }
}

static void transport(int slot, int tables) {
    qc_context *c = prepared();
    qc_allow_tables(c, tables);
    unsigned original = qc_lookup(c, 0)->wires[0];
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false,\"ancilla\":true}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}");
    if (slot) {
        apply(c, "{\"op\":\"qslot\",\"id\":0,\"width\":1,\"signed\":false,\"ancilla\":true}");
        apply(
            c,
            "{\"op\":\"qcompute\",\"id\":0,\"expr\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}");
    } else
        apply(
            c,
            "{\"op\":\"qassign\",\"id\":0,\"expr\":{\"op\":\"not\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}]}}");
    qc_register *dependent = qc_lookup(c, 1);
    require(dependent->ndeps == 1 && dependent->deps[0] != 0 &&
                qc_lookup(c, dependent->deps[0])->wires[0] == original,
            "saved inverse did not follow the old owner",
            c);
    apply(c, "{\"op\":\"quncompute\",\"id\":1,\"saved\":true}");
    apply(c, "{\"op\":\"qrelease\",\"id\":1}");
    apply(c, "{\"op\":\"qdispose\",\"id\":0}");
    require(!c->measure && !c->reset && qc_sparse_size(c) == 2,
            "owner transport introduced a channel",
            c);
    for (size_t i = 0; i < 2; ++i) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(c, i, &low, &high, &amplitude), "missing amplitude", c);
        require(!high && (low == 0 || low == (UINT64_C(1) << original)) &&
                    cabs(amplitude - 1 / sqrt(2.0)) < 1e-10,
                "saved inverse changed the original full state",
                c);
    }
    qc_destroy(c);
}

static void resource_limit(void) {
    qc_context *c = prepared();
    uint64_t result;
    const char *operation =
        "{\"op\":\"qassign\",\"id\":0,\"expr\":{\"op\":\"not\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}]}}";
    unsigned iterations = 0;
    while (iterations <= QC_MAX_PHYSICAL && qc_apply(c, operation, &result))
        ++iterations;
    char limit_msg[64];
    snprintf(limit_msg, sizeof limit_msg, "%d physical", QC_MAX_PHYSICAL);
    require(iterations == QC_MAX_PHYSICAL - 1 && c->physical == QC_MAX_PHYSICAL &&
                strstr(qc_error(c), limit_msg) && !c->measure && !c->reset,
            "runtime history did not enforce its physical budget",
            c);
    qc_destroy(c);
}

int main(void) {
    schemas();
    for (int tables = 0; tables < 2; ++tables)
        for (int slot = 0; slot < 2; ++slot)
            transport(slot, tables);
    resource_limit();
    printf(
        "PASS 10 schema rejections before mutation, 4 saved-inverse state transports and %d-wire history limit\n",
        QC_MAX_PHYSICAL);
    puts(
        "15 runtime history contracts passed; finite states, schema guards and resource bounds have separate scopes.");
    return 0;
}
