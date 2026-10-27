#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static unsigned native, rejected, runtime;

static void
run_case(validation *v, const char *body, unsigned level, const char *diagnostic, int dynamic) {
    char source[4096], compiler[4096], binary[4096], text[8192];
    validation_path(source, sizeof source, "%s/entry.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(binary, sizeof binary, "%s/entry", v->temp);
    validation_path(text, sizeof text, "#define _GNU_SOURCE\n#include <quantum.h>\n%s\n", body);
    validation_write(source, text);
    char *command[] = {compiler,
                       source,
                       "--run",
                       (char *)levels[level],
                       "-o",
                       binary,
                       dynamic ? "-rdynamic" : NULL,
                       dynamic ? "-ldl" : NULL,
                       NULL};
    int status = validation_run(v, command, NULL, 90);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail("function entry contract failed %s: %s\n%s", levels[level], body, log);
    free(log);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_function_entries");
    const char *positive[] = {
        "static bool transform(bool);static bool early(void){return transform(false);}"
        "static bool transform(bool x){return !x;}int main(void){return !early();}",
        "static bool transform(bool);static bool(*saved)(bool)=transform;"
        "static bool transform(bool x){return !x;}int main(void){return !saved(false);}",
        "void transform(quant bool);void transform(quant bool x){x=!x;}"
        "int main(void){quant bool x=false;transform(x);return measure(x)!=1;}",
        "void inner(quant bool);void outer(quant bool x){inner(x);}"
        "void inner(quant bool x){x=!x;}int main(void){quant bool x=false;outer(x);return measure(x)!=1;}",
        "void unused(quant bool x){x=!x;}static int ordinary(int x){return x+1;}"
        "int main(void){return ordinary(2)!=3;}",
        "static int twice(int x){return 2*x;}void transform(quant bool x){int(*cpu)(int)=twice;"
        "int result=cpu(1);x^=result==2;}int main(void){quant bool x=false;transform(x);return measure(x)!=1;}",
    };
    const char *forward =
        "quantum function was used through a classical declaration before its definition";
    const char *pointer = "quantum function pointers need a checked quantum calling convention";
    const char *automatic = "quantum templates cannot be classical constructors or destructors";

    const struct {
        const char *body, *diagnostic;
    } negative[] = {
        {"void transform(bool);void early(void){transform(false);}void transform(quant bool x){x=!x;}"
         "int main(void){early();return 0;}",
         forward},
        {"void transform(bool);static void(*saved)(bool)=transform;void transform(quant bool x){x=!x;}"
         "int main(void){saved(false);return 0;}",
         forward},
        {"void transform(bool);void early(void){void(*saved)(bool)=transform;saved(false);}"
         "void transform(quant bool x){x=!x;}int main(void){early();return 0;}",
         forward},
        {"void transform(bool);static void(*saved[2])(bool)={transform,transform};"
         "void transform(quant bool x){x=!x;}int main(void){saved[0](false);return 0;}",
         forward},
        {"void transform(bool);struct callback{void(*call)(bool);};static struct callback saved={transform};"
         "void transform(quant bool x){x=!x;}int main(void){saved.call(false);return 0;}",
         forward},
        {"void transform(quant bool x){x=!x;}static void(*saved)(bool)=transform;"
         "int main(void){saved(false);return 0;}",
         pointer},
        {"void transform(quant bool);static void(*saved)(bool)=transform;"
         "void transform(quant bool x){x=!x;}int main(void){saved(false);return 0;}",
         pointer},
        {"bool transform(bool);static bool early(void){return transform(false);}"
         "quant bool transform(quant const bool x){return !x;}int main(void){return early();}",
         forward},
        {"void transform(bool);static void use(void(*fn)(bool)){fn(false);}"
         "void early(void){use(transform);}void transform(quant bool x){x=!x;}"
         "int main(void){early();return 0;}",
         forward},
        {"quant void automatic(void)__attribute__((constructor));quant void automatic(void){}"
         "int main(void){return 0;}",
         automatic},
        {"quant void automatic(void)__attribute__((destructor));quant void automatic(void){}"
         "int main(void){return 0;}",
         automatic},
    };

    const char *dynamic =
        "#include <dlfcn.h>\nvoid transform(quant bool x){x=!x;}"
        "int main(void){void(*call)(bool)=(void(*)(bool))dlsym(RTLD_DEFAULT,\"transform\");"
        "if(!call)return 5;call(false);return 7;}";
    validation v;
    validation_init(&v, argv[0]);
    for (unsigned level = 0; level < 5; ++level) {
        for (unsigned test = 0; test < sizeof positive / sizeof *positive; ++test) {
            run_case(&v, positive[test], level, NULL, 0);
            ++native;
        }
        for (unsigned test = 0; test < sizeof negative / sizeof *negative; ++test) {
            run_case(&v, negative[test].body, level, negative[test].diagnostic, 0);
            ++rejected;
        }
        run_case(
            &v, dynamic, level, "quantum function cannot use the classical calling convention", 1);
        ++runtime;
    }
    validation_cleanup(&v);
    if (native != 30 || rejected != 55 || runtime != 5)
        validation_fail("function entry coverage count mismatch");
    puts(
        "90 function entry checks passed; 30 native C/quantum compositions, 55 compile-time ABI rejections and 5 explicit runtime entry rejections.");
    return 0;
}
