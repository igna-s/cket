/* Remainder by a small constant: accumulate weighted source bits modulo d.
 * Every update permutes 0..d-1 and fixes the unused accumulator encodings.
 * The source stays available; the recorded arithmetic plan later clears all
 * work by its exact inverse. No dividend truth table or observation is used. */
#define AR_SMALL_MODULUS 16u

static int
ar_modular_edge(ar_builder *p, const ar_value *value, unsigned from, unsigned to, ar_bit control) {
    unsigned difference = from ^ to, target = (unsigned)__builtin_ctz(difference);
    ar_bit bits[5];
    int wanted[5];
    unsigned count = 0;
    for (unsigned bit = 0; bit < value->width; ++bit) {
        if (bit == target)
            continue;
        bits[count] = value->bit[bit];
        wanted[count++] = !!(from & (1u << bit));
    }
    bits[count] = control;
    wanted[count++] = 1;
    return ar_condition(p, ar_wire(value->bit[target]), bits, wanted, count);
}

/* A Gray path swaps its endpoints while restoring every intermediate state. */
static int ar_modular_swap(
    ar_builder *p, const ar_value *value, unsigned first, unsigned last, ar_bit control) {
    unsigned path[5] = {first}, steps = 0;
    for (unsigned bit = 0; bit < value->width; ++bit) {
        if (!((first ^ last) & (1u << bit)))
            continue;
        path[steps + 1] = path[steps] ^ (1u << bit);
        if (!ar_modular_edge(p, value, path[steps], path[steps + 1], control))
            return 0;
        ++steps;
    }
    for (unsigned index = steps; index > 1; --index)
        if (!ar_modular_edge(p, value, path[index - 2], path[index - 1], control))
            return 0;
    return 1;
}

static int ar_modular_add(
    ar_builder *p, const ar_value *value, unsigned modulus, unsigned amount, ar_bit control) {
    unsigned char visited[AR_SMALL_MODULUS] = {0};
    if (!amount || (!qc_mask_any(control.wire) && !control.inverted))
        return 1;
    for (unsigned first = 0; first < modulus; ++first) {
        if (visited[first])
            continue;
        visited[first] = 1;
        unsigned next = (first + amount) % modulus;
        while (next != first) {
            visited[next] = 1;
            if (!ar_modular_swap(p, value, first, next, control))
                return 0;
            next = (next + amount) % modulus;
        }
    }
    return 1;
}

/* Returns zero when the constant is outside this kernel, minus one on error.
 * For signed a, raw = a + 2^w on the negative branch. Subtract 2^w mod d
 * from the raw residue, then select the nonpositive C remainder. This avoids
 * allocating a full-width magnitude or quotient, including for INT_MIN. */
static int ar_small_remainder(
    ar_builder *p, const ar_value *a, const ar_value *b, unsigned width, int sign, ar_value *out) {
    if (!ar_literal_value(b))
        return 0;
    uint64_t raw = 0;
    for (unsigned bit = 0; bit < b->width; ++bit)
        raw |= (uint64_t)b->bit[bit].inverted << bit;
    int negative = b->sign && !!(raw & (UINT64_C(1) << (b->width - 1)));
    uint64_t divisor = negative ? (~raw + 1) & bits_mask(b->width) : raw;
    if (divisor < 2 || divisor > AR_SMALL_MODULUS)
        return 0;
    unsigned modulus = (unsigned)divisor, residue_bits = 1;
    while ((1u << residue_bits) < modulus)
        ++residue_bits;
    ar_value residue;
    if (!ar_temp(p, residue_bits, &residue))
        return -1;
    unsigned weight = 1;
    for (unsigned bit = 0; bit < a->width; ++bit) {
        if (!ar_modular_add(p, &residue, modulus, weight, a->bit[bit]))
            return -1;
        weight = (weight * 2) % modulus;
    }
    unsigned result_bits = residue_bits + !!sign;
    if (result_bits > width)
        result_bits = width;
    if (!ar_temp(p, result_bits, out))
        return -1;
    out->sign = sign;
    for (unsigned source_negative = 0; source_negative < (a->sign ? 2u : 1u); ++source_negative) {
        for (unsigned value = 0; value < modulus; ++value) {
            unsigned adjusted = source_negative ? (value + modulus - weight) % modulus : value;
            int result = source_negative && adjusted ? (int)adjusted - (int)modulus : (int)adjusted;
            uint64_t encoded = (uint64_t)(int64_t)result;
            ar_bit bits[5];
            int wanted[5];
            unsigned count = 0;
            for (unsigned bit = 0; bit < residue_bits; ++bit) {
                bits[count] = residue.bit[bit];
                wanted[count++] = !!(value & (1u << bit));
            }
            if (a->sign) {
                bits[count] = a->bit[a->width - 1];
                wanted[count++] = (int)source_negative;
            }
            for (unsigned bit = 0; bit < result_bits; ++bit)
                if ((encoded >> bit) & 1)
                    if (!ar_condition(p, ar_wire(out->bit[bit]), bits, wanted, count))
                        return -1;
        }
    }
    return 1;
}
