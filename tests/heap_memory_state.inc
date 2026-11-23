/* The engine's byte-order flag is tested in both directions independently of
 * the host. Native C fixtures separately exercise the GCC-selected byte order. */
static void byte_memory_states(void) {
    for (int big = 0; big < 2; ++big) {
        qc_context *c = qc_create_sparse(7, 1024);
        require(c != NULL, "byte state context");
        uint64_t source = qc_heap_allocate(c, 2, 8, 0);
        uint64_t destination = qc_heap_allocate(c, 1, 16, 0);
        uint32_t a, b, out;
        require(source && destination && qc_heap_element(c, source, 0, &a) &&
                    qc_heap_element(c, source, 1, &b) && qc_heap_element(c, destination, 0, &out),
                "byte state owners");
        prepare(c, a, 0, 1);
        prepare(c, b, 0x9a, 0x9a);
        prepare(c, out, 0x7654, 0x7654);
        unsigned source_wire = qc_lookup(c, a)->wires[0];
        require(qc_emit(c, QC_PHASE, 0, QC_WIRE(source_wire), QC_WIRE(source_wire), .31),
                "byte source phase");
        char text[1000];
        snprintf(text,
                 sizeof text,
                 "{\"op\":\"qheap_memory\",\"mode\":\"copy\","
                 "\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
                 "\"heap_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
                 "\"source\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
                 "\"source_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
                 "\"count\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":2},"
                 "\"big_endian\":%s}",
                 (unsigned long long)destination,
                 (unsigned long long)source,
                 big ? "true" : "false");
        operation(c, text);
        require(qc_heap_element(c, destination, 0, &out), "written byte owner");
        require(qc_sparse_size(c) == 2, "byte copy support");
        for (size_t row = 0; row < qc_sparse_size(c); ++row) {
            uint64_t low, high;
            double complex amplitude;
            require(qc_sparse_entry(c, row, &low, &high, &amplitude), "byte state row");
            qc_mask basis = qc_mask_from_u128(low, high);
            unsigned coin = (unsigned)qc_mask_test(basis, source_wire);
            uint64_t values[] = {coin, 0x9a, big ? (coin << 8) | 0x9a : 0x9a00 | coin};
            uint32_t ids[] = {a, b, out};
            qc_mask expected = qc_mask_zero();
            for (unsigned i = 0; i < 3; ++i) {
                qc_register *r = qc_lookup(c, ids[i]);
                for (unsigned bit = 0; bit < r->width; ++bit)
                    if ((values[i] >> bit) & 1)
                        expected = qc_mask_or(expected, QC_WIRE(r->wires[bit]));
            }
            require(qc_mask_eq(basis, expected), "independent whole joint byte state");
            require(cabs(amplitude - cexp(I * (coin ? .31 : 0)) / sqrt(2)) < 1e-10,
                    "byte copy preserves complex phase");
        }
        require(!c->measure && !c->reset, "byte copy introduces no channel");
        qc_destroy(c);
    }
}

static void malformed_byte_memory(void) {
#define BYTE_U64(v) "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":" #v "}"
#define BYTE_HEADER                                                                                \
    "\"op\":\"qheap_memory\",\"mode\":\"set\",\"heap\":" BYTE_U64(1) ",\"heap_offset\":" BYTE_U64( \
        0) ",\"count\":" BYTE_U64(1)
#define BYTE_FILL "\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":7}"
    const char *cases[] = {
        "{" BYTE_HEADER "," BYTE_FILL ",\"big_endian\":false,\"extra\":0}",
        "{" BYTE_HEADER "," BYTE_FILL "}",
        "{" BYTE_HEADER "," BYTE_FILL ",\"big_endian\":0}",
        "{" BYTE_HEADER ",\"big_endian\":false}",
        "{" BYTE_HEADER
        ",\"expr\":{\"op\":\"const\",\"bits\":16,\"signed\":false,\"value\":7},\"big_endian\":false}",
        "{" BYTE_HEADER "," BYTE_FILL
        ",\"big_endian\":false,\"controls\":[{\"id\":99,\"value\":1}]}",
        "{" BYTE_HEADER "," BYTE_FILL ",\"big_endian\":false,\"source\":" BYTE_U64(1) "}",
    };
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
        qc_context *c = qc_create(1, 7);
        require(c && qc_heap_allocate(c, 1, 8, 0) == 1, "malformed byte fixture");
        size_t gates = c->ngates, registers = c->nregisters;
        unsigned physical = c->physical;
        require(!qc_apply(c, cases[i], NULL) && *qc_error(c), "malformed byte schema rejected");
        require(c->ngates == gates && c->nregisters == registers && c->physical == physical,
                "malformed byte schema rejected before synthesis/allocation");
        qc_destroy(c);
    }
