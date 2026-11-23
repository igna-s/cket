#include <math.h>
#include <quantum.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t f32_bits(float v) {
    uint32_t bits = 0;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

static uint64_t f64_bits(double v) {
    uint64_t bits = 0;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

int main(void) {
    quant bool branch = superposition(0, 1);
    quant float fx = branch ? 3.0f : -1.5f;
    quant float fy = branch ? 4.0f : 2.0f;
    quant float hypot_f = sqrtf(fx * fx + fy * fy);
    quant double dx = branch ? 3.0 : -1.5;
    quant double dy = branch ? 4.0 : 2.0;
    quant double span = fdim(fmax(dx, dy), fmin(dx, dy));
    quant bool ordered_ok =
        isfinite(hypot_f) && !isnan(hypot_f) && !isinf(hypot_f) && isgreater(hypot_f, 2.0f);
    quant int sbit = signbit(dx);

    quant float sf = branch ? 1.5f : -0.5f;
    sf++;
    quant float df = branch ? 3.25f : 1.25f;
    --df;
    quant float sc = copysignf(sf, branch ? -1.0f : 1.0f);
    quant float fl_f = floorf(df / 2.0f);
    quant float ce_f = ceilf(df / 2.0f);
    quant float tr_f = truncf(sc);
    quant float rd_f = roundf(sc);

    quant bool kick = superposition(0, 1);
    if (isless(kick ? 1.5f : 4.5f, 3.0f))
        phase(3.14159265358979323846);
    fourier(kick);
    unsigned interference = measure(kick);

    unsigned b = measure(branch);
    float got_hypot = measure(hypot_f);
    double got_span = measure(span);
    float got_sf = measure(sf);
    float got_df = measure(df);
    float got_sc = measure(sc);
    float got_fl = measure(fl_f);
    float got_ce = measure(ce_f);
    float got_tr = measure(tr_f);
    float got_rd = measure(rd_f);
    int got_sbit = measure(sbit);
    unsigned got_ord = measure(ordered_ok);
    reset(fx);
    reset(fy);
    reset(dx);
    reset(dy);

    float wfx = b ? 3.0f : -1.5f;
    float wfy = b ? 4.0f : 2.0f;
    float want_hypot = sqrtf(wfx * wfx + wfy * wfy);
    double cx = b ? 3.0 : -1.5;
    double cy = b ? 4.0 : 2.0;
    double want_span = fdim(fmax(cx, cy), fmin(cx, cy));
    float want_sf = (b ? 1.5f : -0.5f) + 1.0f;
    float want_df = (b ? 3.25f : 1.25f) - 1.0f;
    float want_sc = copysignf(want_sf, b ? -1.0f : 1.0f);
    float want_fl = floorf(want_df / 2.0f);
    float want_ce = ceilf(want_df / 2.0f);
    float want_tr = truncf(want_sc);
    float want_rd = roundf(want_sc);
    int want_sbit = signbit(cx) != 0;
    unsigned want_ord = isfinite(want_hypot) && !isnan(want_hypot) && !isinf(want_hypot) &&
                        isgreater(want_hypot, 2.0f);

    printf("branch=%u hypot=%.2f span=%.2f sc=%.2f fl=%.0f ce=%.0f tr=%.0f rd=%.0f sbit=%d ord=%u "
           "interference=%u\n",
           b,
           (double)got_hypot,
           got_span,
           (double)got_sc,
           (double)got_fl,
           (double)got_ce,
           (double)got_tr,
           (double)got_rd,
           got_sbit ? 1 : 0,
           got_ord,
           interference);

    return b > 1 || f32_bits(got_hypot) != f32_bits(want_hypot) ||
           f64_bits(got_span) != f64_bits(want_span) || f32_bits(got_sf) != f32_bits(want_sf) ||
           f32_bits(got_df) != f32_bits(want_df) || f32_bits(got_sc) != f32_bits(want_sc) ||
           f32_bits(got_fl) != f32_bits(want_fl) || f32_bits(got_ce) != f32_bits(want_ce) ||
           f32_bits(got_tr) != f32_bits(want_tr) || f32_bits(got_rd) != f32_bits(want_rd) ||
           (got_sbit != 0) != want_sbit || got_ord != want_ord || interference != 1;
}
