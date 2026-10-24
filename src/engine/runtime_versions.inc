/* A CPU iteration reuses source-level names, never the old quantum payload.
 * Saved inverses follow private identities when an owner is replaced. */
static int retire_runtime_owner(qc_context *c, uint32_t id) {
    qc_register *r = reg(c, id);
    if (!r || !r->alive)
        return 1;
    uint32_t history;
    if (!heap_register_identity(c, &history) || !transfer_owner(c, id, history))
        return 0;
    heap_retire(c, history);
    return 1;
}

static int runtime_version(qc_context *c, const cj_doc *d, int inst) {
    uint64_t id;
    cj_u64(d, cj_get(d, inst, "id"), &id);
    if (cj_equal(d, cj_get(d, inst, "op"), "qslot")) {
        uint64_t width;
        cj_u64(d, cj_get(d, inst, "width"), &width);
        if (!retire_runtime_owner(c, (uint32_t)id) || !collect_disposed(c))
            return 0;
        return allocate(c,
                        (uint32_t)id,
                        (unsigned)width,
                        cj_bool(d, cj_get(d, inst, "ancilla")),
                        cj_bool(d, cj_get(d, inst, "signed")));
    }
    qc_register *old = reg(c, (uint32_t)id);
    unsigned width = old->width;
    int sign = old->signed_type, ancillary = old->ancillary;
    char *expression = cj_copy(d, cj_get(d, inst, "expr"));
    if (!expression)
        return qc_fail(c, "cannot save runtime assignment expression");
    uint32_t fresh;
    int ok =
        heap_new_register(c, width, sign, ancillary, &fresh) && heap_compute(c, fresh, expression);
    free(expression);
    if (!ok)
        return 0;
    // Relabel only after computing: self reads and old dependent values must
    // resolve to the previous owner. Transport adds no gates or channels.
    if (!retire_runtime_owner(c, (uint32_t)id) || !transfer_owner(c, fresh, (uint32_t)id))
        return 0;
    qc_register *current = reg(c, (uint32_t)id);
    qc_mask seen = 0;
    for (unsigned i = 0; i < current->ndeps; ++i)
        taint_uniform(c, current->deps[i], &seen);
    return collect_disposed(c);
}
