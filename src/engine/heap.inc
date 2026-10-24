/* Heap capacities are classical. The payload consists of owned quantum
 * register identities, never host copies of amplitudes or measured values. */
#define QC_HEAP_MAX_ELEMENTS 64u
#define QC_HEAP_FIRST_REGISTER UINT32_C(0x80000000)
#define QC_HEAP_LAST_REGISTER UINT32_C(0x8fffffff)

static qc_heap *heap_lookup(qc_context *c, uint64_t handle) {
    for (qc_heap *heap = c->heaps; heap; heap = heap->next)
        if (heap->handle == handle)
            return heap;
    return NULL;
}

size_t qc_heap_count(const qc_context *c, uint64_t handle) {
    for (const qc_heap *heap = c->heaps; heap; heap = heap->next)
        if (heap->handle == handle)
            return heap->count;
    return 0;
}

static int heap_room(qc_context *c, size_t count, unsigned width) {
    unsigned limit = c->simulate && !c->sparse_limit ? QC_MAX_SIMULATED : QC_MAX_PHYSICAL;
    return count <= QC_HEAP_MAX_ELEMENTS && c->physical <= limit &&
           count <= (c->nfree + limit - c->physical) / width;
}

static int heap_new_register(qc_context *c, unsigned width, int sign, uint32_t *id) {
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (!r->alive && r->id >= QC_HEAP_FIRST_REGISTER && r->id <= QC_HEAP_LAST_REGISTER) {
            *id = r->id;
            goto allocate_register;
        }
    }
    if (c->heap_register < QC_HEAP_FIRST_REGISTER)
        c->heap_register = QC_HEAP_FIRST_REGISTER;
    while (c->heap_register <= QC_HEAP_LAST_REGISTER && reg(c, c->heap_register))
        ++c->heap_register;
    if (c->heap_register > QC_HEAP_LAST_REGISTER)
        return qc_fail(c, "quantum heap identity budget exceeded");
    *id = c->heap_register++;
allocate_register:
    if (!allocate(c, *id, width, 0, sign))
        return 0;
    reg(c, *id)->prepared = 1;
    return 1;
}

uint64_t qc_heap_allocate(qc_context *c, size_t count, unsigned width, int sign) {
    if (*c->error)
        return 0;
    if (!width || width > 64 || (sign != 0 && sign != 1) || (width == 1 && sign))
        return qc_fail(c, "invalid quantum heap element type");
    if (!count || !heap_room(c, count, width))
        return 0;
    if (c->heap_serial == UINT64_MAX)
        return qc_fail(c, "quantum heap handle budget exceeded");
    qc_heap *heap = calloc(1, sizeof *heap);
    uint32_t *ids = malloc(count * sizeof *ids);
    if (!heap || !ids) {
        free(heap);
        free(ids);
        return 0;
    }
    size_t made = 0;
    while (made < count && heap_new_register(c, width, sign, &ids[made]))
        ++made;
    if (made != count) {
        // Fresh zero registers have no borrowers, even if a later allocation
        // failed. Release their wires without observing the quantum payload.
        for (size_t i = 0; i < made; ++i)
            release(c, reg(c, ids[i]));
        free(ids);
        free(heap);
        return 0;
    }
    heap->handle = ++c->heap_serial;
    heap->count = count;
    heap->width = width;
    heap->sign = sign;
    heap->ids = ids;
    heap->next = c->heaps;
    c->heaps = heap;
    return heap->handle;
}

int qc_heap_element(qc_context *c, uint64_t handle, size_t index, uint32_t *id) {
    qc_heap *heap = heap_lookup(c, handle);
    if (!heap || index >= heap->count || !id)
        return qc_fail(c, "quantum heap index is outside live storage");
    qc_register *r = reg(c, heap->ids[index]);
    if (!r || !r->alive || r->width != heap->width || r->signed_type != heap->sign)
        return qc_fail(c, "quantum heap register ownership was invalidated");
    *id = r->id;
    return 1;
}

