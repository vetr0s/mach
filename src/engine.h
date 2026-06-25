#ifndef ENGINE_H
#define ENGINE_H

#include <SDL3/SDL.h>

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    int           running;
} Engine;

int  engine_init(Engine *e, const char *title, int w, int h);
void engine_run(Engine *e);
void engine_shutdown(Engine *e);

#endif
