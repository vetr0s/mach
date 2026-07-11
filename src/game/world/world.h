// Entity system and world state for the value-loop sim.
//
// The game is a belt builder: droppers emit items onto conveyors, which route
// them through upgraders (each raises an item's value once) into furnaces that
// bank the value. world_tick runs that simulation.

#ifndef WORLD_H
#define WORLD_H

#include "mach.h"

typedef enum {
    ENTITY_INVALID = 0,
    ENTITY_DROPPER,  // emits items onto the tile it faces
    ENTITY_CONVEYOR, // carries one item per cell in its facing direction
    ENTITY_UPGRADER, // a conveyor that also multiplies a passing item's value
    ENTITY_FURNACE,  // banks the value of any item that reaches it
} Entity_Type;

// Facing direction in grid space. DIR_DX/DIR_DY in world.c map these to deltas.
typedef enum {
    DIR_N = 0,
    DIR_E,
    DIR_S,
    DIR_W,
    DIR_COUNT,
} Direction;

// Grid deltas for each Direction, indexed by the enum.
extern const i32 DIR_DX[DIR_COUNT];
extern const i32 DIR_DY[DIR_COUNT];

typedef struct {
    i32 drop_cooldown; // ticks until the next drop
} Entity_Dropper;

typedef struct {
    i32 upgrader_id; // 0..MAX_UPGRADERS-1; the bit it sets in an item's mask
} Entity_Upgrader;

typedef struct {
    i64 banked; // value banked here over its lifetime
} Entity_Furnace;

// Position and facing are common to every piece, so they live here; the union
// holds only genuinely per-type data (conveyors have none).
typedef struct {
    Entity_Type type;
    i32 grid_x, grid_y;
    Direction dir; // dropper: tile it drops onto; conveyor/upgrader: flow
                   // direction. Furnaces have no facing; theirs is never read.
    union {
        Entity_Dropper dropper;
        Entity_Upgrader upgrader;
        Entity_Furnace furnace;
    } data;
} Entity;

// An item riding the belts. One item occupies one cell at a time. An item exists
// only while it is on a belt: the moment it banks or tips off a dead end it is
// killed, and the transient visual (the drop-out, the payout) is a World_Event
// the renderer animates. The sim carries no visual timing.
typedef struct {
    b32 alive;
    i32 grid_x, grid_y;
    i32 prev_x, prev_y; // cell at the start of this tick, for render interpolation
    i64 value;
    i64 ceiling;       // value cap this ore climbs toward; each distinct upgrader raises it
    u64 upgraded_mask; // bit u set once upgrader u has raised this ore's ceiling
} Item;

// Something the sim did on a tick that has no lasting state but should be seen:
// an ore banked, an ore tipped off a dead end. The sim records what happened,
// with no notion of how long the visual runs or what it looks like; the renderer
// drains this queue once per frame, turns each into a real-time Effect (effects.h),
// and clears it. This is the seam that keeps visual timing out of the sim.
typedef enum {
    WORLD_EVENT_BANKED = 1, // an ore reached a furnace and banked its value
    WORLD_EVENT_FELL,       // an ore rode to a dead end and tipped off
} World_Event_Type;

typedef struct {
    World_Event_Type type;
    i32 from_x, from_y; // the belt cell the ore left
    i32 to_x, to_y;     // where it went: the furnace cell, or the dead-end cell
    i64 value;          // the ore's value at that moment
} World_Event;

#define MAX_ENTITIES 10000
#define MAX_ITEMS 1024
#define MAX_UPGRADERS 64 // bounded by the bits in Item.upgraded_mask
#define WORLD_GRID_SIZE 256
#define MAX_WORLD_EVENTS 256 // per-frame cap; the renderer drains the queue every frame

typedef struct {
    Entity entities[MAX_ENTITIES];
    i32 entity_count;

    // What sits on each cell: 0 = empty, >0 = entity id (index + 1).
    i32 grid[WORLD_GRID_SIZE][WORLD_GRID_SIZE];

    Item items[MAX_ITEMS];
    i32 item_count;
    i32 item_cursor; // where the next free-slot search starts
    // Which item sits on each cell: 0 = none, >0 = item index + 1.
    i32 item_grid[WORLD_GRID_SIZE][WORLD_GRID_SIZE];

    u64 upgrader_ids_used; // allocation bitmap for upgrader ids
    i64 money;             // total value banked across all furnaces

    // Transient events the renderer animates. The sim appends across the ticks of a
    // frame; the renderer drains and resets the count once per frame. See World_Event.
    World_Event events[MAX_WORLD_EVENTS];
    i32 event_count;

    i32 tick;
} World;

// The world is a single arena allocation; its lifetime is the arena's, so there
// is no world_destroy. Free (or reset) the owning arena instead.
World *world_create(Mach_Arena *arena);

void world_tick(World *w);

// Placement. Directional pieces take a facing; furnaces don't. Each returns the
// new entity id, or 0 if the cell was occupied / out of bounds / a limit was hit.
i32 world_spawn_dropper(World *w, i32 x, i32 y, Direction dir);
i32 world_spawn_conveyor(World *w, i32 x, i32 y, Direction dir);
i32 world_spawn_upgrader(World *w, i32 x, i32 y, Direction dir);
i32 world_spawn_furnace(World *w, i32 x, i32 y);
void world_despawn(World *w, i32 entity_id);

// Rotate a placed entity's facing 90 degrees clockwise in place. Returns false for
// entities with no facing (furnaces) or an invalid id.
b32 world_rotate_entity(World *w, i32 entity_id);

// Queries.
i32 world_get_entity_at(World *w, i32 x, i32 y);
Entity *world_get_entity(World *w, i32 entity_id);
b32 world_can_place_at(World *w, i32 x, i32 y);

// The item riding cell (x,y), or NULL.
Item *world_get_item_at(World *w, i32 x, i32 y);

#endif
