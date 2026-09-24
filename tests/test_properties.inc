static uint32_t property_random(uint32_t *seed) {
    *seed=*seed*UINT32_C(1664525)+UINT32_C(1013904223);return *seed;
}
static void property_cases(void) {
    static const char *operators[]={"add","sub","mul","div","mod","band","bor","bxor","shl","shr","eq","lt"};
    uint32_t seed=42;int ok=1;unsigned cases=0,seen=0;
    for(unsigned test=0;test<1024;++test) {
        unsigned choice=property_random(&seed)%12,offset=property_random(&seed)%8,b=1+property_random(&seed)%7;
        if(choice==8 || choice==9) b=1+b%2;
        seen|=1u<<choice;
        char expression[1024],operation[1200];
        snprintf(expression,sizeof expression,
            "{\"op\":\"%s\",\"bits\":3,\"signed\":false,\"args\":["
            "{\"op\":\"add\",\"bits\":3,\"signed\":false,\"args\":["
            "{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false},"
            "{\"op\":\"const\",\"value\":%u,\"bits\":3,\"signed\":false}]},"
            "{\"op\":\"const\",\"value\":%u,\"bits\":3,\"signed\":false}]}",operators[choice],offset,b);
        qc_context *c=qc_create(1,7);
        int good=c && qc_apply(c,"{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}",NULL) &&
            qc_apply(c,"{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}",NULL) &&
            qc_apply(c,"{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}",NULL);
        snprintf(operation,sizeof operation,"{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}",expression);
        good=good && qc_apply(c,operation,NULL);
        if(good) for(size_t i=0;i<qc_state_size(c);++i) {
            unsigned x=(unsigned)i&7,y=(unsigned)(i>>3)&7,a=(x+offset)&7,value=0;
            switch(choice) {
                case 0:value=a+b;break;case 1:value=a-b;break;case 2:value=a*b;break;
                case 3:value=a/b;break;case 4:value=a%b;break;case 5:value=a&b;break;
                case 6:value=a|b;break;case 7:value=a^b;break;case 8:value=a<<b;break;
                case 9:value=a>>b;break;case 10:value=a==b;break;case 11:value=a<b;break;
            }
            good&=cabs(qc_amplitude(c,i)-(y==(value&7)?1/sqrt(8):0))<1e-10;
        }
        snprintf(operation,sizeof operation,"{\"op\":\"quncompute\",\"id\":1,\"expr\":%s}",expression);
        good=good && qc_apply(c,operation,NULL) && qc_apply(c,"{\"op\":\"qrelease\",\"id\":1}",NULL);
        if(good) for(size_t i=0;i<qc_state_size(c);++i) good&=cabs(qc_amplitude(c,i)-(i<8?1/sqrt(8):0))<1e-10;
        ok&=good;++cases;qc_destroy(c);
    }
    report("1024 deterministic typed expressions: every input, phase and inverse",ok && cases==1024 && seen==4095);
    const char *baseline="{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false},{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7},{\"op\":\"qphase\",\"angle\":0.5}]}";
    size_t size=strlen(baseline);unsigned rejected=0;ok=1;
    for(unsigned test=0;test<4096;++test) {
        char changed[512];memcpy(changed,baseline,size+1);
        unsigned mutations=1+property_random(&seed)%4;
        for(unsigned j=0;j<mutations;++j) changed[property_random(&seed)%size]=(char)(property_random(&seed)%128);
        qc_context *c=qc_create(0,7);
        if(!c) {ok=0;break;}
        int accepted=qc_document(c,changed,0,0);
        if(!accepted) {++rejected;ok&=*qc_error(c)!=0;}
        else ok&=*qc_error(c)==0;
        qc_destroy(c);
    }
    report("4096 seeded IR mutations: bounded parse/validation and explicit errors",ok && rejected>1000);
}
