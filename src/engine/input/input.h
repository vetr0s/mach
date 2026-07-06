// Per-frame input snapshot: keyboard and mouse state the game reads directly,
// instead of draining window events itself.
//
// The engine owns one of these (Engine.input) and fills it while draining the
// event queue in engine_frame_begin. "down" persists while a key or button is
// held; "pressed"/"released" mark this frame's edges and are cleared at the top
// of the next frame. Read fields directly: in->key_pressed[RGFW_key1].

#ifndef INPUT_H
#define INPUT_H

#include "../rgfw.h"
#include "../base/base.h"
#include "../math/math.h"

typedef enum {
    MOUSE_LEFT = 0,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_BUTTON_COUNT,
} Mouse_Button;

typedef struct {
    // Keyboard, indexed by RGFW_key. key_pressed excludes OS key repeats.
    u8 key_down[RGFW_keyLast];
    u8 key_pressed[RGFW_keyLast];

    // Mouse, in render coordinates (window points). wheel is this frame's scroll,
    // positive away from the user.
    Vec2 mouse;
    Vec2 mouse_delta;
    f32  wheel;
    u8   mouse_down[MOUSE_BUTTON_COUNT];
    u8   mouse_pressed[MOUSE_BUTTON_COUNT];
    u8   mouse_released[MOUSE_BUTTON_COUNT];

    u8 mouse_seen;  // internal: first motion event seeds mouse without a delta spike
} Input;

// Clear the per-frame edges (pressed/released/delta/wheel). Held state persists.
void input_frame_begin(Input *in);

// Fold one RGFW event into the snapshot. Events the snapshot doesn't model are ignored.
void input_handle_event(Input *in, const RGFW_event *ev);

#endif
