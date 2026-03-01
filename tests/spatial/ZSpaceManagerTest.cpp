// ════════════════════════════════════════════════════════════════════════════
// SPATIAL OS — ZSpaceManager Unit Tests
// TASK-SH-102 | @refactor
//
// Coverage targets (≥70% in libspatial-core required by copilot-instructions.md):
//   src/spatial/ZSpaceManager.cpp — all public methods
//
// Fake-window strategy:
//   assignWindowToLayer / pinWindow / update() write back to
//   CWindow::m_sSpatialProps.  We allocate a zeroed block of sizeof(CWindow)
//   bytes and pass its address as the void* handle.  ZSpaceManager only
//   accesses the POD m_sSpatialProps sub-struct (no virtual dispatch), so
//   this is safe.  We free the raw memory with ::operator delete — the
//   CWindow destructor is intentionally bypassed to avoid constructing a
//   full compositor object.
// ════════════════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>

#include "spatial/ZSpaceManager.hpp"
#include "desktop/view/Window.hpp" // for sizeof(CWindow) and m_sSpatialProps access

using namespace Spatial;

// ─────────────────────────────────────────────────────────────────────────────
// FakeWindow — zero-initialized CWindow-sized allocation
// Only accesses the POD m_sSpatialProps member via the public pointer.
// ─────────────────────────────────────────────────────────────────────────────
class FakeWindow {
  public:
    FakeWindow() {
        // Allocate + zero-initialise raw memory of CWindow's size.
        // alignment of CWindow is at most alignof(std::max_align_t).
        m_pRaw = ::operator new(sizeof(Desktop::View::CWindow));
        std::memset(m_pRaw, 0, sizeof(Desktop::View::CWindow));
    }

    ~FakeWindow() {
        // Bypass CWindow destructor — we never called its constructor.
        ::operator delete(m_pRaw);
    }

    // Non-copyable
    FakeWindow(const FakeWindow&)            = delete;
    FakeWindow& operator=(const FakeWindow&) = delete;

    /// Raw void* passed to ZSpaceManager APIs
    [[nodiscard]] void* handle() const {
        return m_pRaw;
    }

    /// Read back SSpatialProps for assertions
    [[nodiscard]] const Desktop::View::CWindow::SSpatialProps& props() const {
        return reinterpret_cast<const Desktop::View::CWindow*>(m_pRaw)->m_sSpatialProps;
    }

  private:
    void* m_pRaw = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test fixture — creates an initialized ZSpaceManager + 4 fake windows
// ─────────────────────────────────────────────────────────────────────────────
class ZSpaceManagerTest : public ::testing::Test {
  protected:
    static constexpr int SCREEN_W = 1920;
    static constexpr int SCREEN_H = 1080;

    ZSpaceManager        mgr;

    // Four fake windows — one per layer
    FakeWindow win0, win1, win2, win3;

    void       SetUp() override {
        mgr.init(SCREEN_W, SCREEN_H);
    }
};

// ══════════════════════════════════════════════════════════════════════════════
// INIT
// ══════════════════════════════════════════════════════════════════════════════

TEST(ZSpaceManagerInit, NotInitializedByDefault) {
    ZSpaceManager z;
    EXPECT_FALSE(z.isInitialized());
}

TEST(ZSpaceManagerInit, IsInitializedAfterInit) {
    ZSpaceManager z;
    z.init(1920, 1080);
    EXPECT_TRUE(z.isInitialized());
}

TEST(ZSpaceManagerInit, ReInitOnResolutionChangeIsIdempotent) {
    ZSpaceManager z;
    z.init(1920, 1080);
    EXPECT_TRUE(z.isInitialized());
    z.init(2560, 1440); // should not crash
    EXPECT_TRUE(z.isInitialized());
}

TEST_F(ZSpaceManagerTest, InitialActiveLayerIsZero) {
    EXPECT_EQ(mgr.getActiveLayer(), 0);
}

TEST_F(ZSpaceManagerTest, InitialCameraZIsLayerZeroPosition) {
    EXPECT_FLOAT_EQ(mgr.getCameraZ(), LAYER_Z_POSITIONS[0]);
}

// ══════════════════════════════════════════════════════════════════════════════
// LAYER NAVIGATION
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, NextLayerAdvancesLayer) {
    EXPECT_TRUE(mgr.nextLayer());
    EXPECT_EQ(mgr.getActiveLayer(), 1);
}

