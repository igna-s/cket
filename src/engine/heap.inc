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

static int heap_register_identity(qc_context *c, uint32_t *id) {
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (!r->alive && r->id >= QC_HEAP_FIRST_REGISTER && r->id <= QC_HEAP_LAST_REGISTER) {
            *id = r->id;
            return 1;
        }
    }
    if (c->heap_register < QC_HEAP_FIRST_REGISTER)
        c->heap_register = QC_HEAP_FIRST_REGISTER;
    while (c->heap_register <= QC_HEAP_LAST_REGISTER && reg(c, c->heap_register))
        ++c->heap_register;
    if (c->heap_register > QC_HEAP_LAST_REGISTER)
        return qc_fail(c, "quantum heap identity budget exceeded");
    *id = c->heap_register++;
    return 1;
}

static int heap_new_register(qc_context *c, unsigned width, int sign, int ancillary, uint32_t *id) {
    if (!heap_register_identity(c, id))
        return 0;
    if (!allocate(c, *id, width, ancillary, sign))
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
    while (made < count && heap_new_register(c, width, sign, 0, &ids[made]))
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
    while (made < count && heap_new_register(c, heap->width, heap->sign, 0, &ids[made]))
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

static int heap_action(qc_context *c, const cj_doc *d, int inst, uint64_t *result);

static int heap_compute(qc_context *c, uint32_t destination, const char *expression) {
    size_t length = strlen(expression);
    if (length > 64u * 1024u * 1024u)
        return qc_fail(c, "heap expression text budget exceeded");
    char *operation = malloc(length + 120);
    if (!operation)
        return qc_fail(c, "cannot allocate heap computation");
    snprintf(operation,
             length + 120,
             "{\"op\":\"qcompute\",\"id\":%u,\"expr\":%s}",
             destination,
             expression);
    int ok = qc_apply(c, operation, NULL);
    free(operation);
    return ok;
}

static char *heap_guard_expression(
    qc_context *c, const cj_doc *d, int inst, unsigned width, int sign, char *expression) {
    int controls_node = cj_get(d, inst, "controls");
    if (!expression)
        return NULL;
    if (controls_node < 0)
        return expression;
    for (int t = controls_node + 1; t < d->tokens[controls_node].next; t = d->tokens[t].next) {
        uint64_t id, value;
        cj_u64(d, cj_get(d, t, "id"), &id);
        cj_u64(d, cj_get(d, t, "value"), &value);
        size_t capacity = strlen(expression) + 512;
        if (capacity > 64u * 1024u * 1024u) {
            free(expression);
            qc_fail(c, "controlled heap expression text budget exceeded");
            return NULL;
        }
        char *selected = malloc(capacity);
        if (!selected) {
            free(expression);
            qc_fail(c, "cannot allocate guarded heap expression");
            return NULL;
        }
        char zero[100];
        snprintf(zero,
                 sizeof zero,
                 "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":0}",
                 width,
                 sign ? "true" : "false");
        snprintf(
            selected,
            capacity,
            "{\"op\":\"select\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":%u},%s,%s]}",
            width,
            sign ? "true" : "false",
            (uint32_t)id,
            value ? expression : zero,
            value ? zero : expression);
        free(expression);
        expression = selected;
    }
    return expression;
}

static int heap_capture(qc_context *c, const cj_doc *d, int inst, unsigned width, int sign) {
    int capture = cj_get(d, inst, "capture");
    if (capture < 0)
        return 1;
    uint64_t id;
    cj_u64(d, cj_get(d, capture, "id"), &id);
    char *expression =
        heap_guard_expression(c, d, inst, width, sign, cj_copy(d, cj_get(d, capture, "expr")));
    if (!expression)
        return qc_fail(c, "cannot allocate heap result capture");
    int ok = heap_compute(c, (uint32_t)id, expression);
    free(expression);
    return ok;
}

