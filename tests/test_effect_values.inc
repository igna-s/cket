/* Value-producing updates: native C results and complete phased joint states. */
static void effect_value_programs(validation *v) {
    const char *bodies[] = {
        "quant bool x=false;quant bool y=x++;unsigned a=measure(y),b=measure(x);return a!=0||b!=1;",
        "quant bool x=false;quant bool y=++x;unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant bool x=true;quant bool y=x--;unsigned a=measure(y),b=measure(x);return a!=1||b!=0;",
        "quant bool x=true;quant bool y=--x;unsigned a=measure(y),b=measure(x);return a!=0||b!=0;",
        "quant bool x=false;quant bool y=(x=true);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant bool x=false;quant bool y=(x+=1);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant bool x=false;quant bool y=(x++,x);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant uint8_t x=3;quant uint8_t y=x+++2;unsigned a=measure(y),b=measure(x);return a!=5||b!=4;",
        "quant uint8_t x=3;quant uint8_t y=++x;unsigned a=measure(y),b=measure(x);return a!=4||b!=4;",
        "quant uint8_t x=3;quant uint8_t y=x--;unsigned a=measure(y),b=measure(x);return a!=3||b!=2;",
        "quant uint8_t x=3;quant uint8_t y=--x;unsigned a=measure(y),b=measure(x);return a!=2||b!=2;",
        "quant uint8_t x=3;quant uint8_t y=(x+=2);unsigned a=measure(y),b=measure(x);return a!=5||b!=5;",
        "quant uint8_t x=3;quant uint8_t y=(x-=2);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant uint8_t x=3;quant uint8_t y=(x*=2);unsigned a=measure(y),b=measure(x);return a!=6||b!=6;",
        "quant uint8_t x=7;quant uint8_t y=(x/=2);unsigned a=measure(y),b=measure(x);return a!=3||b!=3;",
        "quant uint8_t x=7;quant uint8_t y=(x%=3);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant uint8_t x=3;quant uint8_t y=(x<<=1);unsigned a=measure(y),b=measure(x);return a!=6||b!=6;",
        "quant uint8_t x=7;quant uint8_t y=(x>>=1);unsigned a=measure(y),b=measure(x);return a!=3||b!=3;",
        "quant uint8_t x=7;quant uint8_t y=(x&=3);unsigned a=measure(y),b=measure(x);return a!=3||b!=3;",
        "quant uint8_t x=3;quant uint8_t y=(x|=4);unsigned a=measure(y),b=measure(x);return a!=7||b!=7;",
        "quant uint8_t x=3;quant uint8_t y=(x^=2);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant uint8_t a[1]={3};quant uint8_t *p=&a[0];quant uint8_t y=(*p)++;unsigned old=measure(y),now=measure(a[0]);return old!=3||now!=4;",
        "quant bool x=false;quant bool y=(x=true,x=false,x=true);unsigned a=measure(y),b=measure(x);return a!=1||b!=1;",
        "quant bool x=false;quant bool y=(x++,(bool)0);unsigned a=measure(y),b=measure(x);return a!=0||b!=1;"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/effect.c", v->temp);
            validation_path(executable, sizeof executable, "%s/effect", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\nint main(void){%s}\n",
                            bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("quantum effect result failed case=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            ++count;
        }
    checks += count;
    printf(
        "PASS %u native quantum effect values: prefix/postfix, all assignment forms, comma and fixed aliases at five GCC levels\n",
        count);
}

static void effect_value_states(validation *v) {
    const char *values[] = {"x++", "++x", "x--", "--x", "(x=true)", "(x++,x)"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof values / sizeof *values; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], document[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/effect-state.c", v->temp);
            validation_path(document, sizeof document, "%s/effect-state.json", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\nint main(void){"
                            "quant bool x=superposition(0,1);if(x)phase(.19);"
                            "quant bool y=%s;if(y)phase(.31);measure(y);measure(x);}\n",
                            values[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", document, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("quantum effect state compilation failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            char *ir = validation_read(document);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("invalid effect state document");
            uint64_t outputs[2] = {UINT64_MAX, UINT64_MAX};
            unsigned measured = 0;
            int list = cj_get(&d, 0, "instructions");
            for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next)
                if (cj_equal(&d, cj_get(&d, token, "op"), "qmeasure")) {
                    if (measured == 2 || !cj_u64(&d, cj_get(&d, token, "id"), &outputs[measured++]))
                        validation_fail("invalid effect measurement identities");
                }
            cj_free(&d);
            if (measured != 2)
                validation_fail("effect fixture needs exactly two explicit measurements");
            for (unsigned tables = 0; tables < 2; ++tables) {
                qc_context *c = qc_create(1, 7);
                if (!c)
                    validation_fail("effect state context allocation failed");
                qc_allow_tables(c, tables);
                if (!qc_document(c, ir, 0, 1))
                    validation_fail(
                        "effect state execution failed profile=%u level=%s tables=%u: %s",
                        profile,
                        levels[level],
                        tables,
                        qc_error(c));
                qc_register *y = qc_lookup(c, (uint32_t)outputs[0]);
                qc_register *x = qc_lookup(c, (uint32_t)outputs[1]);
                qc_register *root = qc_lookup(c, 0);
                if (!x || !y || !root || c->reset || c->measure != 2)
                    validation_fail("effect values changed the explicit channel contract");
                size_t indices[2] = {0};
                double angles[2];
                for (unsigned old = 0; old < 2; ++old) {
                    unsigned now = profile == 2 || profile == 3 ? !old : 1;
                    unsigned result = profile == 0 || profile == 2 ? old : now;
                    if (now)
                        indices[old] |= (size_t)1 << x->wires[0];
                    if (result)
                        indices[old] |= (size_t)1 << y->wires[0];
                    if (root->history && old)
                        indices[old] |= (size_t)1 << root->wires[0];
                    angles[old] = .19 * old + .31 * result;
                }
                for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                    double complex expected = 0;
                    for (unsigned old = 0; old < 2; ++old)
                        if (basis == indices[old])
                            expected += cexp(I * angles[old]) / sqrt(2.0);
                    if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                        validation_fail(
                            "quantum effect complete state mismatch profile=%u level=%s tables=%u basis=%zu",
                            profile,
                            levels[level],
                            tables,
                            basis);
                }
                qc_destroy(c);
                ++count;
            }
            free(ir);
        }
    checks += count;
    printf(
        "PASS %u quantum effect complete states: old/new values, correlations, relative phase, history and zero private garbage; five GCC levels and both synthesis policies\n",
        count);
}

static void effect_value_rejections(validation *v) {
    const char *bodies[] = {
        "quant bool x=false;quant bool y=x+++x;measure(y);measure(x);",
        "quant bool x=false;quant bool y=x+++x++;measure(y);measure(x);",
        "quant bool x=false;quant bool y=x++==x;measure(y);measure(x);",
        "quant bool c=superposition(0,1);quant bool x=false;quant bool y=c?x++:x;measure(y);measure(x);measure(c);",
        "quant bool c=superposition(0,1);quant bool x=false;quant bool y=c&&x++;measure(y);measure(x);measure(c);",
        "quant bool c=superposition(0,1);quant bool x=false;quant bool y=c||x++;measure(y);measure(x);measure(c);",
        "int flag=1;quant bool x=false;quant bool y=flag?x++:x;measure(y);measure(x);",
        "quant bool x=false;unsigned old=measure(x++);measure(x);return old;"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/effect-negative.c", v->temp);
            validation_path(executable, sizeof executable, "%s/effect-negative", v->temp);
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
            const char *diagnostic = profile < 3 ? "unsequenced quantum modification"
                                     : profile == 7
                                         ? "measurement/reset requires available quantum storage"
                                         : "effects in lazy expression arms are pending";
            if (!validation_rejected(status, log, diagnostic))
                validation_fail("quantum effect diagnostic failed case=%u level=%s\n%s",
                                profile,
                                levels[level],
                                log);
            free(log);
            ++count;
        }
    checks += count;
    printf(
        "PASS %u quantum effect diagnostics: undefined unsequenced accesses, pending lazy effects and expression measurement at five GCC levels\n",
        count);
}
