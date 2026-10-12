#define _GNU_SOURCE
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name, *expected;
    int static_export, runtime;
} example;

static const example examples[] = {
    {"hello", "12\n", 1, 1},
    {"entanglement", NULL, 1, 1},
    {"find", "3\n", 1, 1},
    {"search", "3\n", 0, 1},
    {"search_empty", "Search budget exhausted\n", 0, 1},
    {"coherent", NULL, 1, 1},
    {"prepare_measure", NULL, 1, 1},
    {"fourier", "7\n", 1, 1},
    {"phase_estimation", "3\n", 1, 1},
    {"functions", "4\n", 1, 1},
    {"circuit_module", "7\n", 1, 1},
    {"products", "9 2\n3 7\n", 1, 1},
    {"vector_function", "10 1\n", 1, 1},
    {"dynamic_vector", "3\n", 0, 1},
    {"hybrid_loop", "29\n", 0, 1},
    /* quant int is 32 bits: accepted for export, above the simulator limit. */
    {"types", "", 1, 0},
    {"for_sum", "sum(1..10)=55\n", 0, 1},
    {"while_sum", "while sum=55 iterations=10\n", 0, 1},
    {"do_while_countdown", "remaining=5 iterations=5\n", 0, 1},
    {"nested_for_sum", "matrix sum=78\n", 0, 1},
    {"register_accumulate", "sum=15 restored=0 term=3\n", 0, 1},
    {"fibonacci", "F(8)=21 F(9)=34\n", 0, 1},
    {"modular_arithmetic", "add=14 subtract=240 xor=255\n", 1, 1},
    {"dot_product", "dot product=70\n", 0, 1},
    {"pair_minimum", "min(19,7)=7\n", 1, 1},
    {"pair_maximum", "max(19,7)=19\n", 1, 1},
    {"minimum_scan", "minimum f(x)=min(x,15-x): value=0 x=0\n", 0, 1},
    {"maximum_scan", "maximum f(x)=min(x,15-x): value=7 x=7\n", 0, 1},
    {"minmax_array", "min=0 index=3 max=255 index=5\n", 0, 1},
    {"input_sum", "count=10 sum=55\n", 0, 1},
    {"input_minmax", "min=0 index=3 max=255 index=5\n", 0, 1},
    {"measurement_histogram", NULL, 0, 1},
    {"repeat_until_success", NULL, 0, 1},
    {"find_multiple", NULL, 1, 1},
    {"search_constraints", NULL, 0, 1},
    {"coherent_accumulate", NULL, 1, 1}};

typedef struct {
    const char *name, *input, *output;
    int status;
} input_case;

static const input_case input_cases[] = {
    {"input_sum", "0\n", "count=0 sum=0\n", 0},
    {"input_sum", "1\n", "count=1 sum=1\n", 0},
    {"input_sum", "4\n", "count=4 sum=10\n", 0},
    {"input_sum", "16\n", "count=16 sum=136\n", 0},
    {"input_sum", "0004\n", "count=4 sum=10\n", 0},
    {"input_sum", "4294967296\n", "", 1},
    {"input_sum", "184467440737095516160\n", "", 1},
    {"input_sum", "16x\n", "", 1},
    {"input_sum", "17\n", "", 1},
    {"input_sum", "-1\n", "", 1},
    {"input_sum", "no\n", "", 1},
    {"input_sum", "", "", 1},
    {"input_minmax", "1 0\n", "min=0 index=0 max=0 index=0\n", 0},
    {"input_minmax", "1 255\n", "min=255 index=0 max=255 index=0\n", 0},
    {"input_minmax", "4 7 7 7 7\n", "min=7 index=0 max=7 index=0\n", 0},
    {"input_minmax", "5 5 4 3 2 1\n", "min=1 index=4 max=5 index=0\n", 0},
    {"input_minmax", "3 0 255 0\n", "min=0 index=0 max=255 index=1\n", 0},
    {"input_minmax", "\n 2 0002 0000\t", "min=0 index=1 max=2 index=0\n", 0},
    {"input_minmax", "4294967297\n", "", 1},
    {"input_minmax", "1 4294967296\n", "", 1},
    {"input_minmax", "1 184467440737095516160\n", "", 1},
    {"input_minmax", "1 7x\n", "", 1},
    {"input_minmax", "0\n", "", 1},
    {"input_minmax", "9\n", "", 1},
    {"input_minmax", "-1\n", "", 1},
    {"input_minmax", "2 7\n", "", 1},
    {"input_minmax", "1 256\n", "", 1},
    {"input_minmax", "1 -1\n", "", 1},
    {"input_minmax", "1 no\n", "", 1},
    {"input_minmax", "no\n", "", 1},
    {"input_minmax", "", "", 1}};

static const char *sample_input(const example *e) {
    if (!strcmp(e->name, "input_sum"))
        return "10\n";
    if (!strcmp(e->name, "input_minmax"))
        return "8 42 17 42 0 128 255 9 255\n";
    return "0\n";
}

static unsigned checks;

