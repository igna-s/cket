#include <quantum.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stage_and_parse(quant char *scratch,
                            quant const char *source,
                            quant uint32_t *parsed_out,
                            quant uint8_t *tail_offset,
                            quant bool *equal_prefix) {
    quant char *alias = scratch;
    memcpy((void *)scratch, (const void *)source, 4);
    memmove((void *)(alias + 1), (const void *)scratch, 3);
    alias[0] = '+';
    alias[4] = '\0';
    *equal_prefix = strncmp((const char *)source, (const char *)(alias + 1), 2) == 0;
    char *end = NULL;
    *parsed_out = (uint32_t)strtoul((const char *)(alias + 1), &end, 10);
    *tail_offset = (uint8_t)(end - alias);
}

int main(void) {
    quant bool sel = superposition(0, 1);
    quant char source[4] = {sel ? '4' : '2', '7', '!', 0};
    quant char scratch[5] = {0, 0, 0, 0, 0};
    quant uint32_t parsed = 0;
    quant uint8_t end_off = 0;
    quant bool eq = 0;
    stage_and_parse(scratch, source, &parsed, &end_off, &eq);

    unsigned got_sel = measure(sel);
    uint32_t got_parsed = measure(parsed);
    unsigned got_off = measure(end_off);
    unsigned got_eq = measure(eq);
    char got_src[4], got_buf[5];
    for (unsigned i = 0; i < 4; ++i)
        got_src[i] = (char)measure(source[i]);
    for (unsigned i = 0; i < 5; ++i)
        got_buf[i] = (char)measure(scratch[i]);

    char want_src[4] = {got_sel ? '4' : '2', '7', '!', 0};
    char want_buf[5] = {0};
    memcpy(want_buf, want_src, 4);
    memmove(want_buf + 1, want_buf, 3);
    want_buf[0] = '+';
    want_buf[4] = '\0';
    char *cpu_end = NULL;
    uint32_t want_parsed = (uint32_t)strtoul(want_buf + 1, &cpu_end, 10);
    unsigned want_off = (unsigned)(cpu_end - want_buf);

    printf("sel=%u parsed=%u off=%u eq=%u src=%s buf=%s\n",
           got_sel,
           got_parsed,
           got_off,
           got_eq,
           got_src,
           got_buf);
    return got_sel > 1 || got_parsed != want_parsed || got_off != want_off || got_eq != 1 ||
           memcmp(got_src, want_src, 4) != 0 || memcmp(got_buf, want_buf, 5) != 0;
}