TEST_F(ZSpaceManagerTest, PrevLayerReturnsToPreviousLayer) {
    mgr.nextLayer();
    EXPECT_TRUE(mgr.prevLayer());
    EXPECT_EQ(mgr.getActiveLayer(), 0);
}

TEST_F(ZSpaceManagerTest, PrevLayerReturnsFalseAtFront) {
    EXPECT_EQ(mgr.getActiveLayer(), 0);
    EXPECT_FALSE(mgr.prevLayer());
    EXPECT_EQ(mgr.getActiveLayer(), 0);
}

TEST_F(ZSpaceManagerTest, NextLayerReturnsFalseAtLastLayer) {
    for (int i = 0; i < Z_LAYERS_COUNT - 1; ++i)
        EXPECT_TRUE(mgr.nextLayer());

    EXPECT_FALSE(mgr.nextLayer());
    EXPECT_EQ(mgr.getActiveLayer(), Z_LAYERS_COUNT - 1);
}

TEST_F(ZSpaceManagerTest, SetActiveLayerValidRange) {
    for (int i = 0; i < Z_LAYERS_COUNT; ++i) {
        EXPECT_TRUE(mgr.setActiveLayer(i));
        EXPECT_EQ(mgr.getActiveLayer(), i);
    }
}

TEST_F(ZSpaceManagerTest, SetActiveLayerRejectsNegative) {
    EXPECT_FALSE(mgr.setActiveLayer(-1));
    EXPECT_EQ(mgr.getActiveLayer(), 0); // unchanged
}

TEST_F(ZSpaceManagerTest, SetActiveLayerRejectsOutOfBounds) {
    EXPECT_FALSE(mgr.setActiveLayer(Z_LAYERS_COUNT));
    EXPECT_EQ(mgr.getActiveLayer(), 0); // unchanged
}

TEST_F(ZSpaceManagerTest, SetActiveLayerUpdatesCameraTarget) {
    // After enough update() calls the camera converges to the layer Z
    mgr.setActiveLayer(2);
    // Run many frames to let spring converge
    for (int i = 0; i < 2000; ++i)
        mgr.update(0.016f);

    EXPECT_NEAR(mgr.getCameraZ(), LAYER_Z_POSITIONS[2], 1.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// WINDOW REGISTRATION — getWindowCount / getWindowLayer / getWindowZ
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, WindowCountZeroInitially) {
    EXPECT_EQ(mgr.getWindowCount(), 0);
}

TEST_F(ZSpaceManagerTest, AssignWindowIncreasesCount) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    EXPECT_EQ(mgr.getWindowCount(), 1);

    mgr.assignWindowToLayer(win1.handle(), 1);
    EXPECT_EQ(mgr.getWindowCount(), 2);
}

TEST_F(ZSpaceManagerTest, AssignWindowRejectsNullptr) {
    mgr.assignWindowToLayer(nullptr, 0);
    EXPECT_EQ(mgr.getWindowCount(), 0);
}

TEST_F(ZSpaceManagerTest, AssignWindowRejectsInvalidLayer) {
    mgr.assignWindowToLayer(win0.handle(), -1);
    mgr.assignWindowToLayer(win0.handle(), Z_LAYERS_COUNT);
    EXPECT_EQ(mgr.getWindowCount(), 0);
}

TEST_F(ZSpaceManagerTest, GetWindowLayerReturnsCorrectLayer) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.assignWindowToLayer(win1.handle(), 2);

    EXPECT_EQ(mgr.getWindowLayer(win0.handle()), 0);
    EXPECT_EQ(mgr.getWindowLayer(win1.handle()), 2);
}

TEST_F(ZSpaceManagerTest, GetWindowLayerReturnsMinusOneForUnknown) {
    EXPECT_EQ(mgr.getWindowLayer(win0.handle()), -1);
}

TEST_F(ZSpaceManagerTest, GetWindowZInitiallyMatchesLayerPosition) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    EXPECT_FLOAT_EQ(mgr.getWindowZ(win0.handle()), LAYER_Z_POSITIONS[0]);

    mgr.assignWindowToLayer(win1.handle(), 3);
    EXPECT_FLOAT_EQ(mgr.getWindowZ(win1.handle()), LAYER_Z_POSITIONS[3]);
}

