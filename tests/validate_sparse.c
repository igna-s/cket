#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void require(int condition, const char *message) {
    if (!condition)
        validation_fail("sparse simulation: %s", message);
}

static qc_context *context(int sparse, unsigned width, uint64_t seed, size_t limit) {
    qc_context *c = sparse ? qc_create_sparse(seed, limit) : qc_create(1, seed);
    require(c != NULL, "create context");
    char operation[128];
    snprintf(operation,
             sizeof operation,
             "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",
             width);
    require(qc_apply(c, operation, NULL), qc_error(c));
    require(qc_apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}", NULL),
            qc_error(c));
    return c;
}

static void compare(qc_context *dense, qc_context *sparse) {
    double norm = 0;
    for (size_t index = 0; index < qc_state_size(dense); ++index)
        require(cabs(qc_amplitude(dense, index) - qc_amplitude(sparse, index)) < 2e-12,
                "dense/sparse complex amplitudes differ");
    for (size_t row = 0; row < qc_sparse_size(sparse); ++row) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(sparse, row, &low, &high, &amplitude), "enumerate support");
        require(!high && low < qc_state_size(dense), "unexpected sparse label");
        norm += creal(amplitude) * creal(amplitude) + cimag(amplitude) * cimag(amplitude);
    }
    require(fabs(norm - 1) < 2e-12, "state norm");
}

static void gate_domains(void) {
    uint64_t random = 20261020;
    for (unsigned test = 0; test < 32; ++test) {
        unsigned width = 2 + test % 5;
        qc_context *dense = context(0, width, 7, 0);
        qc_context *sparse = context(1, width, 7, (size_t)1 << width);
        qc_gate gates[96];
        for (unsigned step = 0; step < 96; ++step) {
            qc_gate g = {0};
            g.kind = (enum qc_kind)validation_bounded(&random, 3);
            g.target = (unsigned)validation_bounded(&random, width);
            uint64_t width_mask = (UINT64_C(1) << width) - 1;
            uint64_t mask_bits = validation_random(&random) & width_mask;
            mask_bits &= ~(UINT64_C(1) << g.target);
            g.mask = qc_mask_from_u64(mask_bits);
            g.pattern = qc_mask_from_u64(validation_random(&random) & mask_bits);
            g.angle = ((int)validation_bounded(&random, 101) - 50) * .037;
            gates[step] = g;
            require(qc_emit(dense, g.kind, g.target, g.mask, g.pattern, g.angle), qc_error(dense));
            require(qc_emit(sparse, g.kind, g.target, g.mask, g.pattern, g.angle),
                    qc_error(sparse));
            compare(dense, sparse);
        }
        for (unsigned step = 96; step > 0; --step) {
            qc_gate g = gates[step - 1];
            require(qc_emit(dense, g.kind, g.target, g.mask, g.pattern, -g.angle), qc_error(dense));
            require(qc_emit(sparse, g.kind, g.target, g.mask, g.pattern, -g.angle),
                    qc_error(sparse));
        }
        compare(dense, sparse);
        require(cabs(qc_amplitude(sparse, 0) - 1) < 2e-12, "inverse did not restore zero");
        require(qc_state_size(sparse) == 0, "sparse context reported a dense extent");
        qc_destroy(dense);
        qc_destroy(sparse);
        ++checks;
    }
}

