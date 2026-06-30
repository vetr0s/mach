// Arena allocator: linear bump allocation over a linked list of malloc'd regions.
//
// Modeled on Tsoding's arena.h (https://github.com/tsoding/arena). An allocation
// bumps a cursor inside the current region; when a region fills, a larger one is
// chained on. Individual allocations are never freed on their own — you reset the
// arena (keep the memory, reuse it) or free it whole. That trades fine-grained
// frees for near-zero bookkeeping and no fragmentation, which fits allocations
// that share a lifetime: a world, a level, per-frame scratch.

#ifndef ARENA_H
#define ARENA_H

#include "../base/base.h"

typedef struct Arena_Region Arena_Region;

// A region's storage is measured in words (uintptr_t), not bytes, so the bump
// cursor stays word-aligned and every allocation comes back aligned for free.
struct Arena_Region {
    Arena_Region *next;
    usize count;        // words handed out
    usize capacity;     // words available
    uintptr_t data[];   // region storage (flexible array member)
};

// Zero-initialize to create an empty arena: Arena a = {0};
typedef struct {
    Arena_Region *begin;
    Arena_Region *end;
} Arena;

// Hand back `size` bytes from the arena, chaining on a new region if the current
// one is full. The result is uintptr_t-aligned. Memory is NOT zeroed. Returns
// NULL only if the backing malloc fails.
void *arena_alloc(Arena *a, usize size);

// Mark every region empty so its memory is reused by later allocations, without
// returning anything to the OS. Pointers from before the reset become garbage.
void arena_reset(Arena *a);

// Return every region to the OS and leave the arena empty (safe to reuse).
void arena_free(Arena *a);

#endif
