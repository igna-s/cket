#define _POSIX_C_SOURCE 200809L
#include "validation.h"
#include "quantum.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name, *angle;
    bool floating_zero;
} phase_case;

static const phase_case cases[] = {
    {"integer", "10/k", false},
    {"absolute", "abs(10/k)", false},
    {"narrow", "(uint8_t)(10/k)", false},
    {"long", "10LL/k", false},
    {"unsigned-long", "UINT64_MAX/(unsigned)k", false},
    {"double", "10.0/k", true},
    {"mixed", "10/k+.17", false},
    {"real-snapshot", "a/k", true},
    {"float", "(double)(10.0f/(float)k)", true},
    {"float-rounding", "(double)((float)(10/k)+.17f)", false},
    {"conditional", "enabled ? 10/k+.17 : .5", false},
};
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const char *basis[][2] = {{"0", "0"}, {"2", "0"}, {"2", "1"}, {"-3", "1"}};
static const char *coherent[][2] = {{"2", "0"}, {"2", "1"}, {"-3", "1"}};

static void
compile(validation *v, const char *source, const char *binary, unsigned level, unsigned builtin) {
    char compiler[4096], engine[4096], tests[4096];
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(engine, sizeof engine, "-I%s/src/engine", v->root);
    validation_path(tests, sizeof tests, "-I%s/tests", v->root);
    char *command[] = {compiler,
                       (char *)source,
                       (char *)levels[level],
                       builtin ? "-fbuiltin" : "-fno-builtin",
                       engine,
                       tests,
                       "-Wl,--wrap=__cket_apply",
                       "-Wl,--wrap=__cket_apply_values",
                       "-Wl,--wrap=__cket_apply_arg",
                       "-Wl,--wrap=__cket_apply_phase",
                       "-Wl,--wrap=__cket_measure",
                       "-Wl,--wrap=__cket_controls_possible",
                       "-o",
                       (char *)binary,
                       NULL};
    if (validation_run(v, command, NULL, 240)) {
        char *log = validation_read(v->log);
        validation_fail("phase fixture compilation failed:\n%s", log);
    }
}

static void execute(validation *v,
                    const char *binary,
                    const char *k,
                    const char *enabled,
                    unsigned inverse,
                    unsigned capture,
                    const char *diagnostic) {
    char *command[] = {
        (char *)binary, (char *)k, (char *)enabled, inverse ? "1" : "0", capture ? "1" : "0", NULL};
    int status = validation_run(v, command, NULL, 60);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail("phase execution k=%s enabled=%s inverse=%u capture=%u failed (%d):\n%s",
                        k,
                        enabled,
                        inverse,
                        capture,
                        status,
                        log);
    free(log);
}

static unsigned frontend(validation *v) {
    char source[4096], binary[4096], text[8192];
    validation_path(source, sizeof source, "%s/phase.c", v->temp);
    validation_path(binary, sizeof binary, "%s/phase.out", v->temp);
    unsigned native = 0, states = 0, domains = 0, nested = 0;
    for (unsigned test = 0; test < sizeof cases / sizeof *cases; ++test)
        for (unsigned kind = 0; kind < 2; ++kind) {
            const char *angle = cases[test].angle;
            int size = snprintf(
                text,
                sizeof text,
                "#include \"phase_capture.inc\"\n#include <quantum.h>\n"
                "int main(int argc,char **argv){if(argc!=5)return 2;"
                "int k=atoi(argv[1]),enabled=atoi(argv[2]),undo=atoi(argv[3]);double a=10;(void)a;"
                "phase_test_capture=atoi(argv[4]);phase_test_preparation=%s;"
                "phase_test_inverse=undo;if(k&&(phase_test_preparation!=0))phase_test_angle=(double)(%s);"
                "quant bool b=%s;if(b){phase(%s);if(undo)phase(-(double)(%s));}"
                "return measure(b)>1;}\n",
                kind ? "-1" : "!!enabled",
                angle,
                kind ? "superposition(0,1)" : "enabled",
                angle,
                angle);
            if (size < 0 || (size_t)size >= sizeof text)
                validation_fail("phase fixture exceeds its checked buffer");
            validation_write(source, text);
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtin = 0; builtin < 2; ++builtin) {
                    compile(v, source, binary, level, builtin);
                    unsigned count = kind ? 3 : 4;
                    const char *(*inputs)[2] = kind ? coherent : basis;
                    for (unsigned input = 0; input < count; ++input)
                        for (unsigned inverse = 0; inverse < 2; ++inverse) {
                            execute(
                                v, binary, inputs[input][0], inputs[input][1], inverse, 0, NULL);
                            ++native;
                            execute(
                                v, binary, inputs[input][0], inputs[input][1], inverse, 1, NULL);
                            ++states;
                        }
                    execute(v,
                            binary,
                            "0",
                            "1",
                            0,
                            0,
                            cases[test].floating_zero
                                ? "nonfinite phase angle"
                                : "C division/remainder by zero is undefined");
                    ++domains;
                }
        }
    const char *bodies[] = {
        "if(b){if(!b)phase(10/k);}",
        "quant bool c=false;if(b){if(c)phase(10/k);}measure(c);",
    };
    for (unsigned body = 0; body < 2; ++body) {
        validation_path(text,
                        sizeof text,
                        "#include <quantum.h>\n#include <stdlib.h>\n"
                        "int main(int argc,char **argv){int k=argc>1?atoi(argv[1]):0;"
                        "quant bool b=superposition(0,1);%s return measure(b)>1;}\n",
                        bodies[body]);
        validation_write(source, text);
        /* These fixtures use the ordinary runtime without capture helpers. */
        char compiler[4096];
        validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                char *command[] = {compiler,
                                   source,
                                   (char *)levels[level],
                                   builtin ? "-fbuiltin" : "-fno-builtin",
                                   "-o",
                                   binary,
                                   NULL};
                if (validation_run(v, command, NULL, 240))
                    validation_fail("nested phase fixture did not compile");
                char *run[] = {binary, "0", NULL};
                if (validation_run(v, run, NULL, 60))
                    validation_fail("proved-inactive nested phase evaluated its zero divisor");
                ++nested;
            }
    }
    if (native != 1540 || states != 1540 || domains != 220 || nested != 20)
        validation_fail("phase frontend coverage count mismatch");
    printf(
        "PASS %u native phase executions, %u compiled complete-state checks, %u domain diagnostics and %u nested controls.\n",
        native,
        states,
        domains,
        nested);
    return native + states + domains + nested;
}

