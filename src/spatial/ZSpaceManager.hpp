#pragma once

#include <array>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Z-Space Management
// ══════════════════════════════════════════════════════════════════════════════
// Manages Z position of windows, depth layers, and animations.
// Thread-safe via internal mutex (m_mtxWindows).
//
// Usage lifecycle:
//   1. Construct ZSpaceManager
//   2. Call init(w, h) once after compositor screen size is known
//   3. Call update(dt) every frame from the render loop
//   4. Call removeWindow() in the window destroy callback — mandatory
// ══════════════════════════════════════════════════════════════════════════════

namespace Spatial {

    // ── Z-System Constants ────────────────────────────────────────────────────────

    /// @brief Total number of discrete Z layers in the spatial system
    constexpr int Z_LAYERS_COUNT = 4;

    /// @brief World-unit distance between adjacent layers
    constexpr float Z_LAYER_STEP = 800.0f;

    /// @brief Near clipping plane for perspective projection
    constexpr float Z_NEAR = 0.1f;

    /// @brief Far clipping plane for perspective projection
    constexpr float Z_FAR = 10000.0f;

    /// @brief Vertical field-of-view in degrees for spatial perspective (layer 0, closest view)
    constexpr float Z_FOV_DEGREES = 60.0f;

    /// @brief Maximum field-of-view in degrees when camera is at the deepest layer (layer 3)
    /// @note FOV interpolates linearly from Z_FOV_DEGREES (layer 0) to Z_FOV_MAX_DEGREES (layer 3),
    ///       producing a subtle cinematic pull-back as the user navigates deeper into the Z stack.
    constexpr float Z_FOV_MAX_DEGREES = 75.0f;

    /// @brief Spring stiffness constant for Z animation (higher = snappier)
    constexpr float Z_ANIM_STIFFNESS = 200.0f;

    /// @brief Spring damping constant for Z animation (higher = less bounce)
    constexpr float Z_ANIM_DAMPING = 20.0f;

    /// @brief World Z coordinate for each discrete layer (index = layer number)
    constexpr std::array<float, Z_LAYERS_COUNT> LAYER_Z_POSITIONS = {
        0.0f,     // Layer 0: Foreground — active app
        -800.0f,  // Layer 1: Near       — recent apps
        -1600.0f, // Layer 2: Mid        — background
        -2800.0f, // Layer 3: Far        — system / config
    };

    /// @brief Opacity multiplier for each layer (1.0 = fully opaque)
    constexpr std::array<float, Z_LAYERS_COUNT> LAYER_OPACITY = {
        1.00f, // Layer 0: full opacity
        0.75f, // Layer 1: slight fade
        0.40f, // Layer 2: clearly in background
        0.15f, // Layer 3: nearly invisible
    };

    /// @brief Gaussian blur radius (px) applied to each layer
    constexpr std::array<float, Z_LAYERS_COUNT> LAYER_BLUR_RADIUS = {
        0.0f,  // Layer 0: no blur
        1.5f,  // Layer 1: subtle blur
        5.0f,  // Layer 2: evident blur
        12.0f, // Layer 3: strong blur
    };

    // ──────────────────────────────────────────────────────────────────────────────
    /// @class ZSpaceManager
    /// @brief Centralized manager for spatial navigation along Z axis
    ///
    /// Responsibilities:
    /// - Assign windows to Z layers and continuous coordinates
    /// - Update camera position based on active layer
    /// - Animate position transitions with spring physics
    /// - Maintain render order (depth sorting) for Renderer.cpp
    /// - Validate depth limits and constraints
    ///
    /// @note Thread-safe — all public methods acquire m_mtxWindows internally.
    ///       Do NOT call from within a m_mtxWindows-held context (deadlock).
    // ──────────────────────────────────────────────────────────────────────────────
    class ZSpaceManager {
      public:
        /// @brief Construct ZSpaceManager in uninitialized state
        /// @note Call init() before any other method
        ZSpaceManager();

        /// @brief Destroy ZSpaceManager and release all resources
        /// @note Window pointers are NOT freed — compositor owns them
        ~ZSpaceManager();

        // ── Initialization ────────────────────────────────────────────────────────

        /// @brief Initialize Z manager with screen parameters
        /// @param screenWidth  Screen width in pixels
        /// @param screenHeight Screen height in pixels
        /// @note  Must be called once before any other method.
        ///        Safe to call again on monitor resolution change.
        void init(int screenWidth, int screenHeight);

        /// @brief Check whether init() has been called successfully
        /// @return true if initialized, false if init() was never called
        [[nodiscard]] bool isInitialized() const;

        // ── Layer Navigation ──────────────────────────────────────────────────────

