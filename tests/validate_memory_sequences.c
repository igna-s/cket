#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory_sequence_state.inc"

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};

typedef struct {
    const char *name, *program, *diagnostic;
} sequence_case;

static const sequence_case
    native_cases[] =
        {
            {"fill-then-memcmp",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t a[2]={17,34};memset(a,b,2);quant bool r=memcmp(a,\"\\x01\\x01\",2)==0;unsigned z=measure(r),x=measure(a[0]),y=measure(a[1]),v=measure(b);return x!=v||y!=v||z!=v;}\n",
             NULL},
            {"fill-then-strncmp",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t a[2]={17,34};memset(a,b,1);const char ref[2]={0,99};quant bool r=strncmp((char*)a,ref,2)<=0;unsigned z=measure(r),x=measure(a[0]),y=measure(a[1]),v=measure(b);return x!=v||y!=34||z!=!v;}\n",
             NULL},
            {"fill-then-copy",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=17,y=0;memset(&x,b,1);memcpy(&y,&x,1);unsigned a=measure(x),d=measure(y),v=measure(b);return a!=v||d!=v;}\n",
             NULL},
            {"copy-then-fill",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t source=b+1,destination=37;memcpy(&destination,&source,1);memset(&destination,17,1);unsigned x=measure(destination);measure(b);return x!=17;}\n",
             NULL},
            {"live-comparison-before-fill",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=b+1;quant bool r=memcmp(&x,\"\\x01\",1)<=0;memset(&x,17,1);unsigned a=measure(x),z=measure(r),v=measure(b);return a!=17||z!=!v;}\n",
             NULL},
            {"cpu-reference-before-fill",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){unsigned char ref=17;quant bool b=superposition(0,1);quant uint8_t x=b+16;quant bool r=memcmp(&x,&ref,1)<=0;ref=99;memset(&x,17,1);unsigned a=measure(x),z=measure(r);measure(b);return a!=17||z!=1||ref!=99;}\n",
             NULL},
            {"memcmp-fill-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t source=b,x=5;const unsigned char ref=1;memset(&x,memcmp(&source,&ref,1),1);unsigned a=measure(x),v=measure(b);return a!=(v?0u:255u);}\n",
             NULL},
            {"self-comparison-fill",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=b+1;memset(&x,strncmp((char*)&x,\"\\x01\",1),1);unsigned a=measure(x),v=measure(b);return a!=v;}\n",
             NULL},
            {"ctype-fill-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,islower(b?97:65),1);unsigned a=measure(x),v=measure(b);return a!=(unsigned char)islower(v?97:65);}\n",
             NULL},
            {"conversion-fill-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,tolower((int)b+65),1);unsigned a=measure(x),v=measure(b);return a!=(unsigned)tolower((int)v+65);}\n",
             NULL},
            {"division-fill-value", "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,div((int)b+5,2).rem,1);unsigned a=measure(x),v=measure(b);return a!=(unsigned)div((int)v+5,2).rem;}\n", NULL},
            {"absolute-fill-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,abs((int)b-1),1);unsigned a=measure(x),v=measure(b);return a!=(unsigned)abs((int)v-1);}\n",
             NULL},
            {"cpu-effect-then-comparison",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){unsigned n=17;quant uint8_t x=5;memset(&x,++n,1);const unsigned char ref=18;quant bool r=memcmp(&x,&ref,1)==0;n=99;unsigned z=measure(r),a=measure(x);return a!=18||z!=1||n!=99;}\n",
             NULL},
            {"comma-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=0;quant uint8_t y=(memset(&x,b,1),x+1);unsigned z=measure(y),a=measure(x),v=measure(b);return a!=v||z!=v+1;}\n",
             NULL},
            {"nested-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant uint8_t x=5,y=7;memset(&x,(memset(&y,17,1),y+1),1);unsigned a=measure(x),z=measure(y);return a!=18||z!=17;}\n",
             NULL},
            {"coherent-comma-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;quant uint8_t y=b?(memset(&x,17,1),x):x;if(y==17)phase(.17);unsigned z=measure(y),a=measure(x),v=measure(b);return a!=(v?17u:5u)||z!=a;}\n",
             NULL},
            {"unsequenced-comma",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=0,y=7;memset(&x,(memset(&y,17,1),b++ + b),1);measure(x);measure(y);return measure(b);}\n",
             "unsequenced quantum modification and access"},
            {"call-body-indeterminate-order",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;quant uint8_t y=(memset(&x,b,1),x)+x;unsigned z=measure(y),a=measure(x),v=measure(b);return a!=v||(z!=2*v&&z!=v+5);}\n",
             NULL},
            {"copy-comma-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=b+1,y=0;quant bool r=(memcpy(&y,&x,1),y==2);unsigned z=measure(r),a=measure(y),v=measure(b);return z!=v||a!=v+1;}\n",
             NULL},
            {"move-comma-value",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t a[2]={b+1,34};quant bool r=(memmove(a+1,a,1),a[1]==2);unsigned z=measure(r),x=measure(a[0]),y=measure(a[1]),v=measure(b);return z!=v||x!=v+1||y!=x;}\n",
             NULL},
            {"logical-and",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;quant bool r=b&&(memset(&x,17,1),x==17);unsigned z=measure(r),a=measure(x),v=measure(b);return z!=v||a!=(v?17u:5u);}\n",
             NULL},
            {"logical-or",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;quant bool r=b||(memset(&x,17,1),x==17);unsigned z=measure(r),a=measure(x),v=measure(b);return z!=1||a!=(v?5u:17u);}\n",
             NULL},
            {"previous-cpu-binding",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){unsigned n=17;quant bool b=superposition(0,1);quant uint8_t x=5;quant uint8_t y=n+(memset(&x,b,1),x);n=99;unsigned z=measure(y),a=measure(x),v=measure(b);return z!=17+v||a!=v||n!=99;}\n",
             NULL},
            {"zero-cpu-comma",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){unsigned n=17;quant uint8_t x=5;quant uint8_t y=(memset((char*)&x+1,++n,0),x+1);unsigned z=measure(y),a=measure(x);return z!=6||a!=5||n!=18;}\n",
             NULL},
            {"call-chain",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5,y=0;quant bool r=(memset(&x,b,1),memcpy(&y,&x,1),y==1);unsigned z=measure(r),a=measure(x),d=measure(y),v=measure(b);return z!=v||a!=v||d!=v;}\n",
             NULL},
            {"coherent-cpu-effect",
             "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){unsigned n=17;quant bool b=superposition(0,1);quant uint8_t x=5;quant uint8_t y=b?(memset(&x,++n,1),x):x;measure(y);measure(x);measure(b);return n;}\n",
             "CPU effects in quantum expressions"},
};