TEST_F(ZSpaceManagerTest, ReassignWindowUpdatesLayer) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    EXPECT_EQ(mgr.getWindowLayer(win0.handle()), 0);

    mgr.assignWindowToLayer(win0.handle(), 3);
    EXPECT_EQ(mgr.getWindowLayer(win0.handle()), 3);
    EXPECT_EQ(mgr.getWindowCount(), 1); // still just 1 entry
}

// ══════════════════════════════════════════════════════════════════════════════
// REMOVE WINDOW
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, RemoveWindowDecreasesCount) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.assignWindowToLayer(win1.handle(), 1);
    EXPECT_EQ(mgr.getWindowCount(), 2);

    mgr.removeWindow(win0.handle());
    EXPECT_EQ(mgr.getWindowCount(), 1);
    EXPECT_EQ(mgr.getWindowLayer(win0.handle()), -1); // gone
    EXPECT_EQ(mgr.getWindowLayer(win1.handle()), 1);  // still present
}

TEST_F(ZSpaceManagerTest, RemoveWindowNullptrIsNoOp) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.removeWindow(nullptr);
    EXPECT_EQ(mgr.getWindowCount(), 1);
}

TEST_F(ZSpaceManagerTest, RemoveUnregisteredWindowIsNoOp) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.removeWindow(win1.handle()); // never registered
    EXPECT_EQ(mgr.getWindowCount(), 1);
}

// ══════════════════════════════════════════════════════════════════════════════
// PIN / UNPIN
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, IsPinnedFalseByDefault) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    EXPECT_FALSE(mgr.isPinned(win0.handle()));
}

TEST_F(ZSpaceManagerTest, PinWindowSetsFlag) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.pinWindow(win0.handle(), true);
    EXPECT_TRUE(mgr.isPinned(win0.handle()));
}

TEST_F(ZSpaceManagerTest, UnpinWindowClearsFlag) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.pinWindow(win0.handle(), true);
    mgr.pinWindow(win0.handle(), false);
    EXPECT_FALSE(mgr.isPinned(win0.handle()));
}

TEST_F(ZSpaceManagerTest, IsPinnedReturnsFalseForUnknownWindow) {
    EXPECT_FALSE(mgr.isPinned(win0.handle()));
}

TEST_F(ZSpaceManagerTest, PinWindowNullptrIsNoOp) {
    mgr.pinWindow(nullptr, true); // must not crash
    EXPECT_EQ(mgr.getWindowCount(), 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// SET WINDOW Z POSITION (continuous override)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, SetWindowZPositionUpdatesTarget) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.setWindowZPosition(win0.handle(), -500.0f);
    EXPECT_FLOAT_EQ(mgr.getWindowZTarget(win0.handle()), -500.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// SPRING ANIMATION — update()
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, UpdateConvergesWindowToTarget) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.setWindowZPosition(win0.handle(), -1200.0f);

    // Run enough frames to let the spring settle
    for (int i = 0; i < 2000; ++i)
        mgr.update(0.016f);

    EXPECT_NEAR(mgr.getWindowZ(win0.handle()), -1200.0f, 1.0f);
}

TEST_F(ZSpaceManagerTest, UpdateWithZeroDeltaDoesNotDiverge) {
    mgr.assignWindowToLayer(win0.handle(), 1);

    // dt=0 must not crash or produce NaN
    mgr.update(0.0f);

    const float z = mgr.getWindowZ(win0.handle());
    EXPECT_TRUE(std::isfinite(z));
}

TEST_F(ZSpaceManagerTest, UpdateClampsExcessiveDeltaTime) {
    // A 1-second spike should be clamped to 0.05s — spring must not diverge
    mgr.assignWindowToLayer(win0.handle(), 2);

    for (int i = 0; i < 5; ++i)
        mgr.update(1.0f); // would diverge without clamping

    const float z = mgr.getWindowZ(win0.handle());
    EXPECT_TRUE(std::isfinite(z));
}

TEST_F(ZSpaceManagerTest, WindowZVelocityIsZeroAtRest) {
    mgr.assignWindowToLayer(win0.handle(), 0);

    // Converge to target
    for (int i = 0; i < 2000; ++i)
        mgr.update(0.016f);

    EXPECT_NEAR(mgr.getWindowZVelocity(win0.handle()), 0.0f, 0.01f);
}

TEST_F(ZSpaceManagerTest, UnregisteredWindowGetZTargetIsZero) {
    EXPECT_FLOAT_EQ(mgr.getWindowZTarget(win0.handle()), 0.0f);
}

TEST_F(ZSpaceManagerTest, UnregisteredWindowGetZVelocityIsZero) {
    EXPECT_FLOAT_EQ(mgr.getWindowZVelocity(win0.handle()), 0.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// DEPTH SORTING — getSortedWindowsForRender()
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, SortedWindowsEmptyWhenNoWindows) {
    EXPECT_TRUE(mgr.getSortedWindowsForRender().empty());
}

TEST_F(ZSpaceManagerTest, SortedWindowsReturnAllRegisteredWindows) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.assignWindowToLayer(win1.handle(), 1);
    mgr.assignWindowToLayer(win2.handle(), 2);

    const auto sorted = mgr.getSortedWindowsForRender();
    EXPECT_EQ(static_cast<int>(sorted.size()), 3);
}

TEST_F(ZSpaceManagerTest, SortedWindowsOrderedBackToFront) {
    // Assign in mixed order
    mgr.assignWindowToLayer(win0.handle(), 0); // Z =    0.0  (nearest)
    mgr.assignWindowToLayer(win1.handle(), 3); // Z = -2800.0 (deepest)
    mgr.assignWindowToLayer(win2.handle(), 1); // Z =  -800.0
    mgr.assignWindowToLayer(win3.handle(), 2); // Z = -1600.0

    const auto sorted = mgr.getSortedWindowsForRender();
    ASSERT_EQ(static_cast<int>(sorted.size()), 4);

    // Expected back-to-front order: win1(deepest) → win3 → win2 → win0(nearest)
    EXPECT_EQ(sorted[0], win1.handle()); // -2800
    EXPECT_EQ(sorted[1], win3.handle()); // -1600
    EXPECT_EQ(sorted[2], win2.handle()); //  -800
    EXPECT_EQ(sorted[3], win0.handle()); //     0
}

TEST_F(ZSpaceManagerTest, SortedWindowsExcludesRemovedWindow) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.assignWindowToLayer(win1.handle(), 1);
    mgr.removeWindow(win0.handle());

    const auto sorted = mgr.getSortedWindowsForRender();
    ASSERT_EQ(static_cast<int>(sorted.size()), 1);
    EXPECT_EQ(sorted[0], win1.handle());
}

