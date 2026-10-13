/* A caller supplies the CPU decisions of one finite execution. This test
 * utility selects that trace explicitly; it never exports a flattened branch
 * template or evaluates a quantum condition on the CPU. */
#include "internal.h"
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *select_cpu_trace(const char *ir, const unsigned *decisions, size_t count) {
    cj_doc d = {0};
    qc_context *check = qc_create(0, 7);
    if (!check || !cj_parse(&d, ir) || !qc_verify_document(check, &d))
        validation_fail("cannot select an invalid CPU trace");
    qc_destroy(check);
    size_t capacity = strlen(ir) + 128;
    char *result = malloc(capacity);
    if (!result)
        validation_fail("CPU trace allocation failed");
    size_t length = (size_t)snprintf(
        result, capacity, "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[");

    struct {
        int parent;
        unsigned decision;
    } stack[256];

    unsigned depth = 0;
    size_t next = 0;
    int active = 1, comma = 0;
    int list = cj_get(&d, 0, "instructions");
    for (int token = list + 1; token < d.tokens[list].next; token = d.tokens[token].next) {
        int op = cj_get(&d, token, "op");
        if (cj_equal(&d, op, "ccontrol_begin")) {
            if (depth == 256 || next == count || decisions[next] > 1)
                validation_fail("CPU trace decision/depth contract failed");
            stack[depth].parent = active;
            stack[depth].decision = decisions[next++];
            active &= (int)stack[depth++].decision;
        } else if (cj_equal(&d, op, "ccontrol_else")) {
            active = stack[depth - 1].parent && !stack[depth - 1].decision;
        } else if (cj_equal(&d, op, "ccontrol_end")) {
            active = stack[--depth].parent;
        } else if (cj_equal(&d, op, "cloop_begin") || cj_equal(&d, op, "cloop_end")) {
            validation_fail("CPU trace utility requires explicit loop expansion");
        } else if (active) {
            size_t size = d.tokens[token].end - d.tokens[token].begin;
            if (length + size + 4 >= capacity)
                validation_fail("CPU trace output exceeded its checked budget");
            if (comma++)
                result[length++] = ',';
            memcpy(result + length, d.text + d.tokens[token].begin, size);
            length += size;
        }
    }
    if (depth || next != count)
        validation_fail("CPU trace decisions do not match the template");
    memcpy(result + length, "]}", 3);
    cj_free(&d);
    return result;
}