#define INT32(v) "{\"op\":\"const\",\"bits\":32,\"signed\":true,\"value\":" #v "}"
#define REAL64(v) "{\"op\":\"real_const\",\"bits\":64,\"value\":" #v "}"
#define FROM_INT(v) "{\"op\":\"real_from_int\",\"bits\":64,\"args\":[" v "]}"
#define REG "{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}"
#define DIVZERO \
    FROM_INT("{\"op\":\"div\",\"bits\":32,\"signed\":true,\"args\":[" INT32(10) "," INT32(0) "]}")

static unsigned schema(void) {
    const char *malformed[] = {
        "{\"op\":\"real_unknown\",\"bits\":64,\"args\":[" REAL64(0.17) "]}",
        "{\"op\":\"real_const\",\"value\":0.17}",
        "{\"op\":\"real_const\",\"bits\":80,\"value\":0.17}",
        "{\"op\":\"real_const\",\"bits\":64,\"value\":\"0.17\"}",
        "{\"op\":\"real_const\",\"bits\":64,\"value\":0.17,\"ignored\":true}",
        "{\"op\":\"real_const\",\"bits\":64,\"value\":0.17,\"signed\":false}",
        "{\"op\":\"real_add\",\"bits\":64,\"args\":[" REAL64(0.17) "]}",
        "{\"op\":\"real_neg\",\"bits\":64,\"args\":[" REAL64(0.17) "," REAL64(0.17) "]}",
        "{\"op\":\"real_add\",\"bits\":64,\"args\":[" REAL64(
            0.17) ",{\"op\":\"real_const\",\"bits\":32,\"value\":0.17}]}",
        FROM_INT(REG),
        "{\"op\":\"real_select\",\"bits\":64,\"args\":[" REG "," REAL64(0.17) "," REAL64(0.17) "]}",
        "{\"op\":\"real_bits\",\"bits\":64,\"args\":[{\"op\":\"const\",\"bits\":64,\"signed\":true,\"value\":0}]}",
        "{\"op\":\"real_bits\",\"bits\":64,\"args\":[{\"op\":\"const\",\"bits\":32,\"signed\":false,\"value\":0}]}",
        "{\"op\":\"real_cast\",\"bits\":64}",
    };
    unsigned checks = 0;
    for (unsigned test = 0; test < sizeof malformed / sizeof *malformed; ++test)
        for (unsigned simulate = 0; simulate < 2; ++simulate)
            for (unsigned enabled = 0; enabled < 2; ++enabled) {
                qc_context *c = qc_create((int)simulate, 7);
                char operation[4096];
                if (!c ||
                    !qc_apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}", NULL))
                    validation_fail("phase schema context setup failed");
                validation_path(operation,
                                sizeof operation,
                                "{\"op\":\"qprepare\",\"id\":0,\"lower\":%u,\"upper\":%u}",
                                enabled,
                                enabled);
                if (!qc_apply(c, operation, NULL))
                    validation_fail("phase schema preparation failed");
                validation_path(
                    operation,
                    sizeof operation,
                    "{\"op\":\"qphase\",\"controls\":[{\"id\":0,\"value\":1}],\"angle_expr\":%s}",
                    malformed[test]);
                if (qc_apply(c, operation, NULL) || !*qc_error(c))
                    validation_fail("invalid phase schema was accepted (case %u)", test);
                qc_destroy(c);
                ++checks;
            }
    for (unsigned simulate = 0; simulate < 2; ++simulate)
        for (unsigned enabled = 0; enabled < 2; ++enabled) {
            qc_context *c = qc_create((int)simulate, 7);
            char operation[4096];
            if (!c ||
                !qc_apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}", NULL))
                validation_fail("phase domain context setup failed");
            validation_path(operation,
                            sizeof operation,
                            "{\"op\":\"qprepare\",\"id\":0,\"lower\":%u,\"upper\":%u}",
                            enabled,
                            enabled);
            if (!qc_apply(c, operation, NULL))
                validation_fail("phase domain preparation failed");
            validation_path(
                operation,
                sizeof operation,
                "{\"op\":\"qphase\",\"controls\":[{\"id\":0,\"value\":1}],\"angle_expr\":%s}",
                DIVZERO);
            bool ok = qc_apply(c, operation, NULL) != 0;
            if (ok != !enabled || (enabled && !strstr(qc_error(c), "division/remainder by zero")))
                validation_fail("phase arithmetic domain was not selected lazily");
            qc_destroy(c);
            ++checks;
        }
    if (checks != 60)
        validation_fail("phase schema coverage count mismatch");
    printf("PASS %u phase schema and lazy-domain checks.\n", checks);
    return checks;
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_phase");
    validation v;
    validation_init(&v, argv[0]);
    unsigned checks = frontend(&v) + schema();
    validation_cleanup(&v);
    printf(
        "%u phase checks passed; compiled CPU angles, complete states, native domains and independent exported proofs have separate scopes.\n",
        checks);
    return 0;
}
