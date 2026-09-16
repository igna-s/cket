#include "json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad(cj_doc *d, const char *message) {
    snprintf(d->error, sizeof d->error, "%s", message); return -1;
}
static void space(const char *s, size_t *p) { while (isspace((unsigned char)s[*p])) ++*p; }
static int value(cj_doc *d, size_t *p, unsigned depth) {
    if (depth > 256) return bad(d, "JSON nesting exceeds 256 levels");
    space(d->text, p);
    if (d->count == d->capacity) {
        int n = d->capacity ? d->capacity * 2 : 128;
        if (n > 1048576) return bad(d, "Quantum IR contains too many tokens");
        cj_token *t = realloc(d->tokens, (size_t)n * sizeof *t);
        if (!t) return bad(d, "out of memory parsing Quantum IR");
        d->tokens = t; d->capacity = n;
    }
    int id = d->count++;
    d->tokens[id] = (cj_token){*p, 0, 0, 0, d->text[*p]};
    char c = d->text[*p];
    if (c == '{' || c == '[') {
        ++*p; space(d->text, p); char close = c == '{' ? '}' : ']';
        if (d->text[*p] != close) for (;;) {
            if (c == '{') {
                if (d->text[*p] != '"' || value(d,p,depth+1) < 0) return bad(d,"invalid JSON object key");
                ++d->tokens[id].children; space(d->text,p);
                if (d->text[(*p)++] != ':') return bad(d,"missing JSON colon");
            }
            if (value(d,p,depth+1) < 0) return -1;
            ++d->tokens[id].children; space(d->text,p);
            if (d->text[*p] == close) break;
            if (d->text[(*p)++] != ',') return bad(d,"missing JSON comma");
            space(d->text,p);
        }
        ++*p;
    } else if (c == '"') {
        ++*p;
        while (d->text[*p] && d->text[*p] != '"') {
            if ((unsigned char)d->text[*p] < 32) return bad(d,"control byte in JSON string");
            if (d->text[(*p)++] == '\\') {
                char escape = d->text[*p];
                if (!escape || !strchr("\"\\/bfnrtu",escape)) return bad(d,"invalid JSON escape");
                ++*p;
                if (escape == 'u') for (int j=0;j<4;++j) {
                    if (!isxdigit((unsigned char)d->text[*p])) return bad(d,"invalid Unicode escape");
                    ++*p;
                }
            }
        }
        if (!d->text[*p]) return bad(d,"unterminated JSON string");
        ++*p;
    } else if (c == '-' || isdigit((unsigned char)c)) {
        if (c == '-') ++*p;
        if (!isdigit((unsigned char)d->text[*p])) return bad(d,"invalid JSON number");
        if (d->text[*p] == '0') ++*p; else while (isdigit((unsigned char)d->text[*p])) ++*p;
        if (d->text[*p] == '.') {
            ++*p; if (!isdigit((unsigned char)d->text[*p])) return bad(d,"invalid JSON fraction");
            while (isdigit((unsigned char)d->text[*p])) ++*p;
        }
        if (d->text[*p] == 'e' || d->text[*p] == 'E') {
            ++*p; if (d->text[*p] == '+' || d->text[*p] == '-') ++*p;
            if (!isdigit((unsigned char)d->text[*p])) return bad(d,"invalid JSON exponent");
            while (isdigit((unsigned char)d->text[*p])) ++*p;
        }
    } else {
        const char *word = c=='t' ? "true" : c=='f' ? "false" : c=='n' ? "null" : NULL;
        if (!word || strncmp(d->text+*p,word,strlen(word))) return bad(d,"invalid JSON value");
        *p += strlen(word);
    }
    d->tokens[id].end = *p; d->tokens[id].next = d->count; return id;
}
int cj_parse(cj_doc *d, const char *s) {
    memset(d,0,sizeof *d); d->text=s; size_t p=0;
    if (value(d,&p,0) < 0) return 0;
    space(s,&p); if (s[p]) { bad(d,"trailing data in Quantum IR"); return 0; }
    return 1;
}
void cj_free(cj_doc *d) { free(d->tokens); d->tokens=NULL; }
int cj_equal(const cj_doc *d, int t, const char *s) {
    if (t < 0 || t >= d->count) return 0;
    size_t begin=d->tokens[t].begin, end=d->tokens[t].end;
    if (d->tokens[t].kind=='"') { ++begin; --end; }
    return end-begin==strlen(s) && !memcmp(d->text+begin,s,end-begin);
}
int cj_get(const cj_doc *d, int t, const char *key) {
    if (t<0 || d->tokens[t].kind!='{') return -1;
    for (int i=t+1;i<d->tokens[t].next;) {
        int v=d->tokens[i].next;
        if (cj_equal(d,i,key)) return v;
        i=d->tokens[v].next;
    }
    return -1;
}
char *cj_copy(const cj_doc *d, int t) {
    if (t<0 || t>=d->count) return NULL;
    size_t n=d->tokens[t].end-d->tokens[t].begin;
    char *s=malloc(n+1); if (!s) return NULL;
    memcpy(s,d->text+d->tokens[t].begin,n); s[n]=0; return s;
}
int cj_u64(const cj_doc *d,int t,uint64_t *v) {
    char *s=cj_copy(d,t), *end; if (!s) return 0; errno=0;
    if (s[0]=='-') *v=(uint64_t)strtoll(s,&end,10); else *v=strtoull(s,&end,10);
    int ok=!errno && end!=s && !*end; free(s); return ok;
}
int cj_bool(const cj_doc *d,int t) { return cj_equal(d,t,"true"); }
double cj_double(const cj_doc *d,int t) {
    if (t<0 || t>=d->count || (d->tokens[t].kind!='-' && (d->tokens[t].kind<'0' || d->tokens[t].kind>'9'))) return NAN;
    char *s=cj_copy(d,t),*end; if (!s) return NAN;
    errno=0; double v=strtod(s,&end);
    if (errno || end==s || *end) v=NAN;
    free(s); return v;
}
