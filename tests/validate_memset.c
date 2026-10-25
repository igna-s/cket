#define _POSIX_C_SOURCE 200809L
#include "validation.h"
#include "memset_state.inc"
#include "memset_effects.inc"

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};

typedef struct {
    const char *name, *source, *diagnostic;
    int execute;
} memset_case;

static const memset_case native_cases[] =
    {
#include "memset_lifetimes.inc"
        {"classical",
         "#include <string.h>\nint main(void){unsigned char a[7];void *p=memset(a,-1,sizeof a);unsigned e=p!=a;for(unsigned i=0;i<7;++i)e+=a[i]!=255;return e!=0;}\n",
         NULL,
         1},
        {"word",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint16_t x=0x1234;memset((void*)&x,-1,2);return measure(x)!=65535;}\n",
         NULL,
         1},
        {"partial-word",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint16_t x=0x1234;memset((char*)&x+1,0xab,1);return measure(x)!=0xab34;}\n",
         NULL,
         1},
        {"unknown-byte",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=superposition(0,255);if(x==3)phase(.31);memset((void*)&x,17,1);return measure(x)!=17;}\n",
         NULL,
         1},
        {"quantum-fill",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);if(b)phase(.31);quant uint16_t x=0x1234;memset((void*)&x,b,2);unsigned y=measure(x),v=measure(b);return y!=(v?257u:0u);}\n",
         NULL,
         1},
        {"self-fill",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=superposition(0,255);if(x==3)phase(.31);memset((void*)&x,x,1);if(x==7)phase(.17);return measure(x)>255;}\n",
         NULL,
         1},
        {"cpu-once",
         "#include <quantum.h>\n#include <string.h>\nint main(void){unsigned n=17;quant uint8_t x=0;memset((void*)&x,++n,1);unsigned y=measure(x);return n!=18||y!=18;}\n",
         NULL,
         1},
        {"zero-cpu-effect",
         "#include <quantum.h>\n#include <string.h>\nint main(void){unsigned n=17;quant uint8_t x=65;memset((char*)&x+1,++n,0);unsigned y=measure(x);return n!=18||y!=65;}\n",
         NULL,
         1},
        {"zero-quantum-value",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=superposition(0,255);memset((char*)&x+1,x,0);if(x==3)phase(.31);return measure(x)>255;}\n",
         NULL,
         1},
        {"coherent-fill",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool c=superposition(0,1);quant uint8_t x=5;if(c)memset((void*)&x,17,1);unsigned y=measure(x),v=measure(c);return y!=(v?17u:5u);}\n",
         NULL,
         1},
        {"readonly",
         "#include <quantum.h>\n#include <string.h>\nstatic void clear(quant const uint8_t *p){memset((void*)p,0,1);}\nint main(void){quant uint8_t x=1;clear(&x);return measure(x);}\n",
         "mutable destination storage",
         0},
        {"dynamic-count",
         "#include <quantum.h>\n#include <string.h>\nint main(void){unsigned n=1;quant uint8_t x=1;memset((void*)&x,0,n);return measure(x);}\n",
         NULL,
         1},
        {"bounds",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=1;memset((void*)&x,0,2);return measure(x);}\n",
         "checked storage bounds",
         0},
        {"cpu-destination",
         "#include <quantum.h>\n#include <string.h>\nint main(void){unsigned char a=0;quant bool x=superposition(0,1);memset(&a,x,1);measure(x);return a;}\n",
         "explicit measurement",
         0},
        {"zero-undefined",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t a=0;quant bool x=superposition(0,1);memset((char*)&a+1,1/(int)x,0);measure(a);return measure(x);}\n",
         "C division/remainder by zero is undefined",
         1},
        {"cpu-array-once",
         "#include <quantum.h>\n#include <string.h>\nint main(void){unsigned n=17;quant uint8_t a[2]={0,0};memset((void*)a,++n,2);unsigned x=measure(a[0]),y=measure(a[1]);return n!=18||x!=18||y!=18;}\n",
         NULL,
         1},
        {"array-offset",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t a[3]={17,34,51};memset((char*)a+1,0xab,2);unsigned x=measure(a[0]),y=measure(a[1]),z=measure(a[2]);return x!=17||y!=171||z!=171;}\n",
         NULL,
         1},
        {"array-alias-native",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t a[2]={superposition(0,3),34};memset((void*)a,a[0],2);unsigned x=measure(a[0]),y=measure(a[1]);return x!=y||x>3;}\n",
         NULL,
         1},
        {"return-pointer",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=0;void *p=memset((void*)&x,1,1);unsigned value=measure(x);return value!=1||p!=&x;}\n",
         NULL,
         1},
        {"coherent-cpu-effect",
         "#include <quantum.h>\n#include <string.h>\nint main(void){unsigned n=17;quant bool c=superposition(0,1);quant uint8_t x=0;if(c)memset((void*)&x,++n,1);measure(x);measure(c);return n;}\n",
         "CPU effects in quantum expressions",
         0},
        {"extra-negative-truncation",
         "#include <quantum.h>\n#include <string.h>\nint main(void){int fill=-257;quant uint16_t x=0x1234;memset((char*)&x,fill,2);return measure(x)!=65535;}\n",
         NULL,
         1},
        {"extra-high-truncation",
         "#include <quantum.h>\n#include <string.h>\nint main(void){int fill=0x1ab;quant uint16_t x=0x1234;memset((char*)&x,fill,2);return measure(x)!=0xabab;}\n",
         NULL,
         1},
        {"extra-quantum-negative-byte",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=superposition(0,255);quant uint8_t y=0;memset((void*)&y,(int)x-300,1);unsigned a=measure(x),b=measure(y);return b!=(uint8_t)((int)a-300);}\n",
         NULL,
         1},
        {"extra-signed-quantum-byte",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant int8_t x=0;fourier(x);quant uint8_t y=0;memset((void*)&y,(int)x,1);int a=measure(x);unsigned b=measure(y);return b!=(uint8_t)a;}\n",
         NULL,
         1},
        {"extra-parenthesized",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=5;(memset)((void*)&x,17,1);return measure(x)!=17;}\n",
         NULL,
         1},
        {"extra-builtin",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t x=5;__builtin_memset((void*)&x,17,1);return measure(x)!=17;}\n",
         NULL,
         1},
        {"extra-static-loop", "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t a[2]={17,34};quant bool b=superposition(0,1);for(int i=0;i<2;++i)memset((void*)&a[i],b,1);unsigned x=measure(a[0]),y=measure(a[1]),z=measure(b);return x!=z||y!=z;}\n", NULL, 1},
        {"extra-cpu-arm-true",
         "#include <quantum.h>\n#include <string.h>\nint main(void){volatile int flag=1;quant uint8_t x=5;if(flag)memset((void*)&x,17,1);else memset((void*)&x,23,1);return measure(x)!=17;}\n",
         NULL,
         1},
        {"extra-cpu-arm-false",
         "#include <quantum.h>\n#include <string.h>\nint main(void){volatile int flag=0;quant uint8_t x=5;if(flag)memset((void*)&x,17,1);else memset((void*)&x,23,1);return measure(x)!=23;}\n",
         NULL,
         1},
        {"extra-borrow",
         "#include <quantum.h>\n#include <string.h>\nstatic void fill(quant uint8_t *x,int n){memset((void*)x,n,1);}\nint main(void){quant uint8_t x=5;fill(&x,17);return measure(x)!=17;}\n",
         NULL,
         1},
        {"extra-borrow-array",
         "#include <quantum.h>\n#include <string.h>\nstatic void fill(quant uint16_t *x,quant const bool *b){memset((void*)x,*b,2);}\nint main(void){quant uint16_t a[1]={0x1234};quant bool b=superposition(0,1);fill(a,&b);unsigned x=measure(a[0]),v=measure(b);return x!=(v?257u:0u);}\n",
         NULL,
         1},
        {"extra-zero-function-value",
         "#include <quantum.h>\n#include <string.h>\nstatic unsigned calls;static int next(void){++calls;return 17;}\nint main(void){quant uint8_t x=5;memset((char*)&x+1,next(),0);return measure(x)!=5||calls!=1;}\n",
         NULL,
         1},
        {"extra-function-array-once",
         "#include <quantum.h>\n#include <string.h>\nstatic unsigned calls;static int next(void){++calls;return 17;}\nint main(void){quant uint8_t a[2]={1,2};memset((void*)a,next(),2);unsigned x=measure(a[0]),y=measure(a[1]);return x!=17||y!=17||calls!=1;}\n",
         NULL,
         1},
        {"extra-postfix",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t old=b;quant uint8_t x=5;memset(&x,b++,1);unsigned a=measure(old),y=measure(x),z=measure(b);return y!=a||z!=1;}\n",
         NULL,
         1},
        {"extra-prefix",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,++b,1);unsigned y=measure(x),z=measure(b);return y!=1||z!=1;}\n",
         NULL,
         1},
        {"extra-postdecrement",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t old=b;quant uint8_t x=5;memset(&x,b--,1);unsigned a=measure(old),y=measure(x),z=measure(b);return y!=a||z!=!a;}\n",
         NULL,
         1},
        {"extra-comma",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,(b=false,b),1);unsigned y=measure(x),z=measure(b);return y!=0||z!=0;}\n",
         NULL,
         1},
        {"extra-zero-length-postfix",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset((char*)&x+1,b++,0);unsigned y=measure(x),z=measure(b);return y!=5||z!=1;}\n",
         NULL,
         1},
        {"extra-array-once",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x[2]={17,34};memset(x,b--,2);unsigned y=measure(x[0]),z=measure(x[1]),v=measure(b);return y!=z||y>1||v!=!y;}\n",
         NULL,
         1},
        {"extra-coherent-postfix",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool c=superposition(0,1),b=superposition(0,1);quant uint8_t x=5;if(c)memset(&x,b--,1);unsigned y=measure(x),z=measure(b),v=measure(c);return y!=(v?!z:5);}\n",
         NULL,
         1},
        {"extra-lazy-postfix", "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool c=superposition(0,1),b=superposition(0,1);quant uint8_t x=5;memset(&x,c?b++:b--,1);unsigned y=measure(x),z=measure(b),v=measure(c);return y>1||z!=(v?1:!y);}\n", NULL, 1},
        {"extra-unsequenced",
         "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset(&x,b++ + b,1);measure(x);return measure(b);}\n",
         "unsequenced quantum modification and access",
         0},
};

