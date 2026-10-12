/* CPU values specialize typed expression leaves; they are never quantum
 * storage placeholders. Stored circuit definitions retain the bound values. */
#define QC_MAX_BINDINGS 128
#define QC_MAX_BOUND_TEXT (64u * 1024u * 1024u)

typedef struct {
    uint32_t id;
    unsigned bits;
    int sign, used;
    uint64_t value;
} qc_binding;

typedef struct {
    qc_context *context;
    const cj_doc *doc;
    qc_binding bindings[QC_MAX_BINDINGS];
    size_t count, length, capacity;
    char *text;
} qc_binding_writer;

static int binding_fields(qc_context *c, const cj_doc *d, int object, const char *const *names) {
    if (object < 0 || d->tokens[object].kind != '{')
        return qc_fail(c, "CPU binding schema requires an object");
    for (int key = object + 1; key < d->tokens[object].next;) {
        int accepted = 0;
        for (unsigned i = 0; names[i]; ++i)
            accepted |= cj_equal(d, key, names[i]);
        if (!accepted)
            return qc_fail(c, "unknown CPU binding field");
        for (int prior = object + 1; prior < key; prior = d->tokens[prior + 1].next) {
            size_t length = d->tokens[key].end - d->tokens[key].begin;
            if (length == d->tokens[prior].end - d->tokens[prior].begin &&
                !memcmp(d->text + d->tokens[key].begin, d->text + d->tokens[prior].begin, length))
                return qc_fail(c, "duplicate CPU binding field");
        }
        key = d->tokens[key + 1].next;
    }
    return 1;
}

static int
binding_type(qc_context *c, const cj_doc *d, int object, uint32_t *id, unsigned *bits, int *sign) {
    uint64_t number, width;
    int identity = cj_get(d, object, "id"), precision = cj_get(d, object, "bits"),
        signedness = cj_get(d, object, "signed");
    if (identity < 0 || precision < 0 || signedness < 0 || d->tokens[identity].kind < '0' ||
        d->tokens[identity].kind > '9' || d->tokens[precision].kind < '0' ||
        d->tokens[precision].kind > '9' || !cj_u64(d, identity, &number) || number > UINT32_MAX ||
        !cj_u64(d, precision, &width) || !width || width > 64 ||
        (d->tokens[signedness].kind != 't' && d->tokens[signedness].kind != 'f') ||
        (width == 1 && cj_bool(d, signedness)))
        return qc_fail(c, "invalid CPU binding integer type");
    *id = (uint32_t)number;
    *bits = (unsigned)width;
    *sign = cj_bool(d, signedness);
    return 1;
}

static int binding_write(qc_binding_writer *writer, const char *text, size_t length) {
    if (length > QC_MAX_BOUND_TEXT - writer->length - 1)
        return qc_fail(writer->context, "bound operation text budget exceeded");
    size_t needed = writer->length + length + 1;
    if (needed > writer->capacity) {
        size_t capacity = writer->capacity ? writer->capacity : 256;
        while (capacity < needed)
            capacity = capacity > QC_MAX_BOUND_TEXT / 2 ? QC_MAX_BOUND_TEXT : capacity * 2;
        char *grown = realloc(writer->text, capacity);
        if (!grown)
            return qc_fail(writer->context, "cannot allocate bound operation");
        writer->text = grown;
        writer->capacity = capacity;
    }
    memcpy(writer->text + writer->length, text, length);
    writer->length += length;
    writer->text[writer->length] = 0;
    return 1;
}

static int binding_token(qc_binding_writer *writer, int token, unsigned depth) {
    const cj_doc *d = writer->doc;
    if (depth > 128)
        return qc_fail(writer->context, "CPU binding nesting budget exceeded");
    if (d->tokens[token].kind == '{' && cj_equal(d, cj_get(d, token, "op"), "parameter")) {
        static const char *const names[] = {"op", "id", "bits", "signed", NULL};
        uint32_t id;
        unsigned bits;
        int sign;
        if (!binding_fields(writer->context, d, token, names) ||
            !binding_type(writer->context, d, token, &id, &bits, &sign))
            return 0;
        for (size_t i = 0; i < writer->count; ++i) {
            qc_binding *binding = &writer->bindings[i];
            if (binding->id != id)
                continue;
            if (binding->bits != bits || binding->sign != sign)
                return qc_fail(writer->context, "CPU parameter type differs from its binding");
            binding->used = 1;
            uint64_t value = bits == 1 ? binding->value != 0 : binding->value & bits_mask(bits);
            int negative = sign && !!(value & (UINT64_C(1) << (bits - 1)));
            uint64_t magnitude = negative ? (~value + 1) & bits_mask(bits) : value;
            char literal[160];
            int length =
                snprintf(literal,
                         sizeof literal,
                         "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":%s%" PRIu64 "}",
                         bits,
                         sign ? "true" : "false",
                         negative ? "-" : "",
                         magnitude);
            return binding_write(writer, literal, (size_t)length);
        }
        return qc_fail(writer->context, "CPU parameter has no declared binding");
    }
    size_t position = d->tokens[token].begin;
    for (int child = token + 1; child < d->tokens[token].next; child = d->tokens[child].next) {
        if (!binding_write(writer, d->text + position, d->tokens[child].begin - position) ||
            !binding_token(writer, child, depth + 1))
            return 0;
        position = d->tokens[child].end;
    }
    return binding_write(writer, d->text + position, d->tokens[token].end - position);
}

