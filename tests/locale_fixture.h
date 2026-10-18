#ifndef CKET_TEST_LOCALE_FIXTURE_H
#define CKET_TEST_LOCALE_FIXTURE_H
#include "validation.h"
#include <stdlib.h>
#include <unistd.h>

static inline void validation_named_locale_fixture(
    validation *v, int remove, const char *name, const char *source, const char *charmap) {
    char path[4096];
    if (!remove) {
        validation_path(path, sizeof path, "%s/%s", v->temp, name);
        char *command[] = {
            "localedef", "--no-archive", "-i", (char *)source, "-f", (char *)charmap, path, NULL};
        if (validation_run(v, command, NULL, 90) || setenv("LOCPATH", v->temp, 1))
            validation_fail(
                "cannot build isolated locale fixture (requires localedef and locales sources)\n%s",
                validation_read(v->log));
        return;
    }
    const char *files[] = {"LC_CTYPE",
                           "LC_NUMERIC",
                           "LC_TIME",
                           "LC_COLLATE",
                           "LC_MONETARY",
                           "LC_MESSAGES/SYS_LC_MESSAGES",
                           "LC_PAPER",
                           "LC_NAME",
                           "LC_ADDRESS",
                           "LC_TELEPHONE",
                           "LC_MEASUREMENT",
                           "LC_IDENTIFICATION"};
    for (unsigned i = 0; i < sizeof files / sizeof *files; ++i) {
        validation_path(path, sizeof path, "%s/%s/%s", v->temp, name, files[i]);
        if (unlink(path))
            validation_fail("cannot remove generated locale fixture file");
    }
    validation_path(path, sizeof path, "%s/%s/LC_MESSAGES", v->temp, name);
    if (rmdir(path))
        validation_fail("cannot remove generated locale message directory");
    validation_path(path, sizeof path, "%s/%s", v->temp, name);
    if (rmdir(path))
        validation_fail("cannot remove generated locale fixture directory");
}

static inline void validation_locale_fixture(validation *v, int remove) {
    validation_named_locale_fixture(v, remove, "en_US.ISO-8859-1", "en_US", "ISO-8859-1");
}

#endif
