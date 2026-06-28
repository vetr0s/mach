# mach engine — roadmap

## Core engine
- [x] Game loop: update/render separation, variable timestep with soft cap
- [x] Input system: keyboard, mouse
- [ ] Event system: input, collision, game events
- [ ] Memory: custom allocators, arena allocation

## Rendering
- [x] 3D renderer on SDL_GPU (Metal backend; runtime-compiled MSL shaders)
- [x] Lit geometry: directional light + depth buffer
- [x] Procedural meshes (cube, plane)
- [x] Camera system: perspective + orthographic, pan/zoom, mouse-pick rays
- [x] Isometric view as an orthographic camera (not iso-only)
- [x] Bitmap font rendering (8x8 baked into a GPU atlas)
- [x] 2D overlay pass (text + filled rects, alpha-blended)
- [x] Placement validation: red/green hover preview
- [ ] Textured meshes + materials
- [ ] External mesh assets (OBJ/glTF)
- [ ] Sprite batching / instancing
- [ ] Window resize handling (recreate depth target)
- [ ] **[v0.4.0] Cross-platform shaders.** The engine is currently macOS-only at
      runtime: shaders are hand-written MSL and the device is created with
      `SDL_GPU_SHADERFORMAT_MSL`, so it won't start on Windows/Linux. SDL_GPU only
      accepts *precompiled bytecode* for Vulkan/D3D12 (no runtime source compile
      like Metal), so going cross-platform means:
      1. Rewrite the two shaders (3D lit, 2D overlay) once in HLSL, using
         SDL_GPU's register-space binding model (vertex uniforms `space1`,
         fragment uniforms `space3`, textures/samplers `space0`/`space2`).
      2. Add `scripts/shaders.sh` to compile HLSL offline → SPIR-V (Vulkan, via
         glslang) → MSL (Metal, via spirv-cross). This covers macOS/Linux/Windows
         (Windows runs Vulkan). Bake outputs into a generated C header so the
         single-binary unity build is preserved (no runtime asset files).
      3. `gpu.c`: create the device with all formats and load the blob matching
         `SDL_GetGPUShaderFormats`; drop the inline MSL strings.
      - DXIL/D3D12 deferred: it needs the heavy DXC toolchain and isn't required
        while Windows has Vulkan. Add later if a Vulkan-less Windows target shows up.
      - Blocked today on tooling: no shadercross/DXC/glslang installed and no
        Homebrew formula for shadercross — see the dev-environment note below.

## Math
- [x] Vector2/3/4 types and operations
- [x] Matrix4x4: transforms, perspective/ortho, look_at, inverse
- [ ] Quaternions
- [x] Common math utilities (lerp, clamp, radians, etc.)

## Gameplay
- [x] Entity system (fat structs + direct arrays)
- [ ] Transform component
- [ ] Collision detection (AABB, circle)
- [ ] Physics: velocity, gravity, basic rigidbody
- [ ] Animation: sprite animation, tweening

## Content
- [ ] Asset loading pipeline
- [ ] Level/tilemap format and loader
- [ ] Data serialization

## Debugging
- [x] Leveled logging (INFO / ERROR / DEBUG)
- [ ] Debug draw: collision bounds, vectors
- [ ] Performance profiler
- [ ] Immediate-mode debug UI

## Audio (later)
- [ ] Audio system
- [ ] Sound and music loading
- [ ] Spatial audio

## Scripting (future)
- [ ] Embedded Lua integration
- [ ] Script hot-reload

## Tools (future)
- [ ] Level editor
- [ ] Sprite editor
- [ ] Scene editor

## Platform
- [x] macOS: building and running
- [ ] Linux: build and test
- [ ] Windows: build and test

> **Note: move the primary dev environment off macOS.** macOS is a poor fit for
> this kind of low-level graphics work — Apple deprecated OpenGL (frozen at 4.1),
> has no native Vulkan, and the cross-platform shader toolchain (shadercross, DXC)
> isn't readily available (no Homebrew formula), making the offline shader build
> painful to even set up here. Metal-only is the path of least resistance on Mac,
> which is exactly the lock-in we want to avoid for a cross-platform engine.
> Plan to develop primarily on **Linux** (native Vulkan, full toolchain via the
> package manager) and keep macOS as a secondary/test target.
