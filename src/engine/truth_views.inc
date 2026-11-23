/* Identify a pure value consumed as truth, negation, or a zero comparison.
 * Type/range/definedness analysis precedes this lowering. */
static int ar_truth_input(ar_builder *p, int e, int *invert) {
    int op = cj_get(p->d, e, "op"), nodes[3];
    unsigned count = expr_nodes(p->d, e, nodes);
    *invert = 0;
    if (count == 1 &&
        (cj_equal(p->d, op, "not") || (cj_equal(p->d, op, "cast") && p->ranges[e].width == 1))) {
        *invert = cj_equal(p->d, op, "not");
        return nodes[0];
    }
    if (count == 2 && (cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne")))
        for (unsigned side = 0; side < 2; ++side) {
            ar_range zero = p->ranges[nodes[1 - side]];
            if (zero.lo == 0 && zero.hi == 0) {
                *invert = cj_equal(p->d, op, "eq");
                return nodes[side];
            }
        }
    return -1;
}

/* Truth of a shift needs a zero test of only the surviving source bits.
 * Count patterns are disjoint. Starting at one, XOR each selected all-zero
 * predicate into one private bit, without a full-width shifted word. */
static int ar_shift_truth(ar_builder *p, int e, ar_value *out) {
    int invert, value = ar_truth_input(p, e, &invert);
    if (value < 0)
        return 0;
    int op = cj_get(p->d, value, "op"), args[3];
    int left = cj_equal(p->d, op, "shl");
    if ((!left && !cj_equal(p->d, op, "shr")) || expr_nodes(p->d, value, args) != 2)
        return 0;
    ar_value source, count;
    if (!ar_lower(p, args[0], 64, &source) || !ar_lower(p, args[1], 64, &count) ||
        !ar_temp(p, 1, out) || !ar_gate(p, ar_wire(out->bit[0]), qc_mask_zero(), qc_mask_zero()))
        return -1;
    unsigned width = p->ranges[args[0]].width, positions = width;
    if (count.width < 6 && positions > (1u << count.width))
        positions = 1u << count.width;
    ar_bit bits[128];
    int wanted[128];
    for (unsigned bit = 0; bit < count.width; ++bit)
        bits[bit] = count.bit[bit];
    for (unsigned distance = 0; distance < positions; ++distance) {
        for (unsigned bit = 0; bit < count.width; ++bit)
            wanted[bit] = !!((uint64_t)distance & (UINT64_C(1) << bit));
        unsigned controls = count.width;
        for (unsigned bit = 0; bit < width - distance; ++bit) {
            bits[controls] = ar_at(&source, left ? bit : bit + distance);
            wanted[controls++] = 0;
        }
        if (!ar_condition(p, ar_wire(out->bit[0]), bits, wanted, controls))
            return -1;
    }
    out->bit[0].inverted ^= invert;
    return 1;
}
