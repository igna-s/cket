static void indexed_cases(void) {
    const struct {const char *name,*source;} valid[]={
        {"CPU index selects quantum vector update","quant uint8_t a[2]={1,2};int i=1;a[i]+=3;int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=5;"},
        {"CPU indexed subtraction and XOR","quant uint8_t a[2]={1,7};int i=1;a[i]-=2;a[i]^=3;int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=6;"},
        {"indexed reassociation must not introduce signed overflow","quant uint8_t a[2]={1,7};int i=1,n=INT32_MIN;a[i]=(a[i]+n)-1;int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=6;"},
        {"CPU indexed increment and decrement","quant uint8_t a[2]={1,7};int i=1;++a[i];a[i]--;int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=7;"},
        {"dynamic index evaluated once","quant uint8_t a[2]={1,7};int i=0;a[i++]+=2;int x=measure(a[0]),y=measure(a[1]);return i!=1||x!=3||y!=7;"},
        {"indexed CPU operand evaluated once","quant uint8_t a[2]={1,7};int i=1,n=2;a[i]+=n++;int x=measure(a[0]),y=measure(a[1]);return n!=3||x!=1||y!=9;"},
        {"dynamic explicit vector measurement","quant uint8_t a[2]={1,7};int i=1;int r=measure(a[i]);int x=measure(a[0]),y=measure(a[1]);return r!=7||x!=1||y!=7;"},
        {"dynamic explicit vector reset","quant uint8_t a[2]={1,7};int i=1;reset(a[i]);int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=0;"},
        {"dynamic vector Fourier and inverse","quant uint8_t a[2]={1,7};int i=1;fourier(a[i]);inverse_fourier(a[i]);int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=7;"},
        {"CPU loop updates dynamically indexed vector","quant uint8_t a[2]={1,7};int n=2;for(int i=0;i<n;++i)a[i]+=i+1;int x=measure(a[0]),y=measure(a[1]);return x!=2||y!=9;"},
        {"CPU loop measures indexed vector explicitly","quant uint8_t a[2]={1,7};int n=2,sum=0;for(int i=0;i<n;++i)sum+=measure(a[i]);measure(a[0]);measure(a[1]);return sum!=8;"},
        {"CPU indexed pointer selects borrowed element","quant uint8_t a[2]={1,7};quant uint8_t *p=a;int i=1;p[i]+=2;int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=9;"},
        {"dynamic index through array function parameter","#include <quantum.h>\nvoid add(quant uint8_t *a,int i){a[i]+=2;}\nint main(void){quant uint8_t a[2]={1,7};int i=1;add(a,i);int x=measure(a[0]),y=measure(a[1]);return x!=1||y!=9;}\n"},
        {"negative dynamic pointer offset within borrow","quant uint8_t a[2]={1,7};quant uint8_t *p=&a[1];int i=-1;p[i]+=2;int x=measure(a[0]),y=measure(a[1]);return x!=3||y!=7;"},
        {"signed dynamic explicit measurement","quant int8_t a[2]={-1,-7};int i=1;int r=measure(a[i]);measure(a[0]);measure(a[1]);return r!=-7;"},
        {"measurement operand precedes indexed update","quant uint8_t a[2]={1,7};quant bool y=1;int i=1;a[i]+=measure(y);int x=measure(a[0]),z=measure(a[1]);return x!=1||z!=8;"},
        {"clean measured operand is released after evaluation","quant uint8_t a[2]={1,7};quant bool y=0;int i=1;a[i]+=measure(y);int x=measure(a[0]),z=measure(a[1]);return x!=1||z!=7;"},
        {"measurement selects array element before update","quant uint8_t a[2]={1,7};quant bool y=1;a[measure(y)]+=2;int x=measure(a[0]),z=measure(a[1]);return x!=1||z!=9;"}
    };
    for(unsigned i=0;i<sizeof valid/sizeof *valid;++i)report(valid[i].name,compile(valid[i].source,"--run",1));
    const struct {const char *name,*source;} invalid[]={
        {"indexed access cannot implicitly read quantum data","quant uint8_t a[2]={1,7};int i=1;int x=a[i];measure(a[0]);measure(a[1]);"},
        {"indexed update cannot substitute another element","quant uint8_t a[2]={1,7};int i=0,j=1;a[i]=a[j]+2;measure(a[0]);measure(a[1]);"},
        {"indexed narrowing cannot masquerade as addition","quant uint16_t a[2]={1,7};int i=1;a[i]=(int8_t)a[i]+2;measure(a[0]);measure(a[1]);"},
        {"measuring one unknown element does not consume all owners","quant uint8_t a[2]={1,7};int i=1;measure(a[i]);"},
        {"indexed measurement cannot invalidate pending inverse","quant uint8_t a[2]={1,7};quant bool f=a[1]==7;int i=1;measure(a[i]);measure(f);measure(a[0]);measure(a[1]);"},
        {"indexed coherent update requires a transformation","quant bool c=1;quant uint8_t a[2]={1,7};int i=1;if(c)a[i]+=2;measure(c);measure(a[0]);measure(a[1]);"},
        {"measurement in index cannot consume a later transformed owner","quant uint8_t a[2]={0,7};fourier(a[measure(a[0])]);measure(a[1]);"}
    };
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i)report(invalid[i].name,compile(invalid[i].source,"--emit-ir",0));
    report("negative array index fails at runtime",compile("quant uint8_t a[2]={1,7};int i=-1;measure(a[i]);measure(a[0]);measure(a[1]);","--run",0));
    report("large array index fails at runtime",compile("quant uint8_t a[2]={1,7};uint64_t i=UINT64_MAX;a[i]+=2;measure(a[0]);measure(a[1]);","--run",0));
    report("static export refuses dynamic register selection",compile("quant uint8_t a[2]={1,7};int i=1;measure(a[i]);measure(a[0]);measure(a[1]);","--emit-qasm",0));
    int ok=1;
    const char *addition="{\"op\":\"qindexed\",\"ids\":[0,1],\"action\":{\"op\":\"qadd_inplace\",\"id\":0,\"dynamic\":true}}";
    const char *xor_update="{\"op\":\"qindexed\",\"ids\":[0,1],\"action\":{\"op\":\"qxor_inplace\",\"id\":0,\"dynamic\":true}}";
    for(unsigned index=0;index<2;++index)for(unsigned a=0;a<8;++a)for(unsigned b=0;b<8;++b) {
        qc_context *c=qc_create(1,7);char prep[128];
        ok&=qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}",NULL)&&
            qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}",NULL);
        snprintf(prep,sizeof prep,"{\"op\":\"qprepare\",\"id\":0,\"lower\":%u,\"upper\":%u}",a,a);ok&=qc_apply(c,prep,NULL);
        snprintf(prep,sizeof prep,"{\"op\":\"qprepare\",\"id\":1,\"lower\":%u,\"upper\":%u}",b,b);ok&=qc_apply(c,prep,NULL);
        ok&=qc_apply_index(c,addition,index,1,3,NULL)&&qc_apply_index(c,xor_update,index,1,5,NULL);
        unsigned x=index?a:((a+3)&7)^5,y=index?((b+3)&7)^5:b;
        ok&=close_to(probability(c,x|(y<<3)),1);
        ok&=qc_apply_index(c,xor_update,index,1,5,NULL)&&qc_apply_index(c,addition,index,1,UINT64_C(0)-3,NULL);
        ok&=close_to(probability(c,a|(b<<3)),1);qc_destroy(c);
    }
    report("indexed updates and inverses on every 3-bit basis pair",ok);
    qc_context *c=qc_create(1,7);
    const char *f="{\"op\":\"qindexed\",\"ids\":[0,1],\"action\":{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}}";
    ok=qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}",NULL)&&qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}",NULL)&&
       qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}",NULL)&&qc_apply(c,"{\"op\":\"qprepare\",\"id\":1,\"lower\":1,\"upper\":1}",NULL)&&qc_apply_index(c,f,1,0,0,NULL);
    for(unsigned i=0;i<4;++i)ok&=cabs(qc_amplitude(c,i)-(i&2?-.5:.5))<1e-10;
    report("indexed Fourier preserves relative phase and other register",ok);qc_destroy(c);
    const char *bad[]={
        "{\"op\":\"qindexed\",\"ids\":[0,0],\"action\":{\"op\":\"qreset\",\"id\":0}}",
        "{\"op\":\"qindexed\",\"ids\":[0,1],\"action\":{\"op\":\"qreset\",\"id\":1}}",
        "{\"op\":\"qindexed\",\"ids\":[0,1],\"action\":{\"op\":\"qrelease\",\"id\":0}}",
        "{\"op\":\"qindexed\",\"ids\":[0,1],\"action\":{\"op\":\"qreset\",\"id\":0,\"controls\":[]}}",
        "{\"op\":\"qindexed\",\"ids\":[],\"action\":{\"op\":\"qreset\",\"id\":0}}"
    };
    ok=1;
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i) {
        c=qc_create(0,7);
        qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}",NULL);qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}",NULL);
        qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}",NULL);qc_apply(c,"{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":0}",NULL);
        ok&=!qc_apply_index(c,bad[i],0,0,0,NULL)&&*qc_error(c);qc_destroy(c);
    }
    report("indexed IR rejects duplicate IDs controls and lifetime actions",ok);
}
