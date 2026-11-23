#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void require(int condition, const char *message, qc_context *c) {
    if (!condition) {
        fprintf(stderr, "heap identity: %s: %s\n", message, qc_error(c));
        exit(1);
    }
    ++checks;
}

static void apply(qc_context *c, const char *text) {
    require(qc_apply(c, text, NULL), text, c);
}

static qc_mask value_bits(qc_context *c, uint32_t id, uint64_t value) {
    qc_register *r = qc_lookup(c, id);
    qc_mask result = qc_mask_zero();
    require(r && r->alive, "live value owner", c);
    for (unsigned bit = 0; bit < r->width; ++bit)
        if ((value >> bit) & 1)
            result = qc_mask_or(result, QC_WIRE(r->wires[bit]));
    return result;
}

static qc_context *fixture(unsigned width, int sign, int high, uint64_t *heap, uint32_t *owner) {
    qc_context *c = qc_create_sparse(7, 1024);
    require(c != NULL, "context", c);
    *heap = qc_heap_allocate(c, 1, width, sign);
    require(*heap && qc_heap_element(c, *heap, 0, owner), "heap", c);
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":0}");
    qc_register *r = qc_lookup(c, *owner), *reference = qc_lookup(c, 1);
    for (unsigned bit = 1; high && bit < width; ++bit)
        require(qc_emit(c, QC_X, r->wires[bit], qc_mask_zero(), qc_mask_zero(), 0),
                "high preparation",
                c);
    require(qc_emit(c, QC_H, r->wires[0], qc_mask_zero(), qc_mask_zero(), 0), "unknown low bit", c);
    require(qc_emit(c, QC_X, reference->wires[0], QC_WIRE(r->wires[0]), QC_WIRE(r->wires[0]), 0),
            "entangled reference",
            c);
    require(
        qc_emit(c, QC_PHASE, 0, QC_WIRE(reference->wires[0]), QC_WIRE(reference->wires[0]), .61),
        "relative phase",
        c);
    r->clean = r->basis_known = r->uniform = 0;
    r->prepared = 1;
    r->zero_mask =
        high ? 0 : (width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1) & ~UINT64_C(1);
    reference->clean = reference->basis_known = reference->uniform = 0;
    reference->zero_mask = 0;
    return c;
}

static void identity_states(void) {
    for (unsigned width = 8; width <= 64; width *= 2)
        for (int sign = 0; sign < 2; ++sign)
            for (int high = 0; high < 2; ++high)
                for (int mode = 0; mode < 3; ++mode) {
                    uint64_t heap;
                    uint32_t owner;
                    qc_context *c = fixture(width, sign, high, &heap, &owner);
                    char text[1400];
                    const char *signed_text = sign ? "true" : "false";
                    if (!mode)
                        snprintf(
                            text,
                            sizeof text,
                            "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"width\":%u,\"signed\":%s,\"expr\":{\"op\":\"reg\",\"bits\":%u,\"signed\":%s,\"id\":%u}}",
                            (unsigned long long)heap,
                            width,
                            signed_text,
                            width,
                            signed_text,
                            owner);
                    else
                        snprintf(
                            text,
                            sizeof text,
                            "{\"op\":\"qheap_memory\",\"mode\":\"set\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},\"heap_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%u},\"count\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":1},\"big_endian\":%s,\"expr\":{\"op\":\"cast\",\"bits\":8,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":%u,\"signed\":%s,\"id\":%u}]}}",
                            (unsigned long long)heap,
                            mode == 2 ? width / 8 - 1 : 0,
                            mode == 2 ? "true" : "false",
                            width,
                            signed_text,
                            owner);
                    uint64_t epoch = qc_lookup(c, owner)->epoch;
                    apply(c, text);
                    uint32_t current;
                    require(qc_heap_element(c, heap, 0, &current) && current == owner &&
                                qc_lookup(c, owner)->epoch == epoch,
                            "identity changed the owner or its epoch",
                            c);
                    require(qc_sparse_size(c) == 2 && !c->measure && !c->reset,
                            "identity support/channel",
                            c);
                    uint64_t base = high ? (width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1) &
                                               ~UINT64_C(1)
                                         : 0;
                    for (size_t row = 0; row < 2; ++row) {
                        uint64_t low, upper;
                        double complex amplitude;
                        require(qc_sparse_entry(c, row, &low, &upper, &amplitude), "state row", c);
                        qc_mask basis = qc_mask_from_u128(low, upper);
                        unsigned bit = (unsigned)qc_mask_test(basis, qc_lookup(c, 1)->wires[0]);
                        qc_mask wanted =
                            qc_mask_or(value_bits(c, owner, base | bit), value_bits(c, 1, bit));
                        require(qc_mask_eq(basis, wanted) &&
                                    cabs(amplitude - cexp(I * (.61 * bit)) / sqrt(2.0)) < 1e-10,
                                "identity changed complete joint amplitudes or left private data",
                                c);
                    }
                    qc_destroy(c);
                }
}

