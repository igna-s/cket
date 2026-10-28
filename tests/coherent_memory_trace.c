/* The program emits phase(0) identities at loop boundaries. Observe the real
 * runtime after those identities; no user CPU call is enabled in coherent C. */
#define __wrap___cket_apply coherent_memory_base_apply
#include "heap_transfer_trace.c"
#undef __wrap___cket_apply

uint64_t __wrap___cket_apply(const char *operation) {
    uint64_t result = coherent_memory_base_apply(operation);
    cj_doc d = {0};
    if (!cj_parse(&d, operation))
        fail_trace("invalid coherent memory marker");
    int inst = cj_equal(&d, cj_get(&d, 0, "op"), "qbind") ? cj_get(&d, 0, "instruction") : 0;
    uint64_t id, other;
    if (cj_equal(&d, cj_get(&d, inst, "op"), "qswap") && cj_bool(&d, cj_get(&d, inst, "rebind")) &&
        cj_u64(&d, cj_get(&d, inst, "id"), &id) && cj_u64(&d, cj_get(&d, inst, "other"), &other)) {
        qc_register *target = qc_lookup(observed, (uint32_t)id);
        qc_register *saved = qc_lookup(observed, (uint32_t)other);
        if (!target || !saved || !target->alive || !saved->alive)
            fail_trace("unavailable swap trace storage");
        printf("SWAP {\"target_wires\":[");
        for (unsigned bit = 0; bit < target->width; ++bit)
            printf("%s%u", bit ? "," : "", target->wires[bit]);
        fputs("],\"history_wires\":[", stdout);
        for (unsigned bit = 0; bit < saved->width; ++bit)
            printf("%s%u", bit ? "," : "", saved->wires[bit]);
        fputs("]}\n", stdout);
    }
    int angle = cj_get(&d, 0, "angle");
    int marker = cj_equal(&d, cj_get(&d, 0, "op"), "qphase") && angle >= 0 &&
                 cj_get(&d, 0, "angle_expr") < 0 && cj_double(&d, angle) == 0;
    cj_free(&d);
    if (marker)
        ownership_checkpoint("step");
    return result;
}
