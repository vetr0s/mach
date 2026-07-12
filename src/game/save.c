#include "save.h"

#include <stdio.h>
#include <string.h>

// One save slot, a small versioned binary blob. Written field by field (not a raw
// struct dump) so it's stable across the compilers on the three target platforms;
// all of them are little-endian, so fixed-width writes need no byte swapping.
//
// What's saved is the minimum to reconstruct the game: the world's money, unlocked
// size, tick, upgrader-id bitmap, every entity (with its per-type data and tier),
// every live item, and the camera/tool state. The cell grids are derived, so they're
// rebuilt on load rather than stored.
// Version history:
//   1  the original: droppers, conveyors, upgraders, furnaces.
//   2  adds splitter records. A v1 file predates ENTITY_SPLITTER, so it cannot contain
//      one, and reading is driven by each entity's type: v1 files still load correctly.
#define SAVE_MAGIC 0x5641534Du // 'MSAV'
#define SAVE_VERSION 2
#define SAVE_VERSION_MIN 1 // oldest format game_load still understands

// Fail the whole operation on any short read/write: a truncated file is a failure,
// not a partial success. Both macros close the file and return false.
#define WR(p, n)                                                                                   \
    do {                                                                                           \
        if (fwrite((p), (n), 1, f) != 1) {                                                         \
            fclose(f);                                                                             \
            return MACH_FALSE;                                                                     \
        }                                                                                          \
    } while (0)
#define RD(p, n)                                                                                   \
    do {                                                                                           \
        if (fread((p), (n), 1, f) != 1) {                                                          \
            fclose(f);                                                                             \
            return MACH_FALSE;                                                                     \
        }                                                                                          \
    } while (0)

static b32 cell_ok(i32 x, i32 y) {
    return x >= 0 && x < WORLD_GRID_SIZE && y >= 0 && y < WORLD_GRID_SIZE;
}

b32 game_save(const Game_State *g, const char *path) {
    if (!g || !g->world)
        return MACH_FALSE;
    FILE *f = fopen(path, "wb");
    if (!f)
        return MACH_FALSE;
    const World *w = g->world;

    u32 magic = SAVE_MAGIC, ver = SAVE_VERSION;
    WR(&magic, 4);
    WR(&ver, 4);

    WR(&w->money, sizeof(i64));
    WR(&w->playable_side, sizeof(i32));
    WR(&w->tick, sizeof(i32));
    WR(&w->upgrader_ids_used, sizeof(u64));

    WR(&w->entity_count, sizeof(i32));
    for (i32 i = 0; i < w->entity_count; i++) {
        const Entity *e = &w->entities[i];
        i32 type = (i32)e->type, dir = (i32)e->dir;
        WR(&type, 4);
        WR(&e->grid_x, 4);
        WR(&e->grid_y, 4);
        WR(&dir, 4);
        switch (e->type) {
        case ENTITY_DROPPER:
            WR(&e->data.dropper.drop_cooldown, 4);
            WR(&e->data.dropper.tier, 4);
            break;
        case ENTITY_UPGRADER:
            WR(&e->data.upgrader.upgrader_id, 4);
            WR(&e->data.upgrader.tier, 4);
            break;
        case ENTITY_FURNACE:
            WR(&e->data.furnace.banked, sizeof(i64));
            break;
        case ENTITY_SPLITTER:
            WR(&e->data.splitter.branch, 4);
            WR(&e->data.splitter.flip, sizeof(b32));
            break;
        default:
            break; // conveyors carry no per-type data
        }
    }

    // Live items only. Count them, then write each.
    i32 alive = 0;
    for (i32 i = 0; i < MAX_ITEMS; i++)
        if (w->items[i].alive)
            alive++;
    WR(&alive, sizeof(i32));
    for (i32 i = 0; i < MAX_ITEMS; i++) {
        const Item *it = &w->items[i];
        if (!it->alive)
            continue;
        WR(&it->grid_x, 4);
        WR(&it->grid_y, 4);
        WR(&it->value, sizeof(i64));
        WR(&it->ceiling, sizeof(i64));
        WR(&it->upgraded_mask, sizeof(u64));
    }

    WR(&g->camera.pan.x, sizeof(f32));
    WR(&g->camera.pan.y, sizeof(f32));
    WR(&g->camera.zoom, sizeof(f32));
    i32 tool = g->selected_tool, tier = g->selected_tier, pdir = (i32)g->place_dir;
    WR(&tool, 4);
    WR(&tier, 4);
    WR(&pdir, 4);

    fclose(f);
    MACH_LOG_INFO("saved %d entities, %d items to %s", w->entity_count, alive, path);
    return MACH_TRUE;
}

