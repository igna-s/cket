#include "validation.h"
#include "select_cpu_trace.inc"
#include "qasm_reference.h"
#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VALIDATION_SANITIZED
#define VALIDATION_SANITIZED 0
#endif

typedef struct {
    const char *name, *body;
} test_case;

static const test_case
    states
        [] =
            {{"entangled arithmetic",
              "quant uint8_t x=superposition(0,3);quant uint8_t y=x%2;measure(y);measure(x);"},
             {"relative phase and cleanup",
              "quant uint8_t x=superposition(0,7);if(x==3)phase(.7);measure(x);"},
             {"controlled global phase",
              "quant bool c=superposition(0,1);if(c)phase(.3);measure(c);"},
             {"open controlled Fourier",
              "quant bool c=superposition(0,1);quant uint8_t x=1;if(c)phase(.4);else fourier(x);measure(x);measure(c);"},
             {"Fourier inverse interference",
              "quant uint8_t x=5;fourier(x);if(x==3)phase(.8);inverse_fourier(x);measure(x);"},
             {"Grover complete circuit",
              "quant uint8_t x=superposition(0,15);quant uint8_t y=find_known(x,x%4==1,4);measure(y);"},
             {"nested open controls",
              "quant bool a=superposition(0,1),b=superposition(0,1);quant uint8_t x=0;if(a){if(b)x+=1;else x+=2;}else x+=3;measure(x);measure(b);measure(a);"},
             {"independent controlled module", NULL},
             {"signed absolute value with relative phase",
              "quant bool x=superposition(0,1);quant uint8_t y=abs((int)x-1);if(x)phase(.37);measure(y);measure(x);"},
             {"function, array, static loop and coherent phase",
              "#include <quantum.h>\nquant bool relation(quant const bool a,quant const bool b){return a^b;}\nint main(void){quant bool bits[2]={superposition(0,1),superposition(0,1)};for(int i=0;i<2;++i)bits[i]^=true;quant bool r=relation(bits[0],bits[1]);if(r)phase(.23);measure(r);measure(bits[1]);measure(bits[0]);return 0;}\n"},
             {"quantum variable left shift and relative phase",
              "quant bool x=superposition(0,1),n=superposition(0,1);quant uint8_t y=(unsigned)x<<n;if(n)phase(.29);measure(y);measure(n);measure(x);"},
             {"signed variable right shift and Boolean conversion",
              "quant bool x=superposition(0,1),n=superposition(0,1);quant bool y=(3*(int)x-2)>>n;if(n)phase(.41);measure(y);measure(n);measure(x);"},
             {"conditional expression with guarded division",
              "quant bool x=superposition(0,1),y=superposition(0,1);quant uint8_t r=x?3/x:y;if(y)phase(.17);measure(r);measure(y);measure(x);"},
             {"short-circuit logic with guarded division",
              "quant bool x=superposition(0,1),y=superposition(0,1);quant bool r=(!x||(1/x))&&y;if(x)phase(.53);measure(r);measure(y);measure(x);"},
             {"conditional signed negation and complement",
              "quant bool x=superposition(0,1),y=superposition(0,1);quant uint8_t r=x?-((int)x+1):~(int)y;if(y)phase(.31);measure(r);measure(y);measure(x);"},
             {"controlled affine in-place arithmetic",
              "quant bool c=superposition(0,1);quant uint8_t x=superposition(0,7);if(x==3)phase(.37);if(c)x=x*3+5;measure(x);measure(c);"},
             {"in-place rotation and complement",
              "quant uint8_t x=superposition(0,255);if(x==3)phase(.43);x=(uint8_t)((x<<3)|(x>>5));x=~x;measure(x);"},
             {"signed affine update after Fourier",
              "quant bool c=superposition(0,1);quant int8_t x=-7;fourier(x);x=(int)x*-3+7;if(c)x=-((int)x);measure(x);measure(c);"},
             {"coherent library memcpy with relative phase",
              "#include <quantum.h>\n#include <string.h>\nvoid copy_word(quant uint8_t *d,quant const uint8_t *s){memcpy((void*)d,(const void*)s,1);}\nint main(void){quant uint8_t a=superposition(0,7),b=37;quant bool c=superposition(0,1);if(a==3)phase(.37);if(c)copy_word(&b,&a);measure(a);measure(b);measure(c);return 0;}\n"},
             {"library memmove preserves signed object bits",
              "#include <quantum.h>\n#include <string.h>\nint main(void){quant int8_t a=0;quant uint8_t b=129;fourier(a);if(a==3)phase(.29);memmove((void*)&b,(const void*)&a,1);measure(a);measure(b);return 0;}\n"},
             {"library memmove exact alias preserves interference",
              "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t a=7;fourier(a);if(a==3)phase(.43);memmove((void*)&a,(const void*)&a,1);inverse_fourier(a);measure(a);return 0;}\n"},
             {"library copy survives coherent source uncomputation",
              "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool a=superposition(0,1);if(a)phase(.31);quant uint8_t source=a+1,destination=0;memcpy((void*)&destination,(const void*)&source,1);measure(destination);measure(a);return 0;}\n"},
             {"library division product projections with relative phase",
              "quant bool n=superposition(0,1);if(n)phase(.31);quant uint8_t q=div((int)n+5,2).quot;quant uint8_t r=div((int)n+5,2).rem;measure(q);measure(r);measure(n);"},
             {"coherent library division with variable denominator", "#include <quantum.h>\n#include <inttypes.h>\nint main(void){quant bool n=superposition(0,1),d=superposition(0,1),c=superposition(0,1);if(d)phase(.47);quant bool q=c?imaxdiv((intmax_t)n,(intmax_t)d+1).quot:false;quant bool r=c?imaxdiv((intmax_t)n,(intmax_t)d+1).rem:false;measure(q);measure(r);measure(n);measure(d);measure(c);return 0;}\n"},
             {"signed library division rounds toward zero",
              "quant bool n=superposition(0,1);if(n)phase(.37);quant bool q=ldiv(3*(long)n-1,2).quot;quant int8_t r=ldiv(3*(long)n-1,2).rem;measure(q);measure(r);measure(n);"},
             {"CPU-bound library division",
              "quant bool n=superposition(0,1);if(n)phase(.31);int d=2;quant uint8_t q=div((int)n+5,d).quot;quant uint8_t r=div((int)n+5,d).rem;measure(q);measure(r);measure(n);"},
             {"saved CPU snapshot inverse",
              "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t y=n+a;++a;if(y==6)phase(.31);measure(n);"},
             {"derived history cleanup",
              "quant bool n=superposition(0,1);quant bool x=!n;if(n)phase(.31);x+=1;if(x)phase(.17);measure(x);measure(n);"},
             {"retained byte history preserves relative phases",
              "quant uint8_t x=superposition(0,3);if(x&2)phase(.19);x&=1;if(x&1)phase(.31);measure(x);"},
             {"history CPU snapshot inverse",
              "quant bool n=superposition(0,1);quant bool x=!n;int d=1;x/=d;++d;if(x)phase(.31);measure(n);return d!=2;"},
             {"coherent two-arm history join",
              "quant bool c=superposition(0,1),x=superposition(0,1);if(x)phase(.19);if(c){x=false;phase(.23);}else{x=!x;phase(.41);}if(x)phase(.31);measure(x);measure(c);"},
             {"self-controlled information loss",
              "quant bool x=superposition(0,1);if(x)phase(.19);if(x)x=false;measure(x);"},
             {"nested coherent history joins",
              "quant bool c=superposition(0,1),x=superposition(0,1);if(x)phase(.19);if(c){if(x)x=false;else x=true;}if(x)phase(.31);measure(x);measure(c);"},
             {"coherent CPU predicate snapshot inverse",
              "quant bool c=superposition(0,1),x=superposition(0,1);if(x)phase(.19);int d=1;if(c<d)x+=1;d=0;if(x)phase(.31);measure(x);measure(c);"},
             {"CPU two-arm assignment trace",
              "volatile int flag=1;quant bool c=superposition(0,1),x=superposition(0,1);if(x)phase(.19);if(flag)x=false;else x=true;if(x)phase(.31);measure(x);measure(c);"},
             {"CPU condition snapshot trace",
              "int d=1;quant bool n=superposition(0,1),x=!n;if(x)phase(.19);if(d){x+=1;d=0;}else x=false;if(x)phase(.31);measure(x);measure(n);"},
             {"CPU coherent branch trace",
              "volatile int flag=1;quant bool c=superposition(0,1),x=superposition(0,1);if(x)phase(.19);if(flag){if(c)x=false;else x=true;}else x=!x;if(x)phase(.31);measure(x);measure(c);"}};
