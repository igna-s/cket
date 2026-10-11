#define _GNU_SOURCE
#include "quantum.h"
#include "json.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char root[PATH_MAX],temp[PATH_MAX],source_path[PATH_MAX+32],output_path[PATH_MAX+32],error_path[PATH_MAX+32],artifact[PATH_MAX+32];
static unsigned passed,failed;
static void report(const char *name,int ok) { printf("%s %s\n",ok?"PASS":"FAIL",name); fflush(stdout); if (ok) ++passed; else ++failed; }
static int close_to(double a,double b) { return fabs(a-b)<1e-10; }
static double probability(qc_context *c,size_t i) { double complex a=qc_amplitude(c,i); return creal(a)*creal(a)+cimag(a)*cimag(a); }
static int compile(const char *body,const char *action,int success) {
    FILE *f=fopen(source_path,"w"); if (!f) return 0;
    if (*body=='#') fputs(body,f);
    else fprintf(f,"#include <quantum.h>\n#include <stdio.h>\nstatic int twice(int n){return n*2;}\nint main(void){\n%s\nreturn 0;\n}\n",body);
    fclose(f);
    char driver[PATH_MAX+32]; snprintf(driver,sizeof driver,"%s/build/cket",root);
    pid_t pid=fork(); if (pid<0) return 0;
    if (!pid) {
        if (!freopen(output_path,"w",stdout) || !freopen(error_path,"w",stderr)) _exit(126);
        if (!strcmp(action,"--run")) execl(driver,driver,source_path,action,"--seed","7","-o",artifact,(char *)NULL);
        else execl(driver,driver,source_path,action,"-o",artifact,(char *)NULL);
        _exit(127);
    }
    int status; pid_t wait;
    do wait=waitpid(pid,&status,0); while (wait<0 && errno==EINTR);
    int code=wait<0?255:WIFEXITED(status)?WEXITSTATUS(status):255;
    char *errors=qc_read_file(error_path);
    int ok=success?code==0:code!=0 && errors && strstr(errors,"|c>");
    if (!ok) fprintf(stderr,"Compilation/run mismatch (%d):\n%s\n%s\n",code,body,errors?errors:"");
    free(errors); return ok;
}
static qc_context *state(const char *body) {
    if (!compile(body,"--emit-ir",1)) return NULL;
    char *text=qc_read_file(artifact); qc_context *c=qc_create(1,7);
    if (!text || !c || !qc_document(c,text,0,1)) { fprintf(stderr,"State lowering: %s\n",qc_error(c)); free(text); qc_destroy(c); return NULL; }
    free(text); return c;
}
static void native_cases(void) {
    const struct { const char *name,*body; } cases[]={
        {"basis bit order", "quant uint8_t x=165; return measure(x)!=165;"},
        {"measurement in return", "quant uint8_t x=0; return measure(x);"},
        {"measurement continuation", "quant uint8_t x=superposition(0,255); uint8_t a=measure(x),b=measure(x); return a!=b;"},
        {"ownership move", "quant uint8_t a=7; quant uint8_t b=a; return measure(b)!=7;"},
        {"boolean reversible update", "quant bool x=0; x=!x; return measure(x)!=1;"},
        {"unsigned increment/decrement", "quant uint8_t x=255; x++; --x; return measure(x)!=255;"},
        {"signed division and measurement type", "quant int8_t x=-7; quant int8_t y=x/2; int r=measure(y); measure(x); return r!=-3;"},
        {"signed narrowing matches GCC", "quant int8_t x=127; quant int8_t y=x+1; int r=measure(y); measure(x); return r!=-128;"},
        {"classical for", "quant uint8_t x=1; for(int i=0;i<8;i++) x+=i; return measure(x)!=29;"},
        {"classical while/do", "quant uint8_t x=0; int i=3; while(i--) x+=2; do {x-=1;} while(++i<1); return measure(x)!=4;"},
        {"explicit measurement as operand", "quant uint8_t x=1; quant uint8_t y=2; x+=measure(y); return measure(x)!=3;"},
        {"classical operand evaluated once", "quant uint8_t x=1; int n=1; x+=++n; return measure(x)!=3 || n!=2;"},
        {"classical aggregate measurement", "quant uint8_t x=7; struct S{int n;}; struct S s={measure(x)}; return s.n!=7;"},
        {"allocation after measurement", "quant uint8_t x=1; measure(x); quant uint8_t y=2; return measure(y)!=2;"},
        {"64-bit unsigned expression", "quant uint8_t x=1; quant bool b=((uint64_t)x*UINT64_MAX==UINT64_MAX); int r=measure(b); measure(x); return r!=1;"},
        {"cuant aliases", "cuant uint8_t x=7; Cuant bool b=x==7; int r=measure(b); measure(x); return r!=1;"},
        {"move through compatible C typedef", "typedef uint8_t Byte; quant Byte a=7; quant uint8_t b=a; return measure(b)!=7;"},
        {"CPU control with quantum updates", "int c=1; quant uint8_t x=4; if(c) x+=3; else x+=5; return measure(x)!=7;"},
        {"ordinary C structs pointers functions", "struct S{int n;}; struct S s={0}; int *p=&s.n; for(int i=0;i<5;i++) *p+=twice(i); return *p!=20;"},
        {"reset and storage reuse", "quant uint8_t x=255; reset(x); x+=5; return measure(x)!=5;"},
    };
    for (size_t i=0;i<sizeof cases/sizeof *cases;++i) report(cases[i].name,compile(cases[i].body,"--run",1));
    /* GCC evaluates the ordinary C expressions independently of the backend. */
    const char *expressions[]={"x+5","x-2","x*3","x/2","x%3","x&3","x|2","x^5","~x","x<<1","x>>1","x==7","x!=7","x<9","x<=7","x>8","x>=7","!x","x&&2","x||0"};
    char body[8192]="quant uint8_t x=7; int bad=0;\n";
    for (unsigned i=0;i<sizeof expressions/sizeof *expressions;++i) {
        char classical[64]; snprintf(classical,sizeof classical,"%s",expressions[i]); for (char *p=classical;*p;++p) if (*p=='x') *p='7';
        size_t used=strlen(body);
        snprintf(body+used,sizeof body-used,"quant uint8_t y%u=%s; bad|=measure(y%u)!=(uint8_t)(%s); reset(y%u);\n",i,expressions[i],i,classical,i);
    }
    strcat(body,"measure(x); return bad;"); report("20 operators, C promotions and clean reuse",compile(body,"--run",1));
}
static void invalid_cases(void) {
    const struct {const char *name,*body;} cases[]={
        {"use after move","quant uint8_t a=1; quant uint8_t b=a; measure(a); measure(b);"},
        {"move between distinct C types","quant char a=1; quant signed char b=a; measure(b);"},
        {"find changes input signedness","quant uint8_t x=superposition(0,15); quant int8_t a=find(x,x==3); measure(a);"},
        {"noninjective conditional update","quant uint8_t x=superposition(0,15); if(x==0) x+=1; measure(x);"},
        {"implicit CPU read","quant uint8_t x=1; int y=x; measure(x);"},
        {"implicit I/O read","quant uint8_t x=1; printf(\"%u\",x); measure(x);"},
        {"destructive assignment","quant uint8_t x=1; x=0; measure(x);"},
        {"folded zero read","quant uint8_t x=1; int y=x*0; measure(x);"},
        {"folded difference read","quant uint8_t x=1; int y=x-x; measure(x);"},
        {"pointer alias","quant uint8_t x=1; void *p=(void *)&x; measure(x);"},
        {"classical measurement","int x=1; measure(x);"},
        {"classical preparation","uint8_t x=superposition(0,255);"},
        {"out of range preparation","quant uint8_t x=superposition(0,256); measure(x);"},
        {"unsupported preparation range","quant uint8_t x=superposition(0,10); measure(x);"},
        {"runtime preparation bounds","int n=15; quant uint8_t x=superposition(0,n); measure(x);"},
        {"coherent I/O","quant bool c=1; if(c) printf(\"oops\"); measure(c);"},
        {"coherent arithmetic I/O","quant bool c=1; quant uint8_t x=0; if(c) x+=printf(\"oops\"); measure(x); measure(c);"},
        {"coherent CPU update","int i=0; quant bool c=1; quant uint8_t x=0; if(c) x+=i++; measure(x); measure(c);"},
        {"coherent CPU postincrement","quant bool c=1;int n=0;if(c)n++;measure(c);"},
        {"coherent CPU predecrement","quant bool c=1;int n=1;if(c)--n;measure(c);"},
        {"coherent CPU argument effect","#include <quantum.h>\nvoid add(quant uint8_t x,int n){x+=n;}\nint main(void){quant bool c=1;quant uint8_t x=0;int n=0;if(c)add(x,n++);measure(x);measure(c);}\n"},
        {"coherent inline assembly","quant bool c=1;if(c)__asm__ volatile(\"nop\");measure(c);"},
        {"quantum while","quant bool c=1; while(c){} measure(c);"},
        {"unmeasured storage","quant uint8_t x=superposition(0,255);"},
        {"early return","quant uint8_t x=1; return 0; measure(x);"},
        {"goto skips allocation","goto skip; quant uint8_t x=1; skip:measure(x);"},
        {"quantum pointer","quant uint8_t *x;"},
        {"quantum float","quant double x;"},
        {"noninjective remainder update","quant uint8_t x=superposition(0,15); x=x%2; measure(x);"},
        {"boolean addition is noninjective","quant bool x=superposition(0,1); x+=1; measure(x);"},
        {"aggregate hides read","quant uint8_t x=1; struct S{int n;}; struct S s={x}; measure(x);"},
        {"mutation of live oracle input","quant uint8_t x=superposition(0,15); quant bool f=x==3; x+=1; if(f) phase_flip(); measure(x);"},
        {"phase-tainted find input","quant uint8_t x=superposition(0,15); if(x==3) phase_flip(); quant uint8_t a=find(x,x==3); measure(a);"},
        {"division by zero on domain","quant uint8_t x=superposition(0,3); quant uint8_t y=8/x; measure(y); measure(x);"},
        {"invalid shift on domain","quant uint8_t x=superposition(0,63); quant uint8_t y=1<<x; measure(y); measure(x);"},
        {"zero known solution count","quant uint8_t x=superposition(0,15); quant uint8_t a=find_known(x,x==42,0); measure(a);"},
        {"global quantum storage","#include <quantum.h>\nquant uint8_t x;\nint main(void){return 0;}\n"},
        {"quantum floating pointer parameter","#include <quantum.h>\nvoid f(quant float *x){}\nint main(void){return 0;}\n"},
    };
    for (size_t i=0;i<sizeof cases/sizeof *cases;++i) { char name[128]; snprintf(name,sizeof name,"reject %s",cases[i].name); report(name,compile(cases[i].body,"--emit-ir",0)); }
}
static void amplitude_cases(void) {
    qc_context *c=state("quant uint8_t x=superposition(0,15); measure(x);"); int ok=c && qc_qubits(c)==8;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) ok&=close_to(probability(c,i),i<16?1.0/16:0);
    report("uniform exact support",ok); qc_destroy(c);
    c=state("quant uint8_t x=superposition(0,15); quant bool f=x*x==9; if(f) phase_flip(); measure(x);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) {
        double expected=i<16?(i==3?-.25:.25):0; double complex a=qc_amplitude(c,i);
        ok&=close_to(creal(a),expected) && close_to(cimag(a),0);
    }
    report("coherent phase and all ancillas zero",ok); qc_destroy(c);
    c=state("quant uint8_t x=superposition(0,15); quant bool f=((unsigned)(x<8)-(unsigned)(x==x))==0u; if(f)phase_flip(); measure(x);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) {
        double expected=i<16?(i<8?-.25:.25):0;
        double complex a=qc_amplitude(c,i);
        ok&=close_to(creal(a),expected) && close_to(cimag(a),0);
    }
    report("GCC logical XOR preserves relative phase and cleanup",ok); qc_destroy(c);
    c=state("quant bool b=superposition(0,1); quant uint8_t x=0; if(b) x+=1; else x+=2; measure(x); measure(b);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) ok&=close_to(probability(c,i),i==3 || i==4?.5:0);
    report("coherent if else joint state",ok); qc_destroy(c);
    const char *bodies[]={
        "quant uint8_t x=superposition(0,15); quant uint8_t y=x%2; measure(y); measure(x);",
        "quant uint8_t x=superposition(0,15); quant uint8_t y; y=x%2; measure(y); measure(x);",
        "quant uint8_t x=superposition(0,15); quant uint8_t y=x*3+5; measure(y); measure(x);",
        "quant uint8_t x=superposition(0,3); quant uint8_t y=x?8/x:7; measure(y); measure(x);"
    };
    for (unsigned test=0;test<4;++test) {
        c=state(bodies[test]); ok=c!=NULL; unsigned total=test==3?4:16;
        if (c) for (size_t i=0;i<qc_state_size(c);++i) {
            unsigned x=(unsigned)i&255,y=(unsigned)i>>8,expected=test==3?(x?8/x:7):test==2?x*3+5:x%2;
            ok&=close_to(probability(c,i),x<total && y==expected?1.0/total:0);
        }
        report(test==0?"modulo preserves entanglement":test==1?"compute into zero storage":test==2?"arithmetic on every superposed input":"short circuit ternary on used domain",ok); qc_destroy(c);
    }
    c=state("quant uint8_t x=superposition(0,15); quant uint8_t a=find(x,x*x==9); measure(a);"); ok=c && close_to(probability(c,3),.9613189697265625);
    if (c) { double garbage=0; for (size_t i=256;i<qc_state_size(c);++i) garbage+=probability(c,i); ok&=close_to(garbage,0); }
    report("Grover amplitude and oracle cleanup",ok); qc_destroy(c);
    c=state("quant uint8_t x=superposition(0,15); quant uint8_t a=find_known(x,x%4==1,4); measure(a);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) ok&=close_to(probability(c,i),i<16 && i%4==1?.25:0);
    report("Grover with multiple solutions",ok); qc_destroy(c);
    c=state("quant bool b=superposition(0,1); quant bool d=superposition(0,1); quant uint8_t x=0; if(b){if(d)x+=1;else x+=2;}else x+=3; measure(x); measure(d); measure(b);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) ok&=close_to(probability(c,i),i==7 || i==9 || i==12 || i==14?.25:0);
    report("nested coherent controls and clean ancillas",ok); qc_destroy(c);
    c=state("quant uint8_t x=superposition(0,255); quant bool f=x*x==9; if(f)phase_flip(); measure(x);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) {
        double expected=i<256?(i==3?-1.0/16:1.0/16):0;
        ok&=close_to(creal(qc_amplitude(c,i)),expected) && close_to(cimag(qc_amplitude(c,i)),0);
    }
    report("C promotions avoid false modular square roots",ok); qc_destroy(c);
    c=state("quant uint8_t x=superposition(0,15); x=x*3+1; measure(x);"); ok=c!=NULL;
    if (c) for (size_t i=0;i<qc_state_size(c);++i) ok&=close_to(probability(c,i),i<=46 && i%3==1?1.0/16:0);
    report("general injective permutation preserves amplitudes",ok); qc_destroy(c);
}
static void backend_cases(void) {
    qc_context *c=qc_create(1,5); uint64_t a=0,b=0,again=0; int ok=c!=NULL;
    ok=ok && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":3}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}",NULL);
    ok=ok && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":3}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":1,\"lower\":5,\"upper\":5}",NULL);
    ok=ok && qc_apply(c,"{\"op\":\"qmeasure\",\"id\":0}",&a) && qc_apply(c,"{\"op\":\"qmeasure\",\"id\":1}",&b) && qc_apply(c,"{\"op\":\"qmeasure\",\"id\":0}",&again);
    report("backend preserves other registers and collapse",ok && b==5 && a==again); qc_destroy(c);
    c=qc_create(1,7); ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1}",NULL) && !qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1}",NULL);
    report("backend rejects reinitialization",ok); qc_destroy(c);
    c=qc_create(1,7); ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":1,\"upper\":1}",NULL) && !qc_apply(c,"{\"op\":\"qrelease\",\"id\":0}",NULL);
    report("backend rejects dirty release",ok); qc_destroy(c);
    c=qc_create(1,7); ok=c && !qc_apply(c,"{broken",NULL); report("malformed IR fails explicitly",ok); qc_destroy(c);
    c=qc_create(1,7); ok=c && !qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":25}",NULL); report("simulation memory limit fails explicitly",ok); qc_destroy(c);
    c=qc_create(0,7); ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":64}",NULL) && qc_qubits(c)==64;
    report("64-bit export storage does not require a statevector",ok); qc_destroy(c);
    c=qc_create(1,7); ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":3}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":7,\"upper\":7}",NULL);
    ok=ok && qc_apply(c,"{\"op\":\"qreset\",\"id\":0}",NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":0}",NULL) && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":3}",NULL);
    ok=ok && qc_qubits(c)==3 && qc_apply(c,"{\"op\":\"qprepare\",\"id\":1,\"lower\":5,\"upper\":5}",NULL) && qc_apply(c,"{\"op\":\"qmeasure\",\"id\":1}",&a) && a==5;
    double norm=0; if (c) for (size_t i=0;i<qc_state_size(c);++i) norm+=probability(c,i);
    report("reset permits zero storage reuse and norm stays one",ok && close_to(norm,1)); qc_destroy(c);
    c=qc_create(1,7); ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}",NULL) && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":1}",NULL);
    ok=ok && qc_apply(c,"{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}",NULL);
    ok=ok && !qc_apply(c,"{\"op\":\"qadd_inplace\",\"id\":0,\"value\":1}",NULL);
    report("backend protects dependencies before invalidating an inverse",ok); qc_destroy(c);
}
/* Exercise the public IR API in both export and simulation contexts.
 * A simulator's final clean-state check must not substitute for a proof. */