b32 game_load(Game_State *g, const char *path) {
    if (!g)
        return MACH_FALSE;
    FILE *f = fopen(path, "rb");
    if (!f)
        return MACH_FALSE;

    u32 magic = 0, ver = 0;
    RD(&magic, 4);
    RD(&ver, 4);
    if (magic != SAVE_MAGIC || ver < SAVE_VERSION_MIN || ver > SAVE_VERSION) {
        fclose(f);
        return MACH_FALSE; // not our file, or a format we don't understand
    }

    // Read the header before touching the live game, and range-check the counts so a
    // corrupt file can't drive a huge or negative loop.
    i64 money;
    i32 side, tick, ecount;
    u64 upg_used;
    RD(&money, sizeof(i64));
    RD(&side, sizeof(i32));
    RD(&tick, sizeof(i32));
    RD(&upg_used, sizeof(u64));
    RD(&ecount, sizeof(i32));
    if (side < 1 || side > WORLD_GRID_SIZE || ecount < 0 || ecount > MAX_ENTITIES) {
        fclose(f);
        return MACH_FALSE;
    }

    // Commit: rebuild a fresh world and fill it. A read failure past here leaves a
    // partial world, but game_load returning false keeps the caller out of it (the
    // menu stays put; a New Game resets everything anyway).
    mach_arena_reset(&g->arena);
    g->world = world_create(&g->arena);
    if (!g->world) {
        fclose(f);
        return MACH_FALSE;
    }
    World *w = g->world;
    w->money = money;
    w->playable_side = side;
    w->tick = tick;
    w->upgrader_ids_used = upg_used;

    for (i32 i = 0; i < ecount; i++) {
        Entity e;
        memset(&e, 0, sizeof e);
        i32 type, dir;
        RD(&type, 4);
        RD(&e.grid_x, 4);
        RD(&e.grid_y, 4);
        RD(&dir, 4);
        e.type = (Entity_Type)type;
        e.dir = (Direction)dir;
        switch (e.type) {
        case ENTITY_DROPPER:
            RD(&e.data.dropper.drop_cooldown, 4);
            RD(&e.data.dropper.tier, 4);
            break;
        case ENTITY_UPGRADER:
            RD(&e.data.upgrader.upgrader_id, 4);
            RD(&e.data.upgrader.tier, 4);
            break;
        case ENTITY_FURNACE:
            RD(&e.data.furnace.banked, sizeof(i64));
            break;
        case ENTITY_SPLITTER:
            RD(&e.data.splitter.branch, 4);
            RD(&e.data.splitter.flip, sizeof(b32));
            break;
        default:
            break;
        }
        // Place into the arrays and rebuild the entity grid. Skip anything that would
        // land off-grid or on an occupied cell: better to drop a stray than corrupt.
        if (cell_ok(e.grid_x, e.grid_y) && w->grid[e.grid_x][e.grid_y] == 0) {
            i32 idx = w->entity_count++;
            w->entities[idx] = e;
            w->grid[e.grid_x][e.grid_y] = idx + 1;
        }
    }

    i32 icount;
    RD(&icount, sizeof(i32));
    if (icount < 0 || icount > MAX_ITEMS) {
        fclose(f);
        return MACH_FALSE;
    }
    for (i32 i = 0; i < icount; i++) {
        Item it;
        memset(&it, 0, sizeof it);
        RD(&it.grid_x, 4);
        RD(&it.grid_y, 4);
        RD(&it.value, sizeof(i64));
        RD(&it.ceiling, sizeof(i64));
        RD(&it.upgraded_mask, sizeof(u64));
        it.alive = MACH_TRUE;
        it.prev_x = it.grid_x; // start interpolation at rest
        it.prev_y = it.grid_y;
        i32 idx = w->item_count;
        w->items[idx] = it;
        if (cell_ok(it.grid_x, it.grid_y))
            w->item_grid[it.grid_x][it.grid_y] = idx + 1;
        w->item_count++;
    }
    w->item_cursor = w->item_count % MAX_ITEMS;

    RD(&g->camera.pan.x, sizeof(f32));
    RD(&g->camera.pan.y, sizeof(f32));
    RD(&g->camera.zoom, sizeof(f32));
    i32 tool, tier, pdir;
    RD(&tool, 4);
    RD(&tier, 4);
    RD(&pdir, 4);
    g->selected_tool = tool;
    g->selected_tier = tier < 1 ? 1 : (tier > MAX_TIER ? MAX_TIER : tier);
    g->place_dir = (Direction)pdir;

    fclose(f);

    // Reset the transient, non-saved parts to a clean playing state.
    g->paused = MACH_FALSE;
    g->sim_accumulator = 0.0f;
    g->anim_time = 0.0f;
    g->pointer_over_ui = MACH_FALSE;
    effects_clear(&g->effects);

    MACH_LOG_INFO("loaded %d entities, %d items from %s", w->entity_count, w->item_count, path);
    return MACH_TRUE;
}

b32 save_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return MACH_FALSE;
    fclose(f);
    return MACH_TRUE;
}
