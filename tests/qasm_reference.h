#ifndef CKET_QASM_REFERENCE_H
#define CKET_QASM_REFERENCE_H
#include <stddef.h>
#include <stdint.h>
#include <QuEST.h>

enum reference_kind { REF_H, REF_X, REF_PHASE, REF_GLOBAL, REF_MEASURE, REF_RESET };

typedef struct {
    enum reference_kind kind;
    unsigned count, wires[24], bit;
    double angle;
} reference_gate;

typedef struct {
    unsigned qubits, bits;
    reference_gate *gates;
    size_t count;
    char error[256];
} reference_circuit;

int reference_parse(reference_circuit *c, const char *qasm);
void reference_destroy(reference_circuit *c);
uint64_t reference_execute(const reference_circuit *c, Qureg q, int measurements);
#endif
