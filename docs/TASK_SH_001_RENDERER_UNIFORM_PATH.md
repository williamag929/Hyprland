# @architect TASK-SH-001 — Renderer Uniform Upload Path Specification

**Module**: spatial-hypr compositor  
**Priority**: P0 (required before `@refactor TASK-SH-104` is considered complete)  
**Author**: @architect  
**Date**: February 2026  
**Status**: APPROVED SPEC — ready for @refactor implementation

---

## Executive Summary

`applySpatialShaderUniforms()` and `selectSpatialShader()` exist in `Renderer.cpp` with correct logic, but they are **never called** and contain one critical implementation error. This spec defines the exact integration path to close TASK-SH-104.

---

## 1. Current State Audit

### What exists and is correct

| File | Location | Status |
|------|----------|--------|
| `src/render/Shader.hpp` | `eShaderUniform` — `SHADER_SPATIAL_PROJ`, `SHADER_SPATIAL_VIEW`, `SHADER_Z_DEPTH`, `SHADER_BLUR_RADIUS` | ✅ correct |
| `src/render/Shader.hpp` | `CShader::setUniformMatrix4fv(eShaderUniform, count, transpose, const glm::mat4&)` | ✅ correct |
| `src/render/OpenGL.hpp` | `SCurrentRenderData::spatialProjection`, `::spatialView` as `glm::mat4` | ✅ correct |
| `src/render/OpenGL.hpp` | `SH_FRAG_SPATIAL_DEPTH`, `SH_FRAG_SPATIAL_DOF` enum values | ✅ correct |
| `src/render/OpenGL.cpp` | Shader loader entries `"depth_spatial.frag"`, `"depth_dof.frag"` | ✅ correct |
| `src/render/Renderer.cpp:1384-1385` | Matrix upload from `ZSpaceManager` into `m_renderData` each frame | ✅ correct |
| `src/render/Renderer.cpp:2784` | `selectSpatialShader()` — layer 3 → DoF; layers 0-2 → depth | ✅ correct |
| `src/render/Renderer.cpp:2801` | `applySpatialShaderUniforms()` — uploads 4 uniforms | ✅ correct logic, wrong call site |

### Critical gaps

#### Gap 1 — `useShader()` not called before uniform upload

`applySpatialShaderUniforms()` writes uniforms to whichever GL program is currently bound,
not to the spatial shader. The spatial program must be bound with `useShader()` first.

```cpp
// CURRENT (broken) — Renderer.cpp:2801
void CHyprRenderer::applySpatialShaderUniforms(PHLWINDOW pWindow) {
    ...
    auto shader = pShader.lock();
    // ❌ Missing: g_pHyprOpenGL->useShader(pShader);
    shader->setUniformMatrix4fv(SHADER_SPATIAL_PROJ, ...);   // writes to wrong program
    ...
}
```

#### Gap 2 — `applySpatialShaderUniforms()` is never invoked

Neither `renderWindow()` nor `CSurfacePassElement::render()` calls it. The spatial path is dead code.

#### Gap 3 — Uniform names may not be registered at program link time

`CShader::createProgram()` calls `glGetUniformLocation()` for every `eShaderUniform` slot.
The spatial shaders must declare GLSL uniforms whose names match the lookup table entries for
the four new enum slots. See §2 for the required mapping.

---

## 2. Uniform Name → Enum Mapping

The spatial GLSL shaders must declare these uniform names exactly:

| `eShaderUniform` | GLSL name | GLSL type |
|-----------------|-----------|-----------|
| `SHADER_SPATIAL_PROJ` | `u_spatialProj` | `mat4` |
| `SHADER_SPATIAL_VIEW` | `u_spatialView` | `mat4` |
| `SHADER_Z_DEPTH` | `u_zDepth` | `float` |
| `SHADER_BLUR_RADIUS` | `u_blurRadius` | `float` |

**Action for @refactor**: Verify that `Shader.cpp` contains these name→enum entries in its
lookup table and add any that are missing before shipping.

---

## 3. Vertex Shader Decision

### Decision: Reuse `texvertnorast.vert` for P0

The depth effect is fragment-only (blur + alpha fade by `u_zDepth`). Hyprland's pass element
system already delivers correct screen-space 2D vertex positions. True 3D vertex displacement
is deferred to TASK-SH-201.

`u_spatialProj` and `u_spatialView` are still uploaded — they become available for future 3D
geometry without a breaking uniform change.

---

## 4. Integration Point in the Render Pipeline

### Render pipeline summary

```
renderWorkspaceWindows()
    └── renderWindow(pWindow, pMonitor, time, ...)    ← Renderer.cpp ~line 520
            ├── fills CSurfacePassElement::SRenderData
            ├── renderdata.pWindow = pWindow
            └── m_renderPass.add(CSurfacePassElement(renderdata))
                        ↓ (end of frame)
                m_renderPass.render()
                    └── CSurfacePassElement::render()
                            └── g_pHyprOpenGL->renderTexture(...)
                                        ↑
                                CORRECT insertion point (§6)
```

### Tagging in `renderWindow()`

