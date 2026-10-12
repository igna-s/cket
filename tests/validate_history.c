#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("history operation failed: %s\n%s", qc_error(c), operation);
}

static void states(void) {
    const char *operations[] = {"mul", "div", "mod", "band", "bor", "shr", "shl"};
    for (unsigned width = 2; width <= 4; ++width)
        for (unsigned sign = 0; sign < 2; ++sign)
            for (unsigned op = 0; op < 7 - sign; ++op)
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("history state allocation failed");
                    qc_allow_tables(c, tables);
                    char text[4096];
                    unsigned mask = (1u << width) - 1;
                    snprintf(text,
                             sizeof text,
                             "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":%s}",
                             width,
                             sign ? "true" : "false");
                    apply(c, text);
                    snprintf(text,
                             sizeof text,
                             "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":%u}",
                             sign ? 0 : mask);
                    apply(c, text);
                    if (sign)
                        apply(c, "{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}");
                    qc_mask phase = QC_WIRE(qc_lookup(c, 0)->wires[0]);
                    if (!qc_emit(c, QC_PHASE, 0, phase, phase, .31))
                        validation_fail("history phase preparation failed");
                    for (unsigned id = 1; id <= 2; ++id) {
                        snprintf(
                            text,
                            sizeof text,
                            "{\"op\":\"qalloc\",\"id\":%u,\"width\":%u,\"signed\":%s,\"ancilla\":true}",
                            id,
                            width,
                            sign ? "true" : "false");
                        apply(c, text);
                    }
                    snprintf(
                        text,
                        sizeof text,
                        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"bnot\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":%s}]}}",
                        width,
                        sign ? "true" : "false",
                        width,
                        sign ? "true" : "false");
                    apply(c, text);
                    uint64_t epoch = qc_lookup(c, 0)->epoch;
                    apply(c, "{\"op\":\"qretain\",\"id\":0}");
                    if (qc_lookup(c, 0)->epoch != epoch)
                        validation_fail("history marking invalidated an inverse");
                    unsigned constant = op < 3 ? 2 : 1;
                    snprintf(
                        text,
                        sizeof text,
                        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"%s\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":%s}]},{\"op\":\"const\",\"bits\":32,\"signed\":true,\"value\":%u}]}]}}",
                        width,
                        sign ? "true" : "false",
                        operations[op],
                        width,
                        sign ? "true" : "false",
                        constant);
                    apply(c, text);
                    for (unsigned inverse = 0; inverse < 2; ++inverse) {
                        for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                            double complex expected = 0;
                            for (unsigned raw = 0; raw <= mask; ++raw) {
                                int value = sign && (raw & (1u << (width - 1)))
                                                ? (int)raw - (int)(mask + 1)
                                                : (int)raw;
                                int result = op == 0   ? value * 2
                                             : op == 1 ? value / 2
                                             : op == 2 ? value % 2
                                             : op == 3 ? value & 1
                                             : op == 4 ? value | 1
                                             : op == 5 ? value >> 1
                                                       : value << 1;
                                size_t index = inverse ? raw
                                                       : raw | ((size_t)(raw ^ mask) << width) |
                                                             ((size_t)((unsigned)result & mask)
                                                              << (2 * width));
                                if (basis == index)
                                    expected =
                                        (raw & 1 ? cexp(I * .31) : 1) / sqrt((double)mask + 1);
                            }
                            if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                                validation_fail(
                                    "history state mismatch width=%u sign=%u op=%s tables=%u inverse=%u basis=%zu",
                                    width,
                                    sign,
                                    operations[op],
                                    tables,
                                    inverse,
                                    basis);
                        }
                        if (!inverse) {
                            apply(c, "{\"op\":\"quncompute\",\"id\":2,\"saved\":true}");
                            apply(c, "{\"op\":\"qrelease\",\"id\":2}");
                            apply(c, "{\"op\":\"quncompute\",\"id\":1,\"saved\":true}");
                            apply(c, "{\"op\":\"qrelease\",\"id\":1}");
                            apply(c, "{\"op\":\"qhistory_end\",\"id\":0}");
                        }
                    }
                    if (c->measure || c->reset || c->history_peak != width ||
                        !qc_lookup(c, 0)->alive)
                        validation_fail("history inserted a channel or erased an unknown state");
                    qc_destroy(c);
                    ++checks;
                }
    puts(
        "PASS 78 history domains: complete phased states, live old dependencies, typed arithmetic, both table policies and clean derived inverses");
}

