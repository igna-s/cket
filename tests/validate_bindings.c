#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};

static void apply(qc_context *c, const char *text) {
    if (!qc_apply(c, text, NULL))
        validation_fail("binding fixture failed: %s", qc_error(c));
}

static void states(void) {
    const char *ops[] = {"add", "mul", "div", "mod", "shl", "shr", "bxor", "gt"};
    for (unsigned op = 0; op < 8; ++op)
        for (unsigned value = 1; value <= 3; ++value)
            for (unsigned control = 0; control < 2; ++control)
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("binding allocation failed");
                    qc_allow_tables(c, tables);
                    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}");
                    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":4,\"signed\":false}");
                    apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":2,\"lower\":0,\"upper\":1}");
                    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .31) ||
                        !qc_emit(c, QC_PHASE, 0, QC_WIRE(2), QC_WIRE(2), .47))
                        validation_fail("binding phase preparation failed");
                    char instruction[3072];
                    validation_path(
                        instruction,
                        sizeof instruction,
                        "{\"op\":\"qbind\",\"parameters\":[{\"id\":17,\"bits\":16,\"signed\":false}],"
                        "\"instruction\":{\"op\":\"qcompute\",\"id\":1,\"controls\":[{\"id\":2,\"value\":%u}],"
                        "\"expr\":{\"op\":\"cast\",\"bits\":4,\"signed\":false,\"args\":[{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"args\":["
                        "{\"op\":\"cast\",\"bits\":16,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}]},"
                        "{\"op\":\"parameter\",\"id\":17,\"bits\":16,\"signed\":false}]}]}}}",
                        control,
                        ops[op],
                        op == 7 ? 1 : 16);
                    uint64_t binding = value;
                    if (!qc_apply_bound(c, instruction, &binding, 1, NULL))
                        validation_fail("bound arithmetic failed: %s", qc_error(c));
                    for (unsigned inverse = 0; inverse < 2; ++inverse) {
                        for (size_t i = 0; i < qc_state_size(c); ++i) {
                            unsigned x = (unsigned)i & 7, flag = (unsigned)(i >> 7) & 1, reference;
                            switch (op) {
                            case 0:
                                reference = x + value;
                                break;
                            case 1:
                                reference = x * value;
                                break;
                            case 2:
                                reference = x / value;
                                break;
                            case 3:
                                reference = x % value;
                                break;
                            case 4:
                                reference = x << value;
                                break;
                            case 5:
                                reference = x >> value;
                                break;
                            case 6:
                                reference = x ^ value;
                                break;
                            default:
                                reference = x > value;
                                break;
                            }
                            unsigned result = !inverse && flag == control ? reference & 15 : 0;
                            double complex expected = 0;
                            if (((i >> 3) & 15) == result && !(i >> 8))
                                expected = cexp(I * ((x & 1 ? .31 : 0) + (x & 4 ? .47 : 0))) / 4;
                            if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                                validation_fail(
                                    "binding state op=%s value=%u control=%u tables=%u inverse=%u basis=%zu",
                                    ops[op],
                                    value,
                                    control,
                                    tables,
                                    inverse,
                                    i);
                        }
                        if (!inverse) {
                            ++binding;
                            validation_path(
                                instruction,
                                sizeof instruction,
                                "{\"op\":\"quncompute\",\"id\":1,\"saved\":true,\"controls\":[{\"id\":2,\"value\":%u}]}",
                                control);
                            apply(c, instruction);
                        }
                    }
                    if (c->measure || c->reset)
                        validation_fail("implicit binding channel");
                    qc_destroy(c);
                    ++checks;
                }
    puts(
        "PASS 96 bound arithmetic domains: full phases, controls, table policies and saved inverse");
    fflush(stdout);
}