static void conditional_recipe(int guarded) {
    uint64_t heap;
    uint32_t owner;
    qc_context *c = fixture(8, 0, 0, &heap, &owner);
    apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":2,\"lower\":0,\"upper\":1}");
    apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":8,\"signed\":false,\"ancilla\":true}");
    char text[1000];
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"reg\",\"bits\":8,\"signed\":false,\"id\":%u},\"controls\":[{\"id\":2,\"value\":1}]}",
        owner);
    apply(c, text);
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"width\":8,\"signed\":false,\"expr\":{\"op\":\"reg\",\"bits\":8,\"signed\":false,\"id\":3},\"controls\":%s}",
        (unsigned long long)heap,
        guarded ? "[{\"id\":2,\"value\":1}]" : "[]");
    apply(c, text);
    uint32_t current;
    require(qc_heap_element(c, heap, 0, &current), "conditional owner", c);
    require(
        guarded ? current == owner : current != owner, "conditional recipe escaped its guard", c);
    require(qc_sparse_size(c) == 4, "conditional support", c);
    for (size_t row = 0; row < 4; ++row) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(c, row, &low, &high, &amplitude), "conditional row", c);
        qc_mask basis = qc_mask_from_u128(low, high);
        unsigned bit = (unsigned)qc_mask_test(basis, qc_lookup(c, 1)->wires[0]);
        unsigned flag = (unsigned)qc_mask_test(basis, qc_lookup(c, 2)->wires[0]);
        qc_mask wanted = value_bits(c, owner, bit);
        wanted = qc_mask_or(wanted, value_bits(c, 1, bit));
        wanted = qc_mask_or(wanted, value_bits(c, 2, flag));
        wanted = qc_mask_or(wanted, value_bits(c, 3, flag ? bit : 0));
        wanted = qc_mask_or(wanted, value_bits(c, current, guarded ? bit : flag ? bit : 0));
        require(qc_mask_eq(basis, wanted) && cabs(amplitude - cexp(I * (.61 * bit)) / 2) < 1e-10,
                "conditional identity/nonidentity state",
                c);
    }
    if (guarded) {
        apply(
            c,
            "{\"op\":\"quncompute\",\"id\":3,\"saved\":true,\"controls\":[{\"id\":2,\"value\":1}]}");
        apply(c, "{\"op\":\"qrelease\",\"id\":3}");
    }
    require(!c->measure && !c->reset, "conditional implicit channel", c);
    qc_destroy(c);
}

static void stale_recipe(void) {
    uint64_t heap;
    uint32_t owner;
    qc_context *c = fixture(8, 0, 0, &heap, &owner);
    apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":8,\"signed\":false,\"ancilla\":true}");
    char text[1000];
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"reg\",\"bits\":8,\"signed\":false,\"id\":%u}}",
        owner);
    apply(c, text);
    qc_register *old = qc_lookup(c, owner);
    require(qc_emit(c, QC_X, old->wires[0], qc_mask_zero(), qc_mask_zero(), 0),
            "controlled fixture mutation",
            c);
    ++old->epoch; // Deliberately invalidate the saved inverse certificate.
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"width\":8,\"signed\":false,\"expr\":{\"op\":\"reg\",\"bits\":8,\"signed\":false,\"id\":3}}",
        (unsigned long long)heap);
    apply(c, text);
    uint32_t current;
    require(qc_heap_element(c, heap, 0, &current) && current != owner,
            "stale recipe elided a real write",
            c);
    for (size_t row = 0; row < qc_sparse_size(c); ++row) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(c, row, &low, &high, &amplitude), "stale row", c);
        qc_mask basis = qc_mask_from_u128(low, high);
        unsigned bit = (unsigned)qc_mask_test(basis, qc_lookup(c, 1)->wires[0]);
        qc_mask wanted = value_bits(c, owner, bit ^ 1);
        wanted = qc_mask_or(wanted, value_bits(c, 1, bit));
        wanted = qc_mask_or(wanted, value_bits(c, 3, bit));
        wanted = qc_mask_or(wanted, value_bits(c, current, bit));
        require(qc_mask_eq(basis, wanted) &&
                    cabs(amplitude - cexp(I * (.61 * bit)) / sqrt(2.0)) < 1e-10,
                "stale recipe changed real store semantics",
                c);
    }
    qc_destroy(c);
}

static void undefined_discard(void) {
    uint64_t heap;
    uint32_t owner;
    qc_context *c = fixture(8, 0, 0, &heap, &owner);
    char text[1500];
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"width\":8,\"signed\":false,\"expr\":{\"op\":\"sequence\",\"bits\":8,\"signed\":false,\"args\":[{\"op\":\"div\",\"bits\":8,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":8,\"signed\":false,\"id\":%u},{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":0}]},{\"op\":\"reg\",\"bits\":8,\"signed\":false,\"id\":%u}]}}",
        (unsigned long long)heap,
        owner,
        owner);
    require(!qc_apply(c, text, NULL) &&
                (strstr(qc_error(c), "zero") || strstr(qc_error(c), "undefined")),
            "identity hid a discarded undefined operation",
            c);
    qc_destroy(c);
}

int main(void) {
    identity_states();
    conditional_recipe(0);
    conditional_recipe(1);
    stale_recipe();
    undefined_discard();
    printf(
        "%u heap identity checks passed; complex joint states, byte order, guarded certificates, stale epochs and discarded undefined expressions have separate scopes.\n",
        checks);
    return 0;
}
