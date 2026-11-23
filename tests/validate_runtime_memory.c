#include "../src/engine/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(value) "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":" value "}"

static void require(int condition, const char *message, qc_context *c) {
    if (!condition) {
        fprintf(stderr, "%s: %s\n", message, c ? qc_error(c) : "no context");
        exit(1);
    }
}

static qc_context *prepared(int upper) {
    qc_context *c = qc_create_sparse(7, 64);
    require(c != NULL, "context allocation", c);
    uint64_t result;
    require(qc_apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}", &result),
            "allocate",
            c);
    require(qc_apply(c,
                     upper ? "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}"
                           : "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}",
                     &result),
            "prepare",
            c);
    return c;
}

int main(void) {
    const char *bad[] = {
        "{\"op\":\"cmemory\",\"count\":1,\"limit\":1}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") "}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":65}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":1,\"overlap\":65}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":1,\"extra\":0}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":1,\"limit\":1}",
        "{\"op\":\"cmemory\",\"count\":{\"op\":\"const\",\"bits\":32,\"signed\":false,\"value\":1},\"limit\":1}",
        "{\"op\":\"cmemory\",\"count\":{\"op\":\"const\",\"bits\":64,\"signed\":true,\"value\":1},\"limit\":1}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("2") ",\"limit\":1}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("18446744073709551615") ",\"limit\":64}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("2") ",\"limit\":2,\"overlap\":1}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":1,\"overlap\":0}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":1,\"controls\":false}",
        "{\"op\":\"cmemory\",\"count\":" COUNT(
            "1") ",\"limit\":1,\"controls\":[{\"id\":99,\"value\":1}]}",
        "{\"op\":\"cmemory\",\"count\":" COUNT(
            "1") ",\"limit\":1,\"controls\":[{\"id\":0,\"value\":2}]}",
        "{\"op\":\"cmemory\",\"count\":" COUNT(
            "1") ",\"limit\":1,\"controls\":[{\"id\":0,\"value\":1,\"extra\":0}]}",
        "{\"op\":\"cmemory\",\"count\":" COUNT(
            "1") ",\"limit\":1,\"controls\":[{\"id\":0,\"value\":1},{\"id\":0,\"value\":0}]}",
    };
    unsigned checks = 0;
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) {
        qc_context *c = prepared(0);
        size_t registers = c->nregisters, gates = c->ngates;
        unsigned physical = c->physical;
        uint64_t result;
        require(!qc_apply(c, bad[i], &result), "invalid extent accepted", c);
        require(c->physical == physical && c->nregisters == registers && c->ngates == gates &&
                    qc_amplitude(c, 0) == 1,
                "invalid extent changed quantum storage/state",
                c);
        qc_destroy(c);
        ++checks;
    }
    const char *valid[] = {
        "{\"op\":\"cmemory\",\"count\":" COUNT("0") ",\"limit\":0,\"overlap\":0}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("64") ",\"limit\":64}",
        "{\"op\":\"cmemory\",\"count\":" COUNT("1") ",\"limit\":2,\"overlap\":1}",
        "{\"op\":\"cmemory\",\"count\":" COUNT(
            "999") ",\"limit\":0,\"controls\":[{\"id\":0,\"value\":1}]}",
    };
    for (unsigned i = 0; i < sizeof valid / sizeof *valid; ++i) {
        qc_context *c = prepared(0);
        uint64_t result;
        require(qc_apply(c, valid[i], &result), "valid checked prefix rejected", c);
        require(c->physical == 1 && c->nregisters == 1 && qc_amplitude(c, 0) == 1,
                "extent validation altered quantum storage/state",
                c);
        qc_destroy(c);
        ++checks;
    }
    qc_context *c = prepared(1);
    uint64_t result;
    require(!qc_apply(c, valid[3], &result), "reachable invalid guarded extent accepted", c);
    qc_destroy(c);
    ++checks;
    c = prepared(0);
    const char *bound =
        "{\"op\":\"qbind\",\"instruction\":{\"op\":\"cmemory\",\"count\":{\"op\":\"parameter\",\"id\":0,\"bits\":64,\"signed\":false},\"limit\":2},\"parameters\":[{\"id\":0,\"bits\":64,\"signed\":false}]}";
    uint64_t value = 2;
    require(qc_apply_bound(c, bound, &value, 1, &result), "typed runtime extent rejected", c);
    require(c->physical == 1 && qc_amplitude(c, 0) == 1, "bound extent altered state", c);
    qc_destroy(c);
    ++checks;

    c = qc_create_sparse(7, 64);
    require(c != NULL, "quantum count cmemory context", c);
    require(
        qc_apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}", &result) &&
            qc_apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}", &result) &&
            qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false}", &result) &&
            qc_apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":3}", &result),
        "quantum count setup",
        c);
    size_t regs_before = c->nregisters, gates_before = c->ngates;
    unsigned phys_before = c->physical;
    const char *q_valid =
        "{\"op\":\"cmemory\",\"count\":{\"op\":\"reg\",\"id\":1,\"bits\":64,\"signed\":false},\"limit\":4,\"overlap\":3}";
    require(qc_apply(c, q_valid, &result), "proven 64-bit interval quantum count rejected", c);
    require(c->physical == phys_before && c->nregisters == regs_before && c->ngates == gates_before,
            "quantum count cmemory mutated state or gates",
            c);
    const char *q_bad_limit =
        "{\"op\":\"cmemory\",\"count\":{\"op\":\"reg\",\"id\":1,\"bits\":64,\"signed\":false},\"limit\":2}";
    require(!qc_apply(c, q_bad_limit, &result) && *qc_error(c),
            "out-of-bounds quantum count interval accepted",
            c);
    qc_destroy(c);
    ++checks;

    c = prepared(1);
    require(qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":4,\"signed\":false}", &result) &&
                qc_apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":3}", &result),
            "guarded quantum count setup",
            c);
    const char *q_guarded_ok =
        "{\"op\":\"cmemory\",\"count\":{\"op\":\"select\",\"bits\":64,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false},{\"op\":\"cast\",\"bits\":64,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":4,\"signed\":false}]},{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":60}]},\"limit\":3,\"controls\":[{\"id\":0,\"value\":1}]}";
    require(qc_apply(c, q_guarded_ok, &result), "active-arm bounded quantum count rejected", c);
    const char *q_guarded_bad =
        "{\"op\":\"cmemory\",\"count\":{\"op\":\"cast\",\"bits\":64,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":4,\"signed\":false}]},\"limit\":3,\"overlap\":2,\"controls\":[{\"id\":0,\"value\":1}]}";
    require(!qc_apply(c, q_guarded_bad, &result) &&
                strstr(qc_error(c), "overlapping quantum memcpy"),
            "active superposition overlap violation missed",
            c);
    qc_destroy(c);
    ++checks;
    printf(
        "%u runtime memory preflight contracts passed; schemas, bounds, overlap, typed binding and inactive controls checked before mutation.\n",
        checks);
    return 0;
}