static int heap_scatter(qc_context *c, const cj_doc *d, int inst, qc_heap *heap, size_t offset) {
    size_t count = heap->count - offset;
    unsigned selector_bits = 1;
    while ((UINT64_C(1) << selector_bits) <= count)
        ++selector_bits;
    char *index = cj_copy(d, cj_get(d, inst, "index"));
    if (!index)
        return qc_fail(c, "cannot capture heap index");
    size_t capacity = strlen(index) * 2 + 1024;
    if (capacity > 64u * 1024u * 1024u) {
        free(index);
        return qc_fail(c, "heap selector text budget exceeded");
    }
    char *checked = malloc(capacity);
    if (!checked) {
        free(index);
        return qc_fail(c, "cannot allocate checked heap selector");
    }
    // Zero denotes an inactive operation. Active indices use 1..count, and
    // the original full-width range is checked before any narrowing or write.
    snprintf(
        checked,
        capacity,
        "{\"op\":\"select\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"lt\",\"bits\":1,\"signed\":false,\"args\":[%s,{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%zu}]},{\"op\":\"add\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":[%s]},{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":1}]},{\"op\":\"div\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":1},{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":0}]}]}",
        selector_bits,
        index,
        count,
        selector_bits,
        selector_bits,
        index,
        selector_bits,
        selector_bits,
        selector_bits,
        selector_bits);
    free(index);
    checked = heap_guard_expression(c, d, inst, selector_bits, 0, checked);
    if (!checked)
        return 0;
    uint32_t selector, value;
    int ok =
        heap_new_register(c, selector_bits, 0, 1, &selector) && heap_compute(c, selector, checked);
    free(checked);
    if (!ok)
        return 0;
    char *expression = heap_guard_expression(
        c, d, inst, heap->width, heap->sign, cj_copy(d, cj_get(d, inst, "expr")));
    if (!expression)
        return qc_fail(c, "cannot capture heap store value");
    ok = heap_new_register(c, heap->width, heap->sign, 1, &value) &&
         heap_compute(c, value, expression);
    free(expression);
    if (!ok || !heap_capture(c, d, inst, heap->width, heap->sign))
        return 0;
    // All reads above resolve against the original registers. Their saved
    // recipes keep those versions alive throughout every selected write.
    for (size_t slot = 0; slot < count; ++slot) {
        char operation[1000];
        uint32_t match;
        snprintf(
            operation,
            sizeof operation,
            "{\"op\":\"eq\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":%u,\"signed\":false,\"id\":%u},{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":%zu}]}",
            selector_bits,
            selector,
            selector_bits,
            slot + 1);
        if (!heap_new_register(c, 1, 0, 1, &match) || !heap_compute(c, match, operation))
            return 0;
        snprintf(
            operation,
            sizeof operation,
            "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%" PRIu64
            "},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%zu},\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"width\":%u,\"signed\":%s,\"expr\":{\"op\":\"reg\",\"bits\":%u,\"signed\":%s,\"id\":%u},\"controls\":[{\"id\":%u,\"value\":1}]}",
            heap->handle,
            offset + slot,
            heap->width,
            heap->sign ? "true" : "false",
            heap->width,
            heap->sign ? "true" : "false",
            value,
            match);
        if (!qc_apply(c, operation, NULL))
            return 0;
        snprintf(operation, sizeof operation, "{\"op\":\"qdispose\",\"id\":%u}", match);
        if (!qc_apply(c, operation, NULL))
            return 0;
    }
    uint32_t temporaries[] = {value, selector};
    for (unsigned i = 0; i < 2; ++i) {
        char inverse[100];
        snprintf(inverse,
                 sizeof inverse,
                 "{\"op\":\"quncompute\",\"id\":%u,\"saved\":true}",
                 temporaries[i]);
        if (!qc_apply(c, inverse, NULL) || !release(c, reg(c, temporaries[i])))
            return 0;
    }
    return collect_disposed(c);
}

#include "heap_identity.inc"

