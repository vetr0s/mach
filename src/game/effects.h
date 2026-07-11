// Transient visual effects: the render-side half of the animation skeleton.
//
// The sim never animates. It records what happened as World_Events (world.h): an
// ore banked, an ore tipped off a dead end. Each frame the game drains that queue
// and turns each event into an Effect here. Effects advance on real time, not sim
// ticks, so they are smooth and decoupled from the fixed 3/sec heartbeat, and they
// outlive the sim item they came from (the item is already gone by the time its
// payout finishes rising). Everything transient and purely visual lives here, so it
// never leaks back into world.c.

#ifndef EFFECTS_H
#define EFFECTS_H

#include "mach.h"

#define MAX_EFFECTS 256

typedef enum {
    EFFECT_NONE = 0,
    EFFECT_BANK, // ore banked at a furnace: slides into the sink, a "+value" rises
    EFFECT_FALL, // ore tipped off a dead end: sinks through the belt and fades out
} Effect_Type;

typedef struct {
    Effect_Type type;
    f32 from_x, from_y; // grid-space start: the belt cell the ore left
    f32 to_x, to_y;     // grid-space end: the furnace cell, or the dead-end cell
    i64 value;
    f32 age;      // real seconds since it started
    f32 lifetime; // real seconds it runs before it retires
} Effect;

typedef struct {
    Effect items[MAX_EFFECTS];
    i32 count;
} Effects;

void effects_clear(Effects *e);

// Claim a slot, or NULL if the pool is full. A dropped effect is a missed spark,
// never a correctness problem, so callers may ignore NULL.
Effect *effects_add(Effects *e);

// Advance every effect by real dt and retire the expired, compacting the live ones
// toward the front. dt is real seconds; effects do not care about the sim rate.
void effects_update(Effects *e, f32 dt);

#endif // EFFECTS_H
