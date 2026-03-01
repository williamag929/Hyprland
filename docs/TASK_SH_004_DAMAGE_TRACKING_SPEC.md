# TASK-SH-004 — Damage Tracking Impact Analysis (@architect spec)
> Status: **COMPLETE — all fixes implemented**  
> Author: @architect / @refactor  
> Date: February 2026  
> Depends on: TASK-SH-102 (ZSpaceManager ✅), TASK-SH-104 (Renderer.cpp ✅)

---

## 1. Objective

Analyse whether Z-layer navigation over-invalidates damage regions and determine
whether spatial code introduces the `pixman_region32_init_rect` warning seen at runtime.

---

## 2. Summary

Four distinct damage-tracking defects were found in spatial code. All four are now
fixed. The table below is the complete issue list:

| # | Severity | Component | Symptom | Fixed |
|---|----------|-----------|---------|-------|
| D-1 | Critical | `Renderer.cpp` | Z animation freezes mid-transition in VFR mode | ✅ |
| D-2 | High | `Compositor.cpp` | Layer change not painted immediately | ✅ |
| D-3 | Medium | `Renderer.cpp` | pixman `*** BUG *** Invalid rectangle` warning | ✅ |
| D-4 | Low | — | `isAnimating()` did not exist | ✅ |

---

## 3. Defect Analysis

### D-1 — Animation freezes in VFR mode (Critical)

**Root cause:**  
`ZSpaceManager::update()` advances spring physics but never asks the compositor for a
next frame. Hyprland's Variable Frame Rate (VFR) path in `renderMonitor()`:

```cpp
bool hasChanged = pMonitor->m_output->needsFrame || pMonitor->m_damage.hasChanged();
if (!hasChanged && damageModeNotNone && pMonitor->m_forceFullFrames == 0)
    return;  // ← render loop idles here
```

Once a Z-layer change is initiated, the spring starts moving BUT nothing adds damage
or schedules a frame. After the first frame the loop goes idle and the animation
freezes at whatever intermediate position was reached on frame 1.

All other animated variables in Hyprland (`CHyprAnimationManager`) solve this via:
```cpp
g_pCompositor->scheduleFrameForMonitor(PMONITOR, AQ_SCHEDULE_ANIMATION);
```

The spatial camera and window springs had no equivalent call.

**Fix — `Renderer.cpp` ~line 1386:**
```cpp
if (g_pZSpaceManager->isAnimating())
    g_pCompositor->scheduleFrameForMonitor(pMonitor, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
```

Called in the spatial block inside `renderMonitor()`, after `update()`, before the
damage check. This keeps the render loop alive for exactly as long as the springs
are above the 0.1 settle threshold — then stops, allowing true GPU idle.

---

### D-2 — Layer change not painted immediately (High)

**Root cause:**  
The layer-change callback in `Compositor.cpp` (wired at compositor init):

```cpp
g_pSpatialInputHandler->setLayerChangeCallback([](int newLayer, int /*oldLayer*/) {
    g_pZSpaceManager->setActiveLayer(newLayer);
    g_pSpatialInputHandler->setCurrentLayer(g_pZSpaceManager->getActiveLayer());
    // ← nothing here told the compositor to redraw
});
```

`setActiveLayer()` updates `m_fCameraZTarget` and sets spring targets on windows.
But the compositor had no pending damage, so the damage check returned early and the
new opacity/blur values were never composited. The old frame was frozen until an
unrelated content change (cursor move, client buffer update, etc.) triggered a repaint.
In a static desktop this could persist indefinitely.

**Fix — `Compositor.cpp` after `setCurrentLayer()`:**
```cpp
if (g_pHyprRenderer && g_pCompositor) {
    for (auto const& m : g_pCompositor->m_monitors)
        g_pHyprRenderer->damageMonitor(m);
}
```

`damageMonitor()` submits `{0, 0, INT16_MAX, INT16_MAX}` to each monitor's damage ring,
guaranteeing a full redraw on the next frame. Since D-1 also ensures the frame is
scheduled, the new appearance is visible within 1 monitor frame (~16ms at 60Hz).

---

### D-3 — pixman `*** BUG ***` Invalid rectangle warning (Medium)

**Symptom in logs:**
```
*** BUG *** In pixman_region32_init_rect: Invalid rectangle passed
```

**Root cause:**  
`CHyprRenderer::damageWindow()` calls `pWindow->getFullWindowBoundingBox()`. During
window construction — specifically when `Window.cpp` calls
`g_pZSpaceManager->assignWindowToLayer(this, 0)` before the first `configure` event
from the client — the bounding box is `{0, 0, 0, 0}`. The zero-size `CBox` was fed
directly to `m->addDamage()`, which internally creates a pixman region via
`pixman_region32_init_rect(r, x, y, 0, 0)`. Pixman rejects width/height ≤ 0.

