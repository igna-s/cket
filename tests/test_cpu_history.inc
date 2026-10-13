#include "select_cpu_trace.inc"

/* Complete joint states come from independent C value/phase references. Native
 * executions separately check that GCC takes the supplied CPU paths and that
 * conditions with side effects are evaluated exactly once. */
static void cpu_history_documents(validation *v) {
    const char *bodies[] = {"if(k)x=false;",
                            "if(k)x=false;else x=true;",
                            "if(++reads&&k){x=false;k=0;}else{x=true;k=1;}",
                            "if(k)x=false;else x=!x;",
                            "if(k){if(second)x=false;else x=!x;}else x=true;",
                            "if(k){wipe(x);n=!n;}else x=!x;",
                            "if(k){x%=d;d=3;}else{x=false;d=4;}",
                            "if(k){if(n)x=false;else x=true;}else x=!x;"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    const char *native_results[] = {"((choice&1)&&result!=0)",
                                    "result!=!(choice&1)",
                                    "result!=!(choice&1)",
                                    "((choice&1)&&result!=0)",
                                    "(!(choice&1)?result!=1:second&&result!=0)",
                                    "((choice&1)&&result!=0)",
                                    "(!(choice&1)&&result!=0)",
                                    "((choice&1)&&result!=!input)"};
    unsigned compiled = 0, executed = 0, states = 0;
    for (unsigned profile = 0; profile < 8; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], path[4096], compiler[4096], program[8192], executable[4096];
            validation_path(source, sizeof source, "%s/cpu-history.c", v->temp);
            validation_path(path, sizeof path, "%s/cpu-history.json", v->temp);
            validation_path(executable, sizeof executable, "%s/cpu-history", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\n"
                            "static void wipe(quant bool x){x=false;}\n"
                            "int main(int argc,char **argv){"
                            "unsigned choice=argc>1?(unsigned)(argv[1][0]-'0'):0;"
                            "int k=choice&1,second=(choice>>1)&1,reads=0,d=2;"
                            "quant bool n=superposition(0,1),x=superposition(0,1);"
                            "if(n)phase(.11);if(x)phase(.19);%s"
                            "if(x)phase(.31);if(n)phase(.17);"
                            "unsigned result=measure(x),input=measure(n);"
                            "return result>1||input>1||reads!=%u||(%s);}\n",
                            bodies[profile],
                            profile == 2 ? 1u : 0u,
                            native_results[profile]);
            validation_write(source, program);
            char *emit[] = {compiler, source, "--emit-ir", (char *)levels[level], "-o", path, NULL};
            if (validation_run(v, emit, NULL, 60))
                validation_fail("CPU history IR failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            char *build[] = {compiler, source, (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, build, NULL, 60))
                validation_fail("CPU history native build failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            ++compiled;
            char *ir = validation_read(path);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("invalid CPU history IR");
            int list = cj_get(&d, 0, "instructions");
            unsigned variants = profile == 4 ? 4 : 2;
            for (unsigned variant = 0; variant < variants; ++variant) {
                unsigned control = variant & 1, second = variant >> 1;
                char argument[] = {(char)('0' + variant), 0};
                char *run[] = {executable, argument, NULL};
                if (validation_run(v, run, NULL, 60))
                    validation_fail(
                        "CPU history native execution failed profile=%u level=%s choice=%u\n%s",
                        profile,
                        levels[level],
                        variant,
                        validation_read(v->log));
                ++executed;
                char rows[8192] = "[";
                size_t length = 1;
                unsigned index = 0, bindings = 0;
                for (int token = list + 1; token < d.tokens[list].next;
                     token = d.tokens[token].next, ++index) {
                    if (!cj_equal(&d, cj_get(&d, token, "op"), "qbind"))
                        continue;
                    int parameters = cj_get(&d, token, "parameters"), descriptor = parameters + 1;
                    uint64_t id, bits;
                    if (d.tokens[parameters].children != 1 ||
                        !cj_u64(&d, cj_get(&d, descriptor, "id"), &id) ||
                        !cj_u64(&d, cj_get(&d, descriptor, "bits"), &bits))
                        validation_fail("unexpected CPU history descriptors");
                    unsigned value = bits != 1 ? 2u : profile == 4 && id == 0 ? second : control;
                    int size = snprintf(rows + length,
                                        sizeof rows - length,
                                        "%s{\"instruction\":%u,\"values\":[%u]}",
                                        bindings++ ? "," : "",
                                        index,
                                        value);
                    if (size < 0 || (size_t)size >= sizeof rows - length)
                        validation_fail("CPU binding output exceeded budget");
                    length += (size_t)size;
                }
                rows[length++] = ']';
                rows[length] = 0;
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("CPU history context allocation failed");
                    qc_allow_tables(c, tables);
                    char *bound = bindings ? qc_bind_document(c, ir, rows) : NULL;
                    if (bindings && !bound)
                        validation_fail("CPU history binding failed: %s", qc_error(c));
                    unsigned decisions[] = {control, second};
                    char *trace =
                        select_cpu_trace(bound ? bound : ir, decisions, profile == 4 ? 2 : 1);
                    if (!qc_document(c, trace, 0, 1))
                        validation_fail(
                            "CPU history state failed profile=%u level=%s choice=%u tables=%u: %s",
                            profile,
                            levels[level],
                            variant,
                            tables,
                            qc_error(c));
                    cj_doc selected = {0};
                    if (!cj_parse(&selected, trace))
                        validation_fail("CPU trace parse failed");
                    int instructions = cj_get(&selected, 0, "instructions");
                    uint64_t output = UINT32_MAX;
                    for (int token = instructions + 1; token < selected.tokens[instructions].next;
                         token = selected.tokens[token].next)
                        if (output == UINT32_MAX &&
                            cj_equal(&selected, cj_get(&selected, token, "op"), "qmeasure"))
                            if (!cj_u64(&selected, cj_get(&selected, token, "id"), &output))
                                validation_fail("CPU history output identifier failed");
                    cj_free(&selected);
                    qc_register *out = output <= UINT32_MAX ? qc_lookup(c, (uint32_t)output) : NULL;
                    if (!out)
                        validation_fail("CPU history output missing");
                    size_t indices[4];
                    double angles[4];
                    for (unsigned sample = 0; sample < 4; ++sample) {
                        unsigned old_n = sample & 1, old_x = sample >> 1;
                        unsigned result = profile == 0                   ? (control ? 0 : old_x)
                                          : profile == 1 || profile == 2 ? !control
                                          : profile == 4 ? (control ? (second ? 0 : !old_x) : 1)
                                          : profile == 6 ? (control ? old_x : 0)
                                          : profile == 7 ? (control ? !old_n : !old_x)
                                          : control      ? 0
                                                         : !old_x;
                        unsigned root_n = profile == 5 && control ? !old_n : old_n;
                        unsigned root_x =
                            ((profile == 3 || profile == 5 || profile == 7) && !control) ||
                                    (profile == 4 && control && !second)
                                ? !old_x
                                : old_x;
                        indices[sample] = ((size_t)root_n << qc_lookup(c, 0)->wires[0]) |
                                          ((size_t)root_x << qc_lookup(c, 1)->wires[0]) |
                                          ((size_t)result << out->wires[0]);
                        angles[sample] = old_n * .11 + old_x * .19 + result * .31 + root_n * .17;
                    }
                    for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                        double complex expected = 0;
                        for (unsigned sample = 0; sample < 4; ++sample)
                            if (basis == indices[sample])
                                expected += cexp(I * angles[sample]) / 2;
                        if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                            validation_fail(
                                "CPU history state mismatch profile=%u level=%s choice=%u tables=%u basis=%zu",
                                profile,
                                levels[level],
                                variant,
                                tables,
                                basis);
                    }
                    if (c->reset || c->measure != 2)
                        validation_fail("CPU history inserted an undeclared channel");
                    for (size_t i = 0; i < c->nregisters; ++i)
                        if (c->registers[i].alive && c->registers[i].history &&
                            c->registers[i].definition)
                            validation_fail("CPU history private inverse was not cleaned");
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
        "PASS CPU history: %u C programs, %u native paths and %u complete selected-trace states; five GCC levels and both synthesis policies\n",
        compiled,
        executed,
        states);
}
