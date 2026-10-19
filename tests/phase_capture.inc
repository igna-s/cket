/* Inspect the actual compiled CPU bindings in a separate state context.
 * The same executable can delegate to the native runtime for domain checks.
 * Independent exported-circuit proofs remain a separate validation layer. */
#include "internal.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static qc_context *phase_test_context;
static int phase_test_capture, phase_test_preparation, phase_test_inverse;
static double phase_test_angle;

extern unsigned long long __real___cket_apply(const char *);
extern unsigned long long __real___cket_apply_values(const char *, unsigned long long, ...);
extern unsigned long long __real___cket_apply_arg(const char *, unsigned long long);
extern unsigned long long __real___cket_apply_phase(const char *, double);
extern unsigned long long __real___cket_measure(unsigned long long);
extern unsigned long long __real___cket_controls_possible(const char *);

static void phase_test_fail(const char *reason) {
    fprintf(stderr, "phase state check: %s\n", reason);
    qc_destroy(phase_test_context);
    exit(1);
}

static qc_context *phase_test_engine(void) {
    if (!phase_test_context) {
        phase_test_context = qc_create(1, 7);
        if (phase_test_context)
            phase_test_context->recording = 1;
    }
    if (!phase_test_context)
        phase_test_fail("cannot allocate state context");
    return phase_test_context;
}

unsigned long long __wrap___cket_controls_possible(const char *envelope) {
    if (!phase_test_capture)
        return __real___cket_controls_possible(envelope);
    int possible;
    if (!qc_controls_possible(phase_test_engine(), envelope, &possible))
        phase_test_fail(qc_error(phase_test_context));
    return (unsigned long long)possible;
}

unsigned long long __wrap___cket_apply(const char *operation) {
    if (!phase_test_capture)
        return __real___cket_apply(operation);
    if (!qc_apply(phase_test_engine(), operation, NULL))
        phase_test_fail(qc_error(phase_test_context));
    return 0;
}

unsigned long long
__wrap___cket_apply_values(const char *operation, unsigned long long count, ...) {
    if (!count || count > 128)
        phase_test_fail("invalid binding count");
    uint64_t values[128] = {0};
    va_list arguments;
    va_start(arguments, count);
    for (unsigned long long i = 0; i < count; ++i)
        values[i] = va_arg(arguments, unsigned long long);
    va_end(arguments);
    if (!phase_test_capture) {
        /* Current fixtures use at most two bindings. Forward their actual ABI
         * to the native runtime; extra initialized varargs are ignored. */
        if (count > 8)
            phase_test_fail("native test fixture exceeds its eight-binding budget");
        return __real___cket_apply_values(operation,
                                          count,
                                          (unsigned long long)values[0],
                                          (unsigned long long)values[1],
                                          (unsigned long long)values[2],
                                          (unsigned long long)values[3],
                                          (unsigned long long)values[4],
                                          (unsigned long long)values[5],
                                          (unsigned long long)values[6],
                                          (unsigned long long)values[7]);
    }
    if (!qc_apply_bound(phase_test_engine(), operation, values, (size_t)count, NULL))
        phase_test_fail(qc_error(phase_test_context));
    return 0;
}

unsigned long long __wrap___cket_apply_arg(const char *operation, unsigned long long value) {
    if (!phase_test_capture)
        return __real___cket_apply_arg(operation, value);
    if (!qc_apply_arg(phase_test_engine(), operation, value))
        phase_test_fail(qc_error(phase_test_context));
    return 0;
}

unsigned long long __wrap___cket_apply_phase(const char *operation, double angle) {
    if (!phase_test_capture)
        return __real___cket_apply_phase(operation, angle);
    if (!qc_apply_phase(phase_test_engine(), operation, angle))
        phase_test_fail(qc_error(phase_test_context));
    return 0;
}

unsigned long long __wrap___cket_measure(unsigned long long id) {
    if (!phase_test_capture) {
        qc_destroy(phase_test_context);
        phase_test_context = NULL;
        return __real___cket_measure(id);
    }
    qc_context *c = phase_test_engine();
    qc_register *source = qc_lookup(c, 0);
    if (id != 0 || !source || !source->alive || source->width != 1 || c->measure || c->reset)
        phase_test_fail("unexpected source register or implicit observation");
    for (size_t i = 0; i < c->nregisters; ++i)
        if (c->registers[i].alive && c->registers[i].id != 0)
            phase_test_fail("temporary register remains live at measurement");
    unsigned phases = 0;
    for (size_t i = 0; i < c->ngates; ++i) {
        if (c->gates[i].kind != QC_PHASE)
            continue;
        double expected = phases ? -phase_test_angle : phase_test_angle;
        if (memcmp(&c->gates[i].angle, &expected, sizeof expected))
            phase_test_fail("angle differs from the ordinary C reference bits");
        ++phases;
    }
    unsigned expected_phases = phase_test_preparation ? 1u + (unsigned)phase_test_inverse : 0u;
    if (phases != expected_phases)
        phase_test_fail("wrong number of selected phase operations");
    double angle = phase_test_inverse ? 0 : phase_test_angle;
    double complex expected[2] = {0, 0};
    if (phase_test_preparation < 0) {
        expected[0] = 1 / sqrt(2.0);
        expected[1] = cexp(I * angle) / sqrt(2.0);
    } else
        expected[phase_test_preparation] = cexp(I * angle * phase_test_preparation);
    size_t marked = (size_t)1 << source->wires[0];
    for (size_t i = 0; i < qc_state_size(c); ++i) {
        double complex amplitude = i == 0 ? expected[0] : i == marked ? expected[1] : 0;
        if (cabs(qc_amplitude(c, i) - amplitude) > 1e-10)
            phase_test_fail("complete complex state differs from its source reference");
    }
    qc_destroy(c);
    phase_test_context = NULL;
    return phase_test_preparation > 0;
}
