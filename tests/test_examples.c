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
    {"types", "", 1, 0}
};

static unsigned checks;
static void require(validation *v, int condition, const char *name, const char *action) {
    if (!condition) {
        char *log = validation_read(v->log);
        fprintf(stderr, "%s", log); free(log);
        validation_fail("example %s failed: %s (artifacts: %s)", name, action, v->temp);
    }
    ++checks;
}
static void check_output(validation *v, const example *e, const char *output) {
    char *text = validation_read(output); int ok = 0; unsigned a, b; char tail;
    if (e->expected) ok = !strcmp(text, e->expected);
    else if (!strcmp(e->name, "entanglement"))
        ok = sscanf(text, "%u %u %c", &a, &b, &tail) == 2 && a <= 1 && a == b;
    else if (!strcmp(e->name, "coherent"))
        ok = sscanf(text, "%u %c", &a, &tail) == 1 && a >= 5 && a <= 20;
    else if (!strcmp(e->name, "prepare_measure"))
        ok = sscanf(text, "classical=10 quantum=%u %c", &a, &tail) == 1 && a <= 255;
    if (!ok) fprintf(stderr, "Unexpected stdout: %s\n", text);
    free(text); require(v, ok, e->name, "output");
}

int main(int argc, char **argv) {
    if (argc != 1) validation_fail("usage: test_examples");
    validation v; validation_init(&v, argv[0]);
    char driver[4096], backend[4096], source[4096], binary[4096], output[4096];
    char input[4096], artifact[4096], seed[32];
    validation_path(driver, sizeof driver, "%s/build/cket", v.root);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v.root);
    validation_path(binary, sizeof binary, "%s/program", v.temp);
    validation_path(output, sizeof output, "%s/stdout", v.temp);
    validation_path(input, sizeof input, "%s/stdin", v.temp);
    validation_path(artifact, sizeof artifact, "%s/artifact", v.temp);
    validation_write(input, "0\n");
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    if (setenv("CKET_SEED", "7", 1)) validation_fail("cannot set example seed");
    for (unsigned i = 0; i < sizeof examples / sizeof *examples; ++i) {
        const example *e = &examples[i];
        validation_path(source, sizeof source, "%s/examples/%s.c", v.root, e->name);
        for (unsigned level = 0; level < sizeof levels / sizeof *levels; ++level) {
            char *compile[] = {driver, source, (char *)levels[level], "-o", binary, NULL};
            require(&v, !validation_run(&v, compile, NULL, 90), e->name, levels[level]);
            char *run[] = {binary, NULL};
            int result = validation_run_input(&v, run, input, output, 90);
            require(&v, result == (e->runtime ? 0 : 1), e->name, "execution status");
            if (e->runtime) check_output(&v, e, output);
            else {
                char *log = validation_read(v.log);
                require(&v, strstr(log, "24") != NULL, e->name, "simulator limit diagnostic");
                free(log);
            }
        }
        const char *actions[] = {"--emit-ast", "--emit-ir", "--emit-qasm", "--resources"};
        for (unsigned action = 0; action < sizeof actions / sizeof *actions; ++action) {
            char *export[] = {driver, source, (char *)actions[action], "-o", artifact, NULL};
            int result = validation_run(&v, export, NULL, 90);
            require(&v, e->static_export || action < 2 ? result == 0 : result == 1,
                    e->name, actions[action]);
            if (!result) {
                char *text = validation_read(artifact);
                require(&v, *text != 0, e->name, "nonempty artifact"); free(text);
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
        char *text = validation_read(output); outcomes |= 1u << (unsigned)strtoul(text, NULL, 10); free(text);
    }
    require(&v, outcomes == 3, "entanglement", "both correlated outcomes in fixed seed corpus");

    validation_path(source, sizeof source, "%s/examples/dynamic_vector.c", v.root);
    char *compile[] = {driver, source, "-o", binary, NULL};
    require(&v, !validation_run(&v, compile, NULL, 90), "dynamic_vector", "input fixture");
    const char *inputs[] = {"1\n", "2\n", "no\n", ""};
    for (unsigned i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        validation_write(input, inputs[i]); char *run[] = {binary, NULL};
        require(&v, validation_run_input(&v, run, input, output, 90) == (i ? 1 : 0),
                "dynamic_vector", "second index, out of bounds, malformed input and EOF");
        char *text = validation_read(output);
        require(&v, !strcmp(text, i ? "" : "9\n"), "dynamic_vector", "input output"); free(text);
    }

    /* A caller's working directory must not determine header/plugin/module locations. */
    validation remote = v; strcpy(remote.root, v.temp);
    validation_path(source, sizeof source, "%s/examples/circuit_module.c", v.root);
    char launcher[4096]; validation_path(launcher, sizeof launcher, "%s/cket", v.root);
    char *away[] = {"bash", launcher, source, "--run", "--seed", "7", "-o", binary, NULL};
    require(&v, !validation_run(&remote, away, output, 90), "circuit_module", "launcher outside repository");
    check_output(&v, &examples[10], output);
    const char *options[] = {"--help", "--version"};
    for (unsigned i = 0; i < sizeof options / sizeof *options; ++i) {
        char *command[] = {driver, (char *)options[i], NULL};
        require(&v, !validation_run(&v, command, output, 30), "driver", options[i]);
        char *text = validation_read(output);
        require(&v, strstr(text, i ? "0.4.0" : "usage:") != NULL, "driver", "CLI information"); free(text);
    }
    char *bad_seed[] = {driver, source, "--run", "--seed", "-1", NULL};
    require(&v, validation_run(&v, bad_seed, NULL, 30) == 1, "driver", "invalid seed");
    char *conflict[] = {driver, source, "--run", "--emit-ir", NULL};
    require(&v, validation_run(&v, conflict, NULL, 30) == 1, "driver", "conflicting actions");
    printf("\n%u checks passed across %zu examples, five optimization levels and CLI/input contracts\n",
           checks, sizeof examples / sizeof *examples);
    validation_cleanup(&v); return 0;
}
