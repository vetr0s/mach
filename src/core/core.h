// (npt): Core engine lifecycle and main loop orchestration.

#ifndef CORE_H
#define CORE_H

#include "../base/base.h"
#include "../os/os.h"
#include "../ui/ui.h"

typedef struct {
    UI_Context ui;
    i32        running;
} Core_Engine;

int  core_init(Core_Engine *e, const char *title, i32 w, i32 h);
void core_run(Core_Engine *e);
void core_shutdown(Core_Engine *e);

#endif
