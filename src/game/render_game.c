// Game scene rendering implementation (included into mach.c).

#include "render_game.h"
#include "world/world.h"

// Fixed directional light, slightly off-axis so the three visible cube faces
// each catch a different amount of light (this is what makes the iso "pop").
#define SCENE_LIGHT ((Vec3){-0.55f, -1.0f, -0.35f})

// Build a translate * scale model matrix (no rotation needed for boxes).
static Mat4 model_ts(Vec3 pos, Vec3 scale) {
    return mat4_mul(mat4_translate(pos), mat4_scale(scale));
}

void game_render_init(Game_Render *gr, SDL_GPUDevice *device) {
    gr->cube = mesh_cube(device);
    gr->plane = mesh_plane(device);
}

void game_render_shutdown(Game_Render *gr, SDL_GPUDevice *device) {
    mesh_destroy(device, &gr->cube);
    mesh_destroy(device, &gr->plane);
}

static void draw_miner(Draw *dr, Game_Render *gr, const Entity_Miner *m) {
    Vec3 pos = {(f32)m->grid_x, 0.4f, (f32)m->grid_y};
    Mat4 model = model_ts(pos, (Vec3){0.8f, 0.8f, 0.8f});
    draw_mesh(dr, &gr->cube, model, (Vec4){0.55f, 0.35f, 0.17f, 1.0f});
}

static void draw_storage(Draw *dr, Game_Render *gr, const Entity_Storage *s) {
    Vec3 pos = {(f32)s->grid_x, 0.4f, (f32)s->grid_y};
    Mat4 model = model_ts(pos, (Vec3){0.8f, 0.8f, 0.8f});
    draw_mesh(dr, &gr->cube, model, (Vec4){0.6f, 0.6f, 0.62f, 1.0f});

    // Inner fill cube scaled by stored ore fraction.
    if (s->ore_capacity > 0 && s->ore_stored > 0) {
        f32 frac = (f32)s->ore_stored / (f32)s->ore_capacity;
        if (frac > 1.0f) frac = 1.0f;
        f32 hgt = 0.7f * frac;
        Vec3 fpos = {(f32)s->grid_x, 0.05f + hgt * 0.5f, (f32)s->grid_y};
        Mat4 fmodel = model_ts(fpos, (Vec3){0.5f, hgt, 0.5f});
        draw_mesh(dr, &gr->cube, fmodel, (Vec4){0.85f, 0.45f, 0.2f, 1.0f});
    }
}

void game_render_draw(Draw *dr, Game_Render *gr, const Game_State *game) {
    Mat4 view_proj = camera_view_proj(&game->camera);
    draw_begin_3d(dr, view_proj, SCENE_LIGHT);

    // Ground plane covering the whole grid, centered on it.
    f32 c = (f32)WORLD_GRID_SIZE * 0.5f;
    Mat4 ground = model_ts((Vec3){c, 0.0f, c},
                           (Vec3){(f32)WORLD_GRID_SIZE, 1.0f, (f32)WORLD_GRID_SIZE});
    draw_mesh(dr, &gr->plane, ground, (Vec4){0.16f, 0.20f, 0.24f, 1.0f});

    // Machines.
    World *w = game->world;
    if (w) {
        for (i32 i = 0; i < w->entity_count; i++) {
            const Entity *e = &w->entities[i];
            if (e->type == ENTITY_MINER)   draw_miner(dr, gr, &e->data.miner);
            else if (e->type == ENTITY_STORAGE) draw_storage(dr, gr, &e->data.storage);
        }
    }

    // Hover preview: a flat highlighted tile on the ground.
    if (game->hover_valid) {
        Vec4 color;
        if (!game->hover_can_place)            color = (Vec4){0.9f, 0.3f, 0.3f, 1.0f};
        else if (game->selected_tool == TOOL_STORAGE) color = (Vec4){0.5f, 0.6f, 0.9f, 1.0f};
        else                                   color = (Vec4){0.4f, 0.85f, 0.4f, 1.0f};

        Vec3 pos = {(f32)game->hover_grid_x, 0.02f, (f32)game->hover_grid_y};
        Mat4 model = model_ts(pos, (Vec3){0.95f, 0.04f, 0.95f});
        draw_mesh(dr, &gr->cube, model, color);
    }
}
