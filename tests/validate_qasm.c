#include "validation.h"
#include "qasm_reference.h"
#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VALIDATION_SANITIZED
#define VALIDATION_SANITIZED 0
#endif

typedef struct { const char *name, *body; } test_case;
static const test_case states[] = {
    {"entangled arithmetic", "quant uint8_t x=superposition(0,3);quant uint8_t y=x%2;measure(y);measure(x);"},
    {"relative phase and cleanup", "quant uint8_t x=superposition(0,7);if(x==3)phase(.7);measure(x);"},
    {"controlled global phase", "quant bool c=superposition(0,1);if(c)phase(.3);measure(c);"},
    {"open controlled Fourier", "quant bool c=superposition(0,1);quant uint8_t x=1;if(c)phase(.4);else fourier(x);measure(x);measure(c);"},
    {"Fourier inverse interference", "quant uint8_t x=5;fourier(x);if(x==3)phase(.8);inverse_fourier(x);measure(x);"},
    {"Grover complete circuit", "quant uint8_t x=superposition(0,15);quant uint8_t y=find_known(x,x%4==1,4);measure(y);"},
    {"nested open controls", "quant bool a=superposition(0,1),b=superposition(0,1);quant uint8_t x=0;if(a){if(b)x+=1;else x+=2;}else x+=3;measure(x);measure(b);measure(a);"},
    {"independent controlled module", NULL}
};
static const test_case channels[] = {
    {"intermediate entangled measurement", "quant bool a=superposition(0,1),b=false;if(a)b^=true;measure(a);fourier(b);if(b)phase(.4);inverse_fourier(b);measure(b);"},
    {"reset of an entangled input", "quant bool a=superposition(0,1),b=false;if(a)b^=true;reset(a);fourier(b);measure(b);measure(a);"},
    {"measurement then reuse", "quant bool a=superposition(0,1);measure(a);fourier(a);if(a)phase(.8);inverse_fourier(a);measure(a);"}
};
static void check_importer(void) {
    const char *invalid[] = {
        "h q[0];", "OPENQASM 2.0;include \"stdgates.inc\";qubit[1] q;h q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;h q[1];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[2] q;ctrl(1) @ x q[0],q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;p(nan) q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;unknown q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;bit[1] results;results[1]=measure q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;h q[0]"
    };
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        reference_circuit c; if (reference_parse(&c, invalid[i])) { reference_destroy(&c); validation_fail("QASM importer accepted malformed case %u", i); }
    }
    puts("PASS independent QASM importer: 8 malformed artifacts rejected");
}
static void exported(validation *v, const char *body, char *ir, reference_circuit *c) {
    char source[4096], qasm[4096], compiler[4096], backend[4096], program[8192];
    validation_path(source, sizeof source, "%s/program.c", v->temp); validation_path(ir, 4096, "%s/program.json", v->temp);
    validation_path(qasm, sizeof qasm, "%s/program.qasm", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root); validation_path(backend, sizeof backend, "%s/build/cket_backend", v->root);
    validation_path(program, sizeof program, "#include <quantum.h>\nint main(void){%s return 0;}\n", body); validation_write(source, program);
    char *compile[] = {compiler, source, "--emit-ir", "-o", ir, NULL};
    char *export[] = {backend, "--emit-qasm", ir, NULL};
    if (validation_run(v, compile, NULL, 30) || validation_run(v, export, qasm, 30)) {
        char *log = validation_read(v->log); validation_fail("QASM preparation failed: %s", log);
    }
    char *text = validation_read(qasm);
    if (!reference_parse(c, text)) validation_fail("independent QASM import failed: %s", c->error);
    free(text);
}
static void check_state(validation *v, QuESTEnv env, const test_case *test) {
    char ir[4096], module[8192]; const char *body = test->body;
    if (!body) {
        validation_path(module, sizeof module, "quant bool c=superposition(0,1);quant uint8_t x=7;if(c)circuit(\"%s/examples/increment.qir.json\",x);measure(x);measure(c);", v->root);
        body = module;
    }
    reference_circuit c; exported(v, body, ir, &c);
    char *text = validation_read(ir); cj_doc d = {0}; qc_context *native = qc_create(1, 7);
    if (!native || !cj_parse(&d, text) || !qc_verify_document(native, &d)) validation_fail("invalid native reference IR");
    native->ignore_measurements = 1; unsigned mapping[20], bits = 0; int list = cj_get(&d, 0, "instructions");
    for (int t = list + 1; t < d.tokens[list].next; t = d.tokens[t].next) {
        if (cj_equal(&d, cj_get(&d, t, "op"), "qmeasure")) {
            uint64_t id; if (!cj_u64(&d, cj_get(&d, t, "id"), &id)) validation_fail("invalid measured register");
            qc_register *r = qc_lookup(native, (uint32_t)id);
            if (!r || r->width > 20 - bits) validation_fail("measurement mapping exceeds validation budget");
            for (unsigned b = 0; b < r->width; ++b) mapping[bits++] = r->wires[b];
        }
        uint64_t ignored;
        if (!qc_apply_token(native, &d, t, 0, 0, &ignored)) validation_fail("native state reference: %s", qc_error(native));
    }
    if (qc_qubits(native) != c.qubits || bits != c.bits) validation_fail("QASM register dimensions differ: %s", test->name);
    Qureg q = createQureg((int)c.qubits, env); initZeroState(q); (void)reference_execute(&c, q, 0);
    double complex overlap = 0; size_t dimension = qc_state_size(native);
    for (size_t i = 0; i < dimension; ++i) { Complex a = getAmp(q, (long long)i); overlap += conj(qc_amplitude(native, i)) * (a.real + I * a.imag); }
    if (fabs(cabs(overlap) - 1) > 1e-10) validation_fail("independent state overlap failed: %s", test->name);
    overlap /= cabs(overlap);
    size_t outcomes = (size_t)1 << bits; double *expected = calloc(outcomes, sizeof *expected), *actual = calloc(outcomes, sizeof *actual);
    if (!expected || !actual) validation_fail("out of memory comparing measurement probabilities");
    for (size_t i = 0; i < dimension; ++i) {
        Complex a = getAmp(q, (long long)i); double complex n = qc_amplitude(native, i);
        if (cabs(a.real + I * a.imag - n * overlap) > 1e-10) validation_fail("independent amplitude mismatch: %s basis=%zu", test->name, i);
        size_t expected_bits = 0, actual_bits = 0;
        for (unsigned b = 0; b < bits; ++b) expected_bits |= ((i >> mapping[b]) & 1) << b;
        for (size_t g = 0; g < c.count; ++g) if (c.gates[g].kind == REF_MEASURE) actual_bits |= ((i >> c.gates[g].wires[0]) & 1) << c.gates[g].bit;
        expected[expected_bits] += creal(n * conj(n)); actual[actual_bits] += a.real * a.real + a.imag * a.imag;
    }
    for (size_t i = 0; i < outcomes; ++i) if (fabs(expected[i] - actual[i]) > 1e-10) validation_fail("independent final measurement mapping differs: %s outcome=%zu", test->name, i);
    printf("PASS independent QASM state and probabilities: %s (%u qubits)\n", test->name, c.qubits); fflush(stdout);
    free(expected); free(actual); destroyQureg(q, env); reference_destroy(&c); qc_destroy(native); cj_free(&d); free(text);
}
static void check_channel(validation *v, QuESTEnv env, const test_case *test) {
    char ir[4096], sampler[4096], counts_path[4096]; reference_circuit c; exported(v, test->body, ir, &c);
    validation_path(sampler, sizeof sampler, "%s/build/qasm_samples%s", v->root, VALIDATION_SANITIZED ? "-sanitized" : "");
    validation_path(counts_path, sizeof counts_path, "%s/counts.json", v->temp);
    char *command[] = {sampler, ir, "8192", NULL};
    if (validation_run(v, command, counts_path, 60)) { char *log = validation_read(v->log); validation_fail("native channel sampling failed: %s", log); }
    char *text = validation_read(counts_path); cj_doc d = {0}; if (!cj_parse(&d, text)) validation_fail("invalid native transcript counts");
    size_t outcomes = (size_t)1 << c.bits; unsigned *native = calloc(outcomes, sizeof *native), *actual = calloc(outcomes, sizeof *actual);
    if (!native || !actual) validation_fail("out of memory comparing channel transcripts");
    unsigned total = 0;
    for (int key = 1; key < d.tokens[0].next; key = d.tokens[key + 1].next) {
        char *s = cj_copy(&d, key); char *end; uint64_t count;
        if (!s || strlen(s) < 3 || s[0] != '"' || s[strlen(s) - 1] != '"') validation_fail("invalid transcript key");
        s[strlen(s) - 1] = 0; unsigned long value = strtoul(s + 1, &end, 10);
        if (end == s + 1 || *end || value >= outcomes || !cj_u64(&d, key + 1, &count) || count > 8192) validation_fail("invalid native transcript bucket");
        native[value] = (unsigned)count; total += (unsigned)count; free(s);
    }
    if (total != 8192) validation_fail("incorrect native transcript sample count");
    Qureg q = createQureg((int)c.qubits, env);
    for (unsigned shot = 0; shot < 8192; ++shot) { initZeroState(q); ++actual[reference_execute(&c, q, 1)]; }
    double tolerance = 2 * sqrt(log(4.0 * 3 * outcomes / 1e-6) / (2 * 8192));
    for (size_t i = 0; i < outcomes; ++i)
        if (fabs((double)native[i] / 8192 - (double)actual[i] / 8192) >= tolerance) validation_fail("independent channel mismatch: %s outcome=%zu", test->name, i);
    printf("PASS independent QASM channel: %s (%u qubits; 8192 transcripts)\n", test->name, c.qubits); fflush(stdout);
    destroyQureg(q, env); free(native); free(actual); cj_free(&d); free(text); reference_destroy(&c);
}
int main(int argc, char **argv) {
    if (argc != 1) validation_fail("usage: validate_qasm");
    check_importer(); validation v; validation_init(&v, argv[0]); QuESTEnv env = createQuESTEnv();
    unsigned long seeds[] = {913}; seedQuEST(&env, seeds, 1);
    for (unsigned i = 0; i < sizeof states / sizeof *states; ++i) check_state(&v, env, &states[i]);
    for (unsigned i = 0; i < sizeof channels / sizeof *channels; ++i) check_channel(&v, env, &channels[i]);
    destroyQuESTEnv(env); validation_cleanup(&v); puts("11 exported circuits independently validated with QuEST 3.7.0 (C)"); return 0;
}