static qc_context *proof_context(int simulate) {
    qc_context *c=qc_create(simulate,7);
    const char *setup[]={
        "{\"op\":\"qalloc\",\"id\":0,\"width\":1}",
        "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}",
        "{\"op\":\"qalloc\",\"id\":1,\"width\":1}",
        "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}",
        "{\"op\":\"qalloc\",\"id\":2,\"width\":1}",
        "{\"op\":\"qalloc\",\"id\":3,\"width\":1}",
        "{\"op\":\"qprepare\",\"id\":3,\"lower\":0,\"upper\":1}"
    };
    if (!c) return NULL;
    for (unsigned i=0;i<sizeof setup/sizeof *setup;++i) if (!qc_apply(c,setup[i],NULL)) { qc_destroy(c); return NULL; }
    return c;
}
static void proof_cases(void) {
    const char *forward="{\"op\":\"qcompute\",\"id\":2,\"controls\":[{\"id\":1,\"value\":1}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}";
    const char *inverse="{\"op\":\"quncompute\",\"id\":2,\"controls\":[{\"id\":1,\"value\":1}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}";
    const struct { const char *name,*intervention,*undo; } invalid[]={
        {"inverse with changed control pattern",NULL,"{\"op\":\"quncompute\",\"id\":2,\"controls\":[{\"id\":1,\"value\":0}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}"},
        {"inverse with missing control",NULL,"{\"op\":\"quncompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}"},
        {"inverse with another control",NULL,"{\"op\":\"quncompute\",\"id\":2,\"controls\":[{\"id\":3,\"value\":1}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}"},
        {"mutation of a live computation control","{\"op\":\"qadd_inplace\",\"id\":1,\"value\":1}",NULL},
        {"measurement of a live computation control","{\"op\":\"qmeasure\",\"id\":1}",NULL},
        {"reset of a live computation control","{\"op\":\"qreset\",\"id\":1}",NULL},
        {"mutation of a live computation input","{\"op\":\"qadd_inplace\",\"id\":0,\"value\":1}",NULL}
    };
    for (unsigned test=0;test<sizeof invalid/sizeof *invalid;++test) {
        int ok=1;
        for (int simulate=0;simulate<=1;++simulate) {
            qc_context *c=proof_context(simulate);
            int pass=c && qc_apply(c,forward,NULL);
            if (pass) pass=!qc_apply(c,invalid[test].intervention?invalid[test].intervention:invalid[test].undo,NULL) && *qc_error(c);
            ok&=pass; qc_destroy(c);
        }
        report(invalid[test].name,ok);
    }
    int ok=1;
    for (int simulate=0;simulate<=1;++simulate) {
        qc_context *c=proof_context(simulate);
        int pass=c && qc_apply(c,forward,NULL) && qc_apply(c,inverse,NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":2}",NULL);
        if (pass && simulate) for (size_t i=0;i<qc_state_size(c);++i) {
            double complex a=qc_amplitude(c,i);
            pass&=close_to(creal(a),(i&4)?0:1/sqrt(8)) && close_to(cimag(a),0);
        }
        ok&=pass; qc_destroy(c);
    }
    report("controlled inverse preserves phases, support and clean output",ok);
    ok=1;
    for (int simulate=0;simulate<=1;++simulate) {
        qc_context *c=proof_context(simulate);
        int pass=c && qc_apply(c,"{\"op\":\"qcompute\",\"id\":2,\"controls\":[{\"id\":1,\"value\":1},{\"id\":3,\"value\":0}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}",NULL);
        pass=pass && qc_apply(c,"{\"op\":\"quncompute\",\"id\":2,\"controls\":[{\"id\":3,\"value\":0},{\"id\":1,\"value\":1}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}",NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":2}",NULL);
        ok&=pass; qc_destroy(c);
    }
    report("control order is normalized in inverse certificates",ok);
    ok=1;
    for (int simulate=0;simulate<=1;++simulate) {
        qc_context *c=proof_context(simulate);
        int pass=c && qc_apply(c,forward,NULL) &&
            qc_apply(c,"{\"op\":\"qphase\",\"angle\":0.25,\"controls\":[{\"id\":1,\"value\":1}]}",NULL) &&
            qc_apply(c,inverse,NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":2}",NULL);
        if (pass && simulate) for (size_t i=0;i<qc_state_size(c);++i) {
            double complex expected=(i&4)?0:((i&2)?cos(.25)+I*sin(.25):1)/sqrt(8);
            double complex a=qc_amplitude(c,i);
            pass&=close_to(creal(a),creal(expected)) && close_to(cimag(a),cimag(expected));
        }
        ok&=pass; qc_destroy(c);
    }
    report("safe input phase survives controlled uncomputation",ok);
    const struct {const char *name,*operation;} malformed[]={
        {"mixed arithmetic operand types","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"add\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false},{\"op\":\"const\",\"value\":1,\"bits\":8,\"signed\":false}]}}"},
        {"ambiguous dynamic addition value","{\"op\":\"qadd_inplace\",\"id\":0,\"dynamic\":true,\"value\":1}"},
        {"contradictory coherent controls","{\"op\":\"qcompute\",\"id\":2,\"controls\":[{\"id\":1,\"value\":0},{\"id\":1,\"value\":1}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}"},
        {"missing binary operand","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"add\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}]}}"},
        {"extra unary operand","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false},{\"op\":\"reg\",\"id\":1,\"bits\":1,\"signed\":false}]}}"},
        {"invalid operator hidden in an unused arm","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"select\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"const\",\"value\":0,\"bits\":1,\"signed\":false},{\"op\":\"unknown\",\"bits\":1,\"signed\":false},{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}]}}"},
        {"string used as signedness","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":\"false\"}}"},
        {"missing expression signedness","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1}}"},
        {"duplicate instruction keys","{\"op\":\"qreset\",\"id\":0,\"id\":1}"},
        {"unknown instruction field","{\"op\":\"qreset\",\"id\":0,\"silently_ignore\":true}"},
        {"measurement under coherent control","{\"op\":\"qmeasure\",\"id\":0,\"controls\":[{\"id\":1,\"value\":1}]}"},
        {"reset under coherent control","{\"op\":\"qreset\",\"id\":0,\"controls\":[{\"id\":1,\"value\":1}]}"},
        {"nonnumeric phase angle","{\"op\":\"qphase\",\"angle\":\"arbitrary\"}"},
        {"forged known basis","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false,\"known\":0}}"},
        {"forged inactive input bits","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false,\"active\":0}}"},
        {"inconsistent occurrences of the same register","{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"bxor\",\"bits\":1,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false},{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":true}]}}"}
    };
    for (unsigned test=0;test<sizeof malformed/sizeof *malformed;++test) {
        ok=1;
        for (int simulate=0;simulate<=1;++simulate) {
            qc_context *c=proof_context(simulate); ok&=c && !qc_apply(c,malformed[test].operation,NULL) && *qc_error(c); qc_destroy(c);
        }
        report(malformed[test].name,ok);
    }
    const struct {const char *name,*setup,*operation;} support[]={
        {"known-zero source cannot alias its output",
         "{\"op\":\"qprepare\",\"id\":2,\"lower\":0,\"upper\":0}",
         "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"reg\",\"id\":2,\"bits\":1,\"signed\":false,\"active\":0,\"known\":0}}"},
        {"clean but borrowed control cannot be released",
         "{\"op\":\"qcompute\",\"id\":3,\"controls\":[{\"id\":2,\"value\":0}],\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}",
         "{\"op\":\"qrelease\",\"id\":2}"},
        {"find cannot reuse an entangled input",forward,
         "{\"op\":\"qfind\",\"id\":0,\"upper\":1,\"predicate\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}"},
        {"find cannot ignore a preceding controlled phase",
         "{\"op\":\"qphase\",\"controls\":[{\"id\":0,\"value\":1}]}",
         "{\"op\":\"qfind\",\"id\":0,\"upper\":1,\"predicate\":{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false}}"}
    };
    for (unsigned test=0;test<sizeof support/sizeof *support;++test) {
        ok=1;
        for (int simulate=0;simulate<=1;++simulate) {
            qc_context *c=proof_context(simulate);
            /* One setup needs a clean output in place of the uniform id 3. */
            int pass=c!=NULL;
            if (test==1) pass=pass && qc_apply(c,"{\"op\":\"qreset\",\"id\":3}",NULL);
            pass=pass && qc_apply(c,support[test].setup,NULL) && !qc_apply(c,support[test].operation,NULL) && *qc_error(c);
            ok&=pass; qc_destroy(c);
        }
        report(support[test].name,ok);
    }
    ok=1;
    for (int simulate=0;simulate<=1;++simulate) {
        qc_context *c=qc_create(simulate,7);
        int pass=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}",NULL) &&
            qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":15}",NULL) &&
            qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":8,\"signed\":false}",NULL) &&
            qc_apply(c,"{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false,\"active\":4}}",NULL) &&
            qc_apply(c,"{\"op\":\"quncompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false,\"active\":4}}",NULL) &&
            qc_apply(c,"{\"op\":\"qrelease\",\"id\":1}",NULL);
        if (pass && simulate) for (size_t i=0;i<qc_state_size(c);++i) pass&=close_to(probability(c,i),i<16?1.0/16:0);
        ok&=pass; qc_destroy(c);
    }
    report("proven inactive bits permit exact synthesis and clean reuse",ok);
    const struct {const char *name,*prepare,*change,*operation;} forged[]={
        {"updated basis invalidates stale known value",
         "{\"op\":\"qprepare\",\"id\":0,\"lower\":3,\"upper\":3}",
         "{\"op\":\"qadd_inplace\",\"id\":0,\"value\":5}",
         "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false,\"known\":3}}"},
        {"updated range invalidates stale active bits",
         "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":15}",
         "{\"op\":\"qadd_inplace\",\"id\":0,\"value\":5}",
         "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false,\"active\":4}}"},
        {"basis preparation cannot claim a uniform find domain",
         "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}",NULL,
         "{\"op\":\"qfind\",\"id\":0,\"upper\":15,\"predicate\":{\"op\":\"eq\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false,\"active\":4}]},{\"op\":\"const\",\"value\":3,\"bits\":32,\"signed\":true}]}}"},
        {"allocation signedness cannot be changed by an expression",
         "{\"op\":\"qprepare\",\"id\":0,\"lower\":3,\"upper\":3}",NULL,
         "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":true,\"known\":3}}"},
        {"output narrowing must be explicit in the expression",
         "{\"op\":\"qprepare\",\"id\":0,\"lower\":3,\"upper\":3}",NULL,
         "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"const\",\"value\":3,\"bits\":32,\"signed\":true}}"}
    };
    for (unsigned test=0;test<sizeof forged/sizeof *forged;++test) {
        ok=1;
        for (int simulate=0;simulate<=1;++simulate) {
            qc_context *c=qc_create(simulate,7);
            int pass=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}",NULL) &&
                qc_apply(c,forged[test].prepare,NULL) && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":8,\"signed\":false}",NULL);
            if (pass && forged[test].change) pass=qc_apply(c,forged[test].change,NULL);
            pass=pass && !qc_apply(c,forged[test].operation,NULL) && *qc_error(c);
            ok&=pass; qc_destroy(c);
        }
        report(forged[test].name,ok);
    }
    const char *documents[]={
        "{\"format\":\"cket.qir\",\"version\":1,\"version\":1,\"instructions\":[]}",
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"cloop_end\"}]}",
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"cloop_begin\"},{\"op\":\"ccontrol_end\"}]}",
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"ccontrol_begin\"}]}"
    };
    for (unsigned test=0;test<sizeof documents/sizeof *documents;++test) {
        qc_context *c=qc_create(0,7);
        ok=c && !qc_document(c,documents[test],1,0) && *qc_error(c);
        report(test?"reject unbalanced CPU control document":"reject duplicate document fields",ok); qc_destroy(c);
    }
}
#include "test_composition.inc"
#include "test_exhaustive.inc"
#include "test_fourier.inc"
#include "test_memory.inc"
#include "test_properties.inc"
#include "test_indexed.inc"
#include "test_scalable.inc"

