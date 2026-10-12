#define _GNU_SOURCE
#include "validation.h"
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

_Noreturn void validation_fail(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

void validation_path(char *out, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(out, size, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= size)
        validation_fail("validation path or expression is too long");
}

void validation_init(validation *v, const char *executable) {
    if (!realpath(executable, v->root))
        validation_fail("cannot resolve validation executable: %s", strerror(errno));
    char *p = strrchr(v->root, '/');
    if (!p)
        validation_fail("invalid executable path");
    *p = 0;
    p = strrchr(v->root, '/');
    if (!p)
        validation_fail("invalid build directory");
    *p = 0;
    validation_path(v->temp, sizeof v->temp, "%s/build/validation-XXXXXX", v->root);
    if (!mkdtemp(v->temp))
        validation_fail("cannot create validation workspace: %s", strerror(errno));
    validation_path(v->log, sizeof v->log, "%s/command.log", v->temp);
}

static double seconds(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        validation_fail("cannot read clock");
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int validation_run(validation *v, char *const argv[], const char *output, unsigned timeout) {
    return validation_run_input(v, argv, NULL, output, timeout);
}

static int source_excerpt(const char *line, const char *end) {
    while (line < end && (*line == ' ' || *line == '\t'))
        ++line;
    const char *digits = line;
    while (line < end && *line >= '0' && *line <= '9')
        ++line;
    if (line == digits)
        return 0;
    while (line < end && (*line == ' ' || *line == '\t'))
        ++line;
    return line < end && *line == '|';
}

static const char *diagnostic_fault(const char *text) {
    static const char *const markers[] = {"ERROR: AddressSanitizer",
                                          "AddressSanitizer:DEADLYSIGNAL",
                                          "SUMMARY: AddressSanitizer",
                                          "ERROR: LeakSanitizer",
                                          "ERROR: UndefinedBehaviorSanitizer",
                                          "UndefinedBehaviorSanitizer:DEADLYSIGNAL",
                                          "SUMMARY: UndefinedBehaviorSanitizer",
                                          "WARNING: ThreadSanitizer",
                                          "FATAL: ThreadSanitizer",
                                          "WARNING: MemorySanitizer",
                                          "runtime error:",
                                          "internal compiler error:",
                                          NULL};
    if (text)
        for (const char *line = text; *line;) {
            const char *end = strchr(line, '\n');
            if (!end)
                end = line + strlen(line);
            // GCC echoes source strings on numbered excerpt lines. They are
            // neither sanitizer reports nor semantic diagnostic messages.
            if (!source_excerpt(line, end))
                for (unsigned i = 0; markers[i]; ++i) {
                    const char *match = strstr(line, markers[i]);
                    if (match && match < end && strlen(markers[i]) <= (size_t)(end - match))
                        return markers[i];
                }
            line = *end ? end + 1 : end;
        }
    return NULL;
}

int validation_rejected(int status, const char *diagnostics, const char *expected) {
    if (status != 1 || !diagnostics || diagnostic_fault(diagnostics))
        return 0;
    for (const char *line = diagnostics; *line;) {
        const char *end = strchr(line, '\n');
        if (!end)
            end = line + strlen(line);
        const char *prefix = strstr(line, "|c>");
        if (prefix && prefix < end && !source_excerpt(line, end)) {
            const char *match = expected ? strstr(prefix, expected) : prefix;
            if (match && match < end && (!expected || strlen(expected) <= (size_t)(end - match)))
                return 1;
        }
        line = *end ? end + 1 : end;
    }
    return 0;
}

static _Noreturn void command_fault(validation *v, const char *executable, const char *reason) {
    char *text = validation_read(v->log);
    validation_fail("validation infrastructure failure: %s: %s\n%s", executable, reason, text);
}

int validation_run_input(
    validation *v, char *const argv[], const char *input, const char *output, unsigned timeout) {
    if (fflush(NULL))
        validation_fail("cannot flush validation output");
    // Setup failure must not leave an older semantic diagnostic in the log.
    validation_write(v->log, "");
    pid_t pid = fork();
    if (pid < 0)
        validation_fail("cannot start validation subprocess");
    if (!pid) {
        if (setpgid(0, 0) || chdir(v->root) || !freopen(v->log, "w", stderr))
            _exit(126);
        if (!freopen(input ? input : "/dev/null", "r", stdin))
            _exit(126);
        if (output ? !freopen(output, "w", stdout) : dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
            _exit(126);
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    (void)setpgid(pid, pid);
    int status = 0;
    double deadline = seconds() + timeout;
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid)
            break;
        if (r < 0 && errno != EINTR)
            validation_fail("cannot wait for validation subprocess");
        if (seconds() >= deadline) {
            (void)kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            command_fault(v, argv[0], "timed out");
        }
        struct timespec pause = {0, 10000000};
        (void)nanosleep(&pause, NULL);
    }
    if (!WIFEXITED(status))
        command_fault(v, argv[0], "terminated by a signal");
    int result = WEXITSTATUS(status);
    if (result == 126 || result == 127)
        command_fault(v, argv[0], "subprocess setup or execution failed");
    char *diagnostics = validation_read(v->log);
    const char *fault = diagnostic_fault(diagnostics);
    if (fault)
        command_fault(v, argv[0], fault);
    free(diagnostics);
    return result;
}

char *validation_read(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        validation_fail("cannot read %s", path);
    if (fseek(f, 0, SEEK_END))
        validation_fail("cannot seek %s", path);
    long n = ftell(f);
    if (n < 0 || n > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET))
        validation_fail("invalid file size: %s", path);
    char *text = malloc((size_t)n + 1);
    if (!text)
        validation_fail("out of memory reading %s", path);
    if (fread(text, 1, (size_t)n, f) != (size_t)n || fclose(f))
        validation_fail("cannot read %s", path);
    if (memchr(text, 0, (size_t)n))
        validation_fail("validation infrastructure failure: embedded NUL in text log/artifact: %s",
                        path);
    text[n] = 0;
    return text;
}

void validation_write(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f)
        validation_fail("cannot write %s", path);
    if (fputs(text, f) < 0 || fclose(f))
        validation_fail("cannot write %s", path);
}

void validation_cleanup(validation *v) {
    DIR *d = opendir(v->temp);
    if (!d)
        return;
    struct dirent *e;
    char path[4096];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        validation_path(path, sizeof path, "%s/%s", v->temp, e->d_name);
        (void)unlink(path);
    }
    closedir(d);
    (void)rmdir(v->temp);
}

uint64_t validation_random(uint64_t *state) {
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

uint64_t validation_bounded(uint64_t *state, uint64_t bound) {
    if (!bound)
        validation_fail("zero random bound");
    uint64_t threshold = -bound % bound, value;
    do
        value = validation_random(state);
    while (value < threshold);
    return value % bound;
}
