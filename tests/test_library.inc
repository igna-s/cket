/* Library compatibility is interface-specific, not implied by including a
 * standard header. Compare C integer values and complete quantum states. */
static void library_cases(void) {
    const struct {
        const char *name, *source;
    } supported[] = {
        {"stdlib abs preserves a negative quantum source",
         "#include <quantum.h>\n#include <stdlib.h>\nint main(void){quant int8_t x=-7;quant uint8_t y=abs(x);int r=measure(y);return r!=7||measure(x)!=-7;}\n"},
        {"stdlib abs restores integer promotion at signed byte minimum",
         "#include <quantum.h>\n#include <stdlib.h>\nint main(void){quant int8_t x=-128;quant uint8_t y=abs(x);int r=measure(y);return r!=128||measure(x)!=-128;}\n"},
        {"stdlib labs preserves a promoted narrow quantum source",
         "#include <quantum.h>\n#include <stdlib.h>\nint main(void){quant int8_t x=-7;quant uint8_t y=labs((long)x);int r=measure(y);return r!=7||measure(x)!=-7;}\n"},
        {"stdlib llabs preserves a promoted narrow quantum source",
         "#include <quantum.h>\n#include <stdlib.h>\nint main(void){quant int8_t x=-7;quant uint8_t y=llabs((long long)x);int r=measure(y);return r!=7||measure(x)!=-7;}\n"},
        {"inttypes imaxabs preserves a promoted narrow quantum source",
         "#include <quantum.h>\n#include <inttypes.h>\nint main(void){quant int8_t x=-7;quant uint8_t y=imaxabs((intmax_t)x);int r=measure(y);return r!=7||measure(x)!=-7;}\n"}};

    for (unsigned i = 0; i < sizeof supported / sizeof *supported; ++i) {
        report(supported[i].name, compile(supported[i].source, "--run", 1));
        char label[160];
        snprintf(label, sizeof label, "%s with builtins disabled", supported[i].name);
        report(label, compile_extra(supported[i].source, "--run", "-fno-builtin", 1));
    }
    report(
        "library adapter does not rewrite a user-defined abs helper",
        compile(
            "#include <quantum.h>\nstatic int abs(int n){return n+1;}\nint main(void){quant int8_t x=-7;quant uint8_t y=abs(x);measure(y);measure(x);}\n",
            "--emit-ir",
            0));
    report(
        "stdlib abs rejects unrepresentable signed int minimum",
        compile(
            "#include <quantum.h>\n#include <stdlib.h>\n#include <limits.h>\nint main(void){quant int x=INT_MIN;quant int y=abs(x);measure(y);measure(x);}\n",
            "--emit-ir",
            0));
    const char *wide_names[] = {"labs", "llabs", "imaxabs"};
    for (unsigned i = 0; i < 3; ++i) {
        char source[512], label[128];
        snprintf(
            source,
            sizeof source,
            "#include <quantum.h>\n#include <stdlib.h>\n#include <inttypes.h>\n#include <limits.h>\nint main(void){quant int32_t x=INT_MIN;quant uint32_t y=%s((intmax_t)x);measure(y);measure(x);return 0;}\n",
            wide_names[i]);
        snprintf(
            label, sizeof label, "promoted %s exports signed int minimum correctly", wide_names[i]);
        report(label, compile(source, "--emit-qasm", 1));
    }
    const char *expr =
        "{\"op\":\"cast\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"abs\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false}]}]}]}]}";
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(1, 7);
        qc_allow_tables(c, tables);
        char operation[1024];
        int ok = c &&
                 qc_apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":2,\"signed\":false}", NULL) &&
                 qc_apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":3}", NULL) &&
                 qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":2,\"signed\":false}", NULL);
        snprintf(operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}", expr);
        ok = ok && qc_apply(c, operation, NULL);
        if (ok)
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                int x = (int)(i & 3), signed_x = x >= 2 ? x - 4 : x;
                unsigned expected = (unsigned)abs(signed_x);
                ok &= cabs(qc_amplitude(c, i) - ((i >> 2) == expected ? .5 : 0)) < 1e-10;
            }
        snprintf(operation, sizeof operation, "{\"op\":\"quncompute\",\"id\":1,\"expr\":%s}", expr);
        ok = ok && qc_apply(c, operation, NULL) &&
             qc_apply(c, "{\"op\":\"qrelease\",\"id\":1}", NULL);
        if (ok)
            for (size_t i = 0; i < qc_state_size(c); ++i)
                ok &= cabs(qc_amplitude(c, i) - (i < 4 ? .5 : 0)) < 1e-10;
        report(tables ? "finite abs: complete signed domain, phase and inverse"
                      : "no-table abs: complete signed domain, phase and inverse",
               ok);
        qc_destroy(c);
    }
    const char *additional[] = {
        "{\"op\":\"uabs\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false}]}]}",
        "{\"op\":\"cast\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"abs\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"sub\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}]},{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":3}]}]}]}"};
    for (unsigned variant = 0; variant < 2; ++variant)
        for (unsigned tables = 0; tables < 2; ++tables) {
            unsigned input_bits = variant ? 3 : 2;
            qc_context *c = qc_create(1, 7);
            qc_allow_tables(c, tables);
            char operation[1200], label[128];
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",
                     input_bits);
            int ok = c && qc_apply(c, operation, NULL);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":%u}",
                     (1u << input_bits) - 1);
            ok = ok && qc_apply(c, operation, NULL) &&
                 qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":2,\"signed\":false}", NULL);
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}",
                     additional[variant]);
            ok = ok && qc_apply(c, operation, NULL);
            if (ok)
                for (size_t j = 0; j < qc_state_size(c); ++j) {
                    int value = (int)(j & ((1u << input_bits) - 1));
                    value = variant ? value - 3 : value >= 2 ? value - 4 : value;
                    unsigned expected = (unsigned)abs(value) & 3;
                    double amplitude =
                        (j >> input_bits) == expected ? 1 / sqrt((double)(1u << input_bits)) : 0;
                    ok &= cabs(qc_amplitude(c, j) - amplitude) < 1e-10;
                }
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"quncompute\",\"id\":1,\"expr\":%s}",
                     additional[variant]);
            ok = ok && qc_apply(c, operation, NULL) &&
                 qc_apply(c, "{\"op\":\"qrelease\",\"id\":1}", NULL);
            if (ok)
                for (size_t j = 0; j < qc_state_size(c); ++j)
                    ok &= cabs(qc_amplitude(c, j) - (j < (1u << input_bits)
                                                         ? 1 / sqrt((double)(1u << input_bits))
                                                         : 0)) < 1e-10;
            snprintf(label,
                     sizeof label,
                     "%s %s: complete domain, phase and inverse",
                     tables ? "finite" : "no-table",
                     variant ? "narrowed abs keeps original sign"
                             : "unsigned abs includes signed minimum");
            report(label, ok);
            qc_destroy(c);
        }
}
