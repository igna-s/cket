/* Linker wrappers observe actual native CPU bindings and the joint quantum
 * state before measurement. The reference uses C object bytes independently
 * of the compiler's parameter ordering and expression serialization. */
#include "../src/engine/internal.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static qc_context *snapshot_engine;
static unsigned char snapshot_reference[8];
static unsigned snapshot_count, snapshot_mutated, snapshot_bound, snapshot_checked;
static const unsigned char *snapshot_cpu_base;
static unsigned snapshot_queries, snapshot_reads, snapshot_expected_reads;
static uint64_t snapshot_seen;

static void snapshot_fail(const char *message);

static void snapshot_set_reference(const void *address, unsigned count) {
    const unsigned char *bytes = address;
    if (count > sizeof snapshot_reference)
        snapshot_fail("reference exceeds the fixture byte budget");
    snapshot_cpu_base = bytes;
    snapshot_count = count;
    memset(snapshot_reference, 0, sizeof snapshot_reference);
    for (unsigned i = 0; i < count; ++i) {
        snapshot_reference[i] = bytes[i];
        ++snapshot_expected_reads;
        if (SNAPSHOT_STRING && !bytes[i])
            break;
    }
}

static void snapshot_fail(const char *message) {
    fprintf(stderr, "byte snapshot state fixture: %s\n", message);
    exit(1);
}

extern unsigned char __real___cket_read_byte(const void *, unsigned char);

unsigned char __wrap___cket_read_byte(const void *address, unsigned char enabled) {
    uintptr_t offset = (uintptr_t)address - (uintptr_t)snapshot_cpu_base;
    if (snapshot_mutated || offset >= snapshot_count || (snapshot_seen & (UINT64_C(1) << offset)))
        snapshot_fail("CPU byte query repeated, escaped the selected view or followed mutation");
    if (SNAPSHOT_STRING && offset && !(snapshot_seen & (UINT64_C(1) << (offset - 1))))
        snapshot_fail("string byte query did not depend on the preceding byte");
    if (!!enabled != (offset < snapshot_expected_reads))
        snapshot_fail("CPU byte load was not disabled after its own NUL");
    snapshot_seen |= UINT64_C(1) << offset;
    ++snapshot_queries;
    snapshot_reads += !!enabled;
    return __real___cket_read_byte(address, enabled);
}

static void snapshot_query_counts(void) {
    if (snapshot_queries != snapshot_count || snapshot_reads != snapshot_expected_reads)
        snapshot_fail("selected CPU bytes were not queried and loaded exactly once");
}

static void snapshot_destroy(void) {
    qc_destroy(snapshot_engine);
    snapshot_engine = NULL;
}

static void snapshot_start(void) {
    if (snapshot_engine)
        return;
    snapshot_engine = qc_create(1, 7);
    if (!snapshot_engine || atexit(snapshot_destroy))
        snapshot_fail("context allocation/cleanup failed");
    qc_allow_tables(snapshot_engine, SNAPSHOT_TABLES);
}

static unsigned snapshot_predicate(unsigned x) {
    uint16_t word = (uint16_t)x;
    unsigned char byte = (unsigned char)x;
    const unsigned char *input = SNAPSHOT_WIDTH == 8 ? &byte : (const unsigned char *)&word;
    int difference = 0;
    for (unsigned i = 0; i < snapshot_count; ++i) {
        difference = (int)input[i] - snapshot_reference[i];
        if (difference || (SNAPSHOT_STRING && !input[i]))
            break;
    }
    return (!SNAPSHOT_CONTROL || x < 128) && difference <= 0;
}

static size_t snapshot_basis(const qc_register *r, unsigned x) {
    size_t result = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        result |= (size_t)((x >> bit) & 1) << r->wires[bit];
    return result;
}

