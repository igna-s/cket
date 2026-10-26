#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void require(int ok, const char *message, qc_context *c) {
    if (!ok) {
        fprintf(stderr, "register patch: %s: %s\n", message, qc_error(c));
        exit(1);
    }
    ++checks;
}

static void apply(qc_context *c, const char *text) {
    require(qc_apply(c, text, NULL), text, c);
}

static void allocate(qc_context *c, uint32_t id, unsigned width, int sign, int uniform) {
    char text[200];
    snprintf(text,
             sizeof text,
             "{\"op\":\"qalloc\",\"id\":%u,\"width\":%u,\"signed\":%s}",
             id,
             width,
             sign ? "true" : "false");
    apply(c, text);
    if (uniform >= 0) {
        snprintf(text,
                 sizeof text,
                 "{\"op\":\"qprepare\",\"id\":%u,\"lower\":0,\"upper\":%u}",
                 id,
                 uniform != 0);
        apply(c, text);
    }
}

static uint64_t full_mask(unsigned width) {
    return (UINT64_C(1) << width) - 1;
}

static qc_mask bits(qc_context *c, uint32_t id, uint64_t value) {
    qc_register *r = qc_lookup(c, id);
    qc_mask result = 0;
    for (unsigned i = 0; i < r->width; ++i)
        if ((value >> i) & 1)
            result |= QC_WIRE(r->wires[i]);
    return result;
}

static uint64_t read_bits(qc_context *c, uint32_t id, qc_mask basis) {
    qc_register *r = qc_lookup(c, id);
    uint64_t value = 0;
    for (unsigned i = 0; i < r->width; ++i)
        value |= (uint64_t)((basis >> r->wires[i]) & 1) << i;
    return value;
}

static qc_context *fixture(unsigned width, int sign, int derived) {
    qc_context *c = qc_create_sparse(7, 1024);
    require(c != NULL, "context", c);
    c->recording = 1;
    qc_allow_tables(c, 0);
    allocate(c, 0, width, sign, -1); /* History identity zero is valid. */
    allocate(c, 1, width, sign, derived ? -1 : 1);
    allocate(c, 2, 1, 0, 1);
    allocate(c, 3, 1, 0, 1);
    allocate(c, 4, 1, 0, 1);
    allocate(c, 5, 1, 0, derived ? 1 : 0);
    unsigned marker = width >= 16 ? 256 : width == 8 ? 4 : 0;
    char text[1300], expression[900];
    snprintf(
        expression,
        sizeof expression,
        "{\"op\":\"bor\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":5,\"bits\":1,\"signed\":false}]},{\"op\":\"select\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":2,\"bits\":1,\"signed\":false},{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":%u},{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":0}]}]}",
        width,
        sign ? "true" : "false",
        width,
        sign ? "true" : "false",
        width,
        sign ? "true" : "false",
        width,
        sign ? "true" : "false",
        marker,
        width,
        sign ? "true" : "false");
    if (derived && sign) {
        snprintf(
            text,
            sizeof text,
            "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"bnot\",\"bits\":%u,\"signed\":true,\"args\":[%s]}}",
            width,
            expression);
    } else {
        snprintf(text,
                 sizeof text,
                 "{\"op\":\"%s\",\"id\":1,\"expr\":%s}",
                 derived ? "qcompute" : "qxor_inplace",
                 expression);
    }
    if (derived || width != 1)
        apply(c, text);
    if (sign && !derived) {
        snprintf(text,
                 sizeof text,
                 "{\"op\":\"qxor_inplace\",\"id\":1,\"value\":%llu}",
                 (unsigned long long)full_mask(width));
        apply(c, text);
    }
    const unsigned ids[] = {2, derived ? 5 : 1, 3, 4};
    const double angles[] = {.11, .23, .41, .17};
    for (unsigned i = 0; i < 4; ++i) {
        qc_mask wire = QC_WIRE(qc_lookup(c, ids[i])->wires[0]);
        require(qc_emit(c, QC_PHASE, 0, wire, wire, angles[i]), "phase", c);
        qc_lookup(c, ids[i])->uniform = 0;
    }
    return c;
}

