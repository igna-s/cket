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
/* Opt-in sparse statevector: 1..QC_SPARSE_MAX_STATES occupied basis labels,
 * up to the existing 128 physical wires. Exceeding support fails explicitly;
 * nonzero amplitudes are never intentionally pruned. */
#define QC_SPARSE_MAX_STATES 1048576u
qc_context *qc_create_sparse(uint64_t seed, size_t max_states);
size_t qc_sparse_size(const qc_context *context);
int qc_sparse_entry(const qc_context *context,
                    size_t row,
                    uint64_t *low,
                    uint64_t *high,
                    double complex *amplitude);
void qc_allow_tables(qc_context *context, int allowed);
/* Native execution captures the effective thread locale at ctype_capture.
 * Static synthesis instead requires an explicitly named immutable locale. */
void qc_ctype_runtime(qc_context *context);
int qc_ctype_locale(qc_context *context, const char *name);
/* Freeze every top-level ctype capture to the explicitly configured profile.
 * This is a declared static precondition, not reconstruction of a CPU trace.
 * The caller owns the returned IR text. Numeric CPU bindings are unchanged. */
char *qc_ctype_specialize_document(qc_context *context, const char *ir);
void qc_destroy(qc_context *context);
const char *qc_error(const qc_context *context);
int qc_apply(qc_context *context, const char *operation, uint64_t *result);
/* Conservative support query for a static zero-angle qphase envelope.
 * Zero is returned in *possible only from saved symbolic value/epoch facts.
 * Unknown support returns one; no gate, measurement or reset is emitted. */
int qc_controls_possible(qc_context *context, const char *envelope, int *possible);
int qc_apply_arg(qc_context *context, const char *operation, uint64_t argument);
/* Bind declared CPU integer inputs before synthesizing a quantum operation.
 * The returned operation owns its text and must be freed by the caller. */
char *
qc_bind_operation(qc_context *context, const char *operation, const uint64_t *values, size_t count);
int qc_apply_bound(qc_context *context,
                   const char *operation,
                   const uint64_t *values,
                   size_t count,
                   uint64_t *result);
/* Binding rows are ordered {instruction: index, values: [raw integers]}.
 * Every qbind instruction requires exactly one row. */
char *qc_bind_document(qc_context *context, const char *ir, const char *bindings);
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
/* Dense extent; zero for nonsimulating or sparse contexts. Sparse enumeration
 * uses qc_sparse_size/entry; qc_amplitude still queries a low-size_t label. */
size_t qc_state_size(const qc_context *context);
double complex qc_amplitude(const qc_context *context, size_t index);
/* Read bounded text, rejecting embedded NUL bytes rather than truncating. */
char *qc_read_file(const char *path);
#endif
