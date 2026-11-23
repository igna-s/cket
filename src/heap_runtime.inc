/* Native pointers address classical placeholder allocations. Their payload is
 * owned by engine heap handles; generated C never reads those placeholder bytes. */
typedef struct native_heap {
    void *address;
    size_t bytes, stride;
    unsigned width;
    int sign;
    uint64_t handle;
    struct native_heap *next;
} native_heap;

static native_heap *native_heaps;

static void shutdown_heap(void) {
    while (native_heaps) {
        native_heap *heap = native_heaps;
        native_heaps = heap->next;
        free(heap->address);
        free(heap);
    }
}

static native_heap *native_heap_at(const void *address, int interior) {
    uintptr_t value = (uintptr_t)address;
    for (native_heap *heap = native_heaps; heap; heap = heap->next) {
        uintptr_t base = (uintptr_t)heap->address;
        if (value == base || (interior && value > base && value - base < heap->bytes &&
                              (value - base) % heap->stride == 0))
            return heap;
    }
    return NULL;
}

uint64_t __cket_heap_identity(const void *address) {
    native_heap *heap = native_heap_at(address, 1);
    return heap ? heap->handle : 0;
}

uint64_t __cket_heap_offset(const void *address) {
    native_heap *heap = native_heap_at(address, 1);
    return heap ? ((uintptr_t)address - (uintptr_t)heap->address) / heap->stride : 0;
}

/* Byte views include unaligned addresses and the one-past address. Only the
 * engine checks the requested extent; these queries never read the payload. */
static native_heap *native_heap_byte_at(const void *address) {
    uintptr_t value = (uintptr_t)address;
    for (native_heap *heap = native_heaps; heap; heap = heap->next) {
        uintptr_t base = (uintptr_t)heap->address;
        if (value >= base && value - base <= heap->bytes)
            return heap;
    }
    return NULL;
}

uint64_t __cket_heap_byte_identity(const void *address) {
    native_heap *heap = native_heap_byte_at(address);
    return heap ? heap->handle : 0;
}

uint64_t __cket_heap_byte_offset(const void *address) {
    native_heap *heap = native_heap_byte_at(address);
    return heap ? (uintptr_t)address - (uintptr_t)heap->address : 0;
}

void __cket_heap_borrow_check(const void *left, const void *right) {
    native_heap *a = native_heap_at(left, 1), *b = native_heap_at(right, 1);
    if (!a || !b || a == b) {
        fputs("|c> runtime: mutable heap parameter suffixes must be live and disjoint\n", stderr);
        exit(EXIT_FAILURE);
    }
}

void *__cket_heap_allocate(uint64_t count, uint64_t stride, uint64_t width, uint64_t sign) {
    if (!stride || count > SIZE_MAX / stride || count > 4096u || !count)
        return NULL;
    if (!width || width > 64 || sign > 1 || (width == 1 && sign) || stride != (width + 7) / 8) {
        fputs("|c> runtime: invalid quantum heap element layout\n", stderr);
        exit(EXIT_FAILURE);
    }
    size_t bytes = (size_t)(count * stride);
    native_heap *heap = calloc(1, sizeof *heap);
    void *address = calloc(1, bytes);
    if (!heap || !address) {
        free(heap);
        free(address);
        return NULL;
    }
    start_engine();
    uint64_t handle = qc_heap_allocate(engine, (size_t)count, (unsigned)width, (int)sign);
    if (!handle) {
        free(heap);
        free(address);
        if (*qc_error(engine))
            fail();
        return NULL;
    }
    heap->address = address;
    heap->bytes = bytes;
    heap->stride = (size_t)stride;
    heap->width = (unsigned)width;
    heap->sign = (int)sign;
    heap->handle = handle;
    heap->next = native_heaps;
    native_heaps = heap;
    return address;
}

void *__cket_heap_malloc(uint64_t bytes, uint64_t stride, uint64_t width, uint64_t sign) {
    if (!stride || bytes % stride)
        return NULL;
    return __cket_heap_allocate(bytes / stride, stride, width, sign);
}

void *
__cket_heap_calloc(uint64_t count, uint64_t size, uint64_t stride, uint64_t width, uint64_t sign) {
    if (size && count > UINT64_MAX / size)
        return NULL;
    return __cket_heap_malloc(count * size, stride, width, sign);
}

void __cket_heap_free(void *address) {
    if (!address)
        return;
    native_heap **slot = &native_heaps;
    while (*slot && (*slot)->address != address)
        slot = &(*slot)->next;
    if (!*slot) {
        fputs("|c> runtime: free requires the base of a live quantum heap\n", stderr);
        exit(EXIT_FAILURE);
    }
    native_heap *heap = *slot;
    if (!qc_heap_free(engine, heap->handle))
        fail();
    *slot = heap->next;
    free(heap->address);
    free(heap);
}

void *
__cket_heap_realloc(void *address, uint64_t bytes, uint64_t stride, uint64_t width, uint64_t sign) {
    if (!address)
        return __cket_heap_malloc(bytes, stride, width, sign);
    native_heap *heap = native_heap_at(address, 0);
    if (!heap || heap->stride != stride || heap->width != width || heap->sign != (int)sign) {
        fputs("|c> runtime: realloc requires a compatible live quantum heap base\n", stderr);
        exit(EXIT_FAILURE);
    }
    if (!bytes) {
        __cket_heap_free(address);
        return NULL;
    }
    if (!stride || bytes % stride || bytes / stride > 4096u || bytes > SIZE_MAX)
        return NULL;
    void *replacement = calloc(1, (size_t)bytes);
    if (!replacement)
        return NULL;
    if (!qc_heap_resize(engine, heap->handle, (size_t)(bytes / stride))) {
        free(replacement);
        if (*qc_error(engine))
            fail();
        return NULL;
    }
    free(heap->address);
    heap->address = replacement;
    heap->bytes = (size_t)bytes;
    return replacement;
}
