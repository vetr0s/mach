// Isometric rendering utilities and world visualization.

#ifndef RENDER_H
#define RENDER_H

#include "../base/base.h"
#include "../ui/ui.h"
#include "../world/world.h"
#include "../math/math.h"

// Convert grid coordinates to isometric screen coordinates
Vec2 grid_to_isometric(i32 grid_x, i32 grid_y, i32 tile_size);

// Rendering functions
void render_world(UI_Context *ui, World *w, i32 tile_size, i32 offset_x, i32 offset_y);
void render_hover_preview(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size, i32 offset_x, i32 offset_y, i32 tool);

#endif
