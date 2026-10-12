/* Complete states of compiled control joins, including every original basis
 * component and every private wire. Expected values come from the C cases. */
static void coherent_documents(validation *v) {
    const char *bodies[] = {"if(c)x=false;",
                            "if(c){x=false;phase(.23);}else{x=!x;phase(.41);}",
                            "if(x)x=false;",
                            "if(c){if(x)x=false;else x=true;}",
                            "int d=1;if(c&&d)x=false;++d;",
                            "int d=0;if(c){if(!c?1/d:x)x=false;}",
                            "erase_if(c,x);",
                            "int d=1;if(c+d)phase(.23);d=0;",
                            "int d=1;if(c<d)x+=1;d=0;",
                            "if(c)x=false;if(!c)x=!x;",
                            "if(c){x=false;c=!c;}"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], path[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/coherent.c", v->temp);
            validation_path(path, sizeof path, "%s/coherent.json", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program,
                sizeof program,
                "#include <quantum.h>\n"
                "static void erase_if(quant bool c,quant bool x){if(c)x=false;}\n"
                "int main(void){quant bool c=superposition(0,1);quant bool x=superposition(0,1);"
                "if(x)phase(.19);%s if(x)phase(.31);measure(x);measure(c);}\n",
                bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", path, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("coherent history compilation failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            char *ir = validation_read(path);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("invalid coherent history document");
            char rows[8192] = "[";
            size_t length = 1;
            unsigned index = 0, bindings = 0;
            uint32_t result_id = UINT32_MAX;
            int list = cj_get(&d, 0, "instructions");
            for (int token = list + 1; token < d.tokens[list].next;
                 token = d.tokens[token].next, ++index) {
                if (cj_equal(&d, cj_get(&d, token, "op"), "qbind")) {
                    int parameters = cj_get(&d, token, "parameters");
                    if (d.tokens[parameters].children != 1)
                        validation_fail("unexpected coherent history CPU descriptor");
                    int written = snprintf(rows + length,
                                           sizeof rows - length,
                                           "%s{\"instruction\":%u,\"values\":[%u]}",
                                           bindings++ ? "," : "",
                                           index,
                                           profile == 5 ? 0u : 1u);
                    if (written < 0 || (size_t)written >= sizeof rows - length)
                        validation_fail("coherent binding row budget exceeded");
                    length += (size_t)written;
                }
                if (result_id == UINT32_MAX && cj_equal(&d, cj_get(&d, token, "op"), "qmeasure")) {
                    uint64_t id;
                    if (!cj_u64(&d, cj_get(&d, token, "id"), &id))
                        validation_fail("invalid coherent output identifier");
                    result_id = (uint32_t)id;
                }
            }
            rows[length++] = ']';
            rows[length] = 0;
            cj_free(&d);
            if (result_id == UINT32_MAX)
                validation_fail("missing coherent history measurement");
            for (unsigned tables = 0; tables < 2; ++tables) {
                qc_context *c = qc_create(1, 7);
                if (!c)
                    validation_fail("coherent history context allocation failed");
                qc_allow_tables(c, tables);
                char *bound = bindings ? qc_bind_document(c, ir, rows) : NULL;
                if ((bindings && !bound) || !qc_document(c, bound ? bound : ir, 0, 1))
                    validation_fail(
                        "coherent history execution failed profile=%u level=%s tables=%u: %s",
                        profile,
                        levels[level],
                        tables,
                        qc_error(c));
                qc_register *out = qc_lookup(c, result_id);
                size_t indices[4];
                double angles[4];
                for (unsigned sample = 0; sample < 4; ++sample) {
                    unsigned control = sample & 1, old = sample >> 1;
                    unsigned result = profile == 1 || profile == 9 ? (control ? 0 : !old)
                                      : profile == 2               ? 0
                                      : profile == 3               ? old ^ control
                                      : profile == 7               ? old
                                      : profile == 8               ? (control ? old : 1)
                                                                   : (control ? 0 : old);
                    indices[sample] = (size_t)control << qc_lookup(c, 0)->wires[0];
                    if (old)
                        indices[sample] |= (size_t)1 << qc_lookup(c, 1)->wires[0];
                    if (result)
                        indices[sample] |= (size_t)1 << out->wires[0];
                    angles[sample] = (old ? .19 : 0) + (result ? .31 : 0);
                    if (profile == 1)
                        angles[sample] += control ? .23 : .41;
                    if (profile == 7)
                        angles[sample] += .23;
                }
                for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                    double complex expected = 0;
                    for (unsigned sample = 0; sample < 4; ++sample)
                        if (basis == indices[sample])
                            expected = cexp(I * angles[sample]) / 2;
                    if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                        validation_fail(
                            "coherent C state mismatch profile=%u level=%s tables=%u basis=%zu",
                            profile,
                            levels[level],
                            tables,
                            basis);
                }
                if (c->reset || c->measure != 2 || ((profile == 7) != !c->history_peak))
                    validation_fail("coherent history channels/resources disagree with source");
                for (size_t i = 0; i < c->nregisters; ++i)
                    if (c->registers[i].alive && c->registers[i].history &&
                        c->registers[i].definition)
                        validation_fail(
                            "coherent predicate or intermediate history was not uncomputed");
                free(bound);
                qc_destroy(c);
                ++checks;
            }
            free(ir);
        }
    puts(
        "PASS 110 coherent C complete states: one/two arms, self controls, nesting, CPU snapshots, lazy definedness, functions and successive joins at five GCC levels with both synthesis policies");
}

