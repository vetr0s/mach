# mach engine — roadmap

## Core engine
- [x] Game loop: update/render separation, variable timestep with soft cap
- [x] Input system: keyboard, mouse
- [ ] Event system: input, collision, game events
- [ ] Memory: custom allocators, arena allocation

## Rendering
- [ ] Sprite system: loading, drawing, transforms
- [ ] Texture loading: PNG/BMP support
- [x] Drawing primitives: rectangles, circles, lines
- [x] Camera system: viewport, scrolling, panning, zooming
- [ ] Sprite batching
- [x] Bitmap font rendering (hardcoded 8x8, immediate-mode)
- [x] Grid visualization
- [x] Placement validation: red/green preview

## Math
- [x] Vector2/3/4 types and operations
- [ ] Matrix4x4 for transforms
- [ ] Quaternions
- [x] Common math utilities (lerp, clamp, etc.)

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
