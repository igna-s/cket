/* Fixed finite-domain reference: seven dense state profiles and one
 * complete 512-label controlled basis-action profile, each before and
 * after inversion, under both engine synthesis policies. The controlled store
 * uses independent reversal of its complete gate suffix, rather than a saved
 * compute inverse: its history is an output of a controlled swap. */
/* Independent C/libc reference for the experiment's complete joint states.
 * Terminal measurements are not executed. Unknown overwritten inputs remain
 * explicit history, and the saved inverse must restore clean private wires. */
#include "internal.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t basis;
    double complex amplitude;
} expected_state;

static void fail(const char *message) {
    fprintf(stderr, "memset state probe: %s\n", message);
    exit(1);
}

static size_t bits(const qc_register *r, unsigned value) {
    size_t basis = 0;
    if (!r || !r->alive)
        fail("required live register missing");
    for (unsigned bit = 0; bit < r->width; ++bit)
        basis |= (size_t)((value >> bit) & 1) << r->wires[bit];
    return basis;
}

static int order(const void *left, const void *right) {
    size_t a = ((const expected_state *)left)->basis;
    size_t b = ((const expected_state *)right)->basis;
    return (a > b) - (a < b);
}

static void compare(qc_context *c, unsigned profile, unsigned current, int inverse) {
    expected_state expected[512];
    unsigned count = 0;
    int history = profile < 4 || profile == 7;
    unsigned values = history ? 256 : 1;
    unsigned flags = profile >= 2 && profile <= 6 ? 2 : 1;
    qc_register *old = history ? qc_lookup(c, 0) : NULL;
    qc_register *result = !inverse || !history ? qc_lookup(c, current) : NULL;
    qc_register *flag = flags == 2 ? qc_lookup(c, profile == 6 ? 2 : 1) : NULL;
    qc_register *second = profile >= 6 ? qc_lookup(c, 1) : NULL;
    if (history && (!old || !old->history || current == 0))
        fail("unknown input was overwritten without a distinct history owner");
    for (unsigned x = 0; x < values; ++x)
        for (unsigned b = 0; b < flags; ++b) {
            uint8_t byte = (uint8_t)x;
            uint16_t word = 0x1234;
            uint8_t array[2] = {profile == 6 ? 17 : (uint8_t)x, 34};
            switch (profile) {
            case 0:
                memset(&byte, 17, 1);
                break;
            case 1:
                memset(&byte, byte, 1);
                break;
            case 2:
                if (b)
                    memset(&byte, 17, 1);
                break;
            case 3:
                memset(&byte, (int)b, 1);
                break;
            case 4:
                memset((unsigned char *)&word + 1, (int)b, 1);
                break;
            case 5:
                memset(&word, (int)b, sizeof word);
                break;
            case 6:
                memset(array, (int)b, sizeof array);
                break;
            case 7:
                memset(array, array[0], sizeof array);
                break;
            default:
                fail("unknown profile");
            }
            double angle = history ? .001 * x : 0;
            if (flags == 2)
                angle += .19 * b;
            if (profile == 0 || ((profile == 1 || profile == 2) && byte == 7) ||
                (profile >= 3 && profile <= 6 && b))
                angle += .17;
            size_t basis = history ? bits(old, x) : 0;
            if (flag)
                basis |= bits(flag, b);
            if (profile < 4 && !inverse)
                basis |= bits(result, byte);
            if (profile == 4 || profile == 5)
                basis |= bits(result, inverse ? 0x1234 : word);
            if (profile == 6)
                basis |= bits(result, inverse ? 17 : array[0]);
            if (profile == 7 && !inverse)
                basis |= bits(result, array[0]);
            if (second)
                basis |= bits(second, inverse ? 34 : array[1]);
            expected[count++] = (expected_state){basis, cexp(I * angle) / sqrt(values * flags)};
        }
    qsort(expected, count, sizeof *expected, order);
    for (unsigned i = 1; i < count; ++i)
        if (expected[i - 1].basis == expected[i].basis)
            fail("distinct retained inputs collide in the joint output");
    unsigned next = 0;
    for (size_t i = 0; i < qc_state_size(c); ++i) {
        double complex wanted =
            next < count && expected[next].basis == i ? expected[next++].amplitude : 0;
        double complex delta = qc_amplitude(c, i) - wanted;
        double re = creal(delta), im = cimag(delta);
        if (!isfinite(re) || !isfinite(im) || re * re + im * im > 1e-20)
            fail(inverse ? "saved inverse changed inputs/phases or left private garbage"
                         : "complete joint amplitudes disagree with classical memset");
    }
    if (next != count || c->measure || c->reset)
        fail("support escaped the state or an implicit channel appeared");
}

/* Complete labeled basis action for the controlled full-byte source profile.
 * Its canonical nine-H preparation is verified; the remaining reversible
 * X/phase gates are followed for each of the 512 input labels. */
static qc_mask memset_mask(const qc_register *r, unsigned value) {
    qc_mask result = 0;
    if (!r || !r->alive)
        fail("controlled source register missing");
    for (unsigned i = 0; i < r->width; ++i)
        result |= (qc_mask)((value >> i) & 1) << r->wires[i];
    return result;
}

