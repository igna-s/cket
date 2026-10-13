// Ownership transport must preserve amplitudes, physical wires and live inverses.
static void cpu_owner_contracts(void) {
    unsigned checked = 0;
    for (unsigned tables = 0; tables < 2; ++tables)
        for (unsigned choice = 0; choice < 2; ++choice)
            for (unsigned profile = 0; profile < 4; ++profile) {
                qc_context *c = qc_create(1, 23);
                if (!c)
                    validation_fail("CPU owner context allocation failed");
                qc_allow_tables(c, tables);
                apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
                apply(c,
                      profile == 2 ? "{\"op\":\"qprepare\",\"id\":0,\"lower\":1,\"upper\":1}"
                                   : "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
                if (profile != 1) {
                    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
                }
                char operation[2048];
                unsigned source = profile == 1 || profile == 2 ? 0 : choice ? 0 : 1;
                if (profile != 1) {
                    apply(
                        c,
                        "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false,\"ancilla\":true}");
                    snprintf(
                        operation,
                        sizeof operation,
                        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"not\",\"bits\":1,\"signed\":false,\"args\":[{\"id\":%u,\"bits\":1,\"signed\":false,\"op\":\"reg\"}]}%s}",
                        source,
                        profile == 3 ? (source ? ",\"controls\":[{\"id\":0,\"value\":1}]"
                                               : ",\"controls\":[{\"id\":1,\"value\":1}]")
                                     : "");
                    apply(c, operation);
                }
                if (profile == 2) {
                    apply(c, "{\"op\":\"qdispose\",\"id\":0}");
                    if (!qc_lookup(c, 0)->alive || !qc_lookup(c, 0)->finished)
                        validation_fail("CPU disposal discarded a live inverse input");
                    apply(c, "{\"op\":\"qdispose\",\"id\":2}");
                    if (qc_lookup(c, 0)->alive || qc_lookup(c, 2)->alive || c->measure || c->reset)
                        validation_fail(
                            "CPU deferred cleanup did not restore known storage unitarily");
                    size_t gates = c->ngates;
                    apply(c, "{\"op\":\"qdispose\",\"id\":0}");
                    apply(c, "{\"op\":\"qdispose\",\"id\":999}");
                    if (c->ngates != gates || cabs(qc_amplitude(c, 0) - 1 / sqrt(2.0)) > 2e-9 ||
                        cabs(qc_amplitude(c, 2) - 1 / sqrt(2.0)) > 2e-9)
                        validation_fail("CPU optional disposal changed the remaining state");
                } else {
                    unsigned wire = qc_lookup(c, source)->wires[0], physical = c->physical;
                    uint64_t epoch = qc_lookup(c, source)->epoch;
                    size_t gates = c->ngates, dimension = qc_state_size(c);
                    double complex *before = malloc(dimension * sizeof *before);
                    if (!before)
                        validation_fail("CPU owner amplitude snapshot allocation failed");
                    for (size_t i = 0; i < dimension; ++i)
                        before[i] = qc_amplitude(c, i);
                    unsigned first = profile == 1 && !choice ? 999 : 0;
                    unsigned second = profile == 1 ? (choice ? 999 : 0) : 1;
                    snprintf(
                        operation,
                        sizeof operation,
                        "{\"op\":\"qphi\",\"id\":4294967295,\"width\":1,\"signed\":false,\"sources\":[%u,%u],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":%u}}",
                        first,
                        second,
                        choice);
                    apply(c, operation);
                    qc_register *owner = qc_lookup(c, UINT32_MAX);
                    if (!owner || !owner->alive || owner->wires[0] != wire ||
                        owner->epoch != epoch || qc_lookup(c, source)->alive ||
                        c->physical != physical || c->ngates != gates)
                        validation_fail(
                            "CPU phi copied state or changed its physical representation");
                    for (size_t i = 0; i < dimension; ++i)
                        if (cabs(qc_amplitude(c, i) - before[i]) > 2e-9)
                            validation_fail("CPU phi changed a quantum amplitude");
                    free(before);
                    if (profile != 1) {
                        qc_register *borrower = qc_lookup(c, 2);
                        int relabeled = 0;
                        for (unsigned i = 0; i < borrower->ndeps; ++i)
                            relabeled |= borrower->deps[i] == UINT32_MAX;
                        if (!relabeled || !strstr(borrower->definition, "4294967295"))
                            validation_fail("CPU phi did not relabel a saved inverse dependency");
                        apply(c, "{\"op\":\"qdispose\",\"id\":2}");
                        if (qc_lookup(c, 2)->alive)
                            validation_fail("CPU transferred inverse did not clean its output");
                    }
                    apply(c, "{\"op\":\"qdispose\",\"id\":4294967295}");
                    owner = qc_lookup(c, UINT32_MAX);
                    if (!owner->alive || !owner->history || !owner->finished || c->measure ||
                        c->reset)
                        validation_fail("CPU unknown output history was silently erased");
                }
                qc_destroy(c);
                ++checked;
            }
    const char *invalid[] = {
        "{\"op\":\"qphi\",\"id\":0,\"width\":1,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,0],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[999,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,1,999],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":2}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"reg\",\"bits\":1,\"signed\":false,\"id\":0}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":0,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":true,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":8,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,1],\"extra\":0,\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qdispose\",\"id\":999,\"extra\":0}",
        "{\"op\":\"qdispose\",\"id\":-1}",
        "{\"op\":\"qdispose\",\"id\":4294967296}",
        "{\"op\":\"qphi\",\"id\":2,\"width\":1,\"signed\":false,\"sources\":[0,1],\"condition\":{\"op\":\"const\",\"bits\":1,\"signed\":false,\"value\":1}}"};
    for (unsigned tables = 0; tables < 2; ++tables)
        for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
            qc_context *c = qc_create(1, 23);
            if (!c)
                validation_fail("CPU owner rejection context allocation failed");
            qc_allow_tables(c, tables);
            apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
            apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
            if (i + 1 == sizeof invalid / sizeof *invalid)
                apply(c, "{\"op\":\"qdispose\",\"id\":0}");
            size_t gates = c->ngates;
            if (qc_apply(c, invalid[i], NULL) || c->ngates != gates || c->physical != 2)
                validation_fail("CPU malformed owner contract altered physical state case=%u", i);
            qc_destroy(c);
            ++checked;
        }
    checks += checked;
    printf(
        "PASS %u CPU owner contracts: gate-free transfer, saved inverses, optional and deferred disposal, retained outputs and schema rejection\n",
        checked);
}
