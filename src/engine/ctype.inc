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
