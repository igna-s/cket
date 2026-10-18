#include "internal.h"
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const char *functions[] = {"memcmp", "strncmp"};

static char *compile_case(validation *v,
                          const char *program,
                          unsigned level,
                          unsigned builtins,
                          int execute,
                          int trace,
                          const char *diagnostic) {
    char source[4096], compiler[4096], output[4096];
    validation_path(source, sizeof source, "%s/snapshot.c", v->temp);
    validation_path(output, sizeof output, "%s/snapshot.out", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_write(source, program);
    char *command[16] = {compiler,
                         source,
                         execute ? "--run" : "--emit-ir",
                         (char *)levels[level],
                         builtins ? "-fbuiltin" : "-fno-builtin",
                         "-o",
                         output};
    unsigned count = 7;
    if (trace) {
        command[count++] = "-Wl,--wrap=__cket_apply";
        command[count++] = "-Wl,--wrap=__cket_apply_values";
        command[count++] = "-Wl,--wrap=__cket_apply_phase";
        command[count++] = "-Wl,--wrap=__cket_measure";
        command[count++] = "-Wl,--wrap=__cket_read_byte";
    }
    command[count] = NULL;
    int status = validation_run(v, command, NULL, 240);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail(
            "byte snapshot %s builtins=%u failed:\n%s\n%s", levels[level], builtins, program, log);
    free(log);
    return !execute && !diagnostic ? validation_read(output) : NULL;
}

static void guarded_cpu_reads(validation *v) {
    const char *programs[] = {
        "#define _POSIX_C_SOURCE 200809L\n#include <quantum.h>\n#include <string.h>\n"
        "#include <stdlib.h>\n#include <sys/mman.h>\n#include <unistd.h>\n"
        "_Alignas(4096) static unsigned char reference[8192];\n"
        "int main(void){\n"
        "if(sysconf(_SC_PAGESIZE)!=4096)abort();reference[4095]=0;\n"
        "if(mprotect(reference+4096,4096,PROT_NONE))abort();\n"
        "quant uint16_t input=0;quant bool result=strncmp((const char*)&input,(const char*)reference+4095,2)<=0;\n"
        "if(mprotect(reference+4096,4096,PROT_READ|PROT_WRITE))abort();\n"
        "unsigned actual=measure(result);measure(input);return actual!=1;\n}\n",
        "#define _POSIX_C_SOURCE 200809L\n#include <stdlib.h>\n#include <sys/mman.h>\n#include <unistd.h>\n"
        "extern unsigned char __cket_read_byte(const void*,unsigned char);\n"
        "_Alignas(4096) static unsigned char reference[8192];\n"
        "int main(void){\n"
        "if(sysconf(_SC_PAGESIZE)!=4096)abort();reference[4095]=17;\n"
        "if(mprotect(reference+4096,4096,PROT_NONE))abort();\n"
        "int failed=__cket_read_byte(NULL,0)!=0 || __cket_read_byte(reference+4096,0)!=0 || "
        "__cket_read_byte(reference+4095,1)!=17;\n"
        "if(mprotect(reference+4096,4096,PROT_READ|PROT_WRITE))abort();return failed;\n}\n",
    };
    for (unsigned profile = 0; profile < 2; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                free(compile_case(v, programs[profile], level, builtins, 1, 0, NULL));
                ++checks;
            }
    puts(
        "PASS 20 CPU read guards: NUL before an inaccessible page, disabled NULL/protected addresses and one enabled byte; five GCC levels and both builtin modes");
    fflush(stdout);
}

static void states(validation *v) {
    const struct {
        const char *setup, *address, *mutation;
        unsigned width, count, controlled, signed_input;
    } cases[] = {
        {"unsigned char ref=65;ref=17;", "&ref", "ref=99;", 8, 1, 0, 0},
        {"unsigned char ref[3]={1,65,2};ref[1]=200;", "ref+1", "ref[1]=99;", 8, 1, 1, 0},
        {"struct {unsigned char a,b;} ref={1,65};ref.b=17;",
         "(char*)&ref+1",
         "ref.b=99;",
         8,
         1,
         0,
         0},
        {"unsigned char ref=65;ref=128;", "&ref", "ref=99;", 8, 1, 0, 1},
        {"uint16_t ref=0x4141;ref=0xab00;", "&ref", "ref=99;", 16, 2, 0, 0},
        {"uint16_t ref=0x4141;ref=0xab41;", "&ref", "ref=99;", 16, 2, 0, 0},
        {"unsigned char ref[8192]={0};ref[8191]=17;", "ref+8191", "ref[8191]=99;", 8, 1, 0, 0},
        {"double ref=1.0;ref=2.0;", "(char*)&ref+7", "ref=3.0;", 8, 1, 0, 0},
        {"unsigned char ref[2];ref[0]=0;", "ref", "ref[0]=99;ref[1]=88;", 16, 2, 0, 0},
    };

    unsigned count = 0;
    for (unsigned function = 0; function < 2; ++function)
        for (unsigned profile = 0; profile < sizeof cases / sizeof *cases; ++profile)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins)
                    for (unsigned tables = 0; tables < 2; ++tables) {
                        if (profile == 8 && !function)
                            continue;
                        if (cases[profile].width == 16 && (level != 2 || tables))
                            continue;
                        char program[8192];
                        validation_path(
                            program,
                            sizeof program,
                            "#include <quantum.h>\n#include <string.h>\n"
                            "#define SNAPSHOT_TABLES %u\n#define SNAPSHOT_WIDTH %u\n"
                            "#define SNAPSHOT_STRING %u\n#define SNAPSHOT_CONTROL %u\n"
                            "#include \"%s/tests/byte_snapshot_trace.inc\"\n"
                            "int main(void) {\n    %s\n"
                            "    snapshot_set_reference((const void*)(%s),%u);\n"
                            "    %s\n    if(input==3) phase(.31);\n"
                            "    quant bool result=%s%s((const char*)&input,(const char*)(%s),%u)<=0;\n"
                            "    %s snapshot_mutated=1;\n    if(result) phase(.17);\n"
                            "    measure(input);\n    return snapshot_finish();\n}\n",
                            tables,
                            cases[profile].width,
                            function,
                            cases[profile].controlled,
                            v->root,
                            cases[profile].setup,
                            cases[profile].address,
                            cases[profile].count,
                            cases[profile].signed_input ? "quant int8_t input=0;fourier(input);"
                            : cases[profile].width == 16
                                ? "quant uint16_t input=superposition(0,65535);"
                                : "quant uint8_t input=superposition(0,255);",
                            cases[profile].controlled ? "input<128 && " : "",
                            functions[function],
                            cases[profile].address,
                            cases[profile].count,
                            cases[profile].mutation);
                        free(compile_case(v, program, level, builtins, 1, 1, NULL));
                        ++count;
                    }
    checks += count;
    printf(
        "PASS %u native complete-state byte snapshots: all byte patterns, signed bytes, two-byte NUL/tail cases, coherent guards, actual CPU bindings, phases and saved inverses\n",
        count);
    fflush(stdout);
}