static void lifetimes(void) {
    qc_context *c = qc_create(1, 7);
    if (!c)
        validation_fail("history lifetime allocation failed");
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":5,\"upper\":5}");
    apply(c, "{\"op\":\"qretain\",\"id\":0}");
    apply(c, "{\"op\":\"qretain\",\"id\":0}");
    apply(c, "{\"op\":\"qhistory_end\",\"id\":0}");
    if (qc_lookup(c, 0)->alive || c->measure || c->reset || c->history_peak != 3 || c->nfree != 3)
        validation_fail("known history was not restored and released unitarily");
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}");
    if (c->physical != 3 || qc_lookup(c, 1)->history)
        validation_fail("history wires were not safely reused");
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - (i == 0 ? 1 : 0)) > 1e-10)
            validation_fail("known history cleanup left garbage");
    qc_destroy(c);
    ++checks;
    const char *invalid[] = {"{\"op\":\"qretain\",\"id\":0,\"controls\":[]}",
                             "{\"op\":\"qhistory_end\",\"id\":0}"};
    for (unsigned i = 0; i < 2; ++i) {
        c = qc_create(1, 7);
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
        apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
        size_t gates = c->ngates;
        if (qc_apply(c, invalid[i], NULL) || c->ngates != gates || qc_lookup(c, 0)->history)
            validation_fail("invalid history operation mutated storage");
        qc_destroy(c);
        ++checks;
    }
    c = qc_create(1, 7);
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":5,\"upper\":5}");
    apply(c, "{\"op\":\"qretain\",\"id\":0}");
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}}");
    size_t gates = c->ngates;
    if (qc_apply(c, "{\"op\":\"qhistory_end\",\"id\":0}", NULL) || c->ngates != gates ||
        !qc_lookup(c, 0)->alive)
        validation_fail("history cleanup ignored a live inverse dependency");
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - (i == 45 ? 1 : 0)) > 1e-10)
            validation_fail("rejected history cleanup altered the joint state");
    qc_destroy(c);
    ++checks;
    c = qc_create(1, 7);
    if (!c)
        validation_fail("predicate cache context allocation failed");
    qc_allow_tables(c, 0);
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false,\"ancilla\":true}");
    apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false,\"ancilla\":true}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}");
    const char *expression = "{\"op\":\"select\",\"bits\":1,\"signed\":false,\"args\":["
                             "{\"op\":\"reg\",\"id\":1,\"bits\":1,\"signed\":false},"
                             "{\"op\":\"select\",\"bits\":1,\"signed\":false,\"args\":["
                             "{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false},"
                             "{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1},"
                             "{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":["
                             "{\"op\":\"div\",\"bits\":32,\"signed\":true,\"args\":["
                             "{\"op\":\"const\",\"bits\":32,\"signed\":true,\"value\":1},"
                             "{\"op\":\"const\",\"bits\":32,\"signed\":true,\"value\":0}]}]}]},"
                             "{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":0}]}";
    char operation[4096];
    snprintf(operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s}", expression);
    apply(c, operation);
    for (size_t basis = 0; basis < qc_state_size(c); ++basis)
        if (cabs(qc_amplitude(c, basis) - (basis == 0 || basis == 7 ? 1 / sqrt(2.0) : 0)) > 1e-10)
            validation_fail("Boolean predicate certificate changed the coherent state");
    snprintf(
        operation, sizeof operation, "{\"op\":\"quncompute\",\"id\":2,\"expr\":%s}", expression);
    apply(c, operation);
    apply(
        c,
        "{\"op\":\"quncompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"not\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}]}}");
    gates = c->ngates;
    snprintf(operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s}", expression);
    if (qc_apply(c, operation, NULL) || c->ngates != gates || qc_lookup(c, 2)->definition ||
        (!strstr(qc_error(c), "truth tables are disabled") &&
         !strstr(qc_error(c), "division/remainder by zero")))
        validation_fail("a changed Boolean predicate reused an invalid cached proof");
    qc_destroy(c);
    ++checks;
    puts(
        "PASS 5 history/proof lifetimes: known basis restoration/reuse, live inverse protection, schema rejection and predicate cache invalidation");
}

