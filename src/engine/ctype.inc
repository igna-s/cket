/* The declared C-locale ASCII profile. Intervals are specification-sized,
 * disjoint character classes, never an input truth table. */
typedef struct {
    const char *name;
    unsigned mask;
    unsigned intervals;
    unsigned bounds[4][2];
} qc_ctype_profile;

static const qc_ctype_profile ctype_profiles[] = {
    {"ctype_isalnum", _ISalnum, 3, {{48, 57}, {65, 90}, {97, 122}}},
    {"ctype_isalpha", _ISalpha, 2, {{65, 90}, {97, 122}}},
    {"ctype_isblank", _ISblank, 2, {{9, 9}, {32, 32}}},
    {"ctype_iscntrl", _IScntrl, 2, {{0, 31}, {127, 127}}},
    {"ctype_isdigit", _ISdigit, 1, {{48, 57}}},
    {"ctype_isgraph", _ISgraph, 1, {{33, 126}}},
    {"ctype_islower", _ISlower, 1, {{97, 122}}},
    {"ctype_isprint", _ISprint, 1, {{32, 126}}},
    {"ctype_ispunct", _ISpunct, 4, {{33, 47}, {58, 64}, {91, 96}, {123, 126}}},
    {"ctype_isspace", _ISspace, 2, {{9, 13}, {32, 32}}},
    {"ctype_isupper", _ISupper, 1, {{65, 90}}},
    {"ctype_isxdigit", _ISxdigit, 3, {{48, 57}, {65, 70}, {97, 102}}},
    {"ctype_tolower", 0, 1, {{65, 90}}},
    {"ctype_toupper", 0, 1, {{97, 122}}},
};

static const qc_ctype_profile *ctype_profile(const cj_doc *d, int op) {
    for (unsigned i = 0; i < sizeof ctype_profiles / sizeof *ctype_profiles; ++i)
        if (cj_equal(d, op, ctype_profiles[i].name))
            return &ctype_profiles[i];
    return NULL;
}

static int ctype_mapping(const qc_ctype_profile *profile) {
    return profile >= ctype_profiles + 12;
}

static int ctype_mask(const qc_ctype_profile *profile) {
    return (int)profile->mask;
}

static int ctype_value(const qc_ctype_profile *profile, int value) {
    for (unsigned i = 0; i < profile->intervals; ++i)
        if (value >= (int)profile->bounds[i][0] && value <= (int)profile->bounds[i][1])
            return ctype_mapping(profile) ? value + (profile == ctype_profiles + 12 ? 32 : -32)
                                          : ctype_mask(profile);
    return ctype_mapping(profile) ? value : 0;
}

/* Immutable byte profiles are sorted disjoint intervals. A classification
 * interval stores the native mask; a case interval stores input XOR output.
 * EOF is outside these intervals and retains its standard behavior. */
typedef struct {
    unsigned intervals, bounds[256][3];
    int mapping;
} qc_ctype_data;

static int ctype_read(qc_context *c, const cj_doc *d, int e, qc_ctype_data *data) {
    const qc_ctype_profile *legacy = ctype_profile(d, cj_get(d, e, "op"));
    if (!legacy)
        return qc_fail(c, "invalid ctype profile operator");
    memset(data, 0, sizeof *data);
    data->mapping = ctype_mapping(legacy);
    int profile = cj_get(d, e, "profile");
    if (profile < 0) {
        data->intervals = legacy->intervals;
        for (unsigned i = 0; i < data->intervals; ++i) {
            data->bounds[i][0] = legacy->bounds[i][0];
            data->bounds[i][1] = legacy->bounds[i][1];
            data->bounds[i][2] = data->mapping ? 32 : legacy->mask;
        }
        return 1;
    }
    if (!qc_verify_ctype_profile(c, d, e, data->mapping))
        return 0;
    if (d->tokens[profile].kind != '[')
        return qc_fail(c, "ctype profile needs immutable specialization before synthesis");
    for (int row = profile + 1; row < d->tokens[profile].next; row = d->tokens[row].next) {
        int field = row + 1;
        for (unsigned j = 0; j < 3; ++j) {
            uint64_t value;
            if (!cj_u64(d, field, &value))
                return qc_fail(c, "invalid ctype profile interval");
            data->bounds[data->intervals][j] = (unsigned)value;
            field = d->tokens[field].next;
        }
        ++data->intervals;
    }
    return 1;
}

