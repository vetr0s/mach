// World implementation. Included into mach.c (not compiled separately).

#include "world.h"
#include <stdlib.h>

static World *g_world = NULL;

World* world_create(void) {
    World *w = (World *)malloc(sizeof(World));
    if (!w) return NULL;

    w->entity_count = 0;
    w->tick = 0;

    for (i32 y = 0; y < 256; y++) {
        for (i32 x = 0; x < 256; x++) {
            w->grid[x][y] = 0;
        }
    }

    return w;
}

void world_destroy(World *w) {
    if (!w) return;
    free(w);
}

static i32 entity_id_from_index(i32 idx) {
    return idx + 1;
}

static i32 entity_index_from_id(i32 id) {
    return id - 1;
}

i32 world_spawn_miner(World *w, i32 x, i32 y) {
    if (!w || w->entity_count >= MAX_ENTITIES) return 0;
    if (x < 0 || x >= 256 || y < 0 || y >= 256) return 0;
    if (w->grid[x][y] != 0) return 0;  // Space occupied

    i32 idx = w->entity_count++;
    Entity *e = &w->entities[idx];

    e->type = ENTITY_MINER;
    e->data.miner.grid_x = x;
    e->data.miner.grid_y = y;
    e->data.miner.ore_produced = 0;
    e->data.miner.ore_stored = 0;
    e->data.miner.state = ENTITY_STATE_IDLE;

    w->grid[x][y] = entity_id_from_index(idx);

    return entity_id_from_index(idx);
}

i32 world_spawn_storage(World *w, i32 x, i32 y) {
    if (!w || w->entity_count >= MAX_ENTITIES) return 0;
    if (x < 0 || x >= 256 || y < 0 || y >= 256) return 0;
    if (w->grid[x][y] != 0) return 0;  // Space occupied

    i32 idx = w->entity_count++;
    Entity *e = &w->entities[idx];

    e->type = ENTITY_STORAGE;
    e->data.storage.grid_x = x;
    e->data.storage.grid_y = y;
    e->data.storage.ore_capacity = 100;
    e->data.storage.ore_stored = 0;

    w->grid[x][y] = entity_id_from_index(idx);

    return entity_id_from_index(idx);
}

void world_despawn(World *w, i32 entity_id) {
    if (!w || entity_id == 0) return;

    i32 idx = entity_index_from_id(entity_id);
    if (idx < 0 || idx >= w->entity_count) return;

    Entity *e = &w->entities[idx];
    w->grid[e->data.miner.grid_x][e->data.miner.grid_y] = 0;

    // Swap with last entity
    if (idx != w->entity_count - 1) {
        w->entities[idx] = w->entities[w->entity_count - 1];
        Entity *moved = &w->entities[idx];
        w->grid[moved->data.miner.grid_x][moved->data.miner.grid_y] = entity_id_from_index(idx);
    }

    w->entity_count--;
}

i32 world_get_entity_at(World *w, i32 x, i32 y) {
    if (!w || x < 0 || x >= 256 || y < 0 || y >= 256) return 0;
    return w->grid[x][y];
}

Entity* world_get_entity(World *w, i32 entity_id) {
    if (!w || entity_id == 0) return NULL;

    i32 idx = entity_index_from_id(entity_id);
    if (idx < 0 || idx >= w->entity_count) return NULL;

    return &w->entities[idx];
}

void world_tick(World *w) {
    if (!w) return;

    w->tick++;

    // Tick all miners: produce ore
    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];
        if (e->type != ENTITY_MINER) continue;

        e->data.miner.state = ENTITY_STATE_WORKING;
        e->data.miner.ore_produced = 1;
        e->data.miner.ore_stored += 1;

        // Try to push ore to adjacent storage
        for (i32 dy = -1; dy <= 1; dy++) {
            for (i32 dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                if (abs(dx) + abs(dy) > 1) continue;  // Only cardinal directions

                i32 nx = e->data.miner.grid_x + dx;
                i32 ny = e->data.miner.grid_y + dy;

                i32 neighbor_id = world_get_entity_at(w, nx, ny);
                if (neighbor_id == 0) continue;

                Entity *neighbor = world_get_entity(w, neighbor_id);
                if (!neighbor || neighbor->type != ENTITY_STORAGE) continue;

                // Transfer ore
                i32 transfer = (e->data.miner.ore_stored > 0 && neighbor->data.storage.ore_stored < neighbor->data.storage.ore_capacity) ? 1 : 0;
                if (transfer) {
                    e->data.miner.ore_stored -= 1;
                    neighbor->data.storage.ore_stored += 1;
                }
            }
        }

        if (e->data.miner.ore_stored == 0) {
            e->data.miner.state = ENTITY_STATE_IDLE;
        }
    }
}