static const struct {
    const char *name, *body;
} state_cases[] = {
    {"constant-history",
     "quant uint8_t x=superposition(0,255);if(x&1)phase(0.001);if(x&2)phase(0.002);if(x&4)phase(0.0040000000000000001);if(x&8)phase(0.0080000000000000002);if(x&16)phase(0.016);if(x&32)phase(0.032000000000000001);if(x&64)phase(0.064000000000000001);if(x&128)phase(0.128);memset((void*)&x,17,1);if(x==17)phase(.17);measure(x);"},
    {"self-history",
     "quant uint8_t x=superposition(0,255);if(x&1)phase(0.001);if(x&2)phase(0.002);if(x&4)phase(0.0040000000000000001);if(x&8)phase(0.0080000000000000002);if(x&16)phase(0.016);if(x&32)phase(0.032000000000000001);if(x&64)phase(0.064000000000000001);if(x&128)phase(0.128);memset((void*)&x,x,1);if(x==7)phase(.17);measure(x);"},
    {"controlled-history",
     "quant uint8_t x=superposition(0,255);quant bool b=superposition(0,1);if(x&1)phase(0.001);if(x&2)phase(0.002);if(x&4)phase(0.0040000000000000001);if(x&8)phase(0.0080000000000000002);if(x&16)phase(0.016);if(x&32)phase(0.032000000000000001);if(x&64)phase(0.064000000000000001);if(x&128)phase(0.128);if(b)phase(.19);if(b)memset((void*)&x,17,1);if(x==7)phase(.17);measure(x);measure(b);"},
    {"quantum-history",
     "quant uint8_t x=superposition(0,255);quant bool b=superposition(0,1);if(x&1)phase(0.001);if(x&2)phase(0.002);if(x&4)phase(0.0040000000000000001);if(x&8)phase(0.0080000000000000002);if(x&16)phase(0.016);if(x&32)phase(0.032000000000000001);if(x&64)phase(0.064000000000000001);if(x&128)phase(0.128);if(b)phase(.19);memset((void*)&x,b,1);if(x)phase(.17);measure(x);measure(b);"},
    {"partial-word",
     "quant uint16_t x=0x1234;quant bool b=superposition(0,1);if(b)phase(.19);memset((char*)&x+1,b,1);if(x==0x134)phase(.17);measure(x);measure(b);"},
    {"whole-word",
     "quant uint16_t x=0x1234;quant bool b=superposition(0,1);if(b)phase(.19);memset((void*)&x,b,2);if(x==257)phase(.17);measure(x);measure(b);"},
    {"quantum-array",
     "quant uint8_t x[2]={17,34};quant bool b=superposition(0,1);if(b)phase(.19);memset((void*)x,b,2);if(x[0])phase(.17);measure(x[0]);measure(x[1]);measure(b);"},
    {"aliased-array", "quant uint8_t a[2]={superposition(0,255),34};if(a[0]&1)phase(0.001);if(a[0]&2)phase(0.002);if(a[0]&4)phase(0.0040000000000000001);if(a[0]&8)phase(0.0080000000000000002);if(a[0]&16)phase(0.016);if(a[0]&32)phase(0.032000000000000001);if(a[0]&64)phase(0.064000000000000001);if(a[0]&128)phase(0.128);memset((void*)a,a[0],2);measure(a[0]);measure(a[1]);"},
};

