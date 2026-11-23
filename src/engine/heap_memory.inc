/* Native libc byte views specialize classical extents, then emit reversible
 * word computations. They never copy host payloads or observe amplitudes. */
static char *heap_memory_text(qc_context *c, const char *format, ...) {
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    int size = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    char *text = size >= 0 && size < 1048576 ? malloc((size_t)size + 1) : NULL;
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

static size_t heap_slot_bytes(const qc_heap *heap) {
    return heap->width == 1 ? 1u : (size_t)(heap->width / 8u);
}

static int heap_memory_range(qc_context *c, qc_heap *heap, uint64_t offset, uint64_t count) {
    if (!heap || (heap->width != 1 && heap->width % 8))
        return qc_fail(c, "heap byte operation requires a live whole-byte or Boolean allocation");
    size_t bytes = heap->count * heap_slot_bytes(heap);
    if (offset > bytes || count > bytes - offset)
        return qc_fail(c, "heap byte range is outside live storage");
    return 1;
}

static int heap_controls_inactive(qc_context *c, const cj_doc *d, int inst, int *inactive) {
    *inactive = 0;
    qc_mask mask, pattern;
    int impossible = 0;
    if (!controls(c, d, inst, &mask, &pattern, &impossible))
        return 0;
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
                *inactive = 1;
                return 1;
            }
            if (*c->error)
                return 0;
        }
    }
    return qc_controls_inactive(c, d, inst, inactive);
}

static int heap_check_extent(qc_context *c,
                             uint64_t val,
                             uint64_t limit,
                             int has_overlap,
                             uint64_t overlap,
                             const char *limit_msg,
                             const char *overlap_msg) {
    if (has_overlap && val > overlap && overlap <= limit)
        return qc_fail(c, "%s", overlap_msg);
    if (val > limit)
        return qc_fail(c, "%s", limit_msg);
    if (has_overlap && val > overlap)
        return qc_fail(c, "%s", overlap_msg);
    return 1;
}

