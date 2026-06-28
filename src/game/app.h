// Game application: implements the engine's Engine_App interface.

#ifndef GAME_APP_H
#define GAME_APP_H

#include "../engine/app.h"

// Build the Engine_App the engine will drive. Backed by a single static
// instance, so this is called once from main().
Engine_App game_app(void);

#endif
