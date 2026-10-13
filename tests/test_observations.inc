/* Explicit value channels must preserve coherence inside each function fiber. */
static qc_context *observation_context(unsigned tables, uint64_t seed) {
    qc_context *c = qc_create(1, seed);
    if (!c)
        validation_fail("observation context allocation failed");
    qc_allow_tables(c, tables);
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":2}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":3}");
    qc_register *source = qc_lookup(c, 0);
    for (unsigned bit = 0; bit < 2; ++bit) {
        qc_mask wire = QC_WIRE(source->wires[bit]);
        if (!qc_emit(c, QC_PHASE, 0, wire, wire, bit ? .37 : .19))
            validation_fail("observation phase preparation failed");
    }
    return c;
}

static void observation_output(qc_context *c) {
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"ancilla\":true}");
    apply(
        c,
        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false}]}}");
}

static void observation_projector(qc_context *c, unsigned result, unsigned output, int dirty) {
    qc_register *source = qc_lookup(c, 0);
    qc_register *destination = qc_lookup(c, output);
    for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
        double complex expected = 0;
        for (unsigned raw = 0; raw < 4; ++raw) {
            if ((raw != 0) != result)
                continue;
            size_t index = 0;
            for (unsigned bit = 0; bit < 2; ++bit)
                if ((raw >> bit) & 1)
                    index |= (size_t)1 << source->wires[bit];
            if (dirty && result)
                index |= (size_t)1 << destination->wires[0];
            if (basis == index)
                expected =
                    cexp(I * (.19 * (raw & 1) + .37 * (raw >> 1))) / sqrt(result ? 3.0 : 1.0);
        }
        if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
            validation_fail("observation fiber lost phase/coherence result=%u dirty=%d basis=%zu",
                            result,
                            dirty,
                            basis);
    }
}

static void observation_channels(void) {
    unsigned count = 0;
    for (unsigned tables = 0; tables < 2; ++tables) {
        unsigned seen[2] = {0};
        for (unsigned shot = 0; shot < 128; ++shot) {
            qc_context *c = observation_context(tables, 913 + shot);
            observation_output(c);
            uint64_t epoch = qc_lookup(c, 0)->epoch, result;
            if (!qc_apply(c, "{\"op\":\"qobserve\",\"id\":1}", &result) || result > 1)
                validation_fail("computed observation failed: %s", qc_error(c));
            if (qc_lookup(c, 0)->epoch != epoch || !qc_lookup(c, 1)->definition ||
                c->measure != 1 || c->reset)
                validation_fail(
                    "observation changed inputs, discarded its inverse or inserted a channel");
            observation_projector(c, (unsigned)result, 1, 1);
            apply(c, "{\"op\":\"quncompute\",\"id\":1,\"saved\":true}");
            apply(c, "{\"op\":\"qrelease\",\"id\":1}");
            observation_projector(c, (unsigned)result, 1, 0);
            if (qc_lookup(c, 1)->alive || !qc_lookup(c, 0)->alive || c->nfree != 1)
                validation_fail("observed temporary lifetime did not end cleanly");
            ++seen[result];
            ++count;
            qc_destroy(c);
        }
        if (!seen[0] || !seen[1])
            validation_fail("observation seeds failed to exercise both fibers");
        for (unsigned invalid = 0; invalid < 2; ++invalid) {
            qc_context *c = observation_context(tables, 7);
            if (invalid) {
                observation_output(c);
                ++qc_lookup(c, 0)->epoch;
            }
            size_t size = qc_state_size(c);
            double complex *before = malloc(size * sizeof *before);
            if (!before)
                validation_fail("observation rejection snapshot allocation failed");
            for (size_t i = 0; i < size; ++i)
                before[i] = qc_amplitude(c, i);
            uint64_t gates = c->ngates;
            const char *operation =
                invalid ? "{\"op\":\"qobserve\",\"id\":1}" : "{\"op\":\"qobserve\",\"id\":0}";
            const char *diagnostic =
                invalid ? "unchanged live inputs" : "saved computed definition";
            if (qc_apply(c, operation, NULL) || !strstr(qc_error(c), diagnostic) ||
                c->ngates != gates || c->measure || c->reset)
                validation_fail(
                    "invalid observation mutated gates/channels or lacked its diagnostic");
            for (size_t i = 0; i < size; ++i)
                if (cabs(qc_amplitude(c, i) - before[i]) > 1e-12)
                    validation_fail("rejected observation altered its input state");
            free(before);
            qc_destroy(c);
            ++count;
        }
    }
    checks += count;
    printf(
        "PASS %u observation channel contracts: complete phased fibers before/after inverse, unchanged input epochs, clean release, both synthesis policies and rejection before mutation\n",
        count);
}