static const test_case channels[] = {
    {"intermediate entangled measurement",
     "quant bool a=superposition(0,1),b=false;if(a)b^=true;measure(a);fourier(b);if(b)phase(.4);inverse_fourier(b);measure(b);"},
    {"reset of an entangled input",
     "quant bool a=superposition(0,1),b=false;if(a)b^=true;reset(a);fourier(b);measure(b);measure(a);"},
    {"measurement then reuse",
     "quant bool a=superposition(0,1);measure(a);fourier(a);if(a)phase(.8);inverse_fourier(a);measure(a);"}};

static void check_importer(void) {
    const char *invalid[] = {
        "h q[0];",
        "OPENQASM 2.0;include \"stdgates.inc\";qubit[1] q;h q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;h q[1];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[2] q;ctrl(1) @ x q[0],q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;p(nan) q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;unknown q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;bit[1] results;results[1]=measure q[0];",
        "OPENQASM 3.0;include \"stdgates.inc\";qubit[1] q;h q[0]"};
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        reference_circuit c;
        if (reference_parse(&c, invalid[i])) {
            reference_destroy(&c);
            validation_fail("QASM importer accepted malformed case %u", i);
        }
    }
    puts("PASS independent QASM importer: 8 malformed artifacts rejected");
}

static void exported(validation *v,
                     const char *body,
                     char *ir,
                     reference_circuit *c,
                     unsigned binding_value,
                     const unsigned *cpu_decisions,
                     size_t cpu_count) {
    char source[4096], qasm[4096], compiler[4096], backend[4096], program[8192];
    validation_path(source, sizeof source, "%s/program.c", v->temp);
    validation_path(ir, 4096, "%s/program.json", v->temp);
    validation_path(qasm, sizeof qasm, "%s/program.qasm", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v->root);
    if (*body == '#')
        validation_write(source, body);
    else {
        validation_path(program,
                        sizeof program,
                        "#include <quantum.h>\n#include <stdlib.h>\nint main(void){%s return 0;}\n",
                        body);
        validation_write(source, program);
    }
    char *compile[] = {compiler, source, "--emit-ir", "-o", ir, NULL};
    char *export[] = {backend, "--emit-qasm", ir, "--no-tables", NULL};
    if (validation_run(v, compile, NULL, 30)) {
        char *log = validation_read(v->log);
        validation_fail("QASM preparation failed: %s", log);
    }
    if (binding_value) {
        char *template = validation_read(ir);
        cj_doc d = {0};
        if (!cj_parse(&d, template))
            validation_fail("invalid binding fixture IR");
        char rows[4096] = "[";
        size_t length = 1;
        unsigned index = 0, count = 0;
        int list = cj_get(&d, 0, "instructions");
        for (int token = list + 1; token < d.tokens[list].next;
             token = d.tokens[token].next, ++index) {
            if (!cj_equal(&d, cj_get(&d, token, "op"), "qbind"))
                continue;
            int parameters = cj_get(&d, token, "parameters");
            if (d.tokens[parameters].children != 1)
                validation_fail("unexpected QASM fixture parameter count");
            int written = snprintf(rows + length,
                                   sizeof rows - length,
                                   "%s{\"instruction\":%u,\"values\":[%u]}",
                                   count++ ? "," : "",
                                   index,
                                   binding_value);
            if (written < 0 || (size_t)written >= sizeof rows - length)
                validation_fail("QASM binding fixture budget exceeded");
            length += (size_t)written;
        }
        if (!count)
            validation_fail("CPU binding QASM fixture emitted no templates");
        rows[length++] = ']';
        rows[length] = 0;
        qc_context *context = qc_create(0, 7);
        char *bound = context ? qc_bind_document(context, template, rows) : NULL;
        if (!bound)
            validation_fail("QASM fixture binding failed: %s", qc_error(context));
        validation_write(ir, bound);
        free(bound);
        qc_destroy(context);
        cj_free(&d);
        free(template);
    }
    if (cpu_count) {
        char *template = validation_read(ir);
        char *trace = select_cpu_trace(template, cpu_decisions, cpu_count);
        validation_write(ir, trace);
        free(trace);
        free(template);
    }
    if (validation_run(v, export, qasm, 30)) {
        char *log = validation_read(v->log);
        validation_fail("QASM export failed: %s", log);
    }
    char *text = validation_read(qasm);
    if (!reference_parse(c, text))
        validation_fail("independent QASM import failed: %s", c->error);
    free(text);
}

