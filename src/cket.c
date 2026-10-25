#define _GNU_SOURCE
#include "quantum.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Native driver: GCC performs every part of parsing and compiling C. */
static char workspace[PATH_MAX], temporary[PATH_MAX];

static void cleanup(void) {
    if (!*temporary)
        return;
    const char *files[] = {"quantum.json", "gcc.ast", "host.o", "gcc-version.txt"};
    char path[PATH_MAX + 32];
    for (size_t i = 0; i < sizeof files / sizeof *files; ++i) {
        snprintf(path, sizeof path, "%s/%s", temporary, files[i]);
        unlink(path);
    }
    rmdir(temporary);
}

static int error(const char *message) {
    fprintf(stderr, "|c> compiler: %s\n", message);
    return 1;
}

static int command(char *const *args, const char *output) {
    pid_t pid = fork();
    if (pid < 0)
        return error("cannot start compiler subprocess");
    if (!pid) {
        if (output && !freopen(output, "w", stdout))
            _exit(126);
        execvp(args[0], args);
        perror(args[0]);
        _exit(127);
    }
    int status;
    pid_t result;
    do
        result = waitpid(pid, &status, 0);
    while (result < 0 && errno == EINTR);
    if (result < 0)
        return error("cannot wait for compiler subprocess");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static int copy_output(const char *input, const char *output) {
    FILE *in = fopen(input, "rb"), *out = output ? fopen(output, "wb") : stdout;
    if (!in || !out) {
        if (in)
            fclose(in);
        if (out && output)
            fclose(out);
        return error("cannot open compiler output");
    }
    char buffer[8192];
    size_t n;
    int bad = 0;
    while ((n = fread(buffer, 1, sizeof buffer, in)))
        if (fwrite(buffer, 1, n, out) != n) {
            bad = 1;
            break;
        }
    bad |= ferror(in);
    fclose(in);
    if (output)
        bad |= fclose(out) != 0;
    else
        bad |= fflush(out) != 0;
    return bad ? error("cannot write compiler output") : 0;
}

static int mkdir_parents(const char *path) {
    char buffer[PATH_MAX];
    if (strlen(path) >= sizeof buffer)
        return error("output path is too long");
    strcpy(buffer, path);
    for (char *p = buffer + 1; *p; ++p)
        if (*p == '/') {
            *p = 0;
            if (mkdir(buffer, 0777) && errno != EEXIST)
                return error("cannot create output directory");
            *p = '/';
        }
    return 0;
}

static void help(void) {
    puts("|c>: real C + quantum data (GCC + native C backend)\n"
         "usage: cket program.c [-o output] [GCC options]\n"
         "       --run [--seed N]\n"
         "       --emit-ast | --emit-ir | --emit-quantum-ir\n"
         "       --emit-qasm | --resources\n"
         "       --ctype-locale name (explicit static character profile)\n"
         "       --gcc compiler | --version | --help");
}

int main(int argc, char **argv) {
    const char *source = NULL, *output = NULL, *gcc = getenv("CC"), *seed = NULL;
    const char *ctype_locale = NULL;
    int action = 0;
    char *extra[argc + 1];
    int nextra = 0;
    if (!gcc || !*gcc)
        gcc = "gcc";
    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            help();
            return 0;
        }
        if (!strcmp(arg, "--version")) {
            puts("|c> / cket 1.0.0-rc.1 (experimental GCC + native C)");
            return 0;
        }
        if (!strcmp(arg, "-o") || !strcmp(arg, "--output") || !strcmp(arg, "--gcc") ||
            !strcmp(arg, "--seed") || !strcmp(arg, "--ctype-locale")) {
            if (++i == argc)
                return error("option requires an argument");
            if (!strcmp(arg, "--gcc"))
                gcc = argv[i];
            else if (!strcmp(arg, "--seed"))
                seed = argv[i];
            else if (!strcmp(arg, "--ctype-locale"))
                ctype_locale = argv[i];
            else
                output = argv[i];
            continue;
        }
        int selected = !strcmp(arg, "--emit-ir") || !strcmp(arg, "--emit-quantum-ir") ? 1
                       : !strcmp(arg, "--emit-ast")                                   ? 2
                       : !strcmp(arg, "--emit-qasm")                                  ? 3
                       : !strcmp(arg, "--resources")                                  ? 4
                       : !strcmp(arg, "--run")                                        ? 5
                                                                                      : 0;
        if (selected) {
            if (action)
                return error("select only one output action");
            action = selected;
        } else if (!strcmp(arg, "--"))
            continue;
        else if (!strncmp(arg, "--backend", 9))
            return error(
                "external backends were removed; execution uses the linked native C motor");
        else if (*arg == '-')
            extra[nextra++] = arg;
        else if (!source)
            source = arg;
        else
            return error("GCC options must be single tokens, e.g. -I/path or -DNAME=1");
    }
    if (!source) {
        help();
        return 2;
    }
    if (seed) {
        char *end;
        errno = 0;
        strtoull(seed, &end, 10);
        if (errno || !*seed || *seed == '-' || *end)
            return error("seed must be a nonnegative 64-bit integer");
    }
    char executable[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", executable, sizeof executable - 1);
    if (len < 0 || (size_t)len >= sizeof executable - 1)
        return error("cannot locate native driver");
    executable[len] = 0;
    char *slash = strrchr(executable, '/');
    if (!slash)
        return error("cannot locate build directory");
    *slash = 0;
    slash = strrchr(executable, '/');
    if (!slash)
        return error("cannot locate workspace");
    *slash = 0;
    strcpy(workspace, executable);
    const char *temporary_root = getenv("TMPDIR");
    if (!temporary_root || !*temporary_root)
        temporary_root = "/tmp";
    int temporary_size = snprintf(temporary, sizeof temporary, "%s/cket-XXXXXX", temporary_root);
    if (temporary_size < 0 || (size_t)temporary_size >= sizeof temporary)
        return error("temporary compilation path is too long");
    if (!mkdtemp(temporary))
        return error("cannot create temporary compilation directory");
    atexit(cleanup);
    char ir[PATH_MAX + 64], ast[PATH_MAX + 64], obj[PATH_MAX + 64], version_out[PATH_MAX + 64],
        expected_path[PATH_MAX + 32], plugin[PATH_MAX + 64], include[PATH_MAX + 16];
    snprintf(ir, sizeof ir, "%s/quantum.json", temporary);
    snprintf(ast, sizeof ast, "%s/gcc.ast", temporary);
    snprintf(obj, sizeof obj, "%s/host.o", temporary);
    snprintf(version_out, sizeof version_out, "%s/gcc-version.txt", temporary);
    snprintf(expected_path, sizeof expected_path, "%s/build/gcc-version.txt", workspace);
    char *version_args[] = {(char *)gcc, "-dumpfullversion", NULL};
    int status = command(version_args, version_out);
    if (status)
        return status;
    char *actual = qc_read_file(version_out), *expected = qc_read_file(expected_path);
    if (!actual || !expected || strcmp(actual, expected)) {
        free(actual);
        free(expected);
        return error(
            "GCC version differs from plugin build; run scripts/build.sh with matching CC/CXX");
    }
    free(actual);
    free(expected);
    snprintf(plugin, sizeof plugin, "-fplugin=%s/build/cket_plugin.so", workspace);
    snprintf(include, sizeof include, "-I%s/include", workspace);
    char ir_option[PATH_MAX + 128], ast_option[PATH_MAX + 128];
    snprintf(ir_option, sizeof ir_option, "-fplugin-arg-cket_plugin-ir=%s", ir);
    snprintf(ast_option, sizeof ast_option, "-fplugin-arg-cket_plugin-ast=%s", ast);
    char *compile_args[argc + 20];
    int n = 0;
    compile_args[n++] = (char *)gcc;
    compile_args[n++] = "-std=c11";
    compile_args[n++] = "-Werror=attributes";
    compile_args[n++] = "-D__CKET__=1";
    compile_args[n++] = include;
    compile_args[n++] = plugin;
    compile_args[n++] = ir_option;
    compile_args[n++] = ast_option;
    compile_args[n++] = "-c";
    compile_args[n++] = (char *)source;
    compile_args[n++] = "-o";
    compile_args[n++] = obj;
    for (int i = 0; i < nextra; ++i)
        compile_args[n++] = extra[i];
    compile_args[n] = NULL;
    if (output && mkdir_parents(output))
        return 1;
    status = command(compile_args, NULL);
    if (status) {
        if (action == 2 && !access(ast, R_OK))
            copy_output(ast, output);
        return status;
    }
    if (action == 2)
        return copy_output(ast, output);
    char *document = qc_read_file(ir);
    qc_context *context = qc_create(0, 0);
    if (!document || !context || !qc_document(context, document, 1, 0)) {
        fprintf(stderr,
                "|c> backend: %s\n",
                document ? qc_error(context) : "GCC did not produce Quantum IR");
        free(document);
        qc_destroy(context);
        return 1;
    }
    qc_destroy(context);
    if (action == 1) {
        free(document);
        return copy_output(ir, output);
    }
    if (action == 3 || action == 4) {
        context = qc_create(0, 0);
        if (!context || (ctype_locale && !qc_ctype_locale(context, ctype_locale)) ||
            !qc_document(context, document, 0, 0)) {
            fprintf(stderr, "|c> backend: %s\n", qc_error(context));
            free(document);
            qc_destroy(context);
            return 1;
        }
        free(document);
        FILE *out = output ? fopen(output, "w") : stdout;
        if (!out) {
            qc_destroy(context);
            return error("cannot open output");
        }
        if (action == 3)
            status = !qc_qasm(context, out);
        else {
            qc_resources(context, out);
            status = ferror(out);
        }
        if (output)
            status |= fclose(out) != 0;
        qc_destroy(context);
        return status;
    }
    free(document);
    char default_output[PATH_MAX], absolute_output[PATH_MAX], runtime[PATH_MAX + 32],
        library[PATH_MAX + 32];
    if (!output) {
        const char *base = strrchr(source, '/');
        base = base ? base + 1 : source;
        if (strlen(base) >= sizeof default_output)
            return error("source filename is too long");
        strcpy(default_output, base);
        char *dot = strrchr(default_output, '.');
        if (dot)
            *dot = 0;
        output = default_output;
    }
    if (mkdir_parents(output))
        return 1;
    snprintf(runtime, sizeof runtime, "%s/build/cket_runtime.o", workspace);
    snprintf(library, sizeof library, "%s/build/libcket_quantum.a", workspace);
    /* GCC driver options such as -fsanitize=..., -pthread and -l... affect
     * linking too. Dropping them produces unresolved runtime/library symbols
     * even though the host C compilation succeeds. Keep library options after
     * the objects so that ordinary static-library ordering still applies. */
    char *link_args[argc + 12];
    n = 0;
    link_args[n++] = (char *)gcc;
    link_args[n++] = obj;
    link_args[n++] = runtime;
    link_args[n++] = library;
    link_args[n++] = "-lm";
    for (int i = 0; i < nextra; ++i)
        link_args[n++] = extra[i];
    link_args[n++] = "-o";
    link_args[n++] = (char *)output;
    link_args[n] = NULL;
    status = command(link_args, NULL);
    if (status)
        return status;
    if (action == 5) {
        if (!realpath(output, absolute_output))
            return error("cannot locate generated executable");
        if (seed && setenv("CKET_SEED", seed, 1))
            return error("cannot set simulation seed");
        char *run_args[] = {absolute_output, NULL};
        return command(run_args, NULL);
    }
    printf("Built %s\nExecution backend: linked native C statevector motor\n", output);
    return 0;
}