static char *
compile_case(validation *v, const memset_case *test, unsigned level, unsigned builtins) {
    char input[4096], output[4096], compiler[4096];
    validation_path(input, sizeof input, "%s/memset.c", v->temp);
    validation_path(output, sizeof output, "%s/memset.out", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_write(input, test->source);
    char *command[] = {compiler,
                       input,
                       test->execute ? "--run" : "--emit-ir",
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       output,
                       NULL};
    int status = validation_run(v, command, NULL, 240);
    char *log = validation_read(v->log);
    if (test->diagnostic ? !validation_rejected(status, log, test->diagnostic) : status != 0)
        validation_fail("memset %s %s builtins=%u failed:\n%s\n%s",
                        test->name,
                        levels[level],
                        builtins,
                        test->source,
                        log);
    free(log);
    return !test->execute && !test->diagnostic ? validation_read(output) : NULL;
}

/* Rejected fixtures must first have valid ordinary C syntax. Reachable
 * division by zero and unsequenced access deliberately have C undefined
 * behavior; parsing them classically makes no defined-execution claim. */
static void classical_syntax(validation *v, const memset_case *test) {
    const char *mock = "#include <stdint.h>\n#include <stdbool.h>\n"
                       "#include <string.h>\n#define quant\n"
                       "#define measure(x) (x)\n#define phase(x) ((void)(x))\n"
                       "#define superposition(a,b) (a)\n";
    const char *body = strstr(test->source, "#include <quantum.h>\n");
    if (!body)
        validation_fail("missing quantum preamble in negative fixture %s", test->name);
    body += strlen("#include <quantum.h>\n");
    size_t size = strlen(mock) + strlen(body) + 1;
    char *program = malloc(size);
    if (!program)
        validation_fail("classical syntax allocation failed");
    validation_path(program, size, "%s%s", mock, body);
    char input[4096];
    validation_path(input, sizeof input, "%s/memset-classical.c", v->temp);
    validation_write(input, program);
    free(program);
    const char *environment = getenv("CC");
    char *command[] = {(char *)(environment ? environment : "gcc"),
                       "-std=c11",
                       "-fsyntax-only",
                       "-Wno-attributes",
                       input,
                       NULL};
    if (validation_run(v, command, NULL, 60) != 0) {
        char *log = validation_read(v->log);
        validation_fail("negative fixture %s is not valid ordinary C syntax:\n%s", test->name, log);
    }
}

static void native_and_rejections(validation *v) {
    unsigned positive = 0, negative = 0;
    for (unsigned i = 0; i < sizeof native_cases / sizeof *native_cases; ++i) {
        const memset_case *test = &native_cases[i];
        if (test->diagnostic)
            classical_syntax(v, test);
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                free(compile_case(v, test, level, builtin));
                if (test->diagnostic)
                    ++negative;
                else
                    ++positive;
            }
    }
    checks += positive + negative;
    printf(
        "PASS %u native memset cases: classical return pointer, byte/word/array writes, partial bytes, coherent conditions, aliases and once-only CPU arguments; five GCC levels and both builtin modes\n",
        positive);
    printf(
        "PASS %u memset rejection contracts: read-only storage, bounds, unmeasured CPU writes, reachable zero-length argument UB and coherent CPU effects\n",
        negative);
    fflush(stdout);
}

