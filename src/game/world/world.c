// World implementation (included into mach.c).

#include "world.h"

// Sim tuning. DROP_PERIOD is in ticks, so an item lands every DROP_PERIOD cells on a
// straight belt regardless of the tick rate. At 3 ticks/sec, DROP_PERIOD 2 drops one
// item every ~0.67s, spaced two cells apart, so belts read as a dense heavy stream.
// (This lives per-dropper as drop_cooldown, so dropper tiers are already a value bump.)
#define DROP_PERIOD 2
#define ITEM_BASE_VALUE 10

// Value model: "diminishing per pass, uncapped roof" (see docs/gdd.typ). Each ore
// climbs toward a ceiling. Every *distinct* upgrader multiplies that ceiling once, so
// more (and, with tiers later, stronger) upgraders raise the roof without limit. Every
// pass -- re-passes on a loop included -- closes a fixed fraction of the gap to the
// ceiling, so a single loop settles toward it and looping forever stops paying.
#define UPGRADER_CEILING_MULT 4       // a tier-1 upgrader's multiply on the ceiling
#define UPGRADER_TIER_STEP 2          // each upgrader tier above 1 adds this to the multiply
#define UPGRADER_CLIMB_DIVISOR 2      // gap-to-ceiling closed per pass is 1/this
#define MAX_ITEM_VALUE ((i64)1 << 62) // clamp so the uncapped ceiling can't overflow i64

// Economy tuning. Money is banked value; the rest are prices and sizes. All in one
// place so the whole curve is legible and tunable. See docs/gdd.typ "Economy".
#define STARTING_MONEY 100    // seed so the first few pieces are affordable
#define PLAYABLE_START_SIDE 4 // a new game's unlocked square side (a power of two)
#define PLAYABLE_MAX_SIDE 128 // cap; the region is centered in the 256 grid
#define PLAYABLE_CENTER (WORLD_GRID_SIZE / 2)
#define CONVEYOR_COST 5 // conveyors, splitters and furnaces are single-tier
#define SPLITTER_COST 25
#define FURNACE_COST 50
#define DROPPER_COST_BASE 50    // dropper price is base * tier
#define UPGRADER_COST_BASE 100  // upgrader price is base * tier
#define EXPAND_COST_PER_CELL 10 // expansion price is this per newly unlocked cell

static i32 clamp_tier(i32 t) {
    return t < 1 ? 1 : (t > MAX_TIER ? MAX_TIER : t);
}

const i32 DIR_DX[DIR_COUNT] = {0, 1, 0, -1};
const i32 DIR_DY[DIR_COUNT] = {-1, 0, 1, 0};

// Entity ids are 1-based so 0 can mean "empty" in the grids.
static i32 entity_id_from_index(i32 idx) {
    return idx + 1;
}
static i32 entity_index_from_id(i32 id) {
    return id - 1;
}

static void item_kill(World *w, i32 idx); // defined in the Items section below

static b32 in_bounds(i32 x, i32 y) {
    return x >= 0 && x < WORLD_GRID_SIZE && y >= 0 && y < WORLD_GRID_SIZE;
}

// Is (nx, ny) backed up? A destination blocks only when a belt-like cell there already
// holds an item. Off-grid, bare ground, and a dropper's back are dead ends, not blocks:
// ore tips off there (see world_move_items), and that is the player's mistake to see, so
// a splitter must not quietly reroute around it.
static b32 cell_backed_up(const World *w, i32 nx, i32 ny) {
    if (!in_bounds(nx, ny) || !w->grid[nx][ny])
        return MACH_FALSE;
    return w->item_grid[nx][ny] != 0;
}

Direction world_splitter_out_dir(const Entity *e) {
    if (e->type != ENTITY_SPLITTER)
        return e->dir;
    return (Direction)((e->dir + e->data.splitter.branch) % DIR_COUNT);
}