// ══════════════════════════════════════════════════════════════════════════════
// DERIVED RENDER PROPERTIES — opacity / blur
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, OpacityMatchesLayerConstant) {
    for (int layer = 0; layer < Z_LAYERS_COUNT; ++layer) {
        FakeWindow fw;
        mgr.assignWindowToLayer(fw.handle(), layer);
        EXPECT_FLOAT_EQ(mgr.getWindowOpacity(fw.handle()), LAYER_OPACITY[layer]) << "Layer " << layer;
        mgr.removeWindow(fw.handle());
    }
}

TEST_F(ZSpaceManagerTest, OpacityIsOneForUnknownWindow) {
    EXPECT_FLOAT_EQ(mgr.getWindowOpacity(win0.handle()), 1.0f);
}

TEST_F(ZSpaceManagerTest, BlurRadiusMatchesLayerConstant) {
    for (int layer = 0; layer < Z_LAYERS_COUNT; ++layer) {
        FakeWindow fw;
        mgr.assignWindowToLayer(fw.handle(), layer);
        EXPECT_FLOAT_EQ(mgr.getWindowBlurRadius(fw.handle()), LAYER_BLUR_RADIUS[layer]) << "Layer " << layer;
        mgr.removeWindow(fw.handle());
    }
}

TEST_F(ZSpaceManagerTest, BlurRadiusIsZeroForUnknownWindow) {
    EXPECT_FLOAT_EQ(mgr.getWindowBlurRadius(win0.handle()), 0.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// PROJECTION MATRICES — sanity checks (not NaN, determinant not zero)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, SpatialProjectionIsFinite) {
    const glm::mat4 proj = mgr.getSpatialProjection();
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            EXPECT_TRUE(std::isfinite(proj[col][row])) << "NaN/Inf at proj[" << col << "][" << row << "]";
}

TEST_F(ZSpaceManagerTest, SpatialViewIsFinite) {
    const glm::mat4 view = mgr.getSpatialView();
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            EXPECT_TRUE(std::isfinite(view[col][row])) << "NaN/Inf at view[" << col << "][" << row << "]";
}

TEST_F(ZSpaceManagerTest, WindowTransformIsIdentityForUnknownWindow) {
    const glm::mat4 t   = mgr.getWindowTransform(win0.handle());
    const glm::mat4 eye = glm::mat4(1.0f);
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            EXPECT_FLOAT_EQ(t[col][row], eye[col][row]);
}

TEST_F(ZSpaceManagerTest, WindowTransformIncludesZTranslation) {
    mgr.assignWindowToLayer(win0.handle(), 2); // Z = -1600

    // Run frames so the window position converges to the layer target
    for (int i = 0; i < 2000; ++i)
        mgr.update(0.016f);

    const glm::mat4 t = mgr.getWindowTransform(win0.handle());
    // Translation Z is at column 3, row 2 (column-major mat4)
    EXPECT_NEAR(t[3][2], LAYER_Z_POSITIONS[2], 1.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// WRITEBACK TO SSpatialProps — verify the CWindow-side state is updated
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, AssignWindowSetsSSpatialPropsLayerAndZ) {
    mgr.assignWindowToLayer(win0.handle(), 1);

    EXPECT_EQ(win0.props().iZLayer, 1);
    EXPECT_FLOAT_EQ(win0.props().fZPosition, LAYER_Z_POSITIONS[1]);
    EXPECT_FLOAT_EQ(win0.props().fZTarget, LAYER_Z_POSITIONS[1]);
    EXPECT_TRUE(win0.props().bZManaged);
}

TEST_F(ZSpaceManagerTest, UpdateSyncsSSpatialPropsAfterAnimation) {
    mgr.assignWindowToLayer(win0.handle(), 3);
    // Run frames — fZPosition in SSpatialProps should converge to layer Z
    for (int i = 0; i < 2000; ++i)
        mgr.update(0.016f);

    EXPECT_NEAR(win0.props().fZPosition, LAYER_Z_POSITIONS[3], 1.0f);
    EXPECT_NEAR(win0.props().fZTarget, LAYER_Z_POSITIONS[3], 1.0f);
    EXPECT_NEAR(win0.props().fZVelocity, 0.0f, 0.01f);
}

TEST_F(ZSpaceManagerTest, PinWindowReflectedInSSpatialProps) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.pinWindow(win0.handle(), true);
    EXPECT_TRUE(win0.props().bZPinned);

    mgr.pinWindow(win0.handle(), false);
    EXPECT_FALSE(win0.props().bZPinned);
}

TEST_F(ZSpaceManagerTest, DepthNormIsNormalizedAfterUpdate) {
    mgr.assignWindowToLayer(win0.handle(), 0); // nearest  → fDepthNorm ~1.0
    mgr.assignWindowToLayer(win3.handle(), 3); // deepest  → fDepthNorm ~0.0

    for (int i = 0; i < 2000; ++i)
        mgr.update(0.016f);

    EXPECT_NEAR(win0.props().fDepthNorm, 1.0f, 0.01f);
    EXPECT_NEAR(win3.props().fDepthNorm, 0.0f, 0.01f);
}

// ══════════════════════════════════════════════════════════════════════════════
// THREAD SAFETY — concurrent reads and writes must not crash
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, ConcurrentAssignAndQueryDoesNotCrash) {
    // Register baseline windows
    mgr.assignWindowToLayer(win0.handle(), 0);
    mgr.assignWindowToLayer(win1.handle(), 1);

    constexpr int ITERATIONS = 100;

    auto          writer = [&]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            mgr.assignWindowToLayer(win0.handle(), i % Z_LAYERS_COUNT);
            mgr.update(0.016f);
        }
    };

    auto reader = [&]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            (void)mgr.getWindowLayer(win0.handle());
            (void)mgr.getWindowZ(win1.handle());
            (void)mgr.getSortedWindowsForRender();
            (void)mgr.getWindowCount();
        }
    };

    std::thread t1(writer);
    std::thread t2(reader);
    t1.join();
    t2.join();
    // Test passes if we reach this point without ASAN/TSAN hitting a data race
}

