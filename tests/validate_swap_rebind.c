#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static int negative_fixture;

static void require(int ok, const char *message, qc_context *c) {
    if (!ok) {
        fprintf(stderr, "swap inverse: %s: %s\n", message, qc_error(c));
        exit(1);
    }
    ++checks;
}

static void apply(qc_context *c, const char *text) {
    require(qc_apply(c, text, NULL), text, c);
}

static void prepare(qc_context *c, uint32_t id, unsigned width, int sign) {
    char text[200];
    snprintf(text,
             sizeof text,
             "{\"op\":\"qalloc\",\"id\":%u,\"width\":%u,\"signed\":%s}",
             id,
             width,
             sign ? "true" : "false");
    apply(c, text);
    snprintf(text, sizeof text, "{\"op\":\"qprepare\",\"id\":%u,\"lower\":0,\"upper\":1}", id);
    apply(c, text);
}

static qc_context *fixture(unsigned width, int sign, int masked) {
    qc_context *c = qc_create_sparse(7, 1024);
    require(c != NULL, "context", c);
    c->recording = 1;
    qc_allow_tables(c, 0);
    prepare(c, 1, width, sign);
    prepare(c, 2, width, sign);
    prepare(c, 3, 1, 0);
    prepare(c, 4, 1, 0);
    if (negative_fixture) {
        qc_register *a = qc_lookup(c, 1);
        for (unsigned bit = 1; bit < width; ++bit)
            require(qc_emit(c, QC_X, a->wires[bit], 0, 0, 0), "negative preparation", c);
        a->zero_mask = 0;
        a->support_interval = 0;
        a->uniform = 0;
        ++a->epoch;
    }
    char text[1200], expression[900];
    unsigned output = masked ? 8 : width;
    snprintf(text,
             sizeof text,
             "{\"op\":\"qalloc\",\"id\":5,\"width\":%u,\"signed\":%s,\"ancilla\":true}",
             output,
             masked ? "false"
             : sign ? "true"
                    : "false");
    apply(c, text);
    if (masked)
        snprintf(
            expression,
            sizeof expression,
            "{\"op\":\"cast\",\"bits\":8,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":2,\"bits\":1,\"signed\":false}]}");
    else if (width == 1 || negative_fixture)
        snprintf(expression,
                 sizeof expression,
                 "{\"op\":\"reg\",\"id\":1,\"bits\":%u,\"signed\":%s}",
                 width,
                 sign ? "true" : "false");
    else
        snprintf(
            expression,
            sizeof expression,
            "{\"op\":\"bxor\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":%u,\"signed\":%s},{\"op\":\"shl\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":2,\"bits\":%u,\"signed\":%s},{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":1}]}]}",
            width,
            sign ? "true" : "false",
            width,
            sign ? "true" : "false",
            width,
            sign ? "true" : "false",
            width,
            sign ? "true" : "false",
            width,
            sign ? "true" : "false");
    snprintf(text,
             sizeof text,
             "{\"op\":\"qcompute\",\"id\":5,\"expr\":%s%s}",
             expression,
             masked ? ",\"controls\":[{\"id\":1,\"value\":1}]" : "");
    apply(c, text);
    for (unsigned id = 1; id <= 5; ++id) {
        const double angles[] = {0, .11, .23, .31, .19, .37};
        qc_mask bit = QC_WIRE(qc_lookup(c, id)->wires[0]);
        require(qc_emit(c, QC_PHASE, 0, bit, bit, angles[id]), "fixture phase", c);
        qc_lookup(c, id)->uniform = 0;
    }
    return c;
}

static qc_mask value_bits(qc_context *c, uint32_t id, unsigned value) {
    qc_register *r = qc_lookup(c, id);
    qc_mask result = 0;
    for (unsigned i = 0; i < r->width; ++i)
        if ((value >> i) & 1)
            result |= QC_WIRE(r->wires[i]);
    return result;
}

