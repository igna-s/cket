/* Arithmetic values are temporary wire views, not public quantum storage.
 * Equal mux columns can share a view; compute still copies each logical result
 * bit into its own destination wire before reversing the private plan. */
static int ar_same_bit(ar_bit a, ar_bit b) {
    return qc_mask_eq(a.wire, b.wire) && a.inverted == b.inverted;
}

static int ar_select_view(ar_builder *p,
                          const ar_value *yes,
                          const ar_value *no,
                          ar_bit condition,
                          unsigned width,
                          int sign,
                          ar_value *out) {
    *out = (ar_value){.width = width, .sign = sign};
    for (unsigned bit = 0; bit < width; ++bit) {
        ar_bit a = ar_at(yes, bit), b = ar_at(no, bit);
        if (ar_same_bit(a, b)) {
            out->bit[bit] = a;
            continue;
        }
        if (!qc_mask_any(a.wire) && !qc_mask_any(b.wire)) {
            out->bit[bit] = condition;
            out->bit[bit].inverted ^= b.inverted;
            continue;
        }
        unsigned previous;
        for (previous = 0; previous < bit; ++previous)
            if (ar_same_bit(a, ar_at(yes, previous)) && ar_same_bit(b, ar_at(no, previous)))
                break;
        if (previous < bit) {
            out->bit[bit] = out->bit[previous];
            continue;
        }
        ar_value column;
        if (!ar_temp(p, 1, &column))
            return 0;
        ar_bit controls[2] = {condition, a};
        int wanted[2] = {1, 1};
        if (!ar_condition(p, ar_wire(column.bit[0]), controls, wanted, 2))
            return 0;
        controls[1] = b;
        wanted[0] = 0;
        if (!ar_condition(p, ar_wire(column.bit[0]), controls, wanted, 2))
            return 0;
        out->bit[bit] = column.bit[0];
    }
    return 1;
}