        /// @brief Advance camera to next layer (deeper into the scene)
        /// @return true if layer changed, false if already at last layer
        [[nodiscard]] bool nextLayer();

        /// @brief Return camera to previous layer (closer to viewer)
        /// @return true if layer changed, false if already at first layer
        [[nodiscard]] bool prevLayer();

        /// @brief Jump camera to a specific layer
        /// @param layer  Target layer index (0 = foreground, Z_LAYERS_COUNT-1 = far)
        /// @return true if the change was applied, false if layer index is out of range
        [[nodiscard]] bool setActiveLayer(int layer);

        /// @brief Get the current active (camera-focused) layer
        /// @return Layer index in [0, Z_LAYERS_COUNT-1]
        [[nodiscard]] int getActiveLayer() const;

        // ── Z Positioning ─────────────────────────────────────────────────────────

        /// @brief Get current animated camera Z position in world units
        /// @return Camera Z coordinate (0.0 = foreground, negative = deeper)
        [[nodiscard]] float getCameraZ() const;

        /// @brief Assign a window to a discrete Z layer
        /// @param window  Opaque pointer to CWindow (void* avoids circular include)
        /// @param layer   Target layer index (0 to Z_LAYERS_COUNT-1)
        /// @note Creates a tracking entry if the window is not yet registered.
        ///       Immediately updates window's SSpatialProps fields.
        void assignWindowToLayer(void* window, int layer);

        /// @brief Unregister a window from Z tracking
        /// @param window  Opaque pointer to the CWindow being destroyed
        /// @warning MUST be called in the window-destroy callback to prevent
        ///          use-after-free inside the update() loop.
        void removeWindow(void* window);

        /// @brief Get the discrete layer a window is currently assigned to
        /// @param window  Opaque pointer to CWindow
        /// @return Layer index in [0, Z_LAYERS_COUNT-1], or -1 if not registered
        [[nodiscard]] int getWindowLayer(void* window) const;

        /// @brief Get the current animated Z position of a window in world units
        /// @param window  Opaque pointer to CWindow
        /// @return Z coordinate (0.0 = foreground, negative = deeper); 0.0 if not registered
        [[nodiscard]] float getWindowZ(void* window) const;

        /// @brief Override the Z animation target for a window (continuous, non-layer)
        /// @param window  Opaque pointer to CWindow
        /// @param z       Target Z position in world units
        /// @note The window's iZLayer is NOT updated — use assignWindowToLayer() for layer changes.
        void setWindowZPosition(void* window, float z);

        // ── Window Pin Control ────────────────────────────────────────────────────

        /// @brief Pin or unpin a window from camera Z movement
        /// @param window  Opaque pointer to CWindow
        /// @param pinned  true = window stays fixed in Z regardless of camera;
        ///                false = window moves with its assigned layer (default)
        /// @note Pinned windows are used for HUD/overlay surfaces (Layer -1 / Overlay).
        void pinWindow(void* window, bool pinned);

        /// @brief Query whether a window is currently pinned
        /// @param window  Opaque pointer to CWindow
        /// @return true if pinned, false if not pinned or not registered
        [[nodiscard]] bool isPinned(void* window) const;

        // ── Animation ─────────────────────────────────────────────────────────────

        /// @brief Advance all Z spring animations by one frame
        /// @param deltaTime  Elapsed time since last frame in seconds
        /// @note  Call once per compositor frame from the render loop.
        ///        Acquires m_mtxWindows internally — do not hold the mutex when calling.
        ///        deltaTime is clamped internally to [0, 0.05] to prevent spring divergence.
        void update(float deltaTime);

        /// @brief Return true when the camera or any window is still moving
        /// @return true if any spring has velocity or position delta above threshold
        /// @note Used by the render loop to schedule the next frame during Z transitions.
        ///       Once all springs settle this returns false so VFR can idle the GPU.
        [[nodiscard]] bool isAnimating() const;

        /// @brief Get the animation target Z position of a window
        /// @param window  Opaque pointer to CWindow
        /// @return Target Z in world units; 0.0 if not registered
        [[nodiscard]] float getWindowZTarget(void* window) const;

        /// @brief Get the current Z animation velocity of a window
        /// @param window  Opaque pointer to CWindow
        /// @return Velocity in world units per second; 0.0 if not registered
        [[nodiscard]] float getWindowZVelocity(void* window) const;

        // ── Projection Matrices ───────────────────────────────────────────────────

        /// @brief Build the perspective projection matrix for the current camera state
        /// @return Column-major 4×4 perspective matrix (FOV lerps 60°→75° with depth, near=0.1, far=10000)
        [[nodiscard]] glm::mat4 getSpatialProjection() const;

