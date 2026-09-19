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
uint64_t __cket_measure_value(uint64_t value)
    __attribute__((cket_intrinsic("measure")));
/* The explicit classical result retains the underlying C integer type. */
#define measure(value) ((__typeof__(value))__cket_measure_value(value))
void phase_flip(void) __attribute__((cket_intrinsic("phase_flip")));
void phase(double radians) __attribute__((cket_intrinsic("phase")));
void fourier(uint64_t value) __attribute__((cket_intrinsic("fourier")));
void inverse_fourier(uint64_t value) __attribute__((cket_intrinsic("inverse_fourier")));
void reset(uint64_t value) __attribute__((cket_intrinsic("reset")));
uint64_t find(uint64_t value, bool predicate) __attribute__((cket_intrinsic("find")));
/* find assumes one solution. The count is a caller-supplied promise. */
uint64_t find_known(uint64_t value, bool predicate, uint64_t solutions)
    __attribute__((cket_intrinsic("find_known")));
/* Measurement-based search: false means the attempt budget was exhausted.
 * For a nonempty domain, false has probability <= (7/8)^attempts. */
bool search(uint64_t value, bool predicate, unsigned attempts)
    __attribute__((cket_intrinsic("search")));
/* Unit-only QIR module, embedded at compile time; typed register borrows. */
void circuit(const char *module, ...) __attribute__((cket_intrinsic("circuit")));
void inverse_circuit(const char *module, ...) __attribute__((cket_intrinsic("inverse_circuit")));

#endif
