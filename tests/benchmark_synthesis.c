#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reg(FILE *f, unsigned id, unsigned width) {
    fprintf(f, "{\"op\":\"reg\",\"id\":%u,\"bits\":%u,\"signed\":false}", id, width);
}

static void document(const char *path, const char *op, unsigned width) {
    FILE *f = fopen(path, "w");
    if (!f)
        validation_fail("cannot create growth IR");
    fprintf(f, "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[");
    for (unsigned id = 0; id < 2; ++id)
        fprintf(
            f,
            "{\"op\":\"qalloc\",\"id\":%u,\"width\":%u,\"signed\":false},{\"op\":\"qprepare\",\"id\":%u,\"lower\":0,\"upper\":%u},",
            id,
            width,
            id,
            (1u << width) - 1);
    unsigned output = !strcmp(op, "lt") ? 1 : width;
    fprintf(
        f,
        "{\"op\":\"qalloc\",\"id\":2,\"width\":%u,\"signed\":false},{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"args\":[",
        output,
        op,
        output);
    reg(f, 0, width);
    fputc(',', f);
    if (!strcmp(op, "div") || !strcmp(op, "mod")) {
        fprintf(f, "{\"op\":\"bor\",\"bits\":%u,\"signed\":false,\"args\":[", width);
        reg(f, 1, width);
        fprintf(f, ",{\"op\":\"const\",\"bits\":%u,\"signed\":false,\"value\":1}]}", width);
    } else
        reg(f, 1, width);
    fputs("]}}]}", f);
    if (ferror(f) || fclose(f))
        validation_fail("cannot finish growth IR");
}

static unsigned long stat(const char *text, const char *name) {
    char marker[80];
    validation_path(marker, sizeof marker, "%s:", name);
    for (const char *p = text; p && *p; p = strchr(p, '\n')) {
        if (*p == '\n')
            ++p;
        if (!strncmp(p, marker, strlen(marker))) {
            char *end;
            unsigned long n = strtoul(p + strlen(marker), &end, 10);
            if (end != p + strlen(marker) && (*end == '\n' || !*end || *end == '\r'))
                return n;
        }
    }
    validation_fail("missing resource metric: %s", name);
    return 0;
}

static unsigned long gates(const char *text) {
    unsigned long total = 0;
    char name[80];
    unsigned long n;
    const char *names[] = {"H", "X", "CNOT", "Toffoli", "MCX", "PHASE", "GLOBAL_PHASE"};
    for (const char *p = text; p && *p; p = strchr(p, '\n')) {
        if (*p == '\n')
            ++p;
        if (sscanf(p, "%79s %lu", name, &n) == 2)
            for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
                if (!strcmp(name, names[i]))
                    total += n;
    }
    return total;
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: benchmark_synthesis");
    validation v;
    validation_init(&v, argv[0]);
    char ir[4096], backend[4096], output[4096], csv[4096];
    validation_path(ir, sizeof ir, "%s/program.json", v.temp);
    validation_path(output, sizeof output, "%s/resources.txt", v.temp);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v.root);
    validation_path(csv, sizeof csv, "%s/build/synthesis-growth.csv", v.root);
    FILE *f = fopen(csv, "w");
    if (!f)
        validation_fail("cannot create growth CSV");
    fputs("operator,width,relevant_bits,gates,ancillas,peak_qubits,depth,gate_ratio\n", f);
    const char *ops[] = {"add", "sub", "mul", "div", "mod", "lt"};
    for (unsigned op = 0; op < 6; ++op) {
        unsigned long previous = 0;
        for (unsigned width = 4; width <= 16; width += 4) {
            document(ir, ops[op], width);
            char *command[] = {backend, "--resources", ir, "--no-tables", NULL};
            if (validation_run(&v, command, output, 30)) {
                char *log = validation_read(v.log);
                validation_fail("growth compilation failed: %s", log);
            }
            char *text = validation_read(output);
            unsigned long count = gates(text), ancillas = stat(text, "Ancillas");
            double ratio = previous ? (double)count / previous : 0;
            if ((previous && ratio > 9) || ancillas > 4 * width + 2)
                validation_fail("growth budget exceeded: %s width=%u ratio=%g ancillas=%lu",
                                ops[op],
                                width,
                                ratio,
                                ancillas);
            fprintf(f,
                    "%s,%u,%u,%lu,%lu,%lu,%lu,%.4f\n",
                    ops[op],
                    width,
                    width * 2,
                    count,
                    ancillas,
                    stat(text, "Peak qubits"),
                    stat(text, "Depth"),
                    ratio);
            printf("PASS growth %s width=%u: gates=%lu, ancillas=%lu\n",
                   ops[op],
                   width,
                   count,
                   ancillas);
            fflush(stdout);
            previous = count;
            free(text);
        }
    }
    if (ferror(f) || fclose(f))
        validation_fail("cannot finish growth CSV");
    printf("24 circuits compiled without tables; %s\n", csv);
    validation_cleanup(&v);
    return 0;
}
