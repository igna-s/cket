#include "validation.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expression(char *out, size_t size, uint64_t *rng, unsigned depth) {
    if (!depth || !validation_bounded(rng, 5)) {
        switch (validation_bounded(rng, 3)) {
        case 0:
            validation_path(out, size, "x");
            break;
        case 1:
            validation_path(out, size, "(unsigned)x");
            break;
        default:
            validation_path(out, size, "%" PRIu64, 1 + validation_bounded(rng, 15));
            break;
        }
        return;
    }
    static const char *ops[] = {"+", "-", "*", "^", "&", "|", "%", "/", "<<", ">>", "<", "=="};
    char left[8192], right[8192];
    expression(left, sizeof left, rng, depth - 1);
    unsigned op = (unsigned)validation_bounded(rng, 12);
    if (op == 6 || op == 7)
        validation_path(right, sizeof right, "%" PRIu64, 1 + validation_bounded(rng, 15));
    else if (op == 8 || op == 9)
        validation_path(right, sizeof right, "%" PRIu64, validation_bounded(rng, 4));
    else
        expression(right, sizeof right, rng, depth - 1);
    validation_path(out, size, "((unsigned)(%s) %s (unsigned)(%s))", left, ops[op], right);
}

static void substitute(FILE *f, const char *expr, const char *replacement) {
    for (const char *p = expr; *p; ++p) {
        if (*p == 'x')
            fputs(replacement, f);
        else
            fputc(*p, f);
    }
}

static void program(const char *path, const char *expr) {
    FILE *f = fopen(path, "w");
    if (!f)
        validation_fail("cannot write generated C program");
    /* Compare the full expression with its CPU reference. A Boolean result
     * avoids implicit narrowing and does not turn identity cases into moves. */
    fputs("#include <quantum.h>\n#include <stdlib.h>\nint main(void){int bad=0;\n", f);
    const unsigned inputs[] = {0, 1, 3, 7, 15, 127, 255};
    for (unsigned i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        unsigned value = inputs[i];
        char name[32], literal[32];
        validation_path(name, sizeof name, "q%u", value);
        validation_path(literal, sizeof literal, "%u", value);
        fprintf(f, "quant uint8_t %s=%u;quant bool r%u=(", name, value, value);
        substitute(f, expr, name);
        fputs(")==(", f);
        substitute(f, expr, literal);
        fprintf(f, ");bad|=!measure(r%u);reset(r%u);reset(%s);\n", value, value, name);
    }
    fputs(
        "quant uint8_t a=1,b=2;int n=1;\na+=++n;a+=measure(b);bad|=n!=2 || measure(a)!=5;reset(a);reset(b);\n"
        "int inc=255,dec=257,post=255;bad|=((uint8_t)++inc)!=0 || inc!=256;\n"
        "bad|=((uint8_t)--dec)!=0 || dec!=256;bad|=((int8_t)post++)!=-1 || post!=256;\n"
        "quant int8_t negative=-128;quant uint8_t magnitude=abs(negative);\n"
        "bad|=measure(magnitude)!=128 || measure(negative)!=-128;reset(magnitude);reset(negative);\nreturn bad;}\n",
        f);
    if (ferror(f) || fclose(f))
        validation_fail("cannot finish generated C program");
}

static uint64_t number(const char *text) {
    char *end;
    errno = 0;
    unsigned long long n = strtoull(text, &end, 10);
    if (!*text || *text == '-' || *end || errno)
        validation_fail("invalid numeric argument: %s", text);
    return (uint64_t)n;
}

int main(int argc, char **argv) {
    uint64_t seed = 20261011, cases = 24;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc)
            validation_fail("usage: fuzz_frontend [--seed N] [--cases N]");
        if (!strcmp(argv[i], "--seed"))
            seed = number(argv[++i]);
        else if (!strcmp(argv[i], "--cases"))
            cases = number(argv[++i]);
        else
            validation_fail("unknown option: %s", argv[i]);
    }
    if (cases > 4096)
        validation_fail("case budget exceeds 4096");
    validation v;
    validation_init(&v, argv[0]);
    char source[4096], binary[4096], compiler[4096];
    validation_path(source, sizeof source, "%s/program.c", v.temp);
    validation_path(binary, sizeof binary, "%s/program", v.temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v.root);
    const char *fixed[] = {
        "x?255/x:7", "x && (7/x)", "!x || (7/x)", "(uint8_t)(x*x)", "((int8_t)x)<0"};
    char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    uint64_t rng = seed;
    for (uint64_t i = 0; i < cases + 5; ++i) {
        char expr[8192];
        if (i < 5)
            validation_path(expr, sizeof expr, "%s", fixed[i]);
        else
            expression(expr, sizeof expr, &rng, 3);
        program(source, expr);
        for (unsigned l = 0; l < 5; ++l) {
            char *command[] = {
                compiler, source, levels[l], "--run", "--seed", "7", "-o", binary, NULL};
            int result = validation_run(&v, command, NULL, 30);
            if (result) {
                char target[4096], detail[16384];
                char *text = validation_read(source), *log = validation_read(v.log);
                validation_path(target, sizeof target, "%s/build/frontend-failure.c", v.root);
                validation_write(target, text);
                free(text);
                validation_path(target, sizeof target, "%s/build/frontend-failure.txt", v.root);
                validation_path(detail,
                                sizeof detail,
                                "seed=%" PRIu64 "\ncase=%" PRIu64
                                "\noptimization=%s\nexit=%d\nexpression=%s\n",
                                seed,
                                i,
                                levels[l],
                                result,
                                expr);
                validation_write(target, detail);
                validation_path(target, sizeof target, "%s/build/frontend-failure.log", v.root);
                validation_write(target, log);
                fprintf(stderr, "%s", log);
                free(log);
                validation_cleanup(&v);
                validation_fail("frontend differential failed: seed=%" PRIu64 ", case=%" PRIu64
                                ", %s; see build/frontend-failure.*",
                                seed,
                                i,
                                levels[l]);
            }
        }
        printf("PASS frontend %" PRIu64 ": all 5 optimization levels\n", i);
        fflush(stdout);
    }
    printf("%" PRIu64 " generated frontend compilations passed; seed=%" PRIu64
           " (C generator, SplitMix64)\n",
           (cases + 5) * 5,
           seed);
    validation_cleanup(&v);
    return 0;
}
