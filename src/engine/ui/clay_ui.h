// Clay UI binding: layout by Clay (third_party/clay), drawing by our 2D renderer.
//
// Clay does the UI layout and hands back a list of render commands; clay_ui_render
// walks that list and draws each with r2d. Clay keeps an internal "current context"
// global plus callback pointers, so those are re-pointed every frame in clay_ui_begin
// to survive hot reload (the reloaded library's globals start empty). The context data
// itself lives in a malloc'd block held in host-owned App memory, so it persists across
// a code swap.

#ifndef CLAY_UI_H
#define CLAY_UI_H

#include "../base/base.h"
#include "../render/render2d.h"
#include "../../../third_party/clay/clay.h"

typedef struct {
    Clay_Context *ctx;
    void         *memory;   // malloc backing for Clay's arena (survives hot reload)
    b32           ready;
} ClayUI;

// One-time setup: allocate Clay's arena and initialize it against the renderer's size.
b32  clay_ui_init(ClayUI *ui, Renderer *r);
void clay_ui_shutdown(ClayUI *ui);

// Per-frame. Call clay_ui_begin, declare the layout with CLAY(...) / CLAY_TEXT(...),
// then clay_ui_render to draw it. `mouse`/`mouse_down` feed Clay's pointer state for
// hover/click handling (pass zero/false when there's nothing interactive yet).
void clay_ui_begin(ClayUI *ui, Renderer *r, Clay_Vector2 mouse, b32 mouse_down);
void clay_ui_render(ClayUI *ui, Renderer *r);

// A Clay_String over a null-terminated C string. The chars are not copied, so they
// must outlive this frame's clay_ui_render call.
static inline Clay_String clay_string(const char *s) {
    return (Clay_String){ .length = (i32)strlen(s), .chars = s };
}

#endif