#undef BYTE_FILL
#undef BYTE_HEADER
#undef BYTE_U64
}

static void bound_heap_byte_states(void) {
    for (int big = 0; big < 2; ++big) {
        qc_context *c = qc_create_sparse(7, 1024);
        uint64_t heap = c ? qc_heap_allocate(c, 1, 16, 0) : 0;
        uint32_t source;
        require(heap && qc_heap_element(c, heap, 0, &source), "bound byte source");
        prepare(c, source, 0, 1);
        unsigned wire = qc_lookup(c, source)->wires[0];
        require(qc_emit(c, QC_PHASE, 0, QC_WIRE(wire), QC_WIRE(wire), .31), "bound byte phase");
        operation(c, "{\"op\":\"qalloc\",\"id\":7,\"width\":8,\"signed\":false,\"ancilla\":true}");
        char text[1100];
        snprintf(
            text,
            sizeof text,
            "{\"op\":\"qbind\",\"instruction\":{\"op\":\"qcompute\",\"id\":7,\"expr\":{"
            "\"op\":\"heap_byte_read\",\"bits\":8,\"signed\":false,"
            "\"heap\":{\"op\":\"parameter\",\"id\":1,\"bits\":64,\"signed\":false},"
            "\"offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
            "\"index\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},\"big_endian\":%s}},"
            "\"parameters\":[{\"id\":1,\"bits\":64,\"signed\":false}]}",
            big ? "true" : "false");
        require(qc_apply_bound(c, text, &heap, 1, NULL), qc_error(c));
        for (unsigned inverse = 0; inverse < 2; ++inverse) {
            if (inverse) {
                require(qc_heap_free(c, heap), "retire captured byte source");
                operation(c, "{\"op\":\"quncompute\",\"id\":7,\"saved\":true}");
            }
            require(qc_sparse_size(c) == 2, "bound byte support");
            for (size_t row = 0; row < qc_sparse_size(c); ++row) {
                uint64_t low, high;
                double complex amplitude;
                require(qc_sparse_entry(c, row, &low, &high, &amplitude), "bound byte state row");
                qc_mask basis = qc_mask_from_u128(low, high);
                unsigned coin = (unsigned)qc_mask_test(basis, wire);
                qc_mask expected = coin ? QC_WIRE(wire) : qc_mask_zero();
                if (!big && !inverse && coin)
                    expected = qc_mask_or(expected, QC_WIRE(qc_lookup(c, 7)->wires[0]));
                require(qc_mask_eq(basis, expected), "independent bound byte joint state");
                require(cabs(amplitude - cexp(I * (coin ? .31 : 0)) / sqrt(2)) < 1e-10,
                        "bound byte read/inverse phase");
            }
        }
        require(!c->measure && !c->reset, "bound byte read introduces no channel");
        qc_destroy(c);
    }
}

