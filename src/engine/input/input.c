// Input snapshot implementation (included into mach.c).

#include "input.h"

// Map an SDL button index to Mouse_Button, or -1 for buttons we don't model.
static i32 mouse_button_index(u8 sdl_button) {
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:   return MOUSE_LEFT;
    case SDL_BUTTON_RIGHT:  return MOUSE_RIGHT;
    case SDL_BUTTON_MIDDLE: return MOUSE_MIDDLE;
    default:                return -1;
    }
}

void input_frame_begin(Input *in) {
    memset(in->key_pressed, 0, sizeof(in->key_pressed));
    memset(in->mouse_pressed, 0, sizeof(in->mouse_pressed));
    memset(in->mouse_released, 0, sizeof(in->mouse_released));
    in->mouse_delta = (Vec2){0.0f, 0.0f};
    in->wheel = 0.0f;
}

void input_handle_event(Input *in, const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
        if (ev->key.scancode < SDL_SCANCODE_COUNT) {
            if (!ev->key.repeat) in->key_pressed[ev->key.scancode] = 1;
            in->key_down[ev->key.scancode] = 1;
        }
        break;
    case SDL_EVENT_KEY_UP:
        if (ev->key.scancode < SDL_SCANCODE_COUNT) in->key_down[ev->key.scancode] = 0;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        in->mouse = (Vec2){ev->motion.x, ev->motion.y};
        in->mouse_delta.x += ev->motion.xrel;
        in->mouse_delta.y += ev->motion.yrel;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        i32 b = mouse_button_index(ev->button.button);
        if (b < 0) break;
        in->mouse = (Vec2){ev->button.x, ev->button.y};
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            in->mouse_down[b] = 1;
            in->mouse_pressed[b] = 1;
        } else {
            in->mouse_down[b] = 0;
            in->mouse_released[b] = 1;
        }
    } break;
    case SDL_EVENT_MOUSE_WHEEL: {
        // (npt): Normalize "natural"/flipped scrolling here so the game never
        // has to look at wheel.direction.
        f32 dir = (ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1.0f : 1.0f;
        in->wheel += ev->wheel.y * dir;
    } break;
    default:
        break;
    }
}