char *qc_bind_operation(qc_context *c, const char *text, const uint64_t *values, size_t count) {
    static const char *const wrapper[] = {"op", "instruction", "parameters", NULL};
    static const char *const descriptor[] = {"id", "bits", "signed", NULL};
    cj_doc d = {0};
    qc_binding_writer writer = {.context = c, .doc = &d, .count = count};
    if (!text || !values || !count || count > QC_MAX_BINDINGS ||
        strlen(text) >= QC_MAX_BOUND_TEXT || !cj_parse(&d, text)) {
        qc_fail(c, "invalid CPU binding input or budget");
        goto failed;
    }
    int instruction = cj_get(&d, 0, "instruction"), parameters = cj_get(&d, 0, "parameters");
    if (d.count > 100000 || !binding_fields(c, &d, 0, wrapper) ||
        !cj_equal(&d, cj_get(&d, 0, "op"), "qbind") || instruction < 0 ||
        d.tokens[instruction].kind != '{' || parameters < 0 || d.tokens[parameters].kind != '[' ||
        (size_t)d.tokens[parameters].children != count) {
        qc_fail(c, "invalid CPU binding wrapper");
        goto failed;
    }
    size_t index = 0;
    for (int token = parameters + 1; token < d.tokens[parameters].next;
         token = d.tokens[token].next) {
        qc_binding *binding = &writer.bindings[index];
        if (!binding_fields(c, &d, token, descriptor) ||
            !binding_type(c, &d, token, &binding->id, &binding->bits, &binding->sign))
            goto failed;
        for (size_t previous = 0; previous < index; ++previous)
            if (writer.bindings[previous].id == binding->id) {
                qc_fail(c, "duplicate CPU binding identity");
                goto failed;
            }
        binding->value = values[index++];
    }
    if (cj_equal(&d, cj_get(&d, instruction, "op"), "qbind")) {
        qc_fail(c, "nested CPU binding wrappers are unsupported");
        goto failed;
    }
    if (!binding_token(&writer, instruction, 0))
        goto failed;
    for (size_t i = 0; i < count; ++i)
        if (!writer.bindings[i].used) {
            qc_fail(c, "unused CPU binding descriptor");
            goto failed;
        }
    cj_free(&d);
    return writer.text;
failed:
    cj_free(&d);
    free(writer.text);
    return NULL;
}

int qc_apply_bound(
    qc_context *c, const char *text, const uint64_t *values, size_t count, uint64_t *result) {
    char *bound = qc_bind_operation(c, text, values, count);
    if (!bound)
        return 0;
    int success = qc_apply(c, bound, result);
    free(bound);
    return success;
}

char *qc_bind_document(qc_context *c, const char *text, const char *binding_text) {
    static const char *const row_fields[] = {"instruction", "values", NULL};
    cj_doc d = {0}, rows = {0};
    qc_binding_writer writer = {.context = c};
    if (!text || !binding_text || strlen(text) >= QC_MAX_BOUND_TEXT ||
        strlen(binding_text) >= QC_MAX_BOUND_TEXT || !cj_parse(&d, text) ||
        !cj_parse(&rows, binding_text) || d.count > 100000 || rows.count > 100000 ||
        !qc_verify_document(c, &d) || rows.tokens[0].kind != '[') {
        qc_fail(c, "invalid circuit binding document or budget");
        goto failed_document;
    }
    int instructions = cj_get(&d, 0, "instructions"), row = 1;
    unsigned index = 0;
    size_t position = 0;
    for (int token = instructions + 1; token < d.tokens[instructions].next;
         token = d.tokens[token].next, ++index) {
        if (!cj_equal(&d, cj_get(&d, token, "op"), "qbind"))
            continue;
        uint64_t identity;
        int identity_token = row < rows.tokens[0].next ? cj_get(&rows, row, "instruction") : -1;
        int values_token = row < rows.tokens[0].next ? cj_get(&rows, row, "values") : -1;
        if (row >= rows.tokens[0].next || !binding_fields(c, &rows, row, row_fields) ||
            identity_token < 0 || rows.tokens[identity_token].kind < '0' ||
            rows.tokens[identity_token].kind > '9' || !cj_u64(&rows, identity_token, &identity) ||
            identity != index || values_token < 0 || rows.tokens[values_token].kind != '[' ||
            !rows.tokens[values_token].children ||
            rows.tokens[values_token].children > QC_MAX_BINDINGS) {
            qc_fail(c, "missing, unordered or invalid circuit binding row");
            goto failed_document;
        }
        uint64_t values[QC_MAX_BINDINGS];
        size_t count = 0;
        for (int value = values_token + 1; value < rows.tokens[values_token].next;
             value = rows.tokens[value].next)
            if (!cj_u64(&rows, value, &values[count++])) {
                qc_fail(c, "invalid raw integer circuit binding value");
                goto failed_document;
            }
        char *operation = cj_copy(&d, token);
        char *bound = operation ? qc_bind_operation(c, operation, values, count) : NULL;
        free(operation);
        if (!bound)
            goto failed_document;
        int success = binding_write(&writer, text + position, d.tokens[token].begin - position) &&
                      binding_write(&writer, bound, strlen(bound));
        free(bound);
        if (!success)
            goto failed_document;
        position = d.tokens[token].end;
        row = rows.tokens[row].next;
    }
    if (row != rows.tokens[0].next) {
        qc_fail(c, "unused circuit binding row");
        goto failed_document;
    }
    if (!binding_write(&writer, text + position, strlen(text) - position))
        goto failed_document;
    cj_free(&d);
    cj_free(&rows);
    return writer.text;
failed_document:
    cj_free(&d);
    cj_free(&rows);
    free(writer.text);
    return NULL;
}
