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

void validation_fail(const char *format, ...) {
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args);
    fputc('\n', stderr); exit(1);
}
void validation_path(char *out, size_t size, const char *format, ...) {
    va_list args; va_start(args, format); int n = vsnprintf(out, size, format, args); va_end(args);
    if (n < 0 || (size_t)n >= size) validation_fail("validation path or expression is too long");
}
void validation_init(validation *v, const char *executable) {
    if (!realpath(executable, v->root)) validation_fail("cannot resolve validation executable: %s", strerror(errno));
    char *p = strrchr(v->root, '/'); if (!p) validation_fail("invalid executable path"); *p = 0;
    p = strrchr(v->root, '/'); if (!p) validation_fail("invalid build directory"); *p = 0;
    validation_path(v->temp, sizeof v->temp, "%s/build/validation-XXXXXX", v->root);
    if (!mkdtemp(v->temp)) validation_fail("cannot create validation workspace: %s", strerror(errno));
    validation_path(v->log, sizeof v->log, "%s/command.log", v->temp);
}
static double seconds(void) {
    struct timespec t; if (clock_gettime(CLOCK_MONOTONIC, &t)) validation_fail("cannot read clock");
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
int validation_run(validation *v, char *const argv[], const char *output, unsigned timeout) {
    if (fflush(NULL)) validation_fail("cannot flush validation output");
    pid_t pid = fork(); if (pid < 0) validation_fail("cannot start validation subprocess");
    if (!pid) {
        if (setpgid(0, 0) || chdir(v->root) || !freopen(v->log, "w", stderr)) _exit(126);
        if (output ? !freopen(output, "w", stdout) : dup2(STDERR_FILENO, STDOUT_FILENO) < 0) _exit(126);
        execvp(argv[0], argv); perror(argv[0]); _exit(127);
    }
    (void)setpgid(pid, pid);
    int status = 0; double deadline = seconds() + timeout;
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0 && errno != EINTR) validation_fail("cannot wait for validation subprocess");
        if (seconds() >= deadline) {
            (void)kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            fprintf(stderr, "validation command timed out after %u seconds: %s\n", timeout, argv[0]);
            return 124;
        }
        struct timespec pause = {0, 10000000}; (void)nanosleep(&pause, NULL);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
char *validation_read(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) validation_fail("cannot read %s", path);
    if (fseek(f, 0, SEEK_END)) validation_fail("cannot seek %s", path);
    long n = ftell(f);
    if (n < 0 || n > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) validation_fail("invalid file size: %s", path);
    char *text = malloc((size_t)n + 1); if (!text) validation_fail("out of memory reading %s", path);
    if (fread(text, 1, (size_t)n, f) != (size_t)n || fclose(f)) validation_fail("cannot read %s", path);
    text[n] = 0; return text;
}
void validation_write(const char *path, const char *text) {
    FILE *f = fopen(path, "wb"); if (!f) validation_fail("cannot write %s", path);
    if (fputs(text, f) < 0 || fclose(f)) validation_fail("cannot write %s", path);
}
void validation_cleanup(validation *v) {
    DIR *d = opendir(v->temp); if (!d) return;
    struct dirent *e; char path[4096];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        validation_path(path, sizeof path, "%s/%s", v->temp, e->d_name); (void)unlink(path);
    }
    closedir(d); (void)rmdir(v->temp);
}
uint64_t validation_random(uint64_t *state) {
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}
uint64_t validation_bounded(uint64_t *state, uint64_t bound) {
    if (!bound) validation_fail("zero random bound");
    uint64_t threshold = -bound % bound, value;
    do value = validation_random(state); while (value < threshold);
    return value % bound;
}
