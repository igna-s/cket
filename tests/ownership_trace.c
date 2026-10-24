#define _POSIX_C_SOURCE 200809L
#include "../src/engine/internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Observe the real linked runtime. Operations and CPU bindings are not replaced.
 * Recording exposes the emitted gate stream to an independent test interpreter. */
static qc_context *observed;
static char *owner_names[4096];
static size_t gate_position;
static uint64_t retained = UINT64_MAX;

static void fail_trace(const char *message) {
    fprintf(stderr, "ownership trace: %s\n", message);
    exit(1);
}

static void free_names(void) {
    for (unsigned i = 0; i < 4096; ++i)
        free(owner_names[i]);
}

extern qc_context *__real_qc_create_sparse(uint64_t, size_t);

qc_context *__wrap_qc_create_sparse(uint64_t seed, size_t limit) {
    observed = __real_qc_create_sparse(seed, limit);
    if (!observed || atexit(free_names))
        fail_trace("cannot observe runtime context");
    observed->recording = 1;
    const char *tables = getenv("OWNERSHIP_TABLES");
    qc_allow_tables(observed, !tables || strcmp(tables, "0"));
    return observed;
}

extern uint64_t __real___cket_apply(const char *);

uint64_t __wrap___cket_apply(const char *operation) {
    cj_doc d = {0};
    uint64_t id;
    if (!cj_parse(&d, operation))
        fail_trace("invalid native operation");
    int op = cj_get(&d, 0, "op");
    if (cj_u64(&d, cj_get(&d, 0, "id"), &id) && id < 4096) {
        if (cj_equal(&d, op, "qalloc")) {
            int name = cj_get(&d, 0, "name");
            free(owner_names[id]);
            owner_names[id] = name < 0 ? NULL : cj_copy(&d, name);
            if (name < 0 && retained < 4096 && owner_names[retained]) {
                owner_names[id] = strdup(owner_names[retained]);
                free(owner_names[retained]);
                char history[80];
                snprintf(history, sizeof history, "\"__history_%" PRIu64 "\"", retained);
                owner_names[retained] = strdup(history);
            }
        } else if (cj_equal(&d, op, "qmove")) {
            int name = cj_get(&d, 0, "to");
            if (name >= 0) {
                free(owner_names[id]);
                owner_names[id] = cj_copy(&d, name);
            }
        }
        retained = cj_equal(&d, op, "qretain") ? id : UINT64_MAX;
    } else
        retained = UINT64_MAX;
    cj_free(&d);
    return __real___cket_apply(operation);
}

static void mask_json(qc_mask value) {
    printf("\"0x%016" PRIx64 "%016" PRIx64 "\"", (uint64_t)(value >> 64), (uint64_t)value);
}

void ownership_checkpoint(const char *tag) {
    if (!observed)
        fail_trace("checkpoint preceded native context creation");
    printf("{\"tag\":\"%s\",\"qubits\":%u,\"measurements\":%" PRIu64 ",\"resets\":%" PRIu64
           ",\"registers\":[",
           tag,
           observed->physical,
           observed->measure,
           observed->reset);
    unsigned count = 0;
    for (size_t i = 0; i < observed->nregisters; ++i) {
        const qc_register *r = &observed->registers[i];
        if (!r->alive)
            continue;
        printf("%s{\"id\":%u,\"name\":%s,\"history\":%d,\"known\":%d,\"known_value\":%" PRIu64
               ",\"wires\":[",
               count++ ? "," : "",
               r->id,
               r->id < 4096 && owner_names[r->id] ? owner_names[r->id] : "null",
               r->history,
               r->basis_known,
               r->basis_value);
        for (unsigned bit = 0; bit < r->width; ++bit)
            printf("%s%u", bit ? "," : "", r->wires[bit]);
        fputs("]}", stdout);
    }
    fputs("],\"heaps\":[", stdout);
    count = 0;
    for (const qc_heap *heap = observed->heaps; heap; heap = heap->next) {
        printf("%s{\"handle\":%" PRIu64 ",\"ids\":[", count++ ? "," : "", heap->handle);
        for (size_t slot = 0; slot < heap->count; ++slot)
            printf("%s%u", slot ? "," : "", heap->ids[slot]);
        fputs("]}", stdout);
    }
    fputs("],\"state\":[", stdout);
    for (size_t row = 0; row < qc_sparse_size(observed); ++row) {
        uint64_t low, high;
        double complex amplitude;
        if (!qc_sparse_entry(observed, row, &low, &high, &amplitude))
            fail_trace("cannot read recorded sparse state");
        if (row)
            putchar(',');
        putchar('[');
        mask_json(((qc_mask)high << 64) | low);
        printf(",%.17g,%.17g]", creal(amplitude), cimag(amplitude));
    }
    fputs("],\"gates\":[", stdout);
    for (size_t i = gate_position; i < observed->ngates; ++i) {
        const qc_gate *g = &observed->gates[i];
        printf("%s[%u,%u,", i == gate_position ? "" : ",", (unsigned)g->kind, g->target);
        mask_json(g->mask);
        putchar(',');
        mask_json(g->pattern);
        printf(",%.17g]", g->angle);
    }
    gate_position = observed->ngates;
    fputs("]}\n", stdout);
}
