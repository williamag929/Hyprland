# SPATIAL OS — AR/VR System Specification for Linux
> Design and architecture document for AI-assisted development  
> Version: 0.1.0 | Status: DRAFT  
> Agents: `@architect` `@refactor`

---

## Table of Contents

1. [Product Vision](#1-product-vision)
2. [General Architecture](#2-general-architecture)
3. [System Components](#3-system-components)
4. [Z-Depth Layers](#4-z-depth-layers)
5. [AR/VR Integration with OpenXR](#5-arvr-integration-with-openxr)
6. [Supported Hardware](#6-supported-hardware)
7. [Technology Stack](#7-technology-stack)
8. [Agent Tasks](#8-agent-tasks)
9. [Development Roadmap](#9-development-roadmap)
10. [Interface Contracts](#10-interface-contracts)
11. [Acceptance Criteria](#11-acceptance-criteria)

---

## 1. Product Vision

**Spatial OS** is a Linux desktop environment where navigation occurs in three real dimensions. Instead of flat menus and icon grids, applications float in **Z-depth layers** — the user moves forward and backward in space to access different work contexts.

The system operates in two modes:

- **3D desktop mode** — Wayland compositor with perspective and depth on a conventional flat screen
- **AR/VR mode** — windows are projected into physical space through a headset (Meta Quest 3, AR glasses) or phone (WebXR + ARCore)

### Design Principles

- **Depth = priority**: urgent items are close, passive items are far
- **No menus**: navigation is spatial, not hierarchical
- **Hardware agnostic**: OpenXR as a universal abstraction layer
- **Linux native**: Wayland, open source, no mandatory proprietary dependencies

---

## 2. General Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    SPATIAL OS                           │
│                                                         │
│  ┌─────────────────┐     ┌──────────────────────────┐  │
│  │  Spatial Shell  │     │     App Nodes (Z-space)  │  │
│  │  (HUD, launcher │     │  Layer 0 | Layer 1 | ... │  │
│  │   notifications)│     │  foreground → background │  │
│  └────────┬────────┘     └────────────┬─────────────┘  │
│           │                           │                 │
│           └──────────┬────────────────┘                 │
│                      ▼                                  │
│         ┌────────────────────────┐                      │
│         │   Spatial Compositor   │                      │
│         │  (wlroots + OpenGL)    │                      │
│         │  Z-camera | shaders    │                      │
│         └────────────┬───────────┘                      │
│                      │                                  │
└──────────────────────┼──────────────────────────────────┘
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
┌──────────────────┐     ┌──────────────────────┐
│  Wayland Display │     │    OpenXR Layer       │
│  (flat screen)   │     │  (headset / mobile)   │
└──────────────────┘     └──────────┬────────────┘
                                    │
                     ┌──────────────┼──────────────┐
                     ▼              ▼               ▼
               Meta Quest 3    Monado (PC)     WebXR (mobile)
```

---

## 3. System Components

### 3.1 `spatial-compositor`
Wayland compositor based on **wlroots**. Manages windows as 3D objects with position `(x, y, z)`, scale, and opacity dependent on camera distance.

**Responsibilities:**
- Assign and maintain the Z position of each window
- Render with 3D perspective using OpenGL
- Apply distance shaders (blur, fade, scale)
- Expose extended Wayland protocol: `xdg-shell` + `spatial-shell-v1` (custom protocol)

**Key files:**
```
src/
  compositor/
    SpatialCompositor.cpp     # main entry point, wlroots setup
    ZSpaceManager.cpp         # Z-layer management
    SpatialRenderer.cpp       # OpenGL rendering with perspective
    shaders/
      depth_fade.glsl         # opacity by distance
      depth_blur.glsl         # blur by distance
      passthrough_ar.glsl     # AR blend with real image
```

### 3.2 `spatial-shell`
The user interface — HUD, app launcher, notifications. Equivalent to GNOME Shell but for 3D space.

**Responsibilities:**
- Render the HUD (clock, metrics, Z indicator)
- Manage the spatial application launcher
- Handle navigation gestures (scroll, swipe, controllers)
- Communicate with `spatial-compositor` via the `spatial-shell-v1` protocol

### 3.3 `spatial-xr-bridge`
Abstraction layer between the compositor and OpenXR. Translates the 2.5D window space into a real XR space with head and hand tracking.

**Responsibilities:**
- Initialize OpenXR session
- Read headset pose → update compositor camera
- Map controllers/hands → Wayland input events
- Manage AR passthrough (Quest 3 cameras)
- WebXR mode: serialize frames and send to phone browser

### 3.4 `spatial-protocol`
Definition of the extended Wayland protocol `spatial-shell-v1`.

```xml
<!-- spatial-shell-v1.xml (excerpt) -->
<interface name="spatial_surface_v1" version="1">
  <request name="set_z_layer">
    <arg name="layer" type="int" summary="depth: 0=front, N=back"/>
  </request>
  <request name="set_spatial_position">
    <arg name="x" type="fixed"/>
    <arg name="y" type="fixed"/>
    <arg name="z" type="fixed"/>
  </request>
  <event name="z_layer_changed">
    <arg name="new_layer" type="int"/>
  </event>
</interface>
```

---

## 4. Z-Depth Layers

The system organizes space into **discrete layers** (0 to N) plus continuous positioning within each layer.

### 4.1 Layer Model

| Layer | Name | Use | Visual Distance |
|-------|------|-----|----------------|
| 0 | Foreground | Active app, full focus | 0 — in front |
| 1 | Near | Recently used apps | ~600px Z |
| 2 | Mid | Background apps | ~1200px Z |
| 3 | Far | System, config, AI | ~2000px Z |
| -1 | Overlay | HUD, notifications | Always on top |

### 4.2 Visual Behavior by Distance

```
Distance 0   → opacity: 1.0  | scale: 1.0   | blur: 0px
Distance 1   → opacity: 0.75 | scale: 0.85  | blur: 1px
Distance 2   → opacity: 0.45 | scale: 0.65  | blur: 4px
Distance 3   → opacity: 0.2  | scale: 0.45  | blur: 10px
```

### 4.3 Navigation

- **Scroll / mouse wheel** → move camera on Z axis (advance/retreat layer)
- **↑↓ keys / ArrowUp/Down** → discrete navigation between layers
- **Right drag** → free rotation of space (±25° X, ±30° Y)
- **Pinch gesture** (touch / controllers) → continuous Z zoom
- **Gaze + dwell** (AR headset) → selection by gaze

---

## 5. AR/VR Integration with OpenXR

### 5.1 OpenXR Session

```cpp
// spatial-xr-bridge: basic initialization
XrInstance instance;
XrSession session;
XrSpace referenceSpace;  // XR_REFERENCE_SPACE_TYPE_LOCAL

// Required extensions
const char* extensions[] = {
  "XR_KHR_opengl_enable",
  "XR_FB_passthrough",          // AR passthrough Meta Quest
  "XR_EXT_hand_tracking",       // hand tracking
  "XR_KHR_android_surface_swapchain"  // Android/Quest
};
```

### 5.2 XR Render Loop

```
Per frame:
  1. xrWaitFrame() → wait for headset sync
  2. xrBeginFrame()
  3. Read pose: xrLocateSpace(headSpace, referenceSpace) → viewMatrix
  4. Update compositor camera with viewMatrix
  5. Render each eye (left/right swapchain)
  6. Composite Wayland windows over AR image
  7. xrEndFrame() → present
```

### 5.3 AR Modes

**Passthrough (Meta Quest 3 / Quest Pro)**
```
xrCreatePassthroughFB()
xrPassthroughLayerCreateFB(XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB)
// Windows are rendered over the real-world camera image
```

**WebXR (mobile)**
```javascript
// In the phone browser
const session = await navigator.xr.requestSession('immersive-ar', {
  requiredFeatures: ['hit-test', 'anchors'],
  optionalFeatures: ['dom-overlay', 'depth-sensing']
});
// The compositor streams frames via WebSocket or WebRTC
// Windows are anchored to the world using hit-test
```

### 5.4 XR Input

| Source | Action | Result |
|--------|--------|--------|
| Controller trigger | Click on node | Activate app |
| Controller thumbstick Y | Scroll | Navigate Z layer |
| Hand pinch | Select | Click in window |
| Hand grab + move | Drag | Reposition window in space |
| Gaze + 1.5s dwell | Focus | Focus window |
| Head nod (gesture) | Confirm | Accept dialog |

---

## 6. Supported Hardware

### Priority 1 — Meta Quest 3
- Runtime: OpenXR via Meta XR SDK
- Passthrough: color, 4K per eye
- Input: Touch Plus controllers + hand tracking
- Linux PC connection: ALVR (WiFi, ~15ms latency) or USB cable

### Priority 2 — Monado (native Linux)
- Any OpenXR-compatible headset (Valve Index, Varjo, etc.)
- 100% open source runtime
- No proprietary intermediaries

### Priority 3 — WebXR on Android
- ARCore (Android 7+)
- Google Cardboard / Cardboard glasses (~$5) for stereoscopy
- PC streaming via WebSocket/WebRTC

### Priority 4 — Flat screen (desktop mode)
- No headset, just the 3D metaphor on a 2D screen
- Mouse scroll to navigate Z
- Full compatibility with any Linux/Wayland setup

---

## 7. Technology Stack

### Core
| Component | Technology | Minimum Version |
|---|---|---|
| Window protocol | Wayland | 1.22 |
| Compositor base | wlroots | 0.17 |
| Rendering | OpenGL | 4.3 |
| Modern GPU alternative | Vulkan | 1.3 |
| XR standard | OpenXR | 1.0 |
| XR Linux runtime | Monado | 23.0 |
| Primary language | C++20 | GCC 13 / Clang 16 |
| Build | CMake + Meson | 3.25 |
| Shaders | GLSL | 4.30 |

### Additional Dependencies
```cmake
find_package(wlroots REQUIRED)
find_package(OpenXR REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)          # 3D math
find_package(nlohmann_json REQUIRED) # config
find_package(spdlog REQUIRED)        # logging
```

### For WebXR bridge
```json
{
  "dependencies": {
    "ws": "^8.0",
    "wrtc": "^0.4",
    "three": "^0.160"
  }
}
```

---

## 8. Agent Tasks

### `@architect` — Design and structure

The `@architect` agent is responsible for high-level design decisions, definition of interfaces between components, and overall architectural coherence of the system.

**Assigned tasks:**

```markdown
@architect TASK-001
Design the extended Wayland protocol `spatial-shell-v1`.
Define all required requests, events and enums for:
- Assigning Z position to surfaces
- Notifying active layer changes
- Registering apps as "spatial nodes" with metadata (icon, label, color)
Deliverable: complete XML protocol file + documentation for each message.

@architect TASK-002
Design the C++ API of ZSpaceManager.
Requirements:
- Manage up to 8 Z layers
- Support continuous positioning within each layer
- Events when camera changes layer
- Thread-safe (compositor and XR bridge run on separate threads)
Deliverable: .hpp header with complete public API + class diagram.

@architect TASK-003
Design the architecture of spatial-xr-bridge.
Must support multiple backends (Monado, Meta, WebXR) with the same interface.
Use the Strategy pattern for backends.
Deliverable: class diagram + abstract C++ interfaces.

@architect TASK-004
Define the data model for a "Spatial Node" (app in space).
Must include: 3D position, Z layer, state (active/inactive/hidden),
associated Wayland surface, visual metadata.
Deliverable: struct/class with JSON serialization for persistence.

@architect TASK-005
Design the gesture system.
Unify input from: mouse, keyboard, touch, XR controllers, hand tracking.
Abstraction that emits semantic events: "navigate_z(+1)", "select()", "drag(dx,dy,dz)".
Deliverable: event flow diagram + GestureEngine class with public API.
```

---

### `@refactor` — Implementation and quality

The `@refactor` agent is responsible for code implementation, refactoring to maintain quality, and ensuring existing code meets project standards.

**Assigned tasks:**

```markdown
@refactor TASK-101
Implement ZSpaceManager.cpp based on @architect TASK-002 design.
Implementation requirements:
- std::map<int, std::vector<SpatialNode*>> to organize nodes by layer
- mutex for thread safety
- getCameraTransform(float cameraZ) function returning glm::mat4
- Unit tests with Google Test
Reference code: src/compositor/ZSpaceManager.hpp (to be created by @architect)

@refactor TASK-102
Implement the GLSL depth shaders.
Files: depth_fade.glsl, depth_blur.glsl
- depth_fade: receives uniform float u_depth (0.0-1.0), applies opacity curve
- depth_blur: implement 9-tap Gaussian blur with radius proportional to u_depth
- Must compile in OpenGL 4.3 core profile
Visual reference: layer 0 → sharp; layer 3 → 10px blur + 20% opacity

@refactor TASK-103
Refactor the existing HTML/JS prototype (spatial-os.html) to:
1. Extract ZSpace logic to a pure JavaScript class (no DOM)
2. Add WebXR support with fallback to 2D mode
3. Add WebSocket connection to receive state from Linux compositor
4. Maintain compatibility with current standalone mode
Standard: ES2022, no external dependencies except three.js for XR

@refactor TASK-104
Implement the WebXR backend of spatial-xr-bridge.
Node.js + ws for streaming frames from compositor to mobile.
- Protocol: WebSocket with JSON messages for state + ArrayBuffer for frames
- Target framerate: 60fps on 5GHz WiFi
- Compression: H.264 via wasm-av1 or mjpeg as fallback

@refactor TASK-105
Create the complete project CMakeLists.txt.
Target structure:
  - spatial-compositor (executable)
  - spatial-shell (executable)
  - spatial-xr-bridge (executable)
  - libspatial-core (shared library with ZSpaceManager, GestureEngine)
  - spatial-protocol (target to generate Wayland code from XML)
Include: tests, installation, pkg-config

@refactor TASK-106
Implement the configuration system.
File: ~/.config/spatial-os/config.json
Fields: active layers, keybindings, preferred hardware, visual theme.
Use nlohmann/json + schema validation.
Must hot-reload (inotify) without restarting the compositor.
```

---

## 9. Development Roadmap

### Phase 0 — Foundations (Weeks 1-4)
```
[ ] @architect  TASK-001  spatial-shell-v1 protocol
[ ] @architect  TASK-002  ZSpaceManager API
[ ] @refactor   TASK-105  CMakeLists.txt
[ ] @refactor   TASK-102  Base GLSL shaders
```

### Phase 1 — Minimal compositor (Weeks 5-10)
```
[ ] @refactor   TASK-101  ZSpaceManager implemented
[ ] @architect  TASK-004  SpatialNode model
[ ] @architect  TASK-005  Gesture system
[ ] Milestone: Wayland windows with Z navigation on flat screen
```

### Phase 2 — WebXR on mobile (Weeks 11-16)
```
[ ] @refactor   TASK-103  HTML prototype → WebXR
[ ] @refactor   TASK-104  Node.js WebXR bridge
[ ] Milestone: Linux desktop visible on Android phone with ARCore
```

### Phase 3 — Native OpenXR (Weeks 17-28)
```
[ ] @architect  TASK-003  XR bridge architecture
[ ] Implement Monado backend
[ ] Implement Meta Quest backend (via ALVR)
[ ] @refactor   TASK-106  Configuration system
[ ] Milestone: 3D desktop on Meta Quest 3 with AR passthrough
```

### Phase 4 — Polish and release (Weeks 29-40)
```
[ ] Full hand tracking
[ ] Layer transition animations
[ ] Multi-monitor support in desktop mode
[ ] Packaging: AUR (Arch), Flatpak, .deb
[ ] Milestone: v1.0.0 public release
```

---

## 10. Interface Contracts

### 10.1 IXRBackend (abstract interface for XR backends)

```cpp
class IXRBackend {
public:
  virtual ~IXRBackend() = default;

  // Lifecycle
  virtual bool initialize() = 0;
  virtual void shutdown() = 0;
  virtual bool isAvailable() const = 0;

  // Frame loop
  virtual XRFrameState waitFrame() = 0;
  virtual void beginFrame(const XRFrameState&) = 0;
  virtual void endFrame(const XRFrameState&, const std::vector<XRLayer>&) = 0;

  // Tracking
  virtual glm::mat4 getHeadPose() const = 0;
  virtual std::optional<HandPose> getHandPose(Hand hand) const = 0;

  // AR
  virtual bool supportsPassthrough() const = 0;
  virtual void setPassthroughEnabled(bool enabled) = 0;

  // Events
  virtual void setInputCallback(InputCallback cb) = 0;
};

// Concrete implementations
class MonadoBackend    : public IXRBackend { ... };
class MetaQuestBackend : public IXRBackend { ... };
class WebXRBackend     : public IXRBackend { ... };
class NullBackend      : public IXRBackend { ... }; // desktop mode
```

### 10.2 ZSpaceManager Public API

```cpp
class ZSpaceManager {
public:
  // Node management
  NodeId addNode(SpatialNode node);
  void   removeNode(NodeId id);
  void   setNodeLayer(NodeId id, int layer);
  void   setNodePosition(NodeId id, glm::vec3 pos);

  // Camera
  void      setCameraLayer(int layer);
  void      setCameraZ(float z);
  float     getCameraZ() const;
  glm::mat4 getCameraTransform() const;

  // Queries
  std::vector<NodeId> getNodesInLayer(int layer) const;
  std::vector<NodeId> getVisibleNodes(float maxDistance = 2.0f) const;
  float               getNodeVisibility(NodeId id) const; // 0.0 - 1.0

  // Events
  using LayerChangedCb = std::function<void(int oldLayer, int newLayer)>;
  void setLayerChangedCallback(LayerChangedCb cb);
};
```

### 10.3 WebSocket Protocol (bridge → mobile)

```typescript
// Messages from compositor to WebXR client

type ServerMessage =
  | { type: 'frame';  data: ArrayBuffer; timestamp: number }
  | { type: 'state';  nodes: SpatialNode[]; cameraZ: number }
  | { type: 'event';  kind: 'node_added' | 'node_removed'; nodeId: string }

type ClientMessage =
  | { type: 'input';  gesture: GestureEvent }
  | { type: 'pose';   head: Matrix4x4; leftHand?: HandData; rightHand?: HandData }
  | { type: 'resize'; width: number; height: number }

interface SpatialNode {
  id:      string
  label:   string
  icon:    string
  layer:   number
  pos:     [number, number, number]
  visible: number  // 0.0 - 1.0
  color:   string
}
```

---

## 11. Acceptance Criteria

### Performance
- [ ] Stable 60fps on flat screen (mid-range GPU: RX 580 / GTX 1060)
- [ ] 72fps on Meta Quest 3 (standalone mode via ALVR)
- [ ] Input→render latency < 20ms in desktop mode
- [ ] End-to-end latency (compositor → Quest) < 30ms on 5GHz WiFi
- [ ] Memory usage < 200MB base without applications

### Functionality
- [ ] Z-layer navigation with scroll/keyboard
- [ ] At least 20 simultaneous windows without degradation
- [ ] Window position persistence between sessions
- [ ] Configuration hot-reload without restart
- [ ] Graceful fallback to 2D mode when no XR hardware is present

### Compatibility
- [ ] Any Wayland app runs without modifications
- [ ] XWayland for legacy X11 apps
- [ ] Works on: Arch Linux, Ubuntu 24.04, Fedora 40
- [ ] Meta Quest 3 via ALVR (WiFi 6 recommended)
- [ ] Android 10+ with ARCore for WebXR

### Code Quality
- [ ] Unit test coverage ≥ 70% in libspatial-core
- [ ] No memory leaks (valgrind clean on test suite)
- [ ] Doxygen documentation on all public APIs
- [ ] Lint: clang-tidy with no warnings at strict level

---

## References

- [The Wayland Book](https://wayland-book.com) — Wayland protocol
- [wlroots wiki](https://gitlab.freedesktop.org/wlroots/wlroots/-/wikis) — compositor base
- [OpenXR spec 1.0](https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html)
- [Monado](https://monado.freedesktop.org) — OpenXR Linux runtime
- [xrdesktop](https://gitlab.freedesktop.org/xrdesktop/xrdesktop) — reference: GNOME in VR
- [Simula](https://github.com/SimulaVR/Simula) — reference: Wayland compositor in VR
- [ALVR](https://github.com/alvr-org/ALVR) — PC→Quest streaming
- [Hyprland](https://github.com/hyprwm/Hyprland) — reference modern C++ compositor
- [learnopengl.com](https://learnopengl.com) — OpenGL for rendering

---

*Generated by SPATIAL OS Design Session — Claude Sonnet 4.6*  
*Last updated: February 2026*
