#ifndef CKET_NATIVE_QUANTUM_H
#define CKET_NATIVE_QUANTUM_H
#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef struct qc_context qc_context;
typedef struct qc_circuit qc_circuit;
/* ABI v1: ordered typed register borrows; circuit artifacts contain qborrow
 * declarations and unitary operations, with all internal storage released. */
qc_circuit *qc_circuit_compile(const char *ir, char *error, size_t error_size);
void qc_circuit_destroy(qc_circuit *circuit);
int qc_circuit_apply(qc_context *context,
                     const qc_circuit *circuit,
                     const uint32_t *bindings,
                     size_t count,
                     int inverse,
                     const uint32_t *controls,
                     const unsigned *values,
                     size_t control_count);
qc_context *qc_create(int simulate, uint64_t seed);
void qc_allow_tables(qc_context *context, int allowed);
void qc_destroy(qc_context *context);
const char *qc_error(const qc_context *context);
int qc_apply(qc_context *context, const char *operation, uint64_t *result);
int qc_apply_arg(qc_context *context, const char *operation, uint64_t argument);
int qc_apply_phase(qc_context *context, const char *operation, double angle);
int qc_apply_index(qc_context *context,
                   const char *operation,
                   uint64_t index,
                   int has_argument,
                   uint64_t argument,
                   uint64_t *result);
int qc_document(qc_context *context, const char *ir, int validation, int ignore_measurements);
int qc_qasm(const qc_context *context, FILE *output);
void qc_resources(const qc_context *context, FILE *output);
unsigned qc_qubits(const qc_context *context);
size_t qc_state_size(const qc_context *context);
double complex qc_amplitude(const qc_context *context, size_t index);
/* Read bounded text, rejecting embedded NUL bytes rather than truncating. */
char *qc_read_file(const char *path);
#endif
