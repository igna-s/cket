#include <quantum.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    quant bool choice = superposition(0, 1);
    quant unsigned char bytes[3] = {'a', choice ? 'b' : 'c', 0};
    memmove(bytes + 1, bytes, 2);
    unsigned third = measure(bytes[2]);
    unsigned second = measure(bytes[1]);
    unsigned first = measure(bytes[0]);
    unsigned flag = measure(choice);
    unsigned char expected[3] = {'a', flag ? 'b' : 'c', 0};
    memmove(expected + 1, expected, 2);

    quant unsigned char input = superposition(0, 1);
    quant uint16_t word = 0x5a7f;
    memcpy((void *)&word, (const void *)&input, 1);
    const unsigned char cpu_bytes[2] = {'Q', 0};
    memcpy((unsigned char *)&word + 1, cpu_bytes, 1);
    unsigned copied = measure(word);
    unsigned source = measure(input);
    uint16_t reference = 0x5a7f;
    unsigned char source_byte = (unsigned char)source;
    memcpy(&reference, &source_byte, 1);
    memcpy((unsigned char *)&reference + 1, cpu_bytes, 1);

    printf("choice=%u move=%u,%u,%u copied=%u source=%u\n",
           flag,
           first,
           second,
           third,
           copied,
           source);
    return first != expected[0] || second != expected[1] || third != expected[2] ||
           copied != reference || source > 1;
}
