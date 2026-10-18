#define _GNU_SOURCE
#include "internal.h"
#include "locale_fixture.h"
#include "validation.h"
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("numeric locale operation failed: %s", qc_error(c));
}

static void scalar_numbers(void) {
    const struct {
        const char *text;
        double expected;
    } cases[] = {{"0.31", .31},
                 {"-0.31", -.31},
                 {"3.1e-1", .31},
                 {"3.1E+1", 31},
                 {"0", 0},
                 {"-0", 0},
                 {"1e-3", .001},
                 {"1.25e2", 125},
                 {"1e999", NAN},
                 {"1e-999", NAN},
                 {"true", NAN},
                 {"null", NAN}};

    locale_t previous = uselocale((locale_t)0);
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
        cj_doc d = {0};
        if (!cj_parse(&d, cases[i].text))
            validation_fail("numeric fixture parse failed: %s", d.error);
        double actual = cj_double(&d, 0);
        if (isnan(cases[i].expected) ? !isnan(actual) : actual != cases[i].expected)
            validation_fail("JSON number depended on LC_NUMERIC: %s", cases[i].text);
        if (uselocale((locale_t)0) != previous)
            validation_fail("JSON number conversion changed the thread locale");
        cj_free(&d);
        ++checks;
    }
}

static void phases_and_export(validation *v, unsigned mode) {
    for (unsigned simulate = 0; simulate < 2; ++simulate) {
        qc_context *c = qc_create((int)simulate, 7);
        if (!c)
            validation_fail("numeric locale state allocation failed");
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
        apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
        apply(c, "{\"op\":\"qphase\",\"angle\":0.31,\"controls\":[{\"id\":0,\"value\":1}]}");
        if (simulate) {
            if (qc_state_size(c) != 2 || cabs(qc_amplitude(c, 0) - 1 / sqrt(2.)) > 1e-10 ||
                cabs(qc_amplitude(c, 1) - cexp(I * .31) / sqrt(2.)) > 1e-10 || c->measure ||
                c->reset)
                validation_fail("numeric locale changed quantum amplitudes");
            apply(c, "{\"op\":\"qphase\",\"angle\":-3.1e-1,\"controls\":[{\"id\":0,\"value\":1}]}");
            for (size_t basis = 0; basis < qc_state_size(c); ++basis)
                if (cabs(qc_amplitude(c, basis) - 1 / sqrt(2.)) > 1e-10)
                    validation_fail("numeric locale phase inverse did not restore the state");
            ++checks;
        } else {
            apply(c, "{\"op\":\"qphase\",\"angle\":0.125}");
            char path[4096];
            validation_path(path, sizeof path, "%s/numeric-%u.qasm", v->temp, mode);
            FILE *output = fopen(path, "w");
            locale_t previous = uselocale((locale_t)0);
            if (!output || !qc_qasm(c, output) || fclose(output))
                validation_fail("numeric locale QASM export failed");
            if (uselocale((locale_t)0) != previous)
                validation_fail("QASM export changed the thread locale");
            char *text = validation_read(path);
            if (!strstr(text, "p(0.31) q[0];") || !strstr(text, "gphase(0.125);") ||
                strstr(text, "p(0,31)"))
                validation_fail("QASM output used a localized decimal separator: %s", text);
            free(text);
            ++checks;
            output = fopen("/dev/full", "w");
            if (!output || setvbuf(output, NULL, _IONBF, 0) || qc_qasm(c, output) ||
                uselocale((locale_t)0) != previous)
                validation_fail("failed QASM stream did not restore the thread locale");
            fclose(output);
            ++checks;
        }
        qc_destroy(c);
    }
}

static void native_programs(validation *v) {
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    for (unsigned thread = 0; thread < 2; ++thread)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], binary[4096], compiler[4096], program[4096];
            validation_path(source, sizeof source, "%s/numeric.c", v->temp);
            validation_path(binary, sizeof binary, "%s/numeric", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program,
                sizeof program,
                "#define _GNU_SOURCE\n#include <quantum.h>\n#include <locale.h>\n"
                "#include <stdio.h>\n#include <string.h>\nint main(void){%s"
                "quant bool q=superposition(0,1);if(q)phase(0.31);if(q)phase(-0.31);"
                "if(q)phase_flip();inverse_fourier(q);int bad=measure(q)!=1;reset(q);"
                "char text[32];snprintf(text,sizeof text,\"%%g\",0.31);"
                "bad|=strcmp(text,\"0,31\")!=0;%sreturn bad;}\n",
                thread
                    ? "locale_t numeric=newlocale(LC_NUMERIC_MASK,\"de_DE.UTF-8\",0);"
                      "if(!numeric)return 2;locale_t previous=uselocale(numeric);if(!previous)return 3;"
                    : "if(!setlocale(LC_NUMERIC,\"de_DE.UTF-8\"))return 2;",
                thread ? "uselocale(previous);freelocale(numeric);" : "");
            validation_write(source, program);
            char *command[] = {
                compiler, source, (char *)levels[level], "--run", "-o", binary, NULL};
            if (validation_run(v, command, NULL, 90))
                validation_fail("native phase failed under a comma locale: %s",
                                validation_read(v->log));
            ++checks;
        }
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_numeric_locale");
    validation v;
    validation_init(&v, argv[0]);
    validation_named_locale_fixture(&v, 0, "de_DE.UTF-8", "de_DE", "UTF-8");
    locale_t numeric_c = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t numeric_comma = newlocale(LC_NUMERIC_MASK, "de_DE.UTF-8", (locale_t)0);
    locale_t original_thread = uselocale((locale_t)0);
    char *original_global = strdup(setlocale(LC_NUMERIC, NULL));
    if (!numeric_c || !numeric_comma || !original_global)
        validation_fail("cannot create numeric locale fixtures");
    for (unsigned mode = 0; mode < 4; ++mode) {
        if (!uselocale(LC_GLOBAL_LOCALE) ||
            !setlocale(LC_NUMERIC, mode & 1 ? "de_DE.UTF-8" : "C") ||
            (mode >= 2 && !uselocale(mode == 2 ? numeric_comma : numeric_c)))
            validation_fail("cannot select numeric locale test mode");
        scalar_numbers();
        phases_and_export(&v, mode);
        if (strcmp(localeconv()->decimal_point, mode == 1 || mode == 2 ? "," : "."))
            validation_fail("numeric tests changed the caller's decimal separator");
    }
    if (!uselocale(LC_GLOBAL_LOCALE) || !setlocale(LC_NUMERIC, "C"))
        validation_fail("cannot restore locale before native tests");
    native_programs(&v);
    if (!setlocale(LC_NUMERIC, original_global) || !uselocale(original_thread))
        validation_fail("cannot restore original numeric locale");
    free(original_global);
    freelocale(numeric_c);
    freelocale(numeric_comma);
    validation_named_locale_fixture(&v, 1, "de_DE.UTF-8", "de_DE", "UTF-8");
    validation_cleanup(&v);
    printf(
        "%u numeric locale checks passed; JSON phases and QASM decimals preserve global/thread locales.\n",
        checks);
    return 0;
}
