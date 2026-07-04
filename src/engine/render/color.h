// Color type and the stock palette.
//
// Color is Vec4 RGBA in [0,1] — the exact type every r2d call takes — under the
// name that says what it is. The palette is modus-vivendi (Protesilaos Stavrou's
// Emacs theme, https://protesilaos.com/emacs/modus-themes): WCAG-AAA-contrast
// colors designed for a black background, which is exactly what a game HUD wants.
// The names mirror the theme's own (bg-dim, red-warmer, ...), so its docs apply.
//
// A game can lean on these everywhere, or define its own colors with COLOR_HEX
// and ignore the palette entirely. The engine itself has no opinion.

#ifndef COLOR_H
#define COLOR_H

#include "../base/base.h"
#include "../math/math.h"

typedef Vec4 Color;

// A Color from 0xRRGGBB, alpha 1. Every palette entry below is one of these, so
// the hex value stays visible and greppable.
#define COLOR_HEX(h) ((Color){ \
    (f32)(((h) >> 16) & 0xFF) / 255.0f, \
    (f32)(((h) >>  8) & 0xFF) / 255.0f, \
    (f32)(((h)      ) & 0xFF) / 255.0f, \
    1.0f })

// --- Basic values -------------------------------------------------------------

#define COLOR_BG_MAIN      COLOR_HEX(0x000000)
#define COLOR_BG_DIM       COLOR_HEX(0x1e1e1e)
#define COLOR_FG_MAIN      COLOR_HEX(0xffffff)
#define COLOR_FG_DIM       COLOR_HEX(0x989898)
#define COLOR_FG_ALT       COLOR_HEX(0xc6daff)
#define COLOR_BG_ACTIVE    COLOR_HEX(0x535353)
#define COLOR_BG_INACTIVE  COLOR_HEX(0x303030)
#define COLOR_BORDER       COLOR_HEX(0x646464)

#define COLOR_BLACK        COLOR_BG_MAIN
#define COLOR_WHITE        COLOR_FG_MAIN

// --- Common accents -----------------------------------------------------------

#define COLOR_RED              COLOR_HEX(0xff5f59)
#define COLOR_RED_WARMER       COLOR_HEX(0xff6b55)
#define COLOR_RED_COOLER       COLOR_HEX(0xff7f86)
#define COLOR_RED_FAINT        COLOR_HEX(0xff9580)
#define COLOR_RED_INTENSE      COLOR_HEX(0xff5f5f)

#define COLOR_GREEN            COLOR_HEX(0x44bc44)
#define COLOR_GREEN_WARMER     COLOR_HEX(0x70b900)
#define COLOR_GREEN_COOLER     COLOR_HEX(0x00c06f)
#define COLOR_GREEN_FAINT      COLOR_HEX(0x88ca9f)
#define COLOR_GREEN_INTENSE    COLOR_HEX(0x44df44)

#define COLOR_YELLOW           COLOR_HEX(0xd0bc00)
#define COLOR_YELLOW_WARMER    COLOR_HEX(0xfec43f)
#define COLOR_YELLOW_COOLER    COLOR_HEX(0xdfaf7a)
#define COLOR_YELLOW_FAINT     COLOR_HEX(0xd2b580)
#define COLOR_YELLOW_INTENSE   COLOR_HEX(0xefef00)

#define COLOR_BLUE             COLOR_HEX(0x2fafff)
#define COLOR_BLUE_WARMER      COLOR_HEX(0x79a8ff)
#define COLOR_BLUE_COOLER      COLOR_HEX(0x00bcff)
#define COLOR_BLUE_FAINT       COLOR_HEX(0x82b0ec)
#define COLOR_BLUE_INTENSE     COLOR_HEX(0x338fff)

#define COLOR_MAGENTA          COLOR_HEX(0xfeacd0)
#define COLOR_MAGENTA_WARMER   COLOR_HEX(0xf78fe7)
#define COLOR_MAGENTA_COOLER   COLOR_HEX(0xb6a0ff)
#define COLOR_MAGENTA_FAINT    COLOR_HEX(0xcaa6df)
#define COLOR_MAGENTA_INTENSE  COLOR_HEX(0xff66ff)

