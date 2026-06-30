# Style guide

Short version: write code that reads like the rest of the codebase. The longer
version is below, but none of it is surprising.

## Comments

Assume whoever's reading knows C. So comments are for the *why*, never the *what*.
If a line just explains what the syntax already says, delete it.

### What you have to comment

1. **File headers** — what the file is for.
2. **Function headers** — what the function does, its parameters, and what it
   returns when that isn't obvious from the name.

### What's worth commenting

- Code that's genuinely hard to follow
- A non-obvious algorithm or bit of logic
- Why you picked this approach over the other one
- Workarounds and platform quirks
- Anything you did for performance that looks weird without the reason

### Format

**File and function headers** are factual. No prefix, no personality, just say what
the thing is.

**Justification and decision comments** get a `(npt):` prefix when they're written in
your own voice: your reasoning, a design call, something non-obvious you want the
next person to understand.

```c
// Module description and purpose

// What the function does and returns
void function_name(void) {
    // (npt): Why we specifically do this, not something else
    some_code();
}
```

### Don't do this

Don't narrate the obvious:
```c
x = y + 1;  // Increment x
```

Don't label a loop with the fact that it's a loop:
```c
// Loop through items
for (i32 i = 0; i < count; i++) {
    // ...
}
```

Do explain the part that would trip someone up:
```c
// (npt): We skip the first item because it's a sentinel
for (i32 i = 1; i < count; i++) {
    // ...
}
```

## Naming

- Types: `Snake_Case` (e.g. `Entity_Miner`)
- Functions: `snake_case` (e.g. `world_spawn_miner`)
- Constants: `SCREAMING_SNAKE_CASE` (e.g. `MAX_ENTITIES`)
- Variables: `snake_case`
- Struct fields: `snake_case`

## Formatting

- 4-space indent
- Aim for 100 columns, hard stop at 120
- Opening brace on the same line
- No trailing whitespace

## How a file is laid out

```c
// (npt): Module description and purpose

#ifndef MODULE_H
#define MODULE_H

// Includes
#include "..."

// Type definitions
typedef struct { ... } Type_Name;

// Function declarations
void function_name(void);

#endif
```

Implementation files are the same minus the header guards.