static void malformed_transfer_memory(void) {
#define TRANSFER_U64(v) "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":" #v "}"
#define TRANSFER_FIELDS                                                                         \
    "\"heap\":" TRANSFER_U64(1) ",\"heap_offset\":" TRANSFER_U64(0) ",\"count\":" TRANSFER_U64( \
        1) ",\"big_endian\":false"
    const char *cases[] = {
        "{\"op\":\"qheap_memory\",\"mode\":\"bytes\"," TRANSFER_FIELDS "}",
        "{\"op\":\"qheap_memory\",\"mode\":\"bytes\"," TRANSFER_FIELDS ",\"bytes\":{}}",
        "{\"op\":\"qheap_memory\",\"mode\":\"bytes\"," TRANSFER_FIELDS
        ",\"bytes\":[{\"op\":\"const\",\"bits\":16,\"signed\":false,\"value\":7}]}",
        "{\"op\":\"qheap_memory\",\"mode\":\"bytes\"," TRANSFER_FIELDS
        ",\"bytes\":[{\"op\":\"const\",\"bits\":8,\"signed\":true,\"value\":7}]}",
        "{\"op\":\"qheap_memory\",\"mode\":\"bytes\"," TRANSFER_FIELDS
        ",\"bytes\":[],\"source\":" TRANSFER_U64(1) "}",
        "{\"op\":\"qheap_memory\",\"mode\":\"check\"," TRANSFER_FIELDS "}",
        "{\"op\":\"qheap_memory\",\"mode\":\"check\"," TRANSFER_FIELDS ",\"limit\":65}",
        "{\"op\":\"qheap_memory\",\"mode\":\"check\"," TRANSFER_FIELDS ",\"limit\":1,\"bytes\":[]}",
    };
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
        qc_context *c = qc_create(1, 7);
        require(c && qc_heap_allocate(c, 1, 8, 0) == 1, "transfer schema fixture");
        size_t registers = c->nregisters, gates = c->ngates;
        unsigned physical = c->physical;
        require(!qc_apply(c, cases[i], NULL) && *qc_error(c), "malformed transfer rejected");
        require(c->nregisters == registers && c->ngates == gates && c->physical == physical,
                "transfer schema rejected before synthesis/allocation");
        qc_destroy(c);
    }
#undef TRANSFER_FIELDS
#undef TRANSFER_U64
}

