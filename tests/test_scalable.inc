static void signed_arithmetic_cases(void) {
    const char *operators[]={"mul","div","mod","eq","ne","lt","le","gt","ge","gt"};
    for(unsigned test=0;test<10;++test) {
        qc_context *c=qc_create(1,7);qc_allow_tables(c,0);int ok=c!=NULL,compare=test>=3;
        unsigned result_width=compare?1:test==0?4:2;
        char operation[4096],expr[3600],computation[3200],a[800],b[1200];
        for(unsigned id=0;id<3 && ok;++id) {
            snprintf(operation,sizeof operation,"{\"op\":\"qalloc\",\"id\":%u,\"width\":%u,\"signed\":false}",id,id==2?result_width:2);
            ok=qc_apply(c,operation,NULL);
            if(id<2) {snprintf(operation,sizeof operation,"{\"op\":\"qprepare\",\"id\":%u,\"lower\":0,\"upper\":3}",id);ok=ok && qc_apply(c,operation,NULL);}
        }
        snprintf(a,sizeof a,"{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false}]}]}");
        if(test==9) snprintf(a,sizeof a,"{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false}]}");
        if(test==1 || test==2)
            snprintf(b,sizeof b,"{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"bor\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":2,\"signed\":false}]},{\"op\":\"const\",\"bits\":2,\"signed\":true,\"value\":1}]}]}");
        else snprintf(b,sizeof b,"{\"op\":\"cast\",\"bits\":4,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":2,\"signed\":false}]}]}");
        snprintf(computation,sizeof computation,"{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[%s,%s]}",operators[test],compare?1:4,compare?"false":"true",a,b);
        if(compare) snprintf(expr,sizeof expr,"%s",computation);
        else snprintf(expr,sizeof expr,"{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":[%s]}",result_width,computation);
        snprintf(operation,sizeof operation,"{\"op\":\"qcompute\",\"id\":2,\"expr\":%s}",expr);ok=ok && qc_apply(c,operation,NULL);
        if(ok) for(size_t i=0;i<qc_state_size(c);++i) {
            int x=(int)(i&3),y=(int)((i>>2)&3);if(test!=9 && x>=2) x-=4;if(y>=2) y-=4;
            int expected=0;
            switch(test) {case 0:expected=x*y;break;case 1:expected=x/(y|1);break;case 2:expected=x%(y|1);break;case 3:expected=x==y;break;case 4:expected=x!=y;break;case 5:expected=x<y;break;case 6:expected=x<=y;break;case 7:case 9:expected=x>y;break;default:expected=x>=y;}
            unsigned mask=(1u<<result_width)-1,out=(unsigned)(i>>4)&mask;
            ok&=cabs(qc_amplitude(c,i)-(out==((unsigned)expected&mask) && !(i>>(4+result_width))?.25:0))<1e-10;
        }
        snprintf(operation,sizeof operation,"{\"op\":\"quncompute\",\"id\":2,\"expr\":%s}",expr);ok=ok && qc_apply(c,operation,NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":2}",NULL);
        if(!ok) fprintf(stderr,"Signed structured %s: %s\n",operators[test],qc_error(c));
        char name[160];snprintf(name,sizeof name,"signed structured %s%s: joint amplitudes, C rounding, inverse and zero garbage",operators[test],test==9?" with a positive leading bit":"");report(name,ok);qc_destroy(c);
    }
    for(unsigned tables=0;tables<2;++tables) {
        qc_context *c=qc_create(0,7);qc_allow_tables(c,tables);
        int ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":2,\"signed\":false}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":3}",NULL) && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":2,\"signed\":false}",NULL);
        const char *sequence="{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"sequence\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"div\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"reg\",\"bits\":2,\"signed\":false,\"id\":0},{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":0}]},{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":1}]}}";
        ok=ok && !qc_apply(c,sequence,NULL);
        report(tables?"finite sequence validates discarded arithmetic":"structured sequence validates discarded arithmetic",ok);qc_destroy(c);
        c=qc_create(0,7);qc_allow_tables(c,tables);
        ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":2,\"signed\":false}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":3}",NULL) && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":2,\"signed\":false}",NULL);
        const char *overflow="{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"div\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"cast\",\"bits\":2,\"signed\":true,\"args\":[{\"op\":\"reg\",\"bits\":2,\"signed\":false,\"id\":0}]},{\"op\":\"const\",\"bits\":2,\"signed\":true,\"value\":-1}]}]}}";
        ok=ok && !qc_apply(c,overflow,NULL);
        report(tables?"finite signed division rejects MIN/-1":"structured signed division rejects MIN/-1",ok);qc_destroy(c);
    }
}
static void scalable_cases(void) {
    signed_arithmetic_cases();
    const char *operators[]={"add","sub","mul","div","mod","eq","ne","lt","le","gt","ge"};
    for(unsigned test=0;test<11;++test) {
        unsigned width=2;int compare=test>=5;qc_context *c=qc_create(1,7);qc_allow_tables(c,0);
        char operation[2048],expr[1700];int ok=c!=NULL;
        for(unsigned id=0;id<3 && ok;++id) {
            snprintf(operation,sizeof operation,"{\"op\":\"qalloc\",\"id\":%u,\"width\":%u,\"signed\":false}",id,id==2 && compare?1:width);
            ok=qc_apply(c,operation,NULL);
            if(id<2) {snprintf(operation,sizeof operation,"{\"op\":\"qprepare\",\"id\":%u,\"lower\":0,\"upper\":3}",id);ok=ok && qc_apply(c,operation,NULL);}
        }
        char rhs[512];snprintf(rhs,sizeof rhs,"{\"op\":\"reg\",\"id\":1,\"bits\":2,\"signed\":false}");
        if(test==3 || test==4) snprintf(rhs,sizeof rhs,"{\"op\":\"bor\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":2,\"signed\":false},{\"op\":\"const\",\"value\":1,\"bits\":2,\"signed\":false}]}");
        snprintf(expr,sizeof expr,"{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false},%s]}",operators[test],compare?1:2,rhs);
        snprintf(operation,sizeof operation,"{\"op\":\"qcompute\",\"id\":2,\"expr\":%s}",expr);ok=ok && qc_apply(c,operation,NULL);
        if(ok) for(size_t i=0;i<qc_state_size(c);++i) {
            unsigned a=(unsigned)i&3,b=((unsigned)i>>2)&3,out=((unsigned)i>>4)&(compare?1:3),expected=0;
            switch(test) {case 0:expected=(a+b)&3;break;case 1:expected=(a-b)&3;break;case 2:expected=(a*b)&3;break;case 3:expected=a/(b|1);break;case 4:expected=a%(b|1);break;case 5:expected=a==b;break;case 6:expected=a!=b;break;case 7:expected=a<b;break;case 8:expected=a<=b;break;case 9:expected=a>b;break;default:expected=a>=b;}
            ok&=cabs(qc_amplitude(c,i)-(out==expected && !(i>>(compare?5:6))?.25:0))<1e-10;
        }
        snprintf(operation,sizeof operation,"{\"op\":\"quncompute\",\"id\":2,\"expr\":%s}",expr);ok=ok && qc_apply(c,operation,NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":2}",NULL);
        if(!ok) fprintf(stderr,"Structured %s: %s\n",operators[test],qc_error(c));
        char name[128];snprintf(name,sizeof name,"structured %s: joint amplitudes, inverse and zero garbage without tables",operators[test]);report(name,ok);qc_destroy(c);
    }
    const struct {const char *name,*body;int success;} frontend[]={
        {"16-bit multiplication exports with 32 relevant bits","quant uint16_t x=superposition(0,65535),y=superposition(0,65535);quant uint32_t z=(unsigned)x*(unsigned)y;measure(z);measure(x);measure(y);",1},
        {"16-bit division exports without input enumeration","quant uint16_t x=superposition(0,65535),y=superposition(0,65535);quant uint16_t z=x/(y|1);measure(z);measure(x);measure(y);",1},
        {"16-bit remainder exports without input enumeration","quant uint16_t x=superposition(0,65535),y=superposition(0,65535);quant uint16_t z=x%(y|1);measure(z);measure(x);measure(y);",1},
        {"32-bit register comparison exports","quant uint32_t x=superposition(0,4294967295u),y=superposition(0,4294967295u);quant bool z=x<y;measure(z);measure(x);measure(y);",1},
        {"signed 16-bit multiplication exports with negative operands","quant uint16_t a=superposition(0,65535),b=superposition(0,65535);quant int16_t x=(int16_t)a,y=(int16_t)b;quant int32_t z=x*y;measure(z);measure(x);measure(y);measure(a);measure(b);",1},
        {"signed division exports beyond twelve relevant bits","quant int16_t x=superposition(0,32767);quant int16_t z=x/-3;measure(z);measure(x);",1},
        {"signed remainder exports beyond twelve relevant bits","quant int16_t x=superposition(0,32767);quant int16_t z=x%-3;measure(z);measure(x);",1},
        {"large signed overflow remains rejected","quant uint16_t x=superposition(0,65535),y=superposition(0,65535);quant bool z=x*y>3;measure(z);measure(x);measure(y);",0},
        {"Grover oracle exceeds the former 12-bit limit","quant uint16_t x=superposition(0,65535);quant uint16_t z=find_known(x,x==42,1);measure(z);",1}
    };
    for(unsigned i=0;i<sizeof frontend/sizeof *frontend;++i) report(frontend[i].name,compile(frontend[i].body,"--emit-qasm",frontend[i].success));
    const struct {const char *name,*body;} hybrid[]={
        {"unknown search verifies its successful candidate","quant uint8_t x=superposition(0,15);bool found=search(x,x==3,80);int answer=measure(x);return !found || answer!=3;"},
        {"unknown search with no solutions exhausts its budget","quant uint8_t x=superposition(0,15);bool found=search(x,x==42,8);measure(x);return found;"},
        {"unknown search with all solutions succeeds","quant uint8_t x=superposition(0,15);bool found=search(x,x<16,8);measure(x);return !found;"},
        {"coherent if inside a CPU loop","quant bool c=superposition(0,1);quant uint8_t x=0;int n=3;while(n--){if(c)x+=1;else x+=2;}unsigned flag=measure(c);return measure(x)!=(flag?3:6);"},
        {"GCC boolean complement preserves a controlled XOR","quant bool a=superposition(0,1),b=false;if(a)b^=true;unsigned flag=measure(a);return measure(b)!=flag;"},
        {"a signed value conversion preserves its source owner","quant uint8_t x=superposition(0,255);quant int8_t y=(int8_t)x;int answer=measure(y);return (uint8_t)answer!=measure(x);"},
        {"CPU paths both consume a register explicitly","quant uint8_t x=7;int c=1;if(c)measure(x);else reset(x);"},
        {"measurement can drive a later CPU decision","quant uint8_t x=superposition(0,1),y=0;int m=measure(x);if(m)y+=2;else y+=3;return measure(y)!=(m?2:3);"}
    };
    for(unsigned i=0;i<sizeof hybrid/sizeof *hybrid;++i) report(hybrid[i].name,compile(hybrid[i].body,"--run",1));
    report("zero-iteration loop does not consume an owner",compile("quant uint8_t x=7;int n=0;while(n--)measure(x);","--emit-ir",0));
    report("one CPU branch does not consume an owner",compile("quant uint8_t x=7;int n=0;if(n)measure(x);","--emit-ir",0));
    report("early GCC narrowing cannot hide signed multiplication overflow",compile("quant uint16_t x=superposition(0,65535),y=superposition(0,65535);quant uint16_t z=x*y;measure(z);measure(x);measure(y);","--emit-ir",0));
    report("unknown search refuses static export",compile(hybrid[0].body,"--emit-qasm",0));
    report("unknown search compiles a 16-bit domain without counting",compile("quant uint16_t x=superposition(0,65535);bool found=search(x,x==42,80);measure(x);(void)found;","--emit-ir",1));
    report("conditional pure function returns compose",compile("#include <quantum.h>\nquant uint8_t choose(quant uint8_t x){if(x<2)return x+1;else return x-1;}\nint main(void){quant uint8_t x=3;quant uint8_t y=choose(x);int r=measure(y);measure(x);return r!=2;}","--run",1));
    report("function transfers ownership of a local result",compile("#include <quantum.h>\nquant uint8_t make(quant uint8_t x){quant uint8_t y=x+2;return y;}\nint main(void){quant uint8_t x=3;quant uint8_t y=make(x);int r=measure(y);measure(x);return r!=5;}","--run",1));
    report("function returns fresh owned basis storage",compile("#include <quantum.h>\nquant uint8_t make(void){quant uint8_t y=7;return y;}\nint main(void){quant uint8_t y=make();return measure(y)!=7;}","--run",1));
    char module_body[2*PATH_MAX+256];
    snprintf(module_body,sizeof module_body,"quant bool c=superposition(0,1);quant uint8_t x=7;if(c)circuit(\"%s/examples/increment.qir.json\",x);if(c)inverse_circuit(\"%s/examples/increment.qir.json\",x);measure(c);return measure(x)!=7;",root,root);
    report("frontend embeds and composes separate controlled inverse modules",compile(module_body,"--run",1));
    const char *module="{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"qborrow\",\"id\":0,\"width\":2,\"signed\":false},{\"op\":\"qadd_inplace\",\"id\":0,\"value\":1},{\"op\":\"qphase\",\"angle\":0.3}]}";
    char error[512];qc_circuit *function=qc_circuit_compile(module,error,sizeof error);qc_context *c=qc_create(1,7);
    int ok=function && c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":5,\"width\":2,\"signed\":false}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":5,\"lower\":0,\"upper\":3}",NULL) &&
        qc_apply(c,"{\"op\":\"qalloc\",\"id\":8,\"width\":1,\"signed\":false}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":8,\"lower\":0,\"upper\":1}",NULL);
    uint32_t bindings[]={5},control[]={8};unsigned values[]={0};
    ok=ok && qc_circuit_apply(c,function,bindings,1,0,control,values,1);
    if(ok) for(size_t i=0;i<qc_state_size(c);++i) {double complex expected=(i&4?1:cos(.3)+I*sin(.3))/sqrt(8);ok&=cabs(qc_amplitude(c,i)-expected)<1e-10;}
    ok=ok && qc_circuit_apply(c,function,bindings,1,1,control,values,1);
    if(ok) for(size_t i=0;i<qc_state_size(c);++i) ok&=cabs(qc_amplitude(c,i)-1/sqrt(8))<1e-10;
    report("separate circuit ABI remaps borrows, controls global phase and inverts",ok);qc_destroy(c);qc_circuit_destroy(function);
    function=qc_circuit_compile("{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"qborrow\",\"id\":0,\"width\":1,\"signed\":false},{\"op\":\"qmeasure\",\"id\":0}]}",error,sizeof error);
    report("unitary module contract rejects measurement",!function && strstr(error,"unitary"));qc_circuit_destroy(function);
    c=qc_create(1,7);qc_allow_tables(c,0);
    const char *compute="{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"mul\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false},{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":3}]}}";
    const char *uncompute="{\"op\":\"quncompute\",\"id\":1,\"expr\":{\"op\":\"mul\",\"bits\":2,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false},{\"op\":\"const\",\"bits\":2,\"signed\":false,\"value\":3}]}}";
    ok=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":2,\"signed\":false}",NULL) && qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":3}",NULL) && qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":2,\"signed\":false}",NULL);
    for(unsigned j=0;j<8 && ok;++j) ok=qc_apply(c,compute,NULL) && qc_apply(c,uncompute,NULL);
    char *resources=NULL;size_t size=0;FILE *stream=open_memstream(&resources,&size);if(stream) {qc_resources(c,stream);fclose(stream);}else ok=0;
    ok=ok && resources && strstr(resources,"JSON parses:   5\n") && strstr(resources,"Synthesis runs:1\n") && strstr(resources,"Plan hits:     15\n");
    report("repeated circuits parse once, synthesize once and reuse a verified plan",ok);free(resources);qc_destroy(c);
}
