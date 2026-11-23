#!/usr/bin/env python3
"""Validate the everyday C profile, IEEE-754 float/double, stdlib extensions, and integration programs."""

from __future__ import annotations

import argparse
import cmath
import hashlib
import json
import math
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

HEADER = (
    '#define _POSIX_C_SOURCE 200809L\n'
    '#include <inttypes.h>\n'
    '#include <limits.h>\n'
    '#include <locale.h>\n'
    '#include <quantum.h>\n'
    '#include <stdbool.h>\n'
    '#include <stddef.h>\n'
    '#include <stdint.h>\n'
    '#include <stdio.h>\n'
    '#include <stdlib.h>\n'
    '#include <string.h>\n'
    'void ownership_checkpoint(const char *tag);\n'
)

PRE_TRACE_CASES = {
    'phase_kickback': (
        'int main(void){'
        'quant bool k=superposition(0,1);'
        'if(k)phase(0.41);'
        'ownership_checkpoint("phased");'
        'measure(k);return 0;}'
    ),
    'shifted_superposition_phase': (
        'int main(void){'
        'quant uint8_t x=superposition(4,7);'
        'if(x==6)phase(0.5);'
        'ownership_checkpoint("shifted");'
        'measure(x);return 0;}'
    ),
    'struct_pointer_move': (
        'struct Pair{uint8_t a,b;};'
        'static void swap_pair(quant struct Pair *p){p->a^=p->b;p->b^=p->a;p->a^=p->b;}'
        'int main(void){'
        'quant bool c=superposition(0,1);'
        'quant struct Pair p={c?3:1,5};'
        'swap_pair(&p);'
        'quant struct Pair q=p;'
        'ownership_checkpoint("moved");'
        'measure(c);measure(q.a);measure(q.b);return 0;}'
    ),
    'grover_find_state': (
        'static quant bool sq9(quant const uint8_t x){return x*x==9;}'
        'int main(void){'
        'quant uint8_t x=superposition(0,15);'
        'quant uint8_t hit=find(x,sq9(x));'
        'ownership_checkpoint("found");'
        'measure(hit);return 0;}'
    ),
}

FULL_TRACE_CASES = {
    'b1_float_phase_kickback': (
        'int main(void){'
        'quant bool k=superposition(0,1);'
        'quant float f=k?1.5f:-1.5f;'
        'if(f>0.0f)phase(0.37);'
        'ownership_checkpoint("float_phase");'
        'measure(k);measure(f);return 0;}'
    ),
    'b1_float_add_state': (
        'int main(void){'
        'quant bool c=superposition(0,1);'
        'quant float a=c?1.25f:-0.5f;'
        'quant float s=a+0.5f;'
        'ownership_checkpoint("float_sum");'
        'measure(c);measure(a);measure(s);return 0;}'
    ),
    'b1_double_compare_phase': (
        'int main(void){'
        'quant bool c=superposition(0,1);'
        'if((c?2.5:-2.5)<0.0)phase(0.29);'
        'ownership_checkpoint("double_phase");'
        'measure(c);return 0;}'
    ),
    'a4_union_bitfield_state': (
        'union U{uint16_t w;uint8_t b[2];};'
        'struct BF{unsigned lo:4;unsigned hi:4;};'
        'int main(void){'
        'quant bool c=superposition(0,1);'
        'quant union U u={.w=c?0x0305u:0x0102u};'
        'quant struct BF bf={c?5u:2u,c?1u:0u};'
        'ownership_checkpoint("union_bf");'
        'measure(c);reset(u);reset(bf);return 0;}'
    ),
}

PRE_PROGRAMS = {
    'pre_shifted_div': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){quant uint8_t x=superposition(4,7);'
            'quant uint8_t q=28/x,r=28%x;unsigned vx=measure(x),vq=measure(q),vr=measure(r);'
            'return vx<4||vx>7||vq!=28/vx||vr!=28%vx;}\n'
        ),
        'static_qasm': True,
    },
    'pre_search_budget': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){quant uint8_t x=superposition(0,7);'
            'if(search(x,x>20,4))return 1;'
            'quant uint8_t y=superposition(8,15);'
            'if(!search(y,y==13,16))return 2;'
            'return measure(y)!=13;}\n'
        ),
        'static_qasm': False,
    },
    'pre_switch_continue': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){quant uint8_t m=superposition(0,1),acc=0;'
            'for(int i=0;i<3;++i){switch(m){case 0:if(i==1)continue;acc+=2;break;'
            'default:acc+=5;break;}}'
            'unsigned vm=measure(m),va=measure(acc);'
            'return va!=(vm?15u:4u);}\n'
        ),
        'static_qasm': True,
    },
}