static void heap_retire(qc_context *c, uint32_t id) {
    qc_register *r = reg(c, id);
    r->finished = r->history = 1;
    // collect_disposed waits for every saved inverse before reclaiming known
    // basis states. Unknown roots remain joint outputs for the context lifetime.
}

int qc_heap_resize(qc_context *c, uint64_t handle, size_t count) {
    if (*c->error)
        return 0;
    qc_heap *heap = heap_lookup(c, handle);
    if (!heap)
        return qc_fail(c, "realloc requires a live quantum heap owner");
    if (!count)
        return qc_heap_free(c, handle);
    if (count > QC_HEAP_MAX_ELEMENTS)
        return 0;
    size_t old_count = heap->count;
    for (size_t i = 0; i < old_count; ++i) {
        uint32_t id;
        if (!qc_heap_element(c, handle, i, &id))
            return 0;
    }
    if (count > old_count && !heap_room(c, count - old_count, heap->width))
        return 0;
    uint32_t *ids = malloc(count * sizeof *ids);
    if (!ids)
        return 0;
    size_t keep = count < old_count ? count : old_count;
    memcpy(ids, heap->ids, keep * sizeof *ids);
    size_t made = keep;
    while (made < count && heap_new_register(c, heap->width, heap->sign, &ids[made]))
        ++made;
    if (made != count) {
        for (size_t i = keep; i < made; ++i)
            release(c, reg(c, ids[i]));
        free(ids);
        return 0;
    }
    for (size_t i = count; i < old_count; ++i)
        heap_retire(c, heap->ids[i]);
    free(heap->ids);
    heap->ids = ids;
    heap->count = count;
    return collect_disposed(c);
}

int qc_heap_free(qc_context *c, uint64_t handle) {
    if (*c->error)
        return 0;
    if (!handle)
        return 1;
    qc_heap **slot = &c->heaps;
    while (*slot && (*slot)->handle != handle)
        slot = &(*slot)->next;
    if (!*slot)
        return qc_fail(c, "free requires a live quantum heap owner");
    qc_heap *heap = *slot;
    for (size_t i = 0; i < heap->count; ++i) {
        uint32_t id;
        if (!qc_heap_element(c, handle, i, &id))
            return 0;
    }
    for (size_t i = 0; i < heap->count; ++i)
        heap_retire(c, heap->ids[i]);
    *slot = heap->next;
    free(heap->ids);
    free(heap);
    return collect_disposed(c);
}

static int heap_borrowed(qc_context *c, uint32_t id) {
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (r->alive && r->definition)
            for (unsigned j = 0; j < r->ndeps; ++j)
                if (r->deps[j] == id)
                    return 1;
    }
    return 0;
}

