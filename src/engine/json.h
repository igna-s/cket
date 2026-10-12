#ifndef CKET_JSON_H
#define CKET_JSON_H
#include <stddef.h>
#include <stdint.h>

/* Parser for serialized Quantum IR only. GCC is the sole C parser. */
typedef struct {
    size_t begin, end;
    int next, children;
    char kind;
} cj_token;

typedef struct {
    const char *text;
    cj_token *tokens;
    int count, capacity;
    char error[160];
} cj_doc;

int cj_parse(cj_doc *doc, const char *text);
void cj_free(cj_doc *doc);
int cj_get(const cj_doc *doc, int object, const char *key);
int cj_equal(const cj_doc *doc, int token, const char *text);
int cj_u64(const cj_doc *doc, int token, uint64_t *value);
int cj_bool(const cj_doc *doc, int token);
double cj_double(const cj_doc *doc, int token);
char *cj_copy(const cj_doc *doc, int token);
#endif