static void states(validation *v) {
    unsigned dense = 0, labeled = 0;
    for (unsigned profile = 0; profile < sizeof state_cases / sizeof *state_cases; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                char program[8192];
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <string.h>\n"
                                "int main(void){%sreturn 0;}\n",
                                state_cases[profile].body);
                memset_case test = {state_cases[profile].name, program, NULL, 0};
                char *ir = compile_case(v, &test, level, builtin);
                unsigned count = memset_state_probe(ir, profile);
                free(ir);
                if (profile == 2)
                    labeled += count;
                else
                    dense += count;
            }
    checks += dense + labeled;
    printf(
        "PASS %u dense complete-state memset cases with phases, retained inputs, saved inverses and clean private wires\n",
        dense);
    printf(
        "PASS %u controlled memset circuits: all 512 labeled input basis paths, amplitudes, phases and saved inverses; canonical nine-H preparation, both synthesis policies\n",
        labeled);
    fflush(stdout);
}

static void value_effects(validation *v) {
    static const char *programs[] = {
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);memset(&x,b++,1);if(x==1)phase(.17);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);memset(&x,++b,1);if(x==1)phase(.17);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);memset(&x,b--,1);if(x==1)phase(.17);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);memset(&x,(b=false,b),1);if(x==1)phase(.17);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);memset((char*)&x+1,b++,0);if(x==1)phase(.17);measure(x);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x[2]={17,34};if(b)phase(.31);memset(x,b--,2);if(x[0]==1)phase(.17);measure(x[0]);measure(x[1]);measure(b);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant bool c=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);if(c)phase(.19);if(c)memset(&x,b--,1);if(x==1)phase(.17);measure(x);measure(b);measure(c);return 0;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){quant bool b=superposition(0,1);quant bool c=superposition(0,1);quant uint8_t x=5;if(b)phase(.31);if(c)phase(.19);memset(&x,c?b++:b--,1);if(x==1)phase(.17);measure(x);measure(b);measure(c);return 0;}\n"};
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof programs / sizeof *programs; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                memset_case test = {"sequenced-value-state", programs[profile], NULL, 0};
                char *ir = compile_case(v, &test, level, builtin);
                count += memset_effect_probe(ir, profile);
                free(ir);
            }
    checks += count;
    printf(
        "PASS %u complete memset value-effect states: postfix/prefix, decrement, comma, zero length, arrays, coherent/lazy effects, relative phases and saved inverses; live history before inversion, private zeros after\n",
        count);
    fflush(stdout);
}