static void snapshot_state(int result_live, int result_phase) {
    qc_register *input = qc_lookup(snapshot_engine, 0);
    qc_register *result = result_live ? qc_lookup(snapshot_engine, 1) : NULL;
    if (!input || input->width != SNAPSHOT_WIDTH ||
        (result_live && (!result || result->width != 1)) || snapshot_engine->measure ||
        snapshot_engine->reset)
        snapshot_fail("registers missing or implicit quantum channel");
    size_t mask = 0;
    for (unsigned bit = 0; bit < input->width; ++bit)
        mask |= (size_t)1 << input->wires[bit];
    if (result)
        mask |= (size_t)1 << result->wires[0];
    unsigned inputs = 1u << SNAPSHOT_WIDTH;
    for (size_t i = 0; i < qc_state_size(snapshot_engine); ++i) {
        unsigned x = 0;
        for (unsigned bit = 0; bit < input->width; ++bit)
            x |= (unsigned)((i >> input->wires[bit]) & 1) << bit;
        unsigned predicate = snapshot_predicate(x);
        size_t expected_basis =
            snapshot_basis(input, x) | (result ? snapshot_basis(result, predicate) : 0);
        double complex expected = 0;
        if (!(i & ~mask) && i == expected_basis)
            expected = cexp(I * ((x == 3 ? .31 : 0) + (result_phase && predicate ? .17 : 0))) /
                       sqrt((double)inputs);
        if (cabs(qc_amplitude(snapshot_engine, i) - expected) > 1e-10)
            snapshot_fail("complete amplitudes, input preservation or private zeros disagree");
    }
    ++snapshot_checked;
}

static int snapshot_target(const char *operation, const char *name) {
    cj_doc document = {0};
    uint64_t id;
    if (!cj_parse(&document, operation))
        snapshot_fail("invalid runtime operation");
    int token = cj_equal(&document, cj_get(&document, 0, "op"), "qbind")
                    ? cj_get(&document, 0, "instruction")
                    : 0;
    int matches = cj_equal(&document, cj_get(&document, token, "op"), name) &&
                  cj_u64(&document, cj_get(&document, token, "id"), &id) && id == 1;
    cj_free(&document);
    return matches;
}

unsigned long long
__wrap___cket_apply_values(const char *operation, unsigned long long count, ...) {
    snapshot_start();
    if (count > 128)
        snapshot_fail("binding count exceeds runtime contract");
    uint64_t values[128], result = 0;
    va_list arguments;
    va_start(arguments, count);
    for (unsigned long long i = 0; i < count; ++i)
        values[i] = va_arg(arguments, unsigned long long);
    va_end(arguments);
    if (!qc_apply_bound(snapshot_engine, operation, values, (size_t)count, &result))
        snapshot_fail(qc_error(snapshot_engine));
    if (snapshot_target(operation, "qcompute")) {
        if (snapshot_mutated || count != snapshot_count || ++snapshot_bound != 1)
            snapshot_fail("CPU bytes were not captured once at the comparison");
        snapshot_query_counts();
        snapshot_state(1, 0);
    }
    return result;
}

unsigned long long __wrap___cket_apply(const char *operation) {
    snapshot_start();
    int inverse = snapshot_target(operation, "quncompute");
    if (inverse) {
        if (!snapshot_mutated)
            snapshot_fail("inverse fixture did not change the CPU object first");
        snapshot_state(1, 1);
    }
    uint64_t result = 0;
    if (!qc_apply(snapshot_engine, operation, &result))
        snapshot_fail(qc_error(snapshot_engine));
    if (inverse)
        snapshot_state(0, 1);
    return result;
}

unsigned long long __wrap___cket_apply_phase(const char *operation, double angle) {
    snapshot_start();
    if (!qc_apply_phase(snapshot_engine, operation, angle))
        snapshot_fail(qc_error(snapshot_engine));
    return 0;
}

unsigned long long __wrap___cket_measure(unsigned long long id) {
    if (id || !snapshot_mutated)
        snapshot_fail("unexpected observation or mutation ordering");
    snapshot_state(0, 1);
    char operation[128];
    snprintf(operation, sizeof operation, "{\"op\":\"qmeasure\",\"id\":%llu}", id);
    uint64_t result = 0;
    if (!qc_apply(snapshot_engine, operation, &result))
        snapshot_fail(qc_error(snapshot_engine));
    return result;
}

static int snapshot_finish(void) {
    snapshot_query_counts();
    if (snapshot_bound != 1 || snapshot_checked != 4)
        snapshot_fail("did not inspect compute, saved inverse and pre-observation states");
    return 0;
}
