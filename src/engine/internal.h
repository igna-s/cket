#ifndef CKET_NATIVE_INTERNAL_H
#define CKET_NATIVE_INTERNAL_H
#include "quantum.h"
#include "json.h"
#define QC_MAX_INPUT 12
#define QC_MAX_PHYSICAL 128
#define QC_MAX_SIMULATED 24
#define QC_MAX_SOURCES 128

enum qc_kind { QC_H, QC_X, QC_PHASE, QC_MEASURE, QC_RESET };

typedef unsigned __int128 qc_mask;
#define QC_WIRE(n) ((qc_mask)1 << (n))

typedef struct {
    enum qc_kind kind;
    unsigned target;
    qc_mask mask, pattern;
    double angle;
} qc_gate;

typedef struct {
    uint32_t id;
    unsigned width, wires[64];
    uint64_t epoch;
    int alive, clean, prepared, ancillary, history, finished;
    int signed_type, basis_known, uniform;
    uint64_t basis_value, zero_mask, uniform_upper;
    char *definition;
    qc_mask definition_mask, definition_pattern;
    uint32_t deps[QC_MAX_SOURCES];
    uint64_t epochs[QC_MAX_SOURCES];
    unsigned ndeps;
} qc_register;

typedef struct qc_parsed_cache {
    char *text;
    cj_doc doc;
    struct qc_parsed_cache *next;
} qc_parsed_cache;

typedef struct qc_plan_cache {
    char *key;
    qc_gate *gates;
    size_t count;
    qc_mask outer, pattern, work;
    unsigned ancillary;
    struct qc_plan_cache *next;
} qc_plan_cache;

typedef struct qc_module_cache {
    char *text;
    qc_circuit *circuit;
    int allow_tables;
    struct qc_module_cache *next;
} qc_module_cache;

struct qc_context {
    int simulate, validation, structural, ignore_measurements, allow_tables, cpu_versions,
        collecting;
    unsigned symbolic_work;
    int recording;
    unsigned module_depth;
    int ctype_runtime;
    char *ctype_snapshots[14];
    qc_parsed_cache *parsed;
    qc_plan_cache *plans;
    qc_module_cache *modules;
    size_t parsed_bytes, plan_bytes, module_bytes;
    uint64_t json_parses, synthesis_runs, plan_hits;
    char error[512];
    qc_register *registers;
    size_t nregisters, register_capacity;
    qc_gate *gates;
    size_t ngates, gate_capacity;
    unsigned physical, free_wires[QC_MAX_PHYSICAL], nfree, logical_peak, ancillary_peak,
        history_peak;
    unsigned depths[QC_MAX_PHYSICAL], depth;
    uint64_t h, x, cx, ccx, mcx, phase, global_phase, measure, reset, rng;
    double complex *state;
    size_t dimension;
};

int qc_fail(qc_context *c, const char *format, ...);
int qc_emit(
    qc_context *c, enum qc_kind kind, unsigned target, qc_mask mask, qc_mask pattern, double angle);
int qc_sim_grow(qc_context *c, unsigned width);
void qc_sim_gate(qc_context *c, const qc_gate *gate);
int qc_sim_measure(qc_context *c, unsigned wire);
int qc_sim_clean(qc_context *c, const qc_register *reg);
int qc_apply_token(qc_context *c,
                   const cj_doc *d,
                   int instruction,
                   int has_argument,
                   uint64_t argument,
                   uint64_t *result);
qc_register *qc_lookup(qc_context *c, uint32_t id);
int qc_verify_expression(qc_context *c, const cj_doc *d, int expression);
int qc_verify_instruction(qc_context *c, const cj_doc *d, int instruction);
int qc_verify_document(qc_context *c, const cj_doc *d);
int qc_verify_ctype_profile(qc_context *c, const cj_doc *d, int expression, int mapping);
#endif