static void coherent_word_documents(validation *v) {
    const char *bodies[] = {
        "quant uint8_t x=0;if(c)x=7;if(x&2)phase(.31);measure(x);measure(c);",
        "quant int8_t x=-7;if(c)x=-1;if(x < -3)phase(.31);measure(x);measure(c);",
        "quant uint8_t values[1]={7};quant uint8_t *p=&values[0];if(values[0]&2)phase(.19);if(c)*p&=1;if(values[0]&2)phase(.31);measure(*p);measure(c);"};
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    for (unsigned profile = 0; profile < 3; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], path[4096], compiler[4096], program[8192];
            validation_path(source, sizeof source, "%s/word.c", v->temp);
            validation_path(path, sizeof path, "%s/word.json", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program,
                sizeof program,
                "#include <quantum.h>\nint main(void){quant bool c=superposition(0,1);%s}\n",
                bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", path, NULL};
            if (validation_run(v, command, NULL, 60))
                validation_fail("coherent word compilation failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            char *ir = validation_read(path);
            cj_doc d = {0};
            if (!cj_parse(&d, ir))
                validation_fail("invalid coherent word document");
            uint32_t result_id = UINT32_MAX;
            int list = cj_get(&d, 0, "instructions");
            for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next)
                if (cj_equal(&d, cj_get(&d, token, "op"), "qmeasure")) {
                    uint64_t id;
                    if (!cj_u64(&d, cj_get(&d, token, "id"), &id))
                        validation_fail("invalid coherent word output");
                    result_id = (uint32_t)id;
                    break;
                }
            cj_free(&d);
            if (result_id == UINT32_MAX)
                validation_fail("missing coherent word measurement");
            for (unsigned tables = 0; tables < 2; ++tables) {
                qc_context *c = qc_create(1, 7);
                if (!c)
                    validation_fail("coherent word context allocation failed");
                qc_allow_tables(c, tables);
                if (!qc_document(c, ir, 0, 1))
                    validation_fail(
                        "coherent word execution failed profile=%u level=%s tables=%u: %s",
                        profile,
                        levels[level],
                        tables,
                        qc_error(c));
                qc_register *out = qc_lookup(c, result_id);
                size_t indices[2];
                double angles[2];
                for (unsigned control = 0; control < 2; ++control) {
                    unsigned result = profile == 0   ? (control ? 7 : 0)
                                      : profile == 1 ? (control ? 255 : 249)
                                                     : (control ? 1 : 7);
                    indices[control] = (size_t)control << qc_lookup(c, 0)->wires[0];
                    for (unsigned bit = 0; bit < out->width; ++bit)
                        if ((result >> bit) & 1)
                            indices[control] |= (size_t)1 << out->wires[bit];
                    angles[control] = profile == 0   ? (control ? .31 : 0)
                                      : profile == 1 ? (control ? 0 : .31)
                                                     : (control ? .19 : .50);
                }
                for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                    double complex expected = 0;
                    for (unsigned control = 0; control < 2; ++control)
                        if (basis == indices[control])
                            expected = cexp(I * angles[control]) / sqrt(2.0);
                    if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                        validation_fail(
                            "coherent word state mismatch profile=%u level=%s tables=%u basis=%zu",
                            profile,
                            levels[level],
                            tables,
                            basis);
                }
                if (c->reset || c->measure != 9 || c->history_peak != 9 || qc_lookup(c, 1)->alive)
                    validation_fail(
                        "coherent word cleanup/channels/resources disagree with source");
                qc_destroy(c);
                ++checks;
            }
            free(ir);
        }
    puts(
        "PASS 30 coherent word states: signed/unsigned eight-bit outputs, known-basis history restoration and fixed pointer/array aliases at five GCC levels with both synthesis policies");
}
