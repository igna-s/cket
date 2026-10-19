/* Pending and unsupported partial libc contexts must diagnose unmeasured inputs. A successful
 * ordinary C syntax check establishes that a rejection is about quantum semantics, rather than a
 * misspelled interface or an invalid function prototype. Positive adapters have separate
 * amplitude/inverse validators. */
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, interfaces;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const char preamble[] =
    "#ifdef __CKET__\n#include <quantum.h>\n"
    "#else\n#include <stdint.h>\n#define quant\n#define reset(x) ((void)(x))\n#endif\n"
    "#include <complex.h>\n#include <fenv.h>\n#include <inttypes.h>\n"
    "#include <locale.h>\n#include <math.h>\n#include <signal.h>\n"
    "#include <stdatomic.h>\n#include <stdio.h>\n#include <stdlib.h>\n"
    "#include <string.h>\n#include <threads.h>\n#include <time.h>\n"
    "#include <uchar.h>\n#include <wchar.h>\n#include <wctype.h>\n";

static void rejected(validation *v, const char *name, const char *body, const char *diagnostic) {
    char source[4096], output[4096], compiler[4096], program[16384];
    validation_path(source, sizeof source, "%s/library.c", v->temp);
    validation_path(output, sizeof output, "%s/library.json", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(program,
                    sizeof program,
                    "%sint main(void){quant uint8_t x=3;\n%s\nreset(x);return 0;}\n",
                    preamble,
                    body);
    validation_write(source, program);
    const char *cc = getenv("CC");
    if (!cc || !*cc)
        cc = "gcc";
    char *reference[] = {
        (char *)cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only", source, NULL};
    if (validation_run(v, reference, NULL, 60))
        validation_fail("invalid classical library fixture %s:\n%s", name, validation_read(v->log));
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtins = 0; builtins < 2; ++builtins) {
            char *command[] = {compiler,
                               source,
                               "--emit-ir",
                               (char *)levels[level],
                               builtins ? "-fno-builtin" : "-fno-strict-aliasing",
                               "-o",
                               output,
                               NULL};
            int status = validation_run(v, command, NULL, 60);
            char *log = validation_read(v->log);
            if (!validation_rejected(status, log, diagnostic))
                validation_fail(
                    "libc quantum boundary %s failed at %s (builtins=%u), status=%d:\n%s",
                    name,
                    levels[level],
                    !builtins,
                    status,
                    log);
            free(log);
            ++checks;
        }
    ++interfaces;
    printf("PASS libc boundary %s: valid C and 10 quantum diagnostics\n", name);
    fflush(stdout);
}

static void floating_families(validation *v) {
    /* All three C floating overloads are distinct interfaces, not three
     * executions of one symbol. Conversion to CPU floating values is pending. */
    const char *unary[] = {"acos",  "asin",      "atan",  "acosh", "asinh",  "atanh",  "cos",
                           "sin",   "tan",       "cosh",  "sinh",  "tanh",   "exp",    "exp2",
                           "expm1", "log",       "log10", "log1p", "log2",   "logb",   "cbrt",
                           "fabs",  "sqrt",      "erf",   "erfc",  "lgamma", "tgamma", "ceil",
                           "floor", "nearbyint", "rint",  "round", "trunc"};
    const char *binary[] = {"atan2",
                            "pow",
                            "hypot",
                            "fmod",
                            "remainder",
                            "copysign",
                            "nextafter",
                            "fdim",
                            "fmax",
                            "fmin"};
    const char *types[] = {"double", "float", "long double"};
    const char *suffixes[] = {"", "f", "l"};
    for (unsigned type = 0; type < 3; ++type) {
        for (unsigned i = 0; i < sizeof unary / sizeof *unary; ++i) {
            char name[64], body[512];
            validation_path(name, sizeof name, "%s%s", unary[i], suffixes[type]);
            validation_path(body,
                            sizeof body,
                            "volatile %s result=%s((%s)x);(void)result;",
                            types[type],
                            name,
                            types[type]);
            rejected(v, name, body, "classical initialization cannot read a quantum value");
        }
        for (unsigned i = 0; i < sizeof binary / sizeof *binary; ++i) {
            char name[64], body[512];
            validation_path(name, sizeof name, "%s%s", binary[i], suffixes[type]);
            validation_path(body,
                            sizeof body,
                            "volatile %s result=%s((%s)x,(%s)2);(void)result;",
                            types[type],
                            name,
                            types[type],
                            types[type]);
            rejected(v, name, body, "classical initialization cannot read a quantum value");
        }
        const char *complex_unary[] = {"cacos",
                                       "casin",
                                       "catan",
                                       "ccos",
                                       "csin",
                                       "ctan",
                                       "cacosh",
                                       "casinh",
                                       "catanh",
                                       "ccosh",
                                       "csinh",
                                       "ctanh",
                                       "cexp",
                                       "clog",
                                       "csqrt",
                                       "conj",
                                       "cproj"};
        for (unsigned i = 0; i < sizeof complex_unary / sizeof *complex_unary; ++i) {
            char name[64], body[512];
            validation_path(name, sizeof name, "%s%s", complex_unary[i], suffixes[type]);
            validation_path(body,
                            sizeof body,
                            "volatile %s complex result=%s((%s complex)x);(void)result;",
                            types[type],
                            name,
                            types[type]);
            rejected(v, name, body, "classical initialization cannot read a quantum value");
        }
    }
}

static void other_boundaries(validation *v) {
    const struct {
        const char *name, *body, *diagnostic;
    } cases[] = {
        {"printf",
         "printf(\"%u\",(unsigned)x);",
         "classical call arguments cannot read quantum values"},
        {"putchar", "putchar(x);", "classical call arguments cannot read quantum values"},
        {"srand", "srand(x);", "classical call arguments cannot read quantum values"},
        {"malloc",
         "void *p=malloc(x);(void)p;",
         "classical initialization cannot read a quantum value"},
        {"calloc",
         "void *p=calloc(x,1);(void)p;",
         "classical initialization cannot read a quantum value"},
        {"raise", "raise(x);", "classical call arguments cannot read quantum values"},
        {"fesetround", "fesetround(x);", "classical call arguments cannot read quantum values"},
        {"feclearexcept",
         "feclearexcept(x);",
         "classical call arguments cannot read quantum values"},
        {"setlocale",
         "char *p=setlocale(x,\"C\");(void)p;",
         "classical initialization cannot read a quantum value"},
        {"atomic_store",
         "atomic_int a=0;atomic_store(&a,x);",
         "classical initialization cannot read a quantum value"},
        {"memset",
         "quant uint8_t a[2]={1,2};void *p=memset(a,0,2);printf(\"%p\",p);reset(a[0]);reset(a[1]);",
         "escape"},
        {"memcmp",
         "quant uint8_t a[2]={1,2};int r=memcmp(a,\"ab\",2);(void)r;reset(a[0]);reset(a[1]);",
         "classical initialization cannot read a quantum value"},
        {"strlen",
         "quant char a[2]={'a',0};size_t r=strlen(a);(void)r;reset(a[0]);reset(a[1]);",
         "classical initialization cannot read a quantum value"},
        {"mbrtoc16",
         "char16_t out;mbstate_t state={0};quant char a[2]={'a',0};size_t r=mbrtoc16(&out,a,1,&state);(void)r;reset(a[0]);reset(a[1]);",
         "quantum products cannot decay to CPU pointers"},
        {"mbrtoc32",
         "char32_t out;mbstate_t state={0};quant char a[2]={'a',0};size_t r=mbrtoc32(&out,a,1,&state);(void)r;reset(a[0]);reset(a[1]);",
         "quantum products cannot decay to CPU pointers"},
        {"time",
         "quant time_t t=0;time(&t);reset(t);",
         "classical call arguments cannot read quantum values"},
    };

    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i)
        rejected(v, cases[i].name, cases[i].body, cases[i].diagnostic);
    const char *wide[] = {"iswalnum",
                          "iswalpha",
                          "iswblank",
                          "iswcntrl",
                          "iswdigit",
                          "iswgraph",
                          "iswlower",
                          "iswprint",
                          "iswpunct",
                          "iswspace",
                          "iswupper",
                          "iswxdigit",
                          "towlower",
                          "towupper"};
    for (unsigned i = 0; i < sizeof wide / sizeof *wide; ++i) {
        char body[256];
        validation_path(body, sizeof body, "volatile wint_t result=%s(x);(void)result;", wide[i]);
        rejected(v, wide[i], body, "classical initialization cannot read a quantum value");
    }
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_library");
    validation v;
    validation_init(&v, argv[0]);
    other_boundaries(&v);
    floating_families(&v);
    printf(
        "%u libc boundary checks passed across %u interfaces; rejections are not quantum implementations.\n",
        checks,
        interfaces);
    validation_cleanup(&v);
    return 0;
}
