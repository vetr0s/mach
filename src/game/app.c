// Game application glue (included into mach.c): wires the game state into the
// loop's per-frame entry points.

#include "app.h"
#include "game.h"
#include "render_game.h"
#include <stdio.h>

// The game defines its window and engine policy here.
Engine_Config game_engine_config(void) {
    return (Engine_Config){
        .title = "mach",
        .width = 1280,
        .height = 720,
        .fullscreen = MACH_FALSE,
        .resizable = MACH_TRUE,
        .clear_color = {0.118f, 0.161f, 0.231f, 1.0f},   // dark slate (0x1e293b)
        .escape_quits = MACH_TRUE,                        // dev convenience, for now
        .target_fps = 120,
    };
}

void app_init(App *a, Engine *e) {
    game_init(&a->game);
    clay_ui_init(&a->clay, &e->r2d);
}

void app_update(App *a, Engine *e, f32 dt) {
    // Input works in the renderer's current pixel space, which may differ from
    // the requested size (fullscreen, or a resized window).
    game_process_input(&a->game, &e->input, (f32)e->r2d.width, (f32)e->r2d.height, dt);
    game_tick(&a->game, dt);
}

// HUD colors, in Clay's 0-255 convention.
#define HUD_PANEL ((Clay_Color){24, 26, 32, 205})
#define HUD_GOLD  ((Clay_Color){230, 199, 89, 255})
#define HUD_GREEN ((Clay_Color){140, 216, 140, 255})
#define HUD_AMBER ((Clay_Color){255, 166, 51, 255})
#define HUD_GREY  ((Clay_Color){166, 168, 184, 255})

void app_render(App *a, Engine *e) {
    Renderer *r = &e->r2d;
    const Game_State *g = &a->game;
    game_render_draw(r, g, &e->frame_arena);

    static const char *tool_names[] = {"None", "Dropper", "Conveyor", "Upgrader",
                                       "Collector", "Delete"};
    static const char *dir_names[]  = {"N", "E", "S", "W"};
    const char *tool = (g->selected_tool >= 0 && g->selected_tool < (i32)ARRAY_COUNT(tool_names))
                           ? tool_names[g->selected_tool] : "?";
    const char *facing = (g->place_dir >= 0 && g->place_dir < (i32)ARRAY_COUNT(dir_names))
                             ? dir_names[g->place_dir] : "?";

    // HUD strings. These buffers must outlive clay_ui_render (Clay keeps the char
    // pointers, not copies), so they stay in scope for the whole function.
    char money_s[32], tool_s[48], fps_s[24], counts_s[64], hover_s[48], cam_s[48];
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

    // A top-left panel: money always, tool/facing and a pause marker under it, and the
    // rest of the diagnostics only when F3 is on. Font sizes are multiples of the 8px
    // bitmap glyph (16 -> 2x, 8 -> 1x) so the text stays crisp.
    const Input *in = &e->input;
    clay_ui_begin(&a->clay, r, (Clay_Vector2){in->mouse.x, in->mouse.y},
                  in->mouse_down[MOUSE_LEFT]);
    CLAY(CLAY_ID("hud"),
         { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .padding = CLAY_PADDING_ALL(10),
                       .childGap = 4,
                       .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) } },
           .backgroundColor = HUD_PANEL,
           .cornerRadius = CLAY_CORNER_RADIUS(6) }) {
        CLAY_TEXT(clay_string(money_s), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = HUD_GOLD }));
        CLAY_TEXT(clay_string(tool_s),  CLAY_TEXT_CONFIG({ .fontSize = 8,  .textColor = HUD_GREEN }));
        if (g->paused)
            CLAY_TEXT(CLAY_STRING("PAUSED"), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_AMBER }));
        if (g->show_debug) {
            CLAY_TEXT(clay_string(fps_s),    CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(counts_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(hover_s),  CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(cam_s),    CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(CLAY_STRING("1drop 2belt 3upgr 4collect 5del   R rotate   Space pause   F3 info"),
                      CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
        }
    }
    clay_ui_render(&a->clay, r);
}

void app_shutdown(App *a, Engine *e) {
    (void)e;
    clay_ui_shutdown(&a->clay);
    game_shutdown(&a->game);
}
