// Entity system and world state management.

#ifndef WORLD_H
#define WORLD_H

#include "../base/base.h"

typedef enum {
    ENTITY_INVALID = 0,
    ENTITY_MINER,
    ENTITY_STORAGE,
} Entity_Type;

// Entity state
typedef enum {
    ENTITY_STATE_IDLE,
    ENTITY_STATE_WORKING,
} Entity_State;

// Miner: extracts ore from deposits
typedef struct {
    i32 grid_x, grid_y;      // Position on grid
    i32 ore_produced;        // Ore produced this tick
    i32 ore_stored;          // Ore buffered in this machine
    Entity_State state;
} Entity_Miner;

// Storage: holds resources
typedef struct {
    i32 grid_x, grid_y;      // Position on grid
    i32 ore_capacity;        // Max ore
    i32 ore_stored;          // Current ore
} Entity_Storage;

// Union: all entity data
typedef struct {
    Entity_Type type;
    union {
        Entity_Miner miner;
        Entity_Storage storage;
    } data;
} Entity;

// World state: all entities and game state
#define MAX_ENTITIES 10000

typedef struct {
    Entity entities[MAX_ENTITIES];
    i32 entity_count;

    // Grid state: what's at each position
    // 0 = empty, >0 = entity index + 1
    i32 grid[256][256];

    // Time tracking
    i32 tick;
} World;

// API
World* world_create(void);
void world_destroy(World *w);

void world_tick(World *w);

// Entity management
i32 world_spawn_miner(World *w, i32 x, i32 y);
i32 world_spawn_storage(World *w, i32 x, i32 y);
void world_despawn(World *w, i32 entity_id);

// Query
i32 world_get_entity_at(World *w, i32 x, i32 y);
Entity* world_get_entity(World *w, i32 entity_id);

#endif