static void output_cases(void) {
    const char *body="int classical=10; quant uint8_t x=superposition(0,255); uint8_t result=measure(x); printf(\"%d %u\\n\",classical+2,(unsigned)result);";
    int ok=compile(body,"--emit-ir",1); char *text=qc_read_file(artifact); cj_doc d; int parsed=text && cj_parse(&d,text),classic=0,quantum=0;
    if (parsed) {
        int symbols=cj_get(&d,0,"symbols");
        for (int i=symbols+1;symbols>=0 && i<d.tokens[symbols].next;i=d.tokens[i].next) {
            int name=cj_get(&d,i,"name"),type=cj_get(&d,i,"type");
            if (cj_equal(&d,name,"classical")) classic=!cj_bool(&d,cj_get(&d,type,"quantum"));
            if (cj_equal(&d,name,"x")) { uint64_t bits=0; cj_u64(&d,cj_get(&d,type,"bits"),&bits); quantum=cj_bool(&d,cj_get(&d,type,"quantum")) && bits==8; }
        }
        cj_free(&d);
    }
    report("real GCC semantic types and Quantum IR",ok && parsed && classic && quantum); free(text);
    ok=compile("int c=10; quant int q=42; quant bool b=1; measure(q); measure(b);","--emit-ir",1);
    text=qc_read_file(artifact); parsed=text && cj_parse(&d,text); unsigned cbits=0,qbits=0,bbits=0; int cq=1,qq=0,bq=0;
    if (parsed) {
        int symbols=cj_get(&d,0,"symbols");
        for (int i=symbols+1;symbols>=0 && i<d.tokens[symbols].next;i=d.tokens[i].next) {
            int name=cj_get(&d,i,"name"),type=cj_get(&d,i,"type"); uint64_t bits=0; cj_u64(&d,cj_get(&d,type,"bits"),&bits);
            if (cj_equal(&d,name,"c")) { cbits=(unsigned)bits; cq=cj_bool(&d,cj_get(&d,type,"quantum")); }
            if (cj_equal(&d,name,"q")) { qbits=(unsigned)bits; qq=cj_bool(&d,cj_get(&d,type,"quantum")); }
            if (cj_equal(&d,name,"b")) { bbits=(unsigned)bits; bq=cj_bool(&d,cj_get(&d,type,"quantum")); }
        }
        cj_free(&d);
    }
    report("int versus quant int and quant bool",ok && parsed && cbits==32 && qbits==cbits && bbits==1 && !cq && qq && bq); free(text);
    ok=compile(body,"--emit-ast",1); text=qc_read_file(artifact); report("genuine GCC AST",ok && text && strstr(text,"function_decl") && strstr(text,"cket_quant")); free(text);
    ok=compile(body,"--emit-qasm",1); text=qc_read_file(artifact); report("OpenQASM 3 export",ok && text && strstr(text,"OPENQASM 3.0;") && strstr(text,"qubit[8]") && strstr(text,"measure")); free(text);
    ok=compile(body,"--resources",1); text=qc_read_file(artifact); report("logical resource estimation",ok && text && strstr(text,"Logical qubits: 8") && strstr(text,"Ancillas:       0") && strstr(text,"H              8")); free(text);
    ok=compile(body,"--run",1); text=qc_read_file(output_path); int classical=-1; unsigned result=256;
    if (text) sscanf(text,"%d %u",&classical,&result); free(text);
    FILE *binary=fopen(artifact,"rb"); unsigned char magic[4]={0}; if (binary) { ok&=fread(magic,1,4,binary)==4; fclose(binary); }
    report("native ELF and CPU output",ok && classical==12 && result<256 && !memcmp(magic,"\177ELF",4));
    report("refuse flattening dynamic CPU control",compile("quant uint8_t x=0; for(int i=0;i<3;i++) x+=i; measure(x);","--emit-qasm",0));
    report("runtime errors propagate failure",compile("quant uint32_t x=1; measure(x);","--run",0));
}
int main(int argc,char **argv) {
    if (argc>1) { if (!realpath(argv[1],root)) return 2; } else if (!getcwd(root,sizeof root)) return 2;
    strcpy(temp,"/tmp/cket-tests-XXXXXX"); if (!mkdtemp(temp)) return 2;
    snprintf(source_path,sizeof source_path,"%s/program.c",temp); snprintf(output_path,sizeof output_path,"%s/stdout",temp);
    snprintf(error_path,sizeof error_path,"%s/stderr",temp); snprintf(artifact,sizeof artifact,"%s/artifact",temp);
    scalable_cases(); indexed_cases(); native_cases(); invalid_cases(); amplitude_cases(); backend_cases(); proof_cases(); composition_cases(); exhaustive_cases(); fourier_cases(); memory_cases(); property_cases(); output_cases();
    unlink(source_path); unlink(output_path); unlink(error_path); unlink(artifact); rmdir(temp);
    printf("\n%u passed, %u failed (native C tests, no external quantum SDK)\n",passed,failed); return failed?1:0;
}
