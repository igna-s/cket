/* Native libc byte views specialize classical extents, then emit reversible
 * word computations. They never copy host payloads or observe amplitudes. */
static char *heap_memory_text(qc_context *c, const char *format, ...) {
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    int size = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    char *text = size >= 0 && size < 65536 ? malloc((size_t)size + 1) : NULL;
    if (text)
        vsnprintf(text, (size_t)size + 1, format, args);
    else
        qc_fail(c, "heap byte expression allocation or text budget exceeded");
    va_end(args);
    return text;
}

static char *heap_memory_leaf(qc_context *c, const char *op, unsigned width, uint64_t value) {
    return heap_memory_text(c,
                            "{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"%s\":%" PRIu64 "}",
                            op,
                            width,
                            !strcmp(op, "reg") ? "id" : "value",
                            value);
}

/* These constructors consume their operands, including on failure. */
static char *heap_memory_cast(qc_context *c, unsigned width, int sign, char *value) {
    char *text = value
                     ? heap_memory_text(c,
                                        "{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[%s]}",
                                        width,
                                        sign ? "true" : "false",
                                        value)
                     : NULL;
    free(value);
    return text;
}

static char *
heap_memory_binary(qc_context *c, const char *op, unsigned width, char *left, char *right) {
    char *text =
        left && right
            ? heap_memory_text(c,
                               "{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"args\":[%s,%s]}",
                               op,
                               width,
                               left,
                               right)
            : NULL;
    free(left);
    free(right);
    return text;
}

static char *heap_memory_register(qc_context *c, uint32_t id) {
    qc_register *r = reg(c, id);
    return heap_memory_text(c,
                            "{\"op\":\"reg\",\"bits\":%u,\"signed\":%s,\"id\":%u}",
                            r->width,
                            r->signed_type ? "true" : "false",
                            id);
}

static int heap_memory_release(qc_context *c, uint32_t id) {
    char operation[100];
    snprintf(operation, sizeof operation, "{\"op\":\"quncompute\",\"id\":%u,\"saved\":true}", id);
    return qc_apply(c, operation, NULL) && release(c, reg(c, id));
}

static int heap_memory_range(qc_context *c, qc_heap *heap, uint64_t offset, uint64_t count) {
    if (!heap || heap->width % 8)
        return qc_fail(c, "heap byte operation requires a live whole-byte integer allocation");
    size_t bytes = heap->count * (heap->width / 8);
    if (offset > bytes || count > bytes - offset)
        return qc_fail(c, "heap byte range is outside live storage");
    return 1;
}

