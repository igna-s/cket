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

static char *heap_memory_text(qc_context *c, const char *format, ...);

static char *
heap_store_value(qc_context *c, const cj_doc *d, int inst, qc_heap *heap, uint64_t selected) {
    char *expression = heap_guard_expression(
        c, d, inst, heap->width, heap->sign, cj_copy(d, cj_get(d, inst, "expr")));
    uint64_t selector;
    if (expression && cj_u64(d, cj_get(d, inst, "mask_register"), &selector)) {
        char *masked = heap_memory_text(
            c,
            "{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"band\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":[%s]},{\"op\":\"reg\",\"id\":%u,\"bits\":%u,\"signed\":false}]}]}",
            heap->width,
            heap->sign ? "true" : "false",
            heap->width,
            heap->width,
            expression,
            (uint32_t)selector,
            heap->width);
        free(expression);
        expression = masked;
    }
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
    return expression;
}

static char *patch_old_definition(qc_context *c, uint32_t owner) {
    qc_register *r = reg(c, owner);
    if (r->basis_known) {
        char text[128];
        snprintf(text,
                 sizeof text,
                 "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":%" PRIu64 "}",
                 r->width,
                 r->signed_type ? "true" : "false",
                 r->basis_value);
        char *value = strdup(text);
        if (!value)
            qc_fail(c, "cannot retain known register patch producer");
        return value;
    }
    if (!r->definition)
        return NULL;
    for (unsigned i = 0; i < r->ndeps; ++i) {
        qc_register *input = reg(c, r->deps[i]);
        if (!input || !input->alive || input->epoch != r->epochs[i]) {
            qc_fail(c, "register patch producer epochs changed");
            return NULL;
        }
    }
    char *value = strdup(r->definition);
    for (unsigned wire = 0; value && wire < QC_MAX_PHYSICAL; ++wire) {
        if (!(r->definition_mask & QC_WIRE(wire)))
            continue;
        qc_register *guard = NULL;
        for (size_t i = 0; i < c->nregisters; ++i)
            if (c->registers[i].alive && c->registers[i].width == 1 &&
                c->registers[i].wires[0] == wire) {
                guard = &c->registers[i];
                break;
            }
        if (!guard) {
            free(value);
            qc_fail(c, "register patch producer control is unavailable");
            return NULL;
        }
        size_t size = strlen(value) + 320;
        char *next = malloc(size);
        if (next) {
            char zero[96];
            snprintf(zero,
                     sizeof zero,
                     "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":0}",
                     r->width,
                     r->signed_type ? "true" : "false");
            int closed = !!(r->definition_pattern & QC_WIRE(wire));
            snprintf(
                next,
                size,
                "{\"op\":\"select\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":%u,\"bits\":1,\"signed\":false},%s,%s]}",
                r->width,
                r->signed_type ? "true" : "false",
                guard->id,
                closed ? value : zero,
                closed ? zero : value);
        }
        free(value);
        value = next;
    }
    if (!value)
        qc_fail(c, "cannot retain register patch producer");
    return value;
}

/* Inline certified producers before changing their public output. This rebases
 * a mask onto unchanged roots without observing or copying the quantum state.
 * An unknown destination root has no such recipe and needs decision history. */
static char *patch_stable_text(qc_context *c,
                               const char *text,
                               uint32_t owner,
                               const char *old_definition,
                               unsigned depth,
                               unsigned *work) {
    if (depth > 128 || ++*work > 4096) {
        qc_fail(c, "quantum mask producer expansion budget exceeded");
        return NULL;
    }
    cj_doc d = {0};
    if (!cj_parse(&d, text)) {
        cj_free(&d);
        qc_fail(c, "cannot parse quantum mask producer");
        return NULL;
    }
    char *out = malloc(65537);
    size_t copied = 0, used = 0;
    if (!out) {
        cj_free(&d);
        qc_fail(c, "cannot expand quantum mask producer");
        return NULL;
    }
    for (int token = 0; token < d.count; ++token) {
        uint64_t id;
        if (d.tokens[token].kind != '{' || !cj_equal(&d, cj_get(&d, token, "op"), "reg") ||
            !cj_u64(&d, cj_get(&d, token, "id"), &id))
            continue;
        qc_register *r = reg(c, (uint32_t)id);
        char *value = NULL;
        if (!r || !r->alive || !r->prepared)
            qc_fail(c, "quantum mask producer input is unavailable");
        else if (id == owner)
            value = old_definition
                        ? patch_stable_text(c, old_definition, owner, NULL, depth + 1, work)
                        : NULL;
        else if (r->basis_known) {
            char *producer = patch_old_definition(c, (uint32_t)id);
            if (producer)
                value = patch_stable_text(c, producer, owner, old_definition, depth + 1, work);
            free(producer);
        } else {
            value = cj_copy(&d, token);
            if (!value)
                qc_fail(c, "cannot copy quantum mask input");
        }
        size_t before = d.tokens[token].begin - copied;
        if (!value || used + before + strlen(value) > 65536) {
            if (value)
                qc_fail(c, "expanded quantum mask text budget exceeded");
            free(value);
            free(out);
            cj_free(&d);
            return NULL;
        }
        memcpy(out + used, text + copied, before);
        used += before;
        memcpy(out + used, value, strlen(value));
        used += strlen(value);
        copied = d.tokens[token].end;
        free(value);
    }
    size_t after = strlen(text + copied);
    if (used + after > 65536) {
        free(out);
        out = NULL;
        qc_fail(c, "expanded quantum mask text budget exceeded");
    } else
        memcpy(out + used, text + copied, after + 1);
    cj_free(&d);
    return out;
}