FULL_PROGRAMS = {
    'a1_i64_div_mod': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){quant bool s=superposition(0,1);'
            'quant int64_t a=s?-1000000000000LL:1000000000007LL;'
            'quant int64_t d=superposition(1,4);'
            'quant int64_t q=a/d,r=a%d;'
            'int64_t va=measure(a),vd=measure(d),vq=measure(q),vr=measure(r);'
            'unsigned vs=measure(s);'
            'int64_t want_a=vs?-1000000000000LL:1000000000007LL;'
            'return va!=want_a||vd<1||vd>4||vq!=want_a/vd||vr!=want_a%vd;}\n'
        ),
        'static_qasm': True,
    },
    'a2_readonly_qborrow': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'static quant uint16_t leaf(quant const uint16_t *p){return p[0]+p[1];}'
            'static quant uint16_t fwd(quant const uint16_t *p){return leaf(p)+1u;}'
            'int main(void){quant bool b=superposition(0,1);'
            'quant uint16_t v[2]={b?11u:3u,7u};'
            'quant uint16_t r=fwd(v);'
            'unsigned vb=measure(b),vr=measure(r),v0=measure(v[0]),v1=measure(v[1]);'
            'return vr!=(vb?19u:11u)||v0!=(vb?11u:3u)||v1!=7u;}\n'
        ),
        'static_qasm': True,
    },
    'a3_const_owner_and_static_global': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'static quant uint16_t g_acc=0;'
            'static void bump(quant const uint16_t k){g_acc+=k;}'
            'int main(void){quant const uint16_t k=5;'
            'bump(k);bump(2);'
            'unsigned r=measure(g_acc);reset(g_acc);measure(k);'
            'return r!=7u;}\n'
        ),
        'static_qasm': True,
    },
    'a4_padded_struct_union_bf_ptr_vla': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n#include <string.h>\n'
            'struct Padded{uint8_t tag;uint32_t word;};'
            'struct WithPtr{quant uint8_t *slot;uint8_t bias;};'
            'struct Bits{unsigned lo:3;unsigned hi:5;};'
            'union Either{uint16_t whole;uint8_t bytes[2];};'
            'int main(void){int n=3;quant uint8_t vla[n];'
            'for(int i=0;i<n;++i)vla[i]=(uint8_t)(i+1);'
            'quant struct Padded p={4,0x11223344u},q={0,0};'
            'memcpy(&q,&p,sizeof p);'
            'quant struct WithPtr wp={&vla[1],5};'
            '*wp.slot+=wp.bias;'
            'quant struct Bits bf={3,2};'
            'quant union Either u={.whole=0x1234u};'
            'unsigned rq=measure(q.word),rt=measure(q.tag),rv=measure(vla[1]);'
            'unsigned rlo=measure(bf.lo),rhi=measure(bf.hi),ru=measure(u.bytes[0]);'
            'reset(p);reset(vla[0]);reset(vla[2]);reset(wp);reset(u);'
            'return rq!=0x11223344u||rt!=4u||rv!=7u||rlo!=3u||rhi!=2u||ru!=0x34u;}\n'
        ),
        'static_qasm': False,
    },
    'a5_ptr_reassign_cpu_write_byte_pun': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n#include <string.h>\n'
            'int main(void){quant uint16_t w[2]={0x0102u,0x0304u};'
            'quant bool c=superposition(0,1);'
            'quant uint16_t *p=&w[0];if(measure(c))p=&w[1];'
            'unsigned char *bp=(unsigned char *)p;bp[0]^=0xFFu;'
            'unsigned v0=measure(w[0]),v1=measure(w[1]);'
            'return (v0!=0x01FDu&&v1!=0x03FBu);}\n'
        ),
        'static_qasm': False,
    },
    'a6_heap_bool_struct_aligned_alloc': {
        'src': (
            '#include <quantum.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n'
            'struct Item{uint8_t a,b;};'
            'int main(void){size_t n=2;'
            'quant bool *flags=calloc(n,sizeof *flags);'
            'quant struct Item *items=calloc(n,sizeof *items);'
            'quant uint32_t *aligned=aligned_alloc(16,4*sizeof *aligned);'
            'if(!flags||!items||!aligned)return 2;'
            'quant bool coin=superposition(0,1);'
            'flags[0]=coin;items[1].a=coin?9:4;aligned[2]=coin?100u:25u;'
            'quant uint8_t len=1;quant char ha[3]={97,0,0},hb[3]={97,0,0};'
            'int eq=measure(memcmp(ha,hb,len)==0);'
            'unsigned vf=measure(flags[0]),vi=measure(items[1].a),va=measure(aligned[2]),vc=measure(coin);'
            'reset(flags[1]);reset(items[0]);reset(items[1].b);'
            'reset(aligned[0]);reset(aligned[1]);reset(aligned[3]);'
            'reset(ha);reset(hb);reset(len);'
            'free((void *)flags);free((void *)items);free((void *)aligned);'
            'return !eq||vf!=vc||vi!=(vc?9u:4u)||va!=(vc?100u:25u);}\n'
        ),
        'static_qasm': False,
    },
    'a7_switch_continue_local_alloc': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){quant uint8_t sel=superposition(0,1),acc=0;'
            'for(int i=0;i<3;++i){'
            'if(sel){quant uint8_t tmp=(uint8_t)(i+1);acc+=tmp;}'
            'switch(sel){case 0:continue;case 1:acc+=1;break;default:break;}}'
            'unsigned vs=measure(sel),va=measure(acc);'
            'return va!=(vs?9u:0u);}\n'
        ),
        'static_qasm': True,
    },
    'a9_owned_subexpr': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'static quant uint16_t dbl(quant const uint16_t x){return (uint16_t)(x*2u);}'
            'static quant uint16_t mix(quant const uint16_t a,quant const uint16_t b){return (uint16_t)(a+b);}'
            'int main(void){quant uint16_t x=superposition(0,3),y=5;'
            'quant uint16_t res=mix(dbl(x),dbl(y)+1u);'
            'unsigned vx=measure(x),vr=measure(res);measure(y);'
            'return vr!=(2u*vx+11u);}\n'
        ),
        'static_qasm': True,
    },
    'a10_indirect_qfn_ptr': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'static quant uint8_t add1(quant const uint8_t x){return (uint8_t)(x+1u);}'
            'static quant uint8_t add3(quant const uint8_t x){return (uint8_t)(x+3u);}'
            'int main(void){typedef quant uint8_t (*qfn_t)(quant const uint8_t);'
            'quant uint8_t x=superposition(0,3);'
            'qfn_t fn=(measure(superposition(0,1))?add3:add1);'
            'quant uint8_t r=fn(x);'
            'unsigned vx=measure(x),vr=measure(r);'
            'return vr!=((fn==add3)?vx+3u:vx+1u);}\n'
        ),
        'static_qasm': False,
    },
    'a11_bounded_recursion_variadic': {
        'src': (
            '#include <quantum.h>\n#include <stdarg.h>\n#include <stdint.h>\n'
            'static quant uint16_t rec_sum(quant const uint16_t x,unsigned n){'
            'if(n==0u)return 0u;'
            'return (uint16_t)(x+rec_sum(x,n-1u));}'
            'static quant uint16_t sum_var(unsigned count,...){'
            'va_list ap;va_start(ap,count);quant uint16_t acc=0;'
            'for(unsigned i=0;i<count;++i)acc+=(uint16_t)va_arg(ap,unsigned);'
            'va_end(ap);return acc;}'
            'int main(void){quant uint16_t x=superposition(0,3);'
            'quant uint16_t r=rec_sum(x,3u)+sum_var(2u,4u,6u);'
            'unsigned vx=measure(x),vr=measure(r);'
            'return vr!=(3u*vx+10u);}\n'
        ),
        'static_qasm': True,
    },
    'a12_uselocale_shifted_grover_runtime_search': {
        'src': (
            '#define _POSIX_C_SOURCE 200809L\n'
            '#include <ctype.h>\n#include <locale.h>\n#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){locale_t loc=newlocale(LC_CTYPE_MASK,"C",(locale_t)0);'
            'if(!loc)return 2;locale_t old=uselocale(loc);'
            'quant unsigned char ch=superposition(48,55);'
            'quant bool isd=isdigit(ch)!=0;'
            'uselocale(old);freelocale(loc);'
            'unsigned vc=measure(ch),vd=measure(isd);'
            'quant uint8_t s=superposition(12,15);'
            'quant uint8_t found=find(s,s==13);'
            'unsigned vf=measure(found);'
            'unsigned tries=12;'
            'quant uint8_t z=superposition(0,7);'
            'int ok=search(z,z==5,tries);'
            'unsigned vz=measure(z);'
            'return vd!=1u||vc<48u||vc>55u||vf!=13u||!ok||vz!=5u;}\n'
        ),
        'static_qasm': False,
    },
    'b1_float_double_ops': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n#include <string.h>\n'
            'static uint32_t f32(float v){uint32_t b=0;memcpy(&b,&v,sizeof b);return b;}'
            'static uint64_t f64(double v){uint64_t b=0;memcpy(&b,&v,sizeof b);return b;}'
            'int main(void){quant bool coin=superposition(0,1);'
            'quant float a=coin?1.5f:-0.0f,b=2.25f;quant float fs=a-b;'
            'quant double da=coin?3.5:-1.25,db=(double)a;quant double ds=da/2.0+db;'
            'quant bool kick=superposition(0,1);'
            'if((kick?1.0:-1.0)>0.0)phase(3.14159265358979323846);'
            'fourier(kick);unsigned ik=measure(kick);'
            'float gfs=measure(fs);double gds=measure(ds);unsigned gc=measure(coin);'
            'reset(a);reset(b);reset(da);reset(db);'
            'float wfs=(gc?1.5f:-0.0f)-2.25f;'
            'double wds=(gc?3.5:-1.25)/2.0+(double)(gc?1.5f:-0.0f);'
            'return ik!=1||f32(gfs)!=f32(wfs)||f64(gds)!=f64(wds);}\n'
        ),
        'static_qasm': True,
    },
    'cfg_while_q_and_forward_goto': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n'
            'int main(void){int rc=1;'
            'quant uint8_t x=superposition(0,3),acc=0;'
            'unsigned step=0;while(step<4u&&acc<4u){if(x)acc+=2u;++step;}'
            'quant uint8_t guard=9;'
            'unsigned vx=measure(x),va=measure(acc);'
            'if(vx>3u)goto cleanup;'
            'rc=(va!=(vx?4u:0u));'
            'cleanup:'
            'if(measure(guard)!=9u)rc=1;'
            'return rc;}\n'
        ),
        'static_qasm': False,
    },
    'stdlib_strto_variants': {
        'src': (
            '#include <inttypes.h>\n#include <quantum.h>\n#include <stdlib.h>\n'
            'int main(void){quant bool c=superposition(0,1);'
            'quant char s[5]={c?\'0\':\'1\',c?\'x\':\'7\',\'f\',0,0};'
            'char *end=0;'
            'quant unsigned long ul=strtoul((const char *)s,&end,0);'
            'quant unsigned long long ull=strtoull((const char *)s,0,0);'
            'quant intmax_t im=strtoimax((const char *)s,0,0);'
            'quant uintmax_t um=strtoumax((const char *)s,0,0);'
            'unsigned vc=measure(c),off=(unsigned)measure(end-s);'
            'unsigned long gul=measure(ul);unsigned long long gull=measure(ull);'
            'intmax_t gim=measure(im);uintmax_t gum=measure(um);'
            'reset(s);'
            'unsigned long w=vc?0xfUL:17UL;'
            'return gul!=w||gull!=w||(uintmax_t)gim!=w||gum!=w||off!=(vc?3u:2u);}\n'
        ),
        'static_qasm': False,
    },
    'stdlib_strtok_coll_xfrm_err': {
        'src': (
            '#include <errno.h>\n#include <quantum.h>\n#include <string.h>\n'
            'int main(void){quant bool c=superposition(0,1);'
            'quant char s[4]={\'a\',\',\',c?\'b\':\'c\',0};'
            'char *tok=strtok((char *)s,",");'
            'char *tok2=strtok((char *)0,",");'
            'quant char a[2]={c?\'b\':\'a\',0},b[2]={\'b\',0};'
            'quant int cmp=strcoll((const char *)a,(const char *)b);'
            'quant char xbuf[4]={0};size_t xlen=strxfrm((char *)xbuf,(const char *)a,sizeof xbuf);'
            'char *err=strerror(EINVAL);'
            'unsigned vc=measure(c),t0=(unsigned)measure(tok[0]),t1=(unsigned)measure(tok2[0]);'
            'int gcmp=measure(cmp);unsigned x0=(unsigned)measure(xbuf[0]);'
            'reset(s);reset(a);reset(b);reset(xbuf);'
            'return !err||!xlen||t0!=\'a\'||t1!=(vc?\'b\':\'c\')||x0!=(vc?\'b\':\'a\')||'
            '(vc?(gcmp!=0):(gcmp>=0));}\n'
        ),
        'static_qasm': False,
    },
    'stdlib_qsort_bsearch': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n#include <stdlib.h>\n'
            'static int cmp_u8(const void *pa,const void *pb){'
            'uint8_t a=*(const uint8_t *)pa,b=*(const uint8_t *)pb;'
            'return (a>b)-(a<b);}'
            'int main(void){quant bool c=superposition(0,1);'
            'quant uint8_t arr[4]={c?9:3,1,c?2:7,4};'
            'qsort((void *)arr,4,sizeof arr[0],cmp_u8);'
            'quant uint8_t key=4;'
            'quant uint8_t *hit=(quant uint8_t *)bsearch((const void *)&key,(const void *)arr,4,sizeof arr[0],cmp_u8);'
            'unsigned vc=measure(c),r0=measure(arr[0]),r1=measure(arr[1]),r2=measure(arr[2]),r3=measure(arr[3]);'
            'unsigned rh=hit?(unsigned)measure(*hit):0u;reset(key);'
            'return vc?(r0!=1||r1!=2||r2!=4||r3!=9||rh!=4):'
            '(r0!=1||r1!=3||r2!=4||r3!=7||rh!=4);}\n'
        ),
        'static_qasm': False,
    },
    'stdlib_snprintf_sscanf': {
        'src': (
            '#include <quantum.h>\n#include <stdint.h>\n#include <stdio.h>\n'
            'int main(void){quant uint8_t x=superposition(0,3);'
            'quant char buf[8]={0};'
            'quant int n=snprintf((char *)buf,sizeof buf,"v=%u",(unsigned)x);'
            'quant unsigned out=0;'
            'quant int sc=sscanf((const char *)buf,"v=%u",&out);'
            'unsigned vx=measure(x),vo=measure(out);int vn=measure(n),vsc=measure(sc);'
            'reset(buf);'
            'return vn!=3||vsc!=1||vo!=vx;}\n'
        ),
        'static_qasm': False,
    },
}