// The direction an item leaves this cell by, if the entity there moves items at all.
// Droppers emit and furnaces consume, so neither answers.
//
// A splitter has two outputs and alternates between them. It takes the one its flip
// points at, unless that one is backed up, in which case it takes the other: a packed
// loop must always be able to drain through the splitter, and strict alternation would
// deadlock exactly when the loop is full (the loop-side output is occupied, so the ore
// waits for a cell that only frees once this ore leaves).
static b32 item_exit_dir(const World *w, const Entity *e, Direction *out) {
    if (e->type == ENTITY_CONVEYOR || e->type == ENTITY_UPGRADER) {
        *out = e->dir;
        return MACH_TRUE;
    }
    if (e->type == ENTITY_SPLITTER) {
        Direction branch = world_splitter_out_dir(e);
        Direction pref = e->data.splitter.flip ? branch : e->dir;
        Direction alt = e->data.splitter.flip ? e->dir : branch;
        i32 px = e->grid_x + DIR_DX[pref], py = e->grid_y + DIR_DY[pref];
        *out = cell_backed_up(w, px, py) ? alt : pref;
        return MACH_TRUE;
    }
    return MACH_FALSE;
}

// Advance a splitter's alternation. Called once per item that actually leaves it, by any
// route (moved on, banked, or tipped off a dead end), so a splitter whose output is
// blocked does not spin its flip while the ore sits there waiting.
static void splitter_advance(Entity *e) {
    if (e->type == ENTITY_SPLITTER)
        e->data.splitter.flip = !e->data.splitter.flip;
}

World *world_create(Mach_Arena *arena) {
    World *w = (World *)mach_arena_alloc(arena, sizeof(World));
    if (!w) {
        MACH_LOG_ERROR("world_create: arena allocation failed (%zu bytes)", sizeof(World));
        return NULL;
    }

    // (npt): mach_arena_alloc doesn't zero, and the sim leans on cleared grids,
    // counts, and bitmaps, so wipe the whole struct.
    memset(w, 0, sizeof(*w));

    w->playable_side = PLAYABLE_START_SIDE;
    w->money = STARTING_MONEY;

    MACH_LOG_INFO("world created (%d entity cap, %d item cap, %dx%d grid)", MAX_ENTITIES, MAX_ITEMS,
                  WORLD_GRID_SIZE, WORLD_GRID_SIZE);
    return w;
}

// --- Entities ---------------------------------------------------------------

// Shared spawn path: validate, claim the cell, set the common fields, hand back
// the fresh slot. Returns NULL on failure with entity_count left untouched.
// (npt): Slots are reused after swap-remove, so spawners must set every field of
// their union variant; the common fields are covered here.
static Entity *world_spawn(World *w, i32 x, i32 y, Direction dir, Entity_Type type) {
    if (!w || w->entity_count >= MAX_ENTITIES) {
        MACH_LOG_DEBUG("spawn rejected: world full");
        return NULL;
    }
    if (!in_bounds(x, y)) {
        MACH_LOG_DEBUG("spawn rejected: out of bounds (%d,%d)", x, y);
        return NULL;
    }
    if (w->grid[x][y] != 0) {
        MACH_LOG_DEBUG("spawn rejected: cell occupied (%d,%d)", x, y);
        return NULL;
    }

    i32 idx = w->entity_count++;
    Entity *e = &w->entities[idx];
    e->type = type;
    e->grid_x = x;
    e->grid_y = y;
    e->dir = dir;
    w->grid[x][y] = entity_id_from_index(idx);
    return e;
}

i32 world_spawn_dropper(World *w, i32 x, i32 y, Direction dir, i32 tier) {
    Entity *e = world_spawn(w, x, y, dir, ENTITY_DROPPER);
    if (!e)
        return 0;
    e->data.dropper = (Entity_Dropper){.drop_cooldown = 0, .tier = clamp_tier(tier)};
    MACH_LOG_DEBUG("spawned dropper at (%d,%d) facing %d tier %d", x, y, dir, clamp_tier(tier));
    return w->grid[x][y];
}

i32 world_spawn_conveyor(World *w, i32 x, i32 y, Direction dir) {
    Entity *e = world_spawn(w, x, y, dir, ENTITY_CONVEYOR);
    return e ? w->grid[x][y] : 0;
}