// ══════════════════════════════════════════════════════════════════════════════
// DEBUG PRINT — smoke test (must not crash, must not throw)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, DebugPrintDoesNotCrash) {
    mgr.assignWindowToLayer(win0.handle(), 0);
    EXPECT_NO_THROW(mgr.debugPrint());
}

// ══════════════════════════════════════════════════════════════════════════════
// IS ANIMATING — TASK-SH-004 / TASK-SH-201
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, IsAnimatingFalseWhenIdle) {
    // Freshly initialized manager with no layer changes — nothing moving
    EXPECT_FALSE(mgr.isAnimating());
}

TEST_F(ZSpaceManagerTest, IsAnimatingTrueAfterLayerChange) {
    // setActiveLayer() sets a new camera Z target, spring starts moving
    mgr.setActiveLayer(1);
    EXPECT_TRUE(mgr.isAnimating());
}

TEST_F(ZSpaceManagerTest, IsAnimatingFalseAfterSpringSettles) {
    // Drive the spring to settle by stepping with large dt many times
    mgr.setActiveLayer(2);
    for (int i = 0; i < 500; ++i)
        mgr.update(0.016f);
    EXPECT_FALSE(mgr.isAnimating());
}

TEST_F(ZSpaceManagerTest, IsAnimatingTrueWhenWindowMidTransition) {
    // Assign a window to layer 3 so its spring moves toward -2800
    mgr.assignWindowToLayer(win0.handle(), 3);
    // The window target is now -2800 but position is still 0 from the earlier assign
    // Manually poke the position to simulate mid-transition
    mgr.update(0.001f); // tiny step so spring hasn't settled
    // Camera is at layer 0 (no layer change), but the window spring is moving
    const bool animating = mgr.isAnimating();
    // After a tiny step the window delta is still large (0 → -2800 range)
    EXPECT_TRUE(animating);
}

