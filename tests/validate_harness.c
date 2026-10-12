#define _GNU_SOURCE
#include "validation.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int tool(const char *mode) {
    if (!strcmp(mode, "success"))
        return 0;
    if (!strcmp(mode, "semantic")) {
        fputs("fixture.c:1: error: |c>: expected rejection\n", stderr);
        return 1;
    }
    if (!strcmp(mode, "unrelated")) {
        fputs("another tool rejected something\n", stderr);
        return 1;
    }
    if (!strcmp(mode, "wrong-line")) {
        fputs("|c>: unrelated rejection\nsource excerpt: expected rejection\n", stderr);
        return 1;
    }
    if (!strcmp(mode, "high-exit")) {
        fputs("|c>: expected rejection\n", stderr);
        return 139;
    }
    if (!strcmp(mode, "benign-asan")) {
        fputs("AddressSanitizer: initialization complete\n", stderr);
        return 0;
    }
    if (!strcmp(mode, "quoted-source")) {
        fputs("fixture.c:1: error: |c>: expected rejection\n"
              "    1 | puts(\"runtime error: ERROR: AddressSanitizer\");\n",
              stderr);
        return 1;
    }
    if (!strcmp(mode, "source-only")) {
        fputs("fixture.c:1: error: ordinary C syntax error\n"
              "    1 | puts(\"|c>: expected rejection\");\n",
              stderr);
        return 1;
    }
    if (!strcmp(mode, "nul")) {
        fputs("|c>: expected rejection\n", stderr);
        fputc(0, stderr);
        fputs("ERROR: AddressSanitizer: hidden synthetic report\n", stderr);
        return 1;
    }
    if (!strcmp(mode, "signal")) {
        (void)raise(SIGTERM);
        return 1;
    }
    if (!strcmp(mode, "timeout")) {
        sleep(4);
        return 1;
    }
    const char *message =
        !strcmp(mode, "asan")          ? "==42==ERROR: AddressSanitizer: synthetic report\n"
        : !strcmp(mode, "asan-deadly") ? "AddressSanitizer:DEADLYSIGNAL\n"
        : !strcmp(mode, "leak")        ? "==42==ERROR: LeakSanitizer: synthetic report\n"
        : !strcmp(mode, "ubsan")       ? "fixture.c:3:7: runtime error: synthetic report\n"
        : !strcmp(mode, "ice")         ? "fixture.c:1: internal compiler error: synthetic report\n"
                                       : NULL;
    if (!message)
        return 127;
    // A simultaneous semantic message must not conceal infrastructure faults.
    fputs("|c>: expected rejection\n", stderr);
    fputs(message, stderr);
    return !strcmp(mode, "ubsan") ? 0 : 1; // Recovering UBSan can exit successfully.
}

static int probe(char **argv) {
    validation v = {0};
    validation_path(v.root, sizeof v.root, "%s", argv[3]);
    validation_path(v.temp, sizeof v.temp, "%s", argv[4]);
    validation_path(v.log, sizeof v.log, "%s/child.log", v.temp);
    const char *mode = argv[2];
    validation_write(v.log, "|c>: expected rejection from an older invocation\n");
    if (!strcmp(mode, "setup"))
        validation_path(v.root, sizeof v.root, "%s/nonexistent-root", v.temp);
    char *command[] = {argv[0], "--tool", (char *)mode, NULL};
    if (!strcmp(mode, "missing"))
        command[0] = "/cket-validation/nonexistent-executable";
    int code = validation_run(&v, command, NULL, 1);
    char *log = validation_read(v.log);
    int accepted = validation_rejected(code, log, "expected rejection");
    free(log);
    if (!strcmp(mode, "success") || !strcmp(mode, "benign-asan"))
        return code;
    if (accepted) {
        puts("matching semantic rejection accepted");
        return 0;
    }
    puts("no matching semantic rejection");
    return 3;
}

static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        validation_fail("cannot read harness test clock");
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--tool"))
        return tool(argv[2]);
    if (argc == 5 && !strcmp(argv[1], "--probe"))
        return probe(argv);
    if (argc != 1)
        validation_fail("usage: validate_harness");
    validation v;
    validation_init(&v, argv[0]);
    char executable[4096], log[4096];
    if (!realpath(argv[0], executable))
        validation_fail("cannot resolve fault-injection executable");
    validation_path(log, sizeof log, "%s/probe.log", v.temp);

    const struct {
        const char *mode, *message;
        int status;
    } cases[] = {{"success", NULL, 0},
                 {"semantic", "matching semantic rejection accepted", 0},
                 {"unrelated", "no matching semantic rejection", 3},
                 {"wrong-line", "no matching semantic rejection", 3},
                 {"high-exit", "no matching semantic rejection", 3},
                 {"benign-asan", NULL, 0},
                 {"asan", "validation infrastructure failure", 1},
                 {"asan-deadly", "validation infrastructure failure", 1},
                 {"leak", "validation infrastructure failure", 1},
                 {"ubsan", "validation infrastructure failure", 1},
                 {"ice", "validation infrastructure failure", 1},
                 {"signal", "terminated by a signal", 1},
                 {"timeout", "timed out", 1},
                 {"setup", "setup or execution failed", 1},
                 {"missing", "setup or execution failed", 1},
                 {"quoted-source", "matching semantic rejection accepted", 0},
                 {"source-only", "no matching semantic rejection", 3},
                 {"nul", "embedded NUL in text log/artifact", 1}};

    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
        if (fflush(NULL))
            validation_fail("cannot flush harness test output");
        pid_t pid = fork();
        if (pid < 0)
            validation_fail("cannot start fault-injection probe");
        if (!pid) {
            if (setpgid(0, 0) || !freopen(log, "w", stderr) ||
                dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
                _exit(126);
            execl(executable, executable, "--probe", cases[i].mode, v.root, v.temp, (char *)NULL);
            _exit(127);
        }
        (void)setpgid(pid, pid);
        int status;
        double deadline = now() + 10;
        for (;;) {
            pid_t ended = waitpid(pid, &status, WNOHANG);
            if (ended == pid)
                break;
            if (ended < 0 && errno != EINTR)
                validation_fail("cannot wait for fault-injection probe");
            if (now() >= deadline) {
                (void)kill(-pid, SIGKILL);
                (void)waitpid(pid, &status, 0);
                validation_fail("fault-injection probe hung: %s", cases[i].mode);
            }
            struct timespec pause = {0, 10000000};
            (void)nanosleep(&pause, NULL);
        }
        char *text = validation_read(log);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != cases[i].status ||
            (cases[i].message && !strstr(text, cases[i].message)))
            validation_fail("fault-injection result mismatch: %s\n%s", cases[i].mode, text);
        free(text);
        printf("PASS validation harness: %s\n", cases[i].mode);
    }
    validation_cleanup(&v);
    puts(
        "18 validation harness checks passed; infrastructure faults cannot count as semantic rejections.");
    return 0;
}