        /// @brief Get the current interpolated field-of-view in degrees
        /// @return FOV in [Z_FOV_DEGREES, Z_FOV_MAX_DEGREES] driven by camera Z position
        /// @note Updated every frame by update(). Useful for tests and HUD diagnostics.
        [[nodiscard]] float getCurrentFov() const;

        /// @brief Build the view matrix for the current camera Z position
        /// @return Column-major 4×4 look-at view matrix (Y-axis inverted for screen space)
        [[nodiscard]] glm::mat4 getSpatialView() const;

        /// @brief Build the model transform matrix for a window (translation only)
        /// @param window  Opaque pointer to CWindow
        /// @return Column-major 4×4 model matrix with XYZ translation applied
        [[nodiscard]] glm::mat4 getWindowTransform(void* window) const;

        // ── Depth Sorting ─────────────────────────────────────────────────────────

        /// @brief Return window pointers sorted back-to-front for painter's algorithm
        /// @return Vector of void* (CWindow pointers) ordered from deepest Z to shallowest
        /// @note Renderer.cpp MUST use this order when drawing to ensure correct
        ///       alpha compositing of background layers over foreground.
        [[nodiscard]] std::vector<void*> getSortedWindowsForRender() const;

        /// @brief Get the total number of windows currently tracked
        /// @return Count of registered windows (useful for tests and debug HUD)
        [[nodiscard]] int getWindowCount() const;

        // ── Derived Render Properties ─────────────────────────────────────────────

        /// @brief Compute the opacity multiplier for a window based on its layer
        /// @param window  Opaque pointer to CWindow
        /// @return Opacity in [0.0, 1.0]; 1.0 if not registered
        [[nodiscard]] float getWindowOpacity(void* window) const;

        /// @brief Compute the Gaussian blur radius (px) for a window based on its layer
        /// @param window  Opaque pointer to CWindow
        /// @return Blur radius in pixels; 0.0 if not registered or on foreground layer
        [[nodiscard]] float getWindowBlurRadius(void* window) const;

        // ── Debug ─────────────────────────────────────────────────────────────────

        /// @brief Print current manager state to stdout
        /// @note Debug builds only — outputs active layer, camera Z, and per-window Z state.
        ///       Do not call in production render loop.
        void debugPrint() const;

      private:
        /// @brief Per-window Z animation state (internal tracking only)
        struct WindowZ {
            void* pWindow    = nullptr; ///< Opaque pointer to CWindow — NOT owned
            float fZPosition = 0.0f;    ///< Current animated Z position (world units)
            float fZTarget   = 0.0f;    ///< Spring target Z position
            float fZVelocity = 0.0f;    ///< Spring velocity (world units/sec)
            int   iZLayer    = 0;       ///< Discrete layer index [0, Z_LAYERS_COUNT-1]
            bool  bZPinned   = false;   ///< true = immune to camera Z movement
        };

        std::vector<WindowZ> m_vWindowsZ;                        ///< All tracked windows
        int                  m_iActiveLayer     = 0;             ///< Current layer index
        float                m_fCameraZ         = 0.0f;          ///< Animated camera Z
        float                m_fCameraZTarget   = 0.0f;          ///< Camera spring target
        float                m_fCameraZVelocity = 0.0f;          ///< Camera spring velocity
        float                m_fCurrentFov      = Z_FOV_DEGREES; ///< [SPATIAL] Interpolated FOV (degrees) — TASK-SH-202
        int                  m_iScreenW         = 0;             ///< Screen width (pixels)
        int                  m_iScreenH         = 0;             ///< Screen height (pixels)

        /// @brief Internal mutex protecting m_vWindowsZ and camera state
        /// @note  Declared mutable so const query methods can acquire it.
        ///        Uses recursive_mutex so navigation methods (nextLayer, prevLayer)
        ///        can safely call setActiveLayer() from within an already-held lock
        ///        without deadlocking on the same thread.
        mutable std::recursive_mutex m_mtxWindows;

        /// @brief Find a WindowZ entry by opaque window pointer (mutable overload)
        /// @param window  Opaque CWindow pointer to search for
        /// @return Pointer to entry, or nullptr if not found
        WindowZ* findWindow(void* window);

        /// @brief Find a WindowZ entry by opaque window pointer (const overload)
        /// @param window  Opaque CWindow pointer to search for
        /// @return Const pointer to entry, or nullptr if not found
        const WindowZ* findWindow(void* window) const;

        /// @brief Advance a single window's spring animation by one timestep
        /// @param wz  Reference to the WindowZ state to update in-place
        /// @param dt  Delta time in seconds (pre-clamped by caller)
        void updateSpringAnimation(WindowZ& wz, float dt);
    };

} // namespace Spatial
