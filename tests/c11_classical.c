/* Finite integration smoke test of the 29 header families on the declared
 * GCC/libc target. CPU operations precede an explicit quantum boundary.
 * This is neither an exhaustive libc test nor a quantum-library certificate. */
#include <assert.h>
#include <complex.h>
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <iso646.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <tgmath.h>
#include <threads.h>
#include <time.h>
#include <uchar.h>
#include <wchar.h>
#include <wctype.h>
#ifdef __CKET__
#include <quantum.h>
#endif

static_assert(CHAR_BIT == 8, "This campaign targets eight-bit bytes");
static_assert(alignof(max_align_t) >= alignof(int), "Target alignment");

struct product {
    int a;
    double b;
};

static_assert(offsetof(struct product, b) >= sizeof(int), "Target product layout");

static int total(int count, ...) {
    va_list args, copy;
    va_start(args, count);
    va_copy(copy, args);
    int sum = 0;
    for (int i = 0; i < count; ++i)
        sum += va_arg(copy, int);
    va_end(copy);
    va_end(args);
    return sum;
}

static int compare(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static once_flag once = ONCE_FLAG_INIT;
static int initialized;

static void initialize(void) {
    initialized = 1;
}

static int worker(void *arg) {
    *(int *)arg = 9;
    return 3;
}

noreturn static void terminate_unused(void) {
    exit(0);
}

static int classical_families(void) {
    int bad = 0;
    bad |= setlocale(LC_ALL, "C") == NULL;
    bad |= total(3, 1, 2, 3) != 6;
    int *values = calloc(4, sizeof *values);
    if (!values)
        return 1;
    values[0] = 4;
    values[1] = 1;
    values[2] = 3;
    values[3] = 2;
    qsort(values, 4, sizeof *values, compare);
    int key = 3;
    bad |= bsearch(&key, values, 4, sizeof *values, compare) == NULL;
    memmove(values + 1, values, 3 * sizeof *values);
    bad |= values[0] != 1 or values[1] != 1 or values[3] != 3;
    int *resized = realloc(values, 8 * sizeof *values);
    if (resized)
        values = resized;
    else
        bad = 1;
    free(values);
    void *aligned = aligned_alloc(16, 32);
    bad |= aligned == NULL;
    if (aligned) {
        bad |= (uintptr_t)aligned % 16 != 0;
        memset(aligned, 0, 32);
        free(aligned);
    }
    char text[64], copied[64];
    snprintf(text, sizeof text, "%" PRIdMAX, (intmax_t)123);
    memcpy(copied, text, strlen(text) + 1);
    bad |= strcmp(copied, "123") != 0;
    char *end;
    errno = 0;
    bad |= strtoimax(text, &end, 10) != 123 or *end != '\0' or errno != 0;
    bad |= abs(-7) != 7 or labs(-7L) != 7 or llabs(-7LL) != 7 or imaxabs(-7) != 7;
    div_t quotient = div(-7, 3);
    bad |= quotient.quot != -2 or quotient.rem != -1;
    bad |= !isalpha((unsigned char)'A') or toupper((unsigned char)'b') != 'B';
    bad |= !iswctype(L'A', wctype("alpha")) or towctrans(L'b', wctrans("toupper")) != L'B';
    bad |= wcslen(L"abc") != 3 or wcscmp(L"a", L"a") != 0;
    wchar_t wide[4];
    wmemcpy(wide, L"abc", 4);
    bad |= wcscmp(wide, L"abc") != 0;
    mbstate_t state = {0};
    char16_t c16 = 0;
    char32_t c32 = 0;
    char encoded[MB_LEN_MAX];
    bad |= mbrtoc16(&c16, "A", 1, &state) != 1 or c16 != u'A';
    memset(&state, 0, sizeof state);
    bad |= c16rtomb(encoded, u'A', &state) != 1 or encoded[0] != 'A';
    memset(&state, 0, sizeof state);
    bad |= mbrtoc32(&c32, "B", 1, &state) != 1 or c32 != U'B';
    memset(&state, 0, sizeof state);
    bad |= c32rtomb(encoded, U'B', &state) != 1 or encoded[0] != 'B';
    int rounding = fegetround();
    bad |= fesetround(FE_TONEAREST) != 0 or fegetround() != FE_TONEAREST;
    bad |= fabs(sqrt(4.0) - 2.0) > DBL_EPSILON or !isfinite(2.0);
    double complex z = CMPLX(3.0, 4.0);
    bad |= fabs(cabs(z) - 5.0) > DBL_EPSILON;
    bad |= creal(conj(z)) != 3.0 or cimag(conj(z)) != -4.0;
    if (rounding >= 0)
        bad |= fesetround(rounding) != 0;
    atomic_int counter = ATOMIC_VAR_INIT(1);
    bad |= atomic_fetch_add_explicit(&counter, 2, memory_order_relaxed) != 1;
    bad |= atomic_load_explicit(&counter, memory_order_acquire) != 3;
    call_once(&once, initialize);
    call_once(&once, initialize);
    bad |= initialized != 1;
    int answer = 0, code = 0;
    thrd_t thread;
    if (thrd_create(&thread, worker, &answer) != thrd_success)
        bad = 1;
    else
        bad |= thrd_join(thread, &code) != thrd_success or answer != 9 or code != 3;
    FILE *file = tmpfile();
    if (!file)
        bad = 1;
    else {
        bad |= fwrite("abc", 1, 3, file) != 3;
        rewind(file);
        bad |= fgetc(file) != 'a';
        bad |= fclose(file) != 0;
    }
    jmp_buf jump;
    volatile int jumped = 0;
    if (setjmp(jump) == 0)
        longjmp(jump, 1);
    else
        jumped = 1;
    bad |= jumped != 1;
    void (*handler)(int) = signal(SIGABRT, SIG_IGN);
    if (handler == SIG_ERR)
        bad = 1;
    else
        bad |= signal(SIGABRT, handler) == SIG_ERR;
    struct timespec now;
    bad |= timespec_get(&now, TIME_UTC) != TIME_UTC;
    struct tm date = {.tm_year = 120, .tm_mon = 0, .tm_mday = 2};
    bad |= strftime(text, sizeof text, "%Y-%m-%d", &date) != 10 or strcmp(text, "2020-01-02") != 0;
    assert(sizeof(size_t) >= sizeof(unsigned));
    (void)terminate_unused;
    return bad;
}

int main(void) {
    int bad = classical_families();
#ifdef __CKET__
    quant uint8_t value = 7;
    value += 5;
    unsigned result = measure(value);
#else
    uint8_t value = 7;
    value += 5;
    unsigned result = value;
#endif
    if (bad or result != 12)
        return 1;
    puts("C11 classical families + explicit quantum boundary: OK");
    return 0;
}
