#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant unsigned char text[2] = {superposition(0, 255), 0};
    quant bool has_a = strchr((const char *)text, 'a') != NULL;
    quant bool has_last_a = strrchr((const char *)text, 'a') != NULL;
    quant bool digit = strpbrk((const char *)text, "0123456789") != NULL;
    quant bool substring = strstr((const char *)text, "a") != NULL;
    quant bool raw_a = memchr(text, 'a', 1) != NULL;

    if (digit)
        phase(0.17);
    if (has_a)
        phase(0.23);

    unsigned observed_a = measure(has_a);
    unsigned observed_last = measure(has_last_a);
    unsigned observed_digit = measure(digit);
    unsigned observed_substring = measure(substring);
    unsigned observed_raw = measure(raw_a);
    unsigned byte = measure(text[0]);
    unsigned tail = measure(text[1]);
    char reference[2] = {(char)byte, 0};
    printf("byte=%u a=%u last=%u digit=%u substring=%u raw=%u\n",
           byte,
           observed_a,
           observed_last,
           observed_digit,
           observed_substring,
           observed_raw);
    return tail || observed_a != (strchr(reference, 'a') != NULL) ||
           observed_last != (strrchr(reference, 'a') != NULL) ||
           observed_digit != (strpbrk(reference, "0123456789") != NULL) ||
           observed_substring != (strstr(reference, "a") != NULL) ||
           observed_raw != (memchr(reference, 'a', 1) != NULL);
}