static void high_wires(void) {
    qc_context *c = context(1, 64, 7, 4);
    require(qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":64,\"signed\":false}", NULL),
            qc_error(c));
    require(qc_apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":0}", NULL),
            qc_error(c));
    require(qc_emit(c, QC_H, 127, qc_mask_zero(), qc_mask_zero(), 0), qc_error(c));
    require(qc_emit(c, QC_X, 63, QC_WIRE(127), QC_WIRE(127), 0), qc_error(c));
    require(qc_emit(c, QC_X, 64, QC_WIRE(127), qc_mask_zero(), 0), qc_error(c));
    qc_mask mask = qc_mask_or(QC_WIRE(127), QC_WIRE(63));
    require(qc_emit(c, QC_PHASE, 0, mask, mask, .37), qc_error(c));
    require(qc_sparse_size(c) == 2, "wide Bell support");
    unsigned seen = 0;
    for (size_t row = 0; row < 2; ++row) {
        uint64_t low, high;
        double complex amplitude;
        require(qc_sparse_entry(c, row, &low, &high, &amplitude), "wide enumeration");
        int branch = !!(high & (UINT64_C(1) << 63));
        require(low == (branch ? UINT64_C(1) << 63 : 0) && high == (branch ? UINT64_C(1) << 63 : 1),
                "wide/open control label");
        double complex expected = (branch ? cexp(I * .37) : 1) / sqrt(2);
        require(cabs(amplitude - expected) < 1e-12, "wide relative phase");
        seen |= 1u << branch;
    }
    require(seen == 3, "missing wide branch");
    require(qc_emit(c, QC_PHASE, 0, mask, mask, -.37), qc_error(c));
    require(qc_emit(c, QC_X, 64, QC_WIRE(127), qc_mask_zero(), 0), qc_error(c));
    require(qc_emit(c, QC_X, 63, QC_WIRE(127), QC_WIRE(127), 0), qc_error(c));
    require(qc_emit(c, QC_H, 127, qc_mask_zero(), qc_mask_zero(), 0), qc_error(c));
    require(cabs(qc_amplitude(c, 0) - 1) < 1e-12, "wide inverse");
    require(qc_sim_clean(c, qc_lookup(c, 0)) && qc_sim_clean(c, qc_lookup(c, 1)), "wide cleanup");
    qc_destroy(c);
    ++checks;

    c = context(1, 64, 7, 2);
    require(qc_emit(c, QC_X, 63, qc_mask_zero(), qc_mask_zero(), 0), qc_error(c));
    require(!qc_sim_clean(c, qc_lookup(c, 0)), "dirty high wire was released");
    qc_destroy(c);
    ++checks;

    /* Verify wires > 128 up to 900+ via qc_sparse_entry_words. */
    c = qc_create_sparse(7, 4);
    require(c != NULL, "high wire sparse context");
    c->physical = 910;
    require(qc_emit(c, QC_H, 900, qc_mask_zero(), qc_mask_zero(), 0), qc_error(c));
    require(qc_emit(c, QC_X, 5, QC_WIRE(900), QC_WIRE(900), 0), qc_error(c));
    qc_mask hi_phase = qc_mask_or(QC_WIRE(900), QC_WIRE(5));
    require(qc_emit(c, QC_PHASE, 0, hi_phase, hi_phase, .41), qc_error(c));
    require(qc_sparse_size(c) == 2, "ultra-wide Bell support");
    unsigned seen_hi = 0;
    for (size_t row = 0; row < 2; ++row) {
        uint64_t words[QC_MASK_WORDS] = {0};
        double complex amplitude;
        require(qc_sparse_entry_words(c, row, words, QC_MASK_WORDS, &amplitude),
                "ultra-wide enumeration");
        int branch = (words[900 / 64] >> (900 % 64)) & 1;
        require((words[0] == (branch ? UINT64_C(32) : 0)), "ultra-wide low word");
        double complex expected = (branch ? cexp(I * .41) : 1) / sqrt(2);
        require(cabs(amplitude - expected) < 1e-12, "ultra-wide relative phase");
        seen_hi |= 1u << branch;
    }
    require(seen_hi == 3, "missing ultra-wide branch");
    require(qc_emit(c, QC_PHASE, 0, hi_phase, hi_phase, -.41), qc_error(c));
    require(qc_emit(c, QC_X, 5, QC_WIRE(900), QC_WIRE(900), 0), qc_error(c));
    require(qc_emit(c, QC_H, 900, qc_mask_zero(), qc_mask_zero(), 0), qc_error(c));
    require(cabs(qc_amplitude(c, 0) - 1) < 1e-12, "ultra-wide inverse");
    qc_destroy(c);
    ++checks;
}

static void channels(void) {
    for (unsigned seed = 0; seed < 16; ++seed) {
        qc_context *dense = context(0, 2, seed, 0);
        qc_context *sparse = context(1, 2, seed, 2);
        qc_context *both[] = {dense, sparse};
        for (unsigned mode = 0; mode < 2; ++mode) {
            require(qc_emit(both[mode], QC_H, 0, qc_mask_zero(), qc_mask_zero(), 0), "Bell H");
            require(qc_emit(both[mode], QC_X, 1, qc_mask_from_u64(1), qc_mask_from_u64(1), 0),
                    "Bell X");
        }
        require(qc_sim_measure(dense, 0) == qc_sim_measure(sparse, 0), "measurement outcome");
        compare(dense, sparse);
        for (unsigned mode = 0; mode < 2; ++mode)
            require(qc_apply(both[mode], "{\"op\":\"qreset\",\"id\":0}", NULL),
                    qc_error(both[mode]));
        compare(dense, sparse);
        require(cabs(qc_amplitude(sparse, 0) - 1) < 1e-12, "explicit reset");
        qc_destroy(dense);
        qc_destroy(sparse);
        ++checks;
    }
    /* SplitMix64 emits zero on its first draw from this seed. A tiny nonzero
     * Born probability must remain selectable; the old dense cutoff erased it. */
    for (int mode = 0; mode < 2; ++mode) {
        qc_context *c = context(mode, 1, UINT64_C(0x61c8864680b583eb), 2);
        require(qc_emit(c, QC_H, 0, qc_mask_zero(), qc_mask_zero(), 0) &&
                    qc_emit(c, QC_PHASE, 0, qc_mask_from_u64(1), qc_mask_from_u64(1), 2e-8) &&
                    qc_emit(c, QC_H, 0, qc_mask_zero(), qc_mask_zero(), 0),
                "tiny-probability preparation");
        require(cabs(qc_amplitude(c, 1)) > 0, "tiny amplitude was pruned");
        require(qc_sim_measure(c, 0) == 1, "tiny Born probability was forced to zero");
        require(fabs(cabs(qc_amplitude(c, 1)) - 1) < 1e-12, "rare collapse normalization");
        qc_destroy(c);
        ++checks;
    }
}