static void programs(validation *v) {
    const char *bodies[] = {
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x*=2;unsigned value=measure(x),nn=measure(n);return value!=2*(nn+3);",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x/=2;unsigned value=measure(x),nn=measure(n);return value!=(nn+3)/2;",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x%=2;unsigned value=measure(x),nn=measure(n);return value!=(nn+3)%2;",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x&=1;unsigned value=measure(x),nn=measure(n);return value!=((nn+3)&1);",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x|=1;unsigned value=measure(x),nn=measure(n);return value!=((nn+3)|1);",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x<<=1;unsigned value=measure(x),nn=measure(n);return value!=((nn+3)<<1);",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x>>=1;unsigned value=measure(x),nn=measure(n);return value!=((nn+3)>>1);",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;x=0;unsigned value=measure(x),nn=measure(n);return value!=0||nn>1;",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;int divisor=2;x/=divisor;++divisor;unsigned value=measure(x),nn=measure(n);return value!=(nn+3)/2||divisor!=3;",
        "quant uint8_t values[2]={3,4};quant uint8_t *p=&values[0];values[0]*=2;unsigned value=measure(*p),other=measure(values[1]);return value!=6||other!=4;",
        "#include <quantum.h>\nstatic void halve(quant uint8_t x){x/=2;}\nint main(void){quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;halve(x);unsigned value=measure(x),nn=measure(n);return value!=(nn+3)/2;}\n",
        "quant bool n=superposition(0,1);quant uint8_t a=(unsigned)n+3;quant uint8_t b=a*2;unsigned value=measure(b),nn=measure(n);return value!=2*(nn+3);",
        "quant uint8_t x=superposition(3,3);x=x*2+1;return measure(x)!=7;",
        "quant uint8_t x=superposition(3,3);x^=x>>1;return measure(x)!=2;",
        "quant uint8_t x=superposition(3,3);x=x*2+x;return measure(x)!=9;"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned i = 0; i < sizeof bodies / sizeof *bodies; ++i) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/history.c", v->temp);
            validation_path(executable, sizeof executable, "%s/history", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            if (*bodies[i] == '#')
                validation_write(source, bodies[i]);
            else {
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\nint main(void){%s}\n",
                                bodies[i]);
                validation_write(source, program);
            }
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, command, NULL, 60)) {
                char *log = validation_read(v->log);
                validation_fail(
                    "history C integration failed case=%u level=%s\n%s", i, levels[level], log);
            }
            if (i >= 12) {
                char path[4096];
                validation_path(path, sizeof path, "%s/affine.json", v->temp);
                char *emit[] = {
                    compiler, source, "--emit-ir", (char *)levels[level], "-o", path, NULL};
                if (validation_run(v, emit, NULL, 60))
                    validation_fail("combined assignment IR compilation failed");
                char *ir = validation_read(path);
                cj_doc d = {0};
                if (!cj_parse(&d, ir))
                    validation_fail("invalid combined assignment document");
                int list = cj_get(&d, 0, "instructions");
                unsigned retained = 0;
                for (int token = list + 1; token < d.tokens[list].next;
                     token = d.tokens[token].next)
                    retained += cj_equal(&d, cj_get(&d, token, "op"), "qretain");
                if (retained != (i == 14 ? 0u : 1u))
                    validation_fail(
                        "combined assignment selected an incorrect history path case=%u level=%s",
                        i,
                        levels[level]);
                cj_free(&d);
                free(ir);
            }
            ++checks;
        }
    puts(
        "PASS 75 C history integrations: combined assignments, CPU snapshots, fixed arrays/pointer aliases, mutable functions and measurement-chain regression at five GCC levels");
}

