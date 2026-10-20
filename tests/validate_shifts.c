#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("shift operation failed: %s", qc_error(c));
}

#include "shift_state.inc"
#include "shift_truth_state.inc"

static void joint_states(void) {
    /* Every basis pair, relative phase, control branch and private wire is
     * compared, followed by an inverse restoring the original superposition. */
    for (unsigned variant = 0; variant < 10; ++variant)
        for (unsigned tables = 0; tables < 2; ++tables) {
            int left = variant == 0 || variant == 3 || variant == 5 || variant == 6 || variant == 8;
            int signed_input = variant == 4 || variant == 9;
            unsigned width = variant < 2 ? 4 : 2;
            unsigned coherent = variant == 6 || variant == 7;
            qc_context *c = qc_create(1, 7);
            if (!c)
                validation_fail("allocation failed");
            qc_allow_tables(c, tables);
            apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}");
            apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":2,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":3}");
            char operation[2400], expr[2000], input[700], count_expr[600];
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":2,\"width\":%u,\"signed\":false}",
                     width);
            apply(c, operation);
            if (coherent) {
                apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":1,\"signed\":false}");
                apply(c, "{\"op\":\"qprepare\",\"id\":3,\"lower\":0,\"upper\":1}");
            }
            if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .37))
                validation_fail("phase failed");
            const char *leaf = "{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}";
            if (signed_input)
                snprintf(input,
                         sizeof input,
                         "{\"op\":\"cast\",\"bits\":3,\"signed\":true,\"args\":[%s]}",
                         leaf);
            else
                snprintf(input, sizeof input, "%s", leaf);
            unsigned promoted = variant == 5 ? 8 : variant == 2 ? 64 : 4;
            int sign = signed_input || variant == 5;
            if (variant >= 8)
                snprintf(
                    count_expr,
                    sizeof count_expr,
                    "{\"op\":\"band\",\"bits\":3,\"signed\":false,\"args\":[%s,{\"op\":\"const\",\"bits\":3,\"signed\":false,\"value\":3}]}",
                    leaf);
            else
                snprintf(count_expr,
                         sizeof count_expr,
                         "{\"op\":\"reg\",\"id\":1,\"bits\":2,\"signed\":false}");
            snprintf(
                expr,
                sizeof expr,
                "{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[{\"op\":\"cast\",\"bits\":%u,\"signed\":%s,\"args\":[%s]},{\"op\":\"cast\",\"bits\":64,\"signed\":false,\"args\":[%s]}]}]}",
                width,
                left ? "shl" : "shr",
                promoted,
                sign ? "true" : "false",
                promoted,
                sign ? "true" : "false",
                input,
                count_expr);
            const char *control = coherent ? ",\"controls\":[{\"id\":3,\"value\":0}]" : "";
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s%s}",
                     expr,
                     control);
            apply(c, operation);
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                int x = (int)(i & 7), count = (int)((i >> 3) & 3);
                if (variant >= 8)
                    count = x & 3;
                if (signed_input && x >= 4)
                    x -= 8;
                /* Independent arithmetic reference for negative right shift. */
                int result = left    ? x * (1 << count)
                             : x < 0 ? -((-x + (1 << count) - 1) / (1 << count))
                                     : x / (1 << count);
                unsigned value = (unsigned)result & ((1u << width) - 1);
                unsigned flag = coherent ? (unsigned)((i >> (5 + width)) & 1) : 0;
                if (flag)
                    value = 0;
                double complex expected = 0;
                if (((i >> 5) & ((1u << width) - 1)) == value && !(i >> (5 + width + coherent)))
                    expected = cexp(I * ((i & 1) ? .37 : 0)) / sqrt(coherent ? 64.0 : 32.0);
                if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                    validation_fail(
                        "joint shift mismatch variant=%u tables=%u basis=%zu", variant, tables, i);
            }
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"quncompute\",\"id\":2,\"expr\":%s%s}",
                     expr,
                     control);
            apply(c, operation);
            apply(c, "{\"op\":\"qrelease\",\"id\":2}");
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                size_t flags = coherent ? ((size_t)1 << (5 + width)) : 0;
                double complex expected =
                    !(i & ~((size_t)31 | flags))
                        ? cexp(I * ((i & 1) ? .37 : 0)) / sqrt(coherent ? 64.0 : 32.0)
                        : 0;
                if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                    validation_fail("shift inverse left garbage");
            }
            qc_destroy(c);
            ++checks;
        }
    puts(
        "PASS shift joint states: 20 domains with relative phase, narrowing, signed extension, correlated count, open control and clean inverse");
}