static void budgets(void) {
    require(!qc_create_sparse(7, 0) && !qc_create_sparse(7, QC_SPARSE_MAX_STATES + 1),
            "invalid budget");
    qc_context *c = context(1, 2, 7, 2);
    require(qc_emit(c, QC_H, 0, qc_mask_zero(), qc_mask_zero(), 0) &&
                qc_emit(c, QC_H, 0, qc_mask_zero(), qc_mask_zero(), 0),
            "interference within support cap");
    require(qc_sparse_size(c) == 1, "exact zero paths were retained");
    require(qc_emit(c, QC_H, 0, qc_mask_zero(), qc_mask_zero(), 0), qc_error(c));
    require(!qc_emit(c, QC_H, 1, qc_mask_zero(), qc_mask_zero(), 0) &&
                strstr(qc_error(c), "support exceeds 2"),
            "support exhaustion");
    require(qc_sparse_size(c) == 2 && cabs(qc_amplitude(c, 0) - 1 / sqrt(2)) < 1e-12 &&
                cabs(qc_amplitude(c, 1) - 1 / sqrt(2)) < 1e-12,
            "failed H changed the state");
    require(!qc_emit(c, QC_X, 0, qc_mask_zero(), qc_mask_zero(), 0),
            "failed context accepted another gate");
    qc_destroy(c);
    ++checks;
}

static void native_programs(validation *v) {
    char driver[4096], binary[4096], source[4096], output[4096];
    validation_path(driver, sizeof driver, "%s/build/cket", v->root);
    validation_path(binary, sizeof binary, "%s/program", v->temp);
    validation_path(output, sizeof output, "%s/stdout", v->temp);
    const char *examples[] = {"wide_integers", "wide_size"};
    const char *levels[] = {"-O0", "-O2"};
    for (unsigned profile = 0; profile < 2; ++profile) {
        validation_path(
            source, sizeof source, "%s/examples/blocks/02/%s.c", v->root, examples[profile]);
        for (unsigned level = 0; level < 2; ++level) {
            char *compile[] = {driver, source, (char *)levels[level], "-o", binary, NULL};
            require(!validation_run(v, compile, NULL, 90), "compile wide integer example");
            require(!setenv("CKET_SIMULATOR", "sparse", 1), "select sparse runtime");
            for (unsigned seed = 0; seed < 8; ++seed) {
                char text[32];
                snprintf(text, sizeof text, "%u", seed);
                require(!setenv("CKET_SEED", text, 1), "set seed");
                char *run[] = {binary, NULL};
                require(!validation_run(v, run, output, 90), "execute wide int quotient");
                char *result = validation_read(output);
                unsigned control, tail_value;
                int value, quotient;
                size_t count;
                char tail;
                if (!profile) {
                    require(sscanf(result,
                                   "control=%u value=%d quotient=%d %c",
                                   &control,
                                   &value,
                                   &quotient,
                                   &tail) == 3 &&
                                control <= 1 && value == (control ? -7 : 10) &&
                                quotient == value / 3,
                            "wide native output relation");
                } else {
                    require(sscanf(result,
                                   "control=%u count=%zu tail=%u %c",
                                   &control,
                                   &count,
                                   &tail_value,
                                   &tail) == 3 &&
                                control <= 1 && count == (control ? SIZE_MAX : SIZE_MAX / 2 + 1) &&
                                tail_value == (uint8_t)(count / 3),
                            "wide size_t output relation");
                }
                free(result);
                ++checks;
            }
        }
    }
    const char *modes[] = {"dense", "invalid", "sparse"};
    const char *diagnostics[] = {
        "24 qubits", "CKET_SIMULATOR must be dense or sparse", "support exceeds 1"};
    for (unsigned mode = 0; mode < 3; ++mode) {
        require(!setenv("CKET_SIMULATOR", modes[mode], 1), "runtime mode");
        require(!setenv("CKET_SPARSE_STATES", "1", 1), "runtime budget");
        char *run[] = {binary, NULL};
        int status = validation_run(v, run, output, 90);
        char *log = validation_read(v->log);
        require(validation_rejected(status, log, diagnostics[mode]),
                "runtime resource/configuration diagnostic");
        free(log);
        ++checks;
    }
    require(!unsetenv("CKET_SIMULATOR") && !unsetenv("CKET_SPARSE_STATES") &&
                !unsetenv("CKET_SEED"),
            "clear runtime environment");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_sparse");
    validation v;
    validation_init(&v, argv[0]);
    gate_domains();
    high_wires();
    channels();
    budgets();
    native_programs(&v);
    printf(
        "%u sparse simulation checks passed; phased gates/inverses, 1024 wires, explicit channels, support bounds and native int32/size_t programs.\n",
        checks);
    validation_cleanup(&v);
    return 0;
}
