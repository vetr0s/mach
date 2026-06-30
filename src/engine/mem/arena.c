// Arena allocator implementation (included into mach.c).

#include "arena.h"
#include "../debug.h"
#include <stdlib.h>

// (npt): Default region size in words. 8K words is 64 KiB on a 64-bit target —
// big enough that most arenas live in one region, small enough to not over-commit.
#define ARENA_REGION_CAPACITY (8 * 1024)

static Arena_Region *region_new(usize capacity) {
    usize bytes = sizeof(Arena_Region) + sizeof(uintptr_t) * capacity;
    Arena_Region *r = (Arena_Region *)malloc(bytes);
    if (!r) {
        LOG_ERROR("arena: region allocation failed (%zu bytes)", bytes);
        return NULL;
    }
    r->next = NULL;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

void *arena_alloc(Arena *a, usize size) {
    // (npt): Round the byte request up to whole words so the next allocation
    // starts word-aligned too.
    usize words = (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    if (a->end == NULL) {
        usize capacity = words > ARENA_REGION_CAPACITY ? words : ARENA_REGION_CAPACITY;
        a->begin = a->end = region_new(capacity);
        if (!a->end) return NULL;
    }

    // After a reset, `end` points at the first region; walk forward over any
    // already-full regions before deciding to allocate a new one.
    while (a->end->count + words > a->end->capacity && a->end->next != NULL) {
        a->end = a->end->next;
    }
    if (a->end->count + words > a->end->capacity) {
        usize capacity = words > ARENA_REGION_CAPACITY ? words : ARENA_REGION_CAPACITY;
        a->end->next = region_new(capacity);
        a->end = a->end->next;
        if (!a->end) return NULL;
    }

    void *result = &a->end->data[a->end->count];
    a->end->count += words;
    return result;
}

void arena_reset(Arena *a) {
    for (Arena_Region *r = a->begin; r != NULL; r = r->next) {
        r->count = 0;
    }
    a->end = a->begin;
}

void arena_free(Arena *a) {
    Arena_Region *r = a->begin;
    while (r != NULL) {
        Arena_Region *next = r->next;
        free(r);
        r = next;
    }
    a->begin = NULL;
    a->end = NULL;
}