static void native(validation *v) {
    const char *bodies[] = {
        "unsigned failures=0;for(unsigned i=0;i<256;++i){unsigned char ref=65;ref=(i*17+3)&255;unsigned old=ref;"
        "quant uint8_t input=i;"
        "CHECK_REL(<);CHECK_REL(<=);CHECK_REL(>);CHECK_REL(>=);CHECK_REL(==);CHECK_REL(!=);"
        "reset(input);}return failures!=0;",
        "unsigned failures=0;for(unsigned i=0;i<256;++i){unsigned char ref=65;ref=(i*17+3)&255;unsigned old=ref;"
        "quant uint8_t input=i;quant uint8_t raw=COMPARE((const char*)&input,(const char*)&ref,1);"
        "ref=99;unsigned actual=measure(raw);reset(raw);reset(input);failures+=actual!=(uint8_t)(i-old)||ref!=99;}return failures!=0;",
        "unsigned char ref=65;ref=17;quant uint8_t input=superposition(0,255);"
        "quant bool result=borrowed(&input,&ref);ref=99;unsigned actual=measure(result),x=measure(input);"
        "return actual!=(x<=17)||ref!=99;",
        "const unsigned char ref=make_byte(17);quant uint8_t input=superposition(0,255);"
        "quant bool result=COMPARE((const char*)&input,(const char*)&ref,1)<=0;"
        "unsigned actual=measure(result),x=measure(input);return calls!=1||actual!=(x<=17);",
        "unsigned failures=0;uint16_t values[]={0,1,65,66,255,256,0xab00,0xab41};"
        "for(unsigned k=0;k<8;++k){uint16_t ref=0x4141;ref=k&1?0xab00:0xab41;uint16_t old=ref;"
        "quant uint16_t input=values[k];quant bool result=COMPARE((const char*)&input,(const char*)&ref,2)<=0;"
        "ref=99;unsigned actual=measure(result);reset(result);reset(input);"
        "failures+=actual!=(COMPARE((const char*)&values[k],(const char*)&old,2)<=0)||ref!=99;}return failures!=0;",
        "unsigned char ref;quant uint16_t input=0;"
        "quant bool result=COMPARE((const char*)&input+2,(const char*)&ref+1,0)==0;"
        "unsigned actual=measure(result);measure(input);return actual!=1;",
        "unsigned char ref[3]={1,65,2};ref[1]=17;quant uint8_t input=superposition(0,255);"
        "quant bool result=COMPARE((const char*)ref+1,(const char*)&input,1)<0;ref[1]=99;"
        "unsigned actual=measure(result),x=measure(input);return actual!=(17<x)||ref[1]!=99;",
    };
    unsigned count = 0;
    for (unsigned function = 0; function < 2; ++function)
        for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins) {
                    char program[8192];
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\n#define COMPARE %s\n"
                        "#define CHECK_REL(rel) {quant bool r=COMPARE((const char*)&input,(const char*)&ref,1)rel 0;"
                        "ref^=255;unsigned actual=measure(r);reset(r);failures+=actual!=((int)i-(int)old rel 0);ref=old;}\n"
                        "static unsigned calls;\nstatic unsigned char make_byte(unsigned n){++calls;return n;}\n"
                        "static quant bool borrowed(quant const uint8_t *input,const void *ref){"
                        "return COMPARE((const char*)input,(const char*)ref,1)<=0;}\n"
                        "int main(void){\n%s\n}\n",
                        functions[function],
                        bodies[profile]);
                    free(compile_case(v, program, level, builtins, 1, 0, NULL));
                    ++count;
                }
    checks += count;
    printf(
        "PASS %u native CPU snapshot programs: six relations over every byte, raw results, helper borrows, callback initialization, word endpoints, zero counts and reversed operands\n",
        count);
    fflush(stdout);
}