PRE_NEGATIVE = {
    'const_borrow_reset': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'void bad(quant const uint8_t *p){reset(*p);}\n'
        'int main(void){quant uint8_t x=7;bad(&x);measure(x);}\n',
        ('mutable quantum borrow', 'read-only'),
    ),
    'implicit_q2c': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant uint8_t x=1;int y=x;measure(x);return y;}\n',
        ('cannot become a cpu value', 'measure explicitly'),
    ),
    'div_superposed_zero': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant uint8_t x=superposition(0,1);quant uint8_t q=4/x;measure(q);measure(x);}\n',
        ('zero', 'divisor'),
    ),
    'uncleaned_local': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant uint8_t x=superposition(0,1);return 0;}\n',
        ('measured/reset', 'uncomputed'),
    ),
    'strtol_bad_radix': (
        '#include <quantum.h>\n#include <stdlib.h>\n'
        'int main(void){quant char s[3]={\'1\',\'2\',0};quant long v=strtol((const char *)s,0,1);measure(v);reset(s);}\n',
        ('radix',),
    ),
    'search_zero_attempts': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant uint8_t x=superposition(0,3);search(x,x==1,0);measure(x);}\n',
        ('attempt', '1..4096', 'budget'),
    ),
    'dyn_mem_over_bound': (
        '#include <quantum.h>\n#include <stdint.h>\n#include <string.h>\n'
        'int main(void){quant uint8_t dst[129]={0},src[129]={1};quant uint8_t n=1;'
        'memcpy(dst,src,n);reset(dst);reset(src);measure(n);}\n',
        ('128', '64-byte', 'synthesis bound'),
    ),
}

