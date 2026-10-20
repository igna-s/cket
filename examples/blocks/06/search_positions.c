#include <quantum.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool repeated = superposition(0, 1);
    quant char text[3] = {'a', repeated ? 'a' : 'b', 0};
    quant int8_t first = strchr(text, 'a') - text;
    quant int8_t last = strrchr(text, 'a') - text;
    quant int8_t raw = (char *)memchr(text, 'a', sizeof text) - text;
    quant int8_t member = strpbrk(text, "ba") - text;
    quant int8_t substring = strstr(text, "a") - text;
    quant bool starts_double = strstr(text, "aa") == text;
    quant bool b_at_second = text + 1 == strchr(text, 'b');
    if (starts_double)
        phase(0.17);

    int observed_first = measure(first);
    int observed_last = measure(last);
    int observed_raw = measure(raw);
    int observed_member = measure(member);
    int observed_substring = measure(substring);
    unsigned observed_double = measure(starts_double);
    unsigned observed_b = measure(b_at_second);
    char reference[3];
    reference[0] = (char)measure(text[0]);
    reference[1] = (char)measure(text[1]);
    reference[2] = (char)measure(text[2]);
    unsigned flag = measure(repeated);
    printf("text=%s first=%d last=%d raw=%d member=%d substring=%d double=%u b=%u\n",
           reference,
           observed_first,
           observed_last,
           observed_raw,
           observed_member,
           observed_substring,
           observed_double,
           observed_b);
    return reference[0] != 'a' || reference[1] != (flag ? 'a' : 'b') || reference[2] ||
           observed_first != strchr(reference, 'a') - reference ||
           observed_last != strrchr(reference, 'a') - reference ||
           observed_raw != (char *)memchr(reference, 'a', sizeof reference) - reference ||
           observed_member != strpbrk(reference, "ba") - reference ||
           observed_substring != strstr(reference, "a") - reference ||
           observed_double != (strstr(reference, "aa") == reference) ||
           observed_b != (reference + 1 == strchr(reference, 'b'));
}
