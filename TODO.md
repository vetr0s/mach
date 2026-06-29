# mach engine — roadmap

## NOTES

- RE: gdd, think city skylines mixed with miners haven and a pinch of factorio

## Core engine
- [x] Game loop: update/render separation, variable timestep with soft cap
- [x] Input system: keyboard, mouse
- [ ] Event system: input, collision, game events
- [ ] Memory: custom allocators, arena allocation

## Rendering
- [x] 3D renderer on SDL_GPU (Metal + Vulkan; offline cross-compiled HLSL shaders)
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
- [x] **[v0.4.0] Cross-platform shaders.** Shaders are authored once in HLSL
      (`src/engine/render/shaders/*.hlsl`) using SDL_GPU's register-space binding
      model (vertex uniforms `space1`, fragment uniforms `space3`,
      textures/samplers `space2`), cross-compiled offline by `scripts/shaders.sh`
      to SPIR-V (`glslangValidator -D`) and MSL (`spirv-cross --msl
      --msl-decoration-binding`), and baked into the committed
      `shaders_generated.h`. `gpu.c` requests `SPIRV | MSL`, narrows to the
      backend's format via `SDL_GetGPUShaderFormats`, and loads the matching blob
      (entrypoint `main` for SPIR-V, `main0` for MSL — spirv-cross renames it).
      - The "blocked on tooling" worry was wrong: `glslang` and `spirv-cross` are
        both in Homebrew (`brew install glslang spirv-cross`). Only the
        SDL_shadercross *wrapper* lacks a formula, and we don't need it — plain
        glslang+spirv-cross with explicit register spaces produces SDL-compatible
        bytecode.
      - Verified on macOS/Metal (loads + renders from the baked MSL). The Vulkan
        (SPIR-V) path is generated but **untested** — needs a Linux/Windows box to
        confirm at runtime.
      - DXIL/D3D12 still deferred: needs the heavy DXC toolchain and isn't required
        while Windows runs on Vulkan. Add later if a Vulkan-less Windows target shows up.

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

> **Note: still want a Linux dev box, but it's no longer urgent.** The offline
> shader toolchain *does* work on macOS (`brew install glslang spirv-cross`), so
> the original "painful to even set up" blocker is gone. What macOS still can't do
> is *run* the Vulkan path: Apple has no native Vulkan (only MoltenVK-over-Metal)
> and deprecated OpenGL (frozen at 4.1). So the SPIR-V blobs we bake here are
> untested until they run on a real Vulkan backend. Keep a **Linux** target for
> runtime-testing Vulkan (and eventually Windows); macOS/Metal stays a fine
> primary dev environment for everything else.
