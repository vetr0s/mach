#pragma once

// Opaque SDL types, forward-declared at global scope to keep <SDL3/SDL.h> out
// of this header. These mirror SDL3's own `typedef struct SDL_Window ...`.
struct SDL_Window;
struct SDL_Renderer;

namespace mach {

// Top-level engine lifecycle. Deliberately tiny for now — owns window/renderer
// setup and the main loop as the engine grows.
class Engine {
public:
    // Brings up SDL and creates the window/renderer. Returns false on failure.
    bool init(const char *title, int width, int height);

    // Runs the main loop until the user closes the window. Returns the process
    // exit code.
    int run();

    // Tears down the window/renderer and SDL. Safe to call after a failed init.
    void shutdown();

    ~Engine();

private:
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
};

} // namespace mach
