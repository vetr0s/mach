// Game application glue (included into mach.c): wires the game state into the
// loop's per-frame entry points.

#include "app.h"
#include "game.h"
#include "render_game.h"
#include <stdio.h>

// The game defines its window and engine policy here.
Mach_Engine_Config game_engine_config(void) {
    return (Mach_Engine_Config){
        .title = "mach",
        .width = 1280,
        .height = 720,
        .fullscreen = MACH_FALSE,
        .resizable = MACH_TRUE,
        .clear_color = COLOR_BG_MAIN,   // modus-vivendi: true black beyond the grid
        .escape_quits = MACH_TRUE,      // dev convenience, for now
        .target_fps = 120,
    };
}

void app_init(App *a, Mach_Engine *e) {
    game_init(&a->game);
    clay_ui_init(&a->clay, &e->r2d);
}

void app_update(App *a, Mach_Engine *e, f32 dt) {
    // Mach_Input works in the renderer's current pixel space, which may differ from
    // the requested size (fullscreen, or a resized window).
    game_process_input(&a->game, &e->input, (f32)e->r2d.width, (f32)e->r2d.height, dt);
    game_tick(&a->game, dt);
}

// HUD colors, from the engine palette (clay_color_of converts to Clay's 0-255).
#define HUD_PANEL   clay_color_of(color_alpha(COLOR_BG_DIM, 0.86f))
#define HUD_GOLD    clay_color_of(COLOR_YELLOW_WARMER)
#define HUD_GREEN   clay_color_of(COLOR_GREEN)
#define HUD_AMBER   clay_color_of(COLOR_YELLOW)
#define HUD_GREY    clay_color_of(COLOR_FG_DIM)
#define HUD_WHITE   clay_color_of(COLOR_FG_MAIN)
#define HUD_PURPLE  clay_color_of(COLOR_MAGENTA_COOLER)

// A floating HUD panel pinned to a screen corner/edge: the same attach point on
// the element and the root, nudged inward by (ox, oy). Follow with a block of
// CLAY_TEXT children.
#define HUD_PANEL_AT(name, attach, ox, oy) \
    CLAY(CLAY_ID(name), { \
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, \
                    .padding = CLAY_PADDING_ALL(10), .childGap = 4, \
                    .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) } }, \
        .backgroundColor = HUD_PANEL, \
        .cornerRadius = CLAY_CORNER_RADIUS(6), \
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, \
                      .attachPoints = { .element = attach, .parent = attach }, \
                      .offset = { ox, oy } } })

