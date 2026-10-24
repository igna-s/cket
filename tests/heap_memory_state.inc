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
            qc_mask basis = ((qc_mask)high << 64) | low;
            unsigned coin = (unsigned)((basis >> source_wire) & 1);
            uint64_t values[] = {coin, 0x9a, big ? (coin << 8) | 0x9a : 0x9a00 | coin};
            uint32_t ids[] = {a, b, out};
            qc_mask expected = 0;
            for (unsigned i = 0; i < 3; ++i) {
                qc_register *r = qc_lookup(c, ids[i]);
                for (unsigned bit = 0; bit < r->width; ++bit)
                    if ((values[i] >> bit) & 1)
                        expected |= QC_WIRE(r->wires[bit]);
            }
            require(basis == expected, "independent whole joint byte state");
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
