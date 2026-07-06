// Input snapshot implementation (included into mach.c).

#include "input.h"

// Map an RGFW button to Mouse_Button, or -1 for buttons we don't model.
static i32 mouse_button_index(u8 rgfw_button) {
    switch (rgfw_button) {
    case RGFW_mouseLeft:   return MOUSE_LEFT;
    case RGFW_mouseRight:  return MOUSE_RIGHT;
    case RGFW_mouseMiddle: return MOUSE_MIDDLE;
    default:               return -1;
    }
}

void input_frame_begin(Input *in) {
    memset(in->key_pressed, 0, sizeof(in->key_pressed));
    memset(in->mouse_pressed, 0, sizeof(in->mouse_pressed));
    memset(in->mouse_released, 0, sizeof(in->mouse_released));
    in->mouse_delta = (Vec2){0.0f, 0.0f};
    in->wheel = 0.0f;
}

void input_handle_event(Input *in, const RGFW_event *ev) {
    switch (ev->type) {
    case RGFW_keyPressed:
        if (!ev->key.repeat) in->key_pressed[ev->key.value] = 1;
        in->key_down[ev->key.value] = 1;
        break;
    case RGFW_keyReleased:
        in->key_down[ev->key.value] = 0;
        break;
    case RGFW_mouseMotion: {
        // RGFW reports absolute positions; the delta is ours to accumulate.
        Vec2 m = {(f32)ev->mouse.x, (f32)ev->mouse.y};
        if (in->mouse_seen) {
            in->mouse_delta.x += m.x - in->mouse.x;
            in->mouse_delta.y += m.y - in->mouse.y;
        }
        in->mouse = m;
        in->mouse_seen = 1;
    } break;
    case RGFW_mouseButtonPressed:
    case RGFW_mouseButtonReleased: {
        i32 b = mouse_button_index(ev->button.value);
        if (b < 0) break;
        if (ev->type == RGFW_mouseButtonPressed) {
            in->mouse_down[b] = 1;
            in->mouse_pressed[b] = 1;
        } else {
            in->mouse_down[b] = 0;
            in->mouse_released[b] = 1;
        }
    } break;
    case RGFW_mouseScroll:
        in->wheel += ev->delta.y;
        break;
    default:
        break;
    }
}