static void
state(qc_context *c, unsigned width, int masked, unsigned swaps, unsigned mode, int inverse) {
    require(qc_sparse_size(c) == 16, "complete support", c);
    for (size_t row = 0; row < 16; ++row) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(c, row, &low, &high, &amplitude), "state row", c);
        qc_mask actual = ((qc_mask)high << 64) | low;
        unsigned a = (unsigned)((actual >> qc_lookup(c, 1)->wires[0]) & 1);
        unsigned b = (unsigned)((actual >> qc_lookup(c, 2)->wires[0]) & 1);
        unsigned flag = (unsigned)((actual >> qc_lookup(c, 3)->wires[0]) & 1);
        unsigned extra = (unsigned)((actual >> qc_lookup(c, 4)->wires[0]) & 1);
        int active = mode == 0 ? 1 : mode == 1 ? flag : mode == 2 ? !flag : flag && !extra;
        if (active && (swaps & 1)) {
            unsigned swap = a;
            a = b;
            b = swap;
        }
        unsigned old_a =
            negative_fixture ? ((width == 32 ? UINT32_MAX : (1u << width) - 1) & ~1u) | a : a;
        unsigned value = negative_fixture ? old_a : masked ? a * b : width == 1 ? a : a ^ (b << 1);
        double angle = .11 * a + .23 * b + .31 * flag + .19 * extra + .37 * (value & 1);
        unsigned now_a = active && (swaps & 1) ? b : old_a,
                 now_b = active && (swaps & 1) ? old_a : b;
        qc_mask expected = value_bits(c, 1, now_a) | value_bits(c, 2, now_b) |
                           value_bits(c, 3, flag) | value_bits(c, 4, extra);
        if (!inverse)
            expected |= value_bits(c, 5, value);
        require(actual == expected && cabs(amplitude - cexp(I * angle) / 4) < 1e-10,
                "full phased joint state or private garbage",
                c);
    }
    require(!c->measure && !c->reset, "implicit channel", c);
}

static void complete_states(void) {
    for (negative_fixture = 0; negative_fixture < 2; ++negative_fixture)
        for (unsigned width = 1; width <= 32; width = width == 1 ? 8 : width * 2)
            for (int sign = 0; sign < 2; ++sign) {
                if (width == 1 && sign)
                    continue;
                if (negative_fixture && (width == 1 || !sign))
                    continue;
                for (unsigned mode = 0; mode < 4; ++mode)
                    for (unsigned rounds = 1; rounds <= 3; ++rounds) {
                        qc_context *c = fixture(width, sign, 0);
                        state(c, width, 0, 0, mode, 0);
                        const char *guards =
                            mode == 0   ? "[]"
                            : mode == 1 ? "[{\"id\":3,\"value\":1}]"
                            : mode == 2 ? "[{\"id\":3,\"value\":0}]"
                                        : "[{\"id\":3,\"value\":1},{\"id\":4,\"value\":0}]";
                        char text[300];
                        snprintf(
                            text,
                            sizeof text,
                            "{\"op\":\"qswap\",\"id\":1,\"other\":2,\"rebind\":true,\"controls\":%s}",
                            guards);
                        for (unsigned n = 0; n < rounds; ++n)
                            apply(c, text);
                        if (negative_fixture)
                            for (unsigned id = 1; id <= 2; ++id)
                                require(!qc_lookup(c, id)->support_interval ||
                                            qc_lookup(c, id)->support_upper <=
                                                ((UINT64_C(1) << (width - 1)) - 1),
                                        "signed negative support was certified nonnegative",
                                        c);
                        state(c, width, 0, rounds, mode, 0);
                        apply(c, "{\"op\":\"quncompute\",\"id\":5,\"saved\":true}");
                        state(c, width, 0, rounds, mode, 1);
                        size_t gates = c->ngates;
                        for (size_t i = gates; i > 0; --i) {
                            qc_gate gate = c->gates[i - 1];
                            require(qc_emit(c,
                                            gate.kind,
                                            gate.target,
                                            gate.mask,
                                            gate.pattern,
                                            gate.kind == QC_PHASE ? -gate.angle : gate.angle),
                                    "full gate inverse",
                                    c);
                        }
                        require(cabs(qc_amplitude(c, 0) - 1) < 1e-10,
                                "complete inverse did not restore zero",
                                c);
                        for (size_t row = 0; row < qc_sparse_size(c); ++row) {
                            uint64_t low, high;
                            double complex amplitude;
                            require(qc_sparse_entry(c, row, &low, &high, &amplitude) &&
                                        (!(low || high) || cabs(amplitude) < 1e-10),
                                    "inverse left nonzero amplitudes",
                                    c);
                        }
                        qc_destroy(c);
                    }
            }
    negative_fixture = 0;
    for (unsigned mode = 1; mode <= 2; ++mode) {
        qc_context *c = fixture(1, 0, 1);
        state(c, 1, 1, 0, mode, 0);
        char text[220];
        snprintf(
            text,
            sizeof text,
            "{\"op\":\"qswap\",\"id\":1,\"other\":2,\"rebind\":true,\"controls\":[{\"id\":3,\"value\":%u}]}",
            mode == 1);
        apply(c, text);
        state(c, 1, 1, 1, mode, 0);
        require(qc_lookup(c, 5)->definition_mask == 0,
                "old physical gate control was not transported",
                c);
        apply(c, "{\"op\":\"quncompute\",\"id\":5,\"saved\":true}");
        state(c, 1, 1, 1, mode, 1);
        qc_destroy(c);
    }
}

