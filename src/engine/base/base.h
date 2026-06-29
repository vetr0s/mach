// Fundamental types, type aliases, and base utilities.

#ifndef BASE_H
#define BASE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Semantic versioning: MAJOR.MINOR.PATCH
#define MACH_VERSION_MAJOR 0
#define MACH_VERSION_MINOR 4
#define MACH_VERSION_PATCH 0

// Sized integer aliases.
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef size_t    usize;
typedef ptrdiff_t isize;

// 32-bit boolean. Used in place of bare `int` for truth values so intent is
// explicit and consistent across the codebase.
typedef i32 b32;
#define MACH_TRUE  1
#define MACH_FALSE 0

// Number of elements in a fixed-size array.
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#endif