static int ctype_data_value(const qc_ctype_data *data, int value) {
    for (unsigned i = 0; i < data->intervals; ++i)
        if (value >= (int)data->bounds[i][0] && value <= (int)data->bounds[i][1])
            return data->mapping ? value ^ (int)data->bounds[i][2] : (int)data->bounds[i][2];
    return data->mapping ? value : 0;
}

/* A locale profile is part of an expression's value, including in range
 * proofs. Comparing only the operator and its input would transfer facts
 * between different immutable functions and can hide reachable division by
 * zero. Decode intervals so whitespace and an explicit legacy profile do
 * not affect equality. Differently partitioned profiles are conservative. */
static int ctype_same_profile(qc_context *c, const cj_doc *d, int a, int b) {
    qc_ctype_data left, right;
    if (!ctype_read(c, d, a, &left) || !ctype_read(c, d, b, &right))
        return 0;
    return left.mapping == right.mapping && left.intervals == right.intervals &&
           !memcmp(left.bounds, right.bounds, left.intervals * sizeof *left.bounds);
}

static int (*const ctype_native[])(int, locale_t) = {isalnum_l,
                                                     isalpha_l,
                                                     isblank_l,
                                                     iscntrl_l,
                                                     isdigit_l,
                                                     isgraph_l,
                                                     islower_l,
                                                     isprint_l,
                                                     ispunct_l,
                                                     isspace_l,
                                                     isupper_l,
                                                     isxdigit_l,
                                                     tolower_l,
                                                     toupper_l};

static int ctype_capture(qc_context *c, const char *name) {
    int saved_errno = errno;
    locale_t locale =
        name ? newlocale(LC_CTYPE_MASK, name, (locale_t)0) : duplocale(uselocale((locale_t)0));
    if (!locale)
        return qc_fail(c, "cannot capture ctype locale: %s", strerror(errno));
    char *profiles[14] = {0};
    int ok = 1;
    for (unsigned operation = 0; operation < 14 && ok; ++operation) {
        int mapping = operation >= 12;
        if (ctype_native[operation](EOF, locale) != (mapping ? EOF : 0)) {
            ok = qc_fail(c, "ctype locale has incompatible EOF behavior");
            break;
        }
        char text[8192];
        size_t length = 1;
        unsigned count = 0;
        text[0] = '[';
        for (unsigned byte = 0; byte < 256;) {
            int result = ctype_native[operation]((int)byte, locale);
            if (result < 0 || (mapping && result > 255)) {
                ok = qc_fail(c, "ctype locale exceeds the declared glibc byte representation");
                break;
            }
            unsigned value = mapping ? byte ^ (unsigned)result : (unsigned)result;
            unsigned low = byte++;
            while (byte < 256) {
                int next = ctype_native[operation]((int)byte, locale);
                if (next < 0 || (mapping && next > 255) ||
                    (mapping ? byte ^ (unsigned)next : (unsigned)next) != value)
                    break;
                ++byte;
            }
            if (!value)
                continue;
            int written = snprintf(text + length,
                                   sizeof text - length,
                                   "%s[%u,%u,%u]",
                                   count++ ? "," : "",
                                   low,
                                   byte - 1,
                                   value);
            if (written < 0 || (size_t)written >= sizeof text - length - 2) {
                ok = qc_fail(c, "ctype locale profile text budget exceeded");
                break;
            }
            length += (size_t)written;
        }
        if (!ok)
            break;
        text[length++] = ']';
        text[length] = 0;
        profiles[operation] = malloc(length + 1);
        if (!profiles[operation]) {
            ok = qc_fail(c, "cannot allocate immutable ctype profile");
            break;
        }
        memcpy(profiles[operation], text, length + 1);
    }
    freelocale(locale);
    for (unsigned i = 0; i < 14; ++i) {
        if (ok) {
            free(c->ctype_snapshots[i]);
            c->ctype_snapshots[i] = profiles[i];
        } else
            free(profiles[i]);
    }
    if (ok)
        errno = saved_errno;
    return ok;
}

void qc_ctype_runtime(qc_context *c) {
    c->ctype_runtime = 1;
}

int qc_ctype_locale(qc_context *c, const char *name) {
    if (!name || !*name)
        return qc_fail(c, "static ctype specialization requires a locale name");
    return ctype_capture(c, name);
}

/* Keep the old fixed C-profile IR safe also under a per-thread locale. */
static int ctype_legacy_require(qc_context *c) {
    locale_t locale = duplocale(uselocale((locale_t)0));
    if (!locale)
        return qc_fail(c, "cannot inspect effective ctype locale");
    int ok = 1;
    for (unsigned operation = 0; operation < 14 && ok; ++operation)
        for (int byte = EOF; byte < 256; ++byte)
            if (ctype_native[operation](byte, locale) !=
                ctype_value(&ctype_profiles[operation], byte)) {
                ok = qc_fail(c, "legacy quantum ctype requires the C byte profile");
                break;
            }
    freelocale(locale);
    return ok;
}