This is **not an upstream Hyprland bug** in the general case — upstream does not
call `damageWindow()` during window construction. It's a spatial-specific trigger
because `assignWindowToLayer()` is called from `CWindow`'s constructor path.

**Fix — `Renderer.cpp` in `damageWindow()`, before the monitor loop:**
```cpp
// [SPATIAL] Guard: skip damage for windows with no geometry yet.
if (windowBox.width <= 0 || windowBox.height <= 0)
    return;
```

This is safe: a window with no geometry produces no visible pixels, so skipping its
damage is correct.

---

### D-4 — `isAnimating()` missing (Low)

Required by D-1's fix. Added:

```cpp
// ZSpaceManager.hpp
[[nodiscard]] bool isAnimating() const;

// ZSpaceManager.cpp
bool ZSpaceManager::isAnimating() const {
    std::lock_guard<std::recursive_mutex> lock(m_mtxWindows);
    if (std::abs(m_fCameraZVelocity) > 0.1f || std::abs(m_fCameraZTarget - m_fCameraZ) > 0.1f)
        return true;
    for (const auto& wz : m_vWindowsZ)
        if (std::abs(wz.fZVelocity) > 0.1f || std::abs(wz.fZTarget - wz.fZPosition) > 0.1f)
            return true;
    return false;
}
```

Uses the same `0.1f` threshold as `update()`'s settle check to guarantee that
`isAnimating()` returns false at exactly the same moment the springs stop moving,
with no off-by-one between "spring settled" and "stop requesting frames".

---

## 4. Does Z Navigation Over-Invalidate?

**Answer: No, with the current design.**

A Z layer transition does exactly two damage operations:

1. `damageMonitor()` × number of monitors — triggered once on keybind/scroll (D-2 fix)
2. `scheduleFrameForMonitor(AQ_SCHEDULE_ANIMATION)` — issued every frame until springs settle (~15–30 frames at 60Hz for the default stiffness/damping values)

This is equivalent to any workspace switch animation in upstream Hyprland, which also
damage-monitors on transition and schedules animation frames until the offset spring
settles. There is no excessive or redundant damage beyond what any animated transition
does.

**Worst case damage area:**  
Full-screen per monitor (from `damageMonitor` submitting `{0,0,INT16_MAX,INT16_MAX}`).
This is the same as `wlr_output_damage_whole()`. Happens once per transition, not
per-frame — per-frame the spring scheduling just signals "needs frame", not full damage.

---

## 5. Recommended Tests

These tests should be added to `tests/spatial/SpatialDamageTest.cpp` in a future
`@refactor TASK-SH-TESTS` pass:

| Test | Validation |
|------|-----------|
| `LayerChangeDamagesMonitor` | Calling `setActiveLayer()` via callback results in `damageMonitor()` being invoked |
| `ZeroGeometryWindowNoCrash` | `damageWindow()` on a zero-bounding-box window does not crash or log pixman warning |
| `AnimatingReturnsTrueDuringTransition` | `isAnimating()` returns true immediately after `setActiveLayer()`, false after settle |
| `AnimatingReturnsFalseWhenSettled` | After manual spring settle (`fZPosition = fZTarget, fZVelocity = 0`), `isAnimating()` is false |

---

## 6. Files Modified by This Task

| File | Change |
|------|--------|
| [src/spatial/ZSpaceManager.hpp](../src/spatial/ZSpaceManager.hpp) | Added `isAnimating()` declaration |
| [src/spatial/ZSpaceManager.cpp](../src/spatial/ZSpaceManager.cpp) | Implemented `isAnimating()` |
| [src/render/Renderer.cpp](../src/render/Renderer.cpp) | `scheduleFrameForMonitor` during animation; zero-size damage guard in `damageWindow()` |
| [src/Compositor.cpp](../src/Compositor.cpp) | `damageMonitor()` all monitors on layer-change callback |

---

## 7. Remaining Risk

| Risk | Likelihood | Mitigation |
|------|-----------|-----------|
| `assignWindowToLayer()` called during creation triggers decoration `damageEntire()` before geometry is ready | Low — decoration list is empty at construction | Guard already on `windowBox.width <= 0` in `damageWindow()` |
| Multiple monitors: `damageMonitor()` on all monitors may cause brief visual flash on secondary monitors when only primary layer changes | Very Low — `damageMonitor` just marks full damage, frame scheduling still subject to `needsFrame` | Acceptable; same as workspace switch on multi-monitor |
| `isAnimating()` called while `m_mtxWindows` is already held | Not possible — `isAnimating()` is only called from render thread which never holds the mutex | N/A |

---

*TASK-SH-004 — Spatial OS damage tracking impact analysis*  
*Feb 2026*
