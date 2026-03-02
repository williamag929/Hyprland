# SPATIAL-HYPR — Hyprland Fork for Spatial OS
> Specification of modifications to the Hyprland compositor  
> to implement spatial navigation on the Z axis  
> Version: 0.1.0 | Status: DRAFT  
> Agents: `@architect` `@refactor`  
> Base: Hyprland v0.45.x  
> Connected with: `SPATIAL_OS_SPEC.md`, `NO_FORMS_SPEC.md`

---

## Table of Contents

1. [Fork Strategy](#1-fork-strategy)
2. [File Modification Map](#2-file-modification-map)
3. [Phase 1 — Z Data Structure](#3-phase-1--z-data-structure)
4. [Phase 2 — Perspective Rendering](#4-phase-2--perspective-rendering)
5. [Phase 3 — Depth Shaders](#5-phase-3--depth-shaders)
6. [Phase 4 — Z Navigation](#6-phase-4--z-navigation)
7. [Phase 5 — Spatial Configuration](#7-phase-5--spatial-configuration)
8. [Phase 6 — ZSpaceManager](#8-phase-6--zspacemanager)
9. [Upstream Synchronization](#9-upstream-synchronization)
10. [Testing and Validation](#10-testing-and-validation)
11. [Agent Tasks](#11-agent-tasks)
12. [Interface Contracts](#12-interface-contracts)

---

## 1. Fork Strategy

### Why a fork and not a plugin

Hyprland has a C++ plugin system (`APICALL`, `HyprlandAPI`). However, the modifications required for the Z axis touch the core renderer and the `CWindow` structure — layers that the plugin API does not expose. A fork is the only option for the level of control required.

### Minimum divergence principle

Every modification follows this rule: **touch the minimum upstream code possible**. New features go in new files whenever feasible. This reduces the cost of synchronizing with upstream Hyprland.

```
Modified files (upstream)  → minimal, surgical changes
New files (spatial)        → all new logic goes here
```

### Repository structure

```
spatial-hypr/
├── src/
│   ├── spatial/                    ← NEW — all spatial code here
│   │   ├── ZSpaceManager.hpp
│   │   ├── ZSpaceManager.cpp
│   │   ├── SpatialConfig.hpp
│   │   ├── SpatialConfig.cpp
│   │   ├── SpatialInputHandler.hpp
│   │   └── SpatialInputHandler.cpp
│   │
│   ├── render/
│   │   ├── Renderer.cpp            ← MODIFIED (3D perspective)
│   │   └── shaders/
│   │       ├── depth_spatial.frag  ← NEW
│   │       ├── depth_dof.frag      ← NEW
│   │       └── passthrough_ar.frag ← NEW (future AR)
│   │
│   ├── Window.hpp                  ← MODIFIED (+Z fields)
│   ├── Window.cpp                  ← MODIFIED (Z init)
│   └── managers/input/
│       └── InputManager.cpp        ← MODIFIED (scroll → Z nav)
│
├── hyprland.conf.example           ← MODIFIED (spatial section)
├── SPATIAL_CHANGES.md              ← NEW — fork changelog
└── UPSTREAM_SYNC.md                ← NEW — rebase guide
```

---

## 2. File Modification Map

Classified by impact and upstream conflict risk:

```
HIGH IMPACT / HIGH RISK (handle with care)
  src/render/Renderer.cpp
    → add perspective matrix
    → pass u_zDepth to shaders
    → sort windows by Z before rendering (depth sorting)

HIGH IMPACT / MEDIUM RISK
  src/Window.hpp
    → add m_fZPosition, m_iZLayer, m_fZVelocity

MEDIUM IMPACT / LOW RISK
  src/managers/input/InputManager.cpp
    → intercept unmodified scroll → Z navigation
    → add spatial_next_layer / spatial_prev_layer keybinds

LOW IMPACT / LOW RISK
  hyprland.conf
    → new spatial{} section with configuration parameters

NEW FILES (zero conflict risk)
  src/spatial/*
  src/render/shaders/depth_spatial.frag
  src/render/shaders/depth_dof.frag
```

---

## 3. Phase 1 — Z Data Structure

### 3.1 Modification of `src/Window.hpp`

Add the Z fields at the end of the `CWindow` properties section, grouped in their own struct to minimize conflicts with upstream:

```cpp
// src/Window.hpp
// Add inside class CWindow, at the end of the public properties:

// ── SPATIAL OS: Z-Space properties ──────────────────────────
struct SSpatialProps {
    float  fZPosition   = 0.0f;   // current Z position (world units)
    float  fZTarget     = 0.0f;   // target Z position (for animation)
    float  fZVelocity   = 0.0f;   // current Z animation velocity
    int    iZLayer      = 0;      // discrete layer (0=front, N=back)
    float  fDepthNorm   = 0.0f;   // 0.0-1.0, normalized for shaders
    bool   bZPinned     = false;  // true = does not move with camera
    bool   bZManaged    = true;   // false = app controls its own Z
} m_sSpatialProps;
// ─────────────────────────────────────────────────────────────
```

### 3.2 Z System Constants

```cpp
// src/spatial/ZSpaceManager.hpp

namespace Spatial {

constexpr int   Z_LAYERS_COUNT  = 4;
constexpr float Z_LAYER_STEP    = 800.0f;   // world units between layers
constexpr float Z_NEAR          = 0.1f;
constexpr float Z_FAR           = 10000.0f;
constexpr float Z_FOV_DEGREES   = 60.0f;
constexpr float Z_ANIM_STIFFNESS = 200.0f;  // spring stiffness
constexpr float Z_ANIM_DAMPING   = 20.0f;   // spring damping

// Z position per layer
constexpr float LAYER_Z_POSITIONS[Z_LAYERS_COUNT] = {
    0.0f,      // Layer 0: Foreground — active app
   -800.0f,    // Layer 1: Near — recent apps
  -1600.0f,    // Layer 2: Mid — background
  -2800.0f,    // Layer 3: Far — system / config
};

// Visibility by layer distance
constexpr float LAYER_OPACITY[Z_LAYERS_COUNT] = {
    1.00f,   // Layer 0: full opacity
    0.75f,   // Layer 1: slight fade
    0.40f,   // Layer 2: clearly in background
    0.15f,   // Layer 3: almost invisible
};

constexpr float LAYER_BLUR_RADIUS[Z_LAYERS_COUNT] = {
    0.0f,    // Layer 0: no blur
    1.5f,    // Layer 1: subtle blur
    5.0f,    // Layer 2: visible blur
   12.0f,    // Layer 3: heavy blur
};

} // namespace Spatial
```

---

## 4. Phase 2 — Perspective Rendering

### 4.1 Modification of `src/render/Renderer.cpp`

This is the most important change. It involves three surgical modifications:

**A) Add perspective projection function**

```cpp
// Add in Renderer.cpp, before renderWindow()

// ── SPATIAL OS: perspective projection ──
glm::mat4 CSpatialRenderer::getSpatialProjection(int w, int h) {
    float aspect = (float)w / (float)h;
    
    // True perspective — distant windows appear smaller
    glm::mat4 proj = glm::perspective(
        glm::radians(Spatial::Z_FOV_DEGREES),
        aspect,
        Spatial::Z_NEAR,
        Spatial::Z_FAR
    );
    
    // Camera — moves in Z according to the active layer
    float camZ = -m_pZSpaceManager->getCameraZ();
    glm::mat4 view = glm::lookAt(
        glm::vec3(w / 2.0f, h / 2.0f, camZ + 1200.0f), // camera position
        glm::vec3(w / 2.0f, h / 2.0f, camZ),            // looks at center
        glm::vec3(0.0f, -1.0f, 0.0f)                    // up (Y inverted on screen)
    );
    
    return proj * view;
}
// ─────────────────────────────────────────────────────────────

// Modify the existing window transform function:
glm::mat4 getWindowTransform(CWindow* pWindow) {
    auto& sp = pWindow->m_sSpatialProps;
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(
        pWindow->m_vRealPosition.x,
        pWindow->m_vRealPosition.y,
        sp.fZPosition  // ← Z axis added
    ));
    
    return getSpatialProjection(m_iScreenW, m_iScreenH) * model;
}
```

**B) Depth sorting before rendering**

Windows must be drawn back-to-front (painter's algorithm) for transparency to work correctly:

```cpp
// In renderAllWindows() — add sort by Z before the loop

// ── SPATIAL OS: depth sort ──
std::vector<CWindow*> sortedWindows = g_pCompositor->m_vWindows;
std::sort(sortedWindows.begin(), sortedWindows.end(),
    [](CWindow* a, CWindow* b) {
        // From furthest (most negative Z) to closest (Z=0)
        return a->m_sSpatialProps.fZPosition < b->m_sSpatialProps.fZPosition;
    }
);
// render sortedWindows instead of m_vWindows
// ───────────────────────────────────────────────
```

**C) Pass u_zDepth to shader per window**

```cpp
// In the shader call for each window:
glUniform1f(glGetUniformLocation(prog, "u_zDepth"),
    pWindow->m_sSpatialProps.fDepthNorm);

glUniform1f(glGetUniformLocation(prog, "u_blurRadius"),
    Spatial::LAYER_BLUR_RADIUS[pWindow->m_sSpatialProps.iZLayer]);
```

---

## 5. Phase 3 — Depth Shaders

### 5.1 `depth_spatial.frag` — main shader

```glsl
// src/render/shaders/depth_spatial.frag
// Depth shader for Spatial OS
// Extends Hyprland's Gaussian blur with Z-depth effects

#version 430 core

uniform sampler2D tex;          // window texture
uniform sampler2D texBlur;      // pre-blurred version (Hyprland framebuffer)
uniform float u_zDepth;         // 0.0 = front, 1.0 = back
uniform float u_blurRadius;     // blur radius in px for this layer
uniform float u_alpha;          // base window alpha (hyprland)
uniform vec2  u_resolution;     // screen resolution

in  vec2 v_texCoord;
out vec4 fragColor;

// Adaptive 9-tap Gaussian blur
vec4 sampleBlur(sampler2D t, vec2 uv, float radius) {
    if (radius < 0.5) return texture(t, uv);
    
    vec2 texel = radius / u_resolution;
    vec4 result = vec4(0.0);
    
    // 3x3 Gaussian kernel with weights
    float weights[9] = float[](
        0.0625, 0.125, 0.0625,
        0.125,  0.25,  0.125,
        0.0625, 0.125, 0.0625
    );
    vec2 offsets[9] = vec2[](
        vec2(-1,-1), vec2(0,-1), vec2(1,-1),
        vec2(-1, 0), vec2(0, 0), vec2(1, 0),
        vec2(-1, 1), vec2(0, 1), vec2(1, 1)
    );
    
    for (int i = 0; i < 9; i++) {
        result += texture(t, uv + offsets[i] * texel) * weights[i];
    }
    return result;
}

void main() {
    // Depth curve — non-linear, more impact on middle layers
    float depth = smoothstep(0.0, 1.0, u_zDepth);
    
    // Mix sharp ↔ blurry based on depth (depth of field)
    vec4 sharp  = texture(tex, v_texCoord);
    vec4 blurry = sampleBlur(tex, v_texCoord, u_blurRadius);
    vec4 color  = mix(sharp, blurry, depth * 0.9);
    
    // Atmospheric tint — distant objects cool visually
    // Mimics how the atmosphere desaturates distant objects
    vec3 tint = mix(
        vec3(1.0, 1.0, 1.0),          // near: pure color
        vec3(0.65, 0.80, 1.0),        // far: cool blue-grey tint
        depth * 0.45
    );
    
    // Opacity by depth
    float alpha = u_alpha * mix(1.0, 0.12, depth * 0.92);
    
    // Subtle vignette on background layers
    vec2 center = v_texCoord - 0.5;
    float vignette = 1.0 - dot(center, center) * depth * 1.2;
    vignette = clamp(vignette, 0.0, 1.0);
    
    fragColor = vec4(color.rgb * tint * vignette, color.a * alpha);
}
```

### 5.2 `depth_dof.frag` — cinematic depth of field

```glsl
// src/render/shaders/depth_dof.frag
// Depth of field with circular bokeh — cinematic effect
// More expensive, toggleable via config: spatial_dof = true

#version 430 core

uniform sampler2D tex;
uniform float u_zDepth;
uniform float u_focalPlane;   // 0.0-1.0, where the focus plane is
uniform float u_aperture;     // 0.0-1.0, how much bokeh (f/stop)
uniform vec2  u_resolution;

in  vec2 v_texCoord;
out vec4 fragColor;

// Circle of Confusion (CoC) — core of photographic depth of field
float circleOfConfusion(float depth, float focal) {
    return abs(depth - focal) * u_aperture * 12.0;
}

vec4 bokehSample(sampler2D t, vec2 uv, float coc) {
    if (coc < 0.5) return texture(t, uv);
    
    vec2 texelSize = coc / u_resolution;
    vec4 result = vec4(0.0);
    float total  = 0.0;
    
    // Circular sampling (real bokeh)
    int samples = 16;
    for (int i = 0; i < samples; i++) {
        float angle  = float(i) / float(samples) * 6.28318;
        float radius = sqrt(float(i) / float(samples));
        vec2  offset = vec2(cos(angle), sin(angle)) * radius;
        
        vec4 s = texture(t, uv + offset * texelSize);
        // Higher brightness = more weight in bokeh (simulates highlights)
        float w = 1.0 + dot(s.rgb, vec3(0.3, 0.59, 0.11)) * 2.0;
        result += s * w;
        total  += w;
    }
    
    return result / total;
}

void main() {
    float coc = circleOfConfusion(u_zDepth, u_focalPlane);
    fragColor = bokehSample(tex, v_texCoord, coc);
}
```

### 5.3 `passthrough_ar.frag` — AR blend (future)

```glsl
// src/render/shaders/passthrough_ar.frag
// Blends Wayland windows with AR camera image
// Activated when spatial_xr_bridge detects an AR headset

#version 430 core

uniform sampler2D texWindow;     // rendered Wayland window
uniform sampler2D texCamera;     // headset camera feed (AR passthrough)
uniform float u_windowAlpha;     // global window transparency in AR
uniform float u_zDepth;

in  vec2 v_texCoord;
out vec4 fragColor;

void main() {
    vec4 window = texture(texWindow, v_texCoord);
    vec4 camera = texture(texCamera, v_texCoord);
    
    // Windows blend over the real world
    // More depth = more transparent (less intrusive)
    float blend = window.a * u_windowAlpha * (1.0 - u_zDepth * 0.5);
    
    fragColor = mix(camera, window, blend);
}
```

---

## 6. Phase 4 — Z Navigation

### 6.1 Modification of `src/managers/input/InputManager.cpp`

```cpp
// InputManager.cpp
// Add in onAxisEvent() or equivalent scroll handler

// ── SPATIAL OS: scroll → Z navigation ──
void CInputManager::handleSpatialScroll(double delta, bool hasModifier) {
    
    // No modifier = Z navigation (paradigm shift)
    // With Super = normal window scroll (legacy behavior)
    if (!hasModifier) {
        if (delta > 0)
            g_pZSpaceManager->navigateForward();   // go deeper
        else
            g_pZSpaceManager->navigateBackward();  // go up a layer
        
        return; // consume event, do not pass to window
    }
    
    // With Super modifier → scroll passes through to window normally
    handleLegacyScroll(delta);
}
// ────────────────────────────────────────

// Add in handleKeybinds():
// spatial_next_layer  → g_pZSpaceManager->navigateForward()
// spatial_prev_layer  → g_pZSpaceManager->navigateBackward()
// spatial_reset_view  → g_pZSpaceManager->resetCamera()
// spatial_pin_window  → g_pZSpaceManager->togglePin(activeWindow)
```

### 6.2 Camera animation with spring physics

Z navigation is not instantaneous — it uses spring physics so movement feels smooth and "weighted":

```cpp
// src/spatial/ZSpaceManager.cpp

void ZSpaceManager::updateCameraAnimation(float deltaTime) {
    // Spring physics — same technique Hyprland uses to move windows
    // F = -k*x - c*v  (force = spring - damping)
    
    float displacement = m_fCameraZTarget - m_fCameraZ;
    float springForce  = displacement * Spatial::Z_ANIM_STIFFNESS;
    float dampForce    = m_fCameraZVelocity * Spatial::Z_ANIM_DAMPING;
    
    float acceleration = springForce - dampForce;
    m_fCameraZVelocity += acceleration * deltaTime;
    m_fCameraZ         += m_fCameraZVelocity * deltaTime;
    
    // Update normalized depth for each window
    for (auto& window : g_pCompositor->m_vWindows) {
        float relZ  = window->m_sSpatialProps.fZPosition - m_fCameraZ;
        float normZ = std::clamp(-relZ / (Spatial::Z_LAYER_STEP * 3.0f),
                                  0.0f, 1.0f);
        window->m_sSpatialProps.fDepthNorm = normZ;
        
        // Update discrete layer
        int layer = static_cast<int>(
            std::round(-relZ / Spatial::Z_LAYER_STEP)
        );
        window->m_sSpatialProps.iZLayer = std::clamp(layer, 0,
            Spatial::Z_LAYERS_COUNT - 1);
    }
}

void ZSpaceManager::navigateForward() {
    int newLayer = std::min(m_iCurrentLayer + 1, Spatial::Z_LAYERS_COUNT - 1);
    setTargetLayer(newLayer);
}

void ZSpaceManager::navigateBackward() {
    int newLayer = std::max(m_iCurrentLayer - 1, 0);
    setTargetLayer(newLayer);
}

void ZSpaceManager::setTargetLayer(int layer) {
    m_iCurrentLayer    = layer;
    m_fCameraZTarget   = -Spatial::LAYER_Z_POSITIONS[layer];
}
```

---

## 7. Phase 5 — Spatial Configuration

### 7.1 New section in `hyprland.conf`

```ini
# ── SPATIAL OS ──────────────────────────────────────
spatial {
    # Enable 3D spatial mode
    enabled = true
    
    # Number of Z layers (2-8)
    z_layers = 4
    
    # Distance between layers in world units
    z_layer_step = 800
    
    # Field of view in degrees (45-90)
    fov = 60
    
    # Animation physics
    anim_stiffness = 200
    anim_damping   = 20
    
    # Visual effects
    depth_of_field   = false  # bokeh (GPU-expensive)
    atmospheric_tint = true   # blue tint at depth
    depth_vignette   = true   # vignette on distant layers
    
    # Scroll without modifier → navigate Z
    # true  = scroll navigates layers (new paradigm)
    # false = normal scroll, use keybinds for Z
    scroll_navigates_z = true
    
    # Default layer rules by app class
    # app_layer = class, layer_number
    app_layer = kitty,       0   # terminal → front
    app_layer = firefox,     0   # browser → front
    app_layer = thunar,      1   # files → layer 1
    app_layer = thunderbird, 1   # mail → layer 1
    app_layer = pavucontrol, 2   # audio → layer 2
    app_layer = nm-applet,   3   # system → background
}

# Z navigation keybinds
bind = SUPER, period,    spatial_next_layer
bind = SUPER, comma,     spatial_prev_layer
bind = SUPER, backslash, spatial_reset_view
bind = SUPER, p,         spatial_pin_window
bind = SUPER, bracketright, spatial_move_window_forward
bind = SUPER, bracketleft,  spatial_move_window_backward
```

---

## 8. Phase 6 — ZSpaceManager

Central component that manages Z-space state, the camera, and window nodes:

```cpp
// src/spatial/ZSpaceManager.hpp

#pragma once
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "../Window.hpp"
#include "SpatialConfig.hpp"

class ZSpaceManager {
public:
    ZSpaceManager();
    ~ZSpaceManager() = default;

    // ── Initialization ──
    void init(int screenW, int screenH);
    void setScreenSize(int w, int h);

    // ── Camera ──
    float     getCameraZ()         const { return m_fCameraZ; }
    float     getCameraZTarget()   const { return m_fCameraZTarget; }
    int       getCurrentLayer()    const { return m_iCurrentLayer; }
    glm::mat4 getViewMatrix()      const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjection()  const;

    // ── Navigation ──
    void navigateForward();
    void navigateBackward();
    void setTargetLayer(int layer);
    void resetCamera();

    // ── Z window management ──
    void assignWindowToLayer(CWindow* w, int layer);
    void moveWindowForward(CWindow* w);
    void moveWindowBackward(CWindow* w);
    void togglePin(CWindow* w);
    void autoAssignLayer(CWindow* w);  // based on config rules

    // ── Update loop ──
    void update(float deltaTime);

    // ── Callbacks ──
    using LayerChangedCb = std::function<void(int oldLayer, int newLayer)>;
    void onLayerChanged(LayerChangedCb cb) { m_cbLayerChanged = cb; }

    // ── Queries ──
    std::vector<CWindow*> getWindowsInLayer(int layer) const;
    float getWindowDepthNorm(CWindow* w) const;
    bool  isWindowVisible(CWindow* w) const;

private:
    // Camera state
    float m_fCameraZ        = 0.0f;
    float m_fCameraZTarget  = 0.0f;
    float m_fCameraZVelocity = 0.0f;
    int   m_iCurrentLayer   = 0;

    // Screen
    int m_iScreenW = 1920;
    int m_iScreenH = 1080;

    // Config
    SpatialConfig* m_pConfig = nullptr;

    // Callbacks
    LayerChangedCb m_cbLayerChanged;

    // Internal
    void updateCameraAnimation(float deltaTime);
    void updateWindowDepths();
    void notifyLayerChanged(int oldLayer, int newLayer);
};

// Global singleton (same pattern as Hyprland's g_pCompositor)
inline std::unique_ptr<ZSpaceManager> g_pZSpaceManager;
```

---

## 9. Upstream Synchronization

Hyprland releases frequently. Strategy for keeping the fork up to date:

### 9.1 Branch structure

```
main              ← stable fork, always compiles
develop           ← spatial feature integration
upstream-sync     ← temporary branch for rebases

Hyprland upstream releases:
  hyprland/v0.45 ← tagged in the fork for reference
  hyprland/v0.46 ← when released, rebase spatial on top
```

### 9.2 Sync script

```bash
#!/usr/bin/env bash
# scripts/sync-upstream.sh
# Syncs the fork with the latest Hyprland version

set -e

UPSTREAM_REMOTE="https://github.com/hyprwm/Hyprland.git"
SPATIAL_BRANCH="develop"

echo "→ Fetching upstream Hyprland..."
git remote add upstream $UPSTREAM_REMOTE 2>/dev/null || true
git fetch upstream

echo "→ Checking upstream version..."
UPSTREAM_TAG=$(git describe --tags upstream/main 2>/dev/null || echo "unknown")
echo "   Upstream: $UPSTREAM_TAG"

echo "→ Creating sync branch..."
git checkout -b "upstream-sync-$(date +%Y%m%d)" upstream/main

echo "→ Cherry-picking spatial commits..."
# Spatial commits are tagged with [SPATIAL] in the commit message
git log $SPATIAL_BRANCH --oneline --grep="\[SPATIAL\]" \
    --format="%H" | tac | xargs git cherry-pick

echo "→ Running build test..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)

echo "✓ Sync complete. Review changes and merge to develop."
```

### 9.3 Commit convention

All fork commits must be prefixed to facilitate cherry-pick:

```
[SPATIAL] feat: add m_fZPosition to CWindow
[SPATIAL] feat: perspective projection in Renderer
[SPATIAL] feat: depth_spatial.frag shader
[SPATIAL] fix: depth sorting order for transparency
[SPATIAL] config: add spatial{} section parser
```

Commits without `[SPATIAL]` are assumed to be upstream changes and are discarded during synchronization.

---

## 10. Testing and Validation

### 10.1 Nested Wayland compositor test

The safest development method — run the modified compositor inside your normal Hyprland session:

```bash
# Terminal 1 — build and run the fork in nested mode
cd spatial-hypr
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Compositor opens in a window inside your current session
# If it crashes, your desktop keeps running
WAYLAND_DISPLAY=wayland-99 ./build/Hyprland --nested

# Terminal 2 — launch test apps inside the nested compositor
WAYLAND_DISPLAY=wayland-99 kitty &
WAYLAND_DISPLAY=wayland-99 firefox &
```

### 10.2 Rendering test suite

```cpp
// tests/spatial/test_zspace.cpp

TEST(ZSpaceManager, LayerNavigation) {
    ZSpaceManager mgr;
    mgr.init(1920, 1080);
    
    EXPECT_EQ(mgr.getCurrentLayer(), 0);
    
    mgr.navigateForward();
    EXPECT_EQ(mgr.getCurrentLayer(), 1);
    
    mgr.navigateBackward();
    EXPECT_EQ(mgr.getCurrentLayer(), 0);
    
    // Must not navigate past the boundary
    mgr.navigateBackward();
    EXPECT_EQ(mgr.getCurrentLayer(), 0);
}

TEST(ZSpaceManager, WindowDepthNorm) {
    ZSpaceManager mgr;
    mgr.init(1920, 1080);
    
    CWindow mockWindow;
    mockWindow.m_sSpatialProps.fZPosition = 0.0f;    // layer 0
    EXPECT_NEAR(mgr.getWindowDepthNorm(&mockWindow), 0.0f, 0.01f);
    
    mockWindow.m_sSpatialProps.fZPosition = -800.0f; // layer 1
    EXPECT_NEAR(mgr.getWindowDepthNorm(&mockWindow), 0.33f, 0.05f);
}

TEST(ZSpaceManager, SpringAnimation) {
    ZSpaceManager mgr;
    mgr.init(1920, 1080);
    mgr.navigateForward(); // target: layer 1
    
    // Simulate 500ms of animation
    for (int i = 0; i < 50; i++)
        mgr.update(0.010f);
    
    // Camera must have moved close to target
    float target = -Spatial::LAYER_Z_POSITIONS[1];
    EXPECT_NEAR(mgr.getCameraZ(), target, 50.0f); // within 50 units
}
```

### 10.3 Visual validation — checklist by phase

```markdown
Phase 1 (Z Structure)
  [ ] CWindow compiles without errors with the new fields
  [ ] m_fZPosition initializes to 0.0f on new windows
  [ ] Config rules assign layers correctly

Phase 2 (Perspective)
  [ ] Windows show perspective effect when changing layer
  [ ] Depth sorting prevents z-fighting in transparencies
  [ ] Camera moves smoothly between layers

Phase 3 (Shaders)
  [ ] Layer 0 windows: sharp, opacity 1.0
  [ ] Layer 1 windows: 1.5px blur, opacity 0.75
  [ ] Layer 2 windows: 5px blur, opacity 0.40
  [ ] Cool-blue tint visible on layers 2 and 3
  [ ] No visual artifacts at window edges

Phase 4 (Navigation)
  [ ] Scroll without modifier → changes Z layer
  [ ] Scroll with Super → normal window scroll
  [ ] SUPER+. and SUPER+, keybinds work
  [ ] Spring animation without excessive overshooting

Phase 5 (Config)
  [ ] hyprland.conf parses spatial{} section without error
  [ ] app_layer assigns layers by class correctly
  [ ] spatial_dof = true activates the bokeh shader
```

---

## 11. Agent Tasks

### `@architect`

```markdown
@architect TASK-SH-001
Review the Renderer.cpp source code from Hyprland v0.45
and identify the exact insertion point for the perspective matrix.
Document the existing functions used to transform window coordinates
to screen coordinates.
Deliverable: map of affected functions + insertion decision.

@architect TASK-SH-002
Design the depth sorting strategy compatible with Hyprland's existing
layer system (overlay, top, normal, bottom).
Hyprland layers must coexist with spatial Z layers without conflict.
Deliverable: layer precedence diagram + sort pseudocode.

@architect TASK-SH-003
Design the parser for the spatial{} section in hyprland.conf.
Hyprland uses its own configuration system (hyprlang).
Document how to add a new section without breaking the existing parser.
Deliverable: minimal diff needed in the parser + definition of all fields.

@architect TASK-SH-004
Evaluate the performance impact of 3D perspective.
Hyprland optimizes rendering with damage tracking (only redraws areas
that change). 3D perspective may invalidate those optimizations.
Propose a strategy to preserve damage tracking with Z active.
Deliverable: impact analysis + mitigation proposal.
```

### `@refactor`

```markdown
[x] COMPLETED
@refactor TASK-SH-101
Implement modifications to Window.hpp and Window.cpp.
Add SSpatialProps at the end of CWindow without touching existing fields.
Initialize all fields in the constructor.
Tests: verify that existing Hyprland builds still pass.

[x] COMPLETED
@refactor TASK-SH-102
Implement ZSpaceManager.hpp and ZSpaceManager.cpp in full
according to the interface contract in this specification.
Include all tests from test_zspace.cpp.
The manager must compile in isolation (mock CWindow for tests).

[x] COMPLETED
@refactor TASK-SH-103
Implement depth_spatial.frag and depth_dof.frag.
Integrate them into Hyprland's shader system.
Validate compilation in GLSL 4.30 core profile.
Test: script that compiles shaders with glslangValidator.

@refactor TASK-SH-104
Implement the Renderer.cpp modification.
Point A: getSpatialProjection() function with glm::perspective.
Point B: depth sorting before the render loop.
Point C: pass u_zDepth and u_blurRadius uniforms per window.
Run in nested Wayland mode and capture a validation screenshot.

@refactor TASK-SH-105
Implement modifications to InputManager.cpp.
Intercept scroll without modifier → ZSpaceManager::navigateForward/Backward.
Register spatial_* keybinds in Hyprland's keybind system.
Test: verify that scroll with Super still works normally.

[x] COMPLETED
@refactor TASK-SH-106
Implement the sync-upstream.sh script and git remote configuration.
Create SPATIAL_CHANGES.md with the initial change log.
Create UPSTREAM_SYNC.md with step-by-step rebase instructions.
Configure GitHub Actions for automatic build on each PR.
```

---

## 12. Interface Contracts

### 12.1 Public ZSpaceManager interface (summary)

```cpp
// What the rest of the compositor needs to know about ZSpaceManager

class ZSpaceManager {
public:
    // Renderer calls this every frame
    glm::mat4 getViewProjection() const;
    float     getWindowDepthNorm(CWindow*) const;

    // InputManager calls this on scroll/keybind events
    void navigateForward();
    void navigateBackward();

    // Compositor calls this when creating/destroying windows
    void autoAssignLayer(CWindow*);
    void removeWindow(CWindow*);

    // Main loop calls this every tick
    void update(float deltaTime);
};
```

### 12.2 Shader uniforms per window

```cpp
// Contract between Renderer.cpp and the spatial shaders
// These uniforms MUST be present in all spatial shaders

struct SpatialShaderUniforms {
    float u_zDepth;        // 0.0-1.0, normalized depth
    float u_blurRadius;    // px blur for this layer
    float u_alpha;         // opacity calculated from Z
    vec2  u_resolution;    // screen resolution
    // + standard Hyprland uniforms (proj, tex, etc.)
};
```

### 12.3 GitHub Actions — Minimum CI

```yaml
# .github/workflows/spatial-build.yml
name: Spatial Hypr Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    container: archlinux:latest

    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }

      - name: Install deps
        run: |
          pacman -Sy --noconfirm \
            wlroots0.17 wayland wayland-protocols \
            cmake meson clang glm glslang \
            gtest

      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_COMPILER=clang++
          cmake --build build -j$(nproc)

      - name: Validate shaders
        run: |
          glslangValidator src/render/shaders/depth_spatial.frag
          glslangValidator src/render/shaders/depth_dof.frag

      - name: Run spatial tests
        run: ctest --test-dir build -R "spatial" --output-on-failure
```

---

## Roadmap Summary (Historical Snapshot)

> ⚠️ This week-by-week timeline reflects the original Phase 1 execution plan/history.
> For current completion and pending items, use `docs/SPATIAL_STATUS.md`
> and `docs/SPATIAL_CHANGES_FINAL.md` as source of truth.

```
Week 1   TASK-SH-001 + TASK-SH-101
         Environment ready, Window.hpp modified, compiles without errors

Week 2   TASK-SH-102
         ZSpaceManager functional with tests passing

Week 3   TASK-SH-103 + TASK-SH-104 (partial)
         Shaders compiling, perspective visible in nested mode

Week 4   TASK-SH-104 (complete) + TASK-SH-105
         Z navigation functional with scroll and keybinds

Week 5   TASK-SH-003 + TASK-SH-103 (config)
         hyprland.conf with spatial{} section parsed

Week 6   TASK-SH-106 + TASK-SH-002
         CI/CD on GitHub Actions, sync script ready

── RECORDABLE DEMO ──
         3D desktop with Z navigation, depth blur,
         atmospheric tint — unlike anything existing on Linux
```

---

## References

- [Hyprland source](https://github.com/hyprwm/Hyprland) — fork base
- [wlroots renderer](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/render) — rendering reference
- [glm docs](https://glm.g-truc.net) — 3D math, `glm::perspective`
- [glslangValidator](https://github.com/KhronosGroup/glslang) — shader validator
- [Hyprland wiki — plugins](https://wiki.hyprland.org/Plugins/Development) — plugin system context
- [Learn OpenGL — depth testing](https://learnopengl.com/Advanced-OpenGL/Depth-testing)
- [Learn OpenGL — depth of field](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom)

---

*Fork spec — Spatial OS / spatial-hypr*  
*Connected to: `SPATIAL_OS_SPEC.md`, `NO_FORMS_SPEC.md`*  
*Last updated: February 2026*
