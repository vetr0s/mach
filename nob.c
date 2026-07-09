// Build the mach game. Bootstrap once, then it rebuilds itself:
//
//   cc -o nob nob.c   (or: cl nob.c)
//   ./nob [debug|release|hot|game]
//
// Targets (default debug):
//   debug     static monolith, -g -O0   -> build/mach_debug
//   release   static monolith, -O3      -> build/mach_release
//   hot       the dev loop: build host + game library, run the host, then watch
//             src/ and rebuild the library on every change so the running game
//             swaps it in live. Ctrl-C (or closing the game) stops the watch.
//   game      rebuild only the game library (one shot, for manual reloads)
//
// No setup step: RGFW is embedded in mach.h (windowing), GL comes from the OS.
// hot and game are dev-only and Unix-only (they use dlopen); on Windows only
// debug and release are available. See ARCHITECTURE.md.

#define NOB_IMPLEMENTATION
#include "nob.h"

#if !defined(_WIN32)
    #include <signal.h>      // kill, SIGTERM
    #include <sys/wait.h>    // waitpid, WNOHANG
#endif

#define BUILD_DIR "build"

// Platform toolchain and the libs RGFW + OpenGL need.
#if defined(_WIN32)
    #define CC "cl.exe"
    #define LIB_EXT ".dll"
    static const char *PLATFORM_LIBS[] = {"opengl32.lib", "winmm.lib"};
#elif defined(__APPLE__)
    #define CC "clang"
    #define LIB_EXT ".dylib"
    static const char *PLATFORM_LIBS[] = {
        "-framework", "Cocoa", "-framework", "CoreVideo",
        "-framework", "IOKit", "-framework", "OpenGL",
    };
#elif defined(__linux__)
    #define CC "clang"
    #define LIB_EXT ".so"
    static const char *PLATFORM_LIBS[] = {"-lX11", "-lXrandr", "-lGL", "-lm", "-ldl"};
#else
    #error "Unsupported platform"
#endif

#define GAME_LIB BUILD_DIR "/libmach_game" LIB_EXT

static void cmd_append_libs(Nob_Cmd *cmd) {
    for (size_t i = 0; i < NOB_ARRAY_LEN(PLATFORM_LIBS); i++) {
        nob_cmd_append(cmd, PLATFORM_LIBS[i]);
    }
}

#if !defined(_WIN32)
// Collect every .c/.h under a directory (recursively) so the hot watcher can
// tell when any source changed. Appends absolute-ish paths into `out`.
static bool collect_sources(const char *dir, Nob_File_Paths *out) {
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir(dir, &children)) return false;
    for (size_t i = 0; i < children.count; i++) {
        const char *name = children.items[i];
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        const char *path = nob_temp_sprintf("%s/%s", dir, name);
        switch (nob_get_file_type(path)) {
            case NOB_FILE_DIRECTORY:
                if (!collect_sources(path, out)) return false;
                break;
            case NOB_FILE_REGULAR:
                if (nob_sv_end_with(nob_sv_from_cstr(name), ".c") ||
                    nob_sv_end_with(nob_sv_from_cstr(name), ".h")) {
                    nob_da_append(out, nob_temp_strdup(path));
                }
                break;
            default: break;
        }
    }
    return true;
}
#endif

#if defined(__linux__)
// Fail early with install hints when the X11/GL dev headers are missing: the
// usual first-run failure on a fresh box. RGFW needs Xlib, Xcursor, Xrandr,
// Xext, XInput2, and GLX headers; the libs themselves ship with the OS. Keep
// this probe in sync with the includes in mach.h's X11 backend, or a missing
// header slips past the preflight and blows up mid-compile instead.
static bool check_x11_headers(void) {
    const char *probe = BUILD_DIR "/.x11check.c";
    const char *probe_src =
        "#include <X11/Xlib.h>\n"
        "#include <X11/Xcursor/Xcursor.h>\n"
        "#include <X11/extensions/Xrandr.h>\n"
        "#include <X11/extensions/sync.h>\n"
        "#include <X11/extensions/shape.h>\n"
        "#include <X11/extensions/XInput2.h>\n"
        "#include <GL/glx.h>\n";
    nob_write_entire_file(probe, probe_src, strlen(probe_src));
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CC, "-fsyntax-only", probe);
    Nob_Cmd_Opt opt = { .stderr_path = "/dev/null" };
    bool ok = nob_cmd_run_opt(&cmd, opt);
    if (!ok) {
        nob_log(NOB_ERROR, "Missing X11/OpenGL development headers. Install them:");
        fprintf(stderr,
            "  Void:          sudo xbps-install -S libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel libglvnd-devel\n"
            "  Debian/Ubuntu: sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxext-dev libxi-dev libgl1-mesa-dev\n"
            "  Fedora:        sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel mesa-libGL-devel\n"
            "  Arch:          sudo pacman -S libx11 libxrandr libxcursor libxext libxi mesa\n");
    }
    return ok;
}
#endif

