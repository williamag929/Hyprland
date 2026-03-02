# Spatial Changes [SPATIAL] — Hyprland v0.45.x Fork

> Change log for Spatial OS integration  
> Last updated: February 26, 2026
>
> ⚠️ **Note:** This document is kept as an intermediate historical snapshot.
> The current source of truth is `docs/SPATIAL_STATUS.md` and
> `docs/SPATIAL_CHANGES_FINAL.md`.

---

## ✅ Completed Changes

### Phase 1: Z Data Structure

**[SPATIAL] src/Compositor.hpp**
- Added `#include "spatial/ZSpaceManager.hpp"` near the top (post Color Management)
- Added `inline UP<Spatial::ZSpaceManager> g_pZSpaceManager;` at the end

**[SPATIAL] src/Compositor.cpp**
- Added initialization of `g_pZSpaceManager` in `initManagers(STAGE_PRIORITY)`
- Log: "Creating the ZSpaceManager!"

**[SPATIAL] src/desktop/view/Window.hpp**
- Added `SSpatialProps` struct with fields:
  - `float fZPosition` (current Z position)
  - `float fZTarget` (target Z position)
  - `float fZVelocity` (animation velocity)
  - `int iZLayer` (discrete layer 0-N)
  - `float fDepthNorm` (normalized 0-1 for shaders)
  - `bool bZPinned` (not affected by camera)
  - `bool bZManaged` (app controls its own Z)
- Declared as `m_sSpatialProps` in `CWindow`

**[SPATIAL] src/managers/input/InputManager.cpp**
- Scroll wheel interception in `onMouseWheel()`
- Positive vertical scroll → `nextLayer()`, negative → `prevLayer()`
- Consumes the event (returns without further processing)

---

## 🔄 Changes In Progress

### Phase 2: Perspective Rendering

**PENDING: src/render/Renderer.cpp**
- [ ] Implement `getSpatialProjection()` with glm::perspective
- [ ] Implement `getSpatialView()` with glm::lookAt
- [ ] Integrate depth sorting in renderAllWindows()
- [ ] Pass uniforms u_zDepth and u_blurRadius to shaders

### Phase 3: Shader Integration

**PENDING: src/render/Shader.cpp**
- [ ] Load depth_spatial.frag, depth_dof.frag, passthrough_ar.frag
- [ ] Compile with glslangValidator validation

**PENDING: Renderer.cpp**
- [ ] Bind depth shaders into the pipeline

---

## 📋 Pending Changes

### Phase 4: Initialization in renderMonitor()

**PENDING: src/render/Renderer.cpp::renderMonitor()**
- [ ] First call: `g_pZSpaceManager->init(w, h)`
- [ ] Per-frame update: `g_pZSpaceManager->update(deltaTime)`

### Phase 5: Assigning Windows to Layers

**PENDING: src/desktop/view/Window.cpp or Compositor.cpp**
- [ ] Call `g_pZSpaceManager->assignWindowToLayer(window, layer)` in mapWindow()
- [ ] Synchronize Z with existing animations

### Phase 6: Configuration Validation

**PENDING: src/spatial/SpatialConfig.cpp integration**
- [ ] Load configuration from hyprlang
- [ ] Apply animation parameters

---

## 🔧 New Files Created

```
✅ src/spatial/ZSpaceManager.hpp/cpp     — Z layer manager
✅ src/spatial/SpatialConfig.hpp/cpp     — Configuration parser
✅ src/spatial/SpatialInputHandler.hpp/cpp — Input handling
✅ src/render/shaders/depth_spatial.frag — Main shader
✅ src/render/shaders/depth_dof.frag     — Depth of field
✅ src/render/shaders/passthrough_ar.frag — AR passthrough (placeholder)
✅ .github/workflows/spatial-build.yml   — CI/CD pipeline
```

---

## 📝 Next Steps

1. **Implement 3D rendering** — perspective matrices in Renderer.cpp
2. **Integrate shaders** — load and compile depth shaders
3. **Synchronize animations** — use spring physics in update()
4. **Configuration** — parser for the $spatial section in hyprland.conf
5. **Testing** — unit test suite with Google Test

---

## 🚧 Design Notes

- **Thread-safety:** ZSpaceManager uses an internal mutex for access from InputManager
- **Upstream compatibility:** All Hyprland changes are prefixed with [SPATIAL]
- **Minimal invasiveness:** SSpatialProps is an isolated struct and does not modify existing Window logic
- **Scroll interception:** Only captures unmodified scroll (normal scroll with Shift/Ctrl still works)

---

## ⚠️ Risks and Mitigation

| Risk | Mitigation |
|--------|-----------|
| Upstream conflicts | Use [SPATIAL] prefix, keep core file changes minimal |
| Performance degradation | Optimized spring physics, input throttling |
| Memory leaks | valgrind clean in CI/CD, RAII patterns |
| Z-ordering issues | Depth sorting in Renderer, SSpatialProps linked to Window |

---

*Document: SPATIAL_CHANGES.md*  
*Fork: spatial-hypr (Hyprland v0.45.x)*  
*Status: In development — Phase 1-2*
