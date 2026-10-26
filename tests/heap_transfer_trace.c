#include "ownership_trace.c"

/* Observe bound CPU joins as well as the public allocation names. No engine
 * operation or byte load is substituted by these tracing wrappers. */
extern int __real_qc_apply(qc_context *, const char *, uint64_t *);
static char *public_names[4096];

/* Record the declared destination/history layout, not their amplitudes.
 * A reference can then predict masked old values when public wires stay put.
 * Existing consumers that do not model patches ignore the PATCH prefix. */
static void trace_patch(const char *operation, const uint64_t *values, size_t count) {
    cj_doc d = {0};
    if (!cj_parse(&d, operation))
        fail_trace("invalid patch trace operation");
    int bound = cj_equal(&d, cj_get(&d, 0, "op"), "qbind");
    int inst = bound ? cj_get(&d, 0, "instruction") : 0;
    if (cj_equal(&d, cj_get(&d, inst, "op"), "qpatch")) {
        uint64_t id, history, mask = 0;
        int node = cj_get(&d, inst, "bit_mask");
        if (!cj_u64(&d, cj_get(&d, inst, "id"), &id) ||
            !cj_u64(&d, cj_get(&d, inst, "history"), &history))
            fail_trace("missing patch trace identities");
        if (cj_equal(&d, cj_get(&d, node, "op"), "parameter")) {
            uint64_t parameter, current;
            int descriptors = cj_get(&d, 0, "parameters"), found = 0;
            if (!cj_u64(&d, cj_get(&d, node, "id"), &parameter) || descriptors < 0)
                fail_trace("missing patch mask binding");
            size_t index = 0;
            for (int t = descriptors + 1; t < d.tokens[descriptors].next;
                 t = d.tokens[t].next, ++index)
                if (cj_u64(&d, cj_get(&d, t, "id"), &current) && current == parameter) {
                    if (!values || index >= count)
                        fail_trace("unavailable patch mask binding");
                    mask = values[index];
                    found = 1;
                }
            if (!found)
                fail_trace("unknown patch mask binding");
        } else if (!cj_u64(&d, cj_get(&d, node, "value"), &mask))
            fail_trace("unavailable patch mask");
        qc_register *target = qc_lookup(observed, (uint32_t)id);
        qc_register *saved = qc_lookup(observed, (uint32_t)history);
        if (!target || !saved || !target->alive || !saved->alive)
            fail_trace("unavailable patch trace storage");
        printf("PATCH {\"bit_mask\":%" PRIu64 ",\"target_wires\":[", mask);
        for (unsigned bit = 0; bit < target->width; ++bit)
            printf("%s%u", bit ? "," : "", target->wires[bit]);
        fputs("],\"history_wires\":[", stdout);
        for (unsigned bit = 0; bit < saved->width; ++bit)
            printf("%s%u", bit ? "," : "", saved->wires[bit]);
        fputs("]}\n", stdout);
    }
    cj_free(&d);
}

static void free_public_names(void) {
    for (unsigned i = 0; i < 4096; ++i)
        free(public_names[i]);
}

static void trace_transfer_names(const char *operation) {
    static int registered;
    if (!registered) {
        if (atexit(free_public_names))
            fail_trace("cannot register transfer trace cleanup");
        registered = 1;
    }
    cj_doc d = {0};
    if (!cj_parse(&d, operation))
        fail_trace("invalid bound heap-transfer operation");
    int inst = cj_equal(&d, cj_get(&d, 0, "op"), "qbind") ? cj_get(&d, 0, "instruction") : 0;
    uint64_t allocated;
    int name = cj_get(&d, inst, "name");
    if ((cj_equal(&d, cj_get(&d, inst, "op"), "qalloc") ||
         cj_equal(&d, cj_get(&d, inst, "op"), "qslot")) &&
        name >= 0 && cj_u64(&d, cj_get(&d, inst, "id"), &allocated) && allocated < 4096) {
        free(public_names[allocated]);
        public_names[allocated] = cj_copy(&d, name);
    }
    if (cj_equal(&d, cj_get(&d, inst, "op"), "qphi")) {
        uint64_t id, first, second;
        int sources = cj_get(&d, inst, "sources");
        if (cj_u64(&d, cj_get(&d, inst, "id"), &id) && id < 4096 && sources >= 0 &&
            cj_u64(&d, sources + 1, &first) && cj_u64(&d, d.tokens[sources + 1].next, &second) &&
            first < 4096 && second < 4096) {
            const char *name = public_names[first] ? public_names[first] : public_names[second];
            char *copy = name ? strdup(name) : NULL;
            free(owner_names[id]);
            owner_names[id] = copy;
            free(public_names[id]);
            public_names[id] = copy ? strdup(copy) : NULL;
        }
    }
    cj_free(&d);
}

int __wrap_qc_apply(qc_context *context, const char *operation, uint64_t *result) {
    trace_transfer_names(operation);
    if (!strstr(operation, "\"qbind\""))
        trace_patch(operation, NULL, 0);
    return __real_qc_apply(context, operation, result);
}

extern int __real_qc_apply_bound(qc_context *, const char *, const uint64_t *, size_t, uint64_t *);

int __wrap_qc_apply_bound(qc_context *context,
                          const char *operation,
                          const uint64_t *values,
                          size_t count,
                          uint64_t *result) {
    trace_transfer_names(operation);
    trace_patch(operation, values, count);
    return __real_qc_apply_bound(context, operation, values, count, result);
}

static unsigned byte_reads;
extern unsigned char __real___cket_read_byte(const void *, unsigned char);

unsigned char __wrap___cket_read_byte(const void *address, unsigned char enabled) {
    byte_reads += enabled != 0;
    return __real___cket_read_byte(address, enabled);
}

unsigned heap_transfer_read_count(void) {
    return byte_reads;
}