// Claim the lowest free upgrader id, or -1 if all MAX_UPGRADERS are taken.
static i32 upgrader_id_alloc(World *w) {
    for (i32 b = 0; b < MAX_UPGRADERS; b++) {
        u64 bit = (u64)1 << b;
        if (!(w->upgrader_ids_used & bit)) {
            w->upgrader_ids_used |= bit;
            return b;
        }
    }
    return -1;
}

static void upgrader_id_free(World *w, i32 b) {
    if (b >= 0 && b < MAX_UPGRADERS)
        w->upgrader_ids_used &= ~((u64)1 << b);
}

i32 world_spawn_upgrader(World *w, i32 x, i32 y, Direction dir, i32 tier) {
    // (npt): Reserve the id before claiming the cell so a failure on either side
    // leaves no half-built upgrader and no leaked id.
    i32 uid = upgrader_id_alloc(w);
    if (uid < 0) {
        MACH_LOG_DEBUG("upgrader rejected: %d-upgrader limit reached", MAX_UPGRADERS);
        return 0;
    }
    Entity *e = world_spawn(w, x, y, dir, ENTITY_UPGRADER);
    if (!e) {
        upgrader_id_free(w, uid);
        return 0;
    }
    e->data.upgrader = (Entity_Upgrader){.upgrader_id = uid, .tier = clamp_tier(tier)};
    return w->grid[x][y];
}

i32 world_spawn_furnace(World *w, i32 x, i32 y) {
    Entity *e = world_spawn(w, x, y, DIR_N, ENTITY_FURNACE); // facing unused
    if (!e)
        return 0;
    e->data.furnace = (Entity_Furnace){.banked = 0};
    return w->grid[x][y];
}

// Wrap a branch offset into 1..3, so the branch output is never the facing itself.
static i32 clamp_branch(i32 b) {
    b %= DIR_COUNT;
    if (b < 0)
        b += DIR_COUNT;
    return b == 0 ? 1 : b;
}

i32 world_spawn_splitter(World *w, i32 x, i32 y, Direction dir, i32 branch) {
    Entity *e = world_spawn(w, x, y, dir, ENTITY_SPLITTER);
    if (!e)
        return 0;
    e->data.splitter = (Entity_Splitter){.branch = clamp_branch(branch), .flip = MACH_FALSE};
    return w->grid[x][y];
}

b32 world_cycle_splitter_branch(World *w, i32 entity_id) {
    Entity *e = world_get_entity(w, entity_id);
    if (!e || e->type != ENTITY_SPLITTER)
        return MACH_FALSE;
    e->data.splitter.branch = clamp_branch(e->data.splitter.branch + 1);
    return MACH_TRUE;
}

void world_despawn(World *w, i32 entity_id) {
    if (!w || entity_id == 0)
        return;

    i32 idx = entity_index_from_id(entity_id);
    if (idx < 0 || idx >= w->entity_count)
        return;

    Entity *e = &w->entities[idx];
    if (e->type == ENTITY_UPGRADER)
        upgrader_id_free(w, e->data.upgrader.upgrader_id);

    i32 ex = e->grid_x, ey = e->grid_y;
    w->grid[ex][ey] = 0;

    // Any ore riding this cell is orphaned by the delete: despawn it rather than
    // leave it stranded on bare ground (where it would sit forever and block the cell).
    i32 item_here = w->item_grid[ex][ey];
    if (item_here)
        item_kill(w, item_here - 1);

    // Swap-remove: move the last entity into the freed slot and fix its grid id.
    i32 last = w->entity_count - 1;
    if (idx != last) {
        w->entities[idx] = w->entities[last];
        w->grid[w->entities[idx].grid_x][w->entities[idx].grid_y] = entity_id_from_index(idx);
    }
    w->entity_count--;
    MACH_LOG_DEBUG("despawned entity %d at (%d,%d), %d remaining", entity_id, ex, ey,
                   w->entity_count);
}

b32 world_rotate_entity(World *w, i32 entity_id) {
    Entity *e = world_get_entity(w, entity_id);
    if (!e || e->type == ENTITY_FURNACE)
        return MACH_FALSE; // furnaces have no facing
    e->dir = (Direction)((e->dir + 1) % DIR_COUNT);
    return MACH_TRUE;
}