static void endpoints(void) {
    const uint64_t values[] = {0, 1, UINT64_MAX, UINT64_C(1) << 63};
    for (unsigned bits = 1; bits <= 64; bits *= 2)
        for (unsigned sign = 0; sign <= (bits != 1); ++sign)
            for (unsigned v = 0; v < 4; ++v) {
                qc_context *c = qc_create(1, 7);
                if (!c)
                    validation_fail("endpoint allocation failed");
                apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}");
                char instruction[1024];
                validation_path(
                    instruction,
                    sizeof instruction,
                    "{\"op\":\"qbind\",\"parameters\":[{\"id\":0,\"bits\":%u,\"signed\":%s}],"
                    "\"instruction\":{\"op\":\"qcompute\",\"id\":0,\"expr\":{\"op\":\"cast\",\"bits\":8,\"signed\":false,"
                    "\"args\":[{\"op\":\"parameter\",\"id\":0,\"bits\":%u,\"signed\":%s}]}}}",
                    bits,
                    sign ? "true" : "false",
                    bits,
                    sign ? "true" : "false");
                if (!qc_apply_bound(c, instruction, &values[v], 1, NULL))
                    validation_fail("bound endpoint failed: %s", qc_error(c));
                uint64_t mask = bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
                uint64_t raw = bits == 1 ? values[v] != 0 : values[v] & mask;
                if (sign && (raw & (UINT64_C(1) << (bits - 1))))
                    raw |= ~mask;
                unsigned expected = (unsigned)raw & 255;
                for (size_t i = 0; i < qc_state_size(c); ++i)
                    if (cabs(qc_amplitude(c, i) - (i == expected ? 1 : 0)) > 1e-10)
                        validation_fail("CPU binding integer representation mismatch");
                qc_destroy(c);
                ++checks;
            }
    puts("PASS 52 typed binding endpoints including signed minimum and unsigned maximum");
}

static void rejected(void) {
    const char *descriptors[] = {
        "[{\"id\":0,\"bits\":8,\"signed\":true}]",
        "[{\"id\":1,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"bits\":8,\"signed\":false},{\"id\":0,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"bits\":0,\"signed\":false}]",
        "[{\"id\":0,\"bits\":65,\"signed\":false}]",
        "[{\"id\":0,\"bits\":1,\"signed\":true}]",
        "[{\"id\":-1,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"id\":0,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"bits\":8,\"signed\":false,\"extra\":1}]",
        "[]"};
    for (unsigned i = 0; i < sizeof descriptors / sizeof *descriptors; ++i) {
        qc_context *c = qc_create(1, 7);
        if (!c)
            validation_fail("negative binding allocation failed");
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}");
        char instruction[1200];
        validation_path(
            instruction,
            sizeof instruction,
            "{\"op\":\"qbind\",\"parameters\":%s,\"instruction\":{\"op\":\"qcompute\",\"id\":0,"
            "\"expr\":{\"op\":\"parameter\",\"id\":0,\"bits\":8,\"signed\":false}}}",
            descriptors[i]);
        uint64_t values[2] = {7, 8};
        if (qc_apply_bound(c, instruction, values, i == 2 ? 2 : 1, NULL) || c->ngates ||
            cabs(qc_amplitude(c, 0) - 1) > 1e-10)
            validation_fail("invalid CPU binding changed quantum state");
        qc_destroy(c);
        ++checks;
    }
    puts("PASS 10 malformed CPU descriptor cases reject before state changes");
}

static void compile_run(
    validation *v, const char *body, unsigned level, unsigned builtins, const char *diagnostic) {
    char source[4096], executable[4096], compiler[4096], program[8192];
    validation_path(source, sizeof source, "%s/bindings.c", v->temp);
    validation_path(executable, sizeof executable, "%s/bindings", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    if (*body == '#')
        validation_write(source, body);
    else {
        validation_path(
            program,
            sizeof program,
            "#include <quantum.h>\n#include <stdlib.h>\n#include <inttypes.h>\nint main(void){%s}\n",
            body);
        validation_write(source, program);
    }
    char *command[] = {compiler,
                       source,
                       "--run",
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       executable,
                       NULL};
    int status = validation_run(v, command, NULL, 60);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail(
            "binding C case failed %s builtins=%u: %s\n%s", levels[level], builtins, body, log);
    free(log);
    ++checks;
}

static void document_contracts(void) {
    const char *ir =
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
        "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false},"
        "{\"op\":\"qbind\",\"parameters\":[{\"id\":3,\"bits\":8,\"signed\":false}],"
        "\"instruction\":{\"op\":\"qprepare\",\"id\":0,\"expr\":{\"op\":\"parameter\",\"id\":3,\"bits\":8,\"signed\":false}}},"
        "{\"op\":\"qmeasure\",\"id\":0}]}";
    const char *bad[] = {"[]",
                         "[{\"instruction\":2,\"values\":[7]}]",
                         "[{\"instruction\":-1,\"values\":[7]}]",
                         "[{\"instruction\":1,\"values\":[]}]",
                         "[{\"instruction\":1,\"values\":[7,8]}]",
                         "[{\"instruction\":1,\"values\":[true]}]",
                         "[{\"instruction\":1,\"values\":[\"7\"]}]",
                         "[{\"instruction\":1,\"values\":[7],\"extra\":0}]",
                         "[{\"instruction\":1}]",
                         "[{\"instruction\":1,\"values\":[7]},{\"instruction\":1,\"values\":[7]}]"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) {
        qc_context *c = qc_create(1, 7);
        if (!c)
            validation_fail("document contract allocation failed");
        char *bound = qc_bind_document(c, ir, bad[i]);
        if (bound || qc_qubits(c) || c->ngates)
            validation_fail("invalid binding rows changed circuit state");
        free(bound);
        qc_destroy(c);
        ++checks;
    }
    qc_context *c = qc_create(1, 7);
    char *bound = c ? qc_bind_document(c, ir, "[{\"instruction\":1,\"values\":[-7]}]") : NULL;
    if (!bound || !qc_document(c, bound, 0, 1) || c->reset || c->measure != 8)
        validation_fail("bound classical basis preparation failed: %s", qc_error(c));
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - (i == 249 ? 1 : 0)) > 1e-10)
            validation_fail("bound preparation did not preserve the converted C value");
    free(bound);
    qc_destroy(c);
    ++checks;
    puts("PASS 11 whole-document binding/preparation contracts before circuit mutation");
}

