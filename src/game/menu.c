#include "menu.h"

// SKELETON STUB. The real title screen (New Game / Continue / Quit, drawn through
// Clay, with input handling and transitions) is built on top of this. For now it
// drops straight into the game so behavior is unchanged from before the menu seam
// existed. game_init starts on SCREEN_PLAYING today; the menu work flips the default
// to SCREEN_MENU and fills this in.
void menu_frame(Game_State *g, Mach *m) {
    (void)m;
    g->screen = SCREEN_PLAYING;
}