static void documents(validation *v) {
    const char *bodies[] = {
        "quant bool n=superposition(0,1);quant bool x=!n;if(n)phase(.31);x+=1;if(x)phase(.17);measure(x);measure(n);",
        "quant uint8_t x=superposition(0,3);if(x&2)phase(.19);x&=1;if(x&1)phase(.31);measure(x);",
        "quant bool n=superposition(0,1);quant bool x=!n;int d=1;x/=d;++d;if(x)phase(.31);measure(n);return d!=2;"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    for (unsigned profile = 0; profile < 3; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], path[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/document.c", v->temp);
            validation_path(path, sizeof path, "%s/document.json", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\nint main(void){%s}\n",
                            bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", path, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("history document compilation failed: %s", validation_read(v->log));
            char *ir = validation_read(path);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("history document JSON failed");
            unsigned index = 0, bindings = 0, result_id = UINT32_MAX;
            char rows[4096] = "[";
            size_t length = 1;
            int list = cj_get(&d, 0, "instructions");
            for (int token = list + 1; token < d.tokens[list].next;
                 token = d.tokens[token].next, ++index) {
                if (cj_equal(&d, cj_get(&d, token, "op"), "qbind")) {
                    int parameters = cj_get(&d, token, "parameters");
                    if (profile != 2 || d.tokens[parameters].children != 1)
                        validation_fail("unexpected history fixture CPU parameters");
                    int written = snprintf(rows + length,
                                           sizeof rows - length,
                                           "%s{\"instruction\":%u,\"values\":[1]}",
                                           bindings++ ? "," : "",
                                           index);
                    if (written < 0 || (size_t)written >= sizeof rows - length)
                        validation_fail("history binding row budget exceeded");
                    length += (size_t)written;
                }
                if (result_id == UINT32_MAX && cj_equal(&d, cj_get(&d, token, "op"), "qmeasure")) {
                    uint64_t id;
                    if (!cj_u64(&d, cj_get(&d, token, "id"), &id))
                        validation_fail("invalid history measurement fixture");
                    result_id = (uint32_t)id;
                }
            }
            rows[length++] = ']';
            rows[length] = 0;
            if ((profile == 2) != !!bindings || result_id == UINT32_MAX)
                validation_fail("history document fixture is missing its bindings/output");
            cj_free(&d);
            for (unsigned tables = 0; tables < 2; ++tables) {
                qc_context *c = qc_create(1, 7);
                if (!c)
                    validation_fail("history document state allocation failed");
                qc_allow_tables(c, tables);
                char *bound = bindings ? qc_bind_document(c, ir, rows) : NULL;
                if ((bindings && !bound) || !qc_document(c, bound ? bound : ir, 0, 1))
                    validation_fail(
                        "history document execution failed profile=%u level=%s tables=%u path=%s: %s",
                        profile,
                        levels[level],
                        tables,
                        path,
                        qc_error(c));
                qc_register *out = qc_lookup(c, result_id);
                size_t indices[4] = {0};
                unsigned count = profile == 1 ? 4 : 2;
                for (unsigned sample = 0; sample < count; ++sample) {
                    if (profile == 0) {
                        indices[sample] = sample;
                        unsigned result = 1;
                        for (unsigned bit = 0; bit < out->width; ++bit)
                            if ((result >> bit) & 1)
                                indices[sample] |= (size_t)1 << out->wires[bit];
                    } else if (profile == 1) {
                        indices[sample] = sample;
                        if (sample & 1)
                            indices[sample] |= (size_t)1 << out->wires[0];
                    } else
                        indices[sample] = sample;
                }
                for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                    double complex expected = 0;
                    for (unsigned sample = 0; sample < count; ++sample)
                        if (basis == indices[sample]) {
                            double angle = profile == 0 ? (sample ? .48 : .17)
                                           : profile == 1
                                               ? ((sample & 1 ? .31 : 0) + (sample & 2 ? .19 : 0))
                                               : (sample ? 0 : .31);
                            expected = cexp(I * angle) / sqrt((double)count);
                        }
                    if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                        validation_fail(
                            "history C complete state mismatch profile=%u level=%s tables=%u basis=%zu",
                            profile,
                            levels[level],
                            tables,
                            basis);
                }
                if (c->reset || c->history_peak != (profile == 1 ? 8u : 1u) ||
                    c->measure != (profile == 0   ? 2u
                                   : profile == 1 ? 8u
                                                  : 1u))
                    validation_fail("history C channels/resources disagree with the source");
                free(bound);
                qc_destroy(c);
                ++checks;
            }
            free(ir);
        }
    puts(
        "PASS 30 C complete states: derived/history cleanup, retained unknown outputs and CPU snapshot inverse at five GCC levels with both synthesis policies");
}

#include "test_coherent_history.inc"

static void rejections(validation *v) {
    const char *bodies[] = {
        "quant uint8_t x=superposition(0,3);volatile int c=1;if(c)x%=2;measure(x);",
        "quant uint8_t x=superposition(0,3);volatile int count=2;while(count--)x%=2;measure(x);",
        "volatile int flag=1;quant bool c=superposition(0,1);quant uint8_t x=superposition(0,3);if(c){if(flag)x%=2;}measure(x);measure(c);",
        "quant bool n=superposition(0,1);quant uint8_t x=(unsigned)n+3;int factor=2;x*=factor;measure(x);measure(n);"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/negative.c", v->temp);
            validation_path(executable, sizeof executable, "%s/negative", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program, sizeof program, "#include <quantum.h>\nint main(void){%s}\n", bodies[i]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            int status = validation_run(v, command, NULL, 60);
            char *log = validation_read(v->log);
            const char *diagnostic = i == 3   ? "GCC narrowed a multiplication"
                                     : i == 2 ? "classical branch inside quantum control"
                                              : "versioned assignment";
            if (!validation_rejected(status, log, diagnostic))
                validation_fail("history contract rejection failed case=%u level=%s\n%s",
                                i,
                                levels[level],
                                log);
            free(log);
            ++checks;
        }
    puts(
        "PASS 20 pending-contract diagnostics: CPU joins/iterations, CPU branches under coherent control and lost signed multiplication promotions at five GCC levels");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_history");
    states();
    lifetimes();
    validation v;
    validation_init(&v, argv[0]);
    programs(&v);
    documents(&v);
    coherent_documents(&v);
    coherent_word_documents(&v);
    rejections(&v);
    validation_cleanup(&v);
    printf(
        "%u versioned history checks passed; retained outputs, known basis cleanup and resource bounds remain distinct.\n",
        checks);
    return 0;
}