static int
heap_decision_reads(qc_context *c, uint32_t id, uint32_t owner, unsigned depth, unsigned *work) {
    if (depth > QC_MAX_PHYSICAL || ++*work > 2000000) {
        qc_fail(c, "heap decision dependency proof budget exceeded");
        return -1;
    }
    qc_register *r = reg(c, id);
    if (!r || !r->alive || !r->prepared) {
        qc_fail(c, "heap decision input is unavailable");
        return -1;
    }
    if (id == owner)
        return 1;
    int reads = 0;
    if (r->definition)
        for (unsigned i = 0; i < r->ndeps; ++i) {
            qc_register *input = reg(c, r->deps[i]);
            if (!input || !input->alive || input->epoch != r->epochs[i]) {
                qc_fail(c, "heap decision input epochs changed");
                return -1;
            }
            int result = heap_decision_reads(c, input->id, owner, depth + 1, work);
            if (result < 0)
                return -1;
            reads |= result;
        }
    return reads;
}

static int
heap_controlled_store(qc_context *c, const cj_doc *d, int inst, qc_heap *heap, uint32_t owner) {
    int guards = cj_get(d, inst, "controls");
    size_t capacity = guards >= 0 ? (size_t)d->tokens[guards].children * 64 + 3 : 3, used = 1;
    char *guard_text = malloc(capacity);
    if (!guard_text)
        return qc_fail(c, "cannot allocate heap swap controls");
    guard_text[0] = '[';
    uint32_t aliased_guard = 0;
    int ok = 1;
    for (int t = guards >= 0 ? guards + 1 : 0; guards >= 0 && t < d->tokens[guards].next && ok;
         t = d->tokens[t].next) {
        uint64_t id, wanted;
        cj_u64(d, cj_get(d, t, "id"), &id);
        cj_u64(d, cj_get(d, t, "value"), &wanted);
        if (id == owner) {
            if (!aliased_guard) {
                char leaf[128];
                snprintf(leaf,
                         sizeof leaf,
                         "{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":%u}",
                         owner);
                ok = heap_new_register(c, 1, 0, 1, &aliased_guard) &&
                     heap_compute(c, aliased_guard, leaf);
                if (!ok)
                    break;
                clear_definition(reg(c, aliased_guard));
            }
            id = aliased_guard;
        }
        unsigned work = 0;
        int reads = heap_decision_reads(c, (uint32_t)id, owner, 0, &work);
        if (reads < 0) {
            ok = 0;
            break;
        }
        // A decision about the old destination cannot depend on its own
        // conditional inverse. Preserve that decision as necessary history.
        if (reads) {
            qc_mask seen = 0;
            taint_uniform(c, (uint32_t)id, &seen);
            clear_definition(reg(c, (uint32_t)id));
        }
        used += (size_t)snprintf(guard_text + used,
                                 capacity - used,
                                 "%s{\"id\":%u,\"value\":%u}",
                                 used > 1 ? "," : "",
                                 (uint32_t)id,
                                 (unsigned)wanted);
    }
    guard_text[used++] = ']';
    guard_text[used] = 0;
    char *expression =
        ok ? heap_guard_expression(
                 c, d, inst, heap->width, heap->sign, cj_copy(d, cj_get(d, inst, "expr")))
           : NULL;
    uint64_t selected = bits_mask(heap->width);
    cj_u64(d, cj_get(d, inst, "bit_mask"), &selected);
    if (expression && selected != bits_mask(heap->width)) {
        size_t size = strlen(expression) + 256;
        char *masked = malloc(size);
        if (masked)
            snprintf(
                masked,
                size,
                "{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"band\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":[%s]},{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":%" PRIu64
                "}]}]}",
                heap->width,
                heap->sign ? "true" : "false",
                heap->width,
                heap->width,
                expression,
                heap->width,
                selected);
        else
            qc_fail(c, "cannot allocate masked heap value");
        free(expression);
        expression = masked;
    }
    uint32_t history = 0;
    ok = expression && heap_new_register(c, heap->width, heap->sign, 0, &history) &&
         heap_compute(c, history, expression);
    free(expression);
    if (ok) {
        qc_register *r = reg(c, history);
        qc_mask seen = 0;
        for (unsigned i = 0; i < r->ndeps; ++i)
            taint_uniform(c, r->deps[i], &seen);
        clear_definition(r);
        size_t size = used + 160;
        char *swap = malloc(size);
        if (!swap)
            ok = qc_fail(c, "cannot allocate controlled heap exchange");
        else {
            snprintf(
                swap,
                size,
                "{\"op\":\"qswap\",\"id\":%u,\"other\":%u,\"rebind\":true,\"bit_mask\":%" PRIu64
                ",\"controls\":%s}",
                owner,
                history,
                selected,
                guard_text);
            ok = qc_apply(c, swap, NULL);
            free(swap);
        }
    }
    free(guard_text);
    if (!ok)
        return 0;
    // The heap retains its public owner. Only active amplitudes move the old
    // destination into history; saved consumers use transported inverse views.
    heap_retire(c, history);
    if (aliased_guard)
        heap_retire(c, aliased_guard);
    return collect_disposed(c);
}

