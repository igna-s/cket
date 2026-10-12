#include "quantum.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *binding_path = NULL;
    int tables = 1;
    int valid_options = 1;
    for (int i = 3; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-tables") && tables)
            tables = 0;
        else if (!strcmp(argv[i], "--bindings") && !binding_path && i + 1 < argc)
            binding_path = argv[++i];
        else
            valid_options = 0;
    }
    if (argc < 3 || !valid_options ||
        (strcmp(argv[1], "--validate") && strcmp(argv[1], "--emit-qasm") &&
         strcmp(argv[1], "--resources") && strcmp(argv[1], "--state"))) {
        fputs(
            "usage: cket_backend --validate|--emit-qasm|--resources|--state quantum-ir.json [--no-tables] [--bindings values.json]\n",
            stderr);
        return 2;
    }
    int state = !strcmp(argv[1], "--state"), validate = !strcmp(argv[1], "--validate");
    char *text = qc_read_file(argv[2]);
    if (!text) {
        fprintf(stderr, "|c> backend: cannot read %s\n", argv[2]);
        return 1;
    }
    qc_context *c = qc_create(state, 7);
    if (c && !tables)
        qc_allow_tables(c, 0);
    if (c && binding_path) {
        char *bindings = qc_read_file(binding_path);
        char *bound = bindings ? qc_bind_document(c, text, bindings) : NULL;
        free(bindings);
        if (!bound) {
            fprintf(stderr, "|c> backend: invalid bindings %s: %s\n", binding_path, qc_error(c));
            free(text);
            qc_destroy(c);
            return 1;
        }
        free(text);
        text = bound;
    }
    if (!c || !qc_document(c, text, validate, state)) {
        fprintf(stderr, "|c> backend: %s\n", qc_error(c));
        free(text);
        qc_destroy(c);
        return 1;
    }
    free(text);
    int result = 0;
    if (!strcmp(argv[1], "--emit-qasm"))
        result = !qc_qasm(c, stdout);
    else if (!strcmp(argv[1], "--resources"))
        qc_resources(c, stdout);
    else if (state) {
        printf("{\"qubits\":%u,\"amplitudes\":[", qc_qubits(c));
        int first = 1;
        for (size_t i = 0; i < qc_state_size(c); ++i) {
            double complex a = qc_amplitude(c, i);
            double re = creal(a), im = cimag(a);
            if (re * re + im * im < 1e-24)
                continue;
            printf("%s{\"index\":%zu,\"real\":%.17g,\"imag\":%.17g}", first ? "" : ",", i, re, im);
            first = 0;
        }
        puts("]}");
    }
    qc_destroy(c);
    return result || ferror(stdout);
}