static int patch_rebase_mask(
    qc_context *c, uint32_t selector, uint32_t owner, const char *old_definition, unsigned depth) {
    if (!old_definition)
        return 0;
    if (depth > 128)
        return qc_fail(c, "quantum mask dependency depth exceeded");
    // Keep shared captures shared. Rebase their recipes individually instead
    // of duplicating the entire producer graph into every mask bit.
    qc_register prior = *reg(c, selector);
    for (unsigned i = 0; i < prior.ndeps; ++i) {
        uint32_t input = prior.deps[i];
        if (input == owner)
            continue;
        unsigned work = 0;
        int reads = heap_decision_reads(c, input, owner, 0, &work);
        if (reads < 0 || (reads && !patch_rebase_mask(c, input, owner, old_definition, depth + 1)))
            return 0;
    }
    char *producer = patch_old_definition(c, selector);
    unsigned work = 0;
    char *stable =
        producer ? patch_stable_text(c, producer, owner, old_definition, 0, &work) : NULL;
    free(producer);
    if (!stable)
        return 0;
    cj_doc d = {0};
    domain inputs;
    int ok = cj_parse(&d, stable) && collect_domain(c, &d, 0, &inputs);
    cj_free(&d);
    if (!ok) {
        free(stable);
        if (!*c->error)
            qc_fail(c, "cannot certify quantum mask producer");
        return 0;
    }
    qc_register *r = reg(c, selector);
    clear_definition(r);
    r->definition = stable;
    for (unsigned i = 0; i < inputs.count; ++i)
        if (!remember_dependency(c, r, inputs.sources[i].id))
            return 0;
    return 1;
}

static int
patch_stabilize_mask(qc_context *c, uint32_t selector, uint32_t owner, const char *old_definition) {
    return patch_rebase_mask(c, selector, owner, old_definition, 0);
}

