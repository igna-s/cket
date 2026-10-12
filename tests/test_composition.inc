/* Real translation units exercise composition; no alternate C evaluator. */
static void composition_cases(void) {
    const struct {
        const char *name, *source;
    } valid[] = {
        {"quantum predicate in another function",
         "#include <quantum.h>\nquant bool p(quant const uint8_t x){return x*x==9;}\nint main(void){quant uint8_t x=3;quant bool f=p(x);int r=measure(f);measure(x);return !r;}\n"},
        {"nested quantum value functions",
         "#include <quantum.h>\nquant uint8_t triple(quant uint8_t x){return x*3;}\nquant bool p(quant uint8_t x){return triple(x)==9;}\nint main(void){quant uint8_t x=3;quant bool f=p(x);int r=measure(f);measure(x);return !r;}\n"},
        {"mutable register borrow and repeated calls",
         "#include <quantum.h>\nvoid add(quant uint8_t x,unsigned n){x+=n;}\nint main(void){quant uint8_t x=1;add(x,2);add(x,3);return measure(x)!=6;}\n"},
        {"borrow with a CPU argument evaluated once",
         "#include <quantum.h>\nvoid add(quant uint8_t x,unsigned n){x+=n;x+=n;}\nint main(void){quant uint8_t x=1;int i=2;add(x,i++);return measure(x)!=5||i!=3;}\n"},
        {"coherent procedure composition",
         "#include <quantum.h>\nvoid add(quant uint8_t x){x+=2;}\nint main(void){quant bool c=1;quant uint8_t x=3;if(c)add(x);int r=measure(x);measure(c);return r!=5;}\n"},
        {"owned function temporaries and repeated calls",
         "#include <quantum.h>\nvoid mark(quant uint8_t x){quant bool f=x==3;if(f)phase_flip();}\nint main(void){quant uint8_t x=3;mark(x);mark(x);return measure(x)!=3;}\n"},
        {"fixed quantum array initialization and updates",
         "quant uint8_t a[2]={7,2};a[0]+=a[1];int x=measure(a[0]),y=measure(a[1]);return x!=9||y!=2;"},
        {"quantum array designated initialization",
         "quant uint8_t a[2]={[1]=7};a[0]+=2;int x=measure(a[0]),y=measure(a[1]);return x!=2||y!=7;"},
        {"multidimensional quantum boolean array",
         "quant bool a[2][2]={{1,0},{0,1}};a[1][0]=!a[1][0];int r=measure(a[0][0])+measure(a[0][1])+measure(a[1][0])+measure(a[1][1]);return r!=3;"},
        {"quantum integer structure",
         "struct S{uint8_t x,y;};quant struct S s={3,4};s.y+=s.x;int x=measure(s.x),y=measure(s.y);return x!=3||y!=7;"},
        {"nested quantum product and sizeof",
         "struct S{bool b[2];uint8_t x;};quant struct S s={{1,0},7};s.b[1]=!s.b[1];int b=measure(s.b[0])+measure(s.b[1]),x=measure(s.x);return b!=2||x!=7||sizeof(s)!=sizeof(struct S);"},
        {"borrowing a quantum array element",
         "#include <quantum.h>\nvoid add(quant uint8_t x){x+=3;}\nint main(void){quant uint8_t a[2]={1,2};add(a[1]);int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=5;}\n"},
        {"element ownership move",
         "quant uint8_t a[2]={1,7};quant uint8_t b=a[1];int x=measure(a[0]),y=measure(b);return x!=1||y!=7;"},
        {"static for quantum vector writes and reads",
         "quant uint8_t a[3]={0};for(int i=0;i<3;++i)a[i]+=i+1;int bad=0;for(int i=0;i<3;++i)bad|=measure(a[i])!=i+1;return bad;"},
        {"static descending for quantum vector",
         "quant uint8_t a[2]={0};for(int i=1;i>=0;--i)a[i]+=i+3;int x=measure(a[0]),y=measure(a[1]);return x!=3||y!=4;"},
        {"quantum register subtraction",
         "quant uint8_t x=2,y=7;x-=y;int a=measure(x),b=measure(y);return a!=251||b!=7;"},
        {"static for coherent condition",
         "quant bool c=1;quant uint8_t x=0;for(int i=0;i<3;++i)if(c)x+=i+1;int a=measure(x),b=measure(c);return a!=6||b!=1;"}};

    for (unsigned i = 0; i < sizeof valid / sizeof *valid; ++i)
        report(valid[i].name, compile(valid[i].source, "--run", 1));

    const struct {
        const char *name, *source;
    } invalid[] = {
        {"reset through a const quantum pointer parameter",
         "#include <quantum.h>\nvoid bad(quant const uint8_t *p){reset(*p);}\nint main(void){quant uint8_t x=7;bad(&x);measure(x);}\n"},
        {"Fourier through a const scalar parameter",
         "#include <quantum.h>\nvoid bad(quant const uint8_t x){fourier(x);}\nint main(void){quant uint8_t x=7;bad(x);measure(x);}\n"},
        {"measurement through a const quantum borrow",
         "#include <quantum.h>\nvoid bad(quant const uint8_t *p){measure(*p);}\nint main(void){quant uint8_t x=7;bad(&x);measure(x);}\n"},
        {"reset through a local const quantum pointer",
         "quant uint8_t x=7;quant const uint8_t *p=&x;reset(*p);measure(x);"},
        {"measurement through a CPU-indexed const quantum pointer",
         "quant uint8_t a[2]={1,7};quant const uint8_t *p=a;int i=1;measure(p[i]);reset(a[0]);reset(a[1]);"},
        {"quantum pointer alias cannot discard const borrow",
         "quant uint8_t x=7;quant const uint8_t *p=&x;quant uint8_t *q=(uint8_t*)p;reset(*q);measure(x);"},
        {"quantum result at the CPU program entry point",
         "#include <quantum.h>\nquant int main(void){return 3;}\n"},
        {"mutable quantum parameter alias",
         "#include <quantum.h>\nvoid add(quant uint8_t a,quant uint8_t b){a+=1;b+=1;}\nint main(void){quant uint8_t x=1;add(x,x);measure(x);}\n"},
        {"borrowed register ownership consumption",
         "#include <quantum.h>\nvoid steal(quant uint8_t a){quant uint8_t b=a;measure(b);}\nint main(void){quant uint8_t x=1;steal(x);measure(x);}\n"},
        {"direct borrowed parameter result",
         "#include <quantum.h>\nquant uint8_t copy(quant uint8_t x){return x;}\nint main(void){return 0;}\n"},
        {"classical argument for a quantum parameter",
         "#include <quantum.h>\nvoid add(quant uint8_t x){x+=1;}\nint main(void){uint8_t x=1;add(x);}\n"},
        {"quantum argument for a classical parameter",
         "#include <quantum.h>\nvoid add(quant uint8_t x,uint8_t n){x+=n;}\nint main(void){quant uint8_t x=1,y=2;add(x,y);measure(x);measure(y);}\n"},
        {"unqualified quantum function result",
         "#include <quantum.h>\nbool p(quant uint8_t x){return x==3;}\nint main(void){return 0;}\n"},
        {"quantum function pointer escape",
         "#include <quantum.h>\nquant bool p(quant uint8_t x){return x==3;}\nint main(void){bool(*f)(uint8_t)=p;return f(3);}\n"},
        {"recursive quantum composition",
         "#include <quantum.h>\nquant bool p(quant uint8_t x){return p(x);}\nint main(void){quant uint8_t x=3;quant bool f=p(x);measure(f);measure(x);}\n"},
        {"borrowed find input",
         "#include <quantum.h>\nvoid search(quant uint8_t x){quant uint8_t a=find(x,x==3);measure(a);}\nint main(void){quant uint8_t x=superposition(0,15);search(x);measure(x);}\n"},
        {"indexed measurement leaves an unconsumed owner",
         "quant uint8_t a[2]={1,2};int i=measure(a[0])&1;measure(a[i]);"},
        {"quantum index without QRAM",
         "quant uint8_t a[2]={1,2};quant bool i=superposition(0,1);measure(a[i]);measure(i);"},
        {"quantum product pointer escape",
         "quant uint8_t a[2]={1,2};void *p=a;measure(a[0]);measure(a[1]);"},
        {"quantum product outside bounds", "quant uint8_t a[2]={1,2};measure(a[2]);"},
        {"quantum element use after move",
         "quant uint8_t a[2]={1,2};quant uint8_t b=a[1];measure(a[1]);measure(a[0]);measure(b);"},
        {"destructive quantum field write",
         "struct S{uint8_t x;};quant struct S s={7};s.x=0;measure(s.x);"},
        {"quantum structure with pointer field", "struct S{uint8_t *p;};quant struct S s;"},
        {"quantum union", "union S{uint8_t x,y;};quant union S s;"},
        {"quantum variable length array", "int n=2;quant uint8_t a[n];"},
        {"quantum vector implicit CPU read",
         "quant uint8_t a[2]={1,2};int x=a[0];measure(a[0]);measure(a[1]);"}};

    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        char name[160];
        snprintf(name, sizeof name, "reject %s", invalid[i].name);
        report(name, compile(invalid[i].source, "--emit-ir", 0));
    }
    qc_context *c = state(
        "#include <quantum.h>\nquant bool p(quant const uint8_t x){return x*x==9;}\nint main(void){quant uint8_t x=superposition(0,15);quant uint8_t a=find(x,p(x));measure(a);}\n");
    int ok = c && close_to(probability(c, 3), .9613189697265625);
    if (c)
        for (size_t i = 256; i < qc_state_size(c); ++i)
            ok &= close_to(probability(c, i), 0);
    report("external predicate Grover amplitudes and clean oracle", ok);
    qc_destroy(c);
    c = state(
        "#include <quantum.h>\nvoid mark(quant uint8_t x){if(x==3)phase_flip();}\nint main(void){quant uint8_t x=superposition(0,15);mark(x);measure(x);}\n");
    ok = c != NULL;
    if (c)
        for (size_t i = 0; i < qc_state_size(c); ++i)
            ok &= cabs(qc_amplitude(c, i) - (i < 16 ? (i == 3 ? -.25 : .25) : 0)) < 1e-10;
    report("procedure phase and ancilla cleanup on every input", ok);
    qc_destroy(c);
    c = state(
        "quant uint8_t a[2]={superposition(0,255),superposition(0,255)};a[0]+=a[1];measure(a[0]);measure(a[1]);");
    ok = c && qc_qubits(c) == 16;
    if (c)
        for (size_t i = 0; i < qc_state_size(c); ++i)
            ok &= cabs(qc_amplitude(c, i) - 1.0 / 256) < 1e-10;
    report("register addition on all 65536 pairs without a truth table", ok);
    qc_destroy(c);
    c = state("quant uint8_t x=0,y=superposition(0,255);x+=y;measure(x);measure(y);");
    ok = c != NULL;
    if (c)
        for (size_t i = 0; i < qc_state_size(c); ++i) {
            unsigned x = (unsigned)i & 255, y = (unsigned)(i >> 8) & 255;
            ok &= cabs(qc_amplitude(c, i) - (x == y ? 1.0 / 16 : 0)) < 1e-10;
        }
    report("register addition computes correlated output rather than cloning", ok);
    qc_destroy(c);
    c = qc_create(0, 7);
    ok = c && qc_apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":32,\"signed\":false}", NULL) &&
         qc_apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":4294967295}", NULL) &&
         qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":32,\"signed\":false}", NULL) &&
         qc_apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":4294967295}", NULL) &&
         qc_apply(c, "{\"op\":\"qadd_register\",\"id\":0,\"source\":1}", NULL) &&
         qc_qubits(c) == 64;
    report("32-bit register addition exports without enumeration or ancillas", ok);
    qc_destroy(c);
}
