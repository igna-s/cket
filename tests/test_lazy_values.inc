/* Lazy effects are compared with ordinary C, then as full phased joint states. */
#include <stdbool.h>

static void lazy_constant_contracts(void) {
    unsigned count = 0;
    for (unsigned tables = 0; tables < 2; ++tables)
        for (unsigned profile = 0; profile < 4; ++profile) {
            qc_context *c = qc_create(1, 7);
            if (!c)
                validation_fail("constant predicate allocation failed");
            qc_allow_tables(c, tables);
            apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1}");
            apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}");
            if (!qc_emit(c, QC_PHASE, 0, 0, 0, .19))
                validation_fail("constant predicate phase failed");
            apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"ancilla\":true}");
            const char *guard =
                profile == 1
                    ? "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":32,\"signed\":true,\"value\":0}]}}"
                    : "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}";
            apply(c, guard);
            apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"ancilla\":true}");
            if (profile == 2)
                ++qc_lookup(c, 0)->epoch;
            if (profile == 3)
                qc_lookup(c, 1)->definition_mask = QC_WIRE(qc_lookup(c, 0)->wires[0]);
            if (profile >= 2) {
                // Exercise the canonical-definition proof without an independent
                // value certificate. Changing definition metadata alone does not
                // invalidate the already proven physical output's support.
                qc_register *predicate = qc_lookup(c, 1);
                predicate->basis_known = 0;
                predicate->zero_mask = 0;
                predicate->support_interval = 0;
            }
            const char *compute =
                "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"select\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":1,\"signed\":false},{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"div\",\"bits\":8,\"signed\":false,\"args\":[{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1},{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":0}]}]},{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}]}}";
            uint64_t gates = c->ngates;
            int success = qc_apply(c, compute, NULL);
            if (profile < 2) {
                if (!success)
                    validation_fail("known inactive predicate rejected: %s", qc_error(c));
                apply(c, "{\"op\":\"quncompute\",\"id\":2,\"saved\":true}");
                apply(c, "{\"op\":\"qrelease\",\"id\":2}");
                apply(c, "{\"op\":\"quncompute\",\"id\":1,\"saved\":true}");
                apply(c, "{\"op\":\"qrelease\",\"id\":1}");
            } else if (success || c->ngates != gates || qc_lookup(c, 2)->definition ||
                       !*qc_error(c))
                validation_fail("uncertified constant predicate changed gates/definition");
            if (c->measure || c->reset)
                validation_fail("constant proof inserted a channel");
            for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                double complex expected = basis ? 0 : cexp(I * .19);
                if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                    validation_fail("constant predicate inverse/rejection changed state");
            }
            qc_destroy(c);
            ++count;
        }
    checks += count;
    printf(
        "PASS %u constant predicate contracts: known roots/literal definitions, missing canonical domain input, stale epochs, partial controls, phases and rejection before mutation\n",
        count);
}

static const char *lazy_values[] = {
    "c?x++:x--",
    "c&&x++",
    "c||x++",
    "c?(x=false):(x=true)",
    "c?(x++,x):(x--,x)",
    "x&&(x=false)",
    "x||(x=true)",
    "c&&(x||x++)",
    "c?c++:c--",
    "c?++x:--x",
    "c?(x+=1):(x^=1)",
    "c?((x=false),c):(x=true)",
    "c?x++:(x?x--:x++)",
    "x++&&++x",
    "x++||--x",
    "(c=false)?x++:x--",
};

