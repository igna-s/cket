#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant unsigned char text[2] = {superposition(0, 255), 0};
    quant uint8_t accepted = strspn((const char *)text, "0123456789");
    quant uint8_t rejected = strcspn((const char *)text, "0123456789");
    if (accepted)
        phase(.17);
    if (rejected)
        phase(.23);

    unsigned observed_accepted = measure(accepted);
    unsigned observed_rejected = measure(rejected);
    unsigned char observed = measure(text[0]);
    unsigned tail = measure(text[1]);
    char reference[2] = {(char)observed, 0};
    printf("byte=%u accepted=%u rejected=%u\n",
           (unsigned)observed,
           observed_accepted,
           observed_rejected);
    return tail != 0 || observed_accepted != strspn(reference, "0123456789") ||
           observed_rejected != strcspn(reference, "0123456789");
}
