#define _POSIX_C_SOURCE 200809L
#include "quantum.h"
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* The native program owns the quantum context; no subprocess or SDK. */
static qc_context *engine;
static void shutdown_heap(void);

_Noreturn void __cket_reject_quantum_entry(void) {
    fputs("|c> runtime: quantum function cannot use the classical calling convention\n", stderr);
    exit(EXIT_FAILURE);
}

static void shutdown_engine(void) {
    shutdown_heap();
    qc_destroy(engine);
    engine = NULL;
}

static void fail(void) {
    fprintf(stderr, "|c> runtime: %s\n", qc_error(engine));
    exit(EXIT_FAILURE);
}

static void start_engine(void) {
    if (engine)
        return;
    uint64_t seed = (uint64_t)time(NULL) ^ (uint64_t)getpid();
    const char *configured = getenv("CKET_SEED");
    if (configured && *configured) {
        char *end;
        errno = 0;
        seed = strtoull(configured, &end, 10);
        if (errno || end == configured || *end || *configured == '-') {
            fputs("|c> runtime: invalid CKET_SEED\n", stderr);
            exit(EXIT_FAILURE);
        }
    }
    const char *simulator = getenv("CKET_SIMULATOR");
    if (simulator && *simulator && strcmp(simulator, "dense") && strcmp(simulator, "sparse")) {
        fputs("|c> runtime: CKET_SIMULATOR must be dense or sparse\n", stderr);
        exit(EXIT_FAILURE);
    }
    if (simulator && !strcmp(simulator, "sparse")) {
        size_t limit = 65536;
        const char *states = getenv("CKET_SPARSE_STATES");
        if (states) {
            char *end;
            errno = 0;
            unsigned long long value = strtoull(states, &end, 10);
            if (errno || end == states || *end || *states == '-' || !value ||
                value > QC_SPARSE_MAX_STATES) {
                fputs("|c> runtime: CKET_SPARSE_STATES must be 1..1048576\n", stderr);
                exit(EXIT_FAILURE);
            }
            limit = (size_t)value;
        }
        engine = qc_create_sparse(seed, limit);
    } else
        engine = qc_create(1, seed);
    if (!engine)
        fail();
    qc_ctype_runtime(engine);
    if (atexit(shutdown_engine)) {
        fputs("|c> runtime: cannot register cleanup\n", stderr);
        exit(EXIT_FAILURE);
    }
}

#include "heap_runtime.inc"

unsigned long long __cket_controls_possible(const char *envelope) {
    int possible;
    start_engine();
    if (!qc_controls_possible(engine, envelope, &possible))
        fail();
    return (unsigned long long)possible;
}

uint64_t __cket_apply(const char *operation) {
    uint64_t result;
    start_engine();
    if (!qc_apply(engine, operation, &result))
        fail();
    return result;
}

uint64_t __cket_apply_arg(const char *operation, uint64_t argument) {
    start_engine();
    if (!qc_apply_arg(engine, operation, argument))
        fail();
    return 0;
}

/* Keep the conditional load opaque to caller object-size speculation. A false
 * enable permits an unreadable address; strncmp uses it after a CPU NUL. This
 * captures a classical byte and never starts or observes the quantum engine. */
__attribute__((noipa)) unsigned char __cket_read_byte(const void *address, unsigned char enabled) {
    if (!enabled)
        return 0;
    return *(const unsigned char *)address;
}

unsigned long long __cket_apply_values(const char *operation, unsigned long long count, ...) {
    uint64_t values[128], result;
    if (!count || count > 128) {
        fputs("|c> runtime: CPU binding count must be 1..128\n", stderr);
        exit(EXIT_FAILURE);
    }
    va_list arguments;
    va_start(arguments, count);
    for (unsigned long long i = 0; i < count; ++i)
        values[i] = va_arg(arguments, unsigned long long);
    va_end(arguments);
    start_engine();
    if (!qc_apply_bound(engine, operation, values, (size_t)count, &result))
        fail();
    return result;
}

uint64_t __cket_apply_phase(const char *operation, double angle) {
    start_engine();
    if (!qc_apply_phase(engine, operation, angle))
        fail();
    return 0;
}

uint64_t __cket_apply_index(const char *operation,
                            uint64_t index,
                            uint64_t has_argument,
                            uint64_t argument) {
    uint64_t result;
    start_engine();
    if (has_argument > 1 ||
        !qc_apply_index(engine, operation, index, (int)has_argument, argument, &result))
        fail();
    return result;
}

uint64_t __cket_measure(uint64_t id) {
    start_engine();
    char operation[128];
    uint64_t result;
    snprintf(operation, sizeof operation, "{\"op\":\"qmeasure\",\"id\":%" PRIu64 "}", id);
    if (!qc_apply(engine, operation, &result))
        fail();
    return result;
}

uint64_t __cket_observe(uint64_t id) {
    start_engine();
    char operation[128];
    uint64_t result;
    snprintf(operation, sizeof operation, "{\"op\":\"qobserve\",\"id\":%" PRIu64 "}", id);
    if (!qc_apply(engine, operation, &result))
        fail();
    return result;
}

/* ABI retained: storage placeholders are never CPU reads of quantum data. */
uint64_t __cket_prepare(uint64_t id, uint64_t width, uint64_t low, uint64_t high) {
    char operation[256];
    snprintf(operation,
             sizeof operation,
             "{\"op\":\"qalloc\",\"id\":%" PRIu64 ",\"width\":%" PRIu64 "}",
             id,
             width);
    __cket_apply(operation);
    snprintf(operation,
             sizeof operation,
             "{\"op\":\"qprepare\",\"id\":%" PRIu64 ",\"lower\":%" PRIu64 ",\"upper\":%" PRIu64 "}",
             id,
             low,
             high);
    return __cket_apply(operation);
}