static void require(validation *v, int condition, const char *name, const char *action) {
    if (!condition) {
        char *log = validation_read(v->log);
        fprintf(stderr, "%s", log);
        free(log);
        validation_fail("example %s failed: %s (artifacts: %s)", name, action, v->temp);
    }
    ++checks;
}

static void check_output(validation *v, const example *e, const char *output) {
    char *text = validation_read(output);
    int ok = 0;
    unsigned a, b, c;
    char tail;
    if (e->expected)
        ok = !strcmp(text, e->expected);
    else if (!strcmp(e->name, "entanglement"))
        ok = sscanf(text, "%u %u %c", &a, &b, &tail) == 2 && a <= 1 && a == b;
    else if (!strcmp(e->name, "coherent"))
        ok = sscanf(text, "%u %c", &a, &tail) == 1 && a >= 5 && a <= 20;
    else if (!strcmp(e->name, "prepare_measure"))
        ok = sscanf(text, "classical=10 quantum=%u %c", &a, &tail) == 1 && a <= 255;
    else if (!strcmp(e->name, "measurement_histogram"))
        ok = sscanf(text, "zeros=%u ones=%u trials=%u %c", &a, &b, &c, &tail) == 3 && a <= 32 &&
             b <= 32 && a + b == 32 && c == 32;
    else if (!strcmp(e->name, "repeat_until_success"))
        ok = sscanf(text, "found=%u attempts=%u %c", &a, &b, &tail) == 2 && a <= 1 && b >= 1 &&
             b <= 16 && (a || b == 16);
    else if (!strcmp(e->name, "find_multiple"))
        ok = sscanf(text, "candidate=%u %c", &a, &tail) == 1 && a <= 15 && a % 4 == 1;
    else if (!strcmp(e->name, "search_constraints"))
        ok = !strcmp(text, "Search budget exhausted\n") ||
             (sscanf(text, "candidate=%u %c", &a, &tail) == 1 && a >= 8 && a <= 13 && a % 3 == 1);
    else if (!strcmp(e->name, "coherent_accumulate"))
        ok = sscanf(text, "flag=%u total=%u %c", &a, &b, &tail) == 2 && a <= 1 &&
             b == (a ? 15u : 30u);
    if (!ok)
        fprintf(stderr, "Unexpected stdout: %s\n", text);
    free(text);
    require(v, ok, e->name, "output");
}