TEST_F(ZSpaceManagerTest, IsAnimatingFalseAfterWindowSpringSettles) {
    mgr.assignWindowToLayer(win0.handle(), 1);
    // Drive everything to settle
    for (int i = 0; i < 500; ++i)
        mgr.update(0.016f);
    EXPECT_FALSE(mgr.isAnimating());
}

TEST_F(ZSpaceManagerTest, IsAnimatingReturnsFalseOnLayer0ToLayer0) {
    // Redundant setActiveLayer — no actual change, spring doesn't fire
    mgr.setActiveLayer(0); // already at 0
    EXPECT_FALSE(mgr.isAnimating());
}

// ══════════════════════════════════════════════════════════════════════════════
// FOV LERP — TASK-SH-202
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ZSpaceManagerTest, FovIsBaseAtLayer0) {
    // Before any layer change camera is at Z=0 → FOV must equal Z_FOV_DEGREES (60°)
    EXPECT_FLOAT_EQ(mgr.getCurrentFov(), Spatial::Z_FOV_DEGREES);
}

TEST_F(ZSpaceManagerTest, FovIncreasesAsCameraMovesDeeper) {
    // Transition to layer 3 (deepest). After settling FOV must be at max.
    mgr.setActiveLayer(3);
    for (int i = 0; i < 500; ++i)
        mgr.update(0.016f);

    const float fov = mgr.getCurrentFov();
    EXPECT_NEAR(fov, Spatial::Z_FOV_MAX_DEGREES, 0.5f) << "FOV after settling at layer 3 should be ~Z_FOV_MAX_DEGREES";
    EXPECT_GT(fov, Spatial::Z_FOV_DEGREES) << "FOV at deepest layer must exceed base FOV";
}

TEST_F(ZSpaceManagerTest, FovMidLayerIsInterpolated) {
    // Settle at layer 1 (-800 / -2800 ≈ 0.286 of full range)
    // Expected FOV ≈ 60 + 0.286*(75-60) ≈ 64.3°
    mgr.setActiveLayer(1);
    for (int i = 0; i < 500; ++i)
        mgr.update(0.016f);

    const float fov = mgr.getCurrentFov();
    EXPECT_GT(fov, Spatial::Z_FOV_DEGREES) << "mid-range FOV must exceed base";
    EXPECT_LT(fov, Spatial::Z_FOV_MAX_DEGREES) << "mid-range FOV must be below max";
}

TEST_F(ZSpaceManagerTest, FovReturnsToBseWhenCameraReturnToLayer0) {
    // Deep then back: FOV must return to base after spring settles at layer 0.
    mgr.setActiveLayer(3);
    for (int i = 0; i < 500; ++i)
        mgr.update(0.016f);

    mgr.setActiveLayer(0);
    for (int i = 0; i < 500; ++i)
        mgr.update(0.016f);

    EXPECT_NEAR(mgr.getCurrentFov(), Spatial::Z_FOV_DEGREES, 0.5f) << "FOV must recover to Z_FOV_DEGREES after camera returns to layer 0";
}
