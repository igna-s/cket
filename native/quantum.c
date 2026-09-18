#include "internal.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846264338327950288
static uint64_t bits_mask(unsigned bits) { return bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1; }
static unsigned pop(qc_mask v) { return (unsigned)(__builtin_popcountll((uint64_t)v)+__builtin_popcountll((uint64_t)(v>>64))); }
int qc_fail(qc_context *c,const char *fmt,...) {
    if (!c->error[0]) { va_list args; va_start(args,fmt); vsnprintf(c->error,sizeof c->error,fmt,args); va_end(args); }
    return 0;
}
const char *qc_error(const qc_context *c) { return c?c->error:"out of memory creating quantum context"; }
qc_context *qc_create(int simulate,uint64_t seed) {
    qc_context *c=calloc(1,sizeof *c); if (!c) return NULL;
    c->simulate=simulate; c->rng=seed; c->allow_tables=1;
    if (!qc_sim_grow(c,0)) { qc_destroy(c); return NULL; }
    return c;
}
void qc_allow_tables(qc_context *c,int allowed) { c->allow_tables=!!allowed; }
void qc_destroy(qc_context *c) {
    if (!c) return;
    for (size_t i=0;i<c->nregisters;++i) free(c->registers[i].definition);
    while(c->parsed) {qc_parsed_cache *p=c->parsed;c->parsed=p->next;cj_free(&p->doc);free(p->text);free(p);}
    while(c->plans) {qc_plan_cache *p=c->plans;c->plans=p->next;free(p->key);free(p->gates);free(p);}
    while(c->modules) {qc_module_cache *p=c->modules;c->modules=p->next;free(p->text);qc_circuit_destroy(p->circuit);free(p);}
    free(c->registers); free(c->gates); free(c->state); free(c);
}
unsigned qc_qubits(const qc_context *c) { return c->physical; }
size_t qc_state_size(const qc_context *c) { return c->dimension; }
double complex qc_amplitude(const qc_context *c,size_t index) { return index<c->dimension?c->state[index]:0; }
char *qc_read_file(const char *path) {
    FILE *f=fopen(path,"rb"); if (!f) return NULL;
    if (fseek(f,0,SEEK_END)) { fclose(f); return NULL; }
    long n=ftell(f); if (n<0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)) { fclose(f); return NULL; }
    char *s=malloc((size_t)n+1);
    if (!s) { fclose(f); return NULL; }
    if (fread(s,1,(size_t)n,f)!=(size_t)n) { free(s); fclose(f); return NULL; }
    s[n]=0; fclose(f); return s;
}
static qc_register *reg(qc_context *c,uint32_t id) {
    for (size_t i=0;i<c->nregisters;++i) if (c->registers[i].id==id) return &c->registers[i];
    return NULL;
}
qc_register *qc_lookup(qc_context *c,uint32_t id) { return reg(c,id); }
static void clear_definition(qc_register *r) {
    free(r->definition); r->definition=NULL; r->ndeps=0;
    r->definition_mask=r->definition_pattern=0;
}
static int unborrowed(qc_context *c,uint32_t id) {
    for (size_t i=0;i<c->nregisters;++i) {
        qc_register *r=&c->registers[i];
        if (!r->alive || !r->definition) continue;
        for (unsigned j=0;j<r->ndeps;++j) if (r->deps[j]==id)
            return qc_fail(c,"changing or releasing a live computation input/control would invalidate its inverse");
    }
    return 1;
}
static int remember_dependency(qc_context *c,qc_register *r,uint32_t id) {
    for (unsigned j=0;j<r->ndeps;++j) if (r->deps[j]==id) return 1;
    qc_register *s=reg(c,id);
    if (!s || !s->alive) return qc_fail(c,"cannot certify an unavailable computation dependency");
    if (r->ndeps==QC_MAX_SOURCES) return qc_fail(c,"too many inverse certificate dependencies");
    unsigned n=r->ndeps++; r->deps[n]=id; r->epochs[n]=s->epoch; return 1;
}
static void unknown_support(qc_register *r) {
    r->basis_known=r->uniform=0; r->zero_mask=0;
}
static void taint_uniform(qc_context *c,uint32_t id,qc_mask *seen) {
    qc_register *r=reg(c,id); if (!r || !r->alive) return;
    qc_mask key=QC_WIRE(r->wires[0]);
    if (*seen&key) return; *seen|=key; r->uniform=0;
    if (r->definition) for (unsigned j=0;j<r->ndeps;++j) taint_uniform(c,r->deps[j],seen);
}
static void taint_controls(qc_context *c,qc_mask mask) {
    qc_mask seen=0;
    for (size_t i=0;i<c->nregisters;++i) {
        qc_register *r=&c->registers[i]; if (!r->alive) continue;
        for (unsigned b=0;b<r->width;++b) if (mask&(QC_WIRE(r->wires[b]))) { taint_uniform(c,r->id,&seen); break; }
    }
}
static int number(qc_context *c,const cj_doc *d,int obj,const char *key,uint64_t *out) {
    return cj_u64(d,cj_get(d,obj,key),out) || qc_fail(c,"missing or invalid integer field: %s",key);
}
int qc_emit(qc_context *c,enum qc_kind kind,unsigned target,qc_mask mask,qc_mask pattern,double angle) {
    if ((kind==QC_X || kind==QC_H) && (mask&(QC_WIRE(target)))) return qc_fail(c,"a quantum control aliases its target");
    if (pattern&~mask) return qc_fail(c,"invalid quantum control pattern");
    qc_gate gate={kind,target,mask,pattern,angle};
    if (!c->simulate || c->recording) {
        if (c->ngates==c->gate_capacity) {
            size_t n=c->gate_capacity?c->gate_capacity*2:128;
            if (n>2097152) return qc_fail(c,"gate IR exceeds the experimental two-million-gate limit");
            qc_gate *g=realloc(c->gates,n*sizeof *g); if (!g) return qc_fail(c,"out of memory allocating gate IR");
            c->gates=g; c->gate_capacity=n;
        }
        c->gates[c->ngates++]=gate;
    }
    qc_mask used=mask;
    if (kind!=QC_PHASE) used|=QC_WIRE(target);
    unsigned depth=0;
    for (unsigned b=0;b<c->physical;++b) if (used&QC_WIRE(b)) if (c->depths[b]>depth) depth=c->depths[b];
    if (used) {
        ++depth; for (unsigned b=0;b<c->physical;++b) if (used&QC_WIRE(b)) c->depths[b]=depth;
        if (depth>c->depth) c->depth=depth;
    }
    if (kind==QC_H) ++c->h;
    else if (kind==QC_X) { unsigned n=pop(mask); if (!n) ++c->x; else if (n==1) ++c->cx; else if (n==2) ++c->ccx; else ++c->mcx; }
    else if (kind==QC_PHASE) { if (mask) ++c->phase; else ++c->global_phase; }
    else if (kind==QC_MEASURE) ++c->measure;
    else if (kind==QC_RESET) ++c->reset;
    qc_sim_gate(c,&gate); return 1;
}
static int allocate(qc_context *c,uint32_t id,unsigned width,int ancillary,int signed_type) {
    if (!width || width>64) return qc_fail(c,"quantum register width must be 1..64");
    qc_register *previous=reg(c,id);
    if (previous && previous->alive) return qc_fail(c,"quantum storage cannot be reinitialized implicitly");
    unsigned extra=width>c->nfree?width-c->nfree:0;
    if (extra>QC_MAX_PHYSICAL-c->physical) return qc_fail(c,"gate IR is limited to 128 physical qubits");
    if (!qc_sim_grow(c,c->physical+extra)) return 0;
    if (!previous && c->nregisters==c->register_capacity) {
        size_t n=c->register_capacity?c->register_capacity*2:16;
        qc_register *r=realloc(c->registers,n*sizeof *r); if (!r) return qc_fail(c,"out of memory allocating registers");
        c->registers=r; c->register_capacity=n;
    }
    qc_register *r=previous?previous:&c->registers[c->nregisters++];
    uint64_t epoch=previous?previous->epoch+1:0;
    memset(r,0,sizeof *r); r->epoch=epoch;
    r->id=id; r->width=width; r->alive=r->clean=1; r->ancillary=ancillary;
    r->signed_type=signed_type; r->basis_known=1; r->zero_mask=bits_mask(width);
    unsigned reused=width<c->nfree?width:c->nfree;
    for (unsigned b=0;b<reused;++b) r->wires[b]=c->free_wires[b];
    memmove(c->free_wires,c->free_wires+reused,(c->nfree-reused)*sizeof *c->free_wires); c->nfree-=reused;
    for (unsigned b=reused;b<width;++b) r->wires[b]=c->physical++;
    unsigned logical=0,ancillas=0;
    for (size_t i=0;i<c->nregisters;++i) if (c->registers[i].alive) {
        if (c->registers[i].ancillary) ancillas+=c->registers[i].width; else logical+=c->registers[i].width;
    }
    if (logical>c->logical_peak) c->logical_peak=logical;
    if (ancillas>c->ancillary_peak) c->ancillary_peak=ancillas;
    return 1;
}
static int release(qc_context *c,qc_register *r) {
    if (!unborrowed(c,r->id)) return 0;
    if (!r->clean) return qc_fail(c,"release would leave garbage: register is not proven |0>");
    if (!qc_sim_clean(c,r)) return 0;
    for (unsigned b=0;b<r->width;++b) c->free_wires[c->nfree++]=r->wires[b];
    r->alive=0; clear_definition(r); return 1;
}
typedef struct { uint32_t id; unsigned bits,active; int is_signed,known; uint64_t value; } source;
typedef struct { source sources[QC_MAX_SOURCES]; unsigned count,nwires,wires[QC_MAX_INPUT],owners[QC_MAX_INPUT],positions[QC_MAX_INPUT]; } domain;
static int leaves(qc_context *c,const cj_doc *d,int expr,domain *dom) {
    if (expr<0 || d->tokens[expr].kind!='{') return qc_fail(c,"invalid typed quantum expression");
    int op=cj_get(d,expr,"op");
    if (cj_equal(d,op,"reg")) {
        uint64_t id,bits,active,known=0;
        if (!number(c,d,expr,"id",&id) || id>UINT32_MAX || !number(c,d,expr,"bits",&bits) || !bits || bits>64) return qc_fail(c,"invalid quantum source type");
        if (dom->count==QC_MAX_SOURCES) return qc_fail(c,"too many quantum expression sources");
        active=bits; int a=cj_get(d,expr,"active"); if (a>=0 && !cj_u64(d,a,&active)) return qc_fail(c,"invalid active source width");
        if (active>bits) return qc_fail(c,"active source width exceeds its C type");
        int k=cj_get(d,expr,"known"); if (k>=0 && !cj_u64(d,k,&known)) return qc_fail(c,"invalid known basis value");
        for (unsigned i=0;i<dom->count;++i) if (dom->sources[i].id==id) {
            source *s=&dom->sources[i];
            if (s->bits!=bits || s->active!=active || s->is_signed!=cj_bool(d,cj_get(d,expr,"signed")) ||
                s->known!=(k>=0) || (s->known && s->value!=known))
                return qc_fail(c,"inconsistent type/range metadata for the same quantum source");
            return 1;
        }
        dom->sources[dom->count++]=(source){(uint32_t)id,(unsigned)bits,(unsigned)active,cj_bool(d,cj_get(d,expr,"signed")),k>=0,known};
    }
    int args=cj_get(d,expr,"args");
    if (args>=0) {
        if (d->tokens[args].kind!='[') return qc_fail(c,"invalid quantum expression operands");
        for (int a=args+1;a<d->tokens[args].next;a=d->tokens[a].next) if (!leaves(c,d,a,dom)) return 0;
    }
    return 1;
}
static int collect_domain(qc_context *c,const cj_doc *d,int expr,domain *dom) {
    memset(dom,0,sizeof *dom);
    if (!qc_verify_expression(c,d,expr) || !leaves(c,d,expr,dom)) return 0;
    for (unsigned i=0;i<dom->count;++i) for (unsigned j=i+1;j<dom->count;++j) if (dom->sources[j].id<dom->sources[i].id) {
        source s=dom->sources[i]; dom->sources[i]=dom->sources[j]; dom->sources[j]=s;
    }
    return 1;
}
static int make_domain(qc_context *c,const cj_doc *d,int expr,domain *dom) {
    if (!collect_domain(c,d,expr,dom)) return 0;
    for (unsigned i=0;i<dom->count;++i) {
        source *s=&dom->sources[i]; qc_register *r=reg(c,s->id);
        if (!r || !r->alive || r->width!=s->bits) return qc_fail(c,"use of unavailable or mistyped quantum storage");
        if (s->known) continue;
        for (unsigned b=0;b<s->active;++b) {
            if (dom->nwires==QC_MAX_INPUT) return qc_fail(c,"exact reversible synthesis supports at most %d input bits",QC_MAX_INPUT);
            unsigned n=dom->nwires++; dom->wires[n]=r->wires[b]; dom->owners[n]=i; dom->positions[n]=b;
        }
    }
    return 1;
}
static void values(const domain *dom,uint64_t index,uint64_t *v) {
    for (unsigned i=0;i<dom->count;++i) v[i]=dom->sources[i].known?dom->sources[i].value:0;
    for (unsigned i=0;i<dom->nwires;++i) v[dom->owners[i]]|=((index>>i)&1)<<dom->positions[i];
}
static __int128 numeric(uint64_t v,unsigned bits,int sign) {
    v&=bits_mask(bits);
    return sign && (v&(UINT64_C(1)<<(bits-1))) ? (__int128)v-((__int128)1<<bits) : (__int128)v;
}
/* Constant evaluation during reversible synthesis, never execution of C. */
static int evaluate(qc_context *c,const cj_doc *d,int e,const domain *dom,const uint64_t *vals,uint64_t *out) {
    uint64_t bits64; if (!number(c,d,e,"bits",&bits64) || bits64<1 || bits64>64) return qc_fail(c,"invalid expression integer width");
    unsigned bits=(unsigned)bits64; int sign=cj_bool(d,cj_get(d,e,"signed")),op=cj_get(d,e,"op");
    if (cj_equal(d,op,"const")) {
        if (!number(c,d,e,"value",out)) return 0;
        *out=bits==1?!!*out:*out&bits_mask(bits); return 1;
    }
    if (cj_equal(d,op,"reg")) {
        uint64_t id; if (!number(c,d,e,"id",&id)) return 0;
        for (unsigned i=0;i<dom->count;++i) if (dom->sources[i].id==id) { *out=vals[i]&bits_mask(bits); return 1; }
        return qc_fail(c,"missing quantum source during synthesis");
    }
    int args=cj_get(d,e,"args"),nodes[3],n=0;
    if (args<0 || d->tokens[args].kind!='[') return qc_fail(c,"missing expression operands");
    for (int a=args+1;a<d->tokens[args].next;a=d->tokens[a].next) { if (n==3) return qc_fail(c,"too many expression operands"); nodes[n++]=a; }
    if (!n) return qc_fail(c,"empty quantum expression");
    uint64_t av,bv=0; if (!evaluate(c,d,nodes[0],dom,vals,&av)) return 0;
    if(cj_equal(d,op,"sequence")) return evaluate(c,d,nodes[1],dom,vals,out);
    if (cj_equal(d,op,"select")) {
        if (n!=3) return qc_fail(c,"conditional expression requires three operands");
        if (!evaluate(c,d,nodes[av?1:2],dom,vals,out)) return 0;
        *out=bits==1?!!*out:*out&bits_mask(bits); return 1;
    }
    if (cj_equal(d,op,"land") && !av) { *out=0; return 1; }
    if (cj_equal(d,op,"lor") && av) { *out=1; return 1; }
    if (n>1 && !evaluate(c,d,nodes[1],dom,vals,&bv)) return 0;
    uint64_t abits,bbits=1;
    if (!number(c,d,nodes[0],"bits",&abits)) return 0;
    if (n>1 && !number(c,d,nodes[1],"bits",&bbits)) return 0;
    __int128 a=numeric(av,(unsigned)abits,cj_bool(d,cj_get(d,nodes[0],"signed")));
    __int128 b=numeric(bv,(unsigned)bbits,n>1 && cj_bool(d,cj_get(d,nodes[1],"signed")));
    __int128 r=0; int check=0;
    if (cj_equal(d,op,"cast")) r=a;
    else if (cj_equal(d,op,"add")) { r=sign?a+b:(__int128)((unsigned __int128)av+bv); check=1; }
    else if (cj_equal(d,op,"sub")) { r=sign?a-b:(__int128)((unsigned __int128)av-bv); check=1; }
    else if (cj_equal(d,op,"mul")) { r=sign?a*b:(__int128)((unsigned __int128)av*bv); check=1; }
    else if (cj_equal(d,op,"neg")) { r=-a; check=1; }
    else if (cj_equal(d,op,"div") || cj_equal(d,op,"mod")) {
        if (!b) return qc_fail(c,"C division/remainder by zero is undefined");
        if (sign && a==-((__int128)1<<(bits-1)) && b==-1) return qc_fail(c,"signed C division/remainder overflows");
        r=cj_equal(d,op,"div")?a/b:a%b; check=1;
    }
    else if (cj_equal(d,op,"band")) r=a&b;
    else if (cj_equal(d,op,"bor")) r=a|b;
    else if (cj_equal(d,op,"bxor")) r=a^b;
    else if (cj_equal(d,op,"bnot")) r=(~av)&bits_mask(bits);
    else if (cj_equal(d,op,"shl") || cj_equal(d,op,"shr")) {
        if (b<0 || b>=(__int128)abits) return qc_fail(c,"C shift count is out of range");
        if (cj_equal(d,op,"shl")) {
            if (sign && a<0) return qc_fail(c,"signed C left shift of a negative value is undefined");
            r=sign?a<<(unsigned)b:(__int128)((unsigned __int128)av<<(unsigned)b); check=1;
        } else r=a>>(unsigned)b;
    }
    else if (cj_equal(d,op,"eq")) r=a==b;
    else if (cj_equal(d,op,"ne")) r=a!=b;
    else if (cj_equal(d,op,"lt")) r=a<b;
    else if (cj_equal(d,op,"le")) r=a<=b;
    else if (cj_equal(d,op,"gt")) r=a>b;
    else if (cj_equal(d,op,"ge")) r=a>=b;
    else if (cj_equal(d,op,"not")) r=!a;
    else if (cj_equal(d,op,"land")) r=!!a && !!b;
    else if (cj_equal(d,op,"lor")) r=!!a || !!b;
    else return qc_fail(c,"unsupported operator in typed Quantum IR");
    if (check && sign) { __int128 limit=(__int128)1<<(bits-1); if (r < -limit || r>=limit) return qc_fail(c,"signed C arithmetic can overflow; use a representable expression or an unsigned C type"); }
    *out=bits==1?!!r:(uint64_t)r&bits_mask(bits); return 1;
}
static int controls(qc_context *c,const cj_doc *d,int inst,qc_mask *mask,qc_mask *pattern,int *impossible) {
    *mask=*pattern=0; *impossible=0; int list=cj_get(d,inst,"controls");
    if (list<0) return 1;
    if (d->tokens[list].kind!='[') return qc_fail(c,"invalid quantum control list");
    for (int t=list+1;t<d->tokens[list].next;t=d->tokens[t].next) {
        uint64_t id,v; if (!number(c,d,t,"id",&id) || id>UINT32_MAX || !number(c,d,t,"value",&v) || v>1) return qc_fail(c,"invalid quantum control");
        qc_register *r=reg(c,(uint32_t)id); if (!r || !r->alive || r->width!=1) return qc_fail(c,"control requires available quant bool storage");
        qc_mask bit=QC_WIRE(r->wires[0]);
        if ((*mask&bit) && !!(*pattern&bit)!=(int)v) return qc_fail(c,"contradictory coherent controls");
        *mask|=bit; if (v) *pattern|=bit;
    }
    return 1;
}
#include "structured.inc"
#include "arithmetic.inc"
static int synthesize_compute(qc_context *c,const cj_doc *d,int expr,qc_register *target,qc_mask outer,qc_mask state) {
    domain dom; if (!collect_domain(c,d,expr,&dom)) return 0;
    qc_mask inputs=0,outputs=0;
    for (unsigned j=0;j<dom.count;++j) {
        qc_register *s=reg(c,dom.sources[j].id);
        for (unsigned b=0;b<s->width;++b) inputs|=QC_WIRE(s->wires[b]);
    }
    for (unsigned b=0;b<target->width;++b) outputs|=QC_WIRE(target->wires[b]);
    if (outputs&(inputs|outer)) return qc_fail(c,"qcompute requires a separate clean output register");
    int structured=structured_compute(c,d,expr,target,outer,state);
    if (structured) return structured>0;
    unsigned relevant=0;
    for(unsigned j=0;j<dom.count;++j) if(!dom.sources[j].known) relevant+=dom.sources[j].active;
    if(relevant>QC_MAX_INPUT || !c->allow_tables) {
        structured=arithmetic_compute(c,d,expr,target,outer,state);
        if (structured) return structured>0;
        if(!c->allow_tables) return qc_fail(c,"expression has no structured synthesis; truth tables are disabled");
    }
    if (!make_domain(c,d,expr,&dom)) return 0;
    size_t n=(size_t)1<<dom.nwires;
    uint64_t *table=malloc(n*sizeof *table),*anf=malloc(n*sizeof *anf);
    if (!table || !anf) { free(table); free(anf); return qc_fail(c,"out of memory synthesizing reversible expression"); }
    uint64_t vals[QC_MAX_SOURCES]; int ok=1;
    for (size_t i=0;i<n;++i) { values(&dom,i,vals); if (!evaluate(c,d,expr,&dom,vals,&table[i])) { ok=0; break; } table[i]&=bits_mask(target->width); }
    if (ok) {
        memcpy(anf,table,n*sizeof *anf);
        for (unsigned b=0;b<dom.nwires;++b) for (size_t i=0;i<n;++i) if (i&((size_t)1<<b)) anf[i]^=anf[i^((size_t)1<<b)];
        size_t tc=0,ac=0; for (size_t i=0;i<n;++i) { tc+=pop(table[i]); ac+=pop(anf[i]); }
        int minterms=tc<ac; uint64_t *terms=minterms?table:anf;
        for (size_t i=0;i<n && ok;++i) if (terms[i]) {
            qc_mask local=0,pattern=0;
            for (unsigned b=0;b<dom.nwires;++b) if (minterms || (i&((size_t)1<<b))) {
                qc_mask bit=QC_WIRE(dom.wires[b]); local|=bit; if (!minterms || (i&((size_t)1<<b))) pattern|=bit;
            }
            if (((state^pattern)&outer&local)) continue;
            for (unsigned b=0;b<target->width && ok;++b) if (terms[i]&(UINT64_C(1)<<b)) ok=qc_emit(c,QC_X,target->wires[b],outer|local,state|pattern,0);
        }
    }
    free(table); free(anf); return ok;
}
/* Cache immutable plans; verify types/ranges on every call before lookup.
 * Physical work wires may only be reused when no live register owns them. */
