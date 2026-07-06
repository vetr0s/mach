// The engine's single include point for RGFW declarations. RGFW_OPENGL must be
// defined before RGFW.h or the GL context API is compiled out, so nothing
// includes third_party/rgfw/RGFW.h directly except this file and the
// implementation include in core/core.c.

#ifndef ENGINE_RGFW_H
#define ENGINE_RGFW_H

// base.h already defines the u8/i64-style aliases RGFW would otherwise typedef.
#include "base/base.h"
#define RGFW_INT_DEFINED

#define RGFW_OPENGL
#include "../../third_party/rgfw/RGFW.h"

#endif
