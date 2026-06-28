// Core engine lifecycle and main loop orchestration.

#ifndef CORE_H
#define CORE_H

#include "../base/base.h"
#include "../os.h"
#include "../ui.h"
#include "../app.h"
#include "../render/gpu.h"

typedef struct {
    UI_Context    ui;
    Gpu_Renderer  gpu;
    i32           running;
} Core_Engine;

b32  core_init(Core_Engine *e, const char *title, i32 w, i32 h);
void core_run(Core_Engine *e, Engine_App *app);
void core_shutdown(Core_Engine *e);

#endif