static void programs(validation *v) {
    const char *valid[] = {
        "quant bool n=superposition(0,1);int d=2;quant uint8_t q=div((int)n+5,d++).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2||d!=3;",
        "quant bool n=superposition(0,1);long d=2;quant uint8_t q=ldiv((long)n+5,d++).rem;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)%2||d!=3;",
        "quant bool n=superposition(0,1);long long d=2;quant uint8_t q=lldiv((long long)n+5,d++).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2||d!=3;",
        "quant bool n=superposition(0,1);intmax_t d=2;quant uint8_t q=imaxdiv((intmax_t)n+5,d++).rem;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)%2||d!=3;",
        "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t y=n+a;++a;if(y==6)phase(.31);measure(n);return a!=6;",
        "quant bool n=superposition(0,1);int d=2;quant bool q=n?div((int)n,d).rem:false;unsigned qq=measure(q),nn=measure(n);return qq!=nn;",
        "quant bool n=false;int d=0;quant bool q=n?1/d:false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=false;int d=32;quant bool q=n?(1<<d):false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=superposition(0,1);int64_t a=INT64_MIN;quant uint8_t q=(uint64_t)((int64_t)n+a);unsigned qq=measure(q),nn=measure(n);return qq!=nn;",
        "struct{int a;} p={5};quant bool n=superposition(0,1);int d=2;quant uint8_t q=div((int)n+p.a,d).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2;",
        "#include <quantum.h>\n#include <stdlib.h>\nstatic int calls;static int denominator(void){++calls;return 2;}\nint main(void){quant bool n=superposition(0,1);quant uint8_t q=div((int)n+5,denominator()).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2||calls!=1;}\n",
        "#include <quantum.h>\ntypedef struct{bool a,b;} pair;static int calls;static pair make(unsigned n){++calls;return(pair){n&1,!(n&1)};}\nint main(void){quant pair p[2]={make(1),make(2)};unsigned a=measure(p[0].a),b=measure(p[0].b),c=measure(p[1].a),d=measure(p[1].b);return calls!=2||a!=1||b||c||d!=1;}\n",
        "#include <quantum.h>\ntypedef struct{uint8_t a,b;} pair;static int calls;static pair make(unsigned n){++calls;return(pair){n,n+1};}\nint main(void){quant pair p=make(7);unsigned a=measure(p.a),b=measure(p.b);return a!=7||b!=8||calls!=1;}\n",
        "#include <quantum.h>\n#include <stdlib.h>\nstatic int calls;static int denominator(int *d){++calls;return (*d)++;}\nint main(void){quant bool n=superposition(0,1);int d=1;unsigned values[2];for(unsigned k=0;k<2;++k){quant uint8_t q=div((int)n+7,denominator(&d)).quot;values[k]=measure(q);}unsigned value=measure(n);if(calls!=2||d!=3)return 1;for(unsigned k=0;k<2;++k)if(values[k]!=(value+7)/(k+1))return 2;return 0;}\n"};
    const char *invalid[] = {
        "quant bool n=superposition(0,1);int d=0;quant bool q=div((int)n,d).quot;measure(q);measure(n);",
        "quant bool n=superposition(0,1);int d=2;quant bool q=n?div((int)n,d++).quot:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t q=n+a++;measure(q);measure(n);",
        "quant bool n=true;int d=0;quant bool q=n?1/d:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);int d=2;int q=div((int)n,d).quot;measure(n);return q;"};
    const char *diagnostics[] = {
        "zero", "coherent control", "once-only capture", "zero", "classical initialization"};
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtins = 0; builtins < 2; ++builtins) {
            for (unsigned i = 0; i < sizeof valid / sizeof *valid; ++i)
                compile_run(v, valid[i], level, builtins, NULL);
            for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i)
                compile_run(v, invalid[i], level, builtins, diagnostics[i]);
        }
    puts(
        "PASS 190 C cases: CPU capture, old-value cleanup, lazy sanitizer guards, integer widths, products, callbacks in loops and nested initializer regression");
}