static int heap_prove_guarded_bounds(qc_context *c,
                                     const cj_doc *guarded,
                                     uint64_t limit,
                                     int has_overlap,
                                     uint64_t overlap,
                                     const char *limit_msg,
                                     const char *overlap_msg,
                                     uint64_t *tight_lo,
                                     uint64_t *tight_hi,
                                     int *proven) {
    uint64_t lo = 0, hi = 0;
    uint64_t eff = has_overlap && overlap < limit ? overlap : limit;
    if (computed_interval(c, guarded, 0, 64, 0, &lo, &hi)) {
        if (!heap_check_extent(c, lo, limit, has_overlap, overlap, limit_msg, overlap_msg))
            return 0;
        *tight_lo = lo;
        *tight_hi = hi;
        if (hi <= eff) {
            *proven = 1;
            return 1;
        }
    }
    domain dom;
    if (!collect_domain(c, guarded, 0, &dom))
        return 0;
    uint64_t fixed_val[QC_MAX_SOURCES];
    int has_fixed[QC_MAX_SOURCES];
    int small_domain = 1;
    for (unsigned i = 0; i < dom.count && small_domain; ++i) {
        source *src = &dom.sources[i];
        qc_register *s = reg(c, src->id);
        if (!s || !s->alive || s->width != src->bits)
            return qc_fail(c, "use of unavailable or mistyped quantum storage");
        has_fixed[i] = src->known || s->basis_known;
        fixed_val[i] = src->known ? src->value : s->basis_value;
        if (has_fixed[i])
            continue;
        for (unsigned b = 0; b < src->active; ++b) {
            if (s->zero_mask & (UINT64_C(1) << b))
                continue;
            if (dom.nwires == QC_MAX_INPUT) {
                small_domain = 0;
                break;
            }
            unsigned n = dom.nwires++;
            dom.wires[n] = s->wires[b];
            dom.owners[n] = i;
            dom.positions[n] = b;
        }
    }
    if (small_domain) {
        unsigned alias[QC_MAX_SOURCES];
        int inverted[QC_MAX_SOURCES], fixed[QC_MAX_SOURCES];
        ar_builder proof = {.c = c, .d = guarded};
        for (unsigned j = 0; j < dom.count; ++j) {
            alias[j] = j;
            inverted[j] = 0;
            fixed[j] = -1;
            int constant;
            if (ar_register_boolean_constant(&proof, dom.sources[j].id, &constant, 0))
                fixed[j] = constant;
            if (*c->error)
                return 0;
            uint32_t source;
            int polarity = 0;
            if (!ar_register_boolean_alias(&proof, dom.sources[j].id, &source, &polarity, 0)) {
                if (*c->error)
                    return 0;
                continue;
            }
            for (unsigned k = 0; k < dom.count; ++k)
                if (source == dom.sources[k].id) {
                    alias[j] = k;
                    inverted[j] = polarity;
                    break;
                }
        }
        size_t n = (size_t)1 << dom.nwires;
        uint64_t vals[QC_MAX_SOURCES], min_seen = UINT64_MAX, max_seen = 0;
        int any = 0;
        for (size_t i = 0; i < n; ++i) {
            for (unsigned j = 0; j < dom.count; ++j)
                vals[j] = has_fixed[j] ? fixed_val[j] : 0;
            for (unsigned j = 0; j < dom.nwires; ++j)
                vals[dom.owners[j]] |= ((uint64_t)(i >> j) & 1u) << dom.positions[j];
            int reachable = 1;
            for (unsigned j = 0; j < dom.count; ++j) {
                qc_register *s = reg(c, dom.sources[j].id);
                if ((s->support_interval &&
                     (vals[j] < s->support_lower || vals[j] > s->support_upper)) ||
                    (s->basis_known && vals[j] != s->basis_value) || (vals[j] & s->zero_mask) ||
                    (fixed[j] >= 0 && vals[j] != (uint64_t)fixed[j]) ||
                    vals[j] != (vals[alias[j]] ^ (uint64_t)inverted[j])) {
                    reachable = 0;
                    break;
                }
            }
            if (!reachable)
                continue;
            uint64_t val = 0;
            if (!evaluate(c, guarded, 0, &dom, vals, &val))
                return 0;
            if (!heap_check_extent(c, val, limit, has_overlap, overlap, limit_msg, overlap_msg))
                return 0;
            if (!any || val < min_seen)
                min_seen = val;
            if (val > max_seen)
                max_seen = val;
            any = 1;
        }
        *tight_lo = any ? min_seen : 0;
        *tight_hi = any ? max_seen : 0;
        *proven = 1;
    } else {
        *tight_lo = lo;
        *tight_hi = eff;
        *proven = 0;
    }
    return 1;
}

static int heap_prove_count_bounds(qc_context *c,
                                   const cj_doc *d,
                                   int inst,
                                   int count_node,
                                   uint64_t limit,
                                   int has_overlap,
                                   uint64_t overlap,
                                   const char *limit_msg,
                                   const char *overlap_msg,
                                   uint64_t *tight_lo,
                                   uint64_t *tight_hi,
                                   int *proven) {
    *tight_lo = 0;
    *tight_hi = 0;
    *proven = 0;
    int inactive = 0;
    if (!heap_controls_inactive(c, d, inst, &inactive))
        return 0;
    if (inactive) {
        *proven = 1;
        return 1;
    }
    if (cj_equal(d, cj_get(d, count_node, "op"), "const")) {
        uint64_t value = 0;
        cj_u64(d, cj_get(d, count_node, "value"), &value);
        if (!heap_check_extent(c, value, limit, has_overlap, overlap, limit_msg, overlap_msg))
            return 0;
        *tight_lo = *tight_hi = value;
        *proven = 1;
        return 1;
    }
    char *guarded_text = heap_guard_expression(c, d, inst, 64, 0, cj_copy(d, count_node));
    if (!guarded_text)
        return 0;
    cj_doc guarded = {0};
    int ok = cj_parse(&guarded, guarded_text);
    if (!ok) {
        qc_fail(c, "%s", guarded.error[0] ? guarded.error : "invalid quantum count");
        cj_free(&guarded);
        free(guarded_text);
        return 0;
    }
    ok = heap_prove_guarded_bounds(c,
                                   &guarded,
                                   limit,
                                   has_overlap,
                                   overlap,
                                   limit_msg,
                                   overlap_msg,
                                   tight_lo,
                                   tight_hi,
                                   proven);
    cj_free(&guarded);
    free(guarded_text);
    return ok;
}