/* These defined C11 operations intentionally exercise Boolean increments. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbool-operation"

static void lazy_reference(unsigned profile,
                           unsigned input_c,
                           unsigned input_x,
                           unsigned *value,
                           unsigned *next_c,
                           unsigned *next_x) {
    bool c = input_c, x = input_x, result;
    switch (profile) {
    case 0:
        result = c ? x++ : x--;
        break;
    case 1:
        result = c && x++;
        break;
    case 2:
        result = c || x++;
        break;
    case 3:
        result = c ? (x = false) : (x = true);
        break;
    case 4:
        result = c ? (x++, x) : (x--, x);
        break;
    case 5:
        result = x && (x = false);
        break;
    case 6:
        result = x || (x = true);
        break;
    case 7:
        result = c && (x || x++);
        break;
    case 8:
        result = c ? c++ : c--;
        break;
    case 9:
        result = c ? ++x : --x;
        break;
    case 10:
        result = c ? (x += 1) : (x ^= 1);
        break;
    case 11:
        result = c ? ((x = false), c) : (x = true);
        break;
    case 12:
        result = c ? x++ : (x ? x-- : x++);
        break;
    case 13:
        result = x++ && ++x;
        break;
    case 14:
        result = x++ || --x;
        break;
    case 15:
        result = (c = false) ? x++ : x--;
        break;
    default:
        validation_fail("unknown lazy C reference profile");
        result = false;
    }
    *value = result;
    *next_c = c;
    *next_x = x;
}

#pragma GCC diagnostic pop

static void lazy_value_programs(validation *v) {
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof lazy_values / sizeof *lazy_values; ++profile)
        for (unsigned input_c = 0; input_c < 2; ++input_c)
            for (unsigned input_x = 0; input_x < 2; ++input_x)
                for (unsigned level = 0; level < 5; ++level) {
                    unsigned result, next_c, next_x;
                    lazy_reference(profile, input_c, input_x, &result, &next_c, &next_x);
                    char source[4096], executable[4096], compiler[4096], program[8192];
                    validation_path(source, sizeof source, "%s/lazy-value.c", v->temp);
                    validation_path(executable, sizeof executable, "%s/lazy-value", v->temp);
                    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\nint main(void){quant bool c=%u,x=%u;quant bool y=%s;"
                        "unsigned a=measure(y),b=measure(x),d=measure(c);return a!=%u||b!=%u||d!=%u;}\n",
                        input_c,
                        input_x,
                        lazy_values[profile],
                        result,
                        next_x,
                        next_c);
                    validation_write(source, program);
                    char *command[] = {
                        compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
                    if (validation_run(v, command, NULL, 60))
                        validation_fail("lazy C values failed profile=%u inputs=%u/%u level=%s\n%s",
                                        profile,
                                        input_c,
                                        input_x,
                                        levels[level],
                                        validation_read(v->log));
                    ++count;
                }
    checks += count;
    printf(
        "PASS %u lazy C values: ordinary C references, selected effects, nested short circuit, self conditions and five GCC levels\n",
        count);
}

static void lazy_value_combinations(validation *v) {
    const char *bodies[] = {
        "int flag=1;quant bool x=false;quant bool y=flag?x++:x--;flag=0;unsigned a=measure(y),b=measure(x);return a!=0||b!=1||flag!=0;",
        "int flag=0;quant bool x=false;quant bool y=flag?x++:x--;flag=1;unsigned a=measure(y),b=measure(x);return a!=0||b!=1||flag!=1;",
        "int offset=1,flag=0;quant bool x=false;quant uint8_t y=offset+(flag?x++:x--);offset=9;flag=1;unsigned a=measure(y),b=measure(x);return a!=1||b!=1||offset!=9||flag!=1;",
        "quant bool c=false,a[1]={false};quant bool *p=&a[0];quant bool y=c?(*p)++:--a[0];unsigned out=measure(y),now=measure(a[0]),flag=measure(c);return out!=1||now!=1||flag!=0;",
        "quant bool c=true,x=false,y=false;choose(c,x,&y);unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=0||now!=1||flag!=1;",
        "quant bool c=true,x=false,y=false;for(unsigned i=0;i<3;++i)y=c?x++:x--;unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=1||now!=1||flag!=1;",
        "quant bool c=true,x=false,y=false;if(c)y=c?x++:x--;unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=0||now!=1||flag!=1;",
        "quant bool c=false,x=true;int zero=0;quant bool y=c?(x/=zero):x++;unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=1||now!=1||flag!=0;",
        "quant bool c=false;quant uint8_t x=7;quant bool y=c?(x-=3):(x/=2);unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=1||now!=3||flag!=0;",
        "quant bool c=true;quant int8_t x=-3;quant bool y=c?(x+=1):(x-=1);unsigned out=measure(y);int now=measure(x);unsigned flag=measure(c);return out!=1||now!=-2||flag!=1;",
        "int flag=1;quant bool x=false;int out=measure(flag?x++:x--);unsigned now=measure(x);return out!=0||now!=1;",
        "quant bool c=true,x=false;quant uint8_t y=(c?x++:x--)+2;unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=2||now!=1||flag!=1;",
        "quant bool c=true,x=false;quant bool y=(c?x++:x--,x);unsigned out=measure(y),now=measure(x),flag=measure(c);return out!=1||now!=1||flag!=1;",
    };
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/lazy-combined.c", v->temp);
            validation_path(executable, sizeof executable, "%s/lazy-combined", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program,
                sizeof program,
                "#include <quantum.h>\nstatic void choose(quant bool c,quant bool x,quant bool *y){*y=c?x++:x--;}\nint main(void){%s}\n",
                bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("lazy combined C failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            ++count;
        }
    checks += count;
    printf(
        "PASS %u lazy C combinations: CPU snapshots, memory aliases, functions, nested control, static loops, inactive arithmetic, typed words and explicit observation\n",
        count);
}

static void lazy_value_states(validation *v) {
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof lazy_values / sizeof *lazy_values; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], document[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/lazy-state.c", v->temp);
            validation_path(document, sizeof document, "%s/lazy-state.json", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program,
                sizeof program,
                "#include <quantum.h>\nint main(void){quant bool c=superposition(0,1),x=superposition(0,1);"
                "if(c)phase(.19);if(x)phase(.31);quant bool y=%s;if(y)phase(.37);"
                "measure(y);measure(x);measure(c);}\n",
                lazy_values[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", document, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("lazy state compilation failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            char *ir = validation_read(document);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("invalid lazy state document");
            uint32_t outputs[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
            unsigned measured = 0;
            int list = cj_get(&d, 0, "instructions");
            for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next)
                if (cj_equal(&d, cj_get(&d, token, "op"), "qmeasure")) {
                    uint64_t id;
                    if (measured >= 3 || !cj_u64(&d, cj_get(&d, token, "id"), &id))
                        validation_fail("invalid lazy output measurement");
                    outputs[measured++] = (uint32_t)id;
                }
            cj_free(&d);
            if (measured != 3)
                validation_fail("lazy values inserted an extra channel");
            for (unsigned tables = 0; tables < 2; ++tables) {
                qc_context *context = qc_create(1, 7);
                if (!context)
                    validation_fail("lazy state allocation failed");
                qc_allow_tables(context, tables);
                if (!qc_document(context, ir, 0, 1))
                    validation_fail("lazy state execution failed profile=%u level=%s tables=%u: %s",
                                    profile,
                                    levels[level],
                                    tables,
                                    qc_error(context));
                if (context->reset || context->measure != 3)
                    validation_fail("lazy effect cleanup changed explicit channels");
                size_t indices[4];
                double angles[4];
                for (unsigned raw = 0; raw < 4; ++raw) {
                    unsigned input_c = raw & 1, input_x = raw >> 1, value, next_c, next_x;
                    lazy_reference(profile, input_c, input_x, &value, &next_c, &next_x);
                    indices[raw] = 0;
                    const uint32_t ids[] = {0, 1, outputs[0], outputs[1], outputs[2]};
                    const unsigned values[] = {input_c, input_x, value, next_x, next_c};
                    for (unsigned field = 0; field < 5; ++field) {
                        qc_register *storage = qc_lookup(context, ids[field]);
                        if (!storage || !storage->alive || storage->width != 1)
                            validation_fail("lazy output/root storage disappeared");
                        if (values[field])
                            indices[raw] |= (size_t)1 << storage->wires[0];
                    }
                    angles[raw] = .19 * input_c + .31 * input_x + .37 * value;
                }
                for (size_t basis = 0; basis < qc_state_size(context); ++basis) {
                    double complex expected = 0;
                    for (unsigned raw = 0; raw < 4; ++raw)
                        if (basis == indices[raw])
                            expected += cexp(I * angles[raw]) / 2;
                    if (cabs(qc_amplitude(context, basis) - expected) > 1e-10)
                        validation_fail(
                            "lazy full state mismatch profile=%u level=%s tables=%u basis=%zu",
                            profile,
                            levels[level],
                            tables,
                            basis);
                }
                for (size_t i = 0; i < context->nregisters; ++i)
                    if (context->registers[i].alive && context->registers[i].history &&
                        context->registers[i].definition)
                        validation_fail("lazy decision/intermediate history was not uncomputed");
                qc_destroy(context);
                ++count;
            }
            free(ir);
        }
    checks += count;
    printf(
        "PASS %u lazy full states: independent C values, phases, retained roots, zero private garbage, exact channels, five GCC levels and both synthesis policies\n",
        count);
}

static void lazy_value_rejections(validation *v) {
    const char *bodies[] = {
        "quant bool c=superposition(0,1),x=false;quant bool y=c?(x+++x):false;measure(y);measure(x);measure(c);",
        "int zero=0;quant bool c=superposition(0,1),x=true;quant bool y=c?(x/=zero):false;measure(y);measure(x);measure(c);",
        "int limit=3;quant bool c=true,x=false,y=false;for(int i=0;i<limit;++i)y=c?x++:x--;measure(y);measure(x);measure(c);",
        "volatile int flag=1;quant bool c=superposition(0,1),x=false;quant bool y=c?(x=flag):x++;measure(y);measure(x);measure(c);",
    };
    const char *messages[] = {"unsequenced quantum modification",
                              "division/remainder by zero",
                              "CPU-loop history is pending",
                              "CPU effects in quantum expressions require once-only capture"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/lazy-negative.c", v->temp);
            validation_path(executable, sizeof executable, "%s/lazy-negative", v->temp);
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
                validation_fail(
                    "lazy diagnostic failed profile=%u level=%s\n%s", profile, levels[level], log);
            free(log);
            ++count;
        }
    checks += count;
    printf(
        "PASS %u lazy diagnostics: reachable undefined operations, pending loop history and volatile CPU effects; five GCC levels\n",
        count);
}
