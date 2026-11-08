#ifndef CKET_QUANTUM_H
#define CKET_QUANTUM_H

#include <stdbool.h>
#include <stdint.h>

#ifndef __CKET__
#error "Use the |c> compiler (cket), not plain gcc, for quantum.h"
#endif

/* Registered by the GCC plugin; this is not an ignored vendor attribute. */
#define quant __attribute__((cket_quant))
#define cuant quant
#define Cuant quant

/* High-level intrinsics. There is deliberately no classical implementation. */
uint64_t superposition(uint64_t lower, uint64_t upper)
    __attribute__((cket_intrinsic("superposition")));
uint64_t __cket_measure_value(uint64_t value) __attribute__((cket_intrinsic("measure")));
uint64_t __cket_measure_unsupported(uint64_t value)
    __attribute__((cket_intrinsic("unsupported_measurement")));
/* Typed returns preserve the representation despite ABI argument promotions.
 * _Generic and typeof inspect types without evaluating the operand. */
#define CKET_DECLARE_MEASUREMENT(type, suffix) \
    type __cket_measure_##suffix(type value) __attribute__((cket_intrinsic("measure")))
CKET_DECLARE_MEASUREMENT(_Bool, bool);
CKET_DECLARE_MEASUREMENT(char, char);
CKET_DECLARE_MEASUREMENT(signed char, schar);
CKET_DECLARE_MEASUREMENT(unsigned char, uchar);
CKET_DECLARE_MEASUREMENT(short, short);
CKET_DECLARE_MEASUREMENT(unsigned short, ushort);
CKET_DECLARE_MEASUREMENT(int, int);
CKET_DECLARE_MEASUREMENT(unsigned int, uint);
CKET_DECLARE_MEASUREMENT(long, long);
CKET_DECLARE_MEASUREMENT(unsigned long, ulong);
CKET_DECLARE_MEASUREMENT(long long, llong);
CKET_DECLARE_MEASUREMENT(unsigned long long, ullong);
#undef CKET_DECLARE_MEASUREMENT
#define measure(value)                                    \
    ((__typeof__(value))_Generic(*(__typeof__(value) *)0, \
        _Bool: __cket_measure_bool,                       \
        char: __cket_measure_char,                        \
        signed char: __cket_measure_schar,                \
        unsigned char: __cket_measure_uchar,              \
        short: __cket_measure_short,                      \
        unsigned short: __cket_measure_ushort,            \
        int: __cket_measure_int,                          \
        unsigned int: __cket_measure_uint,                \
        long: __cket_measure_long,                        \
        unsigned long: __cket_measure_ulong,              \
        long long: __cket_measure_llong,                  \
        unsigned long long: __cket_measure_ullong,        \
        default: __cket_measure_unsupported)(value))
void phase_flip(void) __attribute__((cket_intrinsic("phase_flip")));
void phase(double radians) __attribute__((cket_intrinsic("phase")));
void fourier(uint64_t value) __attribute__((cket_intrinsic("fourier")));
void inverse_fourier(uint64_t value) __attribute__((cket_intrinsic("inverse_fourier")));
void reset(uint64_t value) __attribute__((cket_intrinsic("reset")));
void __cket_reset_product(const volatile void *value)
    __attribute__((cket_intrinsic("reset_product")));
#define reset(value)                                                     \
    (_Generic(*(__typeof__(value) *)0,                                   \
        _Bool: reset,                                                    \
        char: reset,                                                     \
        signed char: reset,                                              \
        unsigned char: reset,                                            \
        short: reset,                                                    \
        unsigned short: reset,                                           \
        int: reset,                                                      \
        unsigned int: reset,                                             \
        long: reset,                                                     \
        unsigned long: reset,                                            \
        long long: reset,                                                \
        unsigned long long: reset,                                       \
        default: __cket_reset_product)(_Generic(*(__typeof__(value) *)0, \
        _Bool: (value),                                                  \
        char: (value),                                                   \
        signed char: (value),                                            \
        unsigned char: (value),                                          \
        short: (value),                                                  \
        unsigned short: (value),                                         \
        int: (value),                                                    \
        unsigned int: (value),                                           \
        long: (value),                                                   \
        unsigned long: (value),                                          \
        long long: (value),                                              \
        unsigned long long: (value),                                     \
        default: &(value))))
uint64_t find(uint64_t value, bool predicate) __attribute__((cket_intrinsic("find")));
/* find assumes one solution. The count is a caller-supplied promise. */
uint64_t find_known(uint64_t value, bool predicate, uint64_t solutions)
    __attribute__((cket_intrinsic("find_known")));
/* Measurement-based search: false means the attempt budget was exhausted.
 * If a solution exists and trials use independent uniform randomness, false
 * has probability <= (7/8)^attempts. This is not a guarantee for every PRNG
 * seed; false for an empty solution set is expected. */
bool search(uint64_t value, bool predicate, unsigned attempts)
    __attribute__((cket_intrinsic("search")));
/* Unit-only QIR module, embedded at compile time; typed register borrows. */
void circuit(const char *module, ...) __attribute__((cket_intrinsic("circuit")));
void inverse_circuit(const char *module, ...) __attribute__((cket_intrinsic("inverse_circuit")));

#endif
