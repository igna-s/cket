/* Validation helper: record the original QIR's measurement transcript.
 * The independent QASM implementation reads the corresponding exported bits. */
#include "internal.h"
#include <inttypes.h>
#include <stdlib.h>
static int compare(const void *a,const void *b) {
    uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y);
}
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    char *end;unsigned long shots=strtoul(argv[2],&end,10);
    if(*end || !shots || shots>100000) return 2;
    char *text=qc_read_file(argv[1]);cj_doc d={0};uint64_t *samples=calloc(shots,sizeof *samples);
    if(!text || !samples || !cj_parse(&d,text)) {free(text);free(samples);cj_free(&d);return 1;}
    int ok=1,list=cj_get(&d,0,"instructions");
    for(unsigned long shot=0;shot<shots && ok;++shot) {
        qc_context *c=qc_create(1,913+shot);unsigned bits=0;
        ok=c && qc_verify_document(c,&d);
        for(int t=list+1;ok && t<d.tokens[list].next;t=d.tokens[t].next) {
            int measured=cj_equal(&d,cj_get(&d,t,"op"),"qmeasure");unsigned width=0;
            if(measured) {
                uint64_t id;ok=cj_u64(&d,cj_get(&d,t,"id"),&id);
                qc_register *r=ok?qc_lookup(c,(uint32_t)id):NULL;
                ok=r && r->width<=64-bits;if(ok) width=r->width;
            }
            uint64_t result=0;ok=ok && qc_apply_token(c,&d,t,0,0,&result);
            if(ok && measured) {samples[shot]|=result<<bits;bits+=width;}
        }
        if(!ok) fprintf(stderr,"measurement validation: %s\n",c?qc_error(c):"out of memory");
        qc_destroy(c);
    }
    if(ok) {
        qsort(samples,shots,sizeof *samples,compare);fputc('{',stdout);
        for(unsigned long start=0;start<shots;) {
            unsigned long next=start+1;while(next<shots && samples[next]==samples[start]) ++next;
            printf("%s\"%" PRIu64 "\":%lu",start?",":"",samples[start],next-start);start=next;
        }
        puts("}");
    }
    free(text);free(samples);cj_free(&d);return !ok;
}
