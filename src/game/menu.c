#include "menu.h"

// The title screen. Shown on SCREEN_MENU (game_frame dispatches here), drawn through
// Clay on the black clear color; the world isn't drawn behind it. New Game lays a
// fresh world, Continue loads the save if there is one, Quit ends the loop.

// Menu-local Clay colors (the HUD's own palette lives in render_game.c).
#define MENU_PANEL mach_clay_color_of(mach_color_alpha(MACH_COLOR_BG_DIM, 0.92f))
#define MENU_BTN mach_clay_color_of(MACH_COLOR_BG_ACTIVE)
#define MENU_BTN_HOVER mach_clay_color_of(MACH_COLOR_BG_HOVER)
#define MENU_TITLE mach_clay_color_of(MACH_COLOR_YELLOW_WARMER)
#define MENU_TEXT mach_clay_color_of(MACH_COLOR_FG_MAIN)
#define MENU_DIM mach_clay_color_of(MACH_COLOR_FG_DIM)

// A fixed-width, centered, hover-lit menu button. Follow with a CLAY_TEXT label.
#define MENU_BUTTON(id)                                                                            \
    CLAY(CLAY_ID(id), {.layout = {.padding = CLAY_PADDING_ALL(12),                                 \
                                  .sizing = {.width = CLAY_SIZING_FIXED(240)},                     \
                                  .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},                   \
                       .backgroundColor = Clay_Hovered() ? MENU_BTN_HOVER : MENU_BTN,              \
                       .cornerRadius = CLAY_CORNER_RADIUS(6)})

// Start playing a brand-new game.
static void menu_start_new(Game_State *g) {
    game_new(g);
    g->screen = SCREEN_PLAYING;
}

void menu_frame(Game_State *g, Mach *m) {
    Mach_Renderer *r = &m->r2d;
    const Mach_Input *in = &m->input;
    b32 has_save = save_exists(SAVE_PATH);

    mach_clay_ui_begin(&g->clay, r, (Clay_Vector2){in->mouse.x, in->mouse.y},
                       in->mouse_down[MACH_MOUSE_LEFT]);

    // Root fills the screen and centers the panel.
    CLAY(CLAY_ID("menu-root"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}) {
        CLAY(CLAY_ID("menu-panel"), {.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                .padding = CLAY_PADDING_ALL(28),
                                                .childGap = 10,
                                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
                                     .backgroundColor = MENU_PANEL,
                                     .cornerRadius = CLAY_CORNER_RADIUS(10)}) {
            CLAY_TEXT(CLAY_STRING("mach"),
                      CLAY_TEXT_CONFIG({.fontSize = 32, .textColor = MENU_TITLE}));
            CLAY_TEXT(CLAY_STRING("a value-loop belt builder"),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = MENU_DIM}));

            MENU_BUTTON("menu-new") {
                CLAY_TEXT(CLAY_STRING("New Game"),
                          CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = MENU_TEXT}));
            }
            if (has_save) {
                MENU_BUTTON("menu-continue") {
                    CLAY_TEXT(CLAY_STRING("Continue"),
                              CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = MENU_TEXT}));
                }
            }
            MENU_BUTTON("menu-quit") {
                CLAY_TEXT(CLAY_STRING("Quit"),
                          CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = MENU_DIM}));
            }
        }
    }

    mach_clay_ui_render(&g->clay, r);

    // Handle clicks against the finished layout (Clay_PointerOver reads this frame's
    // layout at this frame's pointer, so a click resolves the same frame).
    if (in->mouse_pressed[MACH_MOUSE_LEFT]) {
        if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("menu-new")))) {
            menu_start_new(g);
        } else if (has_save && Clay_PointerOver(Clay_GetElementId(CLAY_STRING("menu-continue")))) {
            if (game_load(g, SAVE_PATH))
                g->screen = SCREEN_PLAYING; // a failed load stays on the menu
        } else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("menu-quit")))) {
            m->running = MACH_FALSE; // the host loop reads mach_running
        }
    }

    // Keyboard: Enter starts a new game. Escape already quits the app (escape_quits).
    if (in->key_pressed[RGFW_keyReturn] || in->key_pressed[RGFW_keyEnter])
        menu_start_new(g);
}