static void exports_and_rejections(void) {
    for (unsigned width = 8; width <= 32; width *= 2)
        for (unsigned left = 0; left < 2; ++left) {
            qc_context *c = qc_create(0, 7);
            qc_allow_tables(c, 0);
            char operation[1600];
            unsigned count_bits = width == 8 ? 3 : width == 16 ? 4 : 5;
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",
                     width);
            apply(c, operation);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":%llu}",
                     (unsigned long long)((UINT64_C(1) << width) - 1));
            apply(c, operation);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":1,\"width\":%u,\"signed\":false}",
                     count_bits);
            apply(c, operation);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":%u}",
                     width - 1);
            apply(c, operation);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":2,\"width\":%u,\"signed\":false}",
                     width);
            apply(c, operation);
            snprintf(
                operation,
                sizeof operation,
                "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":false},{\"op\":\"reg\",\"id\":1,\"bits\":%u,\"signed\":false}]}}",
                left ? "shl" : "shr",
                width,
                width,
                count_bits);
            int ok = qc_apply(c, operation, NULL);
            if (!ok)
                validation_fail("wide no-table shift: %s", qc_error(c));
            if (!c->ngates || qc_qubits(c) > 3 * width + count_bits)
                validation_fail("no shift circuit generated");
            printf("PASS %u-bit variable %s: %s (%zu gates)\n",
                   width,
                   left ? "left shift" : "right shift",
                   "no-table circuit generated",
                   c->ngates);
            qc_destroy(c);
            ++checks;
        }

    const struct {
        const char *operation;
        const char *a;
        const char *b;
    } invalid[] = {{"shl",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":-1}",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":1}"},
                   {"shl",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":7}",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":1}"},
                   {"shr",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":1}",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":-1}"},
                   {"shr",
                    "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":1}",
                    "{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":4}"}};

    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i)
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(0, 7);
            qc_allow_tables(c, tables);
            apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":true}");
            char operation[1200];
            snprintf(
                operation,
                sizeof operation,
                "{\"op\":\"qcompute\",\"id\":0,\"expr\":{\"op\":\"%s\",\"bits\":4,\"signed\":true,\"args\":[%s,%s]}}",
                invalid[i].operation,
                invalid[i].a,
                invalid[i].b);
            if (qc_apply(c, operation, NULL) || !*qc_error(c))
                validation_fail("undefined shift accepted case=%u", i);
            qc_destroy(c);
            ++checks;
        }
    for (unsigned variant = 0; variant < 3; ++variant)
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(0, 7);
            qc_allow_tables(c, tables);
            apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}");
            apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":4,\"signed\":true}");
            const char *source = "{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}";
            char operation[1600], a[600], b[600];
            if (variant == 2)
                snprintf(a,
                         sizeof a,
                         "{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[%s]}",
                         source);
            else
                snprintf(a, sizeof a, "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":1}");
            if (variant == 2)
                snprintf(b, sizeof b, "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":1}");
            else if (variant == 1)
                snprintf(b,
                         sizeof b,
                         "{\"op\":\"cast\",\"bits\":3,\"signed\":true,\"args\":[%s]}",
                         source);
            else
                snprintf(b, sizeof b, "%s", source);
            snprintf(
                operation,
                sizeof operation,
                "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"%s\",\"bits\":4,\"signed\":true,\"args\":[%s,%s]}}",
                variant == 2 ? "shl" : "shr",
                a,
                b);
            if (qc_apply(c, operation, NULL) || !*qc_error(c))
                validation_fail("undefined superposed shift accepted case=%u", variant);
            qc_destroy(c);
            ++checks;
        }
    puts(
        "PASS shift definedness: negative/oversized counts, negative left operand and signed overflow rejected in both routes");
}

static void frontend(validation *v) {
    const char *program =
        "#include <quantum.h>\nquant uint8_t shift_right(quant const bool x,quant const bool n){return (x>>n)+1;}\nint main(void){quant bool a[2]={superposition(0,1),superposition(0,1)};for(unsigned i=0;i<2;++i)a[i]^=true;quant uint8_t r=shift_right(a[0],a[1]);unsigned rr=measure(r);reset(r);quant uint8_t l=(unsigned)a[0]<<a[1];unsigned ll=measure(l);reset(l);quant int8_t negative=3*(int)a[0]-2;quant uint8_t nr=negative>>a[1];unsigned nn=measure(nr);int neg=measure(negative);unsigned x=measure(a[0]),n=measure(a[1]);return rr!=(x>>n)+1||ll!=(x<<n)||nn!=(unsigned)((neg>>n)&255)||neg!=3*(int)x-2;}\n";
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    char source[4096], compiler[4096], artifact[4096];
    validation_path(source, sizeof source, "%s/shifts.c", v->temp);
    validation_write(source, program);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(artifact, sizeof artifact, "%s/shifts-program", v->temp);
    for (unsigned level = 0; level < 5; ++level) {
        char *command[] = {
            compiler, source, "--run", "--seed", "7", (char *)levels[level], "-o", artifact, NULL};
        if (validation_run(v, command, NULL, 60)) {
            char *log = validation_read(v->log);
            validation_fail("combined shift program failed %s: %s", levels[level], log);
        }
        ++checks;
    }
    puts(
        "PASS shift C integration: functions, array, casts, arithmetic and explicit measurement at five GCC optimization levels");
    char backend[4096], resources[4096], wide[800];
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v->root);
    validation_path(resources, sizeof resources, "%s/resources.txt", v->temp);
    for (unsigned left = 0; left < 2; ++left)
        for (unsigned level = 0; level < 5; ++level) {
            snprintf(
                wide,
                sizeof wide,
                "#include <quantum.h>\nint main(void){quant uint16_t x=superposition(0,65535);quant uint8_t n=superposition(0,15);quant uint16_t r=(unsigned)x%s n;measure(r);measure(x);measure(n);return 0;}\n",
                left ? "<<" : ">>");
            validation_write(source, wide);
            char *compile[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", artifact, NULL};
            char *synthesize[] = {backend, "--resources", artifact, "--no-tables", NULL};
            if (validation_run(v, compile, NULL, 60) ||
                validation_run(v, synthesize, resources, 60)) {
                char *log = validation_read(v->log);
                validation_fail("20-input-bit frontend shift export failed: %s", log);
            }
            ++checks;
        }
    puts(
        "PASS shift C synthesis: both 16-bit shifts with 20 relevant input bits, no tables, five GCC optimization levels (export only)");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_shifts");
    joint_states();
    wide_sparse_states();
    shift_truth_states();
    exports_and_rejections();
    validation v;
    validation_init(&v, argv[0]);
    frontend(&v);
    validation_cleanup(&v);
    printf(
        "%u shift checks passed; generated wide circuits are distinct from state-validated small domains.\n",
        checks);
    return 0;
}
