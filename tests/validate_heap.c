#include "internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "heap validation: %s\n", message);
        exit(1);
    }
    ++checks;
}

static void operation(qc_context *c, const char *text) {
    require(qc_apply(c, text, NULL), qc_error(c));
}

static void prepare(qc_context *c, uint32_t id, unsigned low, unsigned high) {
    // Establish controlled physical inputs on the allocator's zero registers.
    // This test fixture owns the corresponding internal value certificates.
    qc_register *r = qc_lookup(c, id);
    require(r && r->clean && (!low || low == high), "test input domain");
    for (unsigned bit = 0; bit < r->width; ++bit) {
        if (low == high && ((low >> bit) & 1))
            require(qc_emit(c, QC_X, r->wires[bit], 0, 0, 0), "basis preparation");
        if (low != high && ((high >> bit) & 1))
            require(qc_emit(c, QC_H, r->wires[bit], 0, 0, 0), "superposition preparation");
    }
    r->clean = low == high && !low;
    r->basis_known = low == high;
    r->basis_value = low;
    r->zero_mask = ((UINT64_C(1) << r->width) - 1) & ~high;
}

static void
state(qc_context *c, unsigned source_wire, unsigned other_wire, unsigned other, double angle) {
    for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
        double complex expected = 0;
        for (unsigned x = 0; x < 4; ++x)
            if (basis == (((size_t)x << source_wire) | ((size_t)other << other_wire)))
                expected = .5 * cexp(I * (x == 3 ? angle : 0));
        require(cabs(qc_amplitude(c, basis) - expected) < 1e-10,
                "resize/free changed the full phased state");
    }
}

static void resize_states(void) {
    qc_context *c = qc_create(1, 7);
    require(c != NULL, "context");
    uint64_t heap = qc_heap_allocate(c, 2, 4, 0);
    require(heap != 0, "allocate two registers");
    uint32_t first, second, third;
    require(qc_heap_element(c, heap, 0, &first) && qc_heap_element(c, heap, 1, &second),
            "initial identities");
    prepare(c, first, 0, 3);
    prepare(c, second, 9, 9);
    unsigned wire = qc_lookup(c, first)->wires[0], other = qc_lookup(c, second)->wires[0];
    qc_mask mask = QC_WIRE(wire) | QC_WIRE(wire + 1);
    require(qc_emit(c, QC_PHASE, 0, mask, mask, .31), "relative phase");
    state(c, wire, other, 9, .31);
    require(qc_heap_resize(c, heap, 3), "grow");
    uint32_t after;
    require(qc_heap_element(c, heap, 0, &after) && after == first, "grow transports first owner");
    require(qc_heap_element(c, heap, 1, &after) && after == second, "grow transports second owner");
    require(qc_heap_element(c, heap, 2, &third) && qc_lookup(c, third)->clean, "new zero storage");
    state(c, wire, other, 9, .31);
    require(qc_heap_resize(c, heap, 1), "shrink");
    require(!qc_lookup(c, second)->alive && !qc_lookup(c, third)->alive,
            "known truncated slots are cleaned and reclaimed");
    state(c, wire, other, 0, .31);
    require(qc_heap_free(c, heap), "free");
    require(qc_lookup(c, first)->alive && qc_lookup(c, first)->history,
            "unknown truncated state retained");
    state(c, wire, other, 0, .31);
    require(!c->measure && !c->reset && !qc_heap_count(c, heap), "no implicit channel");
    qc_destroy(c);
}

static void borrowed_inverse(void) {
    qc_context *c = qc_create(1, 7);
    require(c != NULL, "context");
    uint64_t heap = qc_heap_allocate(c, 2, 4, 0);
    uint32_t source, tail;
    require(heap && qc_heap_element(c, heap, 0, &source) && qc_heap_element(c, heap, 1, &tail),
            "borrow allocation");
    prepare(c, source, 0, 3);
    unsigned wire = qc_lookup(c, source)->wires[0], other = qc_lookup(c, tail)->wires[0];
    operation(c, "{\"op\":\"qalloc\",\"id\":7,\"width\":4,\"signed\":false,\"ancilla\":true}");
    char text[320];
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qcompute\",\"id\":7,\"expr\":{\"op\":\"bxor\",\"bits\":4,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":4,\"signed\":false,\"id\":%u},{\"op\":\"const\",\"bits\":4,\"signed\":false,\"value\":1}]}}",
        source);
    operation(c, text);
    require(qc_heap_free(c, heap), "free while a derived inverse is live");
    require(qc_lookup(c, source)->alive, "borrowed source stays live");
    operation(c, "{\"op\":\"quncompute\",\"id\":7,\"saved\":true}");
    operation(c, "{\"op\":\"qrelease\",\"id\":7}");
    state(c, wire, other, 0, 0);
    require(!c->measure && !c->reset, "borrow cleanup has no channel");
    qc_destroy(c);
}

static void resources(void) {
    qc_context *c = qc_create_sparse(7, 32);
    require(c != NULL, "sparse context");
    uint64_t heap = qc_heap_allocate(c, 2, 16, 0);
    uint32_t first, after;
    require(heap && qc_heap_element(c, heap, 0, &first), "resource allocation");
    require(!qc_heap_resize(c, heap, 9) && !*qc_error(c),
            "failed grow is a nonpoisoning resource result");
    require(qc_heap_count(c, heap) == 2 && qc_heap_element(c, heap, 0, &after) && first == after,
            "failed grow preserves old owner and extent");
    require(!qc_heap_allocate(c, 65, 1, 0) && !*qc_error(c), "bounded capacity");
    require(qc_heap_free(c, heap), "release zero payload");
    for (unsigned repeat = 0; repeat < 32; ++repeat) {
        heap = qc_heap_allocate(c, 64, 1, 0);
        require(heap && qc_heap_free(c, heap), "repeated allocations reuse clean wires");
    }
    require(c->nregisters <= 66 && c->physical <= 64, "metadata and physical identity reuse");
    require(!c->measure && !c->reset, "resource paths have no channels");
    qc_destroy(c);
}

int main(void) {
    resize_states();
    borrowed_inverse();
    resources();
    printf(
        "%u heap checks passed; dynamic identities, complete phased states, saved inverses, retained outputs and resource failure have separate scopes.\n",
        checks);
    return 0;
}