static void state(qc_context *c,
                  unsigned width,
                  int sign,
                  int derived,
                  unsigned mode,
                  uint64_t mask,
                  int patched,
                  int cleaned) {
    require(qc_sparse_size(c) == 16, "complete support", c);
    for (size_t row = 0; row < 16; ++row) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(c, row, &low, &high, &amplitude), "state row", c);
        qc_mask actual = ((qc_mask)high << 64) | low;
        unsigned a = (unsigned)read_bits(c, 2, actual), x = (unsigned)read_bits(c, 3, actual),
                 flag = (unsigned)read_bits(c, 4, actual);
        int active = mode == 0 || (mode == 1 ? flag : mode == 2 ? !flag : flag && !a);
        uint64_t current = read_bits(c, 1, actual), history = read_bits(c, 0, actual);
        unsigned b = derived                           ? (unsigned)read_bits(c, 5, actual)
                     : patched && active && (mask & 1) ? (unsigned)(history & 1) ^ sign
                                                       : (unsigned)(current & 1) ^ sign;
        uint64_t old = b | (width >= 16 ? (uint64_t)a << 8 : width == 8 ? (uint64_t)a << 2 : 0);
        if (sign)
            old = (~old) & full_mask(width);
        uint64_t value = patched && active ? (old & ~mask) | (x & mask) : old;
        uint64_t saved = patched && active && !cleaned ? old & mask : 0;
        qc_mask expected = bits(c, 0, saved) | bits(c, 1, value) | bits(c, 2, a) | bits(c, 3, x) |
                           bits(c, 4, flag) | bits(c, 5, derived ? b : 0);
        double angle = .11 * a + .23 * (derived ? b : (b ^ sign)) + .41 * x + .17 * flag;
        require(actual == expected && cabs(amplitude - cexp(I * angle) / 4) < 1e-10,
                "phased joint state or private garbage",
                c);
    }
    require(!c->measure && !c->reset, "implicit observation or reset", c);
}

static void complete_states(void) {
    for (unsigned width = 1; width <= 32; width = width == 1 ? 8 : 2 * width)
        for (int sign = 0; sign <= (width != 1); ++sign)
            for (int derived = 0; derived < 2; ++derived)
                for (unsigned mode = 0; mode < 4; ++mode)
                    for (unsigned selection = 0; selection < 3; ++selection) {
                        uint64_t mask = selection == 0 ? 0 : selection == 1 ? 1 : full_mask(width);
                        qc_context *c = fixture(width, sign, derived);
                        state(c, width, sign, derived, mode, mask, 0, 0);
                        const char *guards =
                            mode == 0   ? "[]"
                            : mode == 1 ? "[{\"id\":4,\"value\":1}]"
                            : mode == 2 ? "[{\"id\":4,\"value\":0}]"
                                        : "[{\"id\":4,\"value\":1},{\"id\":2,\"value\":0}]";
                        char text[600];
                        snprintf(
                            text,
                            sizeof text,
                            "{\"op\":\"qpatch\",\"id\":1,\"history\":0,\"bit_mask\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},\"expr\":{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":3,\"bits\":1,\"signed\":false}]},\"controls\":%s}",
                            (unsigned long long)mask,
                            width,
                            sign ? "true" : "false",
                            guards);
                        apply(c, text);
                        state(c, width, sign, derived, mode, mask, 1, 0);
                        if (derived || !mask) {
                            apply(c, "{\"op\":\"quncompute\",\"id\":0,\"saved\":true}");
                            state(c, width, sign, derived, mode, mask, 1, 1);
                        }
                        size_t gates = c->ngates;
                        for (size_t i = gates; i > 0; --i) {
                            qc_gate gate = c->gates[i - 1];
                            require(qc_emit(c,
                                            gate.kind,
                                            gate.target,
                                            gate.mask,
                                            gate.pattern,
                                            gate.kind == QC_PHASE ? -gate.angle : gate.angle),
                                    "gate inverse",
                                    c);
                        }
                        require(cabs(qc_amplitude(c, 0) - 1) < 1e-10,
                                "inverse did not restore zero",
                                c);
                        qc_destroy(c);
                    }
}

static void rejected_masks(void) {
    const char *masks[] = {"true", "-1", "1.5", "256"};
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; ++i) {
        qc_context *c = fixture(8, 0, 0);
        size_t gates = c->ngates;
        uint64_t epoch = qc_lookup(c, 1)->epoch;
        char text[500];
        snprintf(
            text,
            sizeof text,
            "{\"op\":\"qpatch\",\"id\":1,\"history\":0,\"bit_mask\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%s},\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":0}}",
            masks[i]);
        require(!qc_apply(c, text, NULL), "invalid mask accepted", c);
        require(c->ngates == gates && qc_lookup(c, 1)->epoch == epoch,
                "invalid mask mutated target",
                c);
        qc_destroy(c);
    }
}

int main(void) {
    complete_states();
    rejected_masks();
    printf(
        "%u register patch checks passed; typed masks, history identity zero, phased root/producer states, cleanup and full gate inverses have finite scopes.\n",
        checks);
    return 0;
}