static void rejected(validation *v) {
    const struct {
        const char *setup, *address, *diagnostic;
    } cases[] = {
        {"volatile unsigned char ref=2;", "&ref", "volatile/atomic CPU access contract"},
        {"volatile unsigned char ref[2]={2,3};", "ref", "volatile/atomic CPU access contract"},
        {"_Atomic unsigned char ref=2;", "&ref", "volatile/atomic CPU access contract"},
        {"struct {unsigned char x;volatile unsigned char y;} ref={1,2};",
         "&ref",
         "volatile/atomic CPU access contract"},
        {"struct {unsigned char x;_Atomic unsigned char y;} ref={1,2};",
         "&ref",
         "volatile/atomic CPU access contract"},
        {"char buffer=2;char *ref=&buffer;", "ref", "known CPU object storage"},
        {"char ref[1]={2};", "ref+2", "outside its checked storage bounds"},
        {"char ref[1]={2};", "&ref[2]", "outside its checked storage bounds"},
        {"unsigned n=2;char ref[n];ref[0]=2;", "ref", "known CPU object storage"},
    };

    unsigned count = 0;
    for (unsigned function = 0; function < 2; ++function)
        for (unsigned profile = 0; profile < sizeof cases / sizeof *cases; ++profile)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins) {
                    char program[8192];
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\nint main(void){%s\n"
                        "quant uint8_t input=1;quant bool result=%s((const char*)&input,(const char*)(%s),1)<0;"
                        "measure(result);measure(input);return 0;}\n",
                        cases[profile].setup,
                        functions[function],
                        cases[profile].address);
                    free(
                        compile_case(v, program, level, builtins, 0, 0, cases[profile].diagnostic));
                    ++count;
                }
    checks += count;
    printf(
        "PASS %u CPU byte-boundary rejections: volatile/atomic scalars and members, unchecked pointers, offsets and dynamic storage extents\n",
        count);
    fflush(stdout);
}