static int heap_action(qc_context *c, const cj_doc *d, int inst, uint64_t *result) {
    if (c->structural)
        return 1;
    int guards = cj_get(d, inst, "controls");
    if (guards >= 0) {
        ar_builder proof = {.c = c, .d = d};
        for (int t = guards + 1; t < d->tokens[guards].next; t = d->tokens[t].next) {
            uint64_t id, wanted;
            int fixed;
            cj_u64(d, cj_get(d, t, "id"), &id);
            cj_u64(d, cj_get(d, t, "value"), &wanted);
            if (ar_register_boolean_constant(&proof, (uint32_t)id, &fixed, 0) &&
                fixed != (int)wanted)
                return 1;
            if (*c->error)
                return 0;
        }
    }
    uint64_t handle, offset, index;
    cj_u64(d, cj_get(d, cj_get(d, inst, "heap"), "value"), &handle);
    cj_u64(d, cj_get(d, cj_get(d, inst, "offset"), "value"), &offset);
    cj_u64(d, cj_get(d, cj_get(d, inst, "index"), "value"), &index);
    qc_heap *heap = heap_lookup(c, handle);
    uint64_t width;
    cj_u64(d, cj_get(d, inst, "width"), &width);
    if (!heap || offset >= heap->count || index >= heap->count - offset)
        return qc_fail(c, "quantum heap index is outside live storage");
    if (heap->width != width || heap->sign != cj_bool(d, cj_get(d, inst, "signed")))
        return qc_fail(c, "quantum heap access changes its allocated C element type");
    size_t slot = (size_t)(offset + index);
    uint32_t old;
    if (!qc_heap_element(c, handle, slot, &old))
        return 0;
    char text[200];
    int op = cj_get(d, inst, "op");
    if (!cj_equal(d, op, "qheap_store")) {
        int measure = cj_equal(d, op, "qheap_measure");
        snprintf(
            text, sizeof text, "{\"op\":\"%s\",\"id\":%u}", measure ? "qmeasure" : "qreset", old);
        if (!qc_apply(c, text, result))
            return 0;
        if (measure && c->simulate && !c->ignore_measurements) {
            qc_register *r = reg(c, old);
            r->basis_known = 1;
            r->basis_value = *result;
            r->zero_mask = bits_mask(r->width) & ~*result;
            r->clean = !*result;
        }
        return 1;
    }
    int expr_node = cj_get(d, inst, "expr");
    domain inputs;
    if (!collect_domain(c, d, expr_node, &inputs))
        return 0;
    int aliases = 0;
    for (unsigned i = 0; i < inputs.count; ++i)
        aliases |= inputs.sources[i].id == old;
    int controls_node = cj_get(d, inst, "controls");
    int controlled = controls_node >= 0 && d->tokens[controls_node].children;
    int reuse = reg(c, old)->clean && !heap_borrowed(c, old) && !aliases && !controlled;
    uint32_t destination = old;
    if (!reuse && !heap_new_register(c, heap->width, heap->sign, &destination))
        return 0;
    char *expr = cj_copy(d, expr_node);
    if (!expr)
        return qc_fail(c, "cannot allocate quantum heap store expression");
    const char *signed_text = heap->sign ? "true" : "false";
    if (controlled)
        for (int t = controls_node + 1; t < d->tokens[controls_node].next; t = d->tokens[t].next) {
            uint64_t id, value;
            cj_u64(d, cj_get(d, t, "id"), &id);
            cj_u64(d, cj_get(d, t, "value"), &value);
            size_t capacity = strlen(expr) + 512;
            char *selected = malloc(capacity);
            if (!selected) {
                free(expr);
                return qc_fail(c, "cannot allocate controlled heap store");
            }
            char inactive[160];
            snprintf(inactive,
                     sizeof inactive,
                     "{\"op\":\"reg\",\"bits\":%u,\"signed\":%s,\"id\":%u}",
                     heap->width,
                     signed_text,
                     old);
            snprintf(
                selected,
                capacity,
                "{\"op\":\"select\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":%u},%s,%s]}",
                heap->width,
                signed_text,
                (uint32_t)id,
                value ? expr : inactive,
                value ? inactive : expr);
            free(expr);
            expr = selected;
        }
    size_t capacity = strlen(expr) + 120;
    char *compute_text = malloc(capacity);
    if (!compute_text) {
        free(expr);
        return qc_fail(c, "cannot allocate heap compute operation");
    }
    snprintf(
        compute_text, capacity, "{\"op\":\"qcompute\",\"id\":%u,\"expr\":%s}", destination, expr);
    free(expr);
    int ok = qc_apply(c, compute_text, NULL);
    free(compute_text);
    if (!ok)
        return 0;
    qc_register *r = reg(c, destination);
    qc_mask seen = 0;
    for (unsigned i = 0; i < r->ndeps; ++i)
        taint_uniform(c, r->deps[i], &seen);
    // Persistent heap slots own their output. Saved consumers of previous slots
    // keep the old identities, so later stores never invalidate their inverse.
    clear_definition(r);
    heap->ids[slot] = destination;
    if (!reuse)
        heap_retire(c, old);
    return collect_disposed(c);
}
