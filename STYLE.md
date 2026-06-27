# Code Style Guide

## Comments

Assume readers understand C syntax. Comments are for *why*, not *what*.

### Required Comments

1. **File header** — purpose of the file
2. **Function header** — what the function does, parameters, return value (if non-obvious)

### Optional Comments

- Hard-to-understand code
- Non-obvious algorithm or logic
- Justification for a specific approach
- Workarounds, platform-specific quirks
- Performance rationale

### Format

All comments are prepended with initials `(npt):` for clarity of authorship during team growth.

```c
// (npt): File header explaining module purpose
// License/copyright here if needed

// (npt): Function does X and returns Y
void function_name(void) {
    // (npt): Non-obvious why we do this specific thing
    some_code();
}
```

### Anti-patterns

Don't write:
```c
x = y + 1;  // Increment x
```

Don't write:
```c
// Loop through items
for (i32 i = 0; i < count; i++) {
    // ...
}
```

Do write:
```c
// (npt): We skip the first item because it's a sentinel
for (i32 i = 1; i < count; i++) {
    // ...
}
```

## Naming

- Types: `Snake_Case` (e.g., `Entity_Miner`)
- Functions: `snake_case` (e.g., `world_spawn_miner`)
- Constants: `SCREAMING_SNAKE_CASE` (e.g., `MAX_ENTITIES`)
- Variables: `snake_case`
- Struct fields: `snake_case`

## Formatting

- Indentation: 4 spaces
- Line length: aim for 100, hard limit 120
- Braces: opening brace on same line
- No trailing whitespace

## File Organization

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

Implementation files follow the same structure without the header guards.