/* Substitute profile leaves after structural verification, keeping all other
 * serialized fields intact. Saved definitions and plan keys then own the data.
 * Text expansion is bounded independently of circuit work budgets. */
static int ctype_bind(qc_context *c, const cj_doc *d, int inst, char **out) {
    const size_t limit = 64 * 1024 * 1024;
    size_t length = d->tokens[inst].end - d->tokens[inst].begin;
    unsigned replacements = 0;
    *out = NULL;
    const char **leaves = NULL;
    for (int e = inst; e < d->tokens[inst].next; ++e) {
        if (d->tokens[e].kind != '{')
            continue;
        const qc_ctype_profile *operation = ctype_profile(d, cj_get(d, e, "op"));
        int profile = cj_get(d, e, "profile");
        if (!operation || !cj_equal(d, profile, "current"))
            continue;
        if (!leaves) {
            leaves = calloc((size_t)d->count, sizeof *leaves);
            if (!leaves)
                return qc_fail(c, "cannot allocate ctype specialization leaves");
        }
        const char *text = c->ctype_snapshots[operation - ctype_profiles];
        if (!text) {
            free(leaves);
            return qc_fail(c, "ctype capture is required before immutable specialization");
        }
        size_t addition = strlen(text);
        if (length >= limit || addition >= limit - length) {
            free(leaves);
            return qc_fail(c, "ctype specialization text budget exceeded");
        }
        length += addition;
        leaves[profile] = text;
        ++replacements;
    }
    if (!replacements) {
        free(leaves);
        return 1;
    }
    char *text = malloc(length + 1);
    if (!text) {
        free(leaves);
        return qc_fail(c, "cannot allocate specialized ctype operation");
    }
    size_t cursor = d->tokens[inst].begin, written = 0;
    for (int e = inst; e < d->tokens[inst].next; ++e) {
        if (!leaves[e])
            continue;
        size_t span = d->tokens[e].begin - cursor;
        memcpy(text + written, d->text + cursor, span);
        written += span;
        const char *snapshot = leaves[e];
        span = strlen(snapshot);
        memcpy(text + written, snapshot, span);
        written += span;
        cursor = d->tokens[e].end;
    }
    size_t span = d->tokens[inst].end - cursor;
    memcpy(text + written, d->text + cursor, span);
    text[written + span] = 0;
    *out = text;
    free(leaves);
    return 1;
}

char *qc_ctype_specialize_document(qc_context *c, const char *ir) {
    cj_doc d = {0}, bound_doc = {0};
    char *bound = NULL, *result = NULL;
    if (!c->ctype_snapshots[0]) {
        qc_fail(c, "ctype specialization requires an explicit locale profile");
        return NULL;
    }
    if (!cj_parse(&d, ir) || !qc_verify_document(c, &d) || !ctype_bind(c, &d, 0, &bound))
        goto done;
    const char *text = bound ? bound : ir;
    if (!cj_parse(&bound_doc, text))
        goto done;
    int list = cj_get(&bound_doc, 0, "instructions");
    result = malloc(strlen(text) + 1);
    if (!result) {
        qc_fail(c, "cannot allocate immutable locale document");
        goto done;
    }
    size_t length = bound_doc.tokens[list].begin + 1;
    memcpy(result, text, length);
    unsigned count = 0;
    for (int t = list + 1; t < bound_doc.tokens[list].next; t = bound_doc.tokens[t].next) {
        if (cj_equal(&bound_doc, cj_get(&bound_doc, t, "op"), "ctype_capture")) {
            if (!qc_verify_instruction(c, &bound_doc, t)) {
                free(result);
                result = NULL;
                goto done;
            }
            continue;
        }
        if (count++)
            result[length++] = ',';
        size_t size = bound_doc.tokens[t].end - bound_doc.tokens[t].begin;
        memcpy(result + length, text + bound_doc.tokens[t].begin, size);
        length += size;
    }
    const char *suffix = text + bound_doc.tokens[list].end - 1;
    memcpy(result + length, suffix, strlen(suffix) + 1);
done:
    if (!result && !c->error[0])
        qc_fail(c, "%s", d.error[0] ? d.error : bound_doc.error);
    free(bound);
    cj_free(&d);
    cj_free(&bound_doc);
    return result;
}