static void check_state(validation *v, QuESTEnv env, const test_case *test) {
    char ir[4096], module[8192];
    const char *body = test->body;
    if (!body) {
        validation_path(
            module,
            sizeof module,
            "quant bool c=superposition(0,1);quant uint8_t x=7;if(c)circuit(\"%s/examples/increment.qir.json\",x);measure(x);measure(c);",
            v->root);
        body = module;
    }
    reference_circuit c;
    unsigned cpu_count = !strcmp(test->name, "CPU two-arm assignment trace") ||
                         !strcmp(test->name, "CPU condition snapshot trace") ||
                         !strcmp(test->name, "CPU coherent branch trace");
    unsigned binding_value = !strcmp(test->name, "CPU-bound library division")                ? 2
                             : !strcmp(test->name, "saved CPU snapshot inverse")              ? 5
                             : !strcmp(test->name, "history CPU snapshot inverse")            ? 1
                             : !strcmp(test->name, "coherent CPU predicate snapshot inverse") ? 1
                             : cpu_count                                                      ? 1
                                                                                              : 0;
    unsigned cpu_decisions[] = {1};
    exported(v, body, ir, &c, binding_value, cpu_decisions, cpu_count);
    char *text = validation_read(ir);
    cj_doc d = {0};
    qc_context *native = qc_create(1, 7);
    if (!native || !cj_parse(&d, text) || !qc_verify_document(native, &d))
        validation_fail("invalid native reference IR");
    qc_allow_tables(native, 0);
    native->ignore_measurements = 1;
    unsigned mapping[20], bits = 0;
    int list = cj_get(&d, 0, "instructions");
    for (int t = list + 1; t < d.tokens[list].next; t = d.tokens[t].next) {
        if (cj_equal(&d, cj_get(&d, t, "op"), "qmeasure")) {
            uint64_t id;
            if (!cj_u64(&d, cj_get(&d, t, "id"), &id))
                validation_fail("invalid measured register");
            qc_register *r = qc_lookup(native, (uint32_t)id);
            if (!r || r->width > 20 - bits)
                validation_fail("measurement mapping exceeds validation budget");
            for (unsigned b = 0; b < r->width; ++b)
                mapping[bits++] = r->wires[b];
        }
        uint64_t ignored;
        if (!qc_apply_token(native, &d, t, 0, 0, &ignored))
            validation_fail("native state reference: %s", qc_error(native));
    }
    if (qc_qubits(native) != c.qubits || bits != c.bits)
        validation_fail("QASM register dimensions differ: %s", test->name);
    Qureg q = createQureg((int)c.qubits, env);
    initZeroState(q);
    (void)reference_execute(&c, q, 0);
    double complex overlap = 0;
    size_t dimension = qc_state_size(native);
    for (size_t i = 0; i < dimension; ++i) {
        Complex a = getAmp(q, (long long)i);
        overlap += conj(qc_amplitude(native, i)) * (a.real + I * a.imag);
    }
    if (fabs(cabs(overlap) - 1) > 1e-10)
        validation_fail("independent state overlap failed: %s", test->name);
    overlap /= cabs(overlap);
    size_t outcomes = (size_t)1 << bits;
    double *expected = calloc(outcomes, sizeof *expected),
           *actual = calloc(outcomes, sizeof *actual);
    if (!expected || !actual)
        validation_fail("out of memory comparing measurement probabilities");
    for (size_t i = 0; i < dimension; ++i) {
        Complex a = getAmp(q, (long long)i);
        double complex n = qc_amplitude(native, i);
        if (cabs(a.real + I * a.imag - n * overlap) > 1e-10)
            validation_fail("independent amplitude mismatch: %s basis=%zu", test->name, i);
        size_t expected_bits = 0, actual_bits = 0;
        for (unsigned b = 0; b < bits; ++b)
            expected_bits |= ((i >> mapping[b]) & 1) << b;
        for (size_t g = 0; g < c.count; ++g)
            if (c.gates[g].kind == REF_MEASURE)
                actual_bits |= ((i >> c.gates[g].wires[0]) & 1) << c.gates[g].bit;
        expected[expected_bits] += creal(n * conj(n));
        actual[actual_bits] += a.real * a.real + a.imag * a.imag;
    }
    for (size_t i = 0; i < outcomes; ++i)
        if (fabs(expected[i] - actual[i]) > 1e-10)
            validation_fail(
                "independent final measurement mapping differs: %s outcome=%zu", test->name, i);
    printf("PASS independent QASM state and probabilities: %s (%u qubits)\n", test->name, c.qubits);
    fflush(stdout);
    free(expected);
    free(actual);
    destroyQureg(q, env);
    reference_destroy(&c);
    qc_destroy(native);
    cj_free(&d);
    free(text);
}