i32 world_get_entity_at(World *w, i32 x, i32 y) {
    if (!w || !in_bounds(x, y))
        return 0;
    return w->grid[x][y];
}

Entity *world_get_entity(World *w, i32 entity_id) {
    if (!w || entity_id == 0)
        return NULL;
    i32 idx = entity_index_from_id(entity_id);
    if (idx < 0 || idx >= w->entity_count)
        return NULL;
    return &w->entities[idx];
}

b32 world_can_place_at(World *w, i32 x, i32 y) {
    if (!w || !in_bounds(x, y))
        return MACH_FALSE;
    if (!world_in_playable(w, x, y))
        return MACH_FALSE; // outside the unlocked region
    if (w->grid[x][y] != 0)
        return MACH_FALSE;
    if (w->entity_count >= MAX_ENTITIES)
        return MACH_FALSE;
    return MACH_TRUE;
}

Item *world_get_item_at(World *w, i32 x, i32 y) {
    if (!w || !in_bounds(x, y))
        return NULL;
    i32 id = w->item_grid[x][y];
    return id ? &w->items[id - 1] : NULL;
}

// --- Economy ----------------------------------------------------------------

void world_playable_bounds(const World *w, i32 *lo, i32 *hi) {
    i32 half = w->playable_side / 2;
    *lo = PLAYABLE_CENTER - half;
    *hi = PLAYABLE_CENTER + half;
}

b32 world_in_playable(const World *w, i32 x, i32 y) {
    i32 lo, hi;
    world_playable_bounds(w, &lo, &hi);
    return x >= lo && x < hi && y >= lo && y < hi;
}

i64 world_entity_cost(Entity_Type type, i32 tier) {
    tier = clamp_tier(tier);
    switch (type) {
    case ENTITY_CONVEYOR:
        return CONVEYOR_COST;
    case ENTITY_SPLITTER:
        return SPLITTER_COST;
    case ENTITY_FURNACE:
        return FURNACE_COST;
    case ENTITY_DROPPER:
        return (i64)DROPPER_COST_BASE * tier;
    case ENTITY_UPGRADER:
        return (i64)UPGRADER_COST_BASE * tier;
    default:
        return 0;
    }
}

i32 world_try_place(World *w, Entity_Type type, i32 x, i32 y, Direction dir, i32 tier) {
    if (!w || !world_can_place_at(w, x, y))
        return 0;
    i64 cost = world_entity_cost(type, tier);
    if (w->money < cost)
        return 0;

    i32 id = 0;
    switch (type) {
    case ENTITY_DROPPER:
        id = world_spawn_dropper(w, x, y, dir, tier);
        break;
    case ENTITY_CONVEYOR:
        id = world_spawn_conveyor(w, x, y, dir);
        break;
    case ENTITY_UPGRADER:
        id = world_spawn_upgrader(w, x, y, dir, tier);
        break;
    case ENTITY_SPLITTER:
        id = world_spawn_splitter(w, x, y, dir, 1); // branch defaults to 90 deg clockwise
        break;
    case ENTITY_FURNACE:
        id = world_spawn_furnace(w, x, y);
        break;
    default:
        return 0;
    }
    if (id)
        w->money -= cost;
    return id;
}

i64 world_expand_cost(const World *w) {
    if (w->playable_side >= PLAYABLE_MAX_SIDE)
        return 0; // already maxed
    i32 next = w->playable_side * 2;
    i64 added = (i64)next * next - (i64)w->playable_side * w->playable_side;
    return added * EXPAND_COST_PER_CELL;
}

b32 world_expand(World *w) {
    if (!w || w->playable_side >= PLAYABLE_MAX_SIDE)
        return MACH_FALSE;
    i64 cost = world_expand_cost(w);
    if (w->money < cost)
        return MACH_FALSE;
    w->money -= cost;
    w->playable_side *= 2;
    return MACH_TRUE;
}

// --- Items ------------------------------------------------------------------

