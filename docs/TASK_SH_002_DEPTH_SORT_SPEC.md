# TASK-SH-002 — Depth Sorting Strategy (@architect spec)
> Status: **APPROVED — ready for @refactor**  
> Author: @architect  
> Date: February 2026  
> Depends on: TASK-SH-101 (ZSpaceManager ✅), TASK-SH-104 (Renderer.cpp ✅)

---

## 1. Objective

Define the correct, fully compatible depth sorting strategy for spatial Z layers inside
Hyprland's existing multi-pass render pipeline, and identify where the current
implementation is correct vs where it leaves ordering defects.

---

## 2. Hyprland Render Pipeline (per-frame anatomy)

```
renderWorkspace()
│
├─ 1. BACKGROUND layer-shell surfaces   ← always behind everything
├─ 2. BOTTOM layer-shell surfaces       ← behind workspace windows
│
├─ 3. renderWorkspaceWindows() ─────────── the problematic zone
│      ├─ Pass A: non-floating tiled  (RENDER_PASS_MAIN)
│      │    └─ focused tiled deferred to end of pass A
│      ├─ Pass B: non-floating popups (RENDER_PASS_POPUP)
│      └─ Pass C: floating windows    (RENDER_PASS_ALL)
│           └─ focused floating deferred to end of pass C
│
├─ 4. Special workspace (same A/B/C structure)
├─ 5. Pinned windows               ← always above workspace
│
├─ 6. TOP layer-shell surfaces     ← e.g., waybar
├─ 7. IME popups
└─ 8. OVERLAY layer-shell surfaces ← notifications, HUD
```

### Why Three Passes Exist (must be preserved)

| Pass | Purpose | Must stay? |
|------|---------|-----------|
| RENDER_PASS_MAIN | Renders window body + BOTTOM/UNDER decorations | Yes |
| RENDER_PASS_POPUP | Renders child popup surfaces (xdg-popup) that must float above the parent body but below the parent's OVERLAY decorations | **Yes — popup must follow its parent** |
| RENDER_PASS_ALL | Floating: renders body + popups together | Yes — floating windows have no split-pass need |

The MAIN → POPUP sequence is critical. A popup rendered before its parent body will
be composited underneath — breaking tooltips, context menus, dropdown menus.

---

## 3. Current Implementation Assessment

### What is correct ✅

**Intra-class Z sort (implemented in TASK-SH-104):**

```cpp
// Renderer.cpp ~line 401 — [SPATIAL] block
if (g_pZSpaceManager) {
    std::sort(windows.begin(), windows.end(),
        [](const PHLWINDOWREF& a, const PHLWINDOWREF& b) {
            return a->m_sSpatialProps.fZPosition < b->m_sSpatialProps.fZPosition;
        }
    );
}
```

The single sort correctly orders within each class because all three passes iterate the
same `windows` vector in order. Results:
- Layer 3 tiled behind layer 0 tiled ✅  
- Layer 3 floating behind layer 0 floating ✅  
- Popup pass iterates same sorted order → popups of deeper windows rendered before
  popups of nearer windows ✅

**Layer-shell ordering ✅**  
BACKGROUND / BOTTOM always behind workspace windows; TOP / OVERLAY always above — this
maps perfectly to spatial "Overlay" concept (layer -1). No change needed.

**Pinned windows ✅**  
Rendered last, above all workspace content. Maps to spatial Z = +∞ (HUD).

---

### What is wrong ❌

#### Defect 1 — Cross-class ordering: tiled vs floating at different Z layers

The three-pass structure hardcodes: **ALL tiled render before ALL floating**, regardless
of spatial Z. Example failure:

```
Scenario: Firefox (floating, Z layer 3 = far, Z=-2800)
          Terminal (tiled,    Z layer 0 = near, Z=0)

Actual render order:   Terminal (tiled pass A) → Firefox (floating pass C)
Expected render order: Firefox (Z=-2800, behind) → Terminal (Z=0, in front)

Result: Firefox floats visually above the terminal despite being in the far layer.
```

This violates the fundamental spatial guarantee: **deeper = behind**.

#### Defect 2 — Focused window deferral ignores Z layers

The `lastWindow` / `lastFloatingWindow` deferral ensures the focused window renders
last (on top) within its pass. With spatial Z active, a focused window at layer 3
(far/background) renders on top of unfocused windows at layer 0 (foreground).

```
Focused: terminal in layer 3 (far, behind)
Unfocused: browser in layer 0 (foreground, in front)

Actual:   browser rendered, then terminal (focused, last-in-pass) → terminal on top
Expected: terminal (layer 3) behind browser (layer 0)
```

---

## 4. Proposed Fix Strategy

### Algorithm: Z-Bucket Grouped Rendering

Split the single `renderWorkspaceWindows` call into per-Z-layer groups, rendering
from the deepest layer to layer 0. Within each bucket, the existing
MAIN → POPUP → floating pass order is preserved.

#### Pseudocode

```
for each Z layer L from (Z_LAYERS_COUNT-1) down to 0:
    bucket = windows where getWindowLayer(w) == L

    // Pass A: non-floating tiled, focused deferred
    for w in bucket where !floating, sorted by fZPosition:
        renderWindow(w, RENDER_PASS_MAIN)
    renderWindow(focused_tiled_in_bucket, RENDER_PASS_MAIN)

    // Pass B: non-floating popups
    for w in bucket where !floating:
        renderWindow(w, RENDER_PASS_POPUP)

    // Pass C: floating
    for w in bucket where floating, sorted by fZPosition:
        renderWindow(w, RENDER_PASS_ALL)
    renderWindow(focused_floating_in_bucket, RENDER_PASS_ALL)
```