static void check_channel(validation *v, QuESTEnv env, const test_case *test) {
    char ir[4096], sampler[4096], counts_path[4096];
    reference_circuit c;
    exported(v, test->body, ir, &c, 0, NULL, 0);
    validation_path(sampler,
                    sizeof sampler,
                    "%s/build/qasm_samples%s",
                    v->root,
                    VALIDATION_SANITIZED ? "-sanitized" : "");
    validation_path(counts_path, sizeof counts_path, "%s/counts.json", v->temp);
    char *command[] = {sampler, ir, "8192", NULL};
    if (validation_run(v, command, counts_path, 60)) {
        char *log = validation_read(v->log);
        validation_fail("native channel sampling failed: %s", log);
    }
    char *text = validation_read(counts_path);
    cj_doc d = {0};
    if (!cj_parse(&d, text))
        validation_fail("invalid native transcript counts");
    size_t outcomes = (size_t)1 << c.bits;
    unsigned *native = calloc(outcomes, sizeof *native), *actual = calloc(outcomes, sizeof *actual);
    if (!native || !actual)
        validation_fail("out of memory comparing channel transcripts");
    unsigned total = 0;
    for (int key = 1; key < d.tokens[0].next; key = d.tokens[key + 1].next) {
        char *s = cj_copy(&d, key);
        char *end;
        uint64_t count;
        if (!s || strlen(s) < 3 || s[0] != '"' || s[strlen(s) - 1] != '"')
            validation_fail("invalid transcript key");
        s[strlen(s) - 1] = 0;
        unsigned long value = strtoul(s + 1, &end, 10);
        if (end == s + 1 || *end || value >= outcomes || !cj_u64(&d, key + 1, &count) ||
            count > 8192)
            validation_fail("invalid native transcript bucket");
        native[value] = (unsigned)count;
        total += (unsigned)count;
        free(s);
    }
    if (total != 8192)
        validation_fail("incorrect native transcript sample count");
    Qureg q = createQureg((int)c.qubits, env);
    for (unsigned shot = 0; shot < 8192; ++shot) {
        initZeroState(q);
        ++actual[reference_execute(&c, q, 1)];
    }
    double tolerance = 2 * sqrt(log(4.0 * 3 * outcomes / 1e-6) / (2 * 8192));
    for (size_t i = 0; i < outcomes; ++i)
        if (fabs((double)native[i] / 8192 - (double)actual[i] / 8192) >= tolerance)
            validation_fail("independent channel mismatch: %s outcome=%zu", test->name, i);
    printf(
        "PASS independent QASM channel: %s (%u qubits; 8192 transcripts)\n", test->name, c.qubits);
    fflush(stdout);
    destroyQureg(q, env);
    free(native);
    free(actual);
    cj_free(&d);
    free(text);
    reference_destroy(&c);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_qasm");
    check_importer();
    validation v;
    validation_init(&v, argv[0]);
    QuESTEnv env = createQuESTEnv();
    unsigned long seeds[] = {913};
    seedQuEST(&env, seeds, 1);
    for (unsigned i = 0; i < sizeof states / sizeof *states; ++i)
        check_state(&v, env, &states[i]);
    for (unsigned i = 0; i < sizeof channels / sizeof *channels; ++i)
        check_channel(&v, env, &channels[i]);
    destroyQuESTEnv(env);
    validation_cleanup(&v);
    printf(
        "%zu exported circuits independently validated with QuEST 3.7.0 (C): %zu states, %zu channels; table-free export\n",
        sizeof states / sizeof *states + sizeof channels / sizeof *channels,
        sizeof states / sizeof *states,
        sizeof channels / sizeof *channels);
    return 0;
}