Inside `renderWindow()`, after `renderdata.pWindow = pWindow` (currently ~line 567),
before the `m_renderPass.add()` loop (~line 654):

```cpp
// [SPATIAL] Tag renderdata when window is Z-managed
if (pWindow->m_sSpatialProps.bZManaged && g_pZSpaceManager) {
    renderdata.spatialShader      = selectSpatialShader(pWindow);
    renderdata.spatialZDepth      = std::clamp(
        (pWindow->m_sSpatialProps.fZPosition - (-2800.0f)) / 2800.0f, 0.0f, 1.0f);
    renderdata.spatialBlurRadius  = g_pZSpaceManager->getWindowBlurRadius(pWindow.get());
}
```

---

## 5. Required Additions to `CSurfacePassElement::SRenderData`

**File**: `src/render/pass/SurfacePassElement.hpp` — append to `SRenderData` struct:

```cpp
// [SPATIAL] — zero/null by default; non-null spatialShader activates the spatial path
WP<CShader>  spatialShader;          // shader override — null = use surface default
float        spatialZDepth      = 0.0f;
float        spatialBlurRadius  = 0.0f;
```

These fields are transparent to all non-spatial render paths.

---

## 6. Uniform Upload in `CHyprOpenGLImpl::renderTexture()`

**File**: `src/render/OpenGL.cpp` — `renderTexture()` function

Replace the shader selection block with a spatial-aware version:

```cpp
// [SPATIAL] Shader selection — spatial override takes priority
const bool         hasSpatial  = renderData.spatialShader.lock() != nullptr;
const WP<CShader>  shaderToUse = hasSpatial
                                    ? renderData.spatialShader
                                    : getSurfaceShader(surfaceFeatures);

// BINDING FIRST — uniforms must be set after glUseProgram
auto boundShader = useShader(shaderToUse);

if (auto locked = boundShader.lock()) {
    if (hasSpatial) {
        // Perspective matrices (available for future 3D vertex stage)
        locked->setUniformMatrix4fv(SHADER_SPATIAL_PROJ, 1, GL_FALSE,
                                    m_renderData.spatialProjection);
        locked->setUniformMatrix4fv(SHADER_SPATIAL_VIEW, 1, GL_FALSE,
                                    m_renderData.spatialView);
        // Per-window depth scalars (consumed by fragment shader)
        locked->setUniformFloat(SHADER_Z_DEPTH,        renderData.spatialZDepth);
        locked->setUniformFloat(SHADER_BLUR_RADIUS,    renderData.spatialBlurRadius);
    }
    // ... existing common uniforms (SHADER_PROJ, SHADER_ALPHA, SHADER_TEX, etc.) follow
}
```

`applySpatialShaderUniforms()` in `Renderer.cpp` must be **removed** — it is replaced by the
above inline block and its `useShader` omission would remain a silent bug.

---

## 7. Files Changed by This Spec

| File | Change | Upstream risk |
|------|--------|---------------|
| `src/render/pass/SurfacePassElement.hpp` | +3 fields to `SRenderData` | Low — append-only |
| `src/render/OpenGL.cpp` | Spatial shader branch in `renderTexture()` | Medium — central render hot path |
| `src/render/Renderer.cpp` | Tag `renderdata` in `renderWindow()`; delete `applySpatialShaderUniforms()` | Low |
| `src/render/Shader.cpp` | Verify / add name lookup entries for `SHADER_SPATIAL_*` | Low |
| `src/render/shaders/depth_spatial.frag` | Confirm GLSL uniform names match §2 table | None — new file |
| `src/render/shaders/depth_dof.frag` | Confirm GLSL uniform names match §2 table | None — new file |

---

## 8. Acceptance Criteria for TASK-SH-104

- [ ] `useShader()` is called **before** any `setUniform*` on the spatial shader in the same render call
- [ ] `renderTexture()` in `OpenGL.cpp` activates the spatial path when `renderData.spatialShader` is set
- [ ] All 4 spatial uniforms are uploaded each frame for every Z-managed window
- [ ] `applySpatialShaderUniforms()` in `Renderer.cpp` is deleted
- [ ] Layer 3 windows use `SH_FRAG_SPATIAL_DOF`; layers 0-2 use `SH_FRAG_SPATIAL_DEPTH`
- [ ] Windows with `bZManaged = false` use the default Hyprland surface shader — zero regression
- [ ] `glslangValidator -V` passes on both spatial shaders with uniform names matching §2
- [ ] `ctest --test-dir build -R "spatial" --output-on-failure` — all tests pass
- [ ] No frame-time regression on RX 580 / GTX 1060 class GPU (60 fps target maintained)

---

## 9. Deferred Items (out of scope for TASK-SH-104)

| Item | Future Task |
|------|-------------|
| True 3D vertex positioning via `spatial_depth.vert` | TASK-SH-201 |
| `passthrough_ar.frag` integration with OpenXR framebuffer | TASK-SH-301 |
| Per-frame projection matrix lerp (smooth camera) | TASK-SH-202 |
| `CSpatialPassElement` dedicated render pass element | TASK-SH-201 |

---

*@architect TASK-SH-001 — Spatial OS*  
*Specification version 1.0 — February 2026*