static void memset_control_paths(qc_context *c, unsigned current, int inverse) {
    qc_register *result = qc_lookup(c, current), *flag = qc_lookup(c, 1), *history = NULL;
    if (current != 0 || !result || result->width != 8 || !flag || flag->width != 1 ||
        flag->wires[0] != 8)
        fail("controlled history/input/cleanup invariant failed");
    for (unsigned i = 0; i < 8; ++i)
        if (result->wires[i] != i)
            fail("controlled input preparation map changed");
    for (size_t i = 0; i < c->nregisters; ++i) {
        qc_register *r = &c->registers[i];
        if (!r->alive || r->id == current || r->id == 1)
            continue;
        if (history || !r->history || r->width != 8 || r->definition)
            fail("unexpected live controlled auxiliary");
        history = r;
    }
    if (!history)
        fail("controlled store lost its active previous value");
    qc_mask states[512];
    double complex amplitudes[512];
    for (unsigned i = 0; i < 512; ++i) {
        states[i] = i;
        amplitudes[i] = 1 / sqrt(512.0);
    }
    unsigned prepared = 0;
    int suffix = 0;
    for (size_t i = 0; i < c->ngates; ++i) {
        const qc_gate *g = &c->gates[i];
        if (g->kind == QC_H) {
            if (suffix || prepared >= 9 || g->mask || g->target != prepared++)
                fail("controlled reference requires canonical nine-H preparation");
            continue;
        }
        if (prepared != 9 || (g->kind != QC_X && g->kind != QC_PHASE))
            fail("unexpected operation/channel in controlled unitary suffix");
        suffix = 1;
        double complex phase = cexp(I * g->angle);
        for (unsigned label = 0; label < 512; ++label)
            if ((states[label] & g->mask) == g->pattern) {
                if (g->kind == QC_X)
                    states[label] ^= QC_WIRE(g->target);
                else
                    amplitudes[label] *= phase;
            }
    }
    if (prepared != 9 || c->measure || c->reset)
        fail("incomplete controlled preparation or implicit channel");
    if (inverse)
        for (size_t i = c->ngates; i-- > 0;) {
            const qc_gate *g = &c->gates[i];
            if (g->kind == QC_H)
                continue;
            for (unsigned label = 0; label < 512; ++label)
                if ((states[label] & g->mask) == g->pattern) {
                    if (g->kind == QC_X)
                        states[label] ^= QC_WIRE(g->target);
                    else
                        amplitudes[label] *= cexp(-I * g->angle);
                }
        }
    for (unsigned label = 0; label < 512; ++label) {
        unsigned x = label & 255, b = label >> 8;
        unsigned value = b ? 17 : x;
        qc_mask wanted = memset_mask(result, inverse ? x : value) | memset_mask(flag, b);
        if (!inverse)
            wanted |= memset_mask(history, b ? x : 0);
        double angle = inverse ? 0 : .001 * x + .19 * b + (value == 7 ? .17 : 0);
        if (states[label] != wanted ||
            cabs(amplitudes[label] - cexp(I * angle) / sqrt(512.0)) > 1e-10)
            fail("controlled labeled input/result/phase/private state mismatch");
    }
}

static unsigned memset_state_probe(const char *ir, unsigned profile) {
    char *text = strdup(ir);
    if (profile > 7)
        fail("invalid source profile");
    cj_doc d = {0};
    if (!text || !cj_parse(&d, text))
        fail("invalid IR input");
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(profile == 2 ? 0 : 1, 7);
        if (!c || !qc_verify_document(c, &d))
            fail(c ? qc_error(c) : "context allocation failed");
        qc_allow_tables(c, tables);
        int list = cj_get(&d, 0, "instructions");
        char *writes[16];
        unsigned nwrite = 0, current = UINT32_MAX;
        for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next) {
            int op = cj_get(&d, token, "op");
            uint64_t id;
            if (cj_equal(&d, op, "qmeasure")) {
                if (!cj_u64(&d, cj_get(&d, token, "id"), &id) || id > UINT32_MAX)
                    fail("invalid observation ID");
                current = (unsigned)id;
                break;
            }
            if (cj_equal(&d, op, "qxor_inplace")) {
                if (nwrite == sizeof writes / sizeof *writes)
                    fail("write recording budget exceeded");
                writes[nwrite++] = cj_copy(&d, token);
            }
            uint64_t result;
            if (!qc_apply_token(c, &d, token, 0, 0, &result))
                fail(qc_error(c));
        }
        if (current == UINT32_MAX)
            fail("source did not expose its current destination");
        if (profile == 2) {
            memset_control_paths(c, current, 0);
            memset_control_paths(c, current, 1);
            qc_destroy(c);
            continue;
        } else
            compare(c, profile, current, 0);
        while (nwrite) {
            char *operation = writes[--nwrite];
            if (!qc_apply(c, operation, NULL))
                fail(qc_error(c));
            free(operation);
        }
        if (profile < 4 || profile == 7) {
            qc_register *result = qc_lookup(c, current);
            if (!result || !result->definition)
                fail("history result has no saved inverse definition");
            size_t capacity = strlen(result->definition) + 128;
            char *operation = malloc(capacity);
            if (!operation)
                fail("inverse allocation failed");
            snprintf(operation,
                     capacity,
                     "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}",
                     current,
                     result->definition);
            if (!qc_apply(c, operation, NULL))
                fail(qc_error(c));
            snprintf(operation, capacity, "{\"op\":\"qrelease\",\"id\":%u}", current);
            if (!qc_apply(c, operation, NULL))
                fail(qc_error(c));
            free(operation);
        }
        compare(c, profile, current, 1);
        qc_destroy(c);
    }
    cj_free(&d);
    free(text);
    return 2;
}