This gives:
- Layer 3 tiled → Layer 3 floating → Layer 2 tiled → Layer 2 floating → ... → Layer 0
- Popups always follow their parent window ✅
- Cross-class Z order correct ✅
- Focused window deferred only within its own Z bucket ✅

#### Why not a single unified sort?

A fully unified list (one sort, one pass loop) would require rendering MAIN and POPUP
in the same iteration, which breaks parent-before-popup ordering across windows:
window A's popup would potentially render before window B (which is between A and the popup
in Z depth). The Z-bucket approach avoids this by keeping popup pass subordinate to
its bucket's MAIN pass.

### Focusing within Z buckets

When spatial is active, the "focused last" rule applies **within its Z bucket only**,
not globally. The focused window in layer 3 renders last within the layer-3 bucket,
then layer-2 bucket renders entirely after that. The net result: the focused layer-3
window still renders before any layer-2 window.

This matches user expectation: "I can see which window I focused in the background
layer, but it's still visually behind the foreground layer."

---

## 5. Compatibility Constraints (must not break)

| Feature | Constraint |
|---------|-----------|
| Workspace animations (`m_renderOffset`, `m_alpha`) | Per-window translate/alpha applied inside `renderWindow()` — unaffected |
| Fullscreen mode | `renderWorkspaceWindowsFullscreen()` is a separate function not touched |
| Fading-out windows (`m_fadingOut`) | Must still be deferred to end of their pass |
| `m_pinned` windows | Never enter the workspace window loop — unaffected |
| Special workspaces | Called separately after main workspace — unaffected |
| No ZSpaceManager (`g_pZSpaceManager == nullptr`) | Must fall through to existing behavior |
| `spatial { enabled = false }` | Same as above — no Z sort applied |

---

## 6. When Spatial is Disabled

When `g_pZSpaceManager` is null or `!g_pSpatialInputHandler->isEnabled()`, the function
must behave exactly as the original Hyprland code. The simplest guard:

```cpp
if (!g_pZSpaceManager || !g_pSpatialInputHandler || !g_pSpatialInputHandler->isEnabled())
    return original_renderWorkspaceWindows(pMonitor, pWorkspace, time);
// else: Z-bucket path
```

All windows go into bucket 0 implicitly (their `m_sSpatialProps.iZLayer` defaulting to 0).

---

## 7. Interface Changes Required

### 7.1 ZSpaceManager — no new public API needed

All needed data is already on `CWindow::m_sSpatialProps`:
- `iZLayer` — discrete layer (0–3)
- `fZPosition` — animated float (for secondary sort within a bucket)

No new methods on ZSpaceManager are needed for this change.

### 7.2 Renderer — refactored function signature

```cpp
// EXISTING — keep as fallback
void CHyprRenderer::renderWorkspaceWindows(
    PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time);

// NEW — called when spatial is active
void CHyprRenderer::renderWorkspaceWindowsSpatial(
    PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time);
```

`renderWorkspaceWindowsSpatial` is added to `Renderer.hpp` as a private method.

The call site in `renderWorkspace()` (~line 1004) becomes:

```cpp
if (g_pZSpaceManager && g_pSpatialInputHandler && g_pSpatialInputHandler->isEnabled())
    renderWorkspaceWindowsSpatial(pMonitor, pWorkspace, time);
else
    renderWorkspaceWindows(pMonitor, pWorkspace, time);
```

Same substitution at the fullscreen branch is **not needed** — fullscreen mode does not
benefit from Z sorting (only one window is fullscreen at a time).

---

## 8. Secondary Z sort within a bucket

Within a single bucket, multiple windows may share the same discrete layer but have
continuous `fZPosition` values that differ (e.g., a window in transition between layers
is mid-animation). Sort by `fZPosition` inside each bucket to get smooth transitions:

```cpp
// sort within bucket: back to front
std::sort(bucket.begin(), bucket.end(),
    [](const PHLWINDOWREF& a, const PHLWINDOWREF& b) {
        return a->m_sSpatialProps.fZPosition < b->m_sSpatialProps.fZPosition;
    }
);
```

---

## 9. Blur / Opacity Application

Spatial blur and opacity (`LAYER_BLUR_RADIUS`, `LAYER_OPACITY`) are already applied in
`OpenGL.cpp` inside `renderTexture()` using `g_pZSpaceManager->getWindowBlurRadius()`.
This happens at `renderWindow()` time, which is per-call — the Z-bucket approach does
not change this. No change to OpenGL.cpp is required for this task.

---

## 10. Acceptance Criteria for @refactor

- [ ] `renderWorkspaceWindowsSpatial()` implemented and declared in `Renderer.hpp`
- [ ] When spatial active: a floating window at layer 3 renders **behind** a tiled at layer 0
- [ ] When spatial active: focused window at layer 2 renders behind unfocused at layer 0
- [ ] When spatial active: popups still appear above their parent window surface
- [ ] When spatial disabled (`enabled = false`): behaviour identical to upstream Hyprland
- [ ] When `g_pZSpaceManager == nullptr`: original function is called, no crash
- [ ] Tests: `SpatialDepthSortTest` covering the three cross-class scenarios above
- [ ] Build clean — no new clang-tidy warnings
- [ ] All 75 existing spatial tests still pass

---

## 11. @refactor Task Generated

```
@refactor TASK-SH-201  renderWorkspaceWindowsSpatial() — Z-bucket grouped render function
                       Files: src/render/Renderer.cpp, src/render/Renderer.hpp
                       Depends on: this spec (TASK-SH-002)
```

---

*TASK-SH-002 — Spatial OS depth sorting @architect spec*  
*Feb 2026*