static void
check_inputs(validation *v, const example *e, char *binary, const char *input, const char *output) {
    for (unsigned i = 0; i < sizeof input_cases / sizeof *input_cases; ++i) {
        const input_case *c = &input_cases[i];
        if (strcmp(c->name, e->name))
            continue;
        validation_write(input, c->input);
        char *run[] = {binary, NULL};
        require(v,
                validation_run_input(v, run, input, output, 90) == c->status,
                e->name,
                "boundary/input status");
        char *text = validation_read(output);
        require(v, !strcmp(text, c->output), e->name, "boundary/input output");
        free(text);
        if (c->status) {
            char *log = validation_read(v->log);
            require(
                v, strstr(log, "Expected a count") != NULL, e->name, "invalid input diagnostic");
            free(log);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: test_examples");
    validation v;
    validation_init(&v, argv[0]);
    char driver[4096], backend[4096], source[4096], binary[4096], output[4096];
    char input[4096], artifact[4096], seed[32];
    validation_path(driver, sizeof driver, "%s/build/cket", v.root);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v.root);
    validation_path(binary, sizeof binary, "%s/program", v.temp);
    validation_path(output, sizeof output, "%s/stdout", v.temp);
    validation_path(input, sizeof input, "%s/stdin", v.temp);
    validation_path(artifact, sizeof artifact, "%s/artifact", v.temp);
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    if (setenv("CKET_SEED", "7", 1))
        validation_fail("cannot set example seed");
    for (unsigned i = 0; i < sizeof examples / sizeof *examples; ++i) {
        const example *e = &examples[i];
        validation_path(source, sizeof source, "%s/examples/%s.c", v.root, e->name);
        for (unsigned level = 0; level < sizeof levels / sizeof *levels; ++level) {
            char *compile[] = {driver, source, (char *)levels[level], "-o", binary, NULL};
            require(&v, !validation_run(&v, compile, NULL, 90), e->name, levels[level]);
            validation_write(input, sample_input(e));
            char *run[] = {binary, NULL};
            int result = validation_run_input(&v, run, input, output, 90);
            require(&v, result == (e->runtime ? 0 : 1), e->name, "execution status");
            if (e->runtime)
                check_output(&v, e, output);
            else {
                char *log = validation_read(v.log);
                require(&v, strstr(log, "24") != NULL, e->name, "simulator limit diagnostic");
                free(log);
            }
            check_inputs(&v, e, binary, input, output);
        }
        const char *actions[] = {"--emit-ast", "--emit-ir", "--emit-qasm", "--resources"};
        for (unsigned action = 0; action < sizeof actions / sizeof *actions; ++action) {
            char *export[] = {driver, source, (char *)actions[action], "-o", artifact, NULL};
            int result = validation_run(&v, export, NULL, 90);
            require(&v,
                    e->static_export || action < 2 ? result == 0 : result == 1,
                    e->name,
                    actions[action]);
            if (!result) {
                char *text = validation_read(artifact);
                require(&v, *text != 0, e->name, "nonempty artifact");
                free(text);
            }
            if (action == 1) {
                char *verify[] = {backend, "--validate", artifact, NULL};
                require(&v, !validation_run(&v, verify, NULL, 90), e->name, "IR verification");
            }
        }
        printf("PASS example %-18s: five GCC levels, runtime contract and exports\n", e->name);
    }

    /* Repeat Bell measurements, checking correlations rather than one PRNG outcome. */
    validation_path(source, sizeof source, "%s/examples/entanglement.c", v.root);
    unsigned outcomes = 0;
    for (unsigned s = 0; s < 16; ++s) {
        snprintf(seed, sizeof seed, "%u", s);
        char *run[] = {driver, source, "--run", "--seed", seed, "-o", binary, NULL};
        require(&v, !validation_run(&v, run, output, 90), "entanglement", "seeded --run");
        check_output(&v, &examples[1], output);
        char *text = validation_read(output);
        outcomes |= 1u << (unsigned)strtoul(text, NULL, 10);
        free(text);
    }
    require(&v, outcomes == 3, "entanglement", "both correlated outcomes in fixed seed corpus");

    /* Check stochastic contracts over many seeds, rather than pinning samples. */
    for (unsigned i = 16; i < sizeof examples / sizeof *examples; ++i) {
        const example *e = &examples[i];
        if (e->expected)
            continue;
        validation_path(source, sizeof source, "%s/examples/%s.c", v.root, e->name);
        char *compile[] = {driver, source, "-o", binary, NULL};
        require(&v, !validation_run(&v, compile, NULL, 90), e->name, "seed corpus fixture");
        unsigned branches = 0;
        for (unsigned s = 0; s < 16; ++s) {
            snprintf(seed, sizeof seed, "%u", s);
            if (setenv("CKET_SEED", seed, 1))
                validation_fail("cannot set example seed");
            char *run[] = {binary, NULL};
            require(&v, !validation_run(&v, run, output, 90), e->name, "seed corpus execution");
            check_output(&v, e, output);
            if (!strcmp(e->name, "coherent_accumulate")) {
                char *text = validation_read(output);
                unsigned flag;
                if (sscanf(text, "flag=%u", &flag) != 1)
                    validation_fail("missing coherent flag");
                branches |= 1u << flag;
                free(text);
            }
        }
        if (!strcmp(e->name, "coherent_accumulate"))
            require(&v, branches == 3, e->name, "both coherent branch outcomes");
    }
    if (setenv("CKET_SEED", "7", 1))
        validation_fail("cannot restore example seed");

    validation_path(source, sizeof source, "%s/examples/dynamic_vector.c", v.root);
    char *compile[] = {driver, source, "-o", binary, NULL};
    require(&v, !validation_run(&v, compile, NULL, 90), "dynamic_vector", "input fixture");
    const char *inputs[] = {"1\n", "2\n", "no\n", ""};
    for (unsigned i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        validation_write(input, inputs[i]);
        char *run[] = {binary, NULL};
        require(&v,
                validation_run_input(&v, run, input, output, 90) == (i ? 1 : 0),
                "dynamic_vector",
                "second index, out of bounds, malformed input and EOF");
        char *text = validation_read(output);
        require(&v, !strcmp(text, i ? "" : "9\n"), "dynamic_vector", "input output");
        free(text);
    }

    /* A caller's working directory must not determine header/plugin/module locations. */
    validation remote = v;
    strcpy(remote.root, v.temp);
    validation_path(source, sizeof source, "%s/examples/circuit_module.c", v.root);
    char launcher[4096];
    validation_path(launcher, sizeof launcher, "%s/cket", v.root);
    char *away[] = {"bash", launcher, source, "--run", "--seed", "7", "-o", binary, NULL};
    require(&v,
            !validation_run(&remote, away, output, 90),
            "circuit_module",
            "launcher outside repository");
    check_output(&v, &examples[10], output);
    const char *options[] = {"--help", "--version"};
    for (unsigned i = 0; i < sizeof options / sizeof *options; ++i) {
        char *command[] = {driver, (char *)options[i], NULL};
        require(&v, !validation_run(&v, command, output, 30), "driver", options[i]);
        char *text = validation_read(output);
        require(&v, strstr(text, i ? "0.4.0" : "usage:") != NULL, "driver", "CLI information");
        free(text);
    }
    char *bad_seed[] = {driver, source, "--run", "--seed", "-1", NULL};
    require(&v, validation_run(&v, bad_seed, NULL, 30) == 1, "driver", "invalid seed");
    char *conflict[] = {driver, source, "--run", "--emit-ir", NULL};
    require(&v, validation_run(&v, conflict, NULL, 30) == 1, "driver", "conflicting actions");
    printf(
        "\n%u checks passed across %zu examples, five optimization levels and CLI/input contracts\n",
        checks,
        sizeof examples / sizeof *examples);
    validation_cleanup(&v);
    return 0;
}
