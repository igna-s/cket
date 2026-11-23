#define _GNU_SOURCE
#include "validation.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name, *expected;
    int static_export, runtime, sparse, bindings, ctype_c;
} example;

static const example examples[] = {
    {"hello", "12\n", 1, 1, 0, 0, 0},
    {"entanglement", NULL, 1, 1, 0, 0, 0},
    {"find", "3\n", 1, 1, 0, 0, 0},
    {"search", "3\n", 0, 1, 0, 0, 0},
    {"search_empty", "Search budget exhausted\n", 0, 1, 0, 0, 0},
    {"coherent", NULL, 1, 1, 0, 0, 0},
    {"prepare_measure", NULL, 1, 1, 0, 0, 0},
    {"fourier", "7\n", 1, 1, 0, 0, 0},
    {"phase_estimation", "3\n", 1, 1, 0, 0, 0},
    {"functions", "4\n", 1, 1, 0, 0, 0},
    {"circuit_module", "7\n", 1, 1, 0, 0, 0},
    {"products", "9 2\n3 7\n", 1, 1, 0, 0, 0},
    {"vector_function", "10 1\n", 1, 1, 0, 0, 0},
    {"dynamic_vector", "3\n", 0, 1, 0, 0, 0},
    {"hybrid_loop", "29\n", 0, 1, 0, 0, 0},
    /* quant int is 32 bits: accepted for export, above the simulator limit. */
    {"types", "", 1, 0, 0, 0, 0},
    {"for_sum", "sum(1..10)=55\n", 0, 1, 0, 0, 0},
    {"while_sum", "while sum=55 iterations=10\n", 0, 1, 0, 0, 0},
    {"do_while_countdown", "remaining=5 iterations=5\n", 0, 1, 0, 0, 0},
    {"nested_for_sum", "matrix sum=78\n", 0, 1, 0, 0, 0},
    {"register_accumulate", "sum=15 restored=0 term=3\n", 0, 1, 0, 0, 0},
    {"fibonacci", "F(8)=21 F(9)=34\n", 0, 1, 0, 0, 0},
    {"modular_arithmetic", "add=14 subtract=240 xor=255\n", 1, 1, 0, 0, 0},
    {"dot_product", "dot product=70\n", 0, 1, 0, 0, 0},
    {"pair_minimum", "min(19,7)=7\n", 1, 1, 0, 0, 0},
    {"pair_maximum", "max(19,7)=19\n", 1, 1, 0, 0, 0},
    {"minimum_scan", "minimum f(x)=min(x,15-x): value=0 x=0\n", 0, 1, 0, 0, 0},
    {"maximum_scan", "maximum f(x)=min(x,15-x): value=7 x=7\n", 0, 1, 0, 0, 0},
    {"minmax_array", "min=0 index=3 max=255 index=5\n", 0, 1, 0, 0, 0},
    {"input_sum", "count=10 sum=55\n", 0, 1, 0, 0, 0},
    {"input_minmax", "min=0 index=3 max=255 index=5\n", 0, 1, 0, 0, 0},
    {"measurement_histogram", NULL, 0, 1, 0, 0, 0},
    {"repeat_until_success", NULL, 0, 1, 0, 0, 0},
    {"find_multiple", NULL, 1, 1, 0, 0, 0},
    {"search_constraints", NULL, 0, 1, 0, 0, 0},
    {"coherent_accumulate", NULL, 1, 1, 0, 0, 0},
    {"quantum_buckets", NULL, 1, 1, 0, 0, 0},
    {"coherent_returns", "count=1 control=1 target=0\n", 0, 1, 0, 0, 0},
    {"procedure_returns", "limit=2 visited=2 result=0 interference=0\n", 0, 1, 0, 0, 0},
    {"readonly_borrows", "scalar=1 vector=0,1\n", 1, 1, 0, 0, 0},
    {"blocks/02/wide_integers", NULL, 1, 1, 1, 0, 0},
    {"blocks/02/wide_size", NULL, 1, 1, 1, 0, 0},
    {"blocks/02/variable_shift", NULL, 1, 1, 1, 0, 0},
    {"blocks/02/shift_predicate", NULL, 1, 1, 1, 0, 0},
    {"blocks/03/buffer_functions", NULL, 1, 1, 1, 0, 0},
    {"blocks/03/buffer_words", NULL, 1, 1, 1, 0, 0},
    {"blocks/03/cpu_vector_scan", NULL, 0, 1, 1, 0, 0},
    {"blocks/03/cpu_vector_return", NULL, 0, 1, 1, 0, 0},
    {"blocks/04/quantum_index", NULL, 1, 1, 1, 0, 0},
    {"blocks/05/dynamic_heap", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/coherent_heap", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/heap_bytes", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/heap_transfer", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/heap_iterations", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/fixed_buffers", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/buffer_lengths", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/borrow_cleanup", NULL, 1, 1, 1, 0, 0},
    {"blocks/05/heap_coherence", NULL, 0, 1, 1, 0, 0},
    {"blocks/05/fixed_coherence", NULL, 0, 1, 1, 0, 0},
    {"blocks/06/string_spans", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/string_search", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/search_positions", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/saved_search", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/string_copy", NULL, 1, 1, 1, 1, 0},
    {"blocks/06/string_concat", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/controlled_concat", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/byte_copy", NULL, 1, 1, 1, 0, 0},
    {"blocks/06/decimal_conversion", NULL, 1, 1, 1, 0, 1},
    {"blocks/06/search_write", NULL, 1, 1, 1, 0, 0},
    {"float_double", NULL, 1, 1, 1, 0, 0},
    {"everyday_vector_chain", NULL, 1, 1, 1, 0, 0},
    {"everyday_matrix_struct", NULL, 1, 1, 1, 0, 0},
    {"everyday_buffers_strings", NULL, 0, 1, 1, 0, 0},
    {"everyday_dynamic_vector", NULL, 0, 1, 1, 0, 0},
    {"everyday_control_phase", NULL, 0, 1, 1, 0, 0},
    {"everyday_math_geometry", NULL, 1, 1, 1, 0, 0}};

typedef struct {
    const char *name, *input, *output;
    int status;
} input_case;

static const input_case input_cases[] = {
    {"input_sum", "0\n", "count=0 sum=0\n", 0},
    {"input_sum", "1\n", "count=1 sum=1\n", 0},
    {"input_sum", "4\n", "count=4 sum=10\n", 0},
    {"input_sum", "16\n", "count=16 sum=136\n", 0},
    {"input_sum", "0004\n", "count=4 sum=10\n", 0},
    {"input_sum", "4294967296\n", "", 1},
    {"input_sum", "184467440737095516160\n", "", 1},
    {"input_sum", "16x\n", "", 1},
    {"input_sum", "17\n", "", 1},
    {"input_sum", "-1\n", "", 1},
    {"input_sum", "no\n", "", 1},
    {"input_sum", "", "", 1},
    {"input_minmax", "1 0\n", "min=0 index=0 max=0 index=0\n", 0},
    {"input_minmax", "1 255\n", "min=255 index=0 max=255 index=0\n", 0},
    {"input_minmax", "4 7 7 7 7\n", "min=7 index=0 max=7 index=0\n", 0},
    {"input_minmax", "5 5 4 3 2 1\n", "min=1 index=4 max=5 index=0\n", 0},
    {"input_minmax", "3 0 255 0\n", "min=0 index=0 max=255 index=1\n", 0},
    {"input_minmax", "\n 2 0002 0000\t", "min=0 index=1 max=2 index=0\n", 0},
    {"input_minmax", "4294967297\n", "", 1},
    {"input_minmax", "1 4294967296\n", "", 1},
    {"input_minmax", "1 184467440737095516160\n", "", 1},
    {"input_minmax", "1 7x\n", "", 1},
    {"input_minmax", "0\n", "", 1},
    {"input_minmax", "9\n", "", 1},
    {"input_minmax", "-1\n", "", 1},
    {"input_minmax", "2 7\n", "", 1},
    {"input_minmax", "1 256\n", "", 1},
    {"input_minmax", "1 -1\n", "", 1},
    {"input_minmax", "1 no\n", "", 1},
    {"input_minmax", "no\n", "", 1},
    {"input_minmax", "", "", 1}};

static const char *sample_input(const example *e) {
    if (!strcmp(e->name, "input_sum"))
        return "10\n";
    if (!strcmp(e->name, "input_minmax"))
        return "8 42 17 42 0 128 255 9 255\n";
    return "0\n";
}

static unsigned checks;

static void require(validation *v, int condition, const char *name, const char *action) {
    if (!condition) {
        char *log = validation_read(v->log);
        fprintf(stderr, "%s", log);
        free(log);
        validation_fail("example %s failed: %s (artifacts: %s)", name, action, v->temp);
    }
    ++checks;
}

static void check_output(validation *v, const example *e, const char *output) {
    char *text = validation_read(output);
    int ok = 0;
    unsigned a, b, c, d, e1, f, g, h;
    int x, y, z, w, v1;
    unsigned long long u;
    char buf[8], ch1, ch2, tail;
    if (e->expected)
        ok = !strcmp(text, e->expected);
    else if (!strcmp(e->name, "entanglement"))
        ok = sscanf(text, "%u %u %c", &a, &b, &tail) == 2 && a <= 1 && a == b;
    else if (!strcmp(e->name, "coherent"))
        ok = sscanf(text, "%u %c", &a, &tail) == 1 && a >= 5 && a <= 20;
    else if (!strcmp(e->name, "prepare_measure"))
        ok = sscanf(text, "classical=10 quantum=%u %c", &a, &tail) == 1 && a <= 255;
    else if (!strcmp(e->name, "measurement_histogram"))
        ok = sscanf(text, "zeros=%u ones=%u trials=%u %c", &a, &b, &c, &tail) == 3 && a <= 32 &&
             b <= 32 && a + b == 32 && c == 32;
    else if (!strcmp(e->name, "repeat_until_success"))
        ok = sscanf(text, "found=%u attempts=%u %c", &a, &b, &tail) == 2 && a <= 1 && b >= 1 &&
             b <= 16 && (a || b == 16);
    else if (!strcmp(e->name, "find_multiple"))
        ok = sscanf(text, "candidate=%u %c", &a, &tail) == 1 && a <= 15 && a % 4 == 1;
    else if (!strcmp(e->name, "search_constraints"))
        ok = !strcmp(text, "Search budget exhausted\n") ||
             (sscanf(text, "candidate=%u %c", &a, &tail) == 1 && a >= 8 && a <= 13 && a % 3 == 1);
    else if (!strcmp(e->name, "coherent_accumulate"))
        ok = sscanf(text, "flag=%u total=%u %c", &a, &b, &tail) == 2 && a <= 1 &&
             b == (a ? 15u : 30u);
    else if (!strcmp(e->name, "quantum_buckets"))
        ok = sscanf(text, "value=%u bucket=%u %c", &a, &b, &tail) == 2 && a <= 15 && b == a / 3;
    else if (!strcmp(e->name, "blocks/02/wide_integers"))
        ok = sscanf(text, "control=%u value=%d quotient=%d %c", &a, &x, &y, &tail) == 3 && a <= 1 &&
             x == (a ? -7 : 10) && y == x / 3;
    else if (!strcmp(e->name, "blocks/02/wide_size"))
        ok = sscanf(text, "control=%u count=%llu tail=%u %c", &a, &u, &b, &tail) == 3 && a <= 1 &&
             u == (a ? UINT64_MAX : (1ULL << 63)) && b == (a ? 0x55u : 0xAAu);
    else if (!strcmp(e->name, "blocks/02/variable_shift"))
        ok = sscanf(text, "control=%u value=%u count=%u shifted=%u %c", &a, &b, &c, &d, &tail) ==
                 4 &&
             a <= 1 && c <= 31 && b == (a ? 0x80000001u : 0xfedcba98u) && d == (b >> c);
    else if (!strcmp(e->name, "blocks/02/shift_predicate")) {
        ok =
            sscanf(
                text, "control=%u count=%u nonzero=%u byte_nonzero=%u %c", &a, &b, &c, &d, &tail) ==
                4 &&
            a <= 1 && b <= 63;
        if (ok) {
            uint64_t val = (a ? (UINT64_C(1) << 63) : UINT64_C(3)) << b;
            ok = c == (val != 0) && d == ((uint8_t)val != 0);
        }
    } else if (!strcmp(e->name, "blocks/03/buffer_functions"))
        ok = sscanf(text, "index=%u result=%u values=%u,%u,%u %c", &a, &b, &c, &d, &e1, &tail) ==
                 5 &&
             a <= 1 && b == (a ? 10u : 6u) && c == 2 && d == (a ? 5u : 12u) && e1 == (a ? 12u : 9u);
    else if (!strcmp(e->name, "blocks/03/buffer_words"))
        ok = sscanf(text, "index=%u before=%u values=%u,%u %c", &a, &b, &c, &d, &tail) == 4 &&
             a <= 1 && b == (a ? 0x505u : 0x301u) && c == (a ? 0x201u : 0x1234u) &&
             d == (a ? 0x1234u : 0x405u);
    else if (!strcmp(e->name, "blocks/03/cpu_vector_scan"))
        ok = sscanf(text,
                    "sum=%u parity=%u picked=%u values=%u,%u,%u slot=%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &g,
                    &tail) == 7 &&
             d <= 3 && a == d + 10 && b == (d ^ 4) && c == 2 && e1 == 3 && f == 7 && g == 0;
    else if (!strcmp(e->name, "blocks/03/cpu_vector_return"))
        ok = sscanf(text,
                    "restored=%u values=%u,%u word=%u words=%u,%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &tail) == 6 &&
             a == 0 && b <= 3 && c == 5 && d == 0x556 && e1 == 0x123 && f == 0x456;
    else if (!strcmp(e->name, "blocks/04/quantum_index"))
        ok = sscanf(text,
                    "index=%u before=%u assigned=%u old=%u after=%u values=%u,%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &g,
                    &tail) == 7 &&
             a <= 1 && b == (a ? 9u : 3u) && c == 7 && d == 7 && e1 == 8 && f == (a ? 3u : 8u) &&
             g == (a ? 8u : 9u);
    else if (!strcmp(e->name, "blocks/05/borrow_cleanup"))
        ok = sscanf(text, "control=%u source=%u value=%u expected=%u %c", &a, &b, &c, &d, &tail) ==
                 4 &&
             a <= 1 && b <= 1 && d == (a ? b : 1u) && c == d;
    else if (!strcmp(e->name, "blocks/05/buffer_lengths"))
        ok = sscanf(text, "length=%u output={%u,%u} source=%u %c", &a, &b, &c, &d, &tail) == 4 &&
             a == 2 && c == d && ((b == 2 && c == 3) || (b == 1 && c == 9));
    else if (!strcmp(e->name, "blocks/05/coherent_heap"))
        ok = sscanf(text, "index=%u previous=%u values={%u,%u} %c", &a, &b, &c, &d, &tail) == 4 &&
             a <= 1 && b == (a ? 9u : 3u) && c == (a ? 3u : 4u) && d == (a ? 10u : 9u);
    else if (!strcmp(e->name, "blocks/05/dynamic_heap"))
        ok = sscanf(text,
                    "initial=%u count=%u grew=%d flag=%u sum=%u observed=%u %c",
                    &a,
                    &b,
                    &x,
                    &c,
                    &d,
                    &e1,
                    &tail) == 6 &&
             a == 2 && b == 3 && x == 1 && c <= 1 && d == (c ? 18u : 12u) && e1 == d;
    else if (!strcmp(e->name, "blocks/05/fixed_buffers"))
        ok = sscanf(text, "rounds=%u values={%u,%u,%u} %c", &a, &b, &c, &d, &tail) == 4 && a == 2 &&
             ((b == 11 && c == 10 && d == 9) || (b == 5 && c == 4 && d == 3));
    else if (!strcmp(e->name, "blocks/05/fixed_coherence"))
        ok = sscanf(text,
                    "length=%u control=%u high=%u value=%u expected=%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &tail) == 5 &&
             a == 1 && b <= 1 && c <= 1 && d == e1 && ((d >> 8) & 1u) == c;
    else if (!strcmp(e->name, "blocks/05/heap_bytes"))
        ok = sscanf(text,
                    "coin=%u copy={%u,%u,%u} fill={%u,%u,%u} %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &g,
                    &tail) == 7 &&
             a <= 1 && b == (a ? 9u : 3u) && c == b && d == 5 && e1 == b && f == b && g == b;
    else if (!strcmp(e->name, "blocks/05/heap_coherence"))
        ok = sscanf(text, "control=%u high=%u value=%u expected=%u %c", &a, &b, &c, &d, &tail) ==
                 4 &&
             a <= 1 && b <= 1 && c == d && ((c >> 8) & 1u) == b;
    else if (!strcmp(e->name, "blocks/05/heap_iterations"))
        ok = sscanf(text, "rounds=%u output=%u sum=%u %c", &a, &b, &c, &tail) == 3 && a == 3 &&
             ((b == 12 && c == 30) || (b == 6 && c == 12));
    else if (!strcmp(e->name, "blocks/05/heap_transfer"))
        ok = sscanf(text, "coin=%u input={%u,%u} output={%u,%u} %c", &a, &b, &c, &d, &e1, &tail) ==
                 5 &&
             a <= 1 && b == (a ? 9u : 3u) && c == 7 && d == b && e1 == 5;
    else if (!strcmp(e->name, "blocks/06/byte_copy"))
        ok = sscanf(text,
                    "choice=%u move=%u,%u,%u copied=%u source=%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &tail) == 6 &&
             a <= 1 && b == 97 && c == 97 && d == (a ? 98u : 99u) && e1 == 20736u + f && f <= 1;
    else if (!strcmp(e->name, "blocks/06/controlled_concat"))
        ok = sscanf(text,
                    "append=%u destination=%u,%u,%u interference=%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &tail) == 5 &&
             a <= 1 && b == 120 && c == (a ? 121u : 0u) && d == (a ? 0u : 90u) && e1 == 1;
    else if (!strcmp(e->name, "blocks/06/decimal_conversion"))
        ok = sscanf(text,
                    "choice=%u text=%7s atoi=%d atol=%d atoll=%d %c",
                    &a,
                    buf,
                    &x,
                    &y,
                    &z,
                    &tail) == 5 &&
             a <= 1 && !strcmp(buf, a ? "47" : "27") && x == (a ? 47 : 27) && y == x && z == x;
    else if (!strcmp(e->name, "blocks/06/saved_search"))
        ok = sscanf(text,
                    "text=%7s position=%d character=%u next=%u has_b=%u %c",
                    buf,
                    &x,
                    &b,
                    &c,
                    &d,
                    &tail) == 5 &&
             ((!strcmp(buf, "aa") && x == 1 && b == 97 && c == 0 && d == 0) ||
              (!strcmp(buf, "ab") && x == 0 && b == 97 && c == 98 && d == 1));
    else if (!strcmp(e->name, "blocks/06/search_positions"))
        ok = sscanf(text,
                    "text=%7s first=%d last=%d raw=%d member=%d substring=%d double=%u b=%u %c",
                    buf,
                    &x,
                    &y,
                    &z,
                    &w,
                    &v1,
                    &a,
                    &b,
                    &tail) == 8 &&
             ((!strcmp(buf, "aa") && x == 0 && y == 1 && z == 0 && w == 0 && v1 == 0 && a == 1 &&
               b == 0) ||
              (!strcmp(buf, "ab") && x == 0 && y == 0 && z == 0 && w == 0 && v1 == 0 && a == 0 &&
               b == 1));
    else if (!strcmp(e->name, "blocks/06/search_write"))
        ok = sscanf(
                 text, "repeated=%u text=%7s old=%c assigned=%c %c", &a, buf, &ch1, &ch2, &tail) ==
                 4 &&
             a <= 1 && !strcmp(buf, a ? "ax" : "xb") && ch1 == 'a' && ch2 == 'x';
    else if (!strcmp(e->name, "blocks/06/string_concat"))
        ok = sscanf(text,
                    "leading=%u concat=%u,%u,%u bounded=%u,%u,%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &g,
                    &tail) == 7 &&
             a <= 1 &&
             ((!a && b == 98 && c == 0 && d == 90) || (a && b == 97 && c == 98 && d == 0)) &&
             e1 == 120 && f == 121 && g == 0;
    else if (!strcmp(e->name, "blocks/06/string_copy"))
        ok = sscanf(text,
                    "source=%u copy=%u,%u,%u padded=%u,%u,%u word=%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &g,
                    &h,
                    &tail) == 8 &&
             ((a == 0 && b == 0 && c == 89 && d == 90 && e1 == 0) ||
              (a == 97 && b == 97 && c == 0 && d == 90 && e1 == 97)) &&
             f == 0 && g == 0 && h == 195;
    else if (!strcmp(e->name, "blocks/06/string_search"))
        ok = sscanf(text,
                    "byte=%u a=%u last=%u digit=%u substring=%u raw=%u %c",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e1,
                    &f,
                    &tail) == 6 &&
             a <= 255 && b == (a == 'a') && c == (a == 'a') && d == (a >= '0' && a <= '9') &&
             e1 == (a == 'a') && f == (a == 'a');
    else if (!strcmp(e->name, "blocks/06/string_spans"))
        ok = sscanf(text, "byte=%u accepted=%u rejected=%u %c", &a, &b, &c, &tail) == 3 &&
             a <= 255 && b == (a >= '0' && a <= '9' ? 1u : 0u) &&
             c == (a == 0 ? 0u : (b ? 0u : 1u));
    else if (!strcmp(e->name, "float_double"))
        ok = !strcmp(text, "coin=0 neg=1 fsum=-1.75 dprod=1.25 interference=1\n") ||
             !strcmp(text, "coin=1 neg=0 fsum=2.00 dprod=8.00 interference=1\n");
    else if (!strcmp(e->name, "everyday_vector_chain"))
        ok = !strcmp(text, "alt=0 s1=7 total=42 items=8,9,3 interference=1\n") ||
             !strcmp(text, "alt=1 s1=10 total=57 items=14,12,3 interference=1\n");
    else if (!strcmp(e->name, "everyday_matrix_struct"))
        ok = !strcmp(text, "qi=0 sum=21 before=10 updated=7 tag=1\n") ||
             !strcmp(text, "qi=1 sum=21 before=20 updated=10 tag=2\n");
    else if (!strcmp(e->name, "everyday_buffers_strings"))
        ok = !strcmp(text, "sel=0 parsed=27 off=3 eq=1 src=27! buf=+27!\n") ||
             !strcmp(text, "sel=1 parsed=47 off=3 eq=1 src=47! buf=+47!\n");
    else if (!strcmp(e->name, "everyday_dynamic_vector"))
        ok = !strcmp(text, "branch=0 count=4 sum=19 elements=3,5,7,4\n") ||
             !strcmp(text, "branch=1 count=4 sum=32 elements=7,6,8,11\n");
    else if (!strcmp(e->name, "everyday_control_phase"))
        ok = !strcmp(text, "mode=0 res=12 count=0 x=3 interference=1\n") ||
             !strcmp(text, "mode=1 res=10 count=2 x=3 interference=1\n");
    else if (!strcmp(e->name, "everyday_math_geometry"))
        ok = !strcmp(text,
                     "branch=0 hypot=2.50 span=3.50 sc=0.50 fl=0 ce=1 tr=0 rd=1 sbit=1 ord=1 "
                     "interference=1\n") ||
             !strcmp(text,
                     "branch=1 hypot=5.00 span=1.00 sc=-2.50 fl=1 ce=2 tr=-2 rd=-3 sbit=0 ord=1 "
                     "interference=1\n");
    if (!ok)
        fprintf(stderr, "Unexpected stdout: %s\n", text);
    free(text);
    require(v, ok, e->name, "output");
}

static void
check_inputs(validation *v, const example *e, char *binary, const char *input, const char *output) {
    for (unsigned i = 0; i < sizeof input_cases / sizeof *input_cases; ++i) {
        const input_case *c = &input_cases[i];
        if (strcmp(c->name, e->name))
            continue;
        validation_write(input, c->input);
        char *run[] = {binary, NULL};
        require(v,
                validation_run_input(v, run, input, output, 90) == c->status,
                e->name,
                "boundary/input status");
        char *text = validation_read(output);
        require(v, !strcmp(text, c->output), e->name, "boundary/input output");
        free(text);
        if (c->status) {
            char *log = validation_read(v->log);
            require(
                v, strstr(log, "Expected a count") != NULL, e->name, "invalid input diagnostic");
            free(log);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: test_examples");
    validation v;
    validation_init(&v, argv[0]);
    char driver[4096], backend[4096], source[4096], binary[4096], output[4096];
    char input[4096], artifact[4096], ir_artifact[4096], bindings_path[4096], seed[32];
    validation_path(driver, sizeof driver, "%s/build/cket", v.root);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v.root);
    validation_path(binary, sizeof binary, "%s/program", v.temp);
    validation_path(output, sizeof output, "%s/stdout", v.temp);
    validation_path(input, sizeof input, "%s/stdin", v.temp);
    validation_path(artifact, sizeof artifact, "%s/artifact", v.temp);
    validation_path(ir_artifact, sizeof ir_artifact, "%s/artifact.ir.json", v.temp);
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    if (setenv("CKET_SEED", "7", 1))
        validation_fail("cannot set example seed");
    for (unsigned i = 0; i < sizeof examples / sizeof *examples; ++i) {
        const example *e = &examples[i];
        validation_path(source, sizeof source, "%s/examples/%s.c", v.root, e->name);
        if (e->sparse ? setenv("CKET_SIMULATOR", "sparse", 1) : unsetenv("CKET_SIMULATOR"))
            validation_fail("cannot set example simulator mode");
        for (unsigned level = 0; level < sizeof levels / sizeof *levels; ++level) {
            char *compile[] = {driver, source, (char *)levels[level], "-o", binary, NULL};
            require(&v, !validation_run(&v, compile, NULL, 90), e->name, levels[level]);
            validation_write(input, sample_input(e));
            char *run[] = {binary, NULL};
            int result = validation_run_input(&v, run, input, output, 90);
            require(&v, result == (e->runtime ? 0 : 1), e->name, "execution status");
            if (e->runtime)
                check_output(&v, e, output);
            else {
                char *log = validation_read(v.log);
                require(&v, strstr(log, "24") != NULL, e->name, "simulator limit diagnostic");
                free(log);
            }
            check_inputs(&v, e, binary, input, output);
        }
        const char *actions[] = {"--emit-ast", "--emit-ir", "--emit-qasm", "--resources"};
        for (unsigned action = 0; action < sizeof actions / sizeof *actions; ++action) {
            int result = 1;
            const char *artifact_path = action == 1 ? ir_artifact : artifact;
            if (e->bindings && action >= 2) {
                validation_path(bindings_path,
                                sizeof bindings_path,
                                "%s/examples/%s.bindings.json",
                                v.root,
                                e->name);
                char *export[] = {backend,
                                  (char *)actions[action],
                                  ir_artifact,
                                  "--bindings",
                                  bindings_path,
                                  NULL};
                result = validation_run(&v, export, artifact, 90);
            } else if (e->ctype_c && action >= 2) {
                char *export[] = {driver,
                                  source,
                                  (char *)actions[action],
                                  "--ctype-locale",
                                  "C",
                                  "-o",
                                  (char *)artifact_path,
                                  NULL};
                result = validation_run(&v, export, NULL, 90);
            } else {
                char *export[] = {
                    driver, source, (char *)actions[action], "-o", (char *)artifact_path, NULL};
                result = validation_run(&v, export, NULL, 90);
            }
            require(&v,
                    e->static_export || action < 2 ? result == 0 : result == 1,
                    e->name,
                    actions[action]);
            if (!result) {
                char *text = validation_read(artifact_path);
                require(&v, *text != 0, e->name, "nonempty artifact");
                free(text);
            }
            if (action == 1) {
                char *verify[] = {backend, "--validate", ir_artifact, NULL};
                require(&v, !validation_run(&v, verify, NULL, 90), e->name, "IR verification");
            }
        }
        printf("PASS example %-28s: five GCC levels, runtime contract and exports\n", e->name);
    }

    /* Repeat Bell measurements, checking correlations rather than one PRNG outcome. */
    if (unsetenv("CKET_SIMULATOR"))
        validation_fail("cannot reset example simulator mode");
    validation_path(source, sizeof source, "%s/examples/entanglement.c", v.root);
    unsigned outcomes = 0;
    for (unsigned s = 0; s < 16; ++s) {
        snprintf(seed, sizeof seed, "%u", s);
        char *run[] = {driver, source, "--run", "--seed", seed, "-o", binary, NULL};
        require(&v, !validation_run(&v, run, output, 90), "entanglement", "seeded --run");
        check_output(&v, &examples[1], output);
        char *text = validation_read(output);
        outcomes |= 1u << (unsigned)strtoul(text, NULL, 10);
        free(text);
    }
    require(&v, outcomes == 3, "entanglement", "both correlated outcomes in fixed seed corpus");

    /* Check stochastic contracts over many seeds, rather than pinning samples. */
    for (unsigned i = 0; i < sizeof examples / sizeof *examples; ++i) {
        const example *e = &examples[i];
        if (!e->runtime || e->expected || !strcmp(e->name, "entanglement"))
            continue;
        if (e->sparse ? setenv("CKET_SIMULATOR", "sparse", 1) : unsetenv("CKET_SIMULATOR"))
            validation_fail("cannot set example simulator mode");
        validation_path(source, sizeof source, "%s/examples/%s.c", v.root, e->name);
        char *compile[] = {driver, source, "-o", binary, NULL};
        require(&v, !validation_run(&v, compile, NULL, 90), e->name, "seed corpus fixture");
        unsigned branches = 0;
        for (unsigned s = 0; s < 16; ++s) {
            snprintf(seed, sizeof seed, "%u", s);
            if (setenv("CKET_SEED", seed, 1))
                validation_fail("cannot set example seed");
            char *run[] = {binary, NULL};
            require(&v, !validation_run(&v, run, output, 90), e->name, "seed corpus execution");
            check_output(&v, e, output);
            if (!strcmp(e->name, "coherent_accumulate")) {
                char *text = validation_read(output);
                unsigned flag;
                if (sscanf(text, "flag=%u", &flag) != 1)
                    validation_fail("missing coherent flag");
                branches |= 1u << flag;
                free(text);
            }
        }
        if (!strcmp(e->name, "coherent_accumulate"))
            require(&v, branches == 3, e->name, "both coherent branch outcomes");
    }
    if (unsetenv("CKET_SIMULATOR"))
        validation_fail("cannot restore example simulator mode");
    if (setenv("CKET_SEED", "7", 1))
        validation_fail("cannot restore example seed");

    validation_path(source, sizeof source, "%s/examples/dynamic_vector.c", v.root);
    char *compile[] = {driver, source, "-o", binary, NULL};
    require(&v, !validation_run(&v, compile, NULL, 90), "dynamic_vector", "input fixture");
    const char *inputs[] = {"1\n", "2\n", "no\n", ""};
    for (unsigned i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        validation_write(input, inputs[i]);
        char *run[] = {binary, NULL};
        require(&v,
                validation_run_input(&v, run, input, output, 90) == (i ? 1 : 0),
                "dynamic_vector",
                "second index, out of bounds, malformed input and EOF");
        char *text = validation_read(output);
        require(&v, !strcmp(text, i ? "" : "9\n"), "dynamic_vector", "input output");
        free(text);
    }

    /* A caller's working directory must not determine header/plugin/module locations. */
    validation remote = v;
    strcpy(remote.root, v.temp);
    validation_path(source, sizeof source, "%s/examples/circuit_module.c", v.root);
    char launcher[4096];
    validation_path(launcher, sizeof launcher, "%s/cket", v.root);
    char *away[] = {"bash", launcher, source, "--run", "--seed", "7", "-o", binary, NULL};
    require(&v,
            !validation_run(&remote, away, output, 90),
            "circuit_module",
            "launcher outside repository");
    check_output(&v, &examples[10], output);
    const char *options[] = {"--help", "--version"};
    for (unsigned i = 0; i < sizeof options / sizeof *options; ++i) {
        char *command[] = {driver, (char *)options[i], NULL};
        require(&v, !validation_run(&v, command, output, 30), "driver", options[i]);
        char *text = validation_read(output);
        int informative = i ? !strcmp(text, "|c> / cket 1.0.0-rc.3 (experimental GCC + native C)\n")
                            : strstr(text, "usage:") != NULL;
        require(&v, informative, "driver", "CLI information");
        free(text);
    }
    char *bad_seed[] = {driver, source, "--run", "--seed", "-1", NULL};
    require(&v, validation_run(&v, bad_seed, NULL, 30) == 1, "driver", "invalid seed");
    char *conflict[] = {driver, source, "--run", "--emit-ir", NULL};
    require(&v, validation_run(&v, conflict, NULL, 30) == 1, "driver", "conflicting actions");
    printf(
        "\n%u checks passed across %zu examples, five optimization levels and CLI/input contracts\n",
        checks,
        sizeof examples / sizeof *examples);
    validation_cleanup(&v);
    return 0;
}
