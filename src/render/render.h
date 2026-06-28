// Isometric rendering utilities and world visualization.

#ifndef RENDER_H
#define RENDER_H

#include "../base/base.h"
#include "../ui/ui.h"
#include "../world/world.h"
#include "../math/math.h"
#include "font.h"

// Convert grid coordinates to isometric screen coordinates
Vec2 grid_to_isometric(i32 grid_x, i32 grid_y, i32 tile_size);

// Drawing primitives
void render_rect_filled(SDL_Renderer *rend, i32 x, i32 y, i32 w, i32 h, u8 r, u8 g, u8 b, u8 a);
void render_rect_outline(SDL_Renderer *rend, i32 x, i32 y, i32 w, i32 h, u8 r, u8 g, u8 b);
void render_circle_filled(SDL_Renderer *rend, i32 cx, i32 cy, i32 radius, u8 r, u8 g, u8 b, u8 a);
void render_line(SDL_Renderer *rend, i32 x1, i32 y1, i32 x2, i32 y2, u8 r, u8 g, u8 b);

// Rendering functions
void render_world(UI_Context *ui, World *w, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom);
void render_hover_preview(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom, i32 tool);
void render_debug_text(SDL_Renderer *rend, i32 fps, i32 tool, i32 screen_w, i32 screen_h);

#endif
