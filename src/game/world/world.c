// World implementation (included into mach.c).

#include "world.h"
#include "../../engine/debug.h"

// Entity ID <-> array index conversion. IDs are 1-based so that 0 can mean
// "empty" in the grid.
static i32 entity_id_from_index(i32 idx) {
    return idx + 1;
}

static i32 entity_index_from_id(i32 id) {
    return id - 1;
}

// Read the grid position of any entity. Miner and Storage both store grid_x/
// grid_y as their first two fields, but switching on type keeps this correct if
// that ever changes.
void entity_grid_pos(const Entity *e, i32 *out_x, i32 *out_y) {
    if (!e) { *out_x = 0; *out_y = 0; return; }
    switch (e->type) {
    case ENTITY_MINER:
        *out_x = e->data.miner.grid_x;
        *out_y = e->data.miner.grid_y;
        break;
    case ENTITY_STORAGE:
        *out_x = e->data.storage.grid_x;
        *out_y = e->data.storage.grid_y;
        break;
    default:
        *out_x = 0;
        *out_y = 0;
        break;
    }
}

static b32 in_bounds(i32 x, i32 y) {
    return x >= 0 && x < WORLD_GRID_SIZE && y >= 0 && y < WORLD_GRID_SIZE;
}

// Initialize a new world with an empty entity list and cleared grid. The world
// is one block from `arena`; the caller owns the arena and its teardown.
World* world_create(Arena *arena) {
    World *w = (World *)arena_alloc(arena, sizeof(World));
    if (!w) {
        LOG_ERROR("world_create: arena allocation failed (%zu bytes)", sizeof(World));
        return NULL;
    }

    // (npt): arena_alloc doesn't zero, so clear the whole struct — the grid must
    // start empty and entity_count at zero.
    memset(w, 0, sizeof(*w));

    LOG_INFO("world created (capacity %d entities, %dx%d grid)",
             MAX_ENTITIES, WORLD_GRID_SIZE, WORLD_GRID_SIZE);
    return w;
}

// Shared spawn path: validate, claim the cell, return the fresh entity slot.
// Returns NULL on failure (with the entity_count left untouched).
static Entity* world_spawn(World *w, i32 x, i32 y, Entity_Type type) {
    if (!w || w->entity_count >= MAX_ENTITIES) {
        LOG_DEBUG("spawn rejected: world full (%d/%d)",
                  w ? w->entity_count : -1, MAX_ENTITIES);
        return NULL;
    }
    if (!in_bounds(x, y)) {
        LOG_DEBUG("spawn rejected: out of bounds (%d,%d)", x, y);
        return NULL;
    }
    if (w->grid[x][y] != 0) {
        LOG_DEBUG("spawn rejected: cell occupied (%d,%d)", x, y);
        return NULL;
    }

    i32 idx = w->entity_count++;
    Entity *e = &w->entities[idx];
    e->type = type;
    w->grid[x][y] = entity_id_from_index(idx);
    return e;
}

// Place a miner at grid position (x, y). Returns entity ID on success, 0 on failure.
i32 world_spawn_miner(World *w, i32 x, i32 y) {
    Entity *e = world_spawn(w, x, y, ENTITY_MINER);
    if (!e) return 0;

    e->data.miner.grid_x = x;
    e->data.miner.grid_y = y;
    e->data.miner.ore_produced = 0;
    e->data.miner.ore_stored = 0;
    e->data.miner.state = ENTITY_STATE_IDLE;

    LOG_DEBUG("spawned miner at (%d,%d)", x, y);
    return w->grid[x][y];
}

// Place a storage unit at grid position (x, y). Returns entity ID on success, 0 on failure.
i32 world_spawn_storage(World *w, i32 x, i32 y) {
    Entity *e = world_spawn(w, x, y, ENTITY_STORAGE);
    if (!e) return 0;

    e->data.storage.grid_x = x;
    e->data.storage.grid_y = y;
    e->data.storage.ore_capacity = 100;
    e->data.storage.ore_stored = 0;

    LOG_DEBUG("spawned storage at (%d,%d)", x, y);
    return w->grid[x][y];
}

void world_despawn(World *w, i32 entity_id) {
    if (!w || entity_id == 0) return;

    i32 idx = entity_index_from_id(entity_id);
    if (idx < 0 || idx >= w->entity_count) return;

    i32 ex, ey;
    entity_grid_pos(&w->entities[idx], &ex, &ey);
    w->grid[ex][ey] = 0;

    // Swap-remove: move the last entity into the freed slot and fix its grid ID.
    i32 last = w->entity_count - 1;
    if (idx != last) {
        w->entities[idx] = w->entities[last];
        i32 mx, my;
        entity_grid_pos(&w->entities[idx], &mx, &my);
        w->grid[mx][my] = entity_id_from_index(idx);
    }

    w->entity_count--;
    LOG_DEBUG("despawned entity %d at (%d,%d), %d remaining",
              entity_id, ex, ey, w->entity_count);
}

i32 world_get_entity_at(World *w, i32 x, i32 y) {
    if (!w || !in_bounds(x, y)) return 0;
    return w->grid[x][y];
}

// Check if a position is valid for placement.
b32 world_can_place_at(World *w, i32 x, i32 y) {
    if (!w) return MACH_FALSE;
    if (!in_bounds(x, y)) return MACH_FALSE;
    if (w->grid[x][y] != 0) return MACH_FALSE;
    if (w->entity_count >= MAX_ENTITIES) return MACH_FALSE;
    return MACH_TRUE;
}

Entity* world_get_entity(World *w, i32 entity_id) {
    if (!w || entity_id == 0) return NULL;

    i32 idx = entity_index_from_id(entity_id);
    if (idx < 0 || idx >= w->entity_count) return NULL;

    return &w->entities[idx];
}

// Advance world simulation by one tick. Miners produce ore and transfer it to
// adjacent storage (cardinal neighbors only).
void world_tick(World *w) {
    if (!w) return;

    w->tick++;

    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];
        if (e->type != ENTITY_MINER) continue;

        Entity_Miner *miner = &e->data.miner;
        miner->state = ENTITY_STATE_WORKING;
        miner->ore_produced = 1;
        miner->ore_stored += 1;

        // Check the 4 cardinal neighbors for storage to drain into.
        static const i32 offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (i32 n = 0; n < 4; n++) {
            i32 nx = miner->grid_x + offsets[n][0];
            i32 ny = miner->grid_y + offsets[n][1];

            i32 neighbor_id = world_get_entity_at(w, nx, ny);
            if (neighbor_id == 0) continue;

            Entity *neighbor = world_get_entity(w, neighbor_id);
            if (!neighbor || neighbor->type != ENTITY_STORAGE) continue;

            Entity_Storage *storage = &neighbor->data.storage;
            if (miner->ore_stored > 0 && storage->ore_stored < storage->ore_capacity) {
                miner->ore_stored -= 1;
                storage->ore_stored += 1;
            }
        }

        if (miner->ore_stored == 0) {
            miner->state = ENTITY_STATE_IDLE;
        }
    }
}