FULL_NEGATIVE = {
    'backward_goto_cycle': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant uint8_t x=0;int i=0;again:x+=1;++i;if(i<2)goto again;measure(x);return 0;}\n',
        ('goto', 'backward', 'cycle', 'lifetime'),
    ),
    'goto_across_control': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant bool c=superposition(0,1);if(c){goto inside;}'
        'if(c){inside:;quant uint8_t x=1;measure(x);}measure(c);return 0;}\n',
        ('goto', 'control', 'branch', 'scope'),
    ),
    'unbounded_while_q': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant uint8_t x=superposition(0,3);while(x!=2){x+=1;}measure(x);return 0;}\n',
        ('finite', 'bound', 'while', 'measurement'),
    ),
    'escaping_qptr_cpu': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'static quant uint8_t *g_ptr;\n'
        'int main(void){quant uint8_t x=1;g_ptr=&x;measure(x);return g_ptr!=0;}\n',
        ('escape', 'global', 'borrow', 'local', 'quantum pointer'),
    ),
    'aligned_alloc_bad_align': (
        '#include <quantum.h>\n#include <stdint.h>\n#include <stdlib.h>\n'
        'int main(void){quant uint32_t *p=aligned_alloc(3,16);if(p)free((void *)p);return 0;}\n',
        ('aligned_alloc', 'alignment', 'power'),
    ),
    'float_superposition_rejected': (
        '#include <quantum.h>\n' 'int main(void){quant float f=superposition(0,1);measure(f);}\n',
        ('integer quantum register', 'float', 'superposition'),
    ),
    'float_fourier_rejected': (
        '#include <quantum.h>\n' 'int main(void){quant float f=1.0f;fourier(f);measure(f);}\n',
        ('fourier', 'integer', 'float'),
    ),
    'long_double_quant_rejected': (
        '#include <quantum.h>\n' 'int main(void){quant long double ld=1.0L;return (int)ld;}\n',
        ('quant requires', 'float/double', 'long double'),
    ),
    'const_quant_mutation': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'int main(void){quant const uint8_t c=3;c+=1;measure(c);}\n',
        ('const', 'read-only', 'assignment', 'mutable'),
    ),
    'unbounded_recursion_rejected': (
        '#include <quantum.h>\n#include <stdint.h>\n'
        'static quant uint8_t inf(quant uint8_t x){return inf(x);}\n'
        'int main(void){quant uint8_t x=1;quant uint8_t y=inf(x);measure(y);measure(x);}\n',
        ('recursion', 'recursive', 'finite', 'bound'),
    ),
}

