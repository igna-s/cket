#include <quantum.h>
#include <stdint.h>
#include <stdio.h>

struct Cell {
    uint8_t tag;
    uint16_t weight;
};

int main(void) {
    quant uint8_t grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
    quant struct Cell cells[2] = {{1, 10}, {2, 20}};
    quant bool qi = superposition(0, 1);

    quant uint8_t cpu_sum = 0;
    for (unsigned r = 0; r < 2; ++r)
        for (unsigned c = 0; c < 3; ++c)
            cpu_sum += grid[r][c];

    quant uint16_t before = cells[qi].weight++;
    quant uint8_t updated = (grid[qi][2] += 4);
    quant uint8_t tag_val = cells[qi].tag;

    unsigned got_qi = measure(qi);
    unsigned got_sum = measure(cpu_sum);
    unsigned got_before = measure(before);
    unsigned got_updated = measure(updated);
    unsigned got_tag = measure(tag_val);
    unsigned got_w0 = measure(cells[0].weight);
    unsigned got_w1 = measure(cells[1].weight);
    unsigned got_g02 = measure(grid[0][2]);
    unsigned got_g12 = measure(grid[1][2]);
    reset(grid);
    reset(cells[0]);
    reset(cells[1]);

    uint8_t ref_grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
    uint16_t ref_w[2] = {10, 20};
    uint8_t ref_tag[2] = {1, 2};
    uint16_t ref_before = ref_w[got_qi]++;
    uint8_t ref_updated = (ref_grid[got_qi][2] = (uint8_t)(ref_grid[got_qi][2] + 4));
    printf("qi=%u sum=%u before=%u updated=%u tag=%u\n",
           got_qi,
           got_sum,
           got_before,
           got_updated,
           got_tag);
    return got_qi > 1 || got_sum != 21 || got_before != ref_before || got_updated != ref_updated ||
           got_tag != ref_tag[got_qi] || got_w0 != ref_w[0] || got_w1 != ref_w[1] ||
           got_g02 != ref_grid[0][2] || got_g12 != ref_grid[1][2];
}