// The pin's zero-valued sequence stores the original read expressions. A
// controlled write may transport those expressions to conditional old owners.
// Read the saved recipe, not a now-modified public register with the old id.
static char *heap_memory_snapshot(qc_context *c,
                                  uint32_t pin,
                                  const cj_doc *operation,
                                  int guards,
                                  size_t count,
                                  size_t position) {
    qc_register *r = reg(c, pin);
    cj_doc d = {0};
    if (!r || !r->definition || position >= count || !cj_parse(&d, r->definition)) {
        cj_free(&d);
        qc_fail(c, "heap source snapshot is unavailable");
        return NULL;
    }
    unsigned wanted[QC_MAX_PHYSICAL], n = 0;
    if (guards >= 0)
        for (int t = guards + 1; t < operation->tokens[guards].next;
             t = operation->tokens[t].next) {
            uint64_t value;
            cj_u64(operation, cj_get(operation, t, "value"), &value);
            wanted[n++] = (unsigned)value;
        }
    int e = 0, nodes[3];
    while (n) {
        if (!cj_equal(&d, cj_get(&d, e, "op"), "select") || expr_nodes(&d, e, nodes) != 3)
            goto invalid;
        e = nodes[wanted[--n] ? 1 : 2];
    }
    for (size_t i = count; i; --i) {
        if (!cj_equal(&d, cj_get(&d, e, "op"), "sequence") || expr_nodes(&d, e, nodes) != 2)
            goto invalid;
        if (i - 1 == position) {
            char *value = cj_copy(&d, nodes[0]);
            cj_free(&d);
            return value;
        }
        e = nodes[1];
    }
invalid:
    cj_free(&d);
    qc_fail(c, "heap source snapshot recipe lost its capture structure");
    return NULL;
}

static char *heap_memory_checked_count(qc_context *c,
                                       const cj_doc *d,
                                       int inst,
                                       int count_node,
                                       unsigned sel_bits,
                                       int proven,
                                       uint64_t bound) {
    char *guarded = heap_guard_expression(c, d, inst, 64, 0, cj_copy(d, count_node));
    if (!guarded)
        return NULL;
    char *casted = heap_memory_cast(c, sel_bits, 0, strdup(guarded));
    if (proven) {
        free(guarded);
        return casted;
    }
    char *expr =
        casted
            ? heap_memory_text(
                  c,
                  "{\"op\":\"select\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"lt\",\"bits\":1,\"signed\":false,\"args\":[%s,{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%" PRIu64
                  "}]},%s,{\"op\":\"div\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":1},{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":0}]}]}",
                  sel_bits,
                  guarded,
                  bound + 1,
                  casted,
                  sel_bits,
                  sel_bits,
                  sel_bits)
            : NULL;
    free(guarded);
    free(casted);
    return expr;
}

static char *heap_memory_guard_byte(qc_context *c,
                                    uint64_t rel,
                                    unsigned sel_bits,
                                    uint32_t count_reg,
                                    char *new_byte,
                                    char *orig_byte) {
    if (!new_byte || !orig_byte) {
        free(new_byte);
        free(orig_byte);
        return NULL;
    }
    char *cond = heap_memory_binary(c,
                                    "lt",
                                    1,
                                    heap_memory_leaf(c, "const", sel_bits, rel),
                                    heap_memory_leaf(c, "reg", sel_bits, count_reg));
    char *res = cond ? heap_memory_text(
                           c,
                           "{\"op\":\"select\",\"bits\":8,\"signed\":false,\"args\":[%s,%s,%s]}",
                           cond,
                           new_byte,
                           orig_byte)
                     : NULL;
    free(cond);
    free(new_byte);
    free(orig_byte);
    return res;
}

