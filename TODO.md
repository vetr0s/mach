# mach engine — roadmap

## Core engine
- [x] Game loop: update/render separation, variable timestep with soft cap
- [x] Input system: keyboard, mouse
- [ ] Event system: input, collision, game events
- [ ] Memory: custom allocators, arena allocation

## Rendering
- [x] 3D renderer on SDL_GPU (Metal/Vulkan/D3D12), runtime MSL shaders
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
- [ ] SPIR-V/DXIL shaders for non-Metal backends

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
