// Game implementation (included into mach.c).

#include "game.h"
#include "../engine/ui.h"
#include "../engine/debug.h"
#include <math.h>

// Camera zoom limits (orthographic half-height, world units).
#define ZOOM_MIN 3.0f
#define ZOOM_MAX 60.0f

// Set up the isometric camera: an orthographic camera looking along (1,1,1),
// which yields a true isometric projection. Other games can swap this for a
// perspective or top-down camera without touching the renderer.
static void setup_iso_camera(Camera *c, Vec3 target) {
    Vec3 dir = vec3_normalize((Vec3){1.0f, 1.0f, 1.0f});
    c->target = target;
    c->position = vec3_add(target, vec3_scale(dir, 60.0f));
    c->up = (Vec3){0.0f, 1.0f, 0.0f};
    c->projection = CAMERA_ORTHOGRAPHIC;
    c->fov_y = math_radians(60.0f);
    c->ortho_size = 10.0f;
    c->near_plane = 0.1f;
    c->far_plane = 200.0f;
    c->aspect = (f32)SCREEN_WIDTH / (f32)SCREEN_HEIGHT;
}

// Initialize game state with an empty world and spawn test entities for development.
void game_init(Game_State *g) {
    g->world = world_create();
    g->selected_tool = TOOL_NONE;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;
    g->hover_valid = MACH_FALSE;
    g->hover_can_place = MACH_FALSE;

    setup_iso_camera(&g->camera, (Vec3){5.0f, 0.0f, 5.0f});

    if (g->world) {
        world_spawn_miner(g->world, 5, 5);
        world_spawn_storage(g->world, 6, 5);
    }

    LOG_INFO("game initialized (iso ortho camera)");
}

// Update the hovered grid cell by casting a ray from the mouse onto the ground
// plane (y = 0). Grid cell (x,y) is centered at world (x, 0, y).
void game_update_hover(Game_State *g, i32 mouse_x, i32 mouse_y) {
    if (!g) return;

    Vec3 origin, dir;
    camera_screen_ray(&g->camera, (f32)mouse_x, (f32)mouse_y,
                      SCREEN_WIDTH, SCREEN_HEIGHT, &origin, &dir);

    if (fabsf(dir.y) < 1e-6f) { g->hover_valid = MACH_FALSE; return; }
    f32 t = -origin.y / dir.y;
    if (t < 0.0f) { g->hover_valid = MACH_FALSE; return; }

    Vec3 hit = vec3_add(origin, vec3_scale(dir, t));
    g->hover_grid_x = (i32)floorf(hit.x + 0.5f);
    g->hover_grid_y = (i32)floorf(hit.z + 0.5f);
    g->hover_valid = MACH_TRUE;
    g->hover_can_place = world_can_place_at(g->world, g->hover_grid_x, g->hover_grid_y);
}

// Advance game simulation. dt is delta time in seconds since last frame.
void game_tick(Game_State *g, f32 dt) {
    if (!g || !g->world) return;
    (void)dt;
    world_tick(g->world);
}

// Clean up game state and free resources.
void game_shutdown(Game_State *g) {
    if (g && g->world) {
        world_destroy(g->world);
        g->world = NULL;
    }
    LOG_INFO("game shut down");
}

// Handle mouse input for machine placement and deletion.
void game_handle_input(Game_State *g, i32 mouse_x, i32 mouse_y, i32 button) {
    if (!g || !g->world) return;

    game_update_hover(g, mouse_x, mouse_y);
    if (button != 1 || !g->hover_valid) return;

    switch (g->selected_tool) {
    case TOOL_MINER:
        world_spawn_miner(g->world, g->hover_grid_x, g->hover_grid_y);
        break;
    case TOOL_STORAGE:
        world_spawn_storage(g->world, g->hover_grid_x, g->hover_grid_y);
        break;
    case TOOL_DELETE: {
        i32 entity_id = world_get_entity_at(g->world, g->hover_grid_x, g->hover_grid_y);
        if (entity_id != 0) {
            world_despawn(g->world, entity_id);
        }
        break;
    }
    default:
        break;
    }
}

// Toggle the given tool: selecting the active tool again clears it.
static void toggle_tool(Game_State *g, Tool tool) {
    g->selected_tool = (g->selected_tool == (i32)tool) ? TOOL_NONE : (i32)tool;
    LOG_DEBUG("selected tool: %d", g->selected_tool);
}

// Handle keyboard input for tool selection and actions.
void game_handle_key(Game_State *g, SDL_Scancode scancode) {
    if (!g) return;

    switch (scancode) {
    case SDL_SCANCODE_1: toggle_tool(g, TOOL_MINER);   break;
    case SDL_SCANCODE_2: toggle_tool(g, TOOL_STORAGE); break;
    case SDL_SCANCODE_3: toggle_tool(g, TOOL_DELETE);  break;
    default: break;
    }
}

// Pan the camera across the ground plane. (dx, dy) is screen-space motion in
// pixels; convert to world units and move along the camera's right/forward axes.
void game_camera_pan(Game_State *g, f32 dx, f32 dy) {
    if (!g) return;

    f32 world_per_pixel = (2.0f * g->camera.ortho_size) / (f32)SCREEN_HEIGHT;

    Vec3 view = vec3_normalize(vec3_sub(g->camera.target, g->camera.position));
    Vec3 right = vec3_normalize(vec3_cross(view, g->camera.up));
    Vec3 forward = vec3_normalize((Vec3){view.x, 0.0f, view.z});  // view along ground

    Vec3 move = vec3_add(vec3_scale(right, dx * world_per_pixel),
                         vec3_scale(forward, -dy * world_per_pixel));
    g->camera.target = vec3_add(g->camera.target, move);
    g->camera.position = vec3_add(g->camera.position, move);
}

// Zoom the orthographic camera. Positive delta zooms in (smaller view volume).
void game_camera_zoom(Game_State *g, f32 zoom_delta) {
    if (!g) return;
    g->camera.ortho_size *= (1.0f - zoom_delta);
    g->camera.ortho_size = math_clamp(g->camera.ortho_size, ZOOM_MIN, ZOOM_MAX);
}