// Claim a free item slot, or -1 if the pool is full. The cursor amortizes the
// scan across calls so steady spawning stays roughly O(1).
static i32 item_alloc(World *w) {
    for (i32 k = 0; k < MAX_ITEMS; k++) {
        i32 idx = (w->item_cursor + k) % MAX_ITEMS;
        if (!w->items[idx].alive) {
            w->item_cursor = (idx + 1) % MAX_ITEMS;
            w->item_count++;
            return idx;
        }
    }
    return -1;
}

static void item_kill(World *w, i32 idx) {
    Item *it = &w->items[idx];
    w->item_grid[it->grid_x][it->grid_y] = 0;
    it->alive = MACH_FALSE;
    w->item_count--;
}

// Record a transient event for the renderer to animate this frame. Full is not an
// error: the queue holds a frame's worth (MAX_WORLD_EVENTS), and dropping the
// overflow costs at most a missed spark, never sim state.
static void world_emit(World *w, World_Event ev) {
    if (w->event_count >= MAX_WORLD_EVENTS)
        return;
    w->events[w->event_count++] = ev;
}

// Bank `value` into a furnace: the world total and the furnace's own tally.
static void furnace_bank(World *w, Entity *e, i64 value) {
    w->money += value;
    e->data.furnace.banked += value;
}

// Apply the effect of the entity on (x,y) to an item that just arrived there;
// today that's upgraders raising its value. Furnaces never appear here: both
// arrival paths (world_move_items, world_run_droppers) bank and consume the
// item *before* it would land, so an item never occupies a furnace's cell.
static void item_apply_cell(World *w, i32 item_idx, i32 x, i32 y) {
    i32 id = w->grid[x][y];
    if (!id)
        return;
    Entity *e = world_get_entity(w, id);
    Item *it = &w->items[item_idx];

    if (e->type == ENTITY_UPGRADER) {
        // Raise the roof once per distinct upgrader, then climb toward it (this pass
        // and every re-pass). See the value-model note at the top of the file.
        u64 bit = (u64)1 << e->data.upgrader.upgrader_id;
        if (!(it->upgraded_mask & bit)) {
            it->upgraded_mask |= bit;
            // A higher-tier upgrader lifts the ceiling harder.
            i64 mult =
                UPGRADER_CEILING_MULT + (i64)(e->data.upgrader.tier - 1) * UPGRADER_TIER_STEP;
            if (it->ceiling > MAX_ITEM_VALUE / mult)
                it->ceiling = MAX_ITEM_VALUE;
            else
                it->ceiling *= mult;
        }
        i64 gap = it->ceiling - it->value;
        if (gap > 0)
            it->value += gap / UPGRADER_CLIMB_DIVISOR;
    }
}

// Advance items one cell along their belts. A "moved" flag caps each item to a
// single cell per tick; iterating until nothing moves lets a follower fill the
// cell its leader just vacated, so a belt with a gap advances fully in one tick
// regardless of slot order. A fully packed loop has no gap and correctly jams.
static void world_move_items(World *w) {
    u8 moved[MAX_ITEMS];
    memset(moved, 0, sizeof(moved));

    b32 changed = MACH_TRUE;
    while (changed) {
        changed = MACH_FALSE;
        for (i32 i = 0; i < MAX_ITEMS; i++) {
            Item *it = &w->items[i];
            if (!it->alive || moved[i])
                continue;

            i32 cell_id = w->grid[it->grid_x][it->grid_y];
            if (!cell_id) {
                moved[i] = MACH_TRUE;
                continue;
            } // on bare ground, stuck

            Entity *src = world_get_entity(w, cell_id);
            Direction d;
            if (!item_exit_dir(w, src, &d))
                continue; // furnace/dropper

            i32 nx = it->grid_x + DIR_DX[d];
            i32 ny = it->grid_y + DIR_DY[d];

            i32 dst_id = in_bounds(nx, ny) ? w->grid[nx][ny] : 0;
            Entity *dst = dst_id ? world_get_entity(w, dst_id) : NULL;

            // Dead end ahead -- off the grid, bare ground, or a dropper's back -- so the
            // ore rides to the end and tips off. It is gone from the sim this tick; the
            // renderer plays the drop-out from the WORLD_EVENT_FELL below.
            if (!dst || dst->type == ENTITY_DROPPER) {
                world_emit(
                    w, (World_Event){WORLD_EVENT_FELL, it->grid_x, it->grid_y, nx, ny, it->value});
                item_kill(w, i);
                splitter_advance(src);
                changed = MACH_TRUE;
                continue;
            }

            if (dst->type == ENTITY_FURNACE) {
                world_emit(w, (World_Event){WORLD_EVENT_BANKED, it->grid_x, it->grid_y, nx, ny,
                                            it->value});
                furnace_bank(w, dst, it->value);
                item_kill(w, i);
                splitter_advance(src);
                changed = MACH_TRUE;
                continue;
            }

            if (w->item_grid[nx][ny] != 0)
                continue; // blocked by an item; wait

            w->item_grid[it->grid_x][it->grid_y] = 0;
            it->grid_x = nx;
            it->grid_y = ny;
            w->item_grid[nx][ny] = i + 1;
            moved[i] = MACH_TRUE;
            splitter_advance(src);
            changed = MACH_TRUE;
            item_apply_cell(w, i, nx, ny);
        }
    }
}

