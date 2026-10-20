/* Divide by a small literal without full-width magnitude/remainder words.
 * Factor d = 2^shift * modulus. For odd modulus, r -> 2r mod modulus is
 * reversible; combine it with a controlled modular increment per source bit.
 * Only the bounded residue encodings are enumerated, not the dividend values. */
static int ar_modular_double(ar_builder *p, const ar_value *value, unsigned modulus) {
    unsigned char visited[AR_SMALL_MODULUS] = {0};
    for (unsigned first = 0; first < modulus; ++first) {
        if (visited[first])
            continue;
        visited[first] = 1;
        unsigned next = (2 * first) % modulus;
        while (next != first) {
            visited[next] = 1;
            if (!ar_modular_swap(p, value, first, next, (ar_bit){0, 1}))
                return 0;
            next = (2 * next) % modulus;
        }
    }
    return 1;
}

/* For negative a, bit j of |a| is raw[j] XOR (sign AND any lower raw bit).
 * Express the latter as sign XOR (sign AND all lower bits zero). The fresh
 * one-bit target can be cleared by repeating this XOR network: its controls
 * refer only to unchanged source expressions. INT_MIN needs no signed abs. */
static int ar_magnitude_bit(ar_builder *p, const ar_value *a, unsigned j, unsigned target) {
    ar_bit raw = ar_at(a, j);
    int one = 1;
    if (!ar_condition(p, target, &raw, &one, 1))
        return 0;
    if (!a->sign || !j)
        return 1;
    ar_bit negative = a->bit[a->width - 1], controls[65];
    int wanted[65] = {1};
    if (!ar_condition(p, target, &negative, &one, 1))
        return 0;
    controls[0] = negative;
    for (unsigned bit = 0; bit < j; ++bit)
        controls[bit + 1] = a->bit[bit];
    return ar_condition(p, target, controls, wanted, j + 1);
}

static int ar_small_division(
    ar_builder *p, const ar_value *a, const ar_value *b, unsigned width, int sign, ar_value *out) {
    if (!ar_literal_value(b))
        return 0;
    uint64_t raw = 0;
    for (unsigned bit = 0; bit < b->width; ++bit)
        raw |= (uint64_t)b->bit[bit].inverted << bit;
    int negative_divisor = b->sign && !!(raw & (UINT64_C(1) << (b->width - 1)));
    uint64_t divisor = negative_divisor ? (~raw + 1) & bits_mask(b->width) : raw;
    if (divisor < 2 || divisor > AR_SMALL_MODULUS)
        return 0;
    unsigned shift = 0, modulus = (unsigned)divisor;
    while (!(modulus & 1)) {
        ++shift;
        modulus >>= 1;
    }
    if (modulus == 1)
        return 0; /* The existing power-of-two path handles this case first. */
    unsigned residue_bits = 1;
    while ((1u << residue_bits) < modulus)
        ++residue_bits;
    ar_value residue, magnitude;
    if (!ar_temp(p, residue_bits, &residue) || !ar_temp(p, 1, &magnitude) ||
        !ar_temp(p, width, out))
        return -1;
    out->sign = sign;
    for (unsigned remaining = a->width; remaining > shift; --remaining) {
        unsigned bit = remaining - 1, quotient_bit = bit - shift;
        if (!ar_magnitude_bit(p, a, bit, ar_wire(magnitude.bit[0])))
            return -1;
        if (quotient_bit < width) {
            for (unsigned value = 0; value < modulus; ++value) {
                if (2 * value + 1 < modulus)
                    continue;
                ar_bit controls[5];
                int wanted[5];
                unsigned count = residue_bits;
                for (unsigned j = 0; j < residue_bits; ++j) {
                    controls[j] = residue.bit[j];
                    wanted[j] = !!(value & (1u << j));
                }
                if (2 * value < modulus) {
                    controls[count] = magnitude.bit[0];
                    wanted[count++] = 1;
                }
                if (!ar_condition(p, ar_wire(out->bit[quotient_bit]), controls, wanted, count))
                    return -1;
            }
        }
        if (!ar_modular_double(p, &residue, modulus) ||
            !ar_modular_add(p, &residue, modulus, 1, magnitude.bit[0]) ||
            !ar_magnitude_bit(p, a, bit, ar_wire(magnitude.bit[0])))
            return -1;
    }
    ar_bit negative = a->sign ? a->bit[a->width - 1] : (ar_bit){0, 0};
    negative.inverted ^= negative_divisor;
    return ar_negate_if(p, out, negative) ? 1 : -1;
}

/* A quotient is nonzero exactly outside (-|d|,|d|). Only this fixed, small
 * zero-quotient interval is enumerated, never the full dividend domain.
 * Typed definedness was checked before lowering, including overflow guards. */
static int ar_small_division_truth(ar_builder *p, int e, ar_value *out) {
    int op = cj_get(p->d, e, "op"), nodes[3], division = -1, invert = 0;
    unsigned count = expr_nodes(p->d, e, nodes);
    if (count == 1 &&
        (cj_equal(p->d, op, "not") || (cj_equal(p->d, op, "cast") && p->ranges[e].width == 1)) &&
        cj_equal(p->d, cj_get(p->d, nodes[0], "op"), "div")) {
        division = nodes[0];
        invert = cj_equal(p->d, op, "not");
    } else if (count == 2 && (cj_equal(p->d, op, "eq") || cj_equal(p->d, op, "ne"))) {
        for (unsigned side = 0; side < 2; ++side) {
            ar_range zero = p->ranges[nodes[1 - side]];
            if (zero.lo == 0 && zero.hi == 0 &&
                cj_equal(p->d, cj_get(p->d, nodes[side], "op"), "div")) {
                division = nodes[side];
                invert = cj_equal(p->d, op, "eq");
                break;
            }
        }
    }
    if (division < 0)
        return 0;
    int args[3];
    if (expr_nodes(p->d, division, args) != 2)
        return 0;
    ar_range denominator = p->ranges[args[1]];
    __int128 magnitude = ar_abs(denominator.lo);
    if (denominator.lo != denominator.hi || magnitude < 2 || magnitude > AR_SMALL_MODULUS)
        return 0;
    ar_value input;
    if (!ar_lower(p, args[0], 64, &input) || !ar_temp(p, 1, out) ||
        !ar_gate(p, ar_wire(out->bit[0]), 0, 0))
        return -1;
    __int128 positive_limit = (__int128)1 << (input.width - !!input.sign);
    __int128 negative_limit = input.sign ? -positive_limit : 0;
    for (int value = 1 - (int)magnitude; value < (int)magnitude; ++value) {
        if (value < negative_limit || value >= positive_limit)
            continue;
        uint64_t encoded = (uint64_t)(int64_t)value & bits_mask(input.width);
        int wanted[64];
        for (unsigned bit = 0; bit < input.width; ++bit)
            wanted[bit] = !!(encoded & (UINT64_C(1) << bit));
        if (!ar_condition(p, ar_wire(out->bit[0]), input.bit, wanted, input.width))
            return -1;
    }
    out->bit[0].inverted ^= invert;
    return 1;
}