static int heap_memory_action(qc_context *c, const cj_doc *d, int inst) {
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
    uint64_t handle, offset, count, source_handle = 0, source_offset = 0;
    cj_u64(d, cj_get(d, cj_get(d, inst, "heap"), "value"), &handle);
    cj_u64(d, cj_get(d, cj_get(d, inst, "heap_offset"), "value"), &offset);
    cj_u64(d, cj_get(d, cj_get(d, inst, "count"), "value"), &count);
    int set = cj_equal(d, cj_get(d, inst, "mode"), "set");
    int move = cj_equal(d, cj_get(d, inst, "mode"), "move");
    int big = cj_bool(d, cj_get(d, inst, "big_endian"));
    qc_heap *destination = heap_lookup(c, handle), *source = NULL;
    if (!heap_memory_range(c, destination, offset, count) || count > 64)
        return qc_fail(c, "heap byte operation exceeds its 64-byte budget");
    if (!set) {
        cj_u64(d, cj_get(d, cj_get(d, inst, "source"), "value"), &source_handle);
        cj_u64(d, cj_get(d, cj_get(d, inst, "source_offset"), "value"), &source_offset);
        source = heap_lookup(c, source_handle);
        if (!heap_memory_range(c, source, source_offset, count))
            return 0;
        if (!move && count && source == destination && offset < source_offset + count &&
            source_offset < offset + count)
            return qc_fail(c, "memcpy heap byte ranges overlap; use memmove");
    }
    if ((!count && !set) || (move && source == destination && source_offset == offset))
        return 1;

    // A zero-valued saved recipe pins every source version before the first
    // write. Even shifted overlapping moves therefore read the original bytes.
    uint32_t original[QC_HEAP_MAX_ELEMENTS], pin;
    char *capture = NULL;
    if (set)
        capture = cj_copy(d, cj_get(d, inst, "expr"));
    else {
        memcpy(original, source->ids, source->count * sizeof *original);
        capture = heap_memory_leaf(c, "const", 1, 0);
        size_t stride = source->width / 8;
        size_t first = (size_t)source_offset / stride;
        size_t last = ((size_t)source_offset + (size_t)count - 1) / stride;
        for (size_t slot = first; slot <= last; ++slot)
            capture = heap_memory_binary(
                c, "sequence", 1, heap_memory_register(c, original[slot]), capture);
    }
    capture = heap_guard_expression(c, d, inst, set ? 8 : 1, 0, capture);
    int ok =
        capture && heap_new_register(c, set ? 8 : 1, 0, 1, &pin) && heap_compute(c, pin, capture);
    free(capture);
    if (!ok)
        return 0;
    // C evaluates the fill argument even when n == 0. Capturing and reversing
    // it above preserves arithmetic definedness and any scheduled argument effect.
    if (!count)
        return heap_memory_release(c, pin) && collect_disposed(c);
    char *controls = guards >= 0 ? cj_copy(d, guards) : heap_memory_text(c, "[]");
    if (!controls)
        return qc_fail(c, "cannot allocate heap byte controls");
    size_t stride = destination->width / 8;
    size_t first = (size_t)offset / stride, last = ((size_t)offset + (size_t)count - 1) / stride;
    for (size_t slot = first; slot <= last && ok; ++slot) {
        uint64_t mask = 0;
        for (size_t byte = 0; byte < stride; ++byte) {
            size_t address = slot * stride + byte;
            if (address >= offset && address - offset < count)
                mask |= UINT64_C(255) << (8 * (big ? stride - 1 - byte : byte));
        }
        char *word = heap_memory_binary(
            c,
            "band",
            destination->width,
            heap_memory_cast(
                c, destination->width, 0, heap_memory_register(c, destination->ids[slot])),
            heap_memory_leaf(
                c, "const", destination->width, bits_mask(destination->width) & ~mask));
        for (size_t byte = 0; byte < stride; ++byte) {
            size_t address = slot * stride + byte;
            if (address < offset || address - offset >= count)
                continue;
            char *value;
            if (set)
                value = heap_memory_register(c, pin);
            else {
                size_t position = (size_t)source_offset + address - (size_t)offset;
                size_t source_stride = source->width / 8, in_word = position % source_stride;
                unsigned shift = 8 * (big ? source_stride - 1 - in_word : in_word);
                value = heap_memory_cast(
                    c,
                    8,
                    0,
                    heap_memory_binary(c,
                                       "shr",
                                       source->width,
                                       heap_memory_cast(c,
                                                        source->width,
                                                        0,
                                                        heap_memory_register(
                                                            c, original[position / source_stride])),
                                       heap_memory_leaf(c, "const", 8, shift)));
            }
            unsigned shift = 8 * (big ? stride - 1 - byte : byte);
            value = heap_memory_binary(c,
                                       "shl",
                                       destination->width,
                                       heap_memory_cast(c, destination->width, 0, value),
                                       heap_memory_leaf(c, "const", 8, shift));
            word = heap_memory_binary(c, "bor", destination->width, word, value);
        }
        word = heap_memory_cast(c, destination->width, destination->sign, word);
        char *operation =
            word
                ? heap_memory_text(
                      c,
                      "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%" PRIu64
                      "},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%zu},"
                      "\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
                      "\"width\":%u,\"signed\":%s,\"expr\":%s,\"controls\":%s}",
                      handle,
                      slot,
                      destination->width,
                      destination->sign ? "true" : "false",
                      word,
                      controls)
                : NULL;
        free(word);
        ok = operation && qc_apply(c, operation, NULL);
        free(operation);
    }
    free(controls);
    return ok && heap_memory_release(c, pin) && collect_disposed(c);
}
