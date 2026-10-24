#include <quantum.h>
#include <stdio.h>
#include <stdlib.h>

void scan_heap(quant const uint8_t *values, size_t count, quant uint8_t total) {
    for (size_t i = 0; i < count; ++i)
        total += values[i];
}

int main(int argc, char **argv) {
    size_t count = argc > 1 ? strtoul(argv[1], NULL, 10) : 2;
    if (count < 2 || count > 4)
        return 2;
    size_t initial_count = count;
    quant uint8_t *values = calloc(count, sizeof *values);
    if (!values)
        return 3;
    quant bool choice = superposition(0, 1);
    for (size_t i = 0; i < count; ++i)
        values[i] = i + 1;
    if (choice)
        values[0] = 7;
    quant uint8_t *grown = realloc((void *)values, (count + 1) * sizeof *values);
    bool grew = grown != NULL;
    if (grew) {
        values = grown;
        values[count] = 9;
        ++count;
    }
    quant uint8_t total = 0;
    scan_heap(values, count, total);
    unsigned sum = measure(total), flag = measure(choice);
    unsigned observed = 0;
    for (size_t i = 0; i < count; ++i)
        observed += measure(values[i]);
    free((void *)values);
    unsigned expected =
        (unsigned)(initial_count * (initial_count + 1) / 2) + (grew ? 9 : 0) + (flag ? 6 : 0);
    printf("initial=%zu count=%zu grew=%d flag=%u sum=%u observed=%u\n",
           initial_count,
           count,
           grew,
           flag,
           sum,
           observed);
    return !grew || sum != expected || observed != expected;
}