static int heap_exchange_store(qc_context *c,
                               const cj_doc *d,
                               int inst,
                               qc_heap *heap,
                               uint32_t owner,
                               uint64_t provided_history,
                               uint64_t selected) {
    int provided = provided_history != UINT64_MAX, inactive = 0;
    uint64_t selector = 0;
    int quantum_mask = cj_u64(d, cj_get(d, inst, "mask_register"), &selector);
    char selector_field[64] = "";
    if (quantum_mask)
        snprintf(
            selector_field, sizeof selector_field, ",\"mask_register\":%u", (uint32_t)selector);
    if (provided && !qc_controls_inactive(c, d, inst, &inactive))
        return 0;
    // A proved identity still supplies the compiler's old-value view. Keep
    // that temporary derived so disposal can really reverse the copy.
    int empty_mask = quantum_mask && reg(c, (uint32_t)selector)->basis_known &&
                     !reg(c, (uint32_t)selector)->basis_value;
    if (provided &&
        (!selected || inactive || empty_mask || heap_store_identity(c, d, inst, owner))) {
        char *value = heap_store_value(c, d, inst, heap, selected);
        int ok = value && heap_compute(c, (uint32_t)provided_history, value);
        free(value);
        return ok;
    }
    char *old_definition = provided ? patch_old_definition(c, owner) : NULL;
    if (*c->error)
        return 0;
    uint32_t pin = 0;
    if (old_definition && reg(c, owner)->ndeps) {
        // Preserve the producer's inputs while its public output is changed.
        // The pin is identically zero; its extra dependency certificate emits
        // no duplicate producer computation or observation of the payload.
        qc_register prior = *reg(c, owner);
        if (!heap_new_register(c, 1, 0, 1, &pin)) {
            free(old_definition);
            return 0;
        }
        qc_register *hold = reg(c, pin);
        hold->definition = strdup("{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":0}");
        if (!hold->definition) {
            free(old_definition);
            return qc_fail(c, "cannot pin register patch producer inputs");
        }
        hold->ndeps = prior.ndeps;
        memcpy(hold->deps, prior.deps, prior.ndeps * sizeof *prior.deps);
        memcpy(hold->epochs, prior.epochs, prior.ndeps * sizeof *prior.epochs);
    }
    if (quantum_mask) {
        unsigned work = 0;
        int reads = heap_decision_reads(c, (uint32_t)selector, owner, 0, &work);
        if (reads < 0) {
            free(old_definition);
            return 0;
        }
        if (reads && !patch_stabilize_mask(c, (uint32_t)selector, owner, old_definition)) {
            if (*c->error) {
                free(old_definition);
                return 0;
            }
            qc_mask seen = 0;
            taint_uniform(c, (uint32_t)selector, &seen);
            clear_definition(reg(c, (uint32_t)selector));
        }
    }
    int guards = cj_get(d, inst, "controls");
    size_t capacity = guards >= 0 ? (size_t)d->tokens[guards].children * 64 + 3 : 3, used = 1;
    char *guard_text = malloc(capacity);
    if (!guard_text) {
        free(old_definition);
        return qc_fail(c, "cannot allocate heap swap controls");
    }
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
    char *expression = ok ? heap_store_value(c, d, inst, heap, selected) : NULL;
    uint32_t history = provided ? (uint32_t)provided_history : 0;
    ok = expression && (provided || heap_new_register(c, heap->width, heap->sign, 0, &history)) &&
         heap_compute(c, history, expression);
    free(expression);
    if (ok) {
        qc_register *r = reg(c, history);
        qc_mask seen = 0;
        for (unsigned i = 0; i < r->ndeps; ++i)
            taint_uniform(c, r->deps[i], &seen);
        clear_definition(r);
        r = reg(c, owner);
        if (r->definition) {
            seen = 0;
            taint_uniform(c, owner, &seen);
            clear_definition(r);
        }
        size_t size = used + 256;
        char *swap = malloc(size);
        if (!swap)
            ok = qc_fail(c, "cannot allocate controlled heap exchange");
        else {
            snprintf(
                swap,
                size,
                "{\"op\":\"qswap\",\"id\":%u,\"other\":%u,\"rebind\":true,\"bit_mask\":%" PRIu64
                ",\"controls\":%s%s}",
                owner,
                history,
                selected,
                guard_text,
                selector_field);
            ok = qc_apply(c, swap, NULL);
            free(swap);
        }
    }
    if (ok && old_definition) {
        size_t size = strlen(old_definition) + used + 128;
        char *text = malloc(size);
        cj_doc producer = {0};
        if (text)
            snprintf(text,
                     size,
                     "{\"expr\":%s,\"controls\":%s%s}",
                     old_definition,
                     guard_text,
                     selector_field);
        char *recipe = text && cj_parse(&producer, text)
                           ? heap_store_value(c, &producer, 0, heap, selected)
                           : NULL;
        cj_free(&producer);
        free(text);
        cj_doc expression = {0};
        domain inputs;
        ok = recipe && cj_parse(&expression, recipe) && collect_domain(c, &expression, 0, &inputs);
        if (ok) {
            qc_register *r = reg(c, history);
            r->definition = recipe;
            recipe = NULL;
            for (unsigned i = 0; i < inputs.count && ok; ++i)
                ok = remember_dependency(c, r, inputs.sources[i].id);
        }
        cj_free(&expression);
        free(recipe);
        if (!ok && !*c->error)
            qc_fail(c, "cannot certify register patch history recipe");
    }
    free(old_definition);
    free(guard_text);
    if (!ok)
        return 0;
    // The heap retains its public owner. Only active amplitudes move the old
    // destination into history; saved consumers use transported inverse views.
    if (!provided)
        heap_retire(c, history);
    if (aliased_guard)
        heap_retire(c, aliased_guard);
    if (pin)
        heap_retire(c, pin);
    return collect_disposed(c);
}

static int
heap_controlled_store(qc_context *c, const cj_doc *d, int inst, qc_heap *heap, uint32_t owner) {
    uint64_t selected = bits_mask(heap->width);
    cj_u64(d, cj_get(d, inst, "bit_mask"), &selected);
    return heap_exchange_store(c, d, inst, heap, owner, UINT64_MAX, selected);
}

static int register_patch(qc_context *c, const cj_doc *d, int inst) {
    uint64_t owner, history, selected;
    cj_u64(d, cj_get(d, inst, "id"), &owner);
    cj_u64(d, cj_get(d, inst, "history"), &history);
    cj_u64(d, cj_get(d, cj_get(d, inst, "bit_mask"), "value"), &selected);
    qc_register *r = reg(c, (uint32_t)owner);
    qc_heap representation = {.width = r->width, .sign = r->signed_type};
    if (c->structural) {
        // CPU templates check types and availability, not a placeholder mask
        // as if it were the real runtime write set.
        char *value = heap_store_value(c, d, inst, &representation, selected);
        int ok = value && heap_compute(c, (uint32_t)history, value);
        free(value);
        if (!ok)
            return 0;
        r = reg(c, (uint32_t)owner);
        clear_definition(r);
        unknown_support(r);
        r->clean = 0;
        ++r->epoch;
        return 1;
    }
    return heap_exchange_store(
        c, d, inst, &representation, (uint32_t)owner, (uint32_t)history, selected);
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
