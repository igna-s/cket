// Eight-bit values exercise CPU joins through scalar, array and parameter aliases.
static void cpu_word_history_documents(validation *v) {
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    unsigned compiled = 0, executed = 0, states = 0;
    for (unsigned profile = 0; profile < 3; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], document[4096], executable[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/cpu-word.c", v->temp);
            validation_path(document, sizeof document, "%s/cpu-word.json", v->temp);
            validation_path(executable, sizeof executable, "%s/cpu-word", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            const char *target = profile == 1 ? "*p" : "x";
            snprintf(program,
                     sizeof program,
                     "#include <quantum.h>\n"
                     "void trim(quant uint8_t x){x%%=2;}\n"
                     "int main(int argc,char **argv){int flag=argc>1&&argv[1][0]=='1';"
                     "quant bool n=superposition(0,1);%s"
                     "if(n)phase(.11);if(flag){%s}else{%s^=6;}"
                     "if(n)phase(.17);if(%s)phase(.31);"
                     "unsigned result=measure(%s),input=measure(n);"
                     "return result!=(flag?1u:5u)||input>1;}\n",
                     profile == 1 ? "quant uint8_t a[1]={3};quant uint8_t *p=&a[0];"
                                  : "quant uint8_t x=3;",
                     profile == 2   ? "trim(x);"
                     : profile == 1 ? "*p%=2;"
                                    : "x%=2;",
                     target,
                     target,
                     target);
            validation_write(source, program);
            char *emit[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", document, NULL};
            char *build[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, emit, NULL, 60) || validation_run(v, build, NULL, 60))
                validation_fail("CPU word compilation failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            ++compiled;
            char *ir = validation_read(document);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("CPU word IR parse failed");
            int list = cj_get(&d, 0, "instructions");
            for (unsigned flag = 0; flag < 2; ++flag) {
                char argument[] = {(char)('0' + flag), 0};
                char *run[] = {executable, argument, NULL};
                if (validation_run(v, run, NULL, 60))
                    validation_fail("CPU word execution failed profile=%u level=%s flag=%u\n%s",
                                    profile,
                                    levels[level],
                                    flag,
                                    validation_read(v->log));
                ++executed;
                char bindings[8192];
                size_t used = 0;
                unsigned instruction = 0;
                bindings[used++] = '[';
                for (int token = list + 1; token < d.tokens[list].next;
                     token = d.tokens[token].next, ++instruction)
                    if (cj_equal(&d, cj_get(&d, token, "op"), "qbind")) {
                        int parameters = cj_get(&d, token, "parameters");
                        if (d.tokens[parameters].children != 1)
                            validation_fail("CPU word binding requires one branch decision");
                        int size = snprintf(bindings + used,
                                            sizeof bindings - used,
                                            "%s{\"instruction\":%u,\"values\":[%u]}",
                                            used > 1 ? "," : "",
                                            instruction,
                                            flag);
                        if (size < 0 || (size_t)size >= sizeof bindings - used - 2)
                            validation_fail("CPU word binding buffer exhausted");
                        used += (size_t)size;
                    }
                bindings[used++] = ']';
                bindings[used] = 0;
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 31);
                    if (!c)
                        validation_fail("CPU word context allocation failed");
                    qc_allow_tables(c, tables);
                    char *bound = qc_bind_document(c, ir, bindings);
                    char *trace = bound ? select_cpu_trace(bound, &flag, 1) : NULL;
                    if (!trace || !qc_document(c, trace, 0, 1))
                        validation_fail(
                            "CPU word state failed profile=%u level=%s flag=%u tables=%u: %s",
                            profile,
                            levels[level],
                            flag,
                            tables,
                            qc_error(c));
                    cj_doc selected = {0};
                    if (!cj_parse(&selected, trace))
                        validation_fail("CPU word selected trace parse failed");
                    int instructions = cj_get(&selected, 0, "instructions");
                    uint64_t output = UINT64_MAX;
                    for (int token = instructions + 1; token < selected.tokens[instructions].next;
                         token = selected.tokens[token].next)
                        if (cj_equal(&selected, cj_get(&selected, token, "op"), "qmeasure")) {
                            if (!cj_u64(&selected, cj_get(&selected, token, "id"), &output))
                                validation_fail("CPU word output identity missing");
                            break;
                        }
                    qc_register *owner = qc_lookup(c, (uint32_t)output), *n = qc_lookup(c, 0);
                    if (!owner || owner->width != 8 || !n || n->width != 1 || c->measure != 9 ||
                        c->reset)
                        validation_fail("CPU word output or explicit channel contract failed");
                    size_t value = 0;
                    unsigned expected = flag ? 1 : 5;
                    for (unsigned bit = 0; bit < 8; ++bit)
                        if ((expected >> bit) & 1)
                            value |= (size_t)1 << owner->wires[bit];
                    size_t other = value | ((size_t)1 << n->wires[0]);
                    for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                        double complex amplitude = basis == value   ? cexp(I * .31) / sqrt(2.0)
                                                   : basis == other ? cexp(I * .59) / sqrt(2.0)
                                                                    : 0;
                        if (cabs(qc_amplitude(c, basis) - amplitude) > 1e-10)
                            validation_fail(
                                "CPU word complete-state mismatch profile=%u level=%s flag=%u tables=%u basis=%zu",
                                profile,
                                levels[level],
                                flag,
                                tables,
                                basis);
                    }
                    cj_free(&selected);
                    free(trace);
                    free(bound);
                    qc_destroy(c);
                    ++states;
                }
            }
            cj_free(&d);
            free(ir);
        }
    checks += compiled + executed + states;
    printf(
        "PASS CPU word history: %u C programs, %u native paths and %u complete selected-trace states; known-basis eight-bit words, scalar/array/parameter aliases, five GCC levels and both synthesis policies\n",
        compiled,
        executed,
        states);
}
