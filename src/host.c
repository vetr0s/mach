// Dev host with hot reload (dev builds only).
//
// Owns the window, the engine, and the persistent App memory, and never gets
// reloaded. The game logic lives in a shared library (game_lib.c) that this host
// dlopen's; when the library changes on disk, the host swaps in the new code while
// keeping the same App memory, so state survives the reload. Layout changes to any
// persistent struct still need a full restart.
//
// The library reaches its own copy of the engine (see game_lib.c); the host's copy
// here is what creates the window and drives the frame lifecycle. Both operate on
// the one host-owned Engine, passed into the game by pointer.
//
// GAME_LIB_PATH is defined by build.sh (platform-specific extension).

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Engine (own copy; state is all pointer-passed, so the library's copy and this
// one share the same Engine/App data).
#define MACH_IMPLEMENTATION
#include "mach.h"

// Game types and the reload-interface signatures (NOT the game .c sources: those
// are compiled into the library and reached only through the pointers below).
#include "game/app.h"

#ifndef GAME_LIB_PATH
#error "GAME_LIB_PATH must be defined by the build (path to the game shared library)"
#endif

// Time to let a changed library settle on disk before loading it, so a reload
// never catches a half-written file mid-build.
#define RELOAD_SETTLE_MS 150

// The five game entry points, resolved from the shared library each reload.
typedef struct {
    Engine_Config (*engine_config)(void);
    void (*init)(App *, Engine *);
    void (*update)(App *, Engine *, f32);
    void (*render)(App *, Engine *);
    void (*shutdown)(App *, Engine *);
} Game_Api;

static void *g_handle = NULL;       // current dlopen handle
static char  g_loaded_path[512];    // temp copy backing g_handle (unlinked on swap)
static u32   g_reload_counter = 0;

static time_t lib_mtime(void) {
    struct stat st;
    if (stat(GAME_LIB_PATH, &st) != 0) return 0;
    return st.st_mtime;
}

static b32 copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return MACH_FALSE;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return MACH_FALSE; }

    char buf[1 << 16];
    size_t n;
    b32 ok = MACH_TRUE;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = MACH_FALSE; break; }
    }
    if (ferror(in)) ok = MACH_FALSE;
    fclose(in);
    fclose(out);
    if (!ok) unlink(dst);
    return ok;
}

// Load (or reload) the game library. Copies the built library to a unique path
// first so the loader always maps a fresh image and the next build can overwrite
// the original while we hold our copy. On any failure the previous handle and
// *api are left untouched, so the running game keeps the last good code.
static b32 game_api_reload(Game_Api *api) {
    char next[512];
    snprintf(next, sizeof next, "%s.reload%u", GAME_LIB_PATH, g_reload_counter + 1);
    if (!copy_file(GAME_LIB_PATH, next)) {
        LOG_ERROR("hot reload: could not copy %s", GAME_LIB_PATH);
        return MACH_FALSE;
    }

    void *h = dlopen(next, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        LOG_ERROR("hot reload: dlopen failed: %s", dlerror());
        unlink(next);
        return MACH_FALSE;
    }

    // Object-pointer to function-pointer via &field avoids the ISO C cast warning.
    Game_Api n;
    b32 ok = MACH_TRUE;
#define LOAD(field, sym) do { \
        *(void **)(&n.field) = dlsym(h, sym); \
        if (!n.field) { LOG_ERROR("hot reload: missing symbol %s", sym); ok = MACH_FALSE; } \
    } while (0)
    LOAD(engine_config, "game_engine_config");
    LOAD(init,          "app_init");
    LOAD(update,        "app_update");
    LOAD(render,        "app_render");
    LOAD(shutdown,      "app_shutdown");
#undef LOAD

    if (!ok) { dlclose(h); unlink(next); return MACH_FALSE; }

    // Swap in the new image and retire the old one.
    if (g_handle) { dlclose(g_handle); unlink(g_loaded_path); }
    g_handle = h;
    snprintf(g_loaded_path, sizeof g_loaded_path, "%s", next);
    g_reload_counter++;
    *api = n;
    return MACH_TRUE;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Game_Api api;
    if (!game_api_reload(&api)) {
        LOG_ERROR("failed to load game library %s", GAME_LIB_PATH);
        return 1;
    }

    Engine engine = {0};
    if (!engine_init(&engine, api.engine_config())) {
        return 1;
    }

    // App lives in host-owned memory so it survives library reloads.
    App *app = (App *)calloc(1, sizeof(App));
    if (!app) { engine_shutdown(&engine); return 1; }
    api.init(app, &engine);

    time_t last_mtime = lib_mtime();
    u32    pending_at = 0;   // engine ticks when a change was first seen; 0 = none

    LOG_INFO("entering main loop (hot reload watching %s)", GAME_LIB_PATH);
    while (engine_running(&engine)) {
        // Watch the library; reload once it has settled after a change.
        time_t m = lib_mtime();
        if (m != 0 && m != last_mtime && pending_at == 0) {
            pending_at = engine_ticks_ms();
        }
        if (pending_at != 0 && engine_ticks_ms() - pending_at >= RELOAD_SETTLE_MS) {
            last_mtime = lib_mtime();
            pending_at = 0;
            if (game_api_reload(&api)) LOG_INFO("game hot-reloaded");
            else LOG_ERROR("hot reload failed; keeping previous build");
        }

        f32 dt = engine_frame_begin(&engine);

        api.update(app, &engine, dt);

        if (engine_render_begin(&engine)) {
            api.render(app, &engine);
            engine_render_end(&engine);
        }

        engine_frame_end(&engine);
    }
    LOG_INFO("exited main loop");

    api.shutdown(app, &engine);
    free(app);
    engine_shutdown(&engine);
    if (g_handle) { dlclose(g_handle); unlink(g_loaded_path); }
    return 0;
}