static void bool_and_quantum_count_memory(void) {
    qc_context *c = qc_create_sparse(7, 2048);
    require(c != NULL, "bool quantum count context");
    uint64_t bheap = qc_heap_allocate(c, 3, 1, 0);
    uint64_t sheap = qc_heap_allocate(c, 2, 8, 0);
    uint64_t pheap = qc_heap_allocate(c, 1, 24, 0);
    require(bheap && sheap && pheap, "bool and packed struct heap allocations");
    uint32_t b0, b1, s0, s1, p0;
    require(qc_heap_element(c, bheap, 0, &b0) && qc_heap_element(c, bheap, 1, &b1) &&
                qc_heap_element(c, sheap, 0, &s0) && qc_heap_element(c, sheap, 1, &s1) &&
                qc_heap_element(c, pheap, 0, &p0),
            "bool/packed elements");
    prepare(c, s0, 1, 1);
    prepare(c, s1, 0, 1);
    prepare(c, b0, 0, 1);
    prepare(c, p0, 0x112233, 0x112233);
    operation(c, "{\"op\":\"qalloc\",\"id\":10,\"width\":1,\"signed\":false}");
    operation(c, "{\"op\":\"qprepare\",\"id\":10,\"lower\":0,\"upper\":1}");
    operation(c, "{\"op\":\"qalloc\",\"id\":11,\"width\":64,\"signed\":false}");
    operation(c, "{\"op\":\"qprepare\",\"id\":11,\"lower\":0,\"upper\":3}");
    unsigned ctrl_wire = qc_lookup(c, 10)->wires[0];
    unsigned cnt_wire0 = qc_lookup(c, 11)->wires[0];
    unsigned cnt_wire1 = qc_lookup(c, 11)->wires[1];
    unsigned b0_wire = qc_lookup(c, b0)->wires[0];
    require(qc_emit(c, QC_PHASE, 0, QC_WIRE(ctrl_wire), QC_WIRE(ctrl_wire), .41),
            "ctrl inactive/active phase");
    require(qc_emit(c, QC_PHASE, 0, QC_WIRE(cnt_wire0), QC_WIRE(cnt_wire0), .29),
            "quantum count phase");
    require(qc_emit(c, QC_PHASE, 0, QC_WIRE(b0_wire), QC_WIRE(b0_wire), .17),
            "destination inactive byte phase");
    char text[1400];
    /* First copy into _Bool array unconditionally with fixed count 2 from byte array. */
    snprintf(text,
             sizeof text,
             "{\"op\":\"qheap_memory\",\"mode\":\"copy\","
             "\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
             "\"heap_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":1},"
             "\"source\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
             "\"source_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
             "\"count\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":2},"
             "\"big_endian\":false}",
             (unsigned long long)bheap,
             (unsigned long long)sheap);
    operation(c, text);
    /* Non-identity controlled copy from _Bool array into 24-bit packed struct element with quantum
     * count [0..3]. */
    snprintf(text,
             sizeof text,
             "{\"op\":\"qheap_memory\",\"mode\":\"copy\","
             "\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
             "\"heap_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
             "\"source\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
             "\"source_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
             "\"count\":{\"op\":\"reg\",\"id\":11,\"bits\":64,\"signed\":false},"
             "\"big_endian\":false,\"controls\":[{\"id\":10,\"value\":1}]}",
             (unsigned long long)pheap,
             (unsigned long long)bheap);
    operation(c, text);
    require(qc_heap_element(c, bheap, 1, &b1) && qc_heap_element(c, pheap, 0, &p0),
            "updated element handles");
    uint32_t b2;
    require(qc_heap_element(c, bheap, 2, &b2), "b2 element");
    require(qc_sparse_size(c) == 32, "exact basis support count");
    for (size_t row = 0; row < qc_sparse_size(c); ++row) {
        uint64_t words[16] = {0};
        double complex amp = 0;
        require(qc_sparse_entry_words(c, row, words, 16, &amp), "sparse row");
        qc_mask basis = {0};
        memcpy(basis.w, words, sizeof basis.w);
        unsigned ctrl = (unsigned)qc_mask_test(basis, ctrl_wire);
        uint64_t cnt = (unsigned)qc_mask_test(basis, cnt_wire0) +
                       2u * (unsigned)qc_mask_test(basis, cnt_wire1);
        unsigned bv0 = (unsigned)qc_mask_test(basis, qc_lookup(c, b0)->wires[0]);
        unsigned sv1 = (unsigned)qc_mask_test(basis, qc_lookup(c, s1)->wires[0]);
        unsigned bv1 = (unsigned)qc_mask_test(basis, qc_lookup(c, b1)->wires[0]);
        unsigned bv2 = (unsigned)qc_mask_test(basis, qc_lookup(c, b2)->wires[0]);
        require(bv1 == 1 && bv2 == sv1, "_Bool array copied exact low bits");
        uint64_t pv = 0;
        qc_register *pr = qc_lookup(c, p0);
        for (unsigned bit = 0; bit < 24; ++bit)
            pv |= (uint64_t)qc_mask_test(basis, pr->wires[bit]) << bit;
        uint64_t exp_pv = 0x112233u;
        if (ctrl) {
            if (cnt >= 1)
                exp_pv = (exp_pv & ~UINT64_C(0xff)) | bv0;
            if (cnt >= 2)
                exp_pv = (exp_pv & ~UINT64_C(0xff00)) | ((uint64_t)bv1 << 8);
            if (cnt >= 3)
                exp_pv = (exp_pv & ~UINT64_C(0xff0000)) | ((uint64_t)bv2 << 16);
        }
        require(pv == exp_pv, "non-identity controlled quantum-count byte copy exact payload");
        double exp_ang = .41 * ctrl + .29 * (cnt & 1u) + .17 * bv0;
        require(cabs(amp - cexp(I * exp_ang) / (4.0 * sqrt(2.0))) < 1e-10,
                "inactive and active sector phases exactly preserved");
    }
    /* Reject quantum count exceeding overlap / destination range. */
    snprintf(text,
             sizeof text,
             "{\"op\":\"qheap_memory\",\"mode\":\"copy\","
             "\"heap\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
             "\"heap_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":0},"
             "\"source\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":%llu},"
             "\"source_offset\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":1},"
             "\"count\":{\"op\":\"reg\",\"id\":11,\"bits\":64,\"signed\":false},"
             "\"big_endian\":false}",
             (unsigned long long)sheap,
             (unsigned long long)sheap);
    require(!qc_apply(c, text, NULL) && strstr(qc_error(c), "overlap"),
            "quantum count overlapping memcpy rejected");
    qc_destroy(c);
}