void app_render(App *a, Mach_Engine *e) {
    Mach_Renderer *r = &e->r2d;
    const Game_State *g = &a->game;
    game_render_draw(r, g, &e->frame_arena);

    static const char *tool_names[] = {"None", "Dropper", "Conveyor", "Upgrader",
                                       "Collector", "Delete"};
    static const char *dir_names[]  = {"N", "E", "S", "W"};
    static const char *entity_names[] = {"", "Dropper", "Conveyor", "Upgrader", "Collector"};
    const char *tool = (g->selected_tool >= 0 && g->selected_tool < (i32)ARRAY_COUNT(tool_names))
                           ? tool_names[g->selected_tool] : "?";
    const char *facing = (g->place_dir >= 0 && g->place_dir < (i32)ARRAY_COUNT(dir_names))
                             ? dir_names[g->place_dir] : "?";

    // HUD strings. These buffers must outlive clay_ui_render (Clay keeps the char
    // pointers, not copies), so they stay in scope for the whole function.
    char money_s[32], tool_s[48], fps_s[24], counts_s[64], hover_s[48], cam_s[48];
    char val_a[24], val_b[24], banked_s[24];
    char insp_sub_s[32], insp_extra_s[48], insp_item_s[64];
    snprintf(money_s, sizeof(money_s), "$%lld", (long long)(g->world ? g->world->money : 0));
    snprintf(tool_s, sizeof(tool_s), "%s   facing %s", tool, facing);
    snprintf(fps_s, sizeof(fps_s), "fps %d", engine_fps(e));
    snprintf(counts_s, sizeof(counts_s), "tick %d   entities %d   items %d",
             g->world ? g->world->tick : 0, g->world ? g->world->entity_count : 0,
             g->world ? g->world->item_count : 0);
    if (g->hover_valid)
        snprintf(hover_s, sizeof(hover_s), "hover %d,%d%s", g->hover_grid_x, g->hover_grid_y,
                 g->hover_can_place ? "" : " (blocked)");
    else
        snprintf(hover_s, sizeof(hover_s), "hover --");
    snprintf(cam_s, sizeof(cam_s), "cam %.0f,%.0f   zoom %.2f",
             g->camera.pan.x, g->camera.pan.y, g->camera.zoom);

    // Inspect: describe whatever sits under the cursor (machine and/or ore).
    const Entity *ins = NULL;
    const Item *ore = NULL;
    if (g->world && g->hover_valid) {
        ins = world_get_entity(g->world,
                               world_get_entity_at(g->world, g->hover_grid_x, g->hover_grid_y));
        ore = world_get_item_at(g->world, g->hover_grid_x, g->hover_grid_y);
    }
    insp_sub_s[0] = insp_extra_s[0] = insp_item_s[0] = '\0';
    const char *insp_title = "Ore";
    Clay_Color insp_title_col = HUD_GOLD;
    if (ins) {
        insp_title = entity_names[ins->type];
        switch (ins->type) {
        case ENTITY_DROPPER:
            insp_title_col = HUD_GREEN;
            snprintf(insp_sub_s, sizeof insp_sub_s, "facing %s", dir_names[ins->dir]);
            snprintf(insp_extra_s, sizeof insp_extra_s, "next drop in %dt",
                     ins->data.dropper.drop_cooldown);
            break;
        case ENTITY_CONVEYOR:
            insp_title_col = HUD_WHITE;
            snprintf(insp_sub_s, sizeof insp_sub_s, "facing %s", dir_names[ins->dir]);
            break;
        case ENTITY_UPGRADER:
            insp_title_col = HUD_PURPLE;
            snprintf(insp_sub_s, sizeof insp_sub_s, "facing %s", dir_names[ins->dir]);
            break;
        case ENTITY_COLLECTOR:
            insp_title_col = HUD_GOLD;
            game_format_value(ins->data.collector.banked, banked_s, sizeof banked_s);
            snprintf(insp_extra_s, sizeof insp_extra_s, "banked $%s", banked_s);
            break;
        default:
            break;
        }
    }
    if (ore) {
        game_format_value(ore->value, val_a, sizeof val_a);
        game_format_value(ore->ceiling, val_b, sizeof val_b);
        snprintf(insp_item_s, sizeof insp_item_s, "ore $%s / max $%s", val_a, val_b);
    }

    // Floating panels around the screen edges. Mach_Font sizes are multiples of the 8px
    // bitmap glyph (16 -> 2x, 8 -> 1x) so the text stays crisp.
    const Mach_Input *in = &e->input;
    clay_ui_begin(&a->clay, r, (Clay_Vector2){in->mouse.x, in->mouse.y},
                  in->mouse_down[MOUSE_LEFT]);

    // Top-left: the always-on status.
    HUD_PANEL_AT("hud-status", CLAY_ATTACH_POINT_LEFT_TOP, 10, 10) {
        CLAY_TEXT(clay_string(money_s), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = HUD_GOLD }));
        CLAY_TEXT(clay_string(tool_s),  CLAY_TEXT_CONFIG({ .fontSize = 8,  .textColor = HUD_GREEN }));
        if (g->paused)
            CLAY_TEXT(CLAY_STRING("PAUSED"), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_AMBER }));
    }

    // Top-center: what the cursor is pointing at.
    if (ins || ore) {
        HUD_PANEL_AT("hud-inspect", CLAY_ATTACH_POINT_CENTER_TOP, 0, 10) {
            CLAY_TEXT(clay_string(insp_title), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = insp_title_col }));
            if (insp_sub_s[0])
                CLAY_TEXT(clay_string(insp_sub_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            if (insp_extra_s[0])
                CLAY_TEXT(clay_string(insp_extra_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GOLD }));
            if (insp_item_s[0])
                CLAY_TEXT(clay_string(insp_item_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_WHITE }));
        }
    }

    // Bottom-left: the F3 diagnostics.
    if (g->show_debug) {
        HUD_PANEL_AT("hud-debug", CLAY_ATTACH_POINT_LEFT_BOTTOM, 10, -10) {
            CLAY_TEXT(clay_string(fps_s),    CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(counts_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(hover_s),  CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(cam_s),    CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
        }
    }

    // Bottom-center: the controls, on screen until a real menu exists to teach them.
    HUD_PANEL_AT("hud-controls", CLAY_ATTACH_POINT_CENTER_BOTTOM, 0, -10) {
        CLAY_TEXT(CLAY_STRING("(1)drop (2)belt (3)upgr (4)collect (5)del (R)rotate (Space)pause (F3)info"),
                  CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
    }

    clay_ui_render(&a->clay, r);
}

void app_shutdown(App *a, Mach_Engine *e) {
    (void)e;
    clay_ui_shutdown(&a->clay);
    game_shutdown(&a->game);
}