static int compute(qc_context *c,const cj_doc *d,int expr,qc_register *target,qc_mask outer,qc_mask pattern) {
    domain dom;if(!collect_domain(c,d,expr,&dom)) return 0;
    char *text=cj_copy(d,expr);if(!text) return qc_fail(c,"out of memory creating circuit key");
    size_t length=strlen(text),capacity=length+((size_t)dom.count+1)*2048+64;
    char *key=malloc(capacity);if(!key) {free(text);return qc_fail(c,"out of memory creating circuit key");}
    memcpy(key,text,length);free(text);size_t used=length;
    for(unsigned j=0;j<=dom.count;++j) {
        qc_register *r=j==dom.count?target:reg(c,dom.sources[j].id);
        used+=(size_t)snprintf(key+used,capacity-used,"/%u:%u:%d:%d:%" PRIu64 ":%" PRIu64,r->id,r->width,r->signed_type,j==dom.count?0:r->basis_known,j==dom.count?0:r->basis_value,j==dom.count?0:r->zero_mask);
        for(unsigned b=0;b<r->width;++b) used+=(size_t)snprintf(key+used,capacity-used,":%u",r->wires[b]);
    }
    snprintf(key+used,capacity-used,"/%d",c->allow_tables);
    qc_mask occupied=0;
    for(size_t j=0;j<c->nregisters;++j) if(c->registers[j].alive) for(unsigned b=0;b<c->registers[j].width;++b) occupied|=QC_WIRE(c->registers[j].wires[b]);
    for(qc_plan_cache *p=c->plans;p;p=p->next) if(p->outer==outer && p->pattern==pattern && !(p->work&occupied) && !strcmp(p->key,key)) {
        free(key);++c->plan_hits;
        if(p->ancillary>c->ancillary_peak) c->ancillary_peak=p->ancillary;
        for(size_t j=0;j<p->count;++j) {qc_gate g=p->gates[j];if(!qc_emit(c,g.kind,g.target,g.mask,g.pattern,g.angle)) return 0;}
        return 1;
    }
    size_t begin=c->ngates;int previous=c->recording;c->recording=1;++c->synthesis_runs;
    int ok=synthesize_compute(c,d,expr,target,outer,pattern);c->recording=previous;
    size_t count=c->ngates-begin,bytes=sizeof(qc_plan_cache)+count*sizeof(qc_gate)+strlen(key)+1;
    if(ok && bytes<=16*1024*1024 && c->plan_bytes+bytes<=64*1024*1024) {
        qc_plan_cache *p=calloc(1,sizeof *p);
        if(p) {
            p->gates=count?malloc(count*sizeof *p->gates):NULL;
            if(!count || p->gates) {
                if(count) memcpy(p->gates,c->gates+begin,count*sizeof *p->gates);
                p->key=key;key=NULL;p->count=count;p->outer=outer;p->pattern=pattern;p->ancillary=c->ancillary_peak;
                for(size_t j=0;j<count;++j) {p->work|=p->gates[j].mask;if(p->gates[j].kind!=QC_PHASE) p->work|=QC_WIRE(p->gates[j].target);}
                p->work&=~occupied;p->next=c->plans;c->plans=p;c->plan_bytes+=bytes;
            } else free(p);
        }
    }
    if(c->simulate && !previous) c->ngates=begin;
    free(key);return ok;
}
static int synthesize_add_constant(qc_context *c,qc_register *r,uint64_t value,qc_mask outer,qc_mask state) {
    value&=bits_mask(r->width);
    for (unsigned offset=0;offset<r->width;++offset) if (value&(UINT64_C(1)<<offset)) {
        for (unsigned b=r->width-1;b>offset;--b) {
            qc_mask local=0; for (unsigned k=offset;k<b;++k) local|=QC_WIRE(r->wires[k]);
            if (!qc_emit(c,QC_X,r->wires[b],outer|local,state|local,0)) return 0;
        }
        if (!qc_emit(c,QC_X,r->wires[offset],outer,state,0)) return 0;
    }
    return 1;
}
static int add_constant(qc_context *c,qc_register *r,uint64_t value,qc_mask outer,qc_mask state) {
    char key[1024];size_t used=(size_t)snprintf(key,sizeof key,"@add:%u:%" PRIu64,r->width,value&bits_mask(r->width));
    for(unsigned b=0;b<r->width;++b) used+=(size_t)snprintf(key+used,sizeof key-used,":%u",r->wires[b]);
    for(qc_plan_cache *p=c->plans;p;p=p->next) if(p->outer==outer && p->pattern==state && !strcmp(p->key,key)) {
        ++c->plan_hits;for(size_t j=0;j<p->count;++j) {qc_gate g=p->gates[j];if(!qc_emit(c,g.kind,g.target,g.mask,g.pattern,g.angle)) return 0;}return 1;
    }
    size_t begin=c->ngates;int previous=c->recording;c->recording=1;++c->synthesis_runs;
    int ok=synthesize_add_constant(c,r,value,outer,state);c->recording=previous;
    size_t count=c->ngates-begin,bytes=sizeof(qc_plan_cache)+count*sizeof(qc_gate)+strlen(key)+1;
    if(ok && c->plan_bytes+bytes<=64*1024*1024) {
        qc_plan_cache *p=calloc(1,sizeof *p);
        if(p) {
            p->gates=count?malloc(count*sizeof *p->gates):NULL;p->key=malloc(strlen(key)+1);
            if(p->key && (!count || p->gates)) {
                memcpy(p->key,key,strlen(key)+1);if(count) memcpy(p->gates,c->gates+begin,count*sizeof *p->gates);
                p->count=count;p->outer=outer;p->pattern=state;p->next=c->plans;c->plans=p;c->plan_bytes+=bytes;
            } else {free(p->gates);free(p->key);free(p);}
        }
    }
    if(c->simulate && !previous) c->ngates=begin;return ok;
}
static int swap_edge(qc_context *c,qc_register *r,uint64_t left,uint64_t right,qc_mask outer,qc_mask state) {
    unsigned target=(unsigned)__builtin_ctzll(left^right); qc_mask mask=outer,pattern=state;
    for (unsigned b=0;b<r->width;++b) if (b!=target) { qc_mask bit=QC_WIRE(r->wires[b]); mask|=bit; if (left&(UINT64_C(1)<<b)) pattern|=bit; }
    return qc_emit(c,QC_X,r->wires[target],mask,pattern,0);
}
static int swap_basis(qc_context *c,qc_register *r,uint64_t a,uint64_t b,qc_mask outer,qc_mask state) {
    uint64_t path[65]; unsigned n=1; path[0]=a;
    for (unsigned bit=0;bit<r->width;++bit) if ((a^b)&(UINT64_C(1)<<bit)) { path[n]=path[n-1]^(UINT64_C(1)<<bit); ++n; }
    for (unsigned k=0;k+1<n;++k) if (!swap_edge(c,r,path[k],path[k+1],outer,state)) return 0;
    for (int k=(int)n-3;k>=0;--k) if (!swap_edge(c,r,path[k],path[k+1],outer,state)) return 0;
    return 1;
}
static int permutation(qc_context *c,const cj_doc *d,int expr,qc_register *r,qc_mask outer,qc_mask state) {
    if (r->width>QC_MAX_INPUT) return qc_fail(c,"in-place permutation synthesis exceeds the input-bit limit");
    domain dom={0}; if (!qc_verify_expression(c,d,expr) || !leaves(c,d,expr,&dom)) return 0;
    if (dom.count!=1 || dom.sources[0].id!=r->id) return qc_fail(c,"general reversible assignment currently requires its target as the only quantum source");
    size_t n=(size_t)1<<r->width; uint64_t *map=malloc(n*sizeof *map); unsigned char *seen=calloc(n,1);
    if (!map || !seen) { free(map); free(seen); return qc_fail(c,"out of memory synthesizing permutation"); }
    int ok=1; uint64_t vals[1];
    for (size_t i=0;i<n;++i) {
        vals[0]=i;
        if (!evaluate(c,d,expr,&dom,vals,&map[i])) { ok=0; break; }
        map[i]&=bits_mask(r->width);
        if (seen[map[i]]) { ok=qc_fail(c,"assignment is not injective: it erases quantum information; compute into a fresh quant variable"); break; }
        seen[map[i]]=1;
    }
    memset(seen,0,n);
    for (size_t start=0;start<n && ok;++start) if (!seen[start]) {
        seen[start]=1; uint64_t current=map[start];
        while (current!=start && ok) { seen[current]=1; ok=swap_basis(c,r,start,current,outer,state); current=map[current]; }
    }
    free(map); free(seen); return ok;
}
static int search_input(qc_context *c,const cj_doc *d,int inst,uint32_t id,unsigned *active) {
    uint64_t upper;if(!number(c,d,inst,"upper",&upper)) return 0;
    if(upper<1 || upper==UINT64_MAX || (upper&(upper+1))) return qc_fail(c,"find requires a nontrivial power-of-two uniform domain");
    qc_register *input=reg(c,id);
    if(!input || !input->uniform || input->uniform_upper!=upper || !unborrowed(c,id))
        return qc_fail(c,"find input is not an independently certified pristine uniform register");
    domain dom;if(!collect_domain(c,d,cj_get(d,inst,"predicate"),&dom)) return 0;
    if(dom.count!=1 || dom.sources[0].id!=id || dom.sources[0].known) return qc_fail(c,"find predicate must depend only on its uniform owned input");
    *active=64-(unsigned)__builtin_clzll(upper);return 1;
}
static int grover(qc_context *c,const cj_doc *d,int expr,uint32_t id,unsigned active,unsigned iterations) {
    uint32_t scratch=UINT32_MAX;while(reg(c,scratch) && reg(c,scratch)->alive) --scratch;
    if(!allocate(c,scratch,1,1,0)) return 0;
    for(unsigned k=0;k<iterations;++k) {
        if(!compute(c,d,expr,reg(c,scratch),0,0)) return 0;
        qc_mask flag=QC_WIRE(reg(c,scratch)->wires[0]);
        if(!qc_emit(c,QC_PHASE,0,flag,flag,PI) || !compute(c,d,expr,reg(c,scratch),0,0)) return 0;
        qc_register *r=reg(c,id);qc_mask mask=0;
        for(unsigned j=0;j<active;++j) {mask|=QC_WIRE(r->wires[j]);if(!qc_emit(c,QC_H,r->wires[j],0,0,0) || !qc_emit(c,QC_X,r->wires[j],0,0,0)) return 0;}
        if(!qc_emit(c,QC_PHASE,0,mask,mask,PI)) return 0;
        for(unsigned j=0;j<active;++j) if(!qc_emit(c,QC_X,r->wires[j],0,0,0) || !qc_emit(c,QC_H,r->wires[j],0,0,0)) return 0;
    }
    return release(c,reg(c,scratch));
}
static int find_value(qc_context *c,const cj_doc *d,int inst,uint32_t id) {
    unsigned active;if(!search_input(c,d,inst,id,&active)) return 0;
    uint64_t marked=1,N=UINT64_C(1)<<active;
    if(cj_get(d,inst,"solutions")>=0 && !number(c,d,inst,"solutions",&marked)) return 0;
    if(!marked || marked>N) return qc_fail(c,"solution count promise must be in 1..N; use search for unknown or zero counts");
    double theta=asin(sqrt((double)marked/(double)N)),estimate=fmax(0,PI/(4*theta)-.5);
    if(estimate>1000000) return qc_fail(c,"Grover iteration budget exceeded");
    unsigned a=(unsigned)floor(estimate),b=(unsigned)ceil(estimate);
    unsigned iterations=pow(sin((2*b+1)*theta),2)>pow(sin((2*a+1)*theta),2)?b:a;
    if(!grover(c,d,cj_get(d,inst,"predicate"),id,active,iterations)) return 0;
    qc_register *r=reg(c,id);++r->epoch;unknown_support(r);return 1;
}
static uint64_t search_random(qc_context *c) {
    uint64_t z=(c->rng+=UINT64_C(0x9e3779b97f4a7c15));
    z=(z^(z>>30))*UINT64_C(0xbf58476d1ce4e5b9);z=(z^(z>>27))*UINT64_C(0x94d049bb133111eb);return z^(z>>31);
}
static uint64_t search_below(qc_context *c,uint64_t limit) {
    uint64_t value,threshold=(0-limit)%limit;
    do value=search_random(c);while(value<threshold);
    return value%limit;
}
static int search_value(qc_context *c,const cj_doc *d,int inst,uint32_t id,uint64_t *result) {
    unsigned active;if(!search_input(c,d,inst,id,&active)) return 0;
    uint64_t attempts;if(!number(c,d,inst,"attempts",&attempts) || !attempts || attempts>4096) return qc_fail(c,"search attempt budget must be in 1..4096");
    if(!c->simulate) {
        if(!c->validation) return qc_fail(c,"search requires native CPU feedback and cannot be flattened to static QASM");
        /* Validate the oracle even though the classical strategy has no static circuit. */
        if(!grover(c,d,cj_get(d,inst,"predicate"),id,active,1)) return 0;
        qc_register *r=reg(c,id);unknown_support(r);++r->epoch;return 1;
    }
    uint64_t N=UINT64_C(1)<<active,K=(uint64_t)ceil(sqrt((double)N));
    if(K>1000000) return qc_fail(c,"search oracle query budget exceeded");
    for(uint64_t trial=0;trial<attempts;++trial) {
        if(trial) {
            qc_register *r=reg(c,id);
            for(unsigned j=0;j<r->width;++j) {
                if(qc_sim_measure(c,r->wires[j])) {qc_gate x={QC_X,r->wires[j],0,0,0};qc_sim_gate(c,&x);}
                if(!qc_emit(c,QC_RESET,r->wires[j],0,0,0)) return 0;
            }
            for(unsigned j=0;j<active;++j) if(!qc_emit(c,QC_H,r->wires[j],0,0,0)) return 0;
        }
        /* Half the trials sample directly; half choose a uniform Grover count. */
        unsigned iterations=search_random(c)&1?(unsigned)search_below(c,K):0;
        if(!grover(c,d,cj_get(d,inst,"predicate"),id,active,iterations)) return 0;
        qc_register *r=reg(c,id);uint64_t candidate=0;
        for(unsigned j=0;j<r->width;++j) {candidate|=(uint64_t)qc_sim_measure(c,r->wires[j])<<j;if(!qc_emit(c,QC_MEASURE,r->wires[j],0,0,0)) return 0;}
        domain dom;if(!collect_domain(c,d,cj_get(d,inst,"predicate"),&dom)) return 0;
        uint64_t values[QC_MAX_SOURCES]={candidate},verified;
        if(!evaluate(c,d,cj_get(d,inst,"predicate"),&dom,values,&verified)) return 0;
        if(verified) {*result=1;break;}
    }
    qc_register *r=reg(c,id);unknown_support(r);++r->epoch;return 1;
}
static int swap_wires(qc_context *c,unsigned a,unsigned b,qc_mask mask,qc_mask pattern) {
    qc_mask abit=QC_WIRE(a),bbit=QC_WIRE(b);
    return qc_emit(c,QC_X,a,mask|bbit,pattern|bbit,0) &&
           qc_emit(c,QC_X,b,mask|abit,pattern|abit,0) &&
           qc_emit(c,QC_X,a,mask|bbit,pattern|bbit,0);
}
static int fourier(qc_context *c,qc_register *r,int inverse,qc_mask mask,qc_mask pattern) {
    if (inverse) for (unsigned j=0;j<r->width/2;++j)
        if (!swap_wires(c,r->wires[j],r->wires[r->width-1-j],mask,pattern)) return 0;
    for (unsigned step=0;step<r->width;++step) {
        unsigned j=inverse?step:r->width-1-step;
        if (!inverse && !qc_emit(c,QC_H,r->wires[j],mask,pattern,0)) return 0;
        for (unsigned k=0;k<j;++k) {
            qc_mask both=(QC_WIRE(r->wires[j]))|(QC_WIRE(r->wires[k]));
            if (!qc_emit(c,QC_PHASE,0,mask|both,pattern|both,ldexp(inverse?-PI:PI,-(int)(j-k)))) return 0;
        }
        if (inverse && !qc_emit(c,QC_H,r->wires[j],mask,pattern,0)) return 0;
    }
    if (!inverse) for (unsigned j=0;j<r->width/2;++j)
        if (!swap_wires(c,r->wires[j],r->wires[r->width-1-j],mask,pattern)) return 0;
    return 1;
}
static int operation(qc_context *c,const char *text,int has_arg,uint64_t arg,uint64_t *result);
static int indexed_action(qc_context *c,const cj_doc *d,int inst,uint64_t id,int has_argument,uint64_t argument,uint64_t *result) {
    int action=cj_get(d,inst,"action"),target=cj_get(d,action,"id");
    size_t begin=d->tokens[action].begin,end=d->tokens[action].end;
    size_t before=d->tokens[target].begin-begin,after=end-d->tokens[target].end;
    if(end-begin>64*1024*1024) return qc_fail(c,"indexed action exceeds IR size limit");
    char *text=malloc(before+after+32); if(!text) return qc_fail(c,"out of memory selecting quantum storage");
    memcpy(text,d->text+begin,before);
    int digits=snprintf(text+before,32,"%" PRIu64,id);
    memcpy(text+before+(size_t)digits,d->text+d->tokens[target].end,after);
    text[before+(size_t)digits+after]=0;
    int ok=operation(c,text,has_argument,argument,result);free(text);return ok;
}
static int apply_module(qc_context *c,const cj_doc *d,int inst);
int qc_apply_token(qc_context *c,const cj_doc *d,int inst,int has_argument,uint64_t argument,uint64_t *result) {
    if (c->error[0]) return 0;
    if (!qc_verify_instruction(c,d,inst)) return 0;
    *result=0; int op=cj_get(d,inst,"op"); uint64_t id=0;
    if(cj_equal(d,op,"qmodule")) return !has_argument?apply_module(c,d,inst):qc_fail(c,"module arguments must be typed register borrows");
    if (cj_equal(d,op,"qindexed")) {
        if (has_argument || !c->validation) return qc_fail(c,"indexed quantum storage requires native CPU execution; static export cannot choose an index");
        int ids=cj_get(d,inst,"ids");
        for(int t=ids+1;t<d->tokens[ids].next;t=d->tokens[t].next) {
            uint64_t candidate; if(!cj_u64(d,t,&candidate) || !indexed_action(c,d,inst,candidate,0,0,result)) return 0;
            qc_register *storage=reg(c,(uint32_t)candidate);
            unknown_support(storage);storage->clean=0;
        }
        return 1;
    }
    if (has_argument==2 && !cj_equal(d,op,"qphase")) return qc_fail(c,"floating runtime argument requires a dynamic phase operation");
    if (has_argument==1 && !cj_equal(d,op,"qadd_inplace") && !cj_equal(d,op,"qxor_inplace") && !cj_equal(d,op,"qprepare"))
        return qc_fail(c,"integer runtime argument requires addition, XOR or preparation");
    if (cj_equal(d,op,"cloop_begin") || cj_equal(d,op,"cloop_end") || cj_equal(d,op,"ccontrol_begin") || cj_equal(d,op,"ccontrol_end"))
        return c->validation || qc_fail(c,"dynamic CPU control requires native C execution; flattened QASM would be incorrect");
    if (!cj_equal(d,op,"qphase") && (!number(c,d,inst,"id",&id) || id>UINT32_MAX)) return qc_fail(c,"invalid quantum storage identifier");
    if (cj_equal(d,op,"qalloc") || cj_equal(d,op,"qborrow")) {
        uint64_t width; if (!number(c,d,inst,"width",&width) || width>64) return qc_fail(c,"invalid quantum register width");
        int s=cj_get(d,inst,"signed");
        if(!allocate(c,(uint32_t)id,(unsigned)width,cj_bool(d,cj_get(d,inst,"ancilla")),s<0?-1:cj_bool(d,s))) return 0;
        if(cj_equal(d,op,"qborrow")) {qc_register *r=reg(c,(uint32_t)id);unknown_support(r);r->clean=0;r->prepared=1;}
        return 1;
    }
    qc_mask mask,pattern; int impossible;
    if (!controls(c,d,inst,&mask,&pattern,&impossible)) return 0;
    if (cj_equal(d,op,"qphase")) {
        int dynamic=cj_bool(d,cj_get(d,inst,"dynamic"));
        if (has_argument && (!dynamic || has_argument!=2)) return qc_fail(c,"invalid runtime phase argument kind");
        int angle=cj_get(d,inst,"angle"); double value=angle<0?PI:cj_double(d,angle);
        if (dynamic && !has_argument) {
            if (!c->validation) return qc_fail(c,"runtime phase requires native C execution; static export cannot determine its angle");
            taint_controls(c,mask); return 1;
        }
        if (has_argument==2) memcpy(&value,&argument,sizeof value);
        if (!isfinite(value)) return qc_fail(c,"nonfinite phase angle");
        if (impossible) return 1;
        taint_controls(c,mask);
        return qc_emit(c,QC_PHASE,0,mask,pattern,value);
    }
    qc_register *r=reg(c,(uint32_t)id); if (!r || !r->alive) return qc_fail(c,"use of unknown or released quantum storage");
    if (!cj_equal(d,op,"qmove") && !unborrowed(c,r->id)) return 0;
    if (cj_equal(d,op,"qxor_inplace")) {
        if (r->definition) return qc_fail(c,"updating a computed value invalidates its inverse");
        for (unsigned b=0;b<r->width;++b) if(mask&(QC_WIRE(r->wires[b])))
            return qc_fail(c,"XOR control aliases updated storage");
        int expr=cj_get(d,inst,"expr");
        if (has_argument && expr>=0) return qc_fail(c,"runtime XOR argument conflicts with its expression");
        if (expr>=0) {
            if(!impossible && !compute(c,d,expr,r,mask,pattern)) return 0;
            r=reg(c,(uint32_t)id);
            domain dom;if(!collect_domain(c,d,expr,&dom)) return 0;
            qc_mask seen=0;for(unsigned j=0;j<dom.count;++j) taint_uniform(c,dom.sources[j].id,&seen);
        } else {
            if(has_argument && cj_get(d,inst,"value")>=0) return qc_fail(c,"runtime XOR conflicts with static value");
            if(!has_argument && cj_bool(d,cj_get(d,inst,"dynamic"))) {
                if(!c->validation) return qc_fail(c,"runtime XOR operand requires native C execution");
            } else {
                if(!has_argument && !number(c,d,inst,"value",&argument)) return 0;
                if(!impossible) for(unsigned b=0;b<r->width;++b) if(argument&(UINT64_C(1)<<b))
                    if(!qc_emit(c,QC_X,r->wires[b],mask,pattern,0)) return 0;
            }
        }
        taint_controls(c,mask);unknown_support(r);r->clean=0;++r->epoch;return 1;
    }
    if (cj_equal(d,op,"qfourier")) {
        if (r->definition) return qc_fail(c,"basis transformation of a computed value invalidates its inverse");
        for (unsigned b=0;b<r->width;++b) if (mask&(QC_WIRE(r->wires[b])))
            return qc_fail(c,"Fourier control aliases transformed storage");
        if (!impossible && !fourier(c,r,cj_bool(d,cj_get(d,inst,"inverse")),mask,pattern)) return 0;
        taint_controls(c,mask); unknown_support(r); r->clean=0; r->prepared=1; ++r->epoch; return 1;
    }
    if (cj_equal(d,op,"qprepare")) {
        uint64_t low,high;
        if (!r->clean || r->prepared) return qc_fail(c,"preparation requires fresh clean storage");
        if(cj_bool(d,cj_get(d,inst,"dynamic"))) {
            if(!has_argument) {
                if(!c->validation) return qc_fail(c,"runtime basis preparation requires native C execution");
                unknown_support(r);r->prepared=1;r->clean=0;++r->epoch;return 1;
            }
            if(r->width==1 && argument>1) return qc_fail(c,"runtime bool preparation requires converted zero or one");
            low=high=argument&bits_mask(r->width);
        } else {
            if(has_argument) return qc_fail(c,"runtime preparation argument conflicts with static interval");
            if (!number(c,d,inst,"lower",&low) || !number(c,d,inst,"upper",&high)) return 0;
        }
        if (low>high || high>bits_mask(r->width)) return qc_fail(c,"preparation interval is outside the C integer register");
        if (low!=high && (low || (high!=UINT64_MAX && (high&(high+1))))) return qc_fail(c,"uniform preparation requires [0,2^k-1] or a single basis value");
        if (low!=high && r->signed_type==1 && high>=(UINT64_C(1)<<(r->width-1)))
            return qc_fail(c,"uniform interval is not nonnegative in its signed C type");
        for (unsigned b=0;b<r->width;++b) {
            if (low==high && (low&(UINT64_C(1)<<b))) { if (!qc_emit(c,QC_X,r->wires[b],0,0,0)) return 0; }
            else if (low!=high && (high&(UINT64_C(1)<<b))) { if (!qc_emit(c,QC_H,r->wires[b],0,0,0)) return 0; }
        }
        r->prepared=1; if (high) r->clean=0;
        r->basis_known=low==high; r->basis_value=low;
        r->zero_mask=bits_mask(r->width)&~high;
        r->uniform=low!=high; r->uniform_upper=high; ++r->epoch; return 1;
    }
    if (cj_equal(d,op,"qcompute") || cj_equal(d,op,"quncompute")) {
        int expr=cj_get(d,inst,"expr"); char *definition=cj_copy(d,expr);
        if (!definition) return qc_fail(c,"missing quantum computation expression");
        int inverse=cj_equal(d,op,"quncompute");
        if (inverse) {
            if (!r->definition || strcmp(definition,r->definition)) { free(definition); return qc_fail(c,"safe uncomputation cannot be proven: expression changed"); }
            if (mask!=r->definition_mask || pattern!=r->definition_pattern) {
                free(definition); return qc_fail(c,"safe uncomputation cannot be proven: controls changed");
            }
            for (unsigned j=0;j<r->ndeps;++j) { qc_register *s=reg(c,r->deps[j]); if (!s || !s->alive || s->epoch!=r->epochs[j]) { free(definition); return qc_fail(c,"safe uncomputation cannot be proven after changing or measuring dependencies"); } }
        } else if (!r->clean) { free(definition); return qc_fail(c,"computed quantum output requires clean storage"); }
        if (!compute(c,d,expr,r,mask,pattern)) { free(definition); return 0; }
        r=reg(c,(uint32_t)id);
        if (inverse) {
            free(definition); clear_definition(r); r->clean=r->basis_known=1;
            r->basis_value=0; r->zero_mask=bits_mask(r->width); r->uniform=0;
        }
        else {
            domain dom; if (!collect_domain(c,d,expr,&dom)) { free(definition); return 0; }
            clear_definition(r); r->definition=definition;
            r->definition_mask=mask; r->definition_pattern=pattern;
            for (unsigned j=0;j<dom.count;++j) if (!remember_dependency(c,r,dom.sources[j].id)) return 0;
            int list=cj_get(d,inst,"controls");
            if (list>=0) for (int t=list+1;t<d->tokens[list].next;t=d->tokens[t].next) {
                uint64_t control_id; if (!number(c,d,t,"id",&control_id) || !remember_dependency(c,r,(uint32_t)control_id)) return 0;
            }
            r->clean=0; r->prepared=1; unknown_support(r);
        }
        ++r->epoch;
        return 1;
    }
    if (cj_equal(d,op,"qrelease")) return release(c,r);
    if (cj_equal(d,op,"qadd_inplace") || cj_equal(d,op,"qadd_register") || cj_equal(d,op,"qinplace")) {
        if (has_argument && cj_get(d,inst,"value")>=0) return qc_fail(c,"runtime argument conflicts with a static addition value");
        if (r->definition) return qc_fail(c,"updating a computed value would invalidate its inverse; create a fresh result");
        for (unsigned b=0;b<r->width;++b) if (mask&(QC_WIRE(r->wires[b]))) return qc_fail(c,"updating quantum control storage is not reversible control");
        if (!impossible) {
            if (cj_equal(d,op,"qadd_register")) {
                uint64_t source_id; if (!number(c,d,inst,"source",&source_id)) return 0;
                qc_register *source=reg(c,(uint32_t)source_id);
                for (unsigned b=0;b<source->width && b<r->width;++b) {
                    qc_mask bit=QC_WIRE(source->wires[b]);
                    if ((mask&bit) && !(pattern&bit)) continue;
                    uint64_t amount=UINT64_C(1)<<b;
                    if (cj_bool(d,cj_get(d,inst,"subtract"))) amount=0-amount;
                    if (!add_constant(c,r,amount,mask|bit,pattern|bit)) return 0;
                }
                qc_mask seen=0; taint_uniform(c,source->id,&seen);
            } else if (cj_equal(d,op,"qadd_inplace")) {
                if (!has_argument) { int arg=cj_get(d,inst,"argument"); if (arg>=0) has_argument=cj_u64(d,arg,&argument); }
                if (cj_bool(d,cj_get(d,inst,"dynamic")) && !has_argument) { if (!c->validation) return qc_fail(c,"runtime classical operand requires native C execution"); }
                else { if (!has_argument && !number(c,d,inst,"value",&argument)) return 0; if (!add_constant(c,r,argument,mask,pattern)) return 0; }
            } else if (!permutation(c,d,cj_get(d,inst,"expr"),r,mask,pattern)) return 0;
        }
        taint_controls(c,mask);
        if (cj_equal(d,op,"qadd_inplace") && !mask && r->basis_known &&
            (has_argument || cj_get(d,inst,"value")>=0)) {
            r->basis_value=(r->basis_value+argument)&bits_mask(r->width);
            r->zero_mask=bits_mask(r->width)&~r->basis_value; r->uniform=0;
        } else unknown_support(r);
        ++r->epoch; r->clean=0; return 1;
    }
    if (cj_equal(d,op,"qreset")) {
        qc_mask seen=0; taint_uniform(c,r->id,&seen);
        for (unsigned b=0;b<r->width;++b) {
            int measured=qc_sim_measure(c,r->wires[b]);
            if (measured) { qc_gate xgate={QC_X,r->wires[b],0,0,0}; qc_sim_gate(c,&xgate); }
            if (!qc_emit(c,QC_RESET,r->wires[b],0,0,0)) return 0;
        }
        ++r->epoch; r->clean=r->basis_known=1; r->basis_value=0; r->zero_mask=bits_mask(r->width);
        clear_definition(r); return 1;
    }
    if (cj_equal(d,op,"qmeasure")) {
        if (!r->prepared) return qc_fail(c,"measurement requires prepared or computed storage");
        qc_mask seen=0; taint_uniform(c,r->id,&seen);
        for (unsigned b=0;b<r->width;++b) {
            if (!c->ignore_measurements) *result|=(uint64_t)qc_sim_measure(c,r->wires[b])<<b;
            if (!qc_emit(c,QC_MEASURE,r->wires[b],0,0,0)) return 0;
        }
        ++r->epoch; clear_definition(r); return 1;
    }
    if (cj_equal(d,op,"qfind")) return find_value(c,d,inst,(uint32_t)id);
    if (cj_equal(d,op,"qsearch")) return search_value(c,d,inst,(uint32_t)id,result);
    if (cj_equal(d,op,"qmove")) return 1;
    return qc_fail(c,"unsupported quantum instruction");
}
static const cj_doc *parsed_operation(qc_context *c,const char *text,cj_doc *local) {
    for(qc_parsed_cache *p=c->parsed;p;p=p->next) if(!strcmp(p->text,text)) return &p->doc;
    if(!cj_parse(local,text)) {qc_fail(c,"%s",local->error);return NULL;}
    ++c->json_parses;size_t size=sizeof(qc_parsed_cache)+strlen(text)+1+(size_t)local->capacity*sizeof(cj_token);
    if(size<=1024*1024 && c->parsed_bytes+size<=16*1024*1024) {
        qc_parsed_cache *p=calloc(1,sizeof *p);
        if(p) {
            p->text=malloc(strlen(text)+1);
            if(p->text) memcpy(p->text,text,strlen(text)+1);
            if(p->text) {p->doc=*local;memset(local,0,sizeof *local);p->doc.text=p->text;p->next=c->parsed;c->parsed=p;c->parsed_bytes+=size;return &p->doc;}
            free(p);
        }
    }
    return local;
}
static int operation(qc_context *c,const char *text,int has_arg,uint64_t arg,uint64_t *result) {
    cj_doc local={0};const cj_doc *d=parsed_operation(c,text,&local);
    int ok=d && qc_apply_token(c,d,0,has_arg,arg,result);cj_free(&local);return ok;
}
int qc_apply(qc_context *c,const char *text,uint64_t *result) { uint64_t ignored; return operation(c,text,0,0,result?result:&ignored); }
int qc_apply_arg(qc_context *c,const char *text,uint64_t arg) { uint64_t ignored; return operation(c,text,1,arg,&ignored); }
int qc_apply_phase(qc_context *c,const char *text,double angle) {
    uint64_t bits,ignored; _Static_assert(sizeof bits==sizeof angle,"phase runtime needs binary64 double");
    memcpy(&bits,&angle,sizeof bits); return operation(c,text,2,bits,&ignored);
}
int qc_apply_index(qc_context *c,const char *text,uint64_t index,int has_argument,uint64_t argument,uint64_t *result) {
    uint64_t ignored; if(!result) result=&ignored; *result=0;
    if(c->error[0]) return 0;
    cj_doc local={0};const cj_doc *d=parsed_operation(c,text,&local);if(!d) {cj_free(&local);return 0;}
    int ok=qc_verify_instruction(c,d,0),ids=cj_get(d,0,"ids");
    if(ok && (!cj_equal(d,cj_get(d,0,"op"),"qindexed") || has_argument<0 || has_argument>1))
        ok=qc_fail(c,"indexed runtime API requires an indexed instruction and an integer argument kind");
    if(ok && index>=(uint64_t)d->tokens[ids].children) ok=qc_fail(c,"quantum array index is outside borrowed storage");
    if(ok) {
        int token=ids+1;for(uint64_t i=0;i<index;++i) token=d->tokens[token].next;
        uint64_t id;ok=cj_u64(d,token,&id) && indexed_action(c,d,0,id,has_argument,argument,result);
    }
    cj_free(&local);return ok;
}
int qc_document(qc_context *c,const char *ir,int validation,int ignore_measurements) {
    cj_doc d; if (!cj_parse(&d,ir)) { qc_fail(c,"%s",d.error); cj_free(&d); return 0; }
    int instructions=cj_get(&d,0,"instructions"),ok=qc_verify_document(c,&d);
    c->validation=validation; c->ignore_measurements=ignore_measurements;
    if (ok) for (int t=instructions+1;t<d.tokens[instructions].next;t=d.tokens[t].next) { uint64_t result; if (!qc_apply_token(c,&d,t,0,0,&result)) { ok=0; break; } }
    cj_free(&d); return ok;
}
void qc_resources(const qc_context *c,FILE *f) {
    fprintf(f,"Logical qubits: %u\nAncillas:       %u\nPeak qubits:    %u\n",c->logical_peak,c->ancillary_peak,c->physical);
    const char *names[]={"H","X","CNOT","Toffoli","MCX","PHASE","GLOBAL_PHASE","MEASURE","RESET"};
    uint64_t counts[]={c->h,c->x,c->cx,c->ccx,c->mcx,c->phase,c->global_phase,c->measure,c->reset};
    for (unsigned i=0;i<sizeof counts/sizeof *counts;++i) if (counts[i]) fprintf(f,"%-15s%" PRIu64 "\n",names[i],counts[i]);
    fprintf(f,"Depth:         %u\nLogical gates before hardware decomposition, including open controls. Finite synthesis is exponential.\n",c->depth);
    fprintf(f,"JSON parses:   %" PRIu64 "\nSynthesis runs:%" PRIu64 "\nPlan hits:     %" PRIu64 "\n",c->json_parses,c->synthesis_runs,c->plan_hits);
}
int qc_qasm(const qc_context *c,FILE *f) {
    fprintf(f,"OPENQASM 3.0;\ninclude \"stdgates.inc\";\n");
    if (c->physical) fprintf(f,"qubit[%u] q;\n",c->physical);
    if (c->measure) fprintf(f,"bit[%" PRIu64 "] results;\n",c->measure);
    uint64_t measurement=0;
    for (size_t i=0;i<c->ngates;++i) {
        const qc_gate *g=&c->gates[i]; unsigned n=pop(g->mask);
        if (g->kind==QC_RESET) fprintf(f,"reset q[%u];\n",g->target);
        else if (g->kind==QC_MEASURE) fprintf(f,"results[%" PRIu64 "] = measure q[%u];\n",measurement++,g->target);
        else {
            for (unsigned b=0;b<c->physical;++b) if ((g->mask&QC_WIRE(b)) && !(g->pattern&QC_WIRE(b))) fprintf(f,"x q[%u];\n",b);
            if (g->kind==QC_X || g->kind==QC_H) {
                if (n) fprintf(f,"ctrl(%u) @ ",n);
                fputs(g->kind==QC_H?"h ":"x ",f);
                for (unsigned b=0;b<c->physical;++b) if (g->mask&QC_WIRE(b)) fprintf(f,"q[%u], ",b);
                fprintf(f,"q[%u];\n",g->target);
            } else if (!n) fprintf(f,"gphase(%.17g);\n",g->angle);
            else {
                if (n>1) fprintf(f,"ctrl(%u) @ ",n-1);
                fprintf(f,"p(%.17g) ",g->angle); unsigned used=0;
                for (unsigned b=0;b<c->physical;++b) if (g->mask&QC_WIRE(b)) { if (used++) fputs(", ",f); fprintf(f,"q[%u]",b); }
                fputs(";\n",f);
            }
            for (unsigned b=0;b<c->physical;++b) if ((g->mask&QC_WIRE(b)) && !(g->pattern&QC_WIRE(b))) fprintf(f,"x q[%u];\n",b);
        }
    }
    return !ferror(f);
}
#include "circuit.inc"