static int heap_action(qc_context *c, const cj_doc *d, int inst, uint64_t *result) {
    if (c->structural) {
        uint64_t width;
        cj_u64(d, cj_get(d, inst, "width"), &width);
        // Structural validation still needs a saved capture recipe so that
        // subsequent cleanup can validate its lifetime. It does not run a heap.
        return heap_capture(c, d, inst, (unsigned)width, cj_bool(d, cj_get(d, inst, "signed")));
    }
    int guards = cj_get(d, inst, "controls");
    if (guards >= 0) {
        ar_builder proof = {.c = c, .d = d};
        for (int t = guards + 1; t < d->tokens[guards].next; t = d->tokens[t].next) {
            uint64_t id, wanted;
            int fixed;
            cj_u64(d, cj_get(d, t, "id"), &id);
            cj_u64(d, cj_get(d, t, "value"), &wanted);
            if (ar_register_boolean_constant(&proof, (uint32_t)id, &fixed, 0) &&
                fixed != (int)wanted) {
                uint64_t width;
                cj_u64(d, cj_get(d, inst, "width"), &width);
                return heap_capture(
                    c, d, inst, (unsigned)width, cj_bool(d, cj_get(d, inst, "signed")));
            }
            if (*c->error)
                return 0;
        }
    }
    uint64_t handle, offset, index = 0;
    cj_u64(d, cj_get(d, cj_get(d, inst, "heap"), "value"), &handle);
    cj_u64(d, cj_get(d, cj_get(d, inst, "offset"), "value"), &offset);
    int scatter = cj_equal(d, cj_get(d, inst, "op"), "qheap_scatter");
    if (!scatter)
        cj_u64(d, cj_get(d, cj_get(d, inst, "index"), "value"), &index);
    qc_heap *heap = heap_lookup(c, handle);
    uint64_t width;
    cj_u64(d, cj_get(d, inst, "width"), &width);
    if (!heap || offset >= heap->count || index >= heap->count - offset)
        return qc_fail(c, "quantum heap index is outside live storage");
    if (heap->width != width || heap->sign != cj_bool(d, cj_get(d, inst, "signed")))
        return qc_fail(c, "quantum heap access changes its allocated C element type");
    if (scatter)
        return heap_scatter(c, d, inst, heap, (size_t)offset);
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
    if (!heap_capture(c, d, inst, heap->width, heap->sign))
        return 0;
    int expr_node = cj_get(d, inst, "expr");
    domain inputs;
    if (!collect_domain(c, d, expr_node, &inputs))
        return 0;
    if (heap_store_identity(c, d, inst, old))
        return collect_disposed(c);
    int aliases = 0;
    for (unsigned i = 0; i < inputs.count; ++i)
        aliases |= inputs.sources[i].id == old;
    int controls_node = cj_get(d, inst, "controls");
    int controlled = controls_node >= 0 && d->tokens[controls_node].children;
    uint64_t selected = bits_mask(heap->width);
    cj_u64(d, cj_get(d, inst, "bit_mask"), &selected);
    if (controlled || selected != bits_mask(heap->width))
        return heap_controlled_store(c, d, inst, heap, old);
    int reuse = reg(c, old)->clean && !heap_borrowed(c, old) && !aliases && !controlled;
    uint32_t destination = old;
    if (!reuse && !heap_new_register(c, heap->width, heap->sign, 0, &destination))
        return 0;
    char *expr = cj_copy(d, expr_node);
    if (!expr)
        return qc_fail(c, "cannot allocate quantum heap store expression");
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

#include "heap_memory.inc"
