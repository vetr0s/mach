// Unity build root. Includes all engine code and defines main().

/*
  MACH ENGINE TODO
  ================

  CORE ENGINE
  [x] Game loop: proper update/render separation, fixed timestep
  [x] Input system: keyboard, mouse, controller abstractions
  [ ] Event system: input, collision, game events
  [ ] Memory: custom allocators, arena allocation

  RENDERING
  [ ] Sprite system: loading, drawing, transforms
  [ ] Texture loading: PNG/BMP support
  [ ] Drawing primitives: rectangles, circles, lines
  [ ] Camera system: viewport, scrolling
  [ ] Sprite batching: efficient rendering

  MATH
  [x] Vector2/3/4 types and operations
  [ ] Matrix4x4 for transforms
  [ ] Quaternions
  [x] Common math utilities (lerp, clamp, etc.)

  GAMEPLAY
  [x] Entity/GameObject system
  [ ] Transform component (position, rotation, scale)
  [ ] Collision detection (AABB, circle)
  [ ] Physics: velocity, gravity, basic rigidbody
  [ ] Animation: sprite animation, tweening

  CONTENT
  [ ] Asset loading pipeline (textures, sprites, levels)
  [ ] Level/tilemap format and loader
  [ ] Data serialization (JSON or custom)

  DEBUGGING
  [ ] Debug draw: collision bounds, grid, vectors
  [ ] Performance profiler
  [ ] Immediate mode debug UI

  AUDIO (later)
  [ ] Audio system
  [ ] Sound and music loading
  [ ] Spatial audio

  SCRIPTING (future)
  [ ] Embedded Lua integration
  [ ] Script hot-reload

  TOOLS (future)
  [ ] Level editor
  [ ] Sprite editor
  [ ] Scene editor

  PLATFORM (scaffolding done, expand as needed)
  [ ] macOS: full testing
  [ ] Linux: build and test
  [ ] Windows: build and test
*/

#include "base/base.h"
#include "os/os.h"
#include "ui/ui.h"
#include "math/math.c"
#include "world/world.c"
#include "render/render.c"
#include "core/core.c"
#include "game/game.c"
#include "debug/debug.h"

// Application entry point. Initialize engine and game, run main loop, clean up.
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Core_Engine engine = {0};
    if (!core_init(&engine, "mach", 1280, 720)) {
        return 1;
    }

    Game_State game = {0};
    game_init(&game);

    core_run(&engine);

    game_shutdown(&game);
    core_shutdown(&engine);

    return 0;
}