static int heap_memory_action(qc_context *c, const cj_doc *d, int inst) {
    if (c->structural)
        return 1;
    int inactive = 0;
    if (!heap_controls_inactive(c, d, inst, &inactive))
        return 0;
    if (inactive)
        return 1;
    int guards = cj_get(d, inst, "controls");
    uint64_t handle, offset, source_handle = 0, source_offset = 0;
    cj_u64(d, cj_get(d, cj_get(d, inst, "heap"), "value"), &handle);
    cj_u64(d, cj_get(d, cj_get(d, inst, "heap_offset"), "value"), &offset);
    int count_node = cj_get(d, inst, "count");
    int count_is_const = cj_equal(d, cj_get(d, count_node, "op"), "const");
    int set = cj_equal(d, cj_get(d, inst, "mode"), "set");
    int move = cj_equal(d, cj_get(d, inst, "mode"), "move");
    int big = cj_bool(d, cj_get(d, inst, "big_endian"));
    int check = cj_equal(d, cj_get(d, inst, "mode"), "check");
    int bytes = cj_get(d, inst, "bytes");
    qc_heap *destination = heap_lookup(c, handle), *source = NULL;
    if (!heap_memory_range(c, destination, offset, 0))
        return 0;
    uint64_t hard_limit = destination->count * heap_slot_bytes(destination) - (size_t)offset;
    const char *hard_msg = "heap byte range is outside live storage";
    if (64 < hard_limit) {
        hard_limit = 64;
        hard_msg = "heap byte operation exceeds its 64-byte budget";
    }
    if (check) {
        uint64_t limit = 0;
        cj_u64(d, cj_get(d, inst, "limit"), &limit);
        if (limit < hard_limit) {
            hard_limit = limit;
            hard_msg = "heap transfer exceeds checked fixed storage bounds";
        }
    }
    int byte_nodes[64];
    if (bytes >= 0) {
        uint64_t blimit = (uint64_t)d->tokens[bytes].children;
        if (blimit < hard_limit) {
            hard_limit = blimit;
            hard_msg = "heap transfer exceeds checked fixed source bounds";
        }
        unsigned i = 0;
        for (int node = bytes + 1; node < d->tokens[bytes].next; node = d->tokens[node].next)
            byte_nodes[i++] = node;
    }
    int has_overlap = 0;
    uint64_t overlap_limit = 0;
    if (!set && !check && bytes < 0) {
        cj_u64(d, cj_get(d, cj_get(d, inst, "source"), "value"), &source_handle);
        cj_u64(d, cj_get(d, cj_get(d, inst, "source_offset"), "value"), &source_offset);
        source = heap_lookup(c, source_handle);
        if (!heap_memory_range(c, source, source_offset, 0))
            return 0;
        uint64_t src_avail = source->count * heap_slot_bytes(source) - (size_t)source_offset;
        if (src_avail < hard_limit) {
            hard_limit = src_avail;
            hard_msg = "heap byte range is outside live storage";
        }
        if (!move && source == destination) {
            has_overlap = 1;
            overlap_limit =
                offset >= source_offset ? offset - source_offset : source_offset - offset;
        }
    }
    uint64_t tight_lo = 0, tight_hi = 0;
    int proven = 0;
    if (!heap_prove_count_bounds(c,
                                 d,
                                 inst,
                                 count_node,
                                 hard_limit,
                                 has_overlap,
                                 overlap_limit,
                                 hard_msg,
                                 "memcpy heap byte ranges overlap; use memmove",
                                 &tight_lo,
                                 &tight_hi,
                                 &proven))
        return 0;
    if (check)
        return proven ? 1 : qc_fail(c, "heap transfer exceeds checked fixed storage bounds");
    uint64_t eff_bound = has_overlap && overlap_limit < hard_limit ? overlap_limit : hard_limit;
    uint64_t max_count = proven ? tight_hi : eff_bound;
    int noop_move = move && source == destination && source_offset == offset;
    if (count_is_const && ((!max_count && !set) || noop_move))
        return 1;
    if (proven && ((!max_count && !set) || noop_move))
        return 1;

    uint32_t count_reg = 0;
    int has_count_reg = !count_is_const && (!proven || (max_count > 0 && !noop_move));
    unsigned sel_bits = 1;
    if (has_count_reg) {
        while (sel_bits < 64 && (UINT64_C(1) << sel_bits) <= max_count)
            ++sel_bits;
        char *count_expr =
            heap_memory_checked_count(c, d, inst, count_node, sel_bits, proven, eff_bound);
        int cok = count_expr && heap_new_register(c, sel_bits, 0, 1, &count_reg) &&
                  heap_compute(c, count_reg, count_expr);
        free(count_expr);
        if (!cok) {
            if (!proven && strstr(c->error, "zero")) {
                c->error[0] = 0;
                return qc_fail(c,
                               "%s",
                               has_overlap && overlap_limit < hard_limit
                                   ? "memcpy heap byte ranges overlap; use memmove"
                                   : hard_msg);
            }
            return 0;
        }
    }

    int need_write = max_count > 0 && !noop_move;
    int need_pin = set || need_write;
    uint32_t pin = 0;
    uint32_t *original = NULL;
    int ok = 1;
    if (need_pin) {
        // A zero-valued saved recipe pins every source version before the first
        // write. Even shifted overlapping moves therefore read the original bytes.
        char *capture = NULL;
        if (set)
            capture = cj_copy(d, cj_get(d, inst, "expr"));
        else if (bytes >= 0) {
            capture = heap_memory_leaf(c, "const", 1, 0);
            for (size_t byte = 0; byte < max_count; ++byte)
                capture =
                    heap_memory_binary(c, "sequence", 1, cj_copy(d, byte_nodes[byte]), capture);
        } else {
            original = malloc(source->count * sizeof *original);
            if (!original) {
                if (has_count_reg)
                    heap_memory_release(c, count_reg);
                return qc_fail(c, "cannot allocate heap source register snapshot");
            }
            memcpy(original, source->ids, source->count * sizeof *original);
            capture = heap_memory_leaf(c, "const", 1, 0);
            size_t source_stride = heap_slot_bytes(source);
            size_t first = (size_t)source_offset / source_stride;
            size_t last = ((size_t)source_offset + (size_t)max_count - 1) / source_stride;
            for (size_t slot = first; slot <= last; ++slot)
                capture = heap_memory_binary(
                    c, "sequence", 1, heap_memory_register(c, original[slot]), capture);
        }
        capture = heap_guard_expression(c, d, inst, set ? 8 : 1, 0, capture);
        ok = capture && heap_new_register(c, set ? 8 : 1, 0, 1, &pin) &&
             heap_compute(c, pin, capture);
        free(capture);
        if (!ok) {
            free(original);
            if (has_count_reg)
                heap_memory_release(c, count_reg);
            return 0;
        }
    }
    if (!need_write) {
        free(original);
        if (need_pin)
            ok = ok && heap_memory_release(c, pin);
        if (has_count_reg)
            ok = ok && heap_memory_release(c, count_reg);
        return ok && collect_disposed(c);
    }
    char *controls = guards >= 0 ? cj_copy(d, guards) : heap_memory_text(c, "[]");
    if (!controls) {
        free(original);
        return qc_fail(c, "cannot allocate heap byte controls");
    }
    size_t stride = heap_slot_bytes(destination);
    size_t first = (size_t)offset / stride,
           last = ((size_t)offset + (size_t)max_count - 1) / stride;
    for (size_t slot = first; slot <= last && ok; ++slot) {
        uint64_t mask = 0;
        for (size_t byte = 0; byte < stride; ++byte) {
            size_t address = slot * stride + byte;
            if (address >= offset && address - offset < max_count) {
                if (destination->width == 1)
                    mask = 1;
                else
                    mask |= UINT64_C(255) << (8 * (big ? stride - 1 - byte : byte));
            }
        }
        char *word = NULL;
        if (destination->width == 1) {
            size_t rel = slot - (size_t)offset;
            char *value;
            if (set)
                value = heap_memory_register(c, pin);
            else if (bytes >= 0)
                value = heap_memory_snapshot(c, pin, d, guards, (size_t)max_count, rel);
            else {
                size_t position = (size_t)source_offset + rel;
                size_t source_stride = heap_slot_bytes(source), in_word = position % source_stride;
                size_t source_first = (size_t)source_offset / source_stride,
                       source_last =
                           ((size_t)source_offset + (size_t)max_count - 1) / source_stride;
                unsigned sshift =
                    source->width == 1 ? 0 : 8 * (big ? source_stride - 1 - in_word : in_word);
                value = heap_memory_cast(
                    c,
                    8,
                    0,
                    heap_memory_binary(
                        c,
                        "shr",
                        source->width,
                        heap_memory_cast(
                            c,
                            source->width,
                            0,
                            heap_memory_snapshot(c,
                                                 pin,
                                                 d,
                                                 guards,
                                                 source_last - source_first + 1,
                                                 position / source_stride - source_first)),
                        heap_memory_leaf(c, "const", 8, sshift)));
            }
            if (!count_is_const && rel >= tight_lo) {
                char *orig_byte =
                    heap_memory_cast(c, 8, 0, heap_memory_register(c, destination->ids[slot]));
                value = heap_memory_guard_byte(c, rel, sel_bits, count_reg, value, orig_byte);
            }
            word = heap_memory_cast(c, 1, 0, value);
        } else {
            word = heap_memory_binary(
                c,
                "band",
                destination->width,
                heap_memory_cast(
                    c, destination->width, 0, heap_memory_register(c, destination->ids[slot])),
                heap_memory_leaf(
                    c, "const", destination->width, bits_mask(destination->width) & ~mask));
            for (size_t byte = 0; byte < stride; ++byte) {
                size_t address = slot * stride + byte;
                if (address < offset || address - offset >= max_count)
                    continue;
                size_t rel = address - (size_t)offset;
                char *value;
                if (set)
                    value = heap_memory_register(c, pin);
                else if (bytes >= 0)
                    value = heap_memory_snapshot(c, pin, d, guards, (size_t)max_count, rel);
                else {
                    size_t position = (size_t)source_offset + rel;
                    size_t source_stride = heap_slot_bytes(source),
                           in_word = position % source_stride;
                    size_t source_first = (size_t)source_offset / source_stride,
                           source_last =
                               ((size_t)source_offset + (size_t)max_count - 1) / source_stride;
                    unsigned sshift =
                        source->width == 1 ? 0 : 8 * (big ? source_stride - 1 - in_word : in_word);
                    value = heap_memory_cast(
                        c,
                        8,
                        0,
                        heap_memory_binary(
                            c,
                            "shr",
                            source->width,
                            heap_memory_cast(
                                c,
                                source->width,
                                0,
                                heap_memory_snapshot(c,
                                                     pin,
                                                     d,
                                                     guards,
                                                     source_last - source_first + 1,
                                                     position / source_stride - source_first)),
                            heap_memory_leaf(c, "const", 8, sshift)));
                }
                unsigned shift = 8 * (big ? stride - 1 - byte : byte);
                if (!count_is_const && rel >= tight_lo) {
                    char *orig_byte = heap_memory_cast(
                        c,
                        8,
                        0,
                        heap_memory_binary(
                            c,
                            "shr",
                            destination->width,
                            heap_memory_cast(c,
                                             destination->width,
                                             0,
                                             heap_memory_register(c, destination->ids[slot])),
                            heap_memory_leaf(c, "const", 8, shift)));
                    value = heap_memory_guard_byte(c, rel, sel_bits, count_reg, value, orig_byte);
                }
                value = heap_memory_binary(c,
                                           "shl",
                                           destination->width,
                                           heap_memory_cast(c, destination->width, 0, value),
                                           heap_memory_leaf(c, "const", 8, shift));
                word = heap_memory_binary(c, "bor", destination->width, word, value);
            }
            word = heap_memory_cast(c, destination->width, destination->sign, word);
        }
        char *operation =
            word
                ? heap_memory_text(
                      c,
                      "{\"op\":\"qheap_store\",\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%" PRIu64
                      "},\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%zu},"
                      "\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
                      "\"width\":%u,\"signed\":%s,\"bit_mask\":%" PRIu64
                      ",\"expr\":%s,\"controls\":%s}",
                      handle,
                      slot,
                      destination->width,
                      destination->sign ? "true" : "false",
                      mask,
                      word,
                      controls)
                : NULL;
        free(word);
        ok = operation && qc_apply(c, operation, NULL);
        free(operation);
    }
    free(controls);
    free(original);
    if (need_pin)
        ok = ok && heap_memory_release(c, pin);
    if (has_count_reg)
        ok = ok && heap_memory_release(c, count_reg);
    return ok && collect_disposed(c);
}
