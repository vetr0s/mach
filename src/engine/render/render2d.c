// 2D renderer implementation over SDL_Renderer (included into mach.c).

#include "render2d.h"
#include "image.h"
#include "../debug.h"

// Vec4 [0,1] color -> SDL float color.
static SDL_FColor to_fcolor(Vec4 c) {
    return (SDL_FColor){c.x, c.y, c.z, c.w};
}

// --- Lifecycle --------------------------------------------------------------

b32 r2d_init(Renderer *r, SDL_Window *window) {
    r->window = window;
    r->sdl = SDL_CreateRenderer(window, NULL);
    if (!r->sdl) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    // Our loop has its own frame cap, so disable vsync and let it govern.
    SDL_SetRenderVSync(r->sdl, 0);
    SDL_SetRenderDrawBlendMode(r->sdl, SDL_BLENDMODE_BLEND);

    // Render in window-point coordinates so mouse events (also points) line up;
    // SDL scales to the HiDPI framebuffer for us.
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    r->width = w;
    r->height = h;
    SDL_SetRenderLogicalPresentation(r->sdl, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    r->font = font_create(r->sdl);
    if (!r->font) {
        SDL_DestroyRenderer(r->sdl);
        r->sdl = NULL;
        return MACH_FALSE;
    }

    LOG_INFO("2D renderer ready (%dx%d, driver: %s)", r->width, r->height,
             SDL_GetRendererName(r->sdl));
    return MACH_TRUE;
}

void r2d_shutdown(Renderer *r) {
    if (r->font) { font_destroy(r->font); r->font = NULL; }
    if (r->sdl) { SDL_DestroyRenderer(r->sdl); r->sdl = NULL; }
    LOG_INFO("2D renderer shut down");
}

// --- Frame ------------------------------------------------------------------

void r2d_begin(Renderer *r, u8 clear_r, u8 clear_g, u8 clear_b) {
    SDL_SetRenderDrawColor(r->sdl, clear_r, clear_g, clear_b, 255);
    SDL_RenderClear(r->sdl);
}

void r2d_present(Renderer *r) {
    SDL_RenderPresent(r->sdl);
}

// --- Primitives -------------------------------------------------------------

void r2d_fill_rect(Renderer *r, f32 x, f32 y, f32 w, f32 h, Vec4 color) {
    SDL_SetRenderDrawColorFloat(r->sdl, color.x, color.y, color.z, color.w);
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(r->sdl, &rect);
}

// Fill a convex polygon as a triangle fan via SDL_RenderGeometry.
void r2d_fill_poly(Renderer *r, const Vec2 *pts, i32 n, Vec4 color) {
    if (n < 3 || n > 16) return;
    SDL_Vertex v[16];
    SDL_FColor col = to_fcolor(color);
    for (i32 i = 0; i < n; i++) {
        v[i].position = (SDL_FPoint){pts[i].x, pts[i].y};
        v[i].color = col;
        v[i].tex_coord = (SDL_FPoint){0.0f, 0.0f};
    }
    int idx[(16 - 2) * 3];
    int m = 0;
    for (i32 i = 1; i < n - 1; i++) {
        idx[m++] = 0; idx[m++] = i; idx[m++] = i + 1;
    }
    SDL_RenderGeometry(r->sdl, NULL, v, n, idx, m);
}

void r2d_text(Renderer *r, f32 x, f32 y, f32 scale, const char *text, Vec4 color) {
    if (!text) return;
    Font *font = r->font;

    SDL_SetTextureColorModFloat(font->atlas, color.x, color.y, color.z);
    SDL_SetTextureAlphaModFloat(font->atlas, color.w);

    f32 gw = (f32)font->glyph_w * scale;
    f32 gh = (f32)font->glyph_h * scale;
    f32 adv = (f32)font->advance * scale;
    f32 cur_x = x;
    for (const char *c = text; *c; c++) {
        SDL_FRect src;
        if (font_glyph_src(font, *c, &src)) {
            SDL_FRect dst = {cur_x, y, gw, gh};
            SDL_RenderTexture(r->sdl, font->atlas, &src, &dst);
        }
        cur_x += adv;
    }
}

// --- Sprites ----------------------------------------------------------------

SDL_Texture *r2d_load_texture(Renderer *r, const char *path) {
    Image img = image_load(path);
    if (!img.data) {
        LOG_ERROR("r2d_load_texture: failed to load %s", path);
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTexture(r->sdl, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (tex) {
        SDL_UpdateTexture(tex, NULL, img.data, img.width * 4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    } else {
        LOG_ERROR("r2d_load_texture: SDL_CreateTexture failed: %s", SDL_GetError());
    }
    image_free(&img);
    return tex;
}

void r2d_sprite(Renderer *r, SDL_Texture *tex, f32 x, f32 y, f32 scale, Vec4 tint) {
    if (!tex) return;
    f32 tw, th;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_SetTextureColorModFloat(tex, tint.x, tint.y, tint.z);
    SDL_SetTextureAlphaModFloat(tex, tint.w);
    SDL_FRect dst = {x, y, tw * scale, th * scale};
    SDL_RenderTexture(r->sdl, tex, NULL, &dst);
}

// --- Isometric projection ---------------------------------------------------

Vec2 iso_to_screen(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev) {
    f32 iso_x = (grid_x - grid_y) * (ISO_TILE_W * 0.5f);
    f32 iso_y = (grid_x + grid_y) * (ISO_TILE_H * 0.5f) - elev * ISO_ELEV;
    return (Vec2){
        (iso_x - cam->pan.x) * cam->zoom + screen_w * 0.5f,
        (iso_y - cam->pan.y) * cam->zoom + screen_h * 0.5f,
    };
}

Vec2 screen_to_iso(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y) {
    f32 iso_x = (screen_x - screen_w * 0.5f) / cam->zoom + cam->pan.x;
    f32 iso_y = (screen_y - screen_h * 0.5f) / cam->zoom + cam->pan.y;
    // Invert the elev-0 projection: a = gx-gy, b = gx+gy.
    f32 a = iso_x / (ISO_TILE_W * 0.5f);
    f32 b = iso_y / (ISO_TILE_H * 0.5f);
    return (Vec2){(a + b) * 0.5f, (b - a) * 0.5f};
}