static char *compile_case(
    validation *v, const sequence_case *test, unsigned level, unsigned builtin, int execute) {
    char source[4096], output[4096], compiler[4096];
    validation_path(source, sizeof source, "%s/sequence.c", v->temp);
    validation_path(output, sizeof output, "%s/sequence.out", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_write(source, test->program);
    char *command[] = {compiler,
                       source,
                       execute ? "--run" : "--emit-ir",
                       (char *)levels[level],
                       builtin ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       output,
                       NULL};
    int status = validation_run(v, command, NULL, 240);
    char *log = validation_read(v->log);
    if (test->diagnostic ? !validation_rejected(status, log, test->diagnostic) : status != 0)
        validation_fail("memory sequence %s %s builtins=%u failed:\n%s\n%s",
                        test->name,
                        levels[level],
                        builtin,
                        test->program,
                        log);
    free(log);
    return !execute && !test->diagnostic ? validation_read(output) : NULL;
}

static void syntax(validation *v, const sequence_case *test) {
    const char *preamble =
        "#include <stdint.h>\n#include <stdbool.h>\n#define quant\n"
        "#define measure(x) (x)\n#define phase(x) ((void)(x))\n#define superposition(a,b) (a)\n";
    const char *body = strstr(test->program, "#include <quantum.h>\n");
    if (!body)
        validation_fail("missing memory sequence fixture preamble");
    body += strlen("#include <quantum.h>\n");
    size_t size = strlen(preamble) + strlen(body) + 1;
    char *program = malloc(size);
    if (!program)
        validation_fail("memory sequence fixture allocation failed");
    validation_path(program, size, "%s%s", preamble, body);
    char source[4096];
    validation_path(source, sizeof source, "%s/classical.c", v->temp);
    validation_write(source, program);
    free(program);
    const char *cc = getenv("CC");
    char *command[] = {(char *)(cc ? cc : "gcc"), "-std=c11", "-fsyntax-only", source, NULL};
    if (validation_run(v, command, NULL, 60)) {
        char *log = validation_read(v->log);
        validation_fail("memory sequence rejection fixture is not valid ordinary C syntax:\n%s",
                        log);
    }
}

static void native(validation *v) {
    unsigned positive = 0, negative = 0;
    for (unsigned i = 0; i < sizeof native_cases / sizeof *native_cases; ++i) {
        const sequence_case *test = &native_cases[i];
        if (test->diagnostic)
            syntax(v, test);
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                free(compile_case(v, test, level, builtin, !test->diagnostic));
                if (test->diagnostic)
                    ++negative;
                else
                    ++positive;
            }
    }
    checks += positive + negative;
    printf(
        "PASS %u native memory compositions: comparisons, copies, ctype/division/abs values, comma chains, CPU snapshots and coherent short circuit; five GCC levels, both builtin modes\n",
        positive);
    printf(
        "PASS %u memory sequence rejection contracts: unsequenced value-argument access and coherent CPU effects; ordinary C syntax checked, not a definedness claim\n",
        negative);
    fflush(stdout);
}

static void states(validation *v) {
    static const char *programs[] = {
        "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);quant uint8_t y=(memset(&x,b,1),x+1);if(y==2)phase(.17);measure(y);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5,y=7;if(b)phase(.31);memset(&x,(memset(&y,b,1),y+1),1);if(x==2)phase(.17);measure(x);measure(y);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);quant uint8_t y=b?(memset(&x,17,1),x):x;if(y==17)phase(.17);measure(y);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);quant bool y=b&&(memset(&x,17,1),x==17);if(y)phase(.17);measure(y);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\n#include <ctype.h>\n#include <stdlib.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);quant bool y=b||(memset(&x,17,1),x==17);if(y)phase(.17);measure(y);measure(x);measure(b);return 0;}\n"};
    unsigned count = 0, maximum = 0;
    for (unsigned profile = 0; profile < sizeof programs / sizeof *programs; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                sequence_case test = {"complete-action", programs[profile], NULL};
                char *ir = compile_case(v, &test, level, builtin, 0);
                count += sequence_state_probe(ir, profile, &maximum);
                free(ir);
            }
    checks += count;
    printf(
        "PASS %u complete memory sequence basis-action circuits: both input labels, relative phases, saved result/write inverses and private zeros; verified one-H preparation, both synthesis policies, maximum %u physical wires\n",
        count,
        maximum);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_memory_sequences");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    native(&v);
    validation_cleanup(&v);
    printf(
        "%u memory sequence checks passed; complete labeled action, native integration and expected rejections have distinct proof scopes.\n",
        checks);
    return 0;
}