#define COLOR_CYAN             COLOR_HEX(0x00d3d0)
#define COLOR_CYAN_WARMER      COLOR_HEX(0x4ae2f0)
#define COLOR_CYAN_COOLER      COLOR_HEX(0x6ae4b9)
#define COLOR_CYAN_FAINT       COLOR_HEX(0x9ac8e0)
#define COLOR_CYAN_INTENSE     COLOR_HEX(0x00eff0)

// --- Uncommon accents ---------------------------------------------------------

#define COLOR_RUST    COLOR_HEX(0xdb7b5f)
#define COLOR_GOLD    COLOR_HEX(0xc0965b)
#define COLOR_OLIVE   COLOR_HEX(0x9cbd6f)
#define COLOR_SLATE   COLOR_HEX(0x76afbf)
#define COLOR_INDIGO  COLOR_HEX(0x9099d9)
#define COLOR_MAROON  COLOR_HEX(0xcf7fa7)
#define COLOR_PINK    COLOR_HEX(0xd09dc0)

// --- Accent backgrounds (intense > subtle > nuanced) --------------------------

#define COLOR_BG_RED_INTENSE      COLOR_HEX(0x9d1f1f)
#define COLOR_BG_GREEN_INTENSE    COLOR_HEX(0x2f822f)
#define COLOR_BG_YELLOW_INTENSE   COLOR_HEX(0x7a6100)
#define COLOR_BG_BLUE_INTENSE     COLOR_HEX(0x1640b0)
#define COLOR_BG_MAGENTA_INTENSE  COLOR_HEX(0x7030af)
#define COLOR_BG_CYAN_INTENSE     COLOR_HEX(0x2266ae)

#define COLOR_BG_RED_SUBTLE       COLOR_HEX(0x620f2a)
#define COLOR_BG_GREEN_SUBTLE     COLOR_HEX(0x00422a)
#define COLOR_BG_YELLOW_SUBTLE    COLOR_HEX(0x4a4000)
#define COLOR_BG_BLUE_SUBTLE      COLOR_HEX(0x242679)
#define COLOR_BG_MAGENTA_SUBTLE   COLOR_HEX(0x552f5f)
#define COLOR_BG_CYAN_SUBTLE      COLOR_HEX(0x004065)

#define COLOR_BG_RED_NUANCED      COLOR_HEX(0x3a0c14)
#define COLOR_BG_GREEN_NUANCED    COLOR_HEX(0x092f1f)
#define COLOR_BG_YELLOW_NUANCED   COLOR_HEX(0x381d0f)
#define COLOR_BG_BLUE_NUANCED     COLOR_HEX(0x12154a)
#define COLOR_BG_MAGENTA_NUANCED  COLOR_HEX(0x2f0c3f)
#define COLOR_BG_CYAN_NUANCED     COLOR_HEX(0x042837)

// --- Special purpose ----------------------------------------------------------

#define COLOR_BG_POPUP    COLOR_HEX(0x0c0c0c)
#define COLOR_BG_HOVER    COLOR_HEX(0x45605e)
#define COLOR_BG_HL_LINE  COLOR_HEX(0x2f3849)
#define COLOR_BG_REGION   COLOR_HEX(0x5a5a5a)

// --- Helpers ------------------------------------------------------------------

// Same color, different alpha.
static inline Color color_alpha(Color c, f32 a) { c.w = a; return c; }

// Multiply RGB by f (keeps alpha): cheap directional shading.
static inline Color color_shade(Color c, f32 f) {
    return (Color){c.x * f, c.y * f, c.z * f, c.w};
}

// Move RGB toward white by t in [0,1] (keeps alpha).
static inline Color color_lighten(Color c, f32 t) {
    return (Color){c.x + (1.0f - c.x) * t, c.y + (1.0f - c.y) * t,
                   c.z + (1.0f - c.z) * t, c.w};
}

// Componentwise blend from a to b, alpha included.
static inline Color color_lerp(Color a, Color b, f32 t) {
    return (Color){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                   a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

#endif