// Each ready dropper emits one item onto the tile it faces, if that tile is a
// belt-like cell with room (or a furnace, which banks the base value directly).
static void world_run_droppers(World *w) {
    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];
        if (e->type != ENTITY_DROPPER)
            continue;

        Entity_Dropper *dr = &e->data.dropper;
        if (dr->drop_cooldown > 0) {
            dr->drop_cooldown--;
            continue;
        }

        // A higher-tier dropper emits richer ore (a higher starting value and ceiling).
        i64 base = (i64)ITEM_BASE_VALUE * dr->tier;

        i32 nx = e->grid_x + DIR_DX[e->dir];
        i32 ny = e->grid_y + DIR_DY[e->dir];
        if (!in_bounds(nx, ny))
            continue;

        i32 dst_id = w->grid[nx][ny];
        if (!dst_id)
            continue;
        Entity *dst = world_get_entity(w, dst_id);
        if (dst->type == ENTITY_DROPPER)
            continue;

        if (dst->type == ENTITY_FURNACE) {
            // Dropper feeds a furnace directly: no belt cell between them, so the
            // payout pops at the furnace (from == to).
            world_emit(w, (World_Event){WORLD_EVENT_BANKED, nx, ny, nx, ny, base});
            furnace_bank(w, dst, base);
            dr->drop_cooldown = DROP_PERIOD;
            continue;
        }

        if (w->item_grid[nx][ny] != 0)
            continue; // front occupied; retry next tick

        i32 idx = item_alloc(w);
        if (idx < 0)
            continue; // pool full
        Item *it = &w->items[idx];
        it->alive = MACH_TRUE;
        it->grid_x = nx;
        it->grid_y = ny;
        it->prev_x = nx; // spawns on the belt cell (outside the dropper); the
        it->prev_y = ny; // same-tick move below carries it forward, so no idle beat
        it->value = base;
        it->ceiling = base;
        it->upgraded_mask = 0;
        w->item_grid[nx][ny] = idx + 1;
        dr->drop_cooldown = DROP_PERIOD;
        item_apply_cell(w, idx, nx, ny); // upgrade if dropped onto an upgrader
    }
}

// Snapshot each item's cell so the renderer can interpolate from where it was at
// the start of the tick to where it ends up, sliding it smoothly along the belt.
static void world_snapshot_items(World *w) {
    for (i32 i = 0; i < MAX_ITEMS; i++) {
        Item *it = &w->items[i];
        if (!it->alive)
            continue;
        it->prev_x = it->grid_x;
        it->prev_y = it->grid_y;
    }
}

void world_tick(World *w) {
    if (!w)
        return;
    w->tick++;
    world_snapshot_items(w);
    // Droppers before move so a freshly dropped item is picked up by the same-tick
    // move pass: it spawns on the belt cell and slides forward immediately instead of
    // sitting idle for a whole tick (very visible at the slow sim rate).
    world_run_droppers(w);
    world_move_items(w);
}
