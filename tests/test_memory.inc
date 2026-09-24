static void memory_cases(void) {
    const struct {const char *name,*source;} valid[]={
        {"checked local pointer borrow","quant uint8_t x=7;quant uint8_t *p=&x;*p+=2;return measure(x)!=9;"},
        {"checked pointer alias sees register updates","quant uint8_t x=7;quant uint8_t *p=&x;quant uint8_t *q=p;*q^=3;return measure(*p)!=4;"},
        {"pointer parameter borrows scalar","#include <quantum.h>\nvoid add(quant uint8_t *p){*p+=2;}\nint main(void){quant uint8_t x=7;add(&x);return measure(x)!=9;}\n"},
        {"C array parameter borrows fixed vector","#include <quantum.h>\nvoid add(quant uint8_t a[2]){a[0]+=a[1];}\nint main(void){quant uint8_t a[2]={7,2};add(a);int x=measure(a[0]),y=measure(a[1]);return x!=9||y!=2;}\n"},
        {"constant pointer arithmetic on vector","quant uint8_t a[2]={1,7};quant uint8_t *p=a+1;*p+=2;int x=measure(*(p-1)),y=measure(*p);return x!=1||y!=9;"},
        {"const pointer quantum predicate","#include <quantum.h>\nquant bool p(quant const uint8_t *a){return *a==7;}\nint main(void){quant uint8_t x=7;quant bool f=p(&x);int r=measure(f);measure(x);return !r;}\n"},
        {"coherent update through pointer","quant bool c=1;quant uint8_t x=7;quant uint8_t *p=&x;if(c)*p+=2;int r=measure(x);measure(c);return r!=9;"},
        {"borrow nested pointer procedures","#include <quantum.h>\nvoid inc(quant uint8_t *p){*p+=1;}\nvoid twice(quant uint8_t *p){inc(p);inc(p);}\nint main(void){quant uint8_t x=7;twice(&x);return measure(x)!=9;}\n"},
        {"register XOR and inverse","quant uint8_t x=7,y=2;x^=y;x^=y;int r=measure(x);measure(y);return r!=7;"},
        {"classical XOR argument evaluated once","quant uint8_t x=7;int n=2;x^=n++;return measure(x)!=5||n!=3;"},
        {"XOR of quantum expression","quant uint8_t x=7,y=2;x^=(y|1);int r=measure(x);measure(y);return r!=4;"},
        {"array procedure with bounded for","#include <quantum.h>\nvoid add(quant uint8_t *a){for(int i=0;i<2;++i)a[i]+=i+1;}\nint main(void){quant uint8_t a[2]={1,2};add(a);int x=measure(a[0]),y=measure(a[1]);return x!=2||y!=4;}\n"},
        {"negative offset within borrowed array","quant uint8_t a[2]={1,7};quant uint8_t *p=&a[1];*(p-1)+=2;int x=measure(a[0]),y=measure(a[1]);return x!=3||y!=7;"},
        {"runtime CPU initializer evaluated once","int n=7;quant uint8_t x=n++;return measure(x)!=7||n!=8;"},
        {"runtime signed initializer conversion","int n=-7;quant int8_t x=n;return measure(x)!=-7;"},
        {"runtime bool initializer conversion","int n=2;quant bool x=n;return measure(x)!=1;"},
        {"classical read function prepares a quantum basis","#include <quantum.h>\nstatic int calls;static uint8_t read_value(void){++calls;return 7;}\nint main(void){quant uint8_t x=read_value();return measure(x)!=7||calls!=1;}\n"},
        {"explicit measure prepares fresh basis","quant uint8_t x=7;quant uint8_t y=measure(x);int r=measure(y);return r!=7||measure(x)!=7;"},
        {"CPU vector initializers evaluated in order","int n=1;quant uint8_t a[2]={n++,n++};int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=2||n!=3;"},
        {"procedure-local CPU operands evaluated once","#include <quantum.h>\nvoid add(quant uint8_t x){int n=0;x+=++n;x+=n;}\nint main(void){quant uint8_t x=0;add(x);return measure(x)!=2;}\n"},
        {"final void return in quantum procedure","#include <quantum.h>\nvoid add(quant uint8_t *x){*x+=1;return;}\nint main(void){quant uint8_t x=7;add(&x);return measure(x)!=8;}\n"},
        {"empty quantum procedure returns to caller","#include <quantum.h>\nvoid empty(quant uint8_t x){return;}\nint main(void){quant uint8_t x=7;empty(x);return measure(x)!=7;}\n"}
    };
    for(unsigned i=0;i<sizeof valid/sizeof *valid;++i) report(valid[i].name,compile(valid[i].source,"--run",1));
    const struct {const char *name,*source;} invalid[]={
        {"borrow of classical scalar","uint8_t x=7;quant uint8_t *p=&x;"},
        {"quantum reference escape to CPU call","quant uint8_t x=7;quant uint8_t *p=&x;printf(\"%p\",(void*)p);measure(x);"},
        {"quantum reference rebinding","quant uint8_t x=1,y=2;quant uint8_t *p=&x;p=&y;measure(x);measure(y);"},
        {"pointer reinterpretation cannot change signed C reads","quant uint8_t x=255;quant uint8_t *p=&x;quant bool f=*((int8_t*)p)<0;measure(f);measure(x);"},
        {"indexed pointer reinterpretation cannot change signed C reads","quant uint8_t a[2]={255,7};quant uint8_t *p=a;int i=0;measure(((int8_t*)p)[i]);measure(a[0]);measure(a[1]);"},
        {"one-past quantum pointer dereference","quant uint8_t a[2]={1,2};quant uint8_t *p=a+2;measure(*p);"},
        {"unmeasured quantum pointer index","quant uint8_t a[2]={1,2};quant uint8_t *p=a;quant uint8_t i=1;measure(p[i]);measure(i);"},
        {"overlapping mutable pointer parameters","#include <quantum.h>\nvoid add(quant uint8_t *a,quant uint8_t *b){*a+=1;*b+=1;}\nint main(void){quant uint8_t x=7;add(&x,&x);measure(x);}\n"},
        {"overlapping scalar and pointer parameters","#include <quantum.h>\nvoid add(quant uint8_t a,quant uint8_t *b){a+=1;*b+=1;}\nint main(void){quant uint8_t x=7;add(x,&x);measure(x);}\n"},
        {"pointer parameter ownership consumption","#include <quantum.h>\nvoid steal(quant uint8_t *p){quant uint8_t x=*p;measure(x);}\nint main(void){quant uint8_t x=7;steal(&x);measure(x);}\n"},
        {"direct pointer parameter result","#include <quantum.h>\nquant uint8_t bad(quant uint8_t *p){return *p;}\nint main(void){return 0;}\n"},
        {"use after move through alias","quant uint8_t x=7;quant uint8_t *p=&x;quant uint8_t y=x;measure(*p);measure(y);"},
        {"destructive self XOR","quant uint8_t x=7;x^=x;measure(x);"},
        {"borrow consumed through a local pointer alias","#include <quantum.h>\nvoid steal(quant uint8_t x){quant uint8_t *p=&x;quant uint8_t y=*p;measure(y);}\nint main(void){quant uint8_t x=7;steal(x);}\n"},
        {"read-only scalar loan forwarded to mutator","#include <quantum.h>\nvoid add(quant uint8_t x){x+=1;}\nvoid bad(quant const uint8_t x){add(x);}\nint main(void){quant uint8_t x=7;bad(x);measure(x);}\n"},
        {"read-only pointer loan forwarded to mutator","#include <quantum.h>\nvoid add(quant uint8_t *x){*x+=1;}\nvoid bad(quant const uint8_t *x){add((uint8_t*)x);}\nint main(void){quant uint8_t x=7;bad(&x);measure(x);}\n"},
        {"conditional early procedure return","#include <quantum.h>\nvoid bad(quant uint8_t x,int c){if(c)return;x+=1;}\nint main(void){quant uint8_t x=7;bad(x,1);measure(x);}\n"}
    };
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i) {
        char name[160];snprintf(name,sizeof name,"reject %s",invalid[i].name);
        report(name,compile(invalid[i].source,"--emit-ir",0));
    }
    /* Every input of a full 16-bit register: no 12-bit table is available. */
    const char *predicates[]={"x==40000","x!=40000","x<40000","x<=40000","x>40000","x>=40000","!x","(x^0x1234)==40000"};
    for(unsigned test=0;test<sizeof predicates/sizeof *predicates;++test) {
        char source[512],name[160];snprintf(source,sizeof source,"quant uint16_t x=superposition(0,65535);quant bool f=%s;measure(f);measure(x);",predicates[test]);
        qc_context *c=state(source);int ok=c && qc_qubits(c)==17;
        if(c) for(size_t i=0;i<qc_state_size(c);++i) {
            unsigned x=(unsigned)i&65535,flag=(unsigned)(i>>16);
            int expected=test==0?x==40000:test==1?x!=40000:test==2?x<40000:test==3?x<=40000:
                test==4?x>40000:test==5?x>=40000:test==6?!x:(x^0x1234)==40000;
            ok&=cabs(qc_amplitude(c,i)-(flag==(unsigned)expected?1.0/256:0))<1e-10;
        }
        snprintf(name,sizeof name,"all 65536 inputs through structured predicate: %s",predicates[test]);report(name,ok);qc_destroy(c);
    }
    qc_context *c=state("quant uint8_t x=0,y=superposition(0,255);x^=y;measure(x);measure(y);");int ok=c!=NULL;
    if(c) for(size_t i=0;i<qc_state_size(c);++i)
        ok&=cabs(qc_amplitude(c,i)-((i&255)==((i>>8)&255)?1.0/16:0))<1e-10;
    report("quantum XOR creates correlation on every basis",ok);qc_destroy(c);
    report("32-bit structured predicate frontend exports",compile("quant uint32_t x=superposition(0,4294967295u);quant bool f=x<4000000000u;if(f)phase(.5);measure(x);","--emit-qasm",1));
    report("two 32-bit quantum registers XOR without table",compile("quant uint32_t x=0,y=superposition(0,4294967295u);x^=y;measure(x);measure(y);","--emit-qasm",1));
    report("runtime basis initialization refuses static export",compile("int n=7;quant uint8_t x=n;measure(x);","--emit-qasm",0));
}
