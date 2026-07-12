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
    ENTITY_SPLITTER, // a conveyor with two outputs, alternating between them
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
    i32 tier;          // 1..MAX_TIER; scales the base value of the ore it emits
} Entity_Dropper;

typedef struct {
    i32 upgrader_id; // 0..MAX_UPGRADERS-1; the bit it sets in an item's mask
    i32 tier;        // 1..MAX_TIER; scales how hard it lifts an ore's ceiling
} Entity_Upgrader;

typedef struct {
    i64 banked; // value banked here over its lifetime
} Entity_Furnace;

// A conveyor with two outputs that alternates between them, so a belt loop threaded
// through one sheds every other ore toward the exit while the rest keep circulating.
// Without it a closed loop can never cash out: every other piece has a single facing,
// so ore that enters a cycle rides it forever and eventually packs it into a jam.
typedef struct {
    // Quarter-turns clockwise from the entity's `dir` to the second output, 1..3.
    // Never 0, so the two outputs can never be the same cell. Storing the branch
    // relative to the facing (rather than as its own Direction) means rotating the
    // splitter carries the branch along with it and cannot desync the two.
    i32 branch;
    b32 flip; // which output the next item leaves by: 0 = dir, 1 = the branch
} Entity_Splitter;

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
        Entity_Splitter splitter;
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
#define MAX_TIER 5           // 1..MAX_TIER; higher tiers cost more and do more

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
    i64 money;             // spendable balance: furnaces add to it, buying spends it

    // The unlocked build region: a centered square of this side (a power of two).
    // Placement is confined to it; world_expand doubles it for money. See world.c.
    i32 playable_side;

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

// Raw placement, no cost: used for the free starter layout and by the load path.
// Directional pieces take a facing; furnaces don't. Dropper/upgrader take a tier
// (clamped to 1..MAX_TIER). Each returns the new entity id, or 0 if the cell was
// occupied / outside the unlocked region / a limit was hit.
i32 world_spawn_dropper(World *w, i32 x, i32 y, Direction dir, i32 tier);
i32 world_spawn_conveyor(World *w, i32 x, i32 y, Direction dir);
i32 world_spawn_upgrader(World *w, i32 x, i32 y, Direction dir, i32 tier);
i32 world_spawn_furnace(World *w, i32 x, i32 y);
// `branch` is clamped to 1..3 (quarter-turns clockwise from dir to the second output).
i32 world_spawn_splitter(World *w, i32 x, i32 y, Direction dir, i32 branch);
void world_despawn(World *w, i32 entity_id);

// A splitter's second output, resolved from its facing and its branch offset. Only
// meaningful for ENTITY_SPLITTER; returns the entity's own dir for anything else.
Direction world_splitter_out_dir(const Entity *e);

// Turn a splitter's branch output to the next of the three directions that are not its
// facing, leaving the facing alone. Returns false for anything but a splitter. This is
// the "which way does the ore peel off" control, separate from rotating the whole piece.
b32 world_cycle_splitter_branch(World *w, i32 entity_id);

// --- Economy ----------------------------------------------------------------

// The cost to buy one entity of `type` at `tier`. Conveyors and furnaces ignore tier.
i64 world_entity_cost(Entity_Type type, i32 tier);

// Buy and place: like the spawners, but also checks the money is there and spends it.
// Returns the new id, or 0 if unplaceable or unaffordable (money is untouched then).
i32 world_try_place(World *w, Entity_Type type, i32 x, i32 y, Direction dir, i32 tier);

// The unlocked region as a half-open cell range [lo, hi) on both axes.
void world_playable_bounds(const World *w, i32 *lo, i32 *hi);
b32 world_in_playable(const World *w, i32 x, i32 y);

// The cost of the next expansion, or 0 if already at the maximum size.
i64 world_expand_cost(const World *w);
// Double the unlocked side if it's not maxed and the money is there; spends it and
// returns true, else false.
b32 world_expand(World *w);

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