// Static monolith build (debug or release), the shipping path: src/mach.c
// compiles the whole game in one translation unit.
//
// The release binary is what gets attached to a GitHub release, so it has to
// run on a machine that has never seen a toolchain. That costs two extra flags:
// /MT on Windows statically links the CRT (the default /MD needs the Visual C++
// redistributable installed), and -arch on macOS emits a universal binary so
// one file covers both Apple silicon and Intel. Debug builds skip both: the
// second arch just doubles the compile for no benefit on the dev loop.
static bool build_monolith(bool release) {
    const char *out = release ? BUILD_DIR "/mach_release" : BUILD_DIR "/mach_debug";
    nob_log(NOB_INFO, "Compiling: %s", out);
    Nob_Cmd cmd = {0};
#if defined(_WIN32)
    nob_cmd_append(&cmd, CC, "/std:c11", "/W4", "/Isrc");
    if (release) nob_cmd_append(&cmd, "/O2", "/DNDEBUG", "/MT");
    else         nob_cmd_append(&cmd, "/Od", "/Zi");
    nob_cmd_append(&cmd, nob_temp_sprintf("/Fe%s.exe", out));
    nob_cmd_append(&cmd, "src/mach.c", "/link");
    cmd_append_libs(&cmd);
#else
    nob_cmd_append(&cmd, CC, "-std=c99", "-Wall", "-Wextra", "-Isrc");
    if (release) nob_cmd_append(&cmd, "-O3", "-DNDEBUG");
    else         nob_cmd_append(&cmd, "-g", "-O0");
#if defined(__APPLE__)
    if (release) nob_cmd_append(&cmd, "-arch", "arm64", "-arch", "x86_64");
#endif
    nob_cmd_append(&cmd, "-o", out, "src/mach.c");
    cmd_append_libs(&cmd);
#endif
    return nob_cmd_run(&cmd);
}

#if !defined(_WIN32)
// The hot-reloadable game library from src/game_lib.c. A running host swaps it
// in when it changes on disk.
static bool build_game_lib(void) {
    nob_log(NOB_INFO, "Compiling: %s", GAME_LIB);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CC, "-std=c99", "-Wall", "-Wextra", "-Isrc", "-g", "-O0");
    #if defined(__APPLE__)
        nob_cmd_append(&cmd, "-dynamiclib");
    #else
        nob_cmd_append(&cmd, "-shared", "-fPIC");
    #endif
    nob_cmd_append(&cmd, "-o", GAME_LIB, "src/game_lib.c");
    cmd_append_libs(&cmd);
    return nob_cmd_run(&cmd);
}

// The dev host that dlopen's the game library. Not hot-reloadable itself.
static bool build_host(const char *out) {
    nob_log(NOB_INFO, "Compiling: %s", out);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CC, "-std=c99", "-Wall", "-Wextra", "-Isrc", "-g", "-O0");
    nob_cmd_append(&cmd, nob_temp_sprintf("-DGAME_LIB_PATH=\"%s\"", GAME_LIB));
    nob_cmd_append(&cmd, "-o", out, "src/host.c");
    cmd_append_libs(&cmd);
    return nob_cmd_run(&cmd);
}

// Build host + library, run the host, then watch src/ and rebuild the library
// on every change so the running host swaps it in live.
static bool build_hot(void) {
    if (!build_game_lib()) return false;
    const char *host = BUILD_DIR "/mach_hot";
    if (!build_host(host)) return false;

    // Run the host asynchronously; we stay in charge to watch for source edits.
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, host);
    Nob_Proc game = nob_cmd_run_async_and_reset(&cmd);
    if (game == NOB_INVALID_PROC) return false;

    nob_log(NOB_INFO, "Watching src/ for changes (Ctrl-C or close the game to stop)");
    struct timespec half_sec = { .tv_sec = 0, .tv_nsec = 500 * 1000 * 1000 };
    for (;;) {
        nanosleep(&half_sec, NULL);

        // Has the game exited (window closed)? Reap without blocking; a returned
        // pid means it's gone, so stop the watch.
        if (waitpid(game, NULL, WNOHANG) == game) { game = NOB_INVALID_PROC; break; }

        // Rebuild the library when any source is newer than it. After a build the
        // library is the newest file, so this settles until the next edit.
        Nob_File_Paths sources = {0};
        if (!collect_sources("src", &sources)) { nob_da_free(sources); break; }
        int rb = nob_needs_rebuild(GAME_LIB, sources.items, sources.count);
        nob_da_free(sources);
        nob_temp_reset();
        if (rb < 0) break;
        if (rb == 0) continue;

        if (build_game_lib()) {
            nob_log(NOB_INFO, "Reload ready; the running game picks it up");
        } else {
            nob_log(NOB_WARNING, "Build failed; the game keeps the last good code. Still watching.");
        }
    }

    if (game != NOB_INVALID_PROC) {
        kill(game, SIGTERM);
        nob_proc_wait(game);
    }
    nob_log(NOB_INFO, "Game exited; watch stopped.");
    return true;
}
#endif // !_WIN32

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *target = (argc > 1) ? argv[1] : "debug";

    if (!nob_mkdir_if_not_exists(BUILD_DIR)) return 1;

#if defined(__linux__)
    if (!check_x11_headers()) return 1;
#endif

    if (strcmp(target, "debug") == 0)   return build_monolith(false) ? 0 : 1;
    if (strcmp(target, "release") == 0) return build_monolith(true)  ? 0 : 1;

#if defined(_WIN32)
    if (strcmp(target, "hot") == 0 || strcmp(target, "game") == 0) {
        nob_log(NOB_ERROR, "'%s' is Unix-only (hot reload uses dlopen). Use debug or release.", target);
        return 1;
    }
#else
    if (strcmp(target, "game") == 0) return build_game_lib() ? 0 : 1;
    if (strcmp(target, "hot") == 0)  return build_hot()      ? 0 : 1;
#endif

    nob_log(NOB_ERROR, "Unknown target: %s. Use debug | release | hot | game.", target);
    return 1;
}