static void documents(validation *v) {
    const char *bodies[] = {
        "quant bool n=superposition(0,1);if(n)phase(.31);int d=2;quant uint8_t q=div((int)n+5,d).quot;quant uint8_t r=div((int)n+5,d).rem;measure(q);measure(r);measure(n);return 0;",
        "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t y=n+a;++a;if(y==6)phase(.31);measure(n);return 0;"};
    for (unsigned profile = 0; profile < 2; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char source[4096], compiler[4096], path[4096], program[4096];
                validation_path(source, sizeof source, "%s/document.c", v->temp);
                validation_path(path, sizeof path, "%s/document.json", v->temp);
                validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <stdlib.h>\nint main(void){%s}\n",
                                bodies[profile]);
                validation_write(source, program);
                char *command[] = {compiler,
                                   source,
                                   "--emit-ir",
                                   (char *)levels[level],
                                   builtins ? "-fbuiltin" : "-fno-builtin",
                                   "-o",
                                   path,
                                   NULL};
                if (validation_run(v, command, NULL, 60)) {
                    char *log = validation_read(v->log);
                    validation_fail("binding document compile failed: %s", log);
                }
                char *ir = validation_read(path);
                cj_doc d = {0};
                if (!cj_parse(&d, ir))
                    validation_fail("binding document JSON failed");
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
                        validation_fail("unexpected fixture binding count");
                    int written = snprintf(rows + length,
                                           sizeof rows - length,
                                           "%s{\"instruction\":%u,\"values\":[%u]}",
                                           count++ ? "," : "",
                                           index,
                                           profile ? 5 : 2);
                    if (written < 0 || (size_t)written >= sizeof rows - length)
                        validation_fail("fixture binding rows exceeded budget");
                    length += (size_t)written;
                }
                if (!count)
                    validation_fail("CPU fixture did not emit binding templates");
                rows[length++] = ']';
                rows[length] = 0;
                qc_context *type_context = qc_create(0, 7);
                if (!type_context || !qc_document(type_context, ir, 1, 1) ||
                    !type_context->structural)
                    validation_fail("template schema validation failed: %s",
                                    qc_error(type_context));
                FILE *sink = tmpfile();
                if (!sink || qc_qasm(type_context, sink))
                    validation_fail("template validation was exported as gates");
                fclose(sink);
                qc_destroy(type_context);
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("document state allocation failed");
                    qc_allow_tables(c, tables);
                    char *bound = qc_bind_document(c, ir, rows);
                    if (!bound || !qc_document(c, bound, 0, 1) || c->structural || c->reset ||
                        c->measure != (profile ? 1u : 17u))
                        validation_fail("bound document execution failed: %s", qc_error(c));
                    for (size_t i = 0; i < qc_state_size(c); ++i) {
                        double complex expected = 0;
                        if (i == (profile ? 0u : 516u))
                            expected = 1 / sqrt(2.0);
                        if (i == (profile ? 1u : 7u))
                            expected = cexp(I * .31) / sqrt(2.0);
                        if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                            validation_fail(
                                "bound C document phase/workspace mismatch profile=%u basis=%zu",
                                profile,
                                i);
                    }
                    free(bound);
                    qc_destroy(c);
                    ++checks;
                }
                cj_free(&d);
                free(ir);
            }
    puts(
        "PASS 40 C template specializations: libc projections, CPU mutation, complete phases and structural/export separation");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_bindings");
    states();
    endpoints();
    rejected();
    document_contracts();
    validation v;
    validation_init(&v, argv[0]);
    programs(&v);
    documents(&v);
    validation_cleanup(&v);
    printf(
        "%u CPU binding checks passed; structural templates and bound circuit correctness have separate scopes.\n",
        checks);
    return 0;
}