static void observation_programs(validation *v) {
    const char *bodies[] = {
        "quant bool x=false;unsigned old=measure(x++),now=measure(x);return old!=0||now!=1;",
        "quant bool x=false;unsigned now=measure(++x),value=measure(x);return now!=1||value!=1;",
        "quant bool x=true;unsigned old=measure(x--),now=measure(x);return old!=1||now!=0;",
        "quant bool x=true;unsigned now=measure(--x),value=measure(x);return now!=0||value!=0;",
        "quant uint8_t x=3;unsigned old=measure(x++),now=measure(x);return old!=3||now!=4;",
        "quant uint8_t x=255;unsigned old=measure(x++),now=measure(x);return old!=255||now!=0;",
        "quant uint8_t x=3;int sum=measure(x+2);unsigned old=measure(x);return sum!=5||old!=3;",
        "quant uint8_t x=3;unsigned sum=measure((uint8_t)(x+2)),old=measure(x);return sum!=5||old!=3;",
        "quant uint8_t x=2;unsigned truth=measure((bool)x),old=measure(x);return truth!=1||old!=2;",
        "quant int8_t x=-3;return measure(x)!=-3;",
        "quant int8_t x=-3;int now=measure((int8_t)(x-1)),old=measure(x);return now!=-4||old!=-3;",
        "quant uint8_t x=130;unsigned low=measure((uint8_t)(bool)x),old=measure(x);return low!=1||old!=130;",
        "quant uint8_t x=130;unsigned low=measure((uint16_t)(uint8_t)(bool)x),old=measure(x);return low!=1||old!=130;",
        "quant bool x=false;return measure(!x)!=1;",
        "quant bool x=false;quant bool y=!x;return measure(!y)!=0;",
        "quant bool x=false;unsigned sum=0;for(int i=0;i<3;++i)sum+=measure(!x);measure(x);return sum!=3;",
        "quant bool x=false;unsigned sum=0;int i=0;while(i++<3)sum+=measure(!x);measure(x);return sum!=3;",
        "quant bool x=false;unsigned sum=0;int i=0;do{sum+=measure(!x);}while(++i<3);measure(x);return sum!=3;",
        "quant bool x=false;return measure((x=true,x))!=1;",
        "quant bool x=false;return measure((x=true,x=false,x))!=0;",
        "quant bool x=false;int delta=2;int result=measure((uint8_t)(x+delta));delta=3;measure(x);return result!=2||delta!=3;",
        "quant bool x=false;int result=measure(x?1/x:3);measure(x);return result!=3;",
        "quant uint8_t a[1]={3};quant uint8_t *p=&a[0];unsigned old=measure((*p)++),now=measure(a[0]);return old!=3||now!=4;",
        "quant bool x=false;_Static_assert(__builtin_types_compatible_p(__typeof__(measure(!x)),int),\"C result type\");return sizeof(measure(x++))!=sizeof(bool)||measure(x)!=0;"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/observation.c", v->temp);
            validation_path(executable, sizeof executable, "%s/observation", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\nint main(void){%s}\n",
                            bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("computed measurement C failed case=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            ++count;
        }
    checks += count;
    printf(
        "PASS %u computed measurement C executions: typed results, once-only effects, casts, arithmetic, aliases, loops, last-use history and unevaluated type contexts at five GCC levels\n",
        count);
}

static void observation_diagnostics(validation *v) {
    const char *bodies[] = {
        "quant bool x=false;measure((double)x);measure(x);",
        "quant bool x=false;measure((long double)x);measure(x);",
        "quant bool x=false;int flag=0;measure(flag++?x++:x);measure(x);",
        "quant bool x=false;measure(x+++x);measure(x);",
        "quant bool c=superposition(0,1),x=false;if(c)measure(!x);measure(x);measure(c);",
        "quant bool x=false;unsigned n=measure(3);measure(x);return n;"};
    const char *messages[] = {"other encodings are pending",
                              "other encodings are pending",
                              "CPU effects in quantum expressions require once-only capture",
                              "unsequenced quantum modification",
                              "measurement/reset requires explicit classical control",
                              "measurement requires a quantum integer expression"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/observe-negative.c", v->temp);
            validation_path(executable, sizeof executable, "%s/observe-negative", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\nint main(void){%s}\n",
                            bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            int status = validation_run(v, command, NULL, 60);
            char *log = validation_read(v->log);
            if (!validation_rejected(status, log, messages[profile]))
                validation_fail("observation diagnostic failed case=%u level=%s\n%s",
                                profile,
                                levels[level],
                                log);
            free(log);
            ++count;
        }
    checks += count;
    printf(
        "PASS %u observation diagnostics: pending encodings/control/effects, unsequenced accesses and classical-only operands at five GCC levels\n",
        count);
}

static void compiled_observation_channels(validation *v) {
    const char *values[] = {"measure(a||b);", "quant uint8_t x=a+2*b;measure((bool)x);"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < 2; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], document[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/observe-state.c", v->temp);
            validation_path(document, sizeof document, "%s/observe-state.json", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\nint main(void){"
                            "quant bool a=superposition(0,1),b=superposition(0,1);"
                            "if(a)phase(.19);if(b)phase(.37);%s measure(a);measure(b);}\n",
                            values[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", document, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("compiled observation channel failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            char *ir = validation_read(document);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("invalid compiled observation document");
            for (unsigned tables = 0; tables < 2; ++tables)
                for (unsigned shot = 0; shot < 16; ++shot) {
                    qc_context *c = qc_create(1, 913 + shot);
                    if (!c || !qc_verify_document(c, &d))
                        validation_fail("compiled observation context/schema failed");
                    qc_allow_tables(c, tables);
                    uint64_t result = UINT64_MAX, temporary = UINT64_MAX;
                    unsigned observed = 0;
                    int list = cj_get(&d, 0, "instructions");
                    for (int token = list + 1; token < d.tokens[list].next;
                         token = d.tokens[token].next) {
                        int op = cj_get(&d, token, "op");
                        if (cj_equal(&d, op, "qmeasure"))
                            break;
                        uint64_t value;
                        if (!qc_apply_token(c, &d, token, 0, 0, &value))
                            validation_fail("compiled observation replay failed: %s", qc_error(c));
                        if (cj_equal(&d, op, "qobserve")) {
                            result = value;
                            ++observed;
                            if (!cj_u64(&d, cj_get(&d, token, "id"), &temporary))
                                validation_fail("compiled observation identity missing");
                        }
                    }
                    qc_register *a = qc_lookup(c, 0), *b = qc_lookup(c, 1);
                    if (observed != 1 || result > 1 || !a || !b ||
                        qc_lookup(c, (uint32_t)temporary)->alive || c->measure != 1 || c->reset)
                        validation_fail(
                            "compiled observation failed its width/lifetime/channel contract");
                    for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                        double complex expected = 0;
                        for (unsigned raw = 0; raw < 4; ++raw) {
                            if ((raw != 0) != result)
                                continue;
                            size_t index = ((size_t)(raw & 1) << a->wires[0]) |
                                           ((size_t)(raw >> 1) << b->wires[0]);
                            if (basis == index)
                                expected = cexp(I * (.19 * (raw & 1) + .37 * (raw >> 1))) /
                                           sqrt(result ? 3.0 : 1.0);
                        }
                        if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                            validation_fail(
                                "compiled predicate observation lost fiber coherence profile=%u level=%s tables=%u result=%llu basis=%zu",
                                profile,
                                levels[level],
                                tables,
                                (unsigned long long)result,
                                basis);
                    }
                    qc_destroy(c);
                    ++count;
                }
            cj_free(&d);
            free(ir);
        }
    checks += count;
    printf(
        "PASS %u compiled observation conditional states: logical/predicate C results, preserved relative phases within fibers, clean temporaries, five GCC levels and both synthesis policies\n",
        count);
}
