#include "effects.h"

void effects_clear(Effects *e) {
    e->count = 0;
}

Effect *effects_add(Effects *e) {
    if (e->count >= MAX_EFFECTS)
        return NULL;
    Effect *fx = &e->items[e->count++];
    *fx = (Effect){0};
    return fx;
}

void effects_update(Effects *e, f32 dt) {
    i32 w = 0;
    for (i32 i = 0; i < e->count; i++) {
        Effect *fx = &e->items[i];
        fx->age += dt;
        if (fx->age < fx->lifetime)
            e->items[w++] = *fx; // keep it; expired effects fall off the front-compacted list
    }
    e->count = w;
}