INTEGRATED_EXAMPLES = [
    'float_double',
    'everyday_vector_chain',
    'everyday_matrix_struct',
    'everyday_buffers_strings',
    'everyday_dynamic_vector',
    'everyday_control_phase',
]


def run(
    cmd: list[str], name: str, env: dict[str, str] | None = None
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(cmd, env=env, text=True, capture_output=True, timeout=180)
    completed.stdout = (completed.stdout or '') + (completed.stderr or '')
    assert isinstance(completed.returncode, int), name
    return completed


def evolve(state: dict[int, complex], gates: list) -> dict[int, complex]:
    for kind, wire, mask, pattern, angle in gates:
        mask_i, pattern_i = int(mask, 0), int(pattern, 0)
        updated: dict[int, complex] = {}
        for basis, amp in state.items():
            if basis & mask_i != pattern_i:
                updated[basis] = updated.get(basis, 0j) + amp
            elif kind == 0:
                low = basis & ~(1 << wire)
                high = low | (1 << wire)
                updated[low] = updated.get(low, 0j) + amp / math.sqrt(2)
                updated[high] = updated.get(high, 0j) + amp * (
                    -1 if basis >> wire & 1 else 1
                ) / math.sqrt(2)
            elif kind == 1:
                dst = basis ^ (1 << wire)
                updated[dst] = updated.get(dst, 0j) + amp
            elif kind == 2:
                updated[basis] = updated.get(basis, 0j) + amp * cmath.exp(1j * angle)
            else:
                raise AssertionError('unexpected non-unitary op before trace checkpoint')
        state = {b: a for b, a in updated.items() if abs(a) > 1e-13}
    return state


def equal_states(actual: dict[int, complex], expected: dict[int, complex]) -> None:
    for basis in actual.keys() | expected.keys():
        diff = abs(actual.get(basis, 0j) - expected.get(basis, 0j))
        assert diff < 1e-10, (hex(basis), actual.get(basis), expected.get(basis))


def expected_trace_state(case: str, point: dict) -> dict[int, complex] | None:
    regs = {r['name']: r for r in point['registers'] if r['name'] is not None}

    def encode(assignments: dict[str, int]) -> int:
        basis = 0
        for name, value in assignments.items():
            for bit, wire in enumerate(regs[name]['wires']):
                if (value >> bit) & 1:
                    basis |= 1 << wire
        return basis

    if case == 'phase_kickback':
        return {
            encode({'k': 0}): 1 / math.sqrt(2),
            encode({'k': 1}): cmath.exp(1j * 0.41) / math.sqrt(2),
        }
    if case == 'shifted_superposition_phase':
        return {
            encode({'x': v}): (cmath.exp(1j * 0.5) if v == 6 else 1.0) / 2.0 for v in range(4, 8)
        }
    if case == 'struct_pointer_move':
        return {
            encode(
                {'c': 0, 'q.a': 5, 'q.b': 1, '__history_1': 1, '__history_2': 5, '__history_5': 4}
            ): 1
            / math.sqrt(2),
            encode(
                {'c': 1, 'q.a': 5, 'q.b': 3, '__history_1': 3, '__history_2': 5, '__history_5': 6}
            ): 1
            / math.sqrt(2),
        }
    if case == 'b1_float_phase_kickback':
        return {
            encode({'k': 0, 'f': 0xBFC00000}): 1 / math.sqrt(2),
            encode({'k': 1, 'f': 0x3FC00000}): cmath.exp(1j * 0.37) / math.sqrt(2),
        }
    if case == 'b1_float_add_state':
        return {
            encode({'c': 0, 'a': 0xBF000000, 's': 0x00000000}): 1 / math.sqrt(2),
            encode({'c': 1, 'a': 0x3FA00000, 's': 0x3FE00000}): 1 / math.sqrt(2),
        }
    if case == 'b1_double_compare_phase':
        return {
            encode({'c': 0}): cmath.exp(1j * 0.29) / math.sqrt(2),
            encode({'c': 1}): 1 / math.sqrt(2),
        }
    return None


def verify_sanitizer_instrumentation(compiler_root: Path) -> None:
    for component in (
        'cket_plugin.so',
        'json.o',
        'quantum.o',
        'verify.o',
        'simulator.o',
        'cket_runtime.o',
        'cket',
    ):
        res = run(['nm', '-u', str(compiler_root / 'build' / component)], 'nm-' + component)
        assert (
            res.returncode == 0 and '__asan_init' in res.stdout and '__ubsan_handle' in res.stdout
        ), (
            component,
            res.stdout,
        )


def check_trace_suite(
    cases: dict[str, str],
    compiler: Path,
    adapter: Path,
    output: Path,
    levels: list[str],
    flags: list[str],
    base_env: dict[str, str],
) -> tuple[int, int]:
    points = inverses = 0
    links = ['-Wl,' + str(adapter), '-Wl,--wrap=qc_create_sparse', '-Wl,--wrap=__cket_apply']
    for level in levels:
        for case, body in cases.items():
            src = output / f'{case}.c'
            src.write_text(HEADER + body + '\n')
            binary = output / f'{case}_{level[1:]}'
            comp = run(
                [str(compiler / 'build/cket'), str(src), level, *flags, *links, '-o', str(binary)],
                f'{case}{level}-compile',
                env=base_env,
            )
            assert comp.returncode == 0, comp.stdout
            for tables in (0, 1):
                env = dict(
                    base_env, CKET_SIMULATOR='sparse', CKET_SEED='7', OWNERSHIP_TABLES=str(tables)
                )
                res = run([str(binary)], f'{case}{level}-t{tables}', env=env)
                assert res.returncode == 0, res.stdout
                snaps = [json.loads(ln) for ln in res.stdout.splitlines() if ln.startswith('{')]
                assert len(snaps) == 1, res.stdout
                state: dict[int, complex] = {0: 1 + 0j}
                recorded: list = []
                for point in snaps:
                    assert point['measurements'] == 0 and point['resets'] == 0
                    recorded.extend(point['gates'])
                    state = evolve(state, point['gates'])
                    if point['qubits'] <= 1024:
                        native = {int(b, 0): complex(r, im) for b, r, im in point['state']}
                        equal_states(native, state)
                        want = expected_trace_state(case, point)
                        if want is not None:
                            equal_states(native, want)
                        elif case == 'grover_find_state':
                            hit_wires = {
                                r['name']: r['wires']
                                for r in point['registers']
                                if r['name'] == 'hit'
                            }['hit']
                            prob_3 = sum(
                                abs(a) ** 2
                                for b, a in native.items()
                                if sum(((b >> w) & 1) << bit for bit, w in enumerate(hit_wires))
                                == 3
                            )
                            assert abs(prob_3 - 0.9613189697265625) < 1e-10, prob_3
                inv = [
                    [k, w, m, p, -ang if k == 2 else ang] for k, w, m, p, ang in reversed(recorded)
                ]
                equal_states(evolve(state, inv), {0: 1 + 0j})
                points += len(snaps)
                inverses += 1
                print(f'PASS trace {case} {level} tables={tables}', flush=True)
    return points, inverses


def check_programs_suite(
    programs: dict[str, dict],
    compiler: Path,
    output: Path,
    levels: list[str],
    flags: list[str],
    base_env: dict[str, str],
) -> int:
    checks = 0
    for level in levels:
        for name, spec in programs.items():
            src = output / f'{name}_{level[1:]}.c'
            src.write_text(spec['src'])
            ir = output / f'{name}_{level[1:]}.ir.json'
            binary = output / f'{name}_{level[1:]}'
            comp = run(
                [str(compiler / 'build/cket'), str(src), level, *flags, '-o', str(binary)],
                f'{name}{level}-compile',
                env=base_env,
            )
            assert comp.returncode == 0, comp.stdout
            emit_ir = run(
                [str(compiler / 'build/cket'), str(src), level, '--emit-ir', '-o', str(ir)],
                f'{name}{level}-ir',
                env=base_env,
            )
            assert emit_ir.returncode == 0, emit_ir.stdout
            val_ir = run(
                [str(compiler / 'build/cket_backend'), '--validate', str(ir)], f'{name}{level}-val'
            )
            assert val_ir.returncode == 0, val_ir.stdout
            checks += 2
            if spec.get('static_qasm'):
                qasm = output / f'{name}_{level[1:]}.qasm'
                eq = run(
                    [str(compiler / 'build/cket'), str(src), level, '--emit-qasm', '-o', str(qasm)],
                    f'{name}{level}-qasm',
                    env=base_env,
                )
                assert eq.returncode == 0 and qasm.stat().st_size > 0, eq.stdout
                checks += 1
            for seed in (0, 3, 7, 11):
                env = dict(base_env, CKET_SIMULATOR='sparse', CKET_SEED=str(seed))
                exe = run([str(binary)], f'{name}{level}-seed{seed}', env=env)
                assert exe.returncode == 0, exe.stdout
                checks += 1
            print(f'PASS program {name} {level}', flush=True)
    return checks


def check_multi_tu_suite(
    compiler: Path, output: Path, levels: list[str], flags: list[str], base_env: dict[str, str]
) -> int:
    lib_src = output / 'a8_lib.c'
    main_src = output / 'a8_main.c'
    lib_src.write_text(
        '#include <quantum.h>\n#include <stdint.h>\n'
        'quant uint16_t a8_helper(quant const uint16_t x,uint16_t k){return (uint16_t)(x*3u+k);}\n'
    )
    main_src.write_text(
        '#include <quantum.h>\n#include <stdint.h>\n'
        'quant uint16_t a8_helper(quant const uint16_t x,uint16_t k);\n'
        'int main(void){quant uint16_t x=superposition(0,3);'
        'quant uint16_t r=a8_helper(x,5u);'
        'unsigned vx=measure(x),vr=measure(r);'
        'return vr!=(3u*vx+5u);}\n'
    )
    checks = 0
    for level in levels:
        lib_obj = output / f'a8_lib_{level[1:]}.o'
        main_obj = output / f'a8_main_{level[1:]}.o'
        exe = output / f'a8_prog_{level[1:]}'
        for s, o in ((lib_src, lib_obj), (main_src, main_obj)):
            r = run(
                [str(compiler / 'build/cket'), '-c', str(s), level, *flags, '-o', str(o)],
                f'a8-{s.stem}{level}',
                env=base_env,
            )
            assert r.returncode == 0, r.stdout
        link = run(
            [
                str(compiler / 'build/cket'),
                str(lib_obj),
                str(main_obj),
                level,
                *flags,
                '-o',
                str(exe),
            ],
            f'a8-link{level}',
            env=base_env,
        )
        assert link.returncode == 0, link.stdout
        for seed in (0, 3, 7):
            res = run(
                [str(exe)],
                f'a8-run{level}-{seed}',
                env=dict(base_env, CKET_SIMULATOR='sparse', CKET_SEED=str(seed)),
            )
            assert res.returncode == 0, res.stdout
            checks += 1
        print(f'PASS multi-tu linking {level}', flush=True)
    return checks


def check_negatives_suite(
    negatives: dict[str, tuple[str, tuple[str, ...]]],
    compiler: Path,
    output: Path,
    levels: list[str],
    flags: list[str],
    base_env: dict[str, str],
) -> int:
    rejections = 0
    for level in levels:
        for name, (src_text, diagnostics) in negatives.items():
            src = output / f'neg_{name}_{level[1:]}.c'
            exe = output / f'neg_{name}_{level[1:]}'
            src.write_text(src_text)
            res = run(
                [str(compiler / 'build/cket'), str(src), level, *flags, '-o', str(exe)],
                f'neg-{name}{level}',
                env=base_env,
            )
            if res.returncode == 0:
                res = run(
                    [str(exe)],
                    f'neg-run-{name}{level}',
                    env=dict(base_env, CKET_SIMULATOR='sparse', CKET_SEED='7'),
                )
            low = res.stdout.lower()
            assert res.returncode > 0 and any(tok in low for tok in diagnostics), (
                name,
                level,
                res.stdout,
            )
            rejections += 1
    return rejections


def check_integrated_examples(
    compiler: Path, output: Path, levels: list[str], flags: list[str], base_env: dict[str, str]
) -> int:
    checks = 0
    for level in levels:
        for name in INTEGRATED_EXAMPLES:
            src = ROOT / 'examples' / f'{name}.c'
            binary = output / f'int_{name}_{level[1:]}'
            comp = run(
                [str(compiler / 'build/cket'), str(src), level, *flags, '-o', str(binary)],
                f'int-{name}{level}',
                env=base_env,
            )
            assert comp.returncode == 0, comp.stdout
            ir = output / f'int_{name}_{level[1:]}.ir.json'
            eir = run(
                [str(compiler / 'build/cket'), str(src), level, '--emit-ir', '-o', str(ir)],
                f'int-ir-{name}{level}',
                env=base_env,
            )
            assert eir.returncode == 0, eir.stdout
            vir = run(
                [str(compiler / 'build/cket_backend'), '--validate', str(ir)],
                f'int-vir-{name}{level}',
            )
            assert vir.returncode == 0, vir.stdout
            for seed in (0, 3, 7, 11):
                exe = run(
                    [str(binary)],
                    f'int-run-{name}{level}-{seed}',
                    env=dict(base_env, CKET_SIMULATOR='sparse', CKET_SEED=str(seed)),
                )
                assert exe.returncode == 0, exe.stdout
                checks += 1
            print(f'PASS integrated {name} {level}', flush=True)
    return checks


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', nargs='+', default=['O0', 'O2'])
    parser.add_argument('--sanitized', action='store_true')
    parser.add_argument('--phase', choices=('pre-w1b', 'full'), default='full')
    args = parser.parse_args()

    compiler = args.compiler_root.resolve()
    output = args.output.resolve()
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    levels = [lvl if lvl.startswith('-') else '-' + lvl for lvl in args.levels]
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror']
    base_env = dict(os.environ)
    if args.sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        verify_sanitizer_instrumentation(compiler)
        real_cc = os.environ.get('CKET_REAL_GCC', 'gcc-11')
        base_env['CKET_REAL_GCC'] = real_cc
        base_env['CC'] = str(compiler / 'build/gcc-wrapper')
        base_env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    else:
        real_cc = os.environ.get('CC', 'gcc-11')

    adapter = output / 'ownership_trace.o'
    adp = run(
        [
            real_cc,
            '-std=c11',
            '-O2',
            '-Wall',
            '-Wextra',
            '-Werror',
            *flags,
            '-c',
            str(ROOT / 'tests/ownership_trace.c'),
            '-o',
            str(adapter),
        ],
        'compile-adapter',
    )
    assert adp.returncode == 0, adp.stdout

    trace_cases = dict(PRE_TRACE_CASES)
    program_cases = dict(PRE_PROGRAMS)
    negative_cases = dict(PRE_NEGATIVE)
    if args.phase == 'full':
        trace_cases.update(FULL_TRACE_CASES)
        program_cases.update(FULL_PROGRAMS)
        negative_cases.update(FULL_NEGATIVE)

    trace_points, trace_inverses = check_trace_suite(
        trace_cases, compiler, adapter, output, levels, flags, base_env
    )
    program_checks = check_programs_suite(program_cases, compiler, output, levels, flags, base_env)
    multi_tu_checks = (
        check_multi_tu_suite(compiler, output, levels, flags, base_env)
        if args.phase == 'full'
        else 0
    )
    integrated_checks = (
        check_integrated_examples(compiler, output, levels, flags, base_env)
        if args.phase == 'full'
        else 0
    )
    rejections = check_negatives_suite(negative_cases, compiler, output, levels, flags, base_env)

    summary = {
        'status': 'passed',
        'phase': args.phase,
        'levels': [lvl.lstrip('-') for lvl in levels],
        'sanitized': args.sanitized,
        'trace_checkpoints': trace_points,
        'complete_gate_inverses': trace_inverses,
        'program_checks': program_checks,
        'multi_tu_checks': multi_tu_checks,
        'integrated_checks': integrated_checks,
        'negative_rejections': rejections,
        'plugin_sha256': hashlib.sha256(
            (compiler / 'build/cket_plugin.so').read_bytes()
        ).hexdigest(),
        'scope': (
            'Everyday C profile A1-A12, IEEE-754 float/double, bounded while/goto, '
            'stdlib extensions and 6 integrated examples across opt levels and ASan/UBSan.'
        ),
    }
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
