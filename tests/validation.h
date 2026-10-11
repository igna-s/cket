#ifndef CKET_VALIDATION_H
#define CKET_VALIDATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct { char root[4096], temp[4096], log[4096]; } validation;
void validation_init(validation *v, const char *executable);
void validation_path(char *out, size_t size, const char *format, ...);
int validation_run(validation *v, char *const argv[], const char *output, unsigned timeout);
char *validation_read(const char *path);
void validation_write(const char *path, const char *text);
void validation_cleanup(validation *v);
void validation_fail(const char *format, ...);
uint64_t validation_random(uint64_t *state);
uint64_t validation_bounded(uint64_t *state, uint64_t bound);

#endif
