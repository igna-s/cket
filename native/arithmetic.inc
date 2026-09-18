/* Register circuits, with compute/copy/uncompute. No input truth table.
 * A recorded forward plan makes the cleanup exact for arbitrary amplitudes. */
#define QC_ARITH_WORK 2000000u
typedef struct { qc_mask wire; int inverted; } ar_bit;
typedef struct { unsigned width; int sign; ar_bit bit[64]; } ar_value;
typedef struct { __int128 lo,hi; unsigned width; int sign,valid; } ar_range;
typedef struct {
    qc_context *c; const cj_doc *d; ar_range *ranges;
    qc_gate *gates; size_t count,capacity;
    uint32_t ids[512]; unsigned nids,work;
} ar_builder;
static ar_bit ar_at(const ar_value *v,unsigned b) {
    return b<v->width?v->bit[b]:v->sign?v->bit[v->width-1]:(ar_bit){0,0};
}
static __int128 ar_abs(__int128 value) {return value<0?-value:value;}
static int ar_nonzero(ar_builder *p,int e,const ar_range *r) {
    if(r->lo>0 || r->hi<0) return 1;
    for(unsigned b=0;b<r->width;++b) {
        bit_poly bit;qc_mask wire;int inverted;
        if(expr_bit(p->c,p->d,e,b,&bit,0) && poly_literal(&bit,&wire,&inverted) && !wire && inverted) return 1;
    }
    return 0;
}
static int ar_bound(ar_builder *p,int e,unsigned depth) {
    if(depth>128 || ++p->work>QC_ARITH_WORK) return 0;
    ar_range *r=&p->ranges[e];
    if(r->valid) return 1;
    uint64_t w; if(!cj_u64(p->d,cj_get(p->d,e,"bits"),&w)) return 0;
    r->width=(unsigned)w; r->sign=cj_bool(p->d,cj_get(p->d,e,"signed"));
    __int128 low=r->sign?-((__int128)1<<(w-1)):0;
    __int128 high=r->sign?((__int128)1<<(w-1))-1:(__int128)bits_mask((unsigned)w);
    int op=cj_get(p->d,e,"op"),nodes[3]; unsigned n=expr_nodes(p->d,e,nodes);
    if(cj_equal(p->d,op,"const")) {
        uint64_t v; cj_u64(p->d,cj_get(p->d,e,"value"),&v);
        r->lo=r->hi=numeric(v,(unsigned)w,r->sign);
    } else if(cj_equal(p->d,op,"reg")) {
        uint64_t id; cj_u64(p->d,cj_get(p->d,e,"id"),&id);
        qc_register *s=reg(p->c,(uint32_t)id);
        if(s->basis_known) r->lo=r->hi=numeric(s->basis_value,s->width,s->signed_type==1);
        else { r->lo=low; r->hi=high;
            if(!r->sign || (s->zero_mask&(UINT64_C(1)<<(s->width-1)))) {r->lo=0;r->hi=(__int128)(bits_mask(s->width)&~s->zero_mask);}
        }
    } else {
        if(!n) return 0;
        for(unsigned j=0;j<n;++j) if(!ar_bound(p,nodes[j],depth+1)) return 0;
        ar_range a=p->ranges[nodes[0]],b=n>1?p->ranges[nodes[1]]:a;
        if(cj_equal(p->d,op,"cast")) {
            r->lo=a.lo;r->hi=a.hi;
            if(w==1) {r->lo=0;r->hi=1;}
            else if(r->lo<low || r->hi>high) {r->lo=low;r->hi=high;}
        } else if(cj_equal(p->d,op,"sequence")) {r->lo=b.lo;r->hi=b.hi;
        } else if(cj_equal(p->d,op,"add") || cj_equal(p->d,op,"sub")) {
            int sub=cj_equal(p->d,op,"sub");r->lo=a.lo+(sub?-b.hi:b.lo);r->hi=a.hi+(sub?-b.lo:b.hi);
        } else if(cj_equal(p->d,op,"mul")) {
            if(r->sign) {
                /* Signed 64-bit endpoints multiply safely in signed 128 bits. */
                __int128 products[4]={a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
                r->lo=r->hi=products[0];
                for(unsigned j=1;j<4;++j) {if(products[j]<r->lo) r->lo=products[j];if(products[j]>r->hi) r->hi=products[j];}
            } else {
                /* Avoid overflowing the range analyzer itself at 64 bits. */
                unsigned __int128 product=(unsigned __int128)a.hi*(unsigned __int128)b.hi;
                if(product>(unsigned __int128)high) {r->lo=0;r->hi=high;}
                else {r->lo=a.lo*b.lo;r->hi=(__int128)product;}
            }
        } else if(cj_equal(p->d,op,"div") || cj_equal(p->d,op,"mod")) {
            if(!ar_nonzero(p,nodes[1],&b)) return 0;
            if(r->sign && a.lo<=low && b.lo<=-1 && b.hi>=-1) return 0;
            __int128 amax=ar_abs(a.lo)>ar_abs(a.hi)?ar_abs(a.lo):ar_abs(a.hi);
            __int128 bmax=ar_abs(b.lo)>ar_abs(b.hi)?ar_abs(b.lo):ar_abs(b.hi);
            __int128 bmin=b.lo>0?b.lo:b.hi<0?-b.hi:1;
            int divide=cj_equal(p->d,op,"div");
            __int128 magnitude=divide?amax/bmin:(amax<bmax-1?amax:bmax-1);
            int negative=divide?((a.lo<0 && b.hi>0) || (a.hi>0 && b.lo<0)):a.lo<0;
            int positive=divide?((a.lo<0 && b.lo<0) || (a.hi>0 && b.hi>0)):a.hi>0;
            r->lo=negative?-magnitude:0;r->hi=positive?magnitude:0;
            /* MIN/-1 was excluded above, so these clamps are proven safe. */
            if(r->lo<low) r->lo=low;if(r->hi>high) r->hi=high;
        } else if(cj_equal(p->d,op,"eq") || cj_equal(p->d,op,"ne") ||
                  cj_equal(p->d,op,"lt") || cj_equal(p->d,op,"le") ||
                  cj_equal(p->d,op,"gt") || cj_equal(p->d,op,"ge") || cj_equal(p->d,op,"not")) {
            r->lo=0;r->hi=1;
        } else if(cj_equal(p->d,op,"band") || cj_equal(p->d,op,"bor") || cj_equal(p->d,op,"bxor")) {
            r->lo=low;r->hi=high;
            if(a.lo>=0 && b.lo>=0) {
                r->lo=0;
                unsigned bits=0; unsigned __int128 top=(unsigned __int128)(a.hi>b.hi?a.hi:b.hi);
                while(top) {++bits;top>>=1;} r->hi=(__int128)bits_mask(bits);
                if(cj_equal(p->d,op,"band")) r->hi=a.hi<b.hi?a.hi:b.hi;
                if(cj_equal(p->d,op,"bor")) {if(a.lo==a.hi) r->lo=a.lo;if(b.lo==b.hi && b.lo>r->lo) r->lo=b.lo;}
            }
        } else return 0;
        if(r->lo<low || r->hi>high) {
            if(r->sign && !cj_equal(p->d,op,"cast")) return 0;
            r->lo=low;r->hi=high;
        }
    }
    r->valid=1;return 1;
}
static int ar_gate(ar_builder *p,unsigned target,qc_mask mask,qc_mask pattern) {
    if(++p->work>QC_ARITH_WORK) return qc_fail(p->c,"structured arithmetic work budget exceeded");
    if(p->count==p->capacity) {
        size_t size=p->capacity?p->capacity*2:128;
        qc_gate *g=realloc(p->gates,size*sizeof *g);
        if(!g) return qc_fail(p->c,"out of memory recording arithmetic plan");
        p->gates=g;p->capacity=size;
    }
    p->gates[p->count++]=(qc_gate){QC_X,target,mask,pattern,0};
    return qc_emit(p->c,QC_X,target,mask,pattern,0);
}
static int ar_condition(ar_builder *p,unsigned target,const ar_bit *bits,const int *wanted,unsigned n) {
    qc_mask mask=0,pattern=0;int possible=1;
    for(unsigned j=0;j<n;++j) constrain(bits[j].wire,bits[j].inverted,wanted[j],&mask,&pattern,&possible);
    return !possible || ar_gate(p,target,mask,pattern);
}
static int ar_temp(ar_builder *p,unsigned width,ar_value *v) {
    if(p->nids==512) return qc_fail(p->c,"arithmetic temporary budget exceeded");
    uint32_t id=UINT32_MAX;
    while(reg(p->c,id) && reg(p->c,id)->alive) --id;
    if(!allocate(p->c,id,width,1,0)) return 0;
    p->ids[p->nids++]=id;qc_register *r=reg(p->c,id);
    *v=(ar_value){.width=width};
    for(unsigned b=0;b<width;++b) v->bit[b].wire=QC_WIRE(r->wires[b]);
    return 1;
}
static unsigned ar_wire(ar_bit b) {
    return (uint64_t)b.wire?(unsigned)__builtin_ctzll((uint64_t)b.wire):64+(unsigned)__builtin_ctzll((uint64_t)(b.wire>>64));
}
static int ar_copy(ar_builder *p,ar_value *out,const ar_value *in) {
    int one=1;
    for(unsigned b=0;b<out->width;++b) {ar_bit bit=ar_at(in,b);if(!ar_condition(p,ar_wire(out->bit[b]),&bit,&one,1)) return 0;}
    return 1;
}
/* Controlled increment/decrement of a suffix; O(n) logical MCX. */
static int ar_power(ar_builder *p,ar_value *out,unsigned offset,const ar_bit *control,unsigned n,int subtract) {
    if(offset>=out->width) return 1;
    for(unsigned step=0;step<out->width-offset;++step) {
        unsigned b=subtract?offset+step:out->width-1-step;
        ar_bit bits[66];int wanted[66];unsigned count=0;
        for(unsigned j=0;j<n;++j) {bits[count]=control[j];wanted[count++]=1;}
        for(unsigned k=offset;k<b;++k) {bits[count]=out->bit[k];wanted[count++]=1;}
        if(!ar_condition(p,ar_wire(out->bit[b]),bits,wanted,count)) return 0;
    }
    return 1;
}
static int ar_add(ar_builder *p,ar_value *out,const ar_value *in,int subtract,ar_bit extra) {
    for(unsigned b=0;b<out->width;++b) {ar_bit controls[2]={ar_at(in,b),extra};if(!ar_power(p,out,b,controls,2,subtract)) return 0;}
    return 1;
}
static int ar_negate_if(ar_builder *p,ar_value *out,ar_bit condition) {
    int one=1;
    for(unsigned b=0;b<out->width;++b) if(!ar_condition(p,ar_wire(out->bit[b]),&condition,&one,1)) return 0;
    return ar_power(p,out,0,&condition,1,0);
}
static int ar_magnitude(ar_builder *p,ar_value *value,ar_bit *negative) {
    *negative=value->sign?value->bit[value->width-1]:(ar_bit){0,0};
    if(!value->sign) return 1;
    uint64_t constant=0;int literal=1;
    for(unsigned b=0;b<value->width;++b) {
        literal&=!value->bit[b].wire;
        if(value->bit[b].inverted) constant|=UINT64_C(1)<<b;
    }
    if(literal) {
        if(negative->inverted) constant=(~constant+1)&bits_mask(value->width);
        for(unsigned b=0;b<value->width;++b) value->bit[b]=(ar_bit){0,!!(constant&(UINT64_C(1)<<b))};
        value->sign=0;return 1;
    }
    ar_value magnitude;
    if(!ar_temp(p,value->width,&magnitude) || !ar_copy(p,&magnitude,value) || !ar_negate_if(p,&magnitude,*negative)) return 0;
    *value=magnitude;return 1;
}
/* Compare using a reversible XOR difference and disjoint lexical prefixes. */
static int ar_compare(ar_builder *p,const ar_value *a,const ar_value *b,unsigned width,int sign,int relation,ar_value *out) {
    ar_value diff;if(!ar_temp(p,width,&diff)) return 0;
    size_t begin=p->count;
    if(!ar_copy(p,&diff,a) || !ar_copy(p,&diff,b)) return 0;
    size_t copied=p->count;
    ar_bit bits[66];int wanted[66];unsigned count=0;
    for(unsigned j=0;j<width;++j) {bits[count]=diff.bit[j];wanted[count++]=0;}
    if(relation==0 || relation==1 || relation==3 || relation==5)
        if(!ar_condition(p,ar_wire(out->bit[0]),bits,wanted,count)) return 0;
    if(relation==1 && !ar_gate(p,ar_wire(out->bit[0]),0,0)) return 0;
    if(relation>=2) for(unsigned step=0;step<width;++step) {
        unsigned j=width-1-step;count=0;
        for(unsigned k=j+1;k<width;++k) {bits[count]=diff.bit[k];wanted[count++]=0;}
        bits[count]=ar_at(a,j);wanted[count++]=(relation>=4)^(sign && j==width-1);
        bits[count]=ar_at(b,j);wanted[count++]=(relation<4)^(sign && j==width-1);
        if(!ar_condition(p,ar_wire(out->bit[0]),bits,wanted,count)) return 0;
    }
    for(size_t j=copied;j>begin;) {qc_gate g=p->gates[--j];if(!ar_gate(p,g.target,g.mask,g.pattern)) return 0;}
    qc_register *scratch=reg(p->c,p->ids[--p->nids]);scratch->clean=1;
    return release(p->c,scratch);
}
static int ar_lower(ar_builder *p,int e,unsigned requested,ar_value *out) {
    ar_range range=p->ranges[e];unsigned width=range.width<requested?range.width:requested;
    if(range.lo>=0) {unsigned active=1;unsigned __int128 high=(unsigned __int128)range.hi;while(high>>active && active<64) ++active;if(width>active) width=active;}
    else {
        unsigned active=1;
        while(active<64 && (range.lo<-((__int128)1<<(active-1)) || range.hi>((__int128)1<<(active-1))-1)) ++active;
        if(width>active) width=active;
    }
    int op=cj_get(p->d,e,"op"),nodes[3];unsigned n=expr_nodes(p->d,e,nodes);
    if(cj_equal(p->d,op,"const") || cj_equal(p->d,op,"reg")) {
        *out=(ar_value){.width=width,.sign=range.sign && range.lo<0};
        uint64_t v=0,id=0;qc_register *r=NULL;
        if(cj_equal(p->d,op,"reg")) {cj_u64(p->d,cj_get(p->d,e,"id"),&id);r=reg(p->c,(uint32_t)id);if(r->basis_known) v=r->basis_value;}
        else cj_u64(p->d,cj_get(p->d,e,"value"),&v);
        for(unsigned j=0;j<width;++j) out->bit[j]=r && !r->basis_known && !(r->zero_mask&(UINT64_C(1)<<j))?(ar_bit){QC_WIRE(r->wires[j]),0}:(ar_bit){0,!!(v&(UINT64_C(1)<<j))};
        return 1;
    }
    if(cj_equal(p->d,op,"cast") && range.width!=1) {
        ar_value source;if(!ar_lower(p,nodes[0],requested,&source)) return 0;
        *out=(ar_value){.width=width,.sign=range.sign && range.lo<0};for(unsigned j=0;j<width;++j) out->bit[j]=ar_at(&source,j);return 1;
    }
    if(cj_equal(p->d,op,"sequence")) {ar_value discarded;if(!ar_lower(p,nodes[0],64,&discarded)) return 0;return ar_lower(p,nodes[1],requested,out);}
    if(cj_equal(p->d,op,"band") || cj_equal(p->d,op,"bor") || cj_equal(p->d,op,"bxor")) {
        ar_value literals={.width=width,.sign=range.sign && range.lo<0};int supported=1;
        for(unsigned j=0;j<width;++j) {bit_poly bit;if(!expr_bit(p->c,p->d,e,j,&bit,0) || !poly_literal(&bit,&literals.bit[j].wire,&literals.bit[j].inverted)) {supported=0;break;}}
        if(supported) {*out=literals;return 1;}
    }
    int comparison=cj_equal(p->d,op,"eq") || cj_equal(p->d,op,"ne") || cj_equal(p->d,op,"lt") || cj_equal(p->d,op,"le") || cj_equal(p->d,op,"gt") || cj_equal(p->d,op,"ge");
    int division=cj_equal(p->d,op,"div") || cj_equal(p->d,op,"mod");
    ar_value a,b;
    if(!ar_lower(p,nodes[0],comparison || division || range.width==1?64:width,&a)) return 0;
    if(n>1 && !ar_lower(p,nodes[1],comparison || division?64:width,&b)) return 0;
    if(!ar_temp(p,width,out)) return 0;
    out->sign=range.sign && range.lo<0;
    if(comparison) {
        int signed_order=p->ranges[nodes[0]].sign && (a.sign || b.sign);
        unsigned awidth=a.width+(signed_order && !a.sign),bwidth=b.width+(signed_order && !b.sign);
        unsigned common=awidth>bwidth?awidth:bwidth;
        int relation=cj_equal(p->d,op,"eq")?0:cj_equal(p->d,op,"ne")?1:cj_equal(p->d,op,"lt")?2:cj_equal(p->d,op,"le")?3:cj_equal(p->d,op,"gt")?4:5;
        return ar_compare(p,&a,&b,common,signed_order,relation,out);
    }
    if(cj_equal(p->d,op,"not") || (cj_equal(p->d,op,"cast") && range.width==1)) {
        ar_value zero={.width=1};return ar_compare(p,&a,&zero,a.width,a.sign,cj_equal(p->d,op,"not")?0:1,out);
    }
    if(division) {
        ar_bit a_negative,b_negative;
        if(!ar_magnitude(p,&a,&a_negative) || !ar_magnitude(p,&b,&b_negative)) return 0;
        unsigned nbits=a.width>b.width?a.width:b.width;
        if(nbits==64) return qc_fail(p->c,"divider needs a 65-bit work register; physical backend limit exceeded");
        ar_value remainder,quotient;if(!ar_temp(p,nbits+1,&remainder)) return 0;
        int reused=cj_equal(p->d,op,"div") && out->width==nbits;
        if(reused) quotient=*out;else if(!ar_temp(p,nbits,&quotient)) return 0;
        for(unsigned step=0;step<nbits;++step) {
            /* A wire permutation shifts a remainder whose top bit is zero. */
            ar_bit top=remainder.bit[nbits];for(unsigned j=nbits;j;--j) remainder.bit[j]=remainder.bit[j-1];remainder.bit[0]=top;
            ar_bit next=ar_at(&a,nbits-1-step);int one=1;
            if(!ar_condition(p,ar_wire(remainder.bit[0]),&next,&one,1)) return 0;
            ar_value flag={.width=1,.bit={quotient.bit[nbits-1-step]}};
            if(!ar_compare(p,&remainder,&b,nbits+1,0,5,&flag)) return 0;
            if(!ar_add(p,&remainder,&b,1,flag.bit[0])) return 0;
        }
        int divide=cj_equal(p->d,op,"div");
        if(!reused && !ar_copy(p,out,divide?&quotient:&remainder)) return 0;
        if(!ar_negate_if(p,out,a_negative)) return 0;
        return !divide || ar_negate_if(p,out,b_negative);
    }
    if(cj_equal(p->d,op,"add") || cj_equal(p->d,op,"sub"))
        return ar_copy(p,out,&a) && ar_add(p,out,&b,cj_equal(p->d,op,"sub"),(ar_bit){0,1});
    if(cj_equal(p->d,op,"mul")) {
        for(unsigned i=0;i<(a.sign?width:a.width) && i<width;++i) for(unsigned j=0;j<(b.sign?width:b.width) && i+j<width;++j) {
            ar_bit controls[2]={ar_at(&a,i),ar_at(&b,j)};if(!ar_power(p,out,i+j,controls,2,0)) return 0;
        }
        return 1;
    }
    if(cj_equal(p->d,op,"band") || cj_equal(p->d,op,"bor") || cj_equal(p->d,op,"bxor")) {
        for(unsigned j=0;j<width;++j) {
            ar_bit bits[2]={ar_at(&a,j),ar_at(&b,j)};int ones[2]={1,1};unsigned wire=ar_wire(out->bit[j]);
            if(!cj_equal(p->d,op,"band") && (!ar_condition(p,wire,bits,ones,1) || !ar_condition(p,wire,bits+1,ones,1))) return 0;
            if(!cj_equal(p->d,op,"bxor") && !ar_condition(p,wire,bits,ones,2)) return 0;
        }
        return 1;
    }
    return qc_fail(p->c,"unsupported structured arithmetic operator");
}
static int arithmetic_compute(qc_context *c,const cj_doc *d,int e,qc_register *target,qc_mask outer,qc_mask pattern) {
    if(d->count>100000) return qc_fail(c,"expression token budget exceeded"),-1;
    ar_builder p={.c=c,.d=d};p.ranges=calloc((size_t)d->count,sizeof *p.ranges);
    if(!p.ranges) return qc_fail(c,"out of memory analyzing arithmetic"),-1;
    if(!ar_bound(&p,e,0)) {free(p.ranges);return 0;}
    /* Allocation can relocate the register array: retain only the identity. */
    uint32_t id=target->id;unsigned width=target->width;ar_value value;
    int ok=ar_lower(&p,e,width,&value);
    if(ok) {
        target=reg(c,id);
        for(unsigned b=0;b<width && ok;++b) {
            ar_bit bit=ar_at(&value,b);qc_mask mask=outer,state=pattern;int possible=1;
            constrain(bit.wire,bit.inverted,1,&mask,&state,&possible);
            if(possible) ok=qc_emit(c,QC_X,target->wires[b],mask,state,0);
        }
    }
    if(ok) for(size_t j=p.count;j && ok;) {qc_gate g=p.gates[--j];ok=qc_emit(c,g.kind,g.target,g.mask,g.pattern,g.angle);}
    if(ok) for(unsigned j=p.nids;j && ok;) {qc_register *r=reg(c,p.ids[--j]);r->clean=1;ok=release(c,r);}
    free(p.ranges);free(p.gates);return ok?1:-1;
}
