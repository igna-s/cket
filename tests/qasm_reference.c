/* Independent reader for the static OpenQASM 3 subset emitted by cket.
 * This adapter does not inspect QIR or call cket's gate executor. */
#include "qasm_reference.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void space(const char **p) {
    for (;;) {
        while (isspace((unsigned char)**p)) ++*p;
        if ((*p)[0] != '/' || (*p)[1] != '/') break;
        while (**p && **p != '\n') ++*p;
    }
}
static int take(const char **p, const char *text) {
    space(p); size_t n = strlen(text);
    if (strncmp(*p, text, n)) return 0;
    *p += n; return 1;
}
static int integer(const char **p, unsigned *out) {
    space(p); if (!isdigit((unsigned char)**p)) return 0;
    char *end; errno = 0; unsigned long n = strtoul(*p, &end, 10);
    if (errno || n > 1000000) return 0;
    *out = (unsigned)n; *p = end; return 1;
}
static int angle(const char **p, double *out) {
    space(p); char *end; errno = 0; double n = strtod(*p, &end);
    if (end == *p || errno || !isfinite(n)) return 0;
    *out = n; *p = end; return 1;
}
static int wire(const char **p, unsigned *out, unsigned size) {
    return take(p, "q[") && integer(p, out) && *out < size && take(p, "]");
}
void reference_destroy(reference_circuit *c) { free(c->gates); c->gates = NULL; c->count = 0; }
int reference_parse(reference_circuit *c, const char *qasm) {
    memset(c, 0, sizeof *c); const char *p = qasm; size_t capacity = 0; uint64_t measured = 0;
    if (!take(&p, "OPENQASM") || !take(&p, "3.0") || !take(&p, ";") ||
        !take(&p, "include") || !take(&p, "\"stdgates.inc\"") || !take(&p, ";") ||
        !take(&p, "qubit[") || !integer(&p, &c->qubits) || !c->qubits || c->qubits > 20 ||
        !take(&p, "]") || !take(&p, "q") || !take(&p, ";")) goto invalid;
    if (take(&p, "bit[")) {
        if (!integer(&p, &c->bits) || !c->bits || c->bits > 20 || !take(&p, "]") ||
            !take(&p, "results") || !take(&p, ";")) goto invalid;
    }
    for (;;) {
        space(&p); if (!*p) break;
        reference_gate g = {0}; unsigned controls = 0;
        if (take(&p, "results[")) {
            g.kind = REF_MEASURE; g.count = 1;
            if (!integer(&p, &g.bit) || g.bit >= c->bits || (measured & (UINT64_C(1) << g.bit)) ||
                !take(&p, "]") || !take(&p, "=") || !take(&p, "measure") || !wire(&p, &g.wires[0], c->qubits)) goto invalid;
            measured |= UINT64_C(1) << g.bit;
        } else if (take(&p, "reset")) {
            g.kind = REF_RESET; g.count = 1;
            if (!wire(&p, &g.wires[0], c->qubits)) goto invalid;
        } else {
            if (take(&p, "ctrl(")) {
                if (!integer(&p, &controls) || !controls || controls >= c->qubits || !take(&p, ")") || !take(&p, "@")) goto invalid;
            }
            if (take(&p, "gphase(")) {
                g.kind = REF_GLOBAL;
                if (controls || !angle(&p, &g.angle) || !take(&p, ")")) goto invalid;
            } else {
                if (take(&p, "h")) g.kind = REF_H;
                else if (take(&p, "x")) g.kind = REF_X;
                else if (take(&p, "p(")) {
                    g.kind = REF_PHASE;
                    if (!angle(&p, &g.angle) || !take(&p, ")")) goto invalid;
                } else goto invalid;
                g.count = controls + 1; uint64_t used = 0;
                for (unsigned i = 0; i < g.count; ++i) {
                    if ((i && !take(&p, ",")) || !wire(&p, &g.wires[i], c->qubits) || (used & (UINT64_C(1) << g.wires[i]))) goto invalid;
                    used |= UINT64_C(1) << g.wires[i];
                }
            }
        }
        if (!take(&p, ";") || c->count >= 2097152) goto invalid;
        if (c->count == capacity) {
            capacity = capacity ? capacity * 2 : 128;
            reference_gate *next = realloc(c->gates, capacity * sizeof *next);
            if (!next) { snprintf(c->error, sizeof c->error, "out of memory importing QASM"); reference_destroy(c); return 0; }
            c->gates = next;
        }
        c->gates[c->count++] = g;
    }
    if (c->bits && measured != (UINT64_C(1) << c->bits) - 1) goto invalid;
    return 1;
invalid:
    snprintf(c->error, sizeof c->error, "unsupported or malformed exported QASM at byte %zu", (size_t)(p - qasm));
    reference_destroy(c); return 0;
}
uint64_t reference_execute(const reference_circuit *c, Qureg q, int measurements) {
    uint64_t result = 0;
    for (size_t i = 0; i < c->count; ++i) {
        const reference_gate *g = &c->gates[i]; int wires[24];
        for (unsigned j = 0; j < g->count; ++j) wires[j] = (int)g->wires[j];
        if (g->kind == REF_MEASURE) { if (measurements) result |= (uint64_t)measure(q, wires[0]) << g->bit; }
        else if (g->kind == REF_RESET) { if (measure(q, wires[0])) pauliX(q, wires[0]); }
        else if (g->kind == REF_GLOBAL) {
            ComplexMatrix2 u = {{{cos(g->angle), 0}, {0, cos(g->angle)}}, {{sin(g->angle), 0}, {0, sin(g->angle)}}};
            unitary(q, 0, u);
        } else if (g->kind == REF_PHASE) multiControlledPhaseShift(q, wires, (int)g->count, g->angle);
        else if (g->count == 1) { if (g->kind == REF_H) hadamard(q, wires[0]); else pauliX(q, wires[0]); }
        else {
            double s = 1 / sqrt(2.0); ComplexMatrix2 u = {0};
            if (g->kind == REF_H) { u.real[0][0] = s; u.real[0][1] = s; u.real[1][0] = s; u.real[1][1] = -s; }
            else { u.real[0][1] = 1; u.real[1][0] = 1; }
            multiControlledUnitary(q, wires, (int)g->count - 1, wires[g->count - 1], u);
        }
    }
    return result;
}