static void rejected_graph(int stale) {
    qc_context *c = fixture(1, 0, 0);
    if (stale) {
        require(qc_emit(c, QC_X, qc_lookup(c, 1)->wires[0], 0, 0, 0), "stale fixture mutation", c);
        ++qc_lookup(c, 1)->epoch;
    } else {
        apply(c, "{\"op\":\"qalloc\",\"id\":6,\"width\":1,\"signed\":false,\"ancilla\":true}");
        apply(
            c,
            "{\"op\":\"qcompute\",\"id\":6,\"expr\":{\"op\":\"reg\",\"id\":5,\"bits\":1,\"signed\":false}}");
    }
    size_t bytes = c->sparse_count * sizeof *c->sparse, gates = c->ngates;
    void *before = malloc(bytes);
    require(before != NULL, "snapshot", c);
    memcpy(before, c->sparse, bytes);
    uint64_t epoch = qc_lookup(c, 1)->epoch;
    char *definition = malloc(strlen(qc_lookup(c, 5)->definition) + 1);
    require(definition != NULL, "definition snapshot", c);
    strcpy(definition, qc_lookup(c, 5)->definition);
    char text[220];
    snprintf(
        text,
        sizeof text,
        "{\"op\":\"qswap\",\"id\":1,\"other\":2,\"rebind\":true,\"controls\":[{\"id\":%u,\"value\":1}]}",
        stale ? 3 : 6);
    require(!qc_apply(c, text, NULL) &&
                (strstr(qc_error(c), stale ? "epochs" : "cycle") ||
                 (!stale && strstr(qc_error(c), "decision")) || strstr(qc_error(c), "self")),
            "invalid inverse graph accepted",
            c);
    require(c->ngates == gates && qc_lookup(c, 1)->epoch == epoch &&
                !strcmp(qc_lookup(c, 5)->definition, definition) &&
                !memcmp(before, c->sparse, bytes),
            "failed preflight mutated payload or inverse",
            c);
    free(definition);
    free(before);
    qc_destroy(c);
}

int main(void) {
    complete_states();
    rejected_graph(0);
    rejected_graph(1);
    printf(
        "%u swap inverse checks passed; complete phased states, repeated exchanges, transported Boolean masks, gate inverses and graph rejections have separate scopes.\n",
        checks);
    return 0;
}
