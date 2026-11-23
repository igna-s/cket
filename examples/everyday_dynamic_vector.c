#include <quantum.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static quant uint16_t accumulate_and_bump(quant uint16_t *vec,
                                          size_t count,
                                          quant const uint16_t step) {
    quant uint16_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        vec[i] += step;
        total += vec[i];
    }
    return total;
}

int main(void) {
    size_t initial = 2;
    size_t final_count = 4;
    quant uint16_t *vec = calloc(initial, sizeof *vec);
    if (!vec)
        return 2;

    quant bool branch = superposition(0, 1);
    vec[0] = branch ? 5u : 2u;
    vec[1] = 4u;

    quant uint16_t *grown = realloc((void *)vec, final_count * sizeof *vec);
    if (!grown) {
        measure(vec[0]);
        measure(vec[1]);
        measure(branch);
        free((void *)vec);
        return 3;
    }
    vec = grown;
    vec[2] = 6u;
    vec[3] = branch ? 9u : 3u;

    quant uint16_t step = branch ? 2u : 1u;
    quant uint16_t sum = accumulate_and_bump(vec, final_count, step);

    unsigned got_branch = measure(branch);
    uint16_t got_step = measure(step);
    uint16_t got_sum = measure(sum);
    uint16_t got[4];
    for (size_t i = 0; i < final_count; ++i)
        got[i] = measure(vec[i]);
    free((void *)vec);

    uint16_t ref[4] = {got_branch ? 5u : 2u, 4u, 6u, got_branch ? 9u : 3u};
    uint16_t ref_step = got_branch ? 2u : 1u;
    uint16_t ref_sum = 0;
    for (size_t i = 0; i < final_count; ++i) {
        ref[i] = (uint16_t)(ref[i] + ref_step);
        ref_sum = (uint16_t)(ref_sum + ref[i]);
    }
    printf("branch=%u count=%zu sum=%u elements=%u,%u,%u,%u\n",
           got_branch,
           final_count,
           (unsigned)got_sum,
           (unsigned)got[0],
           (unsigned)got[1],
           (unsigned)got[2],
           (unsigned)got[3]);
    return got_branch > 1 || got_step != ref_step || got_sum != ref_sum || got[0] != ref[0] ||
           got[1] != ref[1] || got[2] != ref[2] || got[3] != ref[3];
}
