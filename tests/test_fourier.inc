/* Independent reference: mathematical DFT matrix, rather than gate order. */
static void fourier_cases(void) {
    const double tau=6.283185307179586476925286766559;
    int ok=1; unsigned bases=0;
    for(unsigned width=1;width<=6;++width) for(unsigned x=0;x<(1u<<width);++x) {
        qc_context *c=qc_create(1,7);char op[256];unsigned size=1u<<width;
        snprintf(op,sizeof op,"{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",width);
        int good=c && qc_apply(c,op,NULL);
        snprintf(op,sizeof op,"{\"op\":\"qprepare\",\"id\":0,\"lower\":%u,\"upper\":%u}",x,x);
        good=good && qc_apply(c,op,NULL) && qc_apply(c,"{\"op\":\"qfourier\",\"id\":0}",NULL);
        if(good) for(unsigned k=0;k<size;++k) {
            double angle=tau*x*k/size;
            good&=cabs(qc_amplitude(c,k)-(cos(angle)+I*sin(angle))/sqrt(size))<1e-10;
        }
        good=good && qc_apply(c,"{\"op\":\"qfourier\",\"id\":0,\"inverse\":true}",NULL);
        if(good) for(unsigned k=0;k<size;++k) good&=cabs(qc_amplitude(c,k)-(k==x?1:0))<1e-10;
        ok&=good;++bases;qc_destroy(c);
    }
    report("QFT matrix and inverse on all 126 bases of widths 1..6",ok && bases==126);
    qc_context *c=qc_create(1,7);
    ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}",NULL) &&
        qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}",NULL) &&
        qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}",NULL) &&
        qc_apply(c,"{\"op\":\"qprepare\",\"id\":1,\"lower\":3,\"upper\":3}",NULL) &&
        qc_apply(c,"{\"op\":\"qfourier\",\"id\":1,\"controls\":[{\"id\":0,\"value\":1}]}",NULL);
    if(ok) for(unsigned k=0;k<8;++k) {
        double angle=tau*3*k/8;
        ok&=cabs(qc_amplitude(c,k*2)-(k==3?1/sqrt(2):0))<1e-10;
        ok&=cabs(qc_amplitude(c,k*2+1)-(cos(angle)+I*sin(angle))/4)<1e-10;
    }
    ok=ok && qc_apply(c,"{\"op\":\"qfourier\",\"id\":1,\"inverse\":true,\"controls\":[{\"id\":0,\"value\":1}]}",NULL);
    if(ok) for(unsigned k=0;k<16;++k) ok&=cabs(qc_amplitude(c,k)-((k==6||k==7)?1/sqrt(2):0))<1e-10;
    report("controlled QFT and inverse preserve joint amplitudes",ok);qc_destroy(c);
    c=state("quant uint8_t x=superposition(0,15);if(x==3)phase(0.5);measure(x);");ok=c!=NULL;
    if(c) for(size_t i=0;i<qc_state_size(c);++i) {
        double complex expected=i<16?.25:0;
        if(i==3) expected*=cos(.5)+I*sin(.5);
        ok&=cabs(qc_amplitude(c,i)-expected)<1e-10;
    }
    report("parameterized phase and condition cleanup",ok);qc_destroy(c);
    c=state("quant uint8_t x=3;fourier(x);measure(x);");ok=c!=NULL;
    if(c) for(size_t i=0;i<qc_state_size(c);++i) {
        double angle=tau*3*(i&255)/256;
        ok&=cabs(qc_amplitude(c,i)-(i<256?(cos(angle)+I*sin(angle))/16:0))<1e-10;
    }
    report("C Fourier matches independent 256 by 256 DFT",ok);qc_destroy(c);
    c=state("quant uint8_t x=0;quant bool e=1;fourier(x);for(int i=0;i<8;++i)if(x&(1u<<i))if(e)phase(6.28318530717958647692*(3.0/256.0)*(1u<<i));inverse_fourier(x);measure(x);measure(e);");
    ok=c && close_to(probability(c,259),1);
    if(c) for(size_t i=0;i<qc_state_size(c);++i) if(i!=259) ok&=close_to(probability(c,i),0);
    report("phase estimation gives 3 with every condition ancilla clean",ok);qc_destroy(c);
    const struct {const char *name,*source;} valid[]={
        {"C Fourier inverse round trip","quant uint8_t x=7;fourier(x);inverse_fourier(x);return measure(x)!=7;"},
        {"Fourier in borrowed procedure","#include <quantum.h>\nvoid roundtrip(quant uint8_t x){fourier(x);inverse_fourier(x);}\nint main(void){quant uint8_t x=7;roundtrip(x);return measure(x)!=7;}\n"},
        {"runtime phase argument evaluated once","double angle=.5;phase(angle++);return angle!=1.5;"},
        {"runtime phase under coherent control","double angle=.5;quant bool c=1;if(c)phase(angle);return measure(c)!=1;"},
        {"array expression product initializers","quant uint8_t a[2]={1,2};quant uint8_t b[1]={a[0]+a[1]};int r=measure(b[0]);measure(a[0]);measure(a[1]);return r!=3;"}
    };
    for(unsigned i=0;i<sizeof valid/sizeof *valid;++i) report(valid[i].name,compile(valid[i].source,"--run",1));
    const struct {const char *name,*source;} invalid[]={
        {"quantum phase angle","quant uint8_t x=1;phase(x);measure(x);"},
        {"infinite phase angle","phase(__builtin_inf());"},
        {"NaN phase angle","phase(__builtin_nan(\"\"));"},
        {"CPU effect in coherent phase","quant bool c=1;double a=.5;if(c)phase(a++);measure(c);"},
        {"Fourier changes inverse dependency","quant uint8_t x=1;quant bool f=x==1;fourier(x);measure(f);measure(x);"},
        {"Fourier on derived storage","quant uint8_t x=1;quant uint8_t y=x+1;fourier(y);measure(y);measure(x);"},
        {"borrow consumed into array","#include <quantum.h>\nvoid steal(quant uint8_t x){quant uint8_t a[1]={x};measure(a[0]);}\nint main(void){quant uint8_t x=1;steal(x);measure(x);}\n"},
        {"measurement in quantum value function","#include <quantum.h>\nquant bool bad(quant uint8_t x){return measure(x)!=0;}\nint main(void){return 0;}\n"}
    };
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i) {
        char name[160];snprintf(name,sizeof name,"reject %s",invalid[i].name);
        report(name,compile(invalid[i].source,"--emit-ir",0));
    }
    report("reject unknown runtime phase static export",compile("double a=.5;phase(a);","--emit-qasm",0));
    ok=compile("quant bool a[1]={0};for(uint8_t i=0;i<=255;++i)a[0]=!a[0];measure(a[0]);","--emit-ir",1);
    char *loop_ir=qc_read_file(artifact);
    report("wrapping CPU counter is preserved rather than falsely terminated",ok && loop_ir && strstr(loop_ir,"cloop_begin"));
    free(loop_ir);
    c=qc_create(1,7);ok=c && qc_apply_phase(c,"{\"op\":\"qphase\",\"dynamic\":true}",.5) &&
        cabs(qc_amplitude(c,0)-(cos(.5)+I*sin(.5)))<1e-10;
    report("binary64 runtime phase API",ok);qc_destroy(c);
    c=qc_create(1,7);report("reject infinite runtime phase",c && !qc_apply_phase(c,"{\"op\":\"qphase\",\"dynamic\":true}",INFINITY));qc_destroy(c);
    c=qc_create(0,7);ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":64,\"signed\":false}",NULL) &&
        qc_apply(c,"{\"op\":\"qfourier\",\"id\":0}",NULL);
    report("64-bit Fourier export uses polynomial synthesis",ok);qc_destroy(c);
    c=qc_create(0,7);ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}",NULL) &&
        qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}",NULL) &&
        qc_apply(c,"{\"op\":\"qfourier\",\"id\":1,\"controls\":[{\"id\":0,\"value\":0}]}",NULL);
    char *qasm=NULL;size_t length=0;FILE *f=open_memstream(&qasm,&length);
    if(f){ok=ok && qc_qasm(c,f);fclose(f);}else ok=0;
    ok=ok && qasm && strstr(qasm,"ctrl(1) @ h q[0], q[1];") && strstr(qasm,"x q[0];");
    report("QASM preserves open controlled Hadamard",ok);free(qasm);qc_destroy(c);
}
