// Title screen and app-screen flow. See game.h's App_Screen.
//
// SKELETON STUB: menu_frame currently just enters the game immediately, so the app
// behaves exactly as it did before the menu existed. The main-menu work fills this
// in (title, New Game / Continue / Quit, transitions) without touching the running
// game, which lives behind SCREEN_PLAYING in game.c.

#ifndef MENU_H
#define MENU_H

#include "game.h"

// Draw and drive the title screen for one frame. Sets g->screen to leave the menu.
void menu_frame(Game_State *g, Mach *m);

#endif // MENU_H