static void wide(validation *v) {
    unsigned count = 0;
    for (unsigned width = 16; width <= 64; width *= 2)
        for (unsigned signed_input = 0; signed_input < 2; ++signed_input)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtin = 0; builtin < 2; ++builtin) {
                    char program[4096];
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\nint main(void){"
                        "quant %s%u_t x=%s;quant bool b=superposition(0,1);"
                        "memset((void*)&x,b,sizeof x);measure(x);measure(b);return 0;}\n",
                        signed_input ? "int" : "uint",
                        width,
                        signed_input ? "-7" : "0x1234");
                    memset_case test = {"wide-layout", program, NULL, 0};
                    char *ir = compile_case(v, &test, level, builtin);
                    qc_context *c = qc_create(0, 7);
                    if (!c)
                        validation_fail("wide memset context allocation failed");
                    qc_allow_tables(c, 0);
                    if (!qc_document(c, ir, 0, 0) || c->reset || qc_qubits(c) > 128)
                        validation_fail("wide memset circuit generation failed: %s", qc_error(c));
                    free(ir);
                    qc_destroy(c);
                    ++count;
                }
    checks += count;
    printf(
        "PASS %u wide memset no-table circuits: signed/unsigned 16/32/64-bit layouts, five GCC levels and both builtin modes; generation evidence\n",
        count);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_memset");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    value_effects(&v);
    native_and_rejections(&v);
    wide(&v);
    validation_cleanup(&v);
    printf(
        "%u memset checks passed; dense states, labeled paths, native executions, rejections and wide generation have distinct proof scopes.\n",
        checks);
    return 0;
}