static void templates(validation *v) {
    for (unsigned function = 0; function < 2; ++function)
        for (unsigned profile = 0; profile < 3; ++profile)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins) {
                    char program[8192];
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\nint main(void){%s\n"
                        "quant uint8_t input=superposition(0,255);quant bool result=%s((const char*)&input,(const char*)(%s),%u)<0;"
                        "measure(result);measure(input);return 0;}\n",
                        profile == 0   ? "unsigned char ref[8192]={0};ref[8191]=17;"
                        : profile == 1 ? "const unsigned char ref=17;"
                                       : "unsigned char ref;",
                        functions[function],
                        profile == 0   ? "ref+8191"
                        : profile == 1 ? "&ref"
                                       : "&ref+1",
                        profile == 2 ? 0 : 1);
                    char *ir = compile_case(v, program, level, builtins, 0, 0, NULL);
                    cj_doc d = {0};
                    if (!cj_parse(&d, ir))
                        validation_fail("invalid snapshot template JSON");
                    int list = cj_get(&d, 0, "instructions");
                    unsigned bindings = 0;
                    for (int token = list + 1; token < d.tokens[list].next;
                         token = d.tokens[token].next)
                        if (cj_equal(&d, cj_get(&d, token, "op"), "qbind")) {
                            int parameters = cj_get(&d, token, "parameters");
                            if (d.tokens[parameters].children != 1)
                                validation_fail(
                                    "one-byte CPU view did not capture exactly one value");
                            ++bindings;
                        }
                    if (bindings != (profile == 0 ? 1u : 0u))
                        validation_fail(
                            "unused bytes, constant operands or zero count acquired CPU bindings");
                    qc_context *c = qc_create(0, 7);
                    if (!c || !qc_document(c, ir, 1, 0) || c->structural != (profile == 0))
                        validation_fail("snapshot template structural/numerical contract mismatch");
                    FILE *sink = tmpfile();
                    if (!sink || qc_qasm(c, sink) != (profile != 0))
                        validation_fail(
                            "unbound snapshot exported or constant circuit failed export");
                    fclose(sink);
                    qc_destroy(c);
                    cj_free(&d);
                    free(ir);
                    ++checks;
                }
    puts(
        "PASS 60 CPU snapshot template/export contracts: selected-byte footprint, constant export and zero-count capture avoidance");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_byte_snapshots");
    validation v;
    validation_init(&v, argv[0]);
    guarded_cpu_reads(&v);
    states(&v);
    native(&v);
    rejected(&v);
    templates(&v);
    validation_cleanup(&v);
    printf(
        "%u CPU byte snapshot checks passed; dynamic addresses, volatile/atomic access and complete quantum C11 remain pending.\n",
        checks);
    return 0;
}
